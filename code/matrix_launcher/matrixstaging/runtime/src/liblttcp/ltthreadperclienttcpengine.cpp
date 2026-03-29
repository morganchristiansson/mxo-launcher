#include "ltthreadperclienttcpengine.h"

#include "../libltmessaging/messageconnection.h"
#include "../../../game/src/libltclientlogin/loginmediator.h"
#include <spdlog/spdlog.h>

#include <winsock2.h>
#include <ws2tcpip.h>

#include <process.h>

#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mxo::liblttcp {

namespace {

// UNANCHORED helper used by the starter scaffold.
// No direct launcher.exe function anchor is assigned yet.
static bool EnsureWinsockReady() {
    static bool initialized = false;
    static bool attempted = false;
    if (attempted) {
        return initialized;
    }
    attempted = true;

    WSADATA wsaData = {};
    initialized = (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
    return initialized;
}

// UNANCHORED helper used by the starter scaffold.
// No direct launcher.exe function anchor is assigned yet.
static bool ResolveIpv4Address(const char* hostName, uint32_t* outIpv4NetworkOrder) {
    if (!hostName || !hostName[0] || !outIpv4NetworkOrder || !EnsureWinsockReady()) {
        return false;
    }

    addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* results = nullptr;
    if (getaddrinfo(hostName, nullptr, &hints, &results) != 0 || !results) {
        return false;
    }

    bool ok = false;
    for (addrinfo* it = results; it; it = it->ai_next) {
        if (it->ai_family != AF_INET || !it->ai_addr || it->ai_addrlen < static_cast<int>(sizeof(sockaddr_in))) {
            continue;
        }
        const sockaddr_in* addr = reinterpret_cast<const sockaddr_in*>(it->ai_addr);
        *outIpv4NetworkOrder = addr->sin_addr.s_addr;
        ok = true;
        break;
    }

    freeaddrinfo(results);
    return ok;
}

// Per-logger SPDLOG_LEVEL overrides only apply on call sites that explicitly fetch a named logger.
// Keep the receive hot-path seam narrow by only routing labels with registered logger names through
// spdlog::get(...); all other bridge labels fall back to the default logger.
static spdlog::logger* LoggerForBridgeLabel(const char* label) {
    if (label && label[0]) {
        if (std::shared_ptr<spdlog::logger> logger = spdlog::get(label)) {
            return logger.get();
        }
    }
    return spdlog::default_logger_raw();
}

struct CLTThreadPerClientTCPEngine_QueuePair {
    uint32_t value0;
    uint32_t value1;
};

static constexpr uint32_t kInvalidSocketHandle = 0xffffffffu;
static void* g_LauncherConnectionBridgeWorkItemVtable[2] = {0};
static void* g_LauncherConnectionBridgeContextVtable[5] = {0};

// UNANCHORED: source-owned narrow mirror of the original queue block free-list behavior.
// Static RE already shows that the consumer path recycles exhausted blocks instead of treating the
// transition as a simple free-and-forget step. Current source keeps that narrower behavior in a
// side cache keyed by the queue object while the exact original in-object free-list plumbing is
// still unrecovered.
static std::unordered_map<CLTThreadPerClientTCPEngine_Queue*, std::vector<uint32_t*>>
    g_QueueRecycledBlocks;

static bool IsSyntheticReceiveDrainWorkType(uint32_t workType) {
    return workType == CLTThreadPerClientTCPEngine::kWorkTypeSyntheticReceiveDrain;
}

// UNANCHORED: source-owned helper for the current connection/worker key normalization seams.
static void* ResolveEngineContextKeyScaffold(void* contextKey) {
    return CBaseConnection_ResolveQueueCleanupContextKeyScaffold(contextKey);
}

// UNANCHORED: source-owned helper for the current queue-context bridge lookup seams.
static CBaseConnection* ResolveEngineQueueContextOwnerScaffold(void* contextKey) {
    return CBaseConnection_FromQueueContextScaffold(contextKey);
}

static const char* LauncherBridgeWorkTypeName(uint32_t workType) {
    switch (workType) {
        case CLTThreadPerClientTCPEngine::kWorkTypeClose:
            return "Close";
        case CLTThreadPerClientTCPEngine::kWorkTypeConnectionStatus:
            return "ConnectionStatus";
        case CLTThreadPerClientTCPEngine::kWorkTypeParsedPacket:
            return "ParsedPacket";
        case CLTThreadPerClientTCPEngine::kWorkTypeSyntheticReceiveDrain:
            return "SyntheticReceiveDrain";
        default:
            return "Unknown";
    }
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static uint32_t __thiscall LauncherConnectionBridgeWorkItem_Release(
    mxo::ltlogin::CLTLoginMediatorQueuedWorkItemScaffold* self) {
    if (self) {
        LoggerForBridgeLabel(self->debugLabel)->info(
            "CLTThreadPerClientTCPEngine launcher bridge releasing queued work item {} type=0x{:08x} ({}) payload=0x{:08x} label='{}'",
            fmt::ptr(self),
            self->header.workType,
            LauncherBridgeWorkTypeName(self->header.workType),
            self->workPayload,
            self->debugLabel ? self->debugLabel : "<null>");
        std::free(self);
    }
    return 1u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static void EnsureLauncherConnectionBridgeWorkItemVtableInitialized() {
    if (!g_LauncherConnectionBridgeWorkItemVtable[1]) {
        g_LauncherConnectionBridgeWorkItemVtable[1] =
            reinterpret_cast<void*>(LauncherConnectionBridgeWorkItem_Release);
    }
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static uint32_t __thiscall LauncherConnectionBridgeContext_Release(
    mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* self) {
    LoggerForBridgeLabel(self ? self->debugLabel : nullptr)->info(
        "CLTThreadPerClientTCPEngine launcher bridge context release self={} label='{}' autoRelease={}",
        fmt::ptr(self),
        (self && self->debugLabel) ? self->debugLabel : "<null>",
        (self && self->autoReleaseFlag) ? 1u : 0u);
    return 1u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static uint32_t __thiscall LauncherConnectionBridgeContext_OnOperationCompleted(
    mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* self,
    mxo::ltlogin::CLTLoginMediatorQueuedWorkItemScaffold* workItem) {
    LoggerForBridgeLabel(workItem && workItem->debugLabel ? workItem->debugLabel : (self ? self->debugLabel : nullptr))->info(
        "CLTThreadPerClientTCPEngine launcher bridge OnOperationCompleted context={} label='{}' workItem={} type=0x{:08x} ({}) payload=0x{:08x}",
        fmt::ptr(self),
        (self && self->debugLabel) ? self->debugLabel : "<null>",
        fmt::ptr(workItem),
        workItem ? workItem->header.workType : 0u,
        LauncherBridgeWorkTypeName(workItem ? workItem->header.workType : 0u),
        workItem ? workItem->workPayload : 0u);

    mxo::ltlogin::CLTLoginMediator* mediator = self ? self->mediator : nullptr;
    if (!self || !workItem || !mediator) {
        return 1u;
    }

    if (workItem->header.workType == CLTThreadPerClientTCPEngine::kWorkTypeConnectionStatus) {
        const uint32_t handled = self->isMarginConnection
            ? mediator->HandleMarginConnectStatus(workItem->workPayload)
            : mediator->HandleAuthConnectStatus(workItem->workPayload);
        const char* routeLabel = self->isMarginConnection ? "margin" : "auth";
        const char* incomingReplyAnchor = self->isMarginConnection
            ? mxo::ltlogin::CLTLoginMediator::kMessageMsLoadCharacterReply
            : mxo::ltlogin::CLTLoginMediator::kMessageAsAuthReply;
        spdlog::info(
            "CLTThreadPerClientTCPEngine launcher bridge routed {} type-2 connect-status payload=0x{:08x} -> handled={} laterIncomingReplyAnchor='{}'",
            routeLabel,
            static_cast<unsigned>(workItem->workPayload),
            static_cast<unsigned>(handled),
            (incomingReplyAnchor && incomingReplyAnchor[0]) ? incomingReplyAnchor : "<none>");
    }

    if (IsSyntheticReceiveDrainWorkType(workItem->header.workType)) {
        // Fallback only:
        // - the bounded receive-entry correction now normally queues this synthetic proxy with the
        //   connection-family queue context so it re-enters through `CMessageConnection`
        // - keep the older mediator-context handling here as a defensive fallback until that seam is
        //   fully retired
        if (!self->isMarginConnection) {
            const uint32_t receiveActions = mediator->HandleAuthConnectionReceiveScaffold();
            if (receiveActions & mxo::ltlogin::CLTLoginMediator::kReceiveActionBeginMarginAfterAuthReply) {
                const uint32_t marginConnectResult =
                    mediator->BeginLauncherMarginConnectionScaffold();
                spdlog::info(
                    "CLTThreadPerClientTCPEngine launcher bridge synthetic receive-drain post-AS_AuthReply margin auto-begin result=0x{:08x}",
                    static_cast<unsigned>(marginConnectResult));
            }
        } else {
            (void)mediator->HandleMarginConnectionReceiveScaffold();
        }
    }

    return 1u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
static void EnsureLauncherConnectionBridgeContextVtableInitialized() {
    if (!g_LauncherConnectionBridgeContextVtable[1]) {
        g_LauncherConnectionBridgeContextVtable[1] =
            reinterpret_cast<void*>(LauncherConnectionBridgeContext_Release);
        g_LauncherConnectionBridgeContextVtable[4] =
            reinterpret_cast<void*>(LauncherConnectionBridgeContext_OnOperationCompleted);
    }
}

// UNANCHORED internal helper used by the current thread-object scaffolds.
static void CloseSocketHandle(uint32_t* socketHandle) {
    if (!socketHandle || *socketHandle == kInvalidSocketHandle) {
        return;
    }

    closesocket(static_cast<SOCKET>(*socketHandle));
    *socketHandle = kInvalidSocketHandle;
}

// UNANCHORED: source-owned helper for the narrowed queue block recycling seam.
static void QueueRecycleBlockScaffold(
    CLTThreadPerClientTCPEngine_Queue* queue,
    uint32_t* block) {
    if (!queue || !block) {
        return;
    }
    g_QueueRecycledBlocks[queue].push_back(block);
}

// UNANCHORED: source-owned helper for the narrowed queue block recycling seam.
static uint32_t* QueueTakeRecycledBlockScaffold(
    CLTThreadPerClientTCPEngine_Queue* queue) {
    if (!queue) {
        return nullptr;
    }

    auto it = g_QueueRecycledBlocks.find(queue);
    if (it == g_QueueRecycledBlocks.end() || it->second.empty()) {
        return nullptr;
    }

    uint32_t* block = it->second.back();
    it->second.pop_back();
    if (it->second.empty()) {
        g_QueueRecycledBlocks.erase(it);
    }
    return block;
}

// UNANCHORED: source-owned helper for the narrowed queue block recycling seam.
static void QueueFreeRecycledBlocksScaffold(
    CLTThreadPerClientTCPEngine_Queue* queue) {
    if (!queue) {
        return;
    }

    auto it = g_QueueRecycledBlocks.find(queue);
    if (it == g_QueueRecycledBlocks.end()) {
        return;
    }

    for (uint32_t* block : it->second) {
        std::free(block);
    }
    g_QueueRecycledBlocks.erase(it);
}

static _RTL_CRITICAL_SECTION* CriticalSectionFromOpaqueStorage(void* storage) {
    return static_cast<_RTL_CRITICAL_SECTION*>(storage);
}

// anchor: launcher.exe:0x452270 / 0x452300 / 0x452320 helper family shape
static uint32_t CreateConnectedWakeupSocketHandle() {
    if (!EnsureWinsockReady()) {
        return kInvalidSocketHandle;
    }

    SOCKET wakeupSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (wakeupSocket == INVALID_SOCKET) {
        return kInvalidSocketHandle;
    }

    sockaddr_in wakeupAddr = {};
    wakeupAddr.sin_family = AF_INET;
    wakeupAddr.sin_port = htons(0);
    wakeupAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(wakeupSocket, reinterpret_cast<const sockaddr*>(&wakeupAddr), sizeof(wakeupAddr)) == SOCKET_ERROR) {
        closesocket(wakeupSocket);
        return kInvalidSocketHandle;
    }

    int wakeupAddrSize = sizeof(wakeupAddr);
    if (getsockname(wakeupSocket, reinterpret_cast<sockaddr*>(&wakeupAddr), &wakeupAddrSize) == SOCKET_ERROR) {
        closesocket(wakeupSocket);
        return kInvalidSocketHandle;
    }

    if (connect(wakeupSocket, reinterpret_cast<const sockaddr*>(&wakeupAddr), sizeof(wakeupAddr)) == SOCKET_ERROR) {
        closesocket(wakeupSocket);
        return kInvalidSocketHandle;
    }

    return static_cast<uint32_t>(wakeupSocket);
}

// anchor: launcher.exe:0x452320 helper shape
static void SignalWakeupSocketHandle(uint32_t socketHandle) {
    if (socketHandle == kInvalidSocketHandle) {
        return;
    }

    (void)send(static_cast<SOCKET>(socketHandle), "", 0, 0);
}

// UNANCHORED internal helper used by the current MonitorPort scaffold.
static uint32_t OpenTcpListenSocket(uint16_t portHostOrder, uint32_t ipv4NetworkOrder) {
    if (!EnsureWinsockReady()) {
        return kInvalidSocketHandle;
    }

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        return kInvalidSocketHandle;
    }

    sockaddr_in listenAddr = {};
    listenAddr.sin_family = AF_INET;
    listenAddr.sin_port = htons(portHostOrder);
    listenAddr.sin_addr.s_addr = ipv4NetworkOrder;

    if (bind(listenSocket, reinterpret_cast<const sockaddr*>(&listenAddr), sizeof(listenAddr)) == SOCKET_ERROR) {
        closesocket(listenSocket);
        return kInvalidSocketHandle;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(listenSocket);
        return kInvalidSocketHandle;
    }

    return static_cast<uint32_t>(listenSocket);
}

// UNANCHORED internal helper used by the current UDPMonitorPort scaffold.
static uint32_t OpenUdpMonitorSocket(uint16_t portHostOrder, uint32_t ipv4NetworkOrder) {
    if (!EnsureWinsockReady()) {
        return kInvalidSocketHandle;
    }

    SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSocket == INVALID_SOCKET) {
        return kInvalidSocketHandle;
    }

    BOOL reuseAddr = TRUE;
    if (setsockopt(
            udpSocket,
            SOL_SOCKET,
            SO_REUSEADDR,
            reinterpret_cast<const char*>(&reuseAddr),
            sizeof(reuseAddr)) == SOCKET_ERROR) {
        closesocket(udpSocket);
        return kInvalidSocketHandle;
    }

    sockaddr_in bindAddr = {};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(portHostOrder);
    bindAddr.sin_addr.s_addr = ipv4NetworkOrder;

    if (bind(udpSocket, reinterpret_cast<const sockaddr*>(&bindAddr), sizeof(bindAddr)) == SOCKET_ERROR) {
        closesocket(udpSocket);
        return kInvalidSocketHandle;
    }

    return static_cast<uint32_t>(udpSocket);
}

// UNANCHORED internal scaffold helper used by Queue_PushPair growth handling.
static bool RecenterQueueSlots(
    CLTThreadPerClientTCPEngine_Queue* queue,
    uint32_t additionalBlocks,
    bool biasTowardTail) {
    if (!queue || !queue->slotsBase || !queue->slotsCurrent || !queue->slotsLast) {
        return false;
    }

    uint32_t** slotsBase = static_cast<uint32_t**>(queue->slotsBase);
    uint32_t** slotsCurrent = static_cast<uint32_t**>(queue->slotsCurrent);
    uint32_t** slotsLast = static_cast<uint32_t**>(queue->slotsLast);
    const uint32_t activeBlocks = static_cast<uint32_t>((slotsLast - slotsCurrent) + 1);
    const uint32_t neededBlocks = activeBlocks + additionalBlocks;

    uint32_t newCapacity = queue->slotCapacity;
    uint32_t** newSlotsBase = slotsBase;
    if (queue->slotCapacity <= (neededBlocks * 2u)) {
        const uint32_t growthBase = (queue->slotCapacity >= additionalBlocks) ? queue->slotCapacity : additionalBlocks;
        newCapacity = queue->slotCapacity + growthBase + 2u;
        newSlotsBase = static_cast<uint32_t**>(std::calloc(newCapacity, sizeof(uint32_t*)));
        if (!newSlotsBase) {
            return false;
        }
    }

    uint32_t newIndex = (newCapacity - neededBlocks) >> 1;
    if (biasTowardTail) {
        newIndex += additionalBlocks;
    }

    uint32_t** newSlotsCurrent = newSlotsBase + newIndex;
    std::memmove(newSlotsCurrent, slotsCurrent, activeBlocks * sizeof(uint32_t*));

    if (newSlotsBase != slotsBase) {
        std::free(slotsBase);
        queue->slotsBase = newSlotsBase;
        queue->slotCapacity = newCapacity;
    }

    queue->slotsCurrent = newSlotsCurrent;
    queue->block0 = *newSlotsCurrent;
    queue->end0 = queue->block0 ? (static_cast<uint8_t*>(queue->block0) + 0x80) : nullptr;

    uint32_t** newSlotsLast = newSlotsCurrent + activeBlocks - 1;
    queue->slotsLast = newSlotsLast;
    queue->block1 = *newSlotsLast;
    queue->end1 = queue->block1 ? (static_cast<uint8_t*>(queue->block1) + 0x80) : nullptr;
    return true;
}

// UNANCHORED internal scaffold helper used by Queue_PushPair block-growth handling.
static bool GrowQueue(
    CLTThreadPerClientTCPEngine_Queue* queue,
    const CLTThreadPerClientTCPEngine_QueuePair* pendingPair) {
    if (!queue || !queue->slotsLast) {
        return false;
    }

    uint32_t** slotsBase = static_cast<uint32_t**>(queue->slotsBase);
    uint32_t** slotsLast = static_cast<uint32_t**>(queue->slotsLast);
    const uint32_t tailFreeSlots = queue->slotCapacity - static_cast<uint32_t>(slotsLast - slotsBase);
    if (tailFreeSlots < 2u) {
        if (!RecenterQueueSlots(queue, 1, false)) {
            return false;
        }
        slotsLast = static_cast<uint32_t**>(queue->slotsLast);
    }

    uint32_t* newBlock = QueueTakeRecycledBlockScaffold(queue);
    if (!newBlock) {
        newBlock = static_cast<uint32_t*>(std::calloc(1, 0x80));
        if (!newBlock) {
            return false;
        }
    } else {
        std::memset(newBlock, 0, 0x80);
    }

    slotsLast[1] = newBlock;

    uint32_t* current1 = static_cast<uint32_t*>(queue->current1);
    if (current1 && pendingPair) {
        current1[0] = pendingPair->value0;
        current1[1] = pendingPair->value1;
    }

    queue->slotsLast = slotsLast + 1;
    queue->block1 = newBlock;
    queue->end1 = static_cast<uint8_t*>(static_cast<void*>(newBlock)) + 0x80;
    queue->current1 = newBlock;
    return true;
}

}  // namespace

// anchor: launcher.exe:0x4319e0
CLTThread::CLTThread(const char* threadName)
    : threadName_(threadName ? threadName : ""),
      startPriority_(2),
      suspendDepth_(0),
      running_(false),
      threadId_(0),
      threadHandle_(0),
      stateMutex_() {}

// anchor: launcher.exe:0x452950 / 0x431a80 deleting wrapper
CLTThread::~CLTThread() {
    const HANDLE threadHandle = reinterpret_cast<HANDLE>(threadHandle_);
    if (threadHandle != nullptr) {
        CloseHandle(threadHandle);
        threadHandle_ = 0;
    }
}

// anchor: launcher.exe:0x4319d0
const std::string& CLTThread::GetNameString() const {
    return threadName_;
}

// anchor: launcher.exe:0x4528d0
uint32_t CLTThread::Start(int startPriority) {
    const auto mapThreadPriority = [](int priority) -> int {
        switch (priority) {
            case 0:
                return THREAD_PRIORITY_BELOW_NORMAL;
            case 1:
                return THREAD_PRIORITY_LOWEST;
            case 3:
                return THREAD_PRIORITY_ABOVE_NORMAL;
            case 4:
                return THREAD_PRIORITY_HIGHEST;
            default:
                return THREAD_PRIORITY_NORMAL;
        }
    };

    std::lock_guard<std::mutex> lock(stateMutex_);
    if (threadHandle_ != 0) {
        const DWORD waitResult = WaitForSingleObject(reinterpret_cast<HANDLE>(threadHandle_), 0);
        if (waitResult == WAIT_TIMEOUT) {
            return kStartAlreadyRunning;
        }
        CloseHandle(reinterpret_cast<HANDLE>(threadHandle_));
        threadHandle_ = 0;
        threadId_ = 0;
        running_ = false;
    }

    unsigned threadId = 0;
    const uintptr_t threadHandle = _beginthreadex(
        nullptr,
        0x10000,
        &CLTThread::ThreadStartAddressScaffold,
        this,
        CREATE_SUSPENDED,
        &threadId);
    threadHandle_ = threadHandle;
    if (threadHandle_ == 0) {
        return kStartFailure;
    }

    startPriority_ = startPriority;
    suspendDepth_ = 0;
    running_ = true;
    threadId_ = static_cast<uint32_t>(threadId);

    if (startPriority != 2) {
        SetThreadPriority(reinterpret_cast<HANDLE>(threadHandle_), mapThreadPriority(startPriority));
    }

    const DWORD resumeResult = ResumeThread(reinterpret_cast<HANDLE>(threadHandle_));
    if (resumeResult == 0xffffffffu) {
        CloseHandle(reinterpret_cast<HANDLE>(threadHandle_));
        threadHandle_ = 0;
        threadId_ = 0;
        running_ = false;
        return kStartFailure;
    }
    return kStartSuccess;
}

// anchor: launcher.exe:0x4525d0
bool CLTThread::Resume() {
    uintptr_t threadHandle = 0;
    bool isCurrentThread = false;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        threadHandle = threadHandle_;
        if (threadHandle == 0) {
            return false;
        }

        isCurrentThread = (threadId_ != 0 && threadId_ == GetCurrentThreadId());
        if (isCurrentThread) {
            ++suspendDepth_;
        }
    }

    if (isCurrentThread) {
        return false;
    }

    const DWORD resumeResult = ResumeThread(reinterpret_cast<HANDLE>(threadHandle));
    return resumeResult != 0xffffffffu;
}

// anchor: launcher.exe:0x452660
int CLTThread::Stop(bool waitAfterTerminate) {
    uintptr_t threadHandle = 0;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        threadHandle = threadHandle_;
    }

    if (threadHandle == 0) {
        return 0;
    }

    if (IsCurrentThread()) {
        ExitThread(0);
    }

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        const DWORD waitResult = WaitForSingleObject(reinterpret_cast<HANDLE>(threadHandle_), 0);
        if (waitResult != WAIT_TIMEOUT) {
            running_ = false;
            return 0;
        }
    }

