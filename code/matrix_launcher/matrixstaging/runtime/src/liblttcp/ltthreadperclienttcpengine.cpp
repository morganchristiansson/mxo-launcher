#include "ltthreadperclienttcpengine.h"

#include "../libltmessaging/messageconnection.h"

#include <winsock2.h>
#include <ws2tcpip.h>

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

}  // namespace

// Keep the implementation intentionally conservative.
// These methods currently provide original-name structure and evidence-backed state
// shaping, but they are not yet the fully faithful runtime path used by arg5.

// anchor: launcher.exe:0x431c30
// vtable: launcher.exe:0x004b2768
CLTThreadPerClientTCPEngine::CLTThreadPerClientTCPEngine()
    : monitoredPorts_(),
      workerThreads_(),
      messageConnections_(),
      nextSyntheticSocketHandle_(0x100) {}

// anchor: launcher.exe:0x40b389..0x40b404 teardown releases arg5 through vtable slot 0
// vtable: launcher.exe:0x004b2768
// NOTE: starter C++ destructor only models local sidecar cleanup, not the full original dtor body.
CLTThreadPerClientTCPEngine::~CLTThreadPerClientTCPEngine() {
    for (CMessageConnection* connection : messageConnections_) {
        delete connection;
    }
    messageConnections_.clear();
}

// anchor: launcher.exe:0x431ce0
// vtable: launcher.exe:0x004b2768 slot +0x04
uint32_t CLTThreadPerClientTCPEngine::MonitorPort(uint16_t portHostOrder, void* ownerContext) {
    const LTTCPEndpointKey key = MakeEndpointKey(portHostOrder, 0);
    if (FindMonitoredPort(key)) {
        return kResultAlreadyMonitored;
    }

    AcceptThreadRecord record = {};
    record.endpoint = key;
    record.ownerContext = ownerContext;
    record.listenSocketHandle = nextSyntheticSocketHandle_++;
    record.shouldRun = true;
    monitoredPorts_.push_back(record);
    return kResultSuccess;
}

// anchor: launcher.exe:0x4325d0
// vtable: launcher.exe:0x004b2768 slot +0x08
uint32_t CLTThreadPerClientTCPEngine::UDPMonitorPort(uint16_t portHostOrder, void* contextKey, void* ownerContext) {
    (void)portHostOrder;

    WorkerThreadRecord worker = {};
    worker.contextKey = contextKey;
    worker.ownerContext = ownerContext;
    worker.socketHandle = nextSyntheticSocketHandle_++;
    worker.state = LTTCPEngineConnectionState::kUdpMonitorActive;

    if (WorkerThreadRecord* existing = FindWorker(contextKey)) {
        *existing = worker;
    } else {
        workerThreads_.push_back(worker);
    }
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
    }
    return result;
}

// UNANCHORED starter overload used by the scaffold.
// Current original anchor is the lower-level connect family at launcher.exe:0x4328a0.
uint32_t CLTThreadPerClientTCPEngine::Connect(uint16_t portHostOrder, uint32_t ipv4NetworkOrder, void* contextKey, void* ownerContext) {
    if (!contextKey || !EnsureWinsockReady()) {
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

    WorkerThreadRecord worker = {};
    worker.contextKey = contextKey;
    worker.ownerContext = ownerContext;
    worker.socketHandle = static_cast<uint32_t>(sock);
    worker.state = LTTCPEngineConnectionState::kConnectActive;

    if (WorkerThreadRecord* existing = FindWorker(contextKey)) {
        *existing = worker;
    } else {
        workerThreads_.push_back(worker);
    }
    return kResultSuccess;
}

// anchor: launcher.exe:0x4328a0
// vtable: launcher.exe:0x004b2768 slot +0x18
uint32_t CLTThreadPerClientTCPEngine::Connect(CLTTCPConnection* connection) {
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
        return 0;
    }

    void* contextKey = connection->OwnerContext() ? connection->OwnerContext() : static_cast<void*>(connection);
    const uint32_t result = Connect(
        portHostOrder,
        ipv4NetworkOrder,
        /*contextKey=*/contextKey,
        /*ownerContext=*/connection->OwnerContext());
    if (result == kResultSuccess) {
        connection->SetState(LTTCPEngineConnectionState::kConnectActive);
        if (WorkerThreadRecord* worker = FindWorker(contextKey)) {
            connection->SetSocketHandle(worker->socketHandle);
        }
    }
    return result;
}

// UNANCHORED starter helper.
// Collapses current arg5 context-oriented slot-6 bridge behavior into liblttcp.
uint32_t CLTThreadPerClientTCPEngine::ConnectContext(void* contextKey) {
    CMessageConnection* connection = GetOrCreateMessageConnection(contextKey);
    return connection ? connection->EnsureConnected() : 0u;
}