    (void)TerminateThread(reinterpret_cast<HANDLE>(threadHandle), 0);
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        running_ = false;
        threadId_ = 0;
    }

    if (waitAfterTerminate) {
        (void)Wait();
    }
    return 0;
}

// anchor: launcher.exe:0x431a60
bool CLTThread::IsRunning() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (threadHandle_ == 0) {
        return false;
    }
    return WaitForSingleObject(reinterpret_cast<HANDLE>(threadHandle_), 0) == WAIT_TIMEOUT;
}

// anchor: launcher.exe:0x4526e0
uint32_t CLTThread::Wait() {
    uintptr_t threadHandle = 0;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        threadHandle = threadHandle_;
    }

    if (threadHandle == 0) {
        return WAIT_FAILED;
    }

    const DWORD waitResult = WaitForSingleObject(reinterpret_cast<HANDLE>(threadHandle), INFINITE);
    if (waitResult != WAIT_TIMEOUT) {
        std::lock_guard<std::mutex> lock(stateMutex_);
        running_ = false;
    }
    return waitResult;
}

// anchor: launcher.exe:0x452620
void CLTThread::Suspend() {
    uintptr_t threadHandle = 0;
    int suspendDepth = 0;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        threadHandle = threadHandle_;
        suspendDepth = suspendDepth_;
        if (suspendDepth_ > 0) {
            --suspendDepth_;
        }
    }

    if (threadHandle == 0 || suspendDepth > 0) {
        return;
    }

    (void)SuspendThread(reinterpret_cast<HANDLE>(threadHandle));
}

// anchor: launcher.exe:0x431a40
bool CLTThread::IsCurrentThread() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return threadId_ != 0 && threadId_ == GetCurrentThreadId();
}

// anchor: launcher.exe:0x437b50 on the current shared base vtable family
uint32_t CLTThread::PreRun() {
    return 0;
}

// UNANCHORED scaffold base default; concrete derived thread classes override this slot
void CLTThread::Run() {}

// anchor: launcher.exe:0x452770
void CLTThread::LogExit() {
    if (!threadName_.empty()) {
        spdlog::debug("{} exiting.", threadName_);
    }
}

// UNANCHORED: source-owned wrapper mirroring `_StartAddress_00452800` thread-entry sequencing.
uint32_t CLTThread::ExecuteThreadMainScaffold() {
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        threadId_ = GetCurrentThreadId();
        running_ = true;
    }

    if (!threadName_.empty()) {
        spdlog::debug("I'm a {}.", threadName_);
    }

    (void)PreRun();
    Run();
    LogExit();

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        running_ = false;
        threadId_ = 0;
    }
    return 0;
}

// UNANCHORED: source-owned `_beginthreadex` entry thunk for ExecuteThreadMainScaffold.
unsigned __stdcall CLTThread::ThreadStartAddressScaffold(void* parameter) {
    CLTThread* self = static_cast<CLTThread*>(parameter);
    return self ? self->ExecuteThreadMainScaffold() : 0u;
}

// anchor: launcher.exe:0x4365a0
CLTThreadPerClientTCPEngine_QueueThread::CLTThreadPerClientTCPEngine_QueueThread(
    CLTThreadPerClientTCPEngine* owner)
    : CLTThread("ILTTCPEngine::QueueThread"),
      owner_(owner) {}

// UNANCHORED: current vtable family keeps the shared CLTThread deleting dtor at slot +0x2c
CLTThreadPerClientTCPEngine_QueueThread::~CLTThreadPerClientTCPEngine_QueueThread() = default;

// UNANCHORED: scaffold accessor for the recovered child +0x38 owner field
CLTThreadPerClientTCPEngine* CLTThreadPerClientTCPEngine_QueueThread::Owner() const {
    return owner_;
}

// anchor: launcher.exe:0x436fc0
void CLTThreadPerClientTCPEngine_QueueThread::Run() {
    if (owner_) {
        owner_->RunCompletedOperationQueue(/*nonBlocking=*/false);
    }
}

// anchor: launcher.exe:0x431ab0
CLTThreadPerClientTCPEngine_AcceptThread::CLTThreadPerClientTCPEngine_AcceptThread(
    uint32_t listenSocketHandle,
    void* ownerContext)
    : CLTThread("CLTThreadPerClientTCPEngine::AcceptThread"),
      ownerContext_(ownerContext),
      listenSocketHandle_(listenSocketHandle),
      wakeupSocketHandle_(CreateConnectedWakeupSocketHandle()) {}

// anchor: launcher.exe:0x431b30 deleting wrapper / +0x40 wakeup helper teardown
CLTThreadPerClientTCPEngine_AcceptThread::~CLTThreadPerClientTCPEngine_AcceptThread() {
    CloseSocketHandle(&wakeupSocketHandle_);
}

// UNANCHORED: scaffold accessor for recovered child +0x38 owner/context field
void* CLTThreadPerClientTCPEngine_AcceptThread::OwnerContext() const {
    return ownerContext_;
}

// UNANCHORED: scaffold accessor for recovered child +0x3c listening socket field
uint32_t CLTThreadPerClientTCPEngine_AcceptThread::ListenSocketHandle() const {
    return listenSocketHandle_;
}

// UNANCHORED: scaffold accessor for recovered child +0x40 wakeup socket helper field
uint32_t CLTThreadPerClientTCPEngine_AcceptThread::WakeupSocketHandle() const {
    return wakeupSocketHandle_;
}

// anchor: launcher.exe:0x452320 helper family use via child +0x40 wakeup socket
void CLTThreadPerClientTCPEngine_AcceptThread::SignalWakeup() {
    SignalWakeupSocketHandle(wakeupSocketHandle_);
}

// anchor: launcher.exe:0x432070
void CLTThreadPerClientTCPEngine_AcceptThread::Run() {
    // Current source ownership only models the class/vtable/wakeup surface.
    // The full accept loop remains a later fidelity target.
}

// anchor: launcher.exe:0x431b60
CLTThreadPerClientTCPEngine_WorkerThread::CLTThreadPerClientTCPEngine_WorkerThread(
    void* contextKey,
    bool datagramMode)
    : CLTThread("CLTThreadPerClientTCPEngine::WorkerThread"),
      contextKey_(contextKey),
      datagramMode_(datagramMode),
      wakeupSocketHandle_(CreateConnectedWakeupSocketHandle()),
      exitRequested_(false) {}

// anchor: launcher.exe:0x431be0 deleting wrapper / +0x40 wakeup helper teardown
CLTThreadPerClientTCPEngine_WorkerThread::~CLTThreadPerClientTCPEngine_WorkerThread() {
    CloseSocketHandle(&wakeupSocketHandle_);
}

// UNANCHORED: scaffold accessor for recovered child +0x38 context/connection key field
void* CLTThreadPerClientTCPEngine_WorkerThread::ContextKey() const {
    return contextKey_;
}

// UNANCHORED: scaffold accessor for recovered child +0x3c datagram-mode byte
bool CLTThreadPerClientTCPEngine_WorkerThread::DatagramMode() const {
    return datagramMode_;
}

// UNANCHORED: scaffold accessor for recovered child +0x40 wakeup socket helper field
uint32_t CLTThreadPerClientTCPEngine_WorkerThread::WakeupSocketHandle() const {
    return wakeupSocketHandle_;
}

// UNANCHORED: scaffold accessor for recovered child +0x44 exit-request byte
bool CLTThreadPerClientTCPEngine_WorkerThread::ExitRequested() const {
    return exitRequested_;
}

// UNANCHORED: source-owned bridge for the recovered child +0x44 exit-request byte
void CLTThreadPerClientTCPEngine_WorkerThread::RequestExit() {
    exitRequested_ = true;
}

// anchor: launcher.exe:0x452320 helper family use via child +0x40 wakeup socket
void CLTThreadPerClientTCPEngine_WorkerThread::SignalWakeup() {
    SignalWakeupSocketHandle(wakeupSocketHandle_);
}

// anchor: launcher.exe:0x42fe50
void CLTThreadPerClientTCPEngine_WorkerThread::Run() {
    // Current source ownership still does not reimplement the full original select/connect/send/
    // recv/wakeup loop here.
    // Narrow update:
    // - the active TCP receive fragment-production subpath from this function is now mirrored through
    //   `CLTTCPConnection::PollReceiveAndDeliverReadOperationFragmentsScaffold()` on the launcher
    //   bridge path
    // - the remaining broader worker-thread loop fidelity still belongs here later
}

// anchor: launcher.exe:0x436340
void CLTThreadPerClientTCPEngine::Queue_Free(CLTThreadPerClientTCPEngine_Queue* queue) {
    if (!queue) {
        return;
    }

    if (queue->slotsBase) {
        uint32_t** slotsBase = static_cast<uint32_t**>(queue->slotsBase);
        uint32_t** slotsCurrent = static_cast<uint32_t**>(queue->slotsCurrent);
        uint32_t** slotsLast = static_cast<uint32_t**>(queue->slotsLast);
        if (slotsCurrent && slotsLast && slotsCurrent <= slotsLast) {
            for (uint32_t** slot = slotsCurrent; slot <= slotsLast; ++slot) {
                if (*slot) {
                    std::free(*slot);
                    *slot = nullptr;
                }
            }
        }
        std::free(slotsBase);
    }

    QueueFreeRecycledBlocksScaffold(queue);
    std::memset(queue, 0, sizeof(*queue));
}

// anchor: launcher.exe:0x436340
bool CLTThreadPerClientTCPEngine::Queue_Init(
    CLTThreadPerClientTCPEngine_Queue* queue,
    uint32_t initialSize) {
    if (!queue) {
        return false;
    }

    Queue_Free(queue);

    uint32_t blockCount = (initialSize >> 4) + 1;
    uint32_t slotCapacity = blockCount + 2;
    if (slotCapacity < 8) {
        slotCapacity = 8;
    }

    uint32_t** slotsBase = static_cast<uint32_t**>(std::calloc(slotCapacity, sizeof(uint32_t*)));
    if (!slotsBase) {
        return false;
    }

    const uint32_t firstIndex = (slotCapacity - blockCount) >> 1;
    for (uint32_t i = 0; i < blockCount; ++i) {
        slotsBase[firstIndex + i] = static_cast<uint32_t*>(std::calloc(1, 0x80));
        if (!slotsBase[firstIndex + i]) {
            queue->slotsBase = slotsBase;
            queue->slotsCurrent = slotsBase + firstIndex;
            queue->slotsLast = slotsBase + firstIndex + i;
            Queue_Free(queue);
            return false;
        }
    }

    uint32_t** slotsCurrent = slotsBase + firstIndex;
    uint32_t** slotsLast = slotsCurrent + blockCount - 1;
    uint8_t* block0 = reinterpret_cast<uint8_t*>(*slotsCurrent);
    uint8_t* block1 = reinterpret_cast<uint8_t*>(*slotsLast);

    queue->slotsBase = slotsBase;
    queue->slotCapacity = slotCapacity;
    queue->slotsCurrent = slotsCurrent;
    queue->slotsLast = slotsLast;
    queue->block0 = block0;
    queue->end0 = block0 ? (block0 + 0x80) : nullptr;
    queue->current0 = block0;
    queue->block1 = block1;
    queue->end1 = block1 ? (block1 + 0x80) : nullptr;
    queue->current1 = block1 ? (block1 + ((initialSize & 0xfu) * 8u)) : nullptr;
    return true;
}

CLTThreadPerClientTCPEngine_NativePrimaryVptrExperimentInfo
CLTThreadPerClientTCPEngine::CollectNativePrimaryVptrExperimentInfoScaffold() {
    CLTThreadPerClientTCPEngine_NativePrimaryVptrExperimentInfo info = {};
    CLTThreadPerClientTCPEngine probe;

    const auto memberOffset = [&probe](const auto& member) -> size_t {
        return static_cast<size_t>(
            reinterpret_cast<const unsigned char*>(&member) -
            reinterpret_cast<const unsigned char*>(&probe));
    };

    info.objectSize = sizeof(CLTThreadPerClientTCPEngine);
    info.waitHelperSize = sizeof(CLTThreadPerClientTCPEngine_WaitHelperScaffold);
    info.lockHelperSize = sizeof(CLTThreadPerClientTCPEngine_LockHelperScaffold);
    info.attachmentSize = sizeof(CLTThreadPerClientTCPEngine_LauncherAbiAttachment);
    info.offsetField04 = memberOffset(probe.ctorFlagsField04Scaffold_);
    info.offsetField08 = memberOffset(probe.queueThreadArrayField08Scaffold_);
    info.offsetQueue0C = memberOffset(probe.ownedQueue0CScaffold_);
    info.offsetQueue34 = memberOffset(probe.ownedQueue34Scaffold_);
    info.offsetWaitHelper5C = memberOffset(probe.ownedWaitHelper5CScaffold_);
    info.offsetQueueLockHelper60 = memberOffset(probe.ownedQueueLockHelper60Scaffold_);
    info.offsetQueueSignalEvent7C = memberOffset(probe.ownedQueueSignalEvent7CScaffold_);
    info.offsetEndpointTreeHead80 = memberOffset(probe.ownedEndpointTreeHead80Scaffold_);
    info.offsetEndpointCount84 = memberOffset(probe.ownedEndpointCount84Scaffold_);
    info.offsetReserved88 = memberOffset(probe.reserved88Scaffold_);
    info.offsetContextTreeHead8C = memberOffset(probe.ownedContextTreeHead8CScaffold_);
    info.offsetContextCount90 = memberOffset(probe.ownedContextCount90Scaffold_);
    info.offsetReserved94 = memberOffset(probe.reserved94Scaffold_);
    info.offsetCleanupLockHelper98 = memberOffset(probe.ownedCleanupLockHelper98Scaffold_);
    info.offsetAttachment = memberOffset(probe.attachedLauncherAbiSurfaceScaffold_);

    info.livePrimaryVptrAddressPoint = *reinterpret_cast<void* const*>(&probe);
    if (info.livePrimaryVptrAddressPoint) {
        const uintptr_t* vtableWords =
            reinterpret_cast<const uintptr_t*>(info.livePrimaryVptrAddressPoint);
        info.rawPrimaryVtableBaseGuess =
            reinterpret_cast<uintptr_t>(info.livePrimaryVptrAddressPoint) - 8u;
        info.offsetToTop = static_cast<intptr_t>(vtableWords[-2]);
        info.typeinfo = reinterpret_cast<void*>(vtableWords[-1]);
    }

    return info;
}

bool CLTThreadPerClientTCPEngine::IsLauncherArg5PrimaryLayoutCompatibleForNativeVptrExperimentScaffold(
    CLTThreadPerClientTCPEngine_NativePrimaryVptrExperimentInfo* outInfo) {
    const CLTThreadPerClientTCPEngine_NativePrimaryVptrExperimentInfo info =
        CollectNativePrimaryVptrExperimentInfoScaffold();
    if (outInfo) {
        *outInfo = info;
    }

    // Conservative gate for the direct-native-vptr experiment:
    // - launcher arg5 shell is still the original-sized 0xb4 object
    // - the native class must not only expose a live GCC address-point, it must also keep the
    //   shell-visible field/subobject offsets compatible enough for native methods to read `this`
    //   safely
    // - current required landmarks are the concrete shell offsets consumed by original/client code
    //   or by any native method that would run directly after the vptr swap
    return info.livePrimaryVptrAddressPoint != nullptr &&
        info.objectSize == 0xb4u &&
        info.waitHelperSize == 0x04u &&
        info.lockHelperSize == 0x1cu &&
        info.offsetField04 == 0x04u &&
        info.offsetField08 == 0x08u &&
        info.offsetQueue0C == 0x0cu &&
        info.offsetQueue34 == 0x34u &&
        info.offsetWaitHelper5C == 0x5cu &&
        info.offsetQueueLockHelper60 == 0x60u &&
        info.offsetQueueSignalEvent7C == 0x7cu &&
        info.offsetEndpointTreeHead80 == 0x80u &&
        info.offsetEndpointCount84 == 0x84u &&
        info.offsetReserved88 == 0x88u &&
        info.offsetContextTreeHead8C == 0x8cu &&
        info.offsetContextCount90 == 0x90u &&
        info.offsetReserved94 == 0x94u &&
        info.offsetCleanupLockHelper98 == 0x98u;
}