// anchor: launcher.exe:0x42f970
// vtable: launcher.exe:0x004b2768 slot +0x1c
uint32_t CLTThreadPerClientTCPEngine::Close(CLTTCPConnection* connection, bool graceful) {
    if (!connection) {
        return 0;
    }
    return connection->Close(graceful);
}

// UNANCHORED starter helper.
// Collapses current arg5 context-oriented slot-7 bridge behavior into liblttcp.
uint32_t CLTThreadPerClientTCPEngine::CloseContext(void* contextKey, bool graceful) {
    CMessageConnection* connection = GetOrCreateMessageConnection(contextKey);
    return connection ? connection->Close(graceful) : 0u;
}

// anchor: launcher.exe:0x42fbd0
// vtable: launcher.exe:0x004b2768 slot +0x20
uint32_t CLTThreadPerClientTCPEngine::SendBuffer(CLTTCPConnection* connection, const void* buffer, uint32_t byteCount, void* completionContext) {
    if (!connection) {
        return 0;
    }
    return connection->SendBuffer(buffer, byteCount, completionContext);
}

// UNANCHORED starter helper.
// Collapses current arg5 context-oriented slot-8 bridge behavior into liblttcp.
uint32_t CLTThreadPerClientTCPEngine::SendPacketContext(void* contextKey, const void* buffer, uint32_t byteCount, void* completionContext) {
    CMessageConnection* connection = GetOrCreateMessageConnection(contextKey);
    return connection ? connection->SendPacket(buffer, byteCount, completionContext) : 0u;
}

// anchor: launcher.exe:0x4316a0
// vtable: launcher.exe:0x004b2768 slot +0x30
uint32_t CLTThreadPerClientTCPEngine::CleanupConnection(void* contextKey) {
    if (CMessageConnection* connection = FindMessageConnection(contextKey)) {
        connection->SetState(LTTCPEngineConnectionState::kClosed);
        connection->SetSocketHandle(0xffffffffu);
    }

    for (auto it = workerThreads_.begin(); it != workerThreads_.end(); ++it) {
        if (it->contextKey == contextKey) {
            workerThreads_.erase(it);
            return kResultSuccess;
        }
    }
    return 0;
}

// anchor: launcher.exe:0x431840
// vtable: launcher.exe:0x004b2768 slot +0x14
uint32_t CLTThreadPerClientTCPEngine::UnmonitorPort(uint16_t portHostOrder, uint32_t ipv4NetworkOrder, uint32_t* outSocketHandle) {
    const LTTCPEndpointKey key = MakeEndpointKey(portHostOrder, ipv4NetworkOrder);
    for (auto it = monitoredPorts_.begin(); it != monitoredPorts_.end(); ++it) {
        if (it->endpoint.portNetworkOrder == key.portNetworkOrder &&
            it->endpoint.ipv4NetworkOrder == key.ipv4NetworkOrder) {
            if (outSocketHandle) {
                *outSocketHandle = it->listenSocketHandle;
            }
            monitoredPorts_.erase(it);
            return 0;
        }
    }

    if (outSocketHandle) {
        *outSocketHandle = 0;
    }
    return kResultEndpointNotFound;
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
    for (CMessageConnection* connection : messageConnections_) {
        if (connection && connection->OwnerContext() == contextKey) {
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

    CMessageConnection* connection = new CMessageConnection(this);
    if (!connection) {
        return nullptr;
    }

    connection->SetOwnerContext(contextKey);
    messageConnections_.push_back(connection);
    return connection;
}

// UNANCHORED starter helper.
// Keeps recovered connection-object-oriented queue/context handling out of diagnostics.cpp.
bool CLTThreadPerClientTCPEngine::DropMessageConnection(void* contextKey) {
    for (auto it = messageConnections_.begin(); it != messageConnections_.end(); ++it) {
        CMessageConnection* connection = *it;
        if (connection && connection->OwnerContext() == contextKey) {
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
    for (auto& record : workerThreads_) {
        if (record.contextKey == contextKey) {
            return &record;
        }
    }
    return nullptr;
}

// UNANCHORED starter binding helper.
// Keeps current owner->engine binding state on the liblttcp side rather than in diagnostics.cpp.
CLTThreadPerClientTCPEngineBinding::CLTThreadPerClientTCPEngineBinding()
    : owner_(nullptr),
      engine_() {}

// UNANCHORED starter binding helper.
CLTThreadPerClientTCPEngineBinding::~CLTThreadPerClientTCPEngineBinding() = default;

// UNANCHORED starter binding helper.
bool CLTThreadPerClientTCPEngineBinding::Bind(void* owner) {
    if (owner_ == owner && engine_) {
        return true;
    }

    owner_ = owner;
    engine_ = std::make_unique<CLTThreadPerClientTCPEngine>();
    return static_cast<bool>(engine_);
}

// UNANCHORED starter binding helper.
void CLTThreadPerClientTCPEngineBinding::Reset() {
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