// anchor: launcher.exe:0x436670 selected-queue push body reached from `0x436820`
void CLTThreadPerClientTCPEngine::Queue_PushPair(
    CLTThreadPerClientTCPEngine_Queue* queue,
    uint32_t value0,
    uint32_t value1) {
    if (!queue || !queue->current1) {
        return;
    }

    uint8_t* lastPairInBlock = queue->end1 ? (static_cast<uint8_t*>(queue->end1) - 8) : nullptr;
    if (static_cast<void*>(queue->current1) == static_cast<void*>(lastPairInBlock)) {
        CLTThreadPerClientTCPEngine_QueuePair pair = {value0, value1};
        (void)GrowQueue(queue, &pair);
        return;
    }

    uint32_t* current1 = static_cast<uint32_t*>(queue->current1);
    current1[0] = value0;
    current1[1] = value1;
    queue->current1 = current1 + 2;
}

// anchor: launcher.exe:0x436b10 / client.dll:0x62531c10 empty-queue check shape
bool CLTThreadPerClientTCPEngine::Queue_IsEmpty(const CLTThreadPerClientTCPEngine_Queue* queue) {
    return !queue || !queue->current0 || (queue->current1 == queue->current0);
}

// anchor: launcher.exe:0x436d31..0x436ee7 consumer pop shape
bool CLTThreadPerClientTCPEngine::Queue_TryPopPair(
    CLTThreadPerClientTCPEngine_Queue* queue,
    CLTThreadPerClientTCPEngine_QueuedPair* outPair) {
    if (!queue || !outPair || Queue_IsEmpty(queue)) {
        return false;
    }

    uint32_t* current0 = static_cast<uint32_t*>(queue->current0);
    outPair->value0 = current0[0];
    outPair->value1 = current0[1];

    uint8_t* lastPairInBlock = queue->end0 ? (static_cast<uint8_t*>(queue->end0) - 8) : nullptr;
    if (static_cast<void*>(queue->current0) == static_cast<void*>(lastPairInBlock)) {
        uint32_t* oldBlock = static_cast<uint32_t*>(queue->block0);
        uint32_t** slotsCurrent = static_cast<uint32_t**>(queue->slotsCurrent);
        uint32_t** slotsLast = static_cast<uint32_t**>(queue->slotsLast);
        if (!slotsCurrent || slotsCurrent >= slotsLast) {
            // No later block is active; the queue just becomes empty within the current block.
            // The recovered free-list behavior matters on the cross-block path below, not here.
            queue->current0 = current0 + 2;
            return true;
        }

        if (oldBlock) {
            // Current bounded fidelity step:
            // - original `0x436d31..0x436ee7` uses block recycling / free-list behavior when the
            //   dequeue cursor leaves a full `0x80` block
            // - current source now mirrors that more closely by caching the exhausted head block for
            //   later `Queue_PushPair` growth reuse instead of freeing it immediately
            QueueRecycleBlockScaffold(queue, oldBlock);
        }
        ++slotsCurrent;
        queue->slotsCurrent = slotsCurrent;
        uint32_t* newBlock = *slotsCurrent;
        queue->block0 = newBlock;
        queue->end0 = newBlock ? (static_cast<uint8_t*>(static_cast<void*>(newBlock)) + 0x80) : nullptr;
        queue->current0 = newBlock;
        return true;
    }

    queue->current0 = current0 + 2;
    return true;
}

// UNANCHORED internal helper for the current source-side consumer scaffold.
// Current best anchor for the type read itself is launcher helper 0x4816f0,
// which returns [workItem+0x04]. We model that as a documented header view rather than
// open-coded pointer arithmetic.
static uint32_t QueueWorkItem_GetType(const void* workItem) {
    if (!workItem) {
        return 0;
    }
    const CLTThreadPerClientTCPEngine_WorkItemHeader* header =
        static_cast<const CLTThreadPerClientTCPEngine_WorkItemHeader*>(workItem);
    return header->workType;
}

// UNANCHORED internal helper for the current source-side consumer scaffold.
// Current best consumer anchor is the trailing work-item release in 0x436d31..0x436ee7.
static void QueueWorkItem_Release(void* workItem) {
    if (!workItem) {
        return;
    }

    void** vtable = *reinterpret_cast<void***>(workItem);
    if (!vtable || !vtable[1]) {
        return;
    }

    typedef uint32_t (__thiscall *ReleaseFn)(void*);
    ReleaseFn fn = reinterpret_cast<ReleaseFn>(vtable[1]);
    (void)fn(workItem);
}

// UNANCHORED internal helper for the current source-side consumer scaffold.
// Current best consumer anchor is context->+0x10(workItem) in 0x436d31..0x436ee7.
static void QueueContext_OnOperationCompleted(void* context, void* workItem) {
    if (!context) {
        return;
    }

    void** vtable = *reinterpret_cast<void***>(context);
    if (!vtable || !vtable[4]) {
        return;
    }

    typedef uint32_t (__thiscall *OnOperationCompletedFn)(void*, void*);
    OnOperationCompletedFn fn = reinterpret_cast<OnOperationCompletedFn>(vtable[4]);
    (void)fn(context, workItem);
}

// UNANCHORED internal helper for the current source-side consumer scaffold.
// Current best consumer anchor is the conditional context->+0x04 release after type-1 work.
static void QueueContext_Release(void* context) {
    if (!context) {
        return;
    }

    void** vtable = *reinterpret_cast<void***>(context);
    if (!vtable || !vtable[1]) {
        return;
    }

    typedef uint32_t (__thiscall *ReleaseFn)(void*);
    ReleaseFn fn = reinterpret_cast<ReleaseFn>(vtable[1]);
    (void)fn(context);
}

// UNANCHORED internal helper for the current source-side consumer scaffold.
// Current best consumer anchor is the `(char)context[1]` test in 0x436d31..0x436ee7.
static bool QueueContext_ShouldAutoReleaseAfterType1(void* context) {
    if (!context) {
        return false;
    }

    const uint8_t* bytes = static_cast<const uint8_t*>(context);
    return bytes[4] != 0;
}

// Keep the implementation intentionally conservative.
// These methods currently provide original-name structure and evidence-backed state
// shaping, but they are not yet the fully faithful runtime path used by arg5.

// anchor: launcher.exe:0x431c30
// vtable: launcher.exe:0x004b2768
// current constructor-layout anchors that should stay aligned with source comments/docs:
// - base ctor `0x4366f0`
// - queue-pair init `0x436610 -> 0x436340(size=0)` covering the `+0x0c` / `+0x34` pair
// - base helper roots at `+0x5c` / `+0x60`
// - CreateEventA result at `+0x7c`
// - derived list heads at `+0x80` (0x24 bytes) and `+0x8c` (0x18 bytes)
// - derived helper root at `+0x98`
// UNANCHORED: no original launcher.exe anchor assigned yet.
bool CLTThreadPerClientTCPEngine::InitializeHeapBackedCriticalSectionScaffold(void** outCritStorage) {
    if (!outCritStorage) {
        return false;
    }

    _RTL_CRITICAL_SECTION* crit =
        static_cast<_RTL_CRITICAL_SECTION*>(std::malloc(sizeof(CRITICAL_SECTION)));
    if (!crit) {
        return false;
    }

    InitializeCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(crit));
    *outCritStorage = crit;
    return true;
}

void CLTThreadPerClientTCPEngine::DeleteHeapBackedCriticalSectionScaffold(void** critStorage) {
    if (!critStorage || !*critStorage) {
        return;
    }

    _RTL_CRITICAL_SECTION* crit = CriticalSectionFromOpaqueStorage(*critStorage);
    DeleteCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(crit));
    std::free(crit);
    *critStorage = nullptr;
}

void CLTThreadPerClientTCPEngine::InitializeEndpointTreeHead24Scaffold(
    CLTThreadPerClientTCPEngine_EndpointTreeHead24* head) {
    if (!head) {
        return;
    }

    std::memset(head, 0, sizeof(*head));
    head->root = nullptr;
    head->first = head;
    head->last = head;
}

void CLTThreadPerClientTCPEngine::InitializeContextTreeHead18Scaffold(
    CLTThreadPerClientTCPEngine_ContextTreeHead18* head) {
    if (!head) {
        return;
    }

    std::memset(head, 0, sizeof(*head));
    head->root = nullptr;
    head->first = head;
    head->last = head;
}

void CLTThreadPerClientTCPEngine::SetEndpointTreeHead24OccupancyScaffold(
    CLTThreadPerClientTCPEngine_EndpointTreeHead24* head,
    bool nonEmpty) {
    if (!head) {
        return;
    }
    if (!nonEmpty) {
        InitializeEndpointTreeHead24Scaffold(head);
        return;
    }

    head->root = &head->keyAndPayload[0];
    head->first = &head->keyAndPayload[4];
    head->last = &head->keyAndPayload[8];
}

void CLTThreadPerClientTCPEngine::SetContextTreeHead18OccupancyScaffold(
    CLTThreadPerClientTCPEngine_ContextTreeHead18* head,
    bool nonEmpty) {
    if (!head) {
        return;
    }
    if (!nonEmpty) {
        InitializeContextTreeHead18Scaffold(head);
        return;
    }

    head->root = &head->keyAndPayload[0];
    head->first = &head->keyAndPayload[0];
    head->last = &head->keyAndPayload[4];
}

CLTThreadPerClientTCPEngine::CLTThreadPerClientTCPEngine()
    : ctorFlagsField04Scaffold_(0),
      queueThreadArrayField08Scaffold_(nullptr),
      ownedQueue0CScaffold_(),
      ownedQueue34Scaffold_(),
      ownedWaitHelper5CScaffold_{nullptr},
      ownedQueueLockHelper60Scaffold_{nullptr, nullptr},
      ownedQueueSignalEvent7CScaffold_(nullptr),
      ownedEndpointTreeHead80Scaffold_(nullptr),
      ownedEndpointCount84Scaffold_(0),
      reserved88Scaffold_(0),
      ownedContextTreeHead8CScaffold_(nullptr),
      ownedContextCount90Scaffold_(0),
      reserved94Scaffold_(0),
      ownedCleanupLockHelper98Scaffold_{nullptr, nullptr},
      attachedLauncherAbiSurfaceScaffold_(),
      authBridgeContextScaffold_(nullptr),
      marginBridgeContextScaffold_(nullptr),
      queueThreads_(),
      monitoredPorts_(),
      workerThreads_(),
      messageConnections_() {
    // anchor: launcher.exe:0x4366f0
    // Source-owned ownership move from the arg5 ABI shell:
    // - the target class now owns fallback/live surrogates for the recovered ctor-visible queue
    //   pair, helper families, event handle, and sentinel-headed container surfaces
    // - the wrapper still supplies the raw shell addresses consumed directly by original code, but
    //   that shell now mirrors this class state instead of remaining the only owner of it
    Queue_Init(&ownedQueue0CScaffold_, 0);
    Queue_Init(&ownedQueue34Scaffold_, 0);
    ownedQueueSignalEvent7CScaffold_ = CreateEventA(NULL, FALSE, FALSE, NULL);
    (void)InitializeHeapBackedCriticalSectionScaffold(
        &ownedQueueLockHelper60Scaffold_.criticalSectionStorage);
    (void)InitializeHeapBackedCriticalSectionScaffold(
        &ownedCleanupLockHelper98Scaffold_.criticalSectionStorage);

    ownedEndpointTreeHead80Scaffold_ =
        static_cast<CLTThreadPerClientTCPEngine_EndpointTreeHead24*>(
            std::malloc(sizeof(CLTThreadPerClientTCPEngine_EndpointTreeHead24)));
    if (ownedEndpointTreeHead80Scaffold_) {
        InitializeEndpointTreeHead24Scaffold(ownedEndpointTreeHead80Scaffold_);
    }

    ownedContextTreeHead8CScaffold_ =
        static_cast<CLTThreadPerClientTCPEngine_ContextTreeHead18*>(
            std::malloc(sizeof(CLTThreadPerClientTCPEngine_ContextTreeHead18)));
    if (ownedContextTreeHead8CScaffold_) {
        InitializeContextTreeHead18Scaffold(ownedContextTreeHead8CScaffold_);
    }

    RefreshOwnedLauncherMirrorStateScaffold();

    // Original base ctor 0x4366f0 allocates queue-thread children only when the effective
    // ctor flag/count at +0x04 is non-zero. The current scaffold still enters through a
    // zero-count binder path, so keep the recovered child family in source but default it empty.
    RebuildQueueThreadsForCtorCount(/*queueThreadCount=*/0);
}

// anchor: launcher.exe:0x40b389..0x40b404 teardown releases arg5 through vtable slot 0
// vtable: launcher.exe:0x004b2768
// NOTE: starter C++ destructor only models local sidecar cleanup, not the full original dtor body.
CLTThreadPerClientTCPEngine::~CLTThreadPerClientTCPEngine() {
    DetachLauncherAbiSurfaceScaffold();

    for (AcceptThreadRecord& record : monitoredPorts_) {
        StopAcceptThreadScaffold(&record);
    }
    monitoredPorts_.clear();

    for (WorkerThreadRecord& record : workerThreads_) {
        StopWorkerThreadScaffold(&record);
    }
    workerThreads_.clear();

    for (CMessageConnection* connection : messageConnections_) {
        delete connection;
    }
    messageConnections_.clear();

    Queue_Free(&ownedQueue0CScaffold_);
    Queue_Free(&ownedQueue34Scaffold_);
    DeleteHeapBackedCriticalSectionScaffold(&ownedQueueLockHelper60Scaffold_.criticalSectionStorage);
    DeleteHeapBackedCriticalSectionScaffold(&ownedCleanupLockHelper98Scaffold_.criticalSectionStorage);
    if (ownedQueueSignalEvent7CScaffold_) {
        CloseHandle(static_cast<HANDLE>(ownedQueueSignalEvent7CScaffold_));
        ownedQueueSignalEvent7CScaffold_ = nullptr;
    }
    if (ownedEndpointTreeHead80Scaffold_) {
        std::free(ownedEndpointTreeHead80Scaffold_);
        ownedEndpointTreeHead80Scaffold_ = nullptr;
    }
    if (ownedContextTreeHead8CScaffold_) {
        std::free(ownedContextTreeHead8CScaffold_);
        ownedContextTreeHead8CScaffold_ = nullptr;
    }
}

// anchor: launcher.exe:0x4319a0
// vtable: launcher.exe:0x004b2768 slot +0x00
int CLTThreadPerClientTCPEngine::Release(uint32_t flags) {
    // Current sidecar owner still handles the real arg5 object lifetime/teardown.
    // Keep the primary-slot surface source-owned here so wrappers can forward through
    // ILTTCPEngine without open-coding placeholder returns.
    (void)flags;
    return 1;
}

// anchor: launcher.exe:0x431ce0
// vtable: launcher.exe:0x004b2768 slot +0x04
uint32_t CLTThreadPerClientTCPEngine::MonitorPort(uint16_t portHostOrder, void* ownerContext, void* reservedArg3) {
    const uint32_t ipv4NetworkOrder = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(reservedArg3));
    const LTTCPEndpointKey key = MakeEndpointKey(portHostOrder, ipv4NetworkOrder);
    if (FindMonitoredPort(key)) {
        return kResultAlreadyMonitored;
    }

    AcceptThreadRecord record = {};
    record.endpoint = key;
    record.ownerContext = ownerContext;
    record.listenSocketHandle = OpenTcpListenSocket(portHostOrder, ipv4NetworkOrder);
    if (record.listenSocketHandle == kInvalidSocketHandle) {
        return 0;
    }

    record.thread = std::make_unique<CLTThreadPerClientTCPEngine_AcceptThread>(
        record.listenSocketHandle,
        ownerContext);
    if (!record.thread) {
        CloseSocketHandle(&record.listenSocketHandle);
        return 0;
    }

    monitoredPorts_.push_back(std::move(record));
    (void)monitoredPorts_.back().thread->Start(/*startPriority=*/2);
    SyncAttachedLauncherObjectStateScaffold();
    return kResultSuccess;
}

// anchor: launcher.exe:0x4325d0
// vtable: launcher.exe:0x004b2768 slot +0x08
uint32_t CLTThreadPerClientTCPEngine::UDPMonitorPort(uint16_t portHostOrder, void* contextKey, void* ownerContext) {
    const uint32_t socketHandle = OpenUdpMonitorSocket(portHostOrder, /*ipv4NetworkOrder=*/0);
    if (socketHandle == kInvalidSocketHandle) {
        return 0;
    }

    WorkerThreadRecord* worker = CreateOrReplaceWorkerThreadScaffold(
        contextKey,
        ownerContext,
        socketHandle,
        LTTCPEngineConnectionState::kUdpMonitorActive,
        /*datagramMode=*/true);
    if (!worker) {
        uint32_t socketHandleToClose = socketHandle;
        CloseSocketHandle(&socketHandleToClose);
        return 0;
    }
    SyncAttachedLauncherObjectStateScaffold();
    return kResultSuccess;
}

// anchor: launcher.exe:0x436000
// vtable: launcher.exe:0x004b2768 slot +0x0c
uint32_t CLTThreadPerClientTCPEngine::MonitorEphemeralUDPPort(uint16_t* outBoundPortHostOrder, void* contextKey, void* ownerContext) {
    // Current best static read: thin helper around slot 2 / UDPMonitorPort(port=0, ...)
    // followed by getsockname/ntohs to report the chosen local port.
    const uint32_t result = UDPMonitorPort(/*portHostOrder=*/0, contextKey, ownerContext);
    if (result == kResultSuccess && outBoundPortHostOrder) {
        *outBoundPortHostOrder = 0;
        if (WorkerThreadRecord* worker = FindWorker(contextKey)) {
            sockaddr_in boundAddr = {};
            int boundAddrSize = sizeof(boundAddr);
            if (worker->socketHandle != kInvalidSocketHandle &&
                getsockname(
                    static_cast<SOCKET>(worker->socketHandle),
                    reinterpret_cast<sockaddr*>(&boundAddr),
                    &boundAddrSize) == 0) {
                *outBoundPortHostOrder = ntohs(boundAddr.sin_port);
            }
        }
    }
    return result;
}

// anchor: launcher.exe:0x42f7c0
// vtable: launcher.exe:0x004b2768 slot +0x10
uint32_t CLTThreadPerClientTCPEngine::Slot4_42F7C0(void* arg1) {
    (void)arg1;
    return 0;
}

// anchor: launcher.exe:0x431840
// vtable: launcher.exe:0x004b2768 slot +0x14
uint32_t CLTThreadPerClientTCPEngine::UnmonitorPort(uint16_t portHostOrder, uint32_t* outSocketHandle, uint32_t ipv4NetworkOrder) {
    const LTTCPEndpointKey key = MakeEndpointKey(portHostOrder, ipv4NetworkOrder);
    for (auto it = monitoredPorts_.begin(); it != monitoredPorts_.end(); ++it) {
        if (it->endpoint.portNetworkOrder == key.portNetworkOrder &&
            it->endpoint.ipv4NetworkOrder == key.ipv4NetworkOrder) {
            if (outSocketHandle) {
                *outSocketHandle = it->listenSocketHandle;
            }
            StopAcceptThreadScaffold(&(*it));
            monitoredPorts_.erase(it);
            SyncAttachedLauncherObjectStateScaffold();
            return 0;
        }
    }

    if (outSocketHandle) {
        *outSocketHandle = 0;
    }
    return kResultEndpointNotFound;
}

// UNANCHORED source-side helper used by the current connection scaffolding.
// Current original anchor is the lower-level connect family at launcher.exe:0x4328a0.
uint32_t CLTThreadPerClientTCPEngine::ConnectResolvedEndpointScaffold(uint16_t portHostOrder, uint32_t ipv4NetworkOrder, void* contextKey, void* ownerContext) {
    void* normalizedContextKey = ResolveEngineContextKeyScaffold(contextKey);
    void* normalizedOwnerContext = ResolveEngineContextKeyScaffold(ownerContext ? ownerContext : contextKey);
    if (!normalizedContextKey || !EnsureWinsockReady()) {
        return 0;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        return 0;
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(portHostOrder);
    addr.sin_addr.s_addr = ipv4NetworkOrder;

    if (connect(sock, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        closesocket(sock);
        return 0;
    }

    WorkerThreadRecord* worker = CreateOrReplaceWorkerThreadScaffold(
        normalizedContextKey,
        normalizedOwnerContext,
        static_cast<uint32_t>(sock),
        LTTCPEngineConnectionState::kConnectActive,
        /*datagramMode=*/false);
    if (!worker) {
        uint32_t socketHandleToClose = static_cast<uint32_t>(sock);
        CloseSocketHandle(&socketHandleToClose);
        return 0;
    }
    SyncAttachedLauncherObjectStateScaffold();
    return kResultSuccess;
}

// UNANCHORED source-side helper used by the current CMessageConnection scaffolding.
uint32_t CLTThreadPerClientTCPEngine::ConnectConnectionScaffold(CLTTCPConnection* connection) {
    if (!connection) {
        return 0;
    }

    const LTTCPEndpointKey& endpoint = connection->RemoteEndpoint();
    const uint16_t portHostOrder =
        static_cast<uint16_t>((endpoint.portNetworkOrder << 8) | (endpoint.portNetworkOrder >> 8));

    uint32_t ipv4NetworkOrder = endpoint.ipv4NetworkOrder;
    if (ipv4NetworkOrder == 0 && !connection->RemoteHostName().empty()) {
        ResolveIpv4Address(connection->RemoteHostName().c_str(), &ipv4NetworkOrder);
    }
    if (ipv4NetworkOrder == 0) {
        spdlog::debug(
            "CLTThreadPerClientTCPEngine::Connect failed to resolve remote host '{}'",
            connection->RemoteHostName().empty() ? std::string("<empty>") : connection->RemoteHostName());
        return 0;
    }

    void* contextKey = connection->OwnerContext() ? connection->OwnerContext() : static_cast<void*>(connection);
    const uint32_t result = ConnectResolvedEndpointScaffold(
        portHostOrder,
        ipv4NetworkOrder,
        /*contextKey=*/contextKey,
        /*ownerContext=*/connection->OwnerContext());
    if (result == kResultSuccess) {
        connection->SetState(LTTCPEngineConnectionState::kConnectActive);
        if (WorkerThreadRecord* worker = FindWorker(contextKey)) {
            connection->SetSocketHandle(worker->socketHandle);
        }
        SyncAttachedLauncherObjectStateScaffold();
    }
    return result;
}

// anchor: launcher.exe:0x4328a0
// vtable: launcher.exe:0x004b2768 slot +0x18
uint32_t CLTThreadPerClientTCPEngine::Connect(void* contextKey) {
    CMessageConnection* connection = ResolveConnectionForEngineSlotScaffold(
        contextKey,
        /*allowCreateFallback=*/true);
    if (!connection) {
        spdlog::debug(
            "CLTThreadPerClientTCPEngine::Connect couldn't resolve connection context={} normalizedContext={}",
            fmt::ptr(contextKey),
            fmt::ptr(ResolveEngineContextKeyScaffold(contextKey)));
        return 0u;
    }
    return connection->EnsureConnected();
}

// UNANCHORED source-side helper used by the current CMessageConnection scaffolding.
uint32_t CLTThreadPerClientTCPEngine::CloseConnectionScaffold(CLTTCPConnection* connection, bool graceful) {
    if (!connection) {
        return 0;
    }

    const uint32_t result = connection->CloseSocketTransportScaffold(graceful);
    if (result != 0) {
        WorkerThreadRecord* worker = FindWorker(connection);
        if (!worker && connection->OwnerContext()) {
            worker = FindWorker(connection->OwnerContext());
        }
        if (worker) {
            worker->socketHandle = connection->SocketHandle();
            worker->state = connection->State();
            SyncConnectionFromWorkerRecordScaffold(worker);
            if (worker->thread) {
                worker->thread->RequestExit();
                worker->thread->SignalWakeup();
            }
        }
        SyncAttachedLauncherObjectStateScaffold();
    }
    return result;
}

// anchor: launcher.exe:0x42f970
// vtable: launcher.exe:0x004b2768 slot +0x1c
uint32_t CLTThreadPerClientTCPEngine::Close(void* contextKey, bool graceful) {
    CMessageConnection* connection = ResolveConnectionForEngineSlotScaffold(
        contextKey,
        /*allowCreateFallback=*/false);
    if (!connection) {
        spdlog::debug(
            "CLTThreadPerClientTCPEngine::Close couldn't resolve existing connection context={} normalizedContext={}",
            fmt::ptr(contextKey),
            fmt::ptr(ResolveEngineContextKeyScaffold(contextKey)));
        return 0u;
    }
    return CloseConnectionScaffold(static_cast<CLTTCPConnection*>(connection), graceful);
}

// UNANCHORED source-side helper used by the current CMessageConnection scaffolding.
uint32_t CLTThreadPerClientTCPEngine::SendBufferConnectionScaffold(CLTTCPConnection* connection, const void* buffer, uint32_t byteCount, void* completionContext) {
    if (!connection) {
        return 0;
    }
    return connection->SendRawSocketBufferScaffold(buffer, byteCount, completionContext);
}

// anchor: launcher.exe:0x42fbd0
// vtable: launcher.exe:0x004b2768 slot +0x20
uint32_t CLTThreadPerClientTCPEngine::SendBuffer(void* contextKey, const void* buffer, uint32_t byteCount, void* completionContext) {
    CMessageConnection* connection = ResolveConnectionForEngineSlotScaffold(
        contextKey,
        /*allowCreateFallback=*/false);
    if (!connection) {
        spdlog::debug(
            "CLTThreadPerClientTCPEngine::SendBuffer couldn't resolve existing connection context={} normalizedContext={} byteCount={}",
            fmt::ptr(contextKey),
            fmt::ptr(ResolveEngineContextKeyScaffold(contextKey)),
            byteCount);
        return 0u;
    }
    return connection->SendPacket(buffer, byteCount, completionContext);
}

// anchor: launcher.exe:0x42fd10
// vtable: launcher.exe:0x004b2768 slot +0x24
uint32_t CLTThreadPerClientTCPEngine::Slot9_42FD10(void* arg1, void* arg2, void* arg3, void* arg4, void* arg5) {
    (void)arg1;
    (void)arg2;
    (void)arg3;
    (void)arg4;
    (void)arg5;
    return 0;
}

// anchor: launcher.exe:0x443810
// vtable: launcher.exe:0x004b2768 slot +0x28
uint32_t CLTThreadPerClientTCPEngine::Slot10_443810(void* arg1) {
    (void)arg1;
    return 0;
}

// anchor: launcher.exe:0x431670
// vtable: launcher.exe:0x004b2768 slot +0x2c
uint32_t CLTThreadPerClientTCPEngine::Slot11_431670(void* arg1, uint32_t* out0, uint32_t* out1) {
    (void)arg1;
    if (out0) {
        *out0 = 0;
    }
    if (out1) {
        *out1 = 0;
    }
    return 0;
}

// anchor: launcher.exe:0x4316a0
// vtable: launcher.exe:0x004b2768 slot +0x30
uint32_t CLTThreadPerClientTCPEngine::CleanupConnection(void* contextKey) {
    // Current bounded fidelity correction:
    // - original queue consumers dequeue a real connection-family object as `context` before
    //   calling arg5 slot 12 / CleanupConnection
    // - current source often queues the explicit `CBaseConnection_QueueContextScaffold` bridge
    //   instead so later callback dispatch can still land on `vtable[4]`
    // - slot-12-style worker lookup/teardown therefore has to unwrap that bridge back to the
    //   owning connection's logical context key instead of searching worker/message tables with
    //   the bridge pointer itself
    // - original `0x4316a0` also acquires arg5 helper `+0x98`; after the current ownership move,
    //   that lock behavior now lives here on the target class side and the shell wrapper only
    //   forwards the primary slot call
    (void)EnterCleanupLockHelperScaffold();

    CBaseConnection* queuedConnectionOwner = CBaseConnection_FromQueueContextScaffold(contextKey);
    void* cleanupContextKey = CBaseConnection_ResolveQueueCleanupContextKeyScaffold(contextKey);
    bool touchedConnectionState = false;
    uint32_t result = 0u;

    if (CLTTCPConnection* queuedTcpConnection =
            dynamic_cast<CLTTCPConnection*>(queuedConnectionOwner)) {
        queuedTcpConnection->SetState(LTTCPEngineConnectionState::kClosed);
        queuedTcpConnection->SetSocketHandle(kInvalidSocketHandle);
        touchedConnectionState = true;
    }

    if (CMessageConnection* connection = FindMessageConnection(cleanupContextKey)) {
        connection->SetState(LTTCPEngineConnectionState::kClosed);
        connection->SetSocketHandle(kInvalidSocketHandle);
        touchedConnectionState = true;
    }

    for (auto it = workerThreads_.begin(); it != workerThreads_.end(); ++it) {
        if (it->contextKey == cleanupContextKey) {
            StopWorkerThreadScaffold(&(*it));
            workerThreads_.erase(it);
            (void)DropMessageConnection(cleanupContextKey);
            result = kResultSuccess;
            goto cleanup_tail;
        }
    }

    {
        const bool droppedGenericConnection = DropMessageConnection(cleanupContextKey);
        if (!touchedConnectionState && !droppedGenericConnection) {
            spdlog::debug(
                "CLTThreadPerClientTCPEngine::CleanupConnection couldn't find socket/context key={} normalizedKey={} owner={}",
                fmt::ptr(contextKey),
                fmt::ptr(cleanupContextKey),
                fmt::ptr(queuedConnectionOwner));
        }
        result = (touchedConnectionState || droppedGenericConnection) ? kResultSuccess : 0u;
    }

cleanup_tail:
    SyncAttachedLauncherObjectStateScaffold();
    (void)LeaveCleanupLockHelperScaffold();
    return result;
}

// UNANCHORED: launcher ABI-shell attachment/mirror entrypoint.
void CLTThreadPerClientTCPEngine::AttachLauncherAbiSurfaceScaffold(
    const CLTThreadPerClientTCPEngine_LauncherAbiAttachment& attachment) {
    attachedLauncherAbiSurfaceScaffold_ = attachment;
    SyncAttachedLauncherObjectStateScaffold();
}

// UNANCHORED: launcher ABI-shell detach/reset helper.
void CLTThreadPerClientTCPEngine::DetachLauncherAbiSurfaceScaffold() {
    if (attachedLauncherAbiSurfaceScaffold_.field04CtorFlags) {
        *attachedLauncherAbiSurfaceScaffold_.field04CtorFlags = 0u;
    }
    if (attachedLauncherAbiSurfaceScaffold_.field08QueueThreadArray) {
        *attachedLauncherAbiSurfaceScaffold_.field08QueueThreadArray = nullptr;
    }
    if (attachedLauncherAbiSurfaceScaffold_.list80EndpointTreeHead) {
        *attachedLauncherAbiSurfaceScaffold_.list80EndpointTreeHead = nullptr;
    }
    if (attachedLauncherAbiSurfaceScaffold_.field84EndpointCount) {
        *attachedLauncherAbiSurfaceScaffold_.field84EndpointCount = 0u;
    }
    if (attachedLauncherAbiSurfaceScaffold_.list8CContextTreeHead) {
        *attachedLauncherAbiSurfaceScaffold_.list8CContextTreeHead = nullptr;
    }
    if (attachedLauncherAbiSurfaceScaffold_.field90ContextCount) {
        *attachedLauncherAbiSurfaceScaffold_.field90ContextCount = 0u;
    }
    attachedLauncherAbiSurfaceScaffold_ = {};
}

void CLTThreadPerClientTCPEngine::RefreshOwnedLauncherMirrorStateScaffold() {
    ownedEndpointCount84Scaffold_ = static_cast<uint32_t>(monitoredPorts_.size());
    ownedContextCount90Scaffold_ = static_cast<uint32_t>(workerThreads_.size());
    SetEndpointTreeHead24OccupancyScaffold(
        ownedEndpointTreeHead80Scaffold_,
        ownedEndpointCount84Scaffold_ != 0u);
    SetContextTreeHead18OccupancyScaffold(
        ownedContextTreeHead8CScaffold_,
        ownedContextCount90Scaffold_ != 0u);
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTThreadPerClientTCPEngine::SyncAttachedLauncherObjectStateScaffold() {
    RefreshOwnedLauncherMirrorStateScaffold();
    if (attachedLauncherAbiSurfaceScaffold_.field04CtorFlags) {
        *attachedLauncherAbiSurfaceScaffold_.field04CtorFlags = ctorFlagsField04Scaffold_;
    }
    if (attachedLauncherAbiSurfaceScaffold_.field08QueueThreadArray) {
        *attachedLauncherAbiSurfaceScaffold_.field08QueueThreadArray = queueThreadArrayField08Scaffold_;
    }
    if (attachedLauncherAbiSurfaceScaffold_.list80EndpointTreeHead) {
        *attachedLauncherAbiSurfaceScaffold_.list80EndpointTreeHead = ownedEndpointTreeHead80Scaffold_;
    }
    if (attachedLauncherAbiSurfaceScaffold_.field84EndpointCount) {
        *attachedLauncherAbiSurfaceScaffold_.field84EndpointCount = ownedEndpointCount84Scaffold_;
    }
    if (attachedLauncherAbiSurfaceScaffold_.list8CContextTreeHead) {
        *attachedLauncherAbiSurfaceScaffold_.list8CContextTreeHead = ownedContextTreeHead8CScaffold_;
    }
    if (attachedLauncherAbiSurfaceScaffold_.field90ContextCount) {
        *attachedLauncherAbiSurfaceScaffold_.field90ContextCount = ownedContextCount90Scaffold_;
    }
}

CLTThreadPerClientTCPEngine_Queue* CLTThreadPerClientTCPEngine::ActiveQueue0CScaffold() {
    return attachedLauncherAbiSurfaceScaffold_.queue0C
        ? attachedLauncherAbiSurfaceScaffold_.queue0C
        : &ownedQueue0CScaffold_;
}

CLTThreadPerClientTCPEngine_Queue* CLTThreadPerClientTCPEngine::ActiveQueue34Scaffold() {
    return attachedLauncherAbiSurfaceScaffold_.queue34
        ? attachedLauncherAbiSurfaceScaffold_.queue34
        : &ownedQueue34Scaffold_;
}

const CLTThreadPerClientTCPEngine_Queue* CLTThreadPerClientTCPEngine::ActiveQueue0CScaffold() const {
    return attachedLauncherAbiSurfaceScaffold_.queue0C
        ? attachedLauncherAbiSurfaceScaffold_.queue0C
        : &ownedQueue0CScaffold_;
}

const CLTThreadPerClientTCPEngine_Queue* CLTThreadPerClientTCPEngine::ActiveQueue34Scaffold() const {
    return attachedLauncherAbiSurfaceScaffold_.queue34
        ? attachedLauncherAbiSurfaceScaffold_.queue34
        : &ownedQueue34Scaffold_;
}

void* CLTThreadPerClientTCPEngine::ActiveQueueLockScaffold() const {
    return attachedLauncherAbiSurfaceScaffold_.queueLock
        ? attachedLauncherAbiSurfaceScaffold_.queueLock
        : ownedQueueLockHelper60Scaffold_.criticalSectionStorage;
}

void* CLTThreadPerClientTCPEngine::ActiveQueueSignalEventScaffold() const {
    return attachedLauncherAbiSurfaceScaffold_.queueSignalEvent
        ? attachedLauncherAbiSurfaceScaffold_.queueSignalEvent
        : ownedQueueSignalEvent7CScaffold_;
}

void* CLTThreadPerClientTCPEngine::ActiveCleanupLockScaffold() const {
    return attachedLauncherAbiSurfaceScaffold_.cleanupLock
        ? attachedLauncherAbiSurfaceScaffold_.cleanupLock
        : ownedCleanupLockHelper98Scaffold_.criticalSectionStorage;
}

uint32_t CLTThreadPerClientTCPEngine::SignalQueueEventHelperScaffold() {
    HANDLE eventHandle = static_cast<HANDLE>(ActiveQueueSignalEventScaffold());
    return (eventHandle && SetEvent(eventHandle)) ? 0u : 1u;
}

uint32_t CLTThreadPerClientTCPEngine::WaitQueueEventHelperScaffold(int reasonMilliseconds) {
    (void)LeaveQueueLockHelperScaffold();
    HANDLE eventHandle = static_cast<HANDLE>(ActiveQueueSignalEventScaffold());
    const DWORD waitResult = eventHandle
        ? WaitForSingleObject(eventHandle, static_cast<DWORD>(reasonMilliseconds))
        : WAIT_FAILED;
    if (waitResult == WAIT_OBJECT_0) {
        (void)EnterQueueLockHelperScaffold(/*pumpLauncherBridge=*/false);
        return 0u;
    }
    if (waitResult == WAIT_TIMEOUT) {
        (void)EnterQueueLockHelperScaffold(/*pumpLauncherBridge=*/false);
        return 3u;
    }
    return 1u;
}

uint32_t CLTThreadPerClientTCPEngine::EnterQueueLockHelperScaffold(bool pumpLauncherBridge) {
    if (_RTL_CRITICAL_SECTION* crit =
            CriticalSectionFromOpaqueStorage(ActiveQueueLockScaffold())) {
        EnterCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(crit));
    }
    if (pumpLauncherBridge) {
        PumpLauncherConnectionBridgeFromArg5HelperScaffold();
    }
    return 0u;
}

uint32_t CLTThreadPerClientTCPEngine::LeaveQueueLockHelperScaffold() {
    if (_RTL_CRITICAL_SECTION* crit =
            CriticalSectionFromOpaqueStorage(ActiveQueueLockScaffold())) {
        LeaveCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(crit));
    }
    return 0u;
}

uint32_t CLTThreadPerClientTCPEngine::EnterCleanupLockHelperScaffold() {
    if (_RTL_CRITICAL_SECTION* crit =
            CriticalSectionFromOpaqueStorage(ActiveCleanupLockScaffold())) {
        EnterCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(crit));
    }
    return 0u;
}

uint32_t CLTThreadPerClientTCPEngine::LeaveCleanupLockHelperScaffold() {
    if (_RTL_CRITICAL_SECTION* crit =
            CriticalSectionFromOpaqueStorage(ActiveCleanupLockScaffold())) {
        LeaveCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(crit));
    }
    return 0u;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* CLTThreadPerClientTCPEngine::EnsureLauncherConnectionContextScaffold(
    mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold** slot,
    mxo::ltlogin::CLTLoginMediator* mediator,
    const char* label,
    bool isMarginConnection) {
    if (!slot) {
        return nullptr;
    }

    EnsureLauncherConnectionBridgeContextVtableInitialized();
    if (!*slot) {
        *slot = static_cast<mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold*>(
            std::calloc(1, sizeof(mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold)));
        if (!*slot) {
            spdlog::info(
                "CLTThreadPerClientTCPEngine::EnsureLauncherConnectionContextScaffold failed label='{}'",
                label ? label : "<null>");
            return nullptr;
        }
        (*slot)->vtable = g_LauncherConnectionBridgeContextVtable;
        (*slot)->autoReleaseFlag = 0;
    }

    (*slot)->debugLabel = label;
    (*slot)->mediator = mediator;
    (*slot)->isMarginConnection = isMarginConnection;
    return *slot;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTThreadPerClientTCPEngine::AttachLauncherConnectionBridgeContextsScaffold(
    mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* authContext,
    mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* marginContext) {
    authBridgeContextScaffold_ = authContext;
    marginBridgeContextScaffold_ = marginContext;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
bool CLTThreadPerClientTCPEngine::EnqueueCompletedOperationScaffold(
    void* workItem,
    void* context,
    bool useQueue34,
    const char* label,
    bool queueLockAlreadyHeld) {
    // Current best read of original `0x436820` / `0x436670`:
    // - `0x436820` itself returns `void`
    // - lock/order is:
    //   enter helper `+0x60` -> snapshot combined queue-pair emptiness -> push pair -> leave lock
    //   -> signal helper `+0x5c` only on pre-push empty -> non-empty transition
    // - no push-success result is surfaced back to callers; caller-side ownership/lifetime does not
    //   branch on queue-growth success/failure once this path is entered
    CLTThreadPerClientTCPEngine_Queue* targetQueue = useQueue34 ? ActiveQueue34Scaffold() : ActiveQueue0CScaffold();
    if (!targetQueue) {
        return false;
    }

    _RTL_CRITICAL_SECTION* queueLock =
        CriticalSectionFromOpaqueStorage(ActiveQueueLockScaffold());
    if (queueLock && !queueLockAlreadyHeld) {
        EnterCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(queueLock));
    }

    const bool queuePairWasEmpty =
        Queue_IsEmpty(ActiveQueue0CScaffold()) && Queue_IsEmpty(ActiveQueue34Scaffold());
    Queue_PushPair(
        targetQueue,
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(workItem)),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(context)));

    if (queueLock && !queueLockAlreadyHeld) {
        LeaveCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(queueLock));
    }

    if (queuePairWasEmpty) {
        (void)SignalQueueEventHelperScaffold();
    }

    LoggerForBridgeLabel(label)->info(
        "CLTThreadPerClientTCPEngine::EnqueueCompletedOperationScaffold label={} queue=[{}] workItem=0x{:08x} context={} pairWasEmpty={:08x} lockHeld={:08x}",
        label ? label : "<null>",
        useQueue34 ? "queue34" : "queue0C",
        static_cast<unsigned>(reinterpret_cast<uintptr_t>(workItem)),
        fmt::ptr(context),
        queuePairWasEmpty ? 1u : 0u,
        queueLockAlreadyHeld ? 1u : 0u);
    return true;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
bool CLTThreadPerClientTCPEngine::EnqueueLauncherConnectionStatusWorkItemInternalScaffold(
    mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* context,
    uint32_t workType,
    uint32_t workPayload,
    const char* label,
    bool queueLockAlreadyHeld) {
    if (!context) {
        return false;
    }

    EnsureLauncherConnectionBridgeWorkItemVtableInitialized();
    mxo::ltlogin::CLTLoginMediatorQueuedWorkItemScaffold* workItem =
        static_cast<mxo::ltlogin::CLTLoginMediatorQueuedWorkItemScaffold*>(
            std::calloc(1, sizeof(mxo::ltlogin::CLTLoginMediatorQueuedWorkItemScaffold)));
    if (!workItem) {
        LoggerForBridgeLabel(label)->info(
            "CLTThreadPerClientTCPEngine::EnqueueLauncherConnectionStatusWorkItemInternalScaffold failed label='{}'",
            label ? label : "<null>");
        return false;
    }

    workItem->header.vtable = g_LauncherConnectionBridgeWorkItemVtable;
    workItem->header.workType = workType;
    workItem->workPayload = workPayload;
    workItem->debugLabel = label;

    void* queuedContext = context;
    if ((IsSyntheticReceiveDrainWorkType(workType) ||
         workType == kWorkTypeConnectionStatus) &&
        context->sidecarConnection) {
        // Current bounded fidelity step:
        // - original queue consumer dispatches type-2/type-3-adjacent work into the
        //   connection-family callback path with `context == connection`
        // - the source-owned receive-drain proxy is still synthetic, but it now reaches that
        //   nearer connection callback surface first instead of jumping straight to the
        //   mediator-owned bridge context callback
        queuedContext = context->sidecarConnection->QueueContextScaffold();
    }

    const bool queued = EnqueueCompletedOperationScaffold(
        workItem,
        queuedContext,
        /*useQueue34=*/false,
        label,
        queueLockAlreadyHeld);
    if (!queued) {
        std::free(workItem);
        return false;
    }

    LoggerForBridgeLabel(label)->info(
        "CLTThreadPerClientTCPEngine launcher bridge queued work label='{}' workItem={} context={} type=0x{:08x} ({}) payload=0x{:08x}",
        label ? label : "<null>",
        fmt::ptr(workItem),
        fmt::ptr(queuedContext),
        workType,
        LauncherBridgeWorkTypeName(workType),
        workPayload);
    return true;
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
bool CLTThreadPerClientTCPEngine::EnqueueLauncherConnectionStatusWorkItemScaffold(
    mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* context,
    uint32_t workType,
    uint32_t workPayload,
    const char* label) {
    return EnqueueLauncherConnectionStatusWorkItemInternalScaffold(
        context,
        workType,
        workPayload,
        label,
        /*queueLockAlreadyHeld=*/false);
}

// UNANCHORED: connection-owned bridge for the recovered `0x449d8a -> 0x436820` handoff.
void CLTThreadPerClientTCPEngine::EnqueueCompletedOperationFromConnectionScaffold(
    CLTTCPConnection_ParsedPacketWorkItemScaffold* workItem,
    CLTTCPConnection* connection,
    const char* label) {
    // Current best original read for this receive path:
    // - `CLTTCPConnection::OnReceive` always calls `0x436820(engine+0x10, workItem, self, false)`
    // - queue selection is therefore fixed to queue0C here
    // - current parser read does not support an intentional `Parse(...) == 0` / `workItem == NULL`
    //   emit on this path; null work items belong to later lifecycle/shutdown producers instead
    // - original caller does not test a success result or reclaim `workItem`; ownership is already
    //   transferred to the queue/consumer boundary when this helper is entered
    (void)EnqueueCompletedOperationScaffold(
        workItem,
        connection ? connection->QueueContextScaffold() : nullptr,
        /*useQueue34=*/false,
        label,
        /*queueLockAlreadyHeld=*/false);
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTThreadPerClientTCPEngine::PumpLauncherConnectionContextScaffold(
    mxo::ltlogin::CLTLoginMediatorConnectionContextScaffold* context,
    const char* receiveLabel) {
    if (!context || !context->sidecarConnection) {
        return;
    }

    // Keep the connection seam itself on the faithful one-fragment
    // `0x42fe50 -> 0x449d40 -> 0x469bf0` receive handoff, but let the launcher bridge re-enter
    // that helper repeatedly within one arg5 helper poll.
    //
    // Why this is currently bounded here instead of inside `CLTTCPConnection`:
    // - a previous full same-poll recv-drain restoration on the bridge path regressed live runs
    //   into a later "Loading Character" stall
    // - original queue type `3` already belongs to the parsed-packet work items that
    //   `CLTTCPConnection::OnReceive` enqueues before control returns here
    // - the extra `AuthReceivePacket` / `MarginReceivePacket` submission below is therefore only a
    //   source-owned receive-drain proxy for the later original `0x4490c0` dispatch tail that
    //   source still does not execute on that same callback
    // - newer bounded leaf-side corrections now source-own two later destinations from that tail:
    //   - handled auth copied packets can re-enter
    //     `0x449a30 -> owner+0x180 / 0x41f250`
    //   - handled margin copied packets can re-enter
    //     `0x44af20 -> 0x442d00 -> owner+0x184 / 0x41f260`
    // - so the later synthetic receive-drain item is increasingly a fallback/no-op path rather
    //   than the primary live consumer on those handled branches
    // - current source queue order on one helper poll is therefore:
    //   - `OnReceive` first queues all parsed-packet work items emitted from the current fragment
    //   - then this helper queues one synthetic receive-drain proxy for that same fragment
    //   - if a later recv in the same poll returns peer-close/error, the type-1 close item queues
    //     after those successful-fragment submissions
    // - that proxy lines up better with the original worker-thread cadence when it is emitted once
    //   per successful recv fragment / `OnReceive` iteration rather than once per whole helper poll
    // - later peer-close notification still queues after any successful fragment notifications from
    //   the same helper poll, matching the original `0x42fe50` ordering more closely than the old
    //   once-per-pump synthetic receive path
    while (true) {
        const int received =
            context->sidecarConnection->PollReceiveAndDeliverReadOperationFragmentsScaffold();
        if (received > 0) {
            (void)EnqueueLauncherConnectionStatusWorkItemInternalScaffold(
                context,
                /*workType=*/kWorkTypeSyntheticReceiveDrain,
                /*workPayload=*/static_cast<uint32_t>(received),
                receiveLabel,
                /*queueLockAlreadyHeld=*/true);
            continue;
        }

        if (received < 0 && !context->peerCloseQueued) {
            context->peerCloseQueued = true;
            spdlog::info(
                "CLTThreadPerClientTCPEngine::PumpLauncherConnectionContextScaffold queued peer-close label='{}' context={} connection={}",
                (context->debugLabel && context->debugLabel[0]) ? context->debugLabel : "<null>",
                fmt::ptr(context),
                fmt::ptr(context->sidecarConnection));
            (void)EnqueueLauncherConnectionStatusWorkItemInternalScaffold(
                context,
                /*workType=*/kWorkTypeClose,
                /*workPayload=*/0u,
                context->isMarginConnection ? "MarginPeerClosed" : "AuthPeerClosed",
                /*queueLockAlreadyHeld=*/true);
        }
        return;
    }
}

// UNANCHORED: no original launcher.exe anchor assigned yet.
void CLTThreadPerClientTCPEngine::PumpLauncherConnectionBridgeFromArg5HelperScaffold() {
    PumpLauncherConnectionContextScaffold(authBridgeContextScaffold_, "AuthReceivePacket");
    PumpLauncherConnectionContextScaffold(marginBridgeContextScaffold_, "MarginReceivePacket");
}

// anchor: launcher.exe:0x436b10
void CLTThreadPerClientTCPEngine::RunCompletedOperationQueue(bool nonBlocking) {
    // Current bounded mirror of the shared launcher/client consumer family:
    // - prefer queue34, else queue0C
    // - nonBlocking=true matches the client poll form; false waits on the attached signal event
    // - null work item is the shutdown sentinel and cascades via the normal enqueue helper
    // - type-1 work runs slot-12-style cleanup before the later context callback
    // - callback runs before work-item release; conditional type-1 context auto-release stays last
    // - queue selection/pop happens under the attached arg5 lock
    _RTL_CRITICAL_SECTION* queueLock =
        CriticalSectionFromOpaqueStorage(ActiveQueueLockScaffold());
    while (true) {
        if (queueLock) {
            EnterCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(queueLock));
        }

        CLTThreadPerClientTCPEngine_Queue* selectedQueue = nullptr;
        if (!Queue_IsEmpty(ActiveQueue34Scaffold())) {
            selectedQueue = ActiveQueue34Scaffold();
        } else if (!Queue_IsEmpty(ActiveQueue0CScaffold())) {
            selectedQueue = ActiveQueue0CScaffold();
        }

        if (!selectedQueue) {
            if (queueLock) {
                LeaveCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(queueLock));
            }
            if (nonBlocking) {
                return;
            }

            // Bounded fidelity step from the shared `0x436b10` / `0x62531c10` queue-consumer family:
            // when both queues are empty, original code routes through arg5 helper `+0x5c` for the
            // wait path instead of immediately returning. With the current ownership move, that
            // helper body now lives on the engine side even though the launcher ABI shell still
            // supplies the raw embedded helper address when original code calls it directly.
            if (!ActiveQueueSignalEventScaffold()) {
                return;
            }

            const uint32_t waitResult = WaitQueueEventHelperScaffold(INFINITE);
            if (waitResult == 0u || waitResult == 3u) {
                continue;
            }
            return;
        }

        CLTThreadPerClientTCPEngine_QueuedPair pair = {};
        const bool popped = Queue_TryPopPair(selectedQueue, &pair);
        if (queueLock) {
            LeaveCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(queueLock));
        }
        if (!popped) {
            return;
        }

        void* workItem = reinterpret_cast<void*>(static_cast<uintptr_t>(pair.value0));
        void* context = reinterpret_cast<void*>(static_cast<uintptr_t>(pair.value1));
        if (!workItem) {
            // Current best read: a null work item is the shutdown-style queue sentinel.
            // Original `0x436f56..0x436fa8` cascades shutdown via the normal enqueue helper path,
            // not by open-coding another raw queue write.
            (void)EnqueueCompletedOperationScaffold(
                nullptr,
                nullptr,
                /*useQueue34=*/false,
                "RunCompletedOperationQueueShutdownCascade",
                /*queueLockAlreadyHeld=*/false);
            return;
        }

        const uint32_t workType = QueueWorkItem_GetType(workItem);
        const bool isType1 = (workType == kWorkTypeClose);
        const bool shouldAutoReleaseContext =
            context && isType1 && QueueContext_ShouldAutoReleaseAfterType1(context);

        spdlog::debug(
            "CLTThreadPerClientTCPEngine::RunCompletedOperationQueue consume queue=[{}] workItem={} workType=0x{:08x} context={} autoReleaseType1Context={}",
            (selectedQueue == ActiveQueue34Scaffold()) ? "queue34" : "queue0C",
            fmt::ptr(workItem),
            workType,
            fmt::ptr(context),
            shouldAutoReleaseContext ? 1u : 0u);

        if (context && isType1) {
            CleanupConnection(context);
        }

        if (context) {
            QueueContext_OnOperationCompleted(context, workItem);
        }

        QueueWorkItem_Release(workItem);
        if (shouldAutoReleaseContext) {
            QueueContext_Release(context);
        }
    }
}

// UNANCHORED scaffold helper used to mirror the recovered 0x4366f0 child-allocation shape in source.
void CLTThreadPerClientTCPEngine::RebuildQueueThreadsForCtorCount(uint32_t queueThreadCount) {
    queueThreads_.clear();
    queueThreads_.reserve(queueThreadCount);
    for (uint32_t i = 0; i < queueThreadCount; ++i) {
        queueThreads_.push_back(std::make_unique<CLTThreadPerClientTCPEngine_QueueThread>(this));
    }
}

// UNANCHORED scaffold accessor for source-side queue-thread child tracking.
size_t CLTThreadPerClientTCPEngine::QueueThreadCount() const {
    return queueThreads_.size();
}

// UNANCHORED starter accessor.
// Exposes scaffold state; no direct launcher.exe function anchor is assigned yet.
const std::vector<CLTThreadPerClientTCPEngine::AcceptThreadRecord>& CLTThreadPerClientTCPEngine::MonitoredPorts() const {
    return monitoredPorts_;
}

// UNANCHORED starter accessor.
// Exposes scaffold state; no direct launcher.exe function anchor is assigned yet.
const std::vector<CLTThreadPerClientTCPEngine::WorkerThreadRecord>& CLTThreadPerClientTCPEngine::WorkerThreads() const {
    return workerThreads_;
}

// UNANCHORED starter helper.
// Keeps recovered connection-object-oriented queue/context handling out of diagnostics.cpp.
CMessageConnection* CLTThreadPerClientTCPEngine::FindMessageConnection(void* contextKey) {
    CBaseConnection* queueContextOwner = ResolveEngineQueueContextOwnerScaffold(contextKey);
    void* resolvedContextKey = ResolveEngineContextKeyScaffold(contextKey);
    auto matchesConnectionKey =
        [contextKey, resolvedContextKey, queueContextOwner](CMessageConnection* connection) -> bool {
        if (!connection) {
            return false;
        }
        return connection == contextKey ||
            connection == resolvedContextKey ||
            connection == queueContextOwner ||
            connection->OwnerContext() == contextKey ||
            connection->OwnerContext() == resolvedContextKey;
    };

    // Prefer the live auth/margin bridge-tracked sidecar connections before falling back to
    // engine-owned generic entries. That keeps slot 6/7/8 lookup closer to the real
    // connection-family objects already driving the active path.
    if (authBridgeContextScaffold_ && matchesConnectionKey(authBridgeContextScaffold_->sidecarConnection)) {
        return authBridgeContextScaffold_->sidecarConnection;
    }
    if (marginBridgeContextScaffold_ && matchesConnectionKey(marginBridgeContextScaffold_->sidecarConnection)) {
        return marginBridgeContextScaffold_->sidecarConnection;
    }

    for (CMessageConnection* connection : messageConnections_) {
        if (matchesConnectionKey(connection)) {
            return connection;
        }
    }
    return nullptr;
}

// UNANCHORED starter helper.
// Keeps recovered connection-object-oriented queue/context handling out of diagnostics.cpp.
CMessageConnection* CLTThreadPerClientTCPEngine::GetOrCreateMessageConnection(void* contextKey) {
    if (!contextKey) {
        return nullptr;
    }

    if (CMessageConnection* existing = FindMessageConnection(contextKey)) {
        return existing;
    }

    void* resolvedContextKey = ResolveEngineContextKeyScaffold(contextKey);
    if (resolvedContextKey != contextKey) {
        if (CMessageConnection* existing = FindMessageConnection(resolvedContextKey)) {
            return existing;
        }
    }

    CMessageConnection* connection = new CMessageConnection(this);
    if (!connection) {
        return nullptr;
    }

    connection->SetOwnerContext(resolvedContextKey);
    messageConnections_.push_back(connection);
    return connection;
}

// UNANCHORED starter helper.
// Keeps recovered connection-object-oriented queue/context handling out of diagnostics.cpp.
bool CLTThreadPerClientTCPEngine::DropMessageConnection(void* contextKey) {
    CBaseConnection* queueContextOwner = ResolveEngineQueueContextOwnerScaffold(contextKey);
    void* resolvedContextKey = ResolveEngineContextKeyScaffold(contextKey);
    for (auto it = messageConnections_.begin(); it != messageConnections_.end(); ++it) {
        CMessageConnection* connection = *it;
        if (connection &&
            (connection == contextKey ||
             connection == resolvedContextKey ||
             connection == queueContextOwner ||
             connection->OwnerContext() == contextKey ||
             connection->OwnerContext() == resolvedContextKey)) {
            delete connection;
            messageConnections_.erase(it);
            return true;
        }
    }
    return false;
}

// UNANCHORED starter helper.
// No direct launcher.exe helper body is assigned yet; this just mirrors the recovered key shape.
LTTCPEndpointKey CLTThreadPerClientTCPEngine::MakeEndpointKey(uint16_t portHostOrder, uint32_t ipv4NetworkOrder) {
    LTTCPEndpointKey key = {};
    key.family = 2;
    key.portNetworkOrder = static_cast<uint16_t>((portHostOrder << 8) | (portHostOrder >> 8));
    key.ipv4NetworkOrder = ipv4NetworkOrder;
    return key;
}

// UNANCHORED starter helper.
// No direct launcher.exe helper body is assigned yet.
CLTThreadPerClientTCPEngine::AcceptThreadRecord* CLTThreadPerClientTCPEngine::FindMonitoredPort(const LTTCPEndpointKey& key) {
    for (auto& record : monitoredPorts_) {
        if (record.endpoint.portNetworkOrder == key.portNetworkOrder &&
            record.endpoint.ipv4NetworkOrder == key.ipv4NetworkOrder) {
            return &record;
        }
    }
    return nullptr;
}

// UNANCHORED starter helper.
// No direct launcher.exe helper body is assigned yet.
CLTThreadPerClientTCPEngine::WorkerThreadRecord* CLTThreadPerClientTCPEngine::FindWorker(void* contextKey) {
    CBaseConnection* queueContextOwner = ResolveEngineQueueContextOwnerScaffold(contextKey);
    void* resolvedContextKey = ResolveEngineContextKeyScaffold(contextKey);
    for (auto& record : workerThreads_) {
        if (record.contextKey == contextKey ||
            record.contextKey == resolvedContextKey ||
            record.contextKey == queueContextOwner ||
            record.ownerContext == contextKey ||
            record.ownerContext == resolvedContextKey) {
            return &record;
        }
    }
    return nullptr;
}

CMessageConnection* CLTThreadPerClientTCPEngine::ResolveConnectionForEngineSlotScaffold(
    void* contextKey,
    bool allowCreateFallback) {
    if (!contextKey) {
        return nullptr;
    }

    void* normalizedContextKey = ResolveEngineContextKeyScaffold(contextKey);
    CMessageConnection* connection = FindMessageConnection(contextKey);
    if (!connection && normalizedContextKey != contextKey) {
        connection = FindMessageConnection(normalizedContextKey);
    }
    if (!connection && allowCreateFallback) {
        connection = GetOrCreateMessageConnection(normalizedContextKey);
    }
    if (!connection) {
        return nullptr;
    }

    connection->SetEngine(this);
    if (connection->OwnerContext() == nullptr && normalizedContextKey != connection) {
        connection->SetOwnerContext(normalizedContextKey);
    }
    return connection;
}

void CLTThreadPerClientTCPEngine::SyncConnectionFromWorkerRecordScaffold(
    const WorkerThreadRecord* record) {
    if (!record) {
        return;
    }

    CMessageConnection* connection = ResolveConnectionForEngineSlotScaffold(
        record->contextKey ? record->contextKey : record->ownerContext,
        /*allowCreateFallback=*/false);
    if (!connection) {
        return;
    }

    connection->SetEngine(this);
    if (connection->OwnerContext() == nullptr && record->ownerContext) {
        connection->SetOwnerContext(record->ownerContext);
    }
    connection->SetSocketHandle(record->socketHandle);
    connection->SetState(record->state);
}

// UNANCHORED source-owned helper shaped after launcher.exe:0x431ff0 worker creation/insertion.
CLTThreadPerClientTCPEngine::WorkerThreadRecord* CLTThreadPerClientTCPEngine::CreateOrReplaceWorkerThreadScaffold(
    void* contextKey,
    void* ownerContext,
    uint32_t socketHandle,
    LTTCPEngineConnectionState state,
    bool datagramMode) {
    void* normalizedContextKey = ResolveEngineContextKeyScaffold(contextKey);
    void* normalizedOwnerContext = ResolveEngineContextKeyScaffold(ownerContext ? ownerContext : contextKey);
    if (!normalizedContextKey) {
        normalizedContextKey = contextKey;
    }
    if (!normalizedOwnerContext) {
        normalizedOwnerContext = normalizedContextKey;
    }

    WorkerThreadRecord worker = {};
    worker.contextKey = normalizedContextKey;
    worker.ownerContext = normalizedOwnerContext;
    worker.socketHandle = socketHandle;
    worker.state = state;
    worker.thread = std::make_unique<CLTThreadPerClientTCPEngine_WorkerThread>(normalizedContextKey, datagramMode);
    if (!worker.thread) {
        return nullptr;
    }

    WorkerThreadRecord* inserted = nullptr;
    if (WorkerThreadRecord* existing = FindWorker(normalizedContextKey)) {
        StopWorkerThreadScaffold(existing);
        *existing = std::move(worker);
        inserted = existing;
    } else {
        workerThreads_.push_back(std::move(worker));
        inserted = workerThreads_.empty() ? nullptr : &workerThreads_.back();
    }

    if (inserted) {
        SyncConnectionFromWorkerRecordScaffold(inserted);
    }

    if (inserted && inserted->thread) {
        (void)inserted->thread->Start(/*startPriority=*/2);
    }
    return inserted;
}

// UNANCHORED: source-owned teardown helper for recovered AcceptThread-style payloads.
void CLTThreadPerClientTCPEngine::StopAcceptThreadScaffold(AcceptThreadRecord* record) {
    if (!record) {
        return;
    }

    if (record->thread) {
        record->thread->SignalWakeup();
        (void)record->thread->Stop(/*waitAfterTerminate=*/true);
        record->thread.reset();
    }

    CloseSocketHandle(&record->listenSocketHandle);
}

// UNANCHORED: source-owned teardown helper for recovered WorkerThread-style payloads.
void CLTThreadPerClientTCPEngine::StopWorkerThreadScaffold(WorkerThreadRecord* record) {
    if (!record) {
        return;
    }

    // Bounded fidelity step from slot-12 / worker-teardown RE:
    // - original cleanup first marks the worker payload as exit-requested before the later wakeup /
    //   stop / removal steps
    // - keep that intermediate transport state explicit in source too instead of jumping straight
    //   from active to closed only at the very end
    record->state = LTTCPEngineConnectionState::kClosing;
    SyncConnectionFromWorkerRecordScaffold(record);
    if (record->thread) {
        record->thread->RequestExit();
        record->thread->SignalWakeup();
        (void)record->thread->Stop(/*waitAfterTerminate=*/true);
        record->thread.reset();
    }

    CloseSocketHandle(&record->socketHandle);
    record->state = LTTCPEngineConnectionState::kClosed;
    SyncConnectionFromWorkerRecordScaffold(record);
}

// UNANCHORED starter binding helper.
// Keeps current owner->engine binding state on the liblttcp side rather than in diagnostics.cpp.
CLTThreadPerClientTCPEngineBinding::CLTThreadPerClientTCPEngineBinding()
    : owner_(nullptr),
      engine_() {}

// UNANCHORED starter binding helper.
CLTThreadPerClientTCPEngineBinding::~CLTThreadPerClientTCPEngineBinding() = default;

// UNANCHORED starter binding helper.
bool CLTThreadPerClientTCPEngineBinding::Bind(void* owner, mxo::ltlogin::CLTLoginMediator* mediator) {
    if (owner_ == owner && engine_) {
        return true;
    }

    if (engine_ && mediator) {
        mediator->ResetLauncherConnectionBridgeScaffold();
    }

    owner_ = owner;
    engine_ = std::make_unique<CLTThreadPerClientTCPEngine>();
    if (engine_ && mediator) {
        mediator->BindLauncherConnectionBridgeScaffold(engine_.get());
    }
    return static_cast<bool>(engine_);
}

// UNANCHORED starter binding helper.
void CLTThreadPerClientTCPEngineBinding::Reset(mxo::ltlogin::CLTLoginMediator* mediator) {
    if (engine_ && mediator) {
        mediator->ResetLauncherConnectionBridgeScaffold();
    }
    engine_.reset();
    owner_ = nullptr;
}

// UNANCHORED starter binding helper.
void* CLTThreadPerClientTCPEngineBinding::Owner() const {
    return owner_;
}

// UNANCHORED starter binding helper.
CLTThreadPerClientTCPEngine* CLTThreadPerClientTCPEngineBinding::Engine() const {
    return engine_.get();
}

// UNANCHORED starter binding helper.
bool CLTThreadPerClientTCPEngineBinding::HasEngine() const {
    return static_cast<bool>(engine_);
}

// UNANCHORED starter binding helper.
bool CLTThreadPerClientTCPEngineBinding::HasMonitoredPorts() const {
    return engine_ && !engine_->MonitoredPorts().empty();
}

// UNANCHORED starter binding helper.
bool CLTThreadPerClientTCPEngineBinding::HasWorkerThreads() const {
    return engine_ && !engine_->WorkerThreads().empty();
}

}  // namespace mxo::liblttcp
