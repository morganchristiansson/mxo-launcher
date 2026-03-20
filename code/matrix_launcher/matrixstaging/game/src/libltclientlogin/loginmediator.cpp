/**
 * CLTLoginMediator - Launcher-owned authentication mediator.
 *
 * PURPOSE:
 * - Faithful reimplementation of the original launcher.exe authentication flow
 * - Coordinates auth/margin connections, packet handling, and state progression
 *
 * ADDRESS ANCHORS (from Ghidra analysis):
 * =============================================================================
 *
 * CONSTRUCTION / INITIALIZATION:
 * - launcher.exe:0x43b300 = CLTLoginMediator_InitializeHelperDispatchTable (helper array at 0x4f7868..0x4f78a0)
 * - launcher.exe:0x4f7868..0x4f78a0 = contiguous helper/state array (16 slots)
 * - launcher.exe:0x438d80 = current shared launcher-side event gate symbolized as
 *   `LaunchPadClient_ProcessEvent0x17`
 *   - launcher.exe:0x4816f0 = reused inline helper symbolized as
 *     `LaunchPadClient_GetVtableOffset` (anchor only; not a mediator-class identity claim)
 *   - launcher.exe:0x41cfb0 = CLTLoginMediator_PostEvent (event posting mechanism, logs "CLTLoginMediator::PostEvent(): Event# %d\n")
 *   - launcher.exe:0x41b450 = CLTLoginMediator_SwitchHelperState (switches helper dispatch table)
 *   - launcher.exe:0x41d090 = CLTLoginMediator_PostError (error reporting via fprintf, calls PostError, logs "CLTLoginMediator::PostError(): Error# %d\n")
 * - launcher.exe:0x4b51e0, 0x4b4fec, 0x4b5014, etc. = PTR_FUN data entries pointing to 0x438d80
 *
 * AUTH CONNECTION INITIALIZATION:
 * - launcher.exe:0x41d170 = CLTLoginMediator_BeginAuthConnection (strongest anchor)
 * - launcher.exe:0x439090 = CLTLoginMediator_Helper1_StartAuthConnection
 * - launcher.exe:0x41e500 = CLTLoginMediator_BeginMarginConnection
 * - owner +0x4c = auth DNS name storage
 * - owner +0x4c8 = auth port value
 * - owner +0x5c = endpoint block (sockaddr-like)
 * - connection->+0x1c = ensure-connected wrapper
 *
 * AUTH HANDSHAKE / BOOTSTRAP:
 * - launcher.exe:0x439210 = CLTLoginMediator_Helper2_BeginAuthBootstrap (strongest anchor)
 * - launcher.exe:0x448050 = AuthBootstrap680_PrepareAndDispatch (upstream dispatcher)
 * - launcher.exe:0x447eb0 = AuthBootstrap680_SendGetPublicKeyRequest (raw 0x06 send builder)
 * - launcher.exe:0x4474f0 = AuthBootstrap680_SendAuthRequest (raw 0x08 send builder)
 * - branch site selecting raw 0x06 vs raw 0x08 path: 0x448050
 *
 * AUTH CHALLENGE RESPONSE:
 * - launcher.exe:0x429b0 = later challenge/material continuation anchor
 * - exact original raw 0x0a builder/send VA: [not yet isolated]
 *
 * AUTH REPLY HANDLING:
 * - launcher.exe:0x4401a0 = HandleAuthReply (strongest anchor)
 * - launcher.exe:0x43a330 = auth-reply parser object helper
 * - launcher.exe:0x43b830 = later GetWorldList sender (upstream after success)
 * - owner-side writeback areas: +0x80, +0x684, +0x688, +0x818, +0xd84, +0xcc8
 *
 * MARGIN CONNECTION DISPATCH:
 * - launcher.exe:0x439300 = CLTLoginState_State4 slot 3 margin-dispatch body (strongest anchor)
 *   - source ownership for that case split now lives in loginstate.cpp
 *   - mediator keeps only the narrower owner-side route getters plus `0x41e500` transport/init
 * - launcher.exe:0x41e500 = CLTLoginMediator_BeginMarginConnection
 * - owner vtable +0xe0 = character/route resolution
 * - owner vtable +0xfc = owner field `+0x12c / +0x104` resolution through the `+0xd84` table
 * - owner vtable +0x10c = descriptor first-dword resolution
 * - owner +0x6c = margin endpoint block
 *
 * ARG6: ILTLoginMediator.Default at 0x4d2c58 - World List Provider
 * - launcher.exe:0x4d2c58 = ILTLoginMediator_Default (object pointer)
 * - launcher.exe:0x4d3584 = ILTLoginMediator_SiblingObject (vtable root at +0xc)
 * - launcher.exe:0x40d6f0 = ILTLoginMediator_ResolveSelectionFromListCtrl
 * - launcher.exe:0x40e480 = ILTLoginMediator_BuildWorldList
 * - launcher.exe:0x40cd10 = ILTLoginMediator_GetWorldNameByIndex (fallback)
 * - launcher.exe:0x40cd60 = ILTLoginMediator_GetWorldNameByIndex_Fallback
 * - launcher.exe:0x40e5b0 = ILTLoginMediator_GetWorldListCount
 * - launcher.exe:0x40e560 = ILTLoginMediator_GetWorldListCount_Active
 * - launcher.exe:0x40e670 = ILTLoginMediator_GetAvailableWorlds
 * - launcher.exe:0x40e6c0 = ILTLoginMediator_GetAvailableWorldName
 * - vtable +0xfc = GetWorldNameByIndex(index) -> char* (Arg6GetWorldNameByIndex): launcher.exe:0x40cd10
 * - vtable +0x100 = GetWorldVariantByIndex(index) -> uint (1,2,3,5) (Arg6GetWorldVariantByIndex): launcher.exe:0x4d3584+0x100
 * - vtable +0xe4 = ValidateWorldSelection(variant) -> 0 or 7 (Arg6ValidateWorldSelection): launcher.exe:0x4d3584+0xe4
 * - vtable +0xf8 = GetWorldListCount() -> uint (Arg6GetWorldListCount): launcher.exe:0x40e5b0
 * - vtable +0xd8 = GetActiveWorldCount() -> uint (Arg6GetActiveWorldListCount): launcher.exe:0x40e560
 * - vtable +0xe0 = GetAvailableWorlds(index) -> bool (Arg6GetAvailableWorlds): launcher.exe:0x40e670
 * - vtable +0xdc = GetAvailableWorldName(index) -> char* (Arg6GetAvailableWorldName): launcher.exe:0x40cd60
 *
 * ARG7 SELECTION RESOLUTION (through arg6 vtable at 0x4d3584):
 * - g_PackedArg7Selection = (high8bits << 24) | low24bits
 *   high8bits = variant state from vtable[+0x100]
 *   low24bits = world index from GetItemData low bits
 *
 * PROGRESS STATUS:
 * - [COMPLETE] Auth packet/protocol wire loop (0x06 -> 0x08 -> 0x0a -> 0x0b)
 * - [COMPLETE] Address anchors documented throughout source with Ghidra-synced names
 * - [COMPLETE] Helper dispatch table structure discovered from Ghidra analysis of 0x43b300:
 *   - [COMPLETE] CLTLoginMediator_InitializeHelperDispatchTable allocates 16 heap-allocated tables
 *   - [COMPLETE] `0x438d80` remains anchored under the current symbol
 *     `LaunchPadClient_ProcessEvent0x17`, but here it should be read as a shared launcher-side
 *     event gate rather than evidence that CLTLoginMediator is a LaunchPadClient instance
 *   - [COMPLETE] CLTLoginMediator_PostEvent (0x41cfb0), CLTLoginMediator_SwitchHelperState (0x41b450),
 *     CLTLoginMediator_PostError (0x41d090) are the helper functions
 *   - [COMPLETE] `0x4816f0` remains anchored as `LaunchPadClient_GetVtableOffset`, a reused inline
 *     helper name rather than a mediator-class identity claim
 * - [IN PROGRESS] Faithful arg5 (launcherNetworkObject at 0x4d6304)
 * - [COMPLETE] Faithful arg6 starter surface (ILTLoginMediator.Default at 0x4d2c58):
 *   - [COMPLETE] InitializeArg6DefaultObject method with address anchors
 *   - [COMPLETE] Vtable methods implemented (+0xfc, +0x100, +0xe4, +0xf8, +0xd8, +0xe0, +0xdc)
 *   - [COMPLETE] Arg6WorldListData struct with worldNames_, worldVariants_, worldValid_
 *   - [IN PROGRESS] Populate arg6WorldList_ with real launcher.exe-backed world data
 *   - [COMPLETE] Stop treating BuildWorldList as a required separate pre-InitClientDLL call on this scaffold path; current best read is inline mediator-owned world-list state
 * - [IN PROGRESS] Faithful arg7 selection resolution through 0x4d3584 vtable
 * - [IN PROGRESS] Endpoint builder implementations (+0x4c, +0x5c, +0x6c)
 * =============================================================================
 */

#include "loginmediator.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include "../../../../src/diagnostics.h"
#include "loginstate.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace mxo::ltlogin {

namespace {

static mxo::liblttcp::LTTCPEndpointKey BuildLoopbackEndpoint(uint16_t portHostOrder) {
    mxo::liblttcp::LTTCPEndpointKey key = {};
    key.family = 2;  // AF_INET
    key.portNetworkOrder = static_cast<uint16_t>((portHostOrder << 8) | (portHostOrder >> 8));
    key.ipv4NetworkOrder = 0;
    return key;
}

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

static bool ResolveAllIpv4Addresses(const char* hostName, std::vector<uint32_t>* outIpv4NetworkOrderList) {
    if (!hostName || !hostName[0] || !outIpv4NetworkOrderList || !EnsureWinsockReady()) {
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

    outIpv4NetworkOrderList->clear();
    for (addrinfo* it = results; it; it = it->ai_next) {
        if (it->ai_family != AF_INET || !it->ai_addr ||
            it->ai_addrlen < static_cast<int>(sizeof(sockaddr_in))) {
            continue;
        }

        const sockaddr_in* addr = reinterpret_cast<const sockaddr_in*>(it->ai_addr);
        const uint32_t ipv4NetworkOrder = addr->sin_addr.s_addr;
        if (std::find(outIpv4NetworkOrderList->begin(), outIpv4NetworkOrderList->end(), ipv4NetworkOrder) ==
            outIpv4NetworkOrderList->end()) {
            outIpv4NetworkOrderList->push_back(ipv4NetworkOrder);
        }
    }

    freeaddrinfo(results);
    return !outIpv4NetworkOrderList->empty();
}

static const char* MaskedAuthValue(const std::string& value) {
    return value.empty() ? "<empty>" : "<provided>";
}

static std::string LowercaseAsciiString(const std::string& value) {
    std::string out = value;
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return out;
}

static std::array<uint8_t, 16> CopyPrefix16(const std::vector<uint8_t>& bytes) {
    std::array<uint8_t, 16> out = {};
    const size_t count = std::min(out.size(), bytes.size());
    std::copy_n(bytes.begin(), count, out.begin());
    return out;
}

static std::string BuildHexPreview(const void* bytes, size_t byteCount, size_t maxPreviewBytes) {
    if (!bytes || byteCount == 0u || maxPreviewBytes == 0u) {
        return "<empty>";
    }

    const uint8_t* p = static_cast<const uint8_t*>(bytes);
    const size_t previewCount = std::min(byteCount, maxPreviewBytes);
    std::string out;
    out.reserve(previewCount * 3u);
    static const char kHexDigits[] = "0123456789abcdef";
    for (size_t i = 0; i < previewCount; ++i) {
        const uint8_t value = p[i];
        out.push_back(kHexDigits[(value >> 4) & 0x0fu]);
        out.push_back(kHexDigits[value & 0x0fu]);
        if (i + 1u != previewCount) {
            out.push_back(' ');
        }
    }
    return out;
}

static std::string BuildRecentEventHistoryPreview(const std::array<uint32_t, 8>& events, uint32_t count) {
    if (count == 0u) {
        return "[]";
    }

    const uint32_t boundedCount = std::min<uint32_t>(count, static_cast<uint32_t>(events.size()));
    std::string out = "[";
    char buffer[16] = {};
    for (uint32_t i = 0; i < boundedCount; ++i) {
        if (i != 0u) {
            out += ", ";
        }
        std::snprintf(buffer, sizeof(buffer), "0x%02x", static_cast<unsigned>(events[i] & 0xffu));
        out += buffer;
    }
    out += "]";
    return out;
}

static uint16_t ReadU16LE(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

static uint32_t ReadU32LE(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

static uint16_t ReadOpcodePrefixVariableWidth(const uint8_t* bytes, size_t byteCount) {
    if (!bytes || byteCount == 0u) {
        return 0u;
    }
    if ((bytes[0] & 0x80u) != 0u && byteCount >= 2u) {
        return static_cast<uint16_t>(((bytes[0] & 0x7fu) << 8) | bytes[1]);
    }
    return bytes[0];
}

static void CopyCStringIntoFixed(char* dest, size_t destSize, const uint8_t* src, size_t srcAvailable) {
    if (!dest || destSize == 0u) {
        return;
    }

    std::fill(dest, dest + destSize, '\0');
    if (!src || srcAvailable == 0u) {
        return;
    }

    size_t copyCount = 0u;
    while (copyCount + 1u < destSize && copyCount < srcAvailable && src[copyCount] != '\0') {
        dest[copyCount] = static_cast<char>(src[copyCount]);
        ++copyCount;
    }
    dest[copyCount] = '\0';
}

static void AppendOwnedSectionBytes(void*& buffer, uint16_t& length, const uint8_t* src, uint16_t appendLen) {
    if (!src || appendLen == 0u) {
        return;
    }

    const size_t oldLength = length;
    const size_t newLength = oldLength + appendLen;
    void* newBuffer = buffer ? std::realloc(buffer, newLength) : std::malloc(newLength);
    if (!newBuffer) {
        return;
    }

    std::memcpy(static_cast<uint8_t*>(newBuffer) + oldLength, src, appendLen);
    buffer = newBuffer;
    length = static_cast<uint16_t>(newLength & 0xffffu);
}

static void AssignOwnedSmallString(
    CLTLoginMediator::AuthBootstrapSelectedSource38Sketch& dest,
    const char* begin,
    const char* current) {
    dest.string60Owned.clear();
    dest.string60 = {};

    if (!begin || !current || current <= begin) {
        return;
    }

    dest.string60Owned.assign(begin, current);
    dest.string60.begin = dest.string60Owned.c_str();
    dest.string60.current = dest.string60Owned.c_str() + dest.string60Owned.size();
    dest.string60.capacity = dest.string60.current;
}

}  // namespace

CLTLoginMediator::CLTLoginMediator()
    : engine_(nullptr),
      currentState_(nullptr),
      scaffoldState3_(nullptr),
      scaffoldState4_(nullptr),
      scaffoldState6_(nullptr),
      scaffoldState8_(nullptr),
      scaffoldState9_(nullptr),
      scaffoldState10_(nullptr),
      scaffoldState11_(nullptr),
      scaffoldState12_(nullptr),
      scaffoldState13_(nullptr),
      authConnection_(nullptr),
      marginConnection_(nullptr),
      authConnectionOwnedByMediator_(false),
      marginConnectionOwnedByMediator_(false),
      authConnectionContextKey_(nullptr),
      marginConnectionContextKey_(nullptr),
      helpers_{},
      marginRouteState_{},
      marginAddressList3c_{},
      authBootstrapSource38_{},
      authBootstrap680_{},
      sessionCallbackHelper65c_(nullptr),
      state8SelectionContextSnapshotState_{},
      postAuthMarginLoadingState_{},
      authServerPortHostOrder_(11000),
      ignoreHostsFileForAuth_(false),
      marginServerPortHostOrder_(10000),
      ignoreHostsFileForMargin_(false),
      authEndpoint_(BuildLoopbackEndpoint(authServerPortHostOrder_)),
      marginEndpoint_(BuildLoopbackEndpoint(marginServerPortHostOrder_)),
      authUsername_(),
      authPassword_(),
      authLauncherVersion_(76005),
      authCurrentPublicKeyId_(0),
      authLoginType_(1),
      authKeyConfigMd5_(),
      authUiConfigMd5_(),
      authGetPublicKeyRequestSent_(false),
      authRequestSent_(false),
      authChallengeResponseSent_(false),
      lastAuthPublicKeyReply_(),
      lastAuthRequestBuildResult_(),
      lastAuthChallenge_(),
      lastAuthReply_(),
      lastAuthConnectStatus_(0),
      lastMarginConnectStatus_(0),
      authConnectStatusCount_(0),
      marginConnectStatusCount_(0),
      expectedAuthRequestName_(nullptr),
      expectedMarginRequestName_(nullptr),
      worldSlots_{},
      worldPayloadSlots_{} {
    SyncRecoveredAuthBootstrapFixedFieldsFromCurrentConfig();
    InitializeArg6DefaultObject();
}

CLTLoginMediator::~CLTLoginMediator() {
    if (authConnectionOwnedByMediator_) {
        delete authConnection_;
    }
    if (marginConnectionOwnedByMediator_) {
        delete marginConnection_;
    }
}

void CLTLoginMediator::SetNetworkEngine(mxo::liblttcp::CLTThreadPerClientTCPEngine* engine) {
    engine_ = engine;
    if (authConnection_) {
        authConnection_->SetEngine(engine_);
    }
    if (marginConnection_) {
        marginConnection_->SetEngine(engine_);
        if (auto* marginConnection = dynamic_cast<mxo::liblttcp::CMarginConnection*>(marginConnection_)) {
            marginConnection->SetMarginEngine(engine_);
        }
    }
}

mxo::liblttcp::CLTThreadPerClientTCPEngine* CLTLoginMediator::NetworkEngine() const {
    return engine_;
}

void CLTLoginMediator::SetCurrentState(CLTLoginState* state) {
    currentState_ = state;
}

CLTLoginState* CLTLoginMediator::CurrentState() const {
    return currentState_;
}

void CLTLoginMediator::SwitchHelperStateScaffold(uint32_t helperStateId, CLTLoginState* state) {
    // anchor: launcher.exe:0x41b450
    // Exact recovered shape from the current Ghidra pass:
    // - if an old helper exists, call its vtable `+0x0c` with the new helper object
    // - install the dispatch-table target into owner `+0x10`
    // - then call the new helper's vtable `+0x08` with the old helper object
    // Current source scaffold keeps that boundary explicit and records the target helper id, but
    // does not yet claim the exact old/new helper notification slot semantics beyond the proven
    // call shape.
    lastSwitchedHelperStateScaffold_ = helperStateId;
    CLTLoginState* oldState = currentState_;
    if (!state) {
        spdlog::info(
            "CLTLoginMediator::SwitchHelperStateScaffold helperState=0x{:02x} oldState={} newState=<null> (source scaffold leaves currentState unchanged)",
            static_cast<unsigned>(helperStateId),
            oldState ? oldState->DebugName() : "<null>");
        Log(
            "DIAGNOSTIC: CLTLoginMediator::SwitchHelperStateScaffold helperState=0x%02x oldState=%s newState=<null>",
            (unsigned)(helperStateId & 0xffu),
            oldState ? oldState->DebugName() : "<null>");
        return;
    }

    currentState_ = state;
    spdlog::info(
        "CLTLoginMediator::SwitchHelperStateScaffold helperState=0x{:02x} oldState={} newState={} (anchor: launcher.exe:0x41b450; original also performs old/new helper notification calls around the install)",
        static_cast<unsigned>(helperStateId),
        oldState ? oldState->DebugName() : "<null>",
        state->DebugName());
    Log(
        "DIAGNOSTIC: CLTLoginMediator::SwitchHelperStateScaffold helperState=0x%02x oldState=%s newState=%s",
        (unsigned)(helperStateId & 0xffu),
        oldState ? oldState->DebugName() : "<null>",
        state->DebugName());
}

void CLTLoginMediator::PostEventScaffold(uint32_t eventId) {
    // anchor: launcher.exe:0x41cfb0
    // Current Ghidra-first tightening matters for the post-state9 boundary:
    // - this is not a trivial logger
    // - original walks the owner `+0x674` listener tree and calls each observer callback
    // - newer live original proof now also closes the immediate post-state9 success event shape:
    //   - `0x43c180` success -> `0x41b450(0x0c)` -> `0x41cfb0(0x18)`
    //   - then later a follow-on `0x41cfb0(0x0f)` was observed before entering game
    //   - still with no natural hit on `0x004397e0` / `0x0041c5c0`
    // - practical scaffold consequence:
    //   keep explicit event-history logging here so replacement-launcher runs can be compared
    //   against the natural original event sequence without pretending the listener tree is already
    //   reconstructed.
    lastPostedEventScaffold_ = eventId;
    if (recentPostedEventCountScaffold_ < recentPostedEventsScaffold_.size()) {
        recentPostedEventsScaffold_[recentPostedEventCountScaffold_++] = eventId;
    } else {
        std::move(
            recentPostedEventsScaffold_.begin() + 1,
            recentPostedEventsScaffold_.end(),
            recentPostedEventsScaffold_.begin());
        recentPostedEventsScaffold_.back() = eventId;
    }
    const std::string recentEventsPreview =
        BuildRecentEventHistoryPreview(recentPostedEventsScaffold_, recentPostedEventCountScaffold_);
    spdlog::info(
        "{} Event# {} currentState={} lastSwitch=0x{:02x} recentEvents={} (listener tree at owner+0x674 not yet scaffolded)",
        kLogPrefixPostEvent,
        static_cast<unsigned>(eventId),
        currentState_ ? currentState_->DebugName() : "<null>",
        static_cast<unsigned>(lastSwitchedHelperStateScaffold_ & 0xffu),
        recentEventsPreview);
    Log(
        "DIAGNOSTIC: CLTLoginMediator::PostEvent() Event# %u currentState=%s lastSwitch=0x%02x recentEvents=%s",
        (unsigned)eventId,
        currentState_ ? currentState_->DebugName() : "<null>",
        (unsigned)(lastSwitchedHelperStateScaffold_ & 0xffu),
        recentEventsPreview.c_str());
}

void CLTLoginMediator::PostErrorScaffold(uint32_t errorId) {
    lastPostedErrorScaffold_ = errorId;
    spdlog::info("{} Error# {}", kLogPrefixPostError, static_cast<unsigned>(errorId));
}

void CLTLoginMediator::RegisterScaffoldState3(CLTLoginState* state) {
    scaffoldState3_ = state;
}

void CLTLoginMediator::RegisterScaffoldState4(CLTLoginState* state) {
    scaffoldState4_ = state;
}

void CLTLoginMediator::RegisterScaffoldState6(CLTLoginState* state) {
    scaffoldState6_ = state;
}

void CLTLoginMediator::RegisterScaffoldState8(CLTLoginState* state) {
    scaffoldState8_ = state;
}

void CLTLoginMediator::RegisterScaffoldState9(CLTLoginState* state) {
    scaffoldState9_ = state;
}

void CLTLoginMediator::RegisterScaffoldState10(CLTLoginState* state) {
    scaffoldState10_ = state;
}

void CLTLoginMediator::RegisterScaffoldState11(CLTLoginState* state) {
    scaffoldState11_ = state;
}

void CLTLoginMediator::RegisterScaffoldState12(CLTLoginState* state) {
    scaffoldState12_ = state;
}

void CLTLoginMediator::RegisterScaffoldState13(CLTLoginState* state) {
    scaffoldState13_ = state;
}

// anchor: launcher.exe:0x41f1d0
void CLTLoginMediator::SetState9CallbackObjectTriple84_88_8c(void* callback84, void* object88, void* object8c) {
    ownerCallback84_ = callback84;
    ownerObject88_ = object88;
    ownerObject8c_ = object8c;
}

CLTLoginState* CLTLoginMediator::ScaffoldState3() const {
    return scaffoldState3_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState4() const {
    return scaffoldState4_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState6() const {
    return scaffoldState6_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState8() const {
    return scaffoldState8_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState9() const {
    return scaffoldState9_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState10() const {
    return scaffoldState10_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState11() const {
    return scaffoldState11_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState12() const {
    return scaffoldState12_;
}

CLTLoginState* CLTLoginMediator::ScaffoldState13() const {
    return scaffoldState13_;
}

void CLTLoginMediator::SetAuthConnectionContextKey(void* contextKey) {
    authConnectionContextKey_ = contextKey;
}

void CLTLoginMediator::SetMarginConnectionContextKey(void* contextKey) {
    marginConnectionContextKey_ = contextKey;
}

void CLTLoginMediator::SetAuthCredentials(const char* username, const char* password) {
    authUsername_ = username ? username : "";
    authPassword_ = password ? password : "";
    authGetPublicKeyRequestSent_ = false;
    authRequestSent_ = false;
    authChallengeResponseSent_ = false;
    lastAuthPublicKeyReply_ = mxo::auth::GetPublicKeyReply();
    lastAuthRequestBuildResult_ = mxo::auth::AuthRequestBuildResult();
    lastAuthChallenge_ = mxo::auth::AuthChallenge();
    lastAuthReply_ = mxo::auth::AuthReply();

    Log(
        "DIAGNOSTIC: CLTLoginMediator auth credentials configured username='%s' password=%s",
        authUsername_.empty() ? "<empty>" : authUsername_.c_str(),
        MaskedAuthValue(authPassword_));
}

void CLTLoginMediator::SetAuthBootstrapConfig(
    uint32_t launcherVersion,
    uint32_t currentPublicKeyId,
    uint8_t loginType,
    const std::vector<uint8_t>& keyConfigMd5,
    const std::vector<uint8_t>& uiConfigMd5) {
    authLauncherVersion_ = launcherVersion;
    authCurrentPublicKeyId_ = currentPublicKeyId;
    authLoginType_ = loginType;
    authKeyConfigMd5_ = keyConfigMd5;
    authUiConfigMd5_ = uiConfigMd5;
    SyncRecoveredAuthBootstrapFixedFieldsFromCurrentConfig();

    Log(
        "DIAGNOSTIC: CLTLoginMediator auth bootstrap configured launcherVersion=%u currentPublicKeyId=%u loginType=%u keyConfigMd5Len=%u uiConfigMd5Len=%u",
        (unsigned)authLauncherVersion_,
        (unsigned)authCurrentPublicKeyId_,
        (unsigned)authLoginType_,
        (unsigned)authKeyConfigMd5_.size(),
        (unsigned)authUiConfigMd5_.size());
}

void CLTLoginMediator::SetAuthServerConfig(const char* dnsName, uint16_t portHostOrder, bool ignoreHostsFile) {
    authServerDnsName_ = dnsName ? dnsName : "";
    authServerPortHostOrder_ = portHostOrder;
    ignoreHostsFileForAuth_ = ignoreHostsFile;
    BuildAuthEndpoint();
}

void CLTLoginMediator::SetMarginServerConfig(const char* dnsSuffix, uint16_t portHostOrder, bool ignoreHostsFile) {
    marginServerDnsSuffix_ = dnsSuffix ? dnsSuffix : "";
    marginServerPortHostOrder_ = portHostOrder;
    ignoreHostsFileForMargin_ = ignoreHostsFile;
    BuildMarginEndpoint();
}

const std::string& CLTLoginMediator::AuthServerDnsName() const {
    return authServerDnsName_;
}

uint16_t CLTLoginMediator::AuthServerPortHostOrder() const {
    return authServerPortHostOrder_;
}

bool CLTLoginMediator::IgnoreHostsFileForAuth() const {
    return ignoreHostsFileForAuth_;
}

const std::string& CLTLoginMediator::MarginServerDnsSuffix() const {
    return marginServerDnsSuffix_;
}

uint16_t CLTLoginMediator::MarginServerPortHostOrder() const {
    return marginServerPortHostOrder_;
}

bool CLTLoginMediator::IgnoreHostsFileForMargin() const {
    return ignoreHostsFileForMargin_;
}

std::string CLTLoginMediator::ResolvedMarginHostName() const {
    if (!marginRouteState_.exactMarginHostName.empty()) {
        return marginRouteState_.exactMarginHostName;
    }
    if (!marginRouteState_.routeHostPrefix.empty() && !marginServerDnsSuffix_.empty()) {
        return marginRouteState_.routeHostPrefix + marginServerDnsSuffix_;
    }
    return std::string();
}

const mxo::liblttcp::LTTCPEndpointKey& CLTLoginMediator::AuthEndpoint() const {
    return authEndpoint_;
}

const mxo::liblttcp::LTTCPEndpointKey& CLTLoginMediator::MarginEndpoint() const {
    return marginEndpoint_;
}

mxo::liblttcp::CMessageConnection* CLTLoginMediator::AuthConnection() const {
    return authConnection_;
}

mxo::liblttcp::CMessageConnection* CLTLoginMediator::MarginConnection() const {
    return marginConnection_;
}

void CLTLoginMediator::SetMarginRouteState(uint8_t currentCharacterOrRouteIndex, uint32_t pendingWorldId, int32_t currentWorldId) {
    marginRouteState_.currentCharacterOrRouteIndex = currentCharacterOrRouteIndex;
    marginRouteState_.pendingWorldId = pendingWorldId;
    marginRouteState_.currentWorldId = currentWorldId;
}

void CLTLoginMediator::SetMarginRouteHostPrefix(const char* routeHostPrefix) {
    marginRouteState_.routeHostPrefix = routeHostPrefix ? routeHostPrefix : "";
}

void CLTLoginMediator::SetExactMarginHostName(const char* exactMarginHostName) {
    marginRouteState_.exactMarginHostName = exactMarginHostName ? exactMarginHostName : "";
}

const CLTLoginMediator::MarginRouteState& CLTLoginMediator::CurrentMarginRouteState() const {
    return marginRouteState_;
}

const CLTLoginMediator::ConnectionHelperFamily& CLTLoginMediator::Helpers() const {
    return helpers_;
}

const CLTLoginMediator::AuthBootstrapState680Sketch& CLTLoginMediator::AuthBootstrap680() const {
    return authBootstrap680_;
}

void CLTLoginMediator::InitializeConnectionHelpers() {
    // Address anchors (from Ghidra decompilation of 0x43b300):
    // - launcher.exe:0x43b300 = CLTLoginMediator_InitializeHelperDispatchTable
    //   This function allocates the helper/state objects installed into 0x4f7868..0x4f78b4.
    // - launcher.exe:0x4b51e0, 0x4b4fc4, 0x4b4fec, 0x4b5014, etc. = `CLTLoginState_*` vtables
    //   whose slot 1 commonly points at 0x438d80.
    // - launcher.exe:0x420640 = InitializeHelperDispatchSlot15 (initializes slot at 0x4f78a4)
    // - launcher.exe:0x4206e0 = InitializeHelperDispatchSlot16 (initializes slot at 0x4f78a8)
    // - launcher.exe:0x420850 = InitializeHelperDispatchSlot17 (initializes slot at 0x4f78ac)
    // - launcher.exe:0x420920 = InitializeHelperDispatchSlot18 (initializes slot at 0x4f78b0)
    // - launcher.exe:0x4209a0 = InitializeHelperDispatchSlot19 (initializes slot at 0x4f78b4)
    // - launcher.exe:0x438d80 = current shared launcher-side event gate symbolized as
    //   `LaunchPadClient_ProcessEvent0x17`
    // - launcher.exe:0x4816f0 = reused inline helper symbolized as
    //   `LaunchPadClient_GetVtableOffset` (keep as an anchor, not a class-identity claim)
    // - launcher.exe:0x41cfb0 = CLTLoginMediator_PostEvent (event posting mechanism)
    // - launcher.exe:0x41b450 = CLTLoginMediator_SwitchHelperState (switches helper dispatch table)
    // - launcher.exe:0x41d090 = CLTLoginMediator_PostError (error reporting via fprintf)
    //
    // The 16 slots at 0x4f7868..0x4f78a0 are:
    //   Slot 0x68: helper for event dispatch table entry 0
    //   Slot 0x6c: helper for event dispatch table entry 1
    //   Slot 0x70: helper for event dispatch table entry 2
    //   ... and so on through slot 0xa0
    // Each slot ultimately stores a helper/state object whose vtable slot 1 commonly points to
    // `0x438d80`; the installed object pointer itself carries the concrete `CLTLoginState_*` vtable.
    //
    // From disassembly of 0x438d80:
    //   - Calls the helper currently named `LaunchPadClient_GetVtableOffset(this+8)` to get a
    //     vtable offset
    //   - Checks if event flag at [this+0x2c] is set
    //   - If event flag set, calls CLTLoginMediator_PostEvent(this, 1)
    //   - Otherwise calls vtable[+0x178]() and updates state at [this+0x80]
    //
    // The re-implementation calls the helper methods to properly initialize all slots.
    InitializeHelperDispatchSlot15();
    InitializeHelperDispatchSlot16();
    InitializeHelperDispatchSlot17();
    InitializeHelperDispatchSlot18();
    InitializeHelperDispatchSlot19();
}

//
// These are called from InitializeConnectionHelpers() to properly initialize the
// helper dispatch table slots beyond the main 15 (0x68-0xa0).

void CLTLoginMediator::InitializeHelperDispatchSlot15() {
    // Address anchor: launcher.exe:0x420640 = InitializeHelperDispatchSlot15
    // Original: allocates 8 bytes, stores vtable 0x4b51e0 (`CLTLoginState_State0`)
    // Current best concrete object identity: heap object with vtable `0x4b51e0` (`CLTLoginState_State0`).
    void* ptr = malloc(8);
    if (ptr) {
        *(void**)ptr = reinterpret_cast<void*>(0x4b51e0);
        helpers_.helper78A4 = ptr;
    }
}

void CLTLoginMediator::InitializeHelperDispatchSlot16() {
    // Address anchor: launcher.exe:0x4206e0 = InitializeHelperDispatchSlot16
    // Original: allocates 4 bytes, stores vtable 0x4b4fec (`CLTLoginState_WorldListPending`)
    void* ptr = malloc(4);
    if (ptr) {
        *(void**)ptr = reinterpret_cast<void*>(0x4b4fec);
        helpers_.helper78A8 = ptr;
    }
}

void CLTLoginMediator::InitializeHelperDispatchSlot17() {
    // Address anchor: launcher.exe:0x420850 = InitializeHelperDispatchSlot17
    // Original: allocates 4 bytes, stores vtable 0x4b4fc4 (`CLTLoginState_State1`)
    void* ptr = malloc(4);
    if (ptr) {
        *(void**)ptr = reinterpret_cast<void*>(0x4b4fc4);
        helpers_.helper78AC = ptr;
    }
}

void CLTLoginMediator::InitializeHelperDispatchSlot18() {
    // Address anchor: launcher.exe:0x420920 = InitializeHelperDispatchSlot18
    // Original: allocates 8 bytes, stores vtable 0x4b5014 (`CLTLoginState_AuthenticatePending`)
    void* ptr = malloc(8);
    if (ptr) {
        *(void**)ptr = reinterpret_cast<void*>(0x4b5014);
        helpers_.helper78B0 = ptr;
    }
}

void CLTLoginMediator::InitializeHelperDispatchSlot19() {
    // Address anchor: launcher.exe:0x4209a0 = InitializeHelperDispatchSlot19
    // Original: allocates 4 bytes, stores vtable 0x4b508c (`CLTLoginState_State6`)
    void* ptr = malloc(4);
    if (ptr) {
        *(void**)ptr = reinterpret_cast<void*>(0x4b508c);
        helpers_.helper78B4 = ptr;
    }
}

uint32_t CLTLoginMediator::BeginAuthConnection() {
    // Address anchors:
    // - launcher.exe:0x41d170 = strongest current BeginAuthConnection implementation
    // - launcher.exe:0x439090 = CLTLoginMediator_Helper1_StartAuthConnection (upstream)
    // - launcher.exe:0x41e500 = downstream connection initializer call site
    //
    // Current best launcher path:
    // - copy current `qsAuthServerDNSName` into owner `+0x4c`
    // - read `AuthServerPort` from owner `+0x4c8`
    // - build endpoint at owner `+0x5c`
    // - allocate auth-side CMessageConnection child
    // - call `connection->+0x1c(owner+0x5c)` (ensure-connected wrapper)
    authGetPublicKeyRequestSent_ = false;
    authRequestSent_ = false;
    authChallengeResponseSent_ = false;
    lastAuthPublicKeyReply_ = mxo::auth::GetPublicKeyReply();
    lastAuthRequestBuildResult_ = mxo::auth::AuthRequestBuildResult();
    lastAuthChallenge_ = mxo::auth::AuthChallenge();
    lastAuthReply_ = mxo::auth::AuthReply();
    expectedAuthRequestName_ = nullptr;
    expectedMarginRequestName_ = nullptr;
    BuildAuthEndpoint();
    auto* connection = EnsureAuthConnectionObject();
    if (!connection) return 0;
    connection->SetRemoteHostName(authServerDnsName_.c_str());
    connection->SetRemoteEndpoint(authEndpoint_);
    return connection->EnsureConnected();
}

uint32_t CLTLoginMediator::HandleAuthConnectStatus(uint32_t workResultCode) {
    lastAuthConnectStatus_ = workResultCode;
    ++authConnectStatusCount_;
    return (workResultCode == kConnectStatusSuccess) ? BeginAuthHandshake() : 0u;
}

uint32_t CLTLoginMediator::HandleMarginConnectStatus(uint32_t workResultCode) {
    lastMarginConnectStatus_ = workResultCode;
    ++marginConnectStatusCount_;
    if (workResultCode == kConnectStatusSuccess && marginConnection_ != nullptr) {
        // Active state8/state10 sender gates at `0x41b4b0` require owner `+0x1c` connection state `== 2`.
        // On the current scaffold path the successful margin connect-status callback is the
        // narrowest evidence-backed place to promote the live margin connection into that ready
        // send state before slot-3 send bodies run.
        marginConnection_->SetState(mxo::liblttcp::LTTCPEngineConnectionState::kUdpMonitorActive);
        spdlog::info(
            "CLTLoginMediator::HandleMarginConnectStatus promoted margin connection to ready state=2 after connect-status success currentState={}",
            currentState_ ? currentState_->DebugName() : "<null>");
    }
    return (workResultCode == kConnectStatusSuccess) ? BeginMarginHandshake() : 0u;
}

uint32_t CLTLoginMediator::BeginAuthHandshake() {
    // Address anchors (NOW WITH ACTUAL FUNCTION NAMES):
    // - launcher.exe:0x439210 = CLTLoginMediator_Helper2_BeginAuthBootstrap
    // - launcher.exe:0x448050 = AuthBootstrap680_PrepareAndDispatch (upstream dispatcher)
    // - launcher.exe:0x447eb0 = AuthBootstrap680_SendGetPublicKeyRequest (raw 0x06 send builder)
    // - launcher.exe:0x4474f0 = AuthBootstrap680_SendAuthRequest (raw 0x08 send builder)
    // - launcher.exe:0x448050 = branch site selecting raw 0x06 vs raw 0x08 path
    //
    // Transitional note:
    // - current scaffold still begins with an explicit 0x06 request step
    // - original helper-state progression is not yet fully rebuilt around 0x448050
    // The standalone auth probe is the current wire/reference implementation for the
    // launcher-owned auth loop. Reuse the shared low-level runtime-style auth helpers here
    // instead of keeping a second launcher-only packet path.
    expectedAuthRequestName_ = kMessageAsGetPublicKeyRequest;
    return SendAuthGetPublicKeyRequest();
}

uint32_t CLTLoginMediator::BeginMarginHandshake() {
    // Important correction from newer helper-state review:
    // - the immediate post-`AS_AuthReply` continuation is not the later auth-side
    //   `AS_GetWorldListRequest` helper at `0x43b830`
    // - original `0x4401a0` success instead reaches `0x41b450(0x0b)`, which selects helper
    //   `0x4f7894` and immediately runs `CLTLoginState_State11` slot 3 /
    //   `0x43c020`
    // - keep that ownership split explicit in source too:
    //   - mediator only owns the shared margin-connection transport helper
    //   - the concrete send body remains on the active CLTLoginState vtable object
    if (!currentState_) {
        Log("DIAGNOSTIC: BeginMarginHandshake has no active CLTLoginState to dispatch");
        return 0u;
    }

    return currentState_->Slot3_BeginOrContinue(/*upstreamOrArg=*/currentState_, this);
}

// anchor: launcher.exe:0x41ecd0
uint32_t CLTLoginMediator::ProcessLoginRequest(const ProcessLoginRequestInputSketch& input) {
    const uint32_t stateCode = currentState_ ? currentState_->DispatchPhaseCode() : 0u;
    switch (stateCode) {
        case 1u:
        case 2u:
        case 3u:
            return 0x12000006u;
        case 4u:
        case 6u:
        case 7u:
        case 8u:
        case 9u:
        case 10u:
        case 11u:
            return 0x12000000u;
        case 12u:
            return 0x12000007u;
        default:
            break;
    }

    if ((input.inlineString00[0] == '\0' || input.inlineString20[0] == '\0') &&
        input.string60.current == input.string60.begin) {
        return 4u;
    }

    authBootstrapSource38_.inlineString00 = input.inlineString00;
    authBootstrapSource38_.inlineString20 = input.inlineString20;
    authBootstrapSource38_.block40 = input.block40;
    authBootstrapSource38_.block50 = input.block50;
    authBootstrapSource38_.flag6C = input.flag6C;
    AssignOwnedSmallString(authBootstrapSource38_, input.string60.begin, input.string60.current);

    if (currentState_ && currentState_->DispatchPhaseCode() == 2u) {
        Log(
            "DIAGNOSTIC: ProcessLoginRequest copied owner+0x94 username='%s' password='%s' string60Len=%u and remains on helper state 2",
            authBootstrapSource38_.inlineString00[0] ? authBootstrapSource38_.inlineString00.data() : "<empty>",
            authBootstrapSource38_.inlineString20[0] ? authBootstrapSource38_.inlineString20.data() : "<empty>",
            (unsigned)authBootstrapSource38_.string60Owned.size());
    } else {
        Log(
            "DIAGNOSTIC: ProcessLoginRequest copied owner+0x94 username='%s' password='%s' string60Len=%u (state-switch scaffolding beyond active state-2 branch still partial)",
            authBootstrapSource38_.inlineString00[0] ? authBootstrapSource38_.inlineString00.data() : "<empty>",
            authBootstrapSource38_.inlineString20[0] ? authBootstrapSource38_.inlineString20.data() : "<empty>",
            (unsigned)authBootstrapSource38_.string60Owned.size());
    }

    // Active original branch observed under WineDbg had `DAT_004d66ec == 0`, which means:
    // - clear owner `+0xf4` (`+0x94 + 0x60`) through the small-string helper
    // - switch helper state to `2`
    // Current source scaffold keeps the already-live state-2 side conservative and only mirrors
    // the owner-state mutation here.
    AssignOwnedSmallString(authBootstrapSource38_, nullptr, nullptr);
    Log(
        "DIAGNOSTIC: ProcessLoginRequest mirrored default DAT_004d66ec==0 branch by clearing owner+0xf4 small-string state");
    return 0u;
}

// anchor: launcher.exe:0x41c1f0
uint32_t CLTLoginMediator::PersistSelectionContextForState8(const State3SelectionContextInputSketch& input) {
    // Current best recovered live branch after password confirmation:
    // - owner `+0xec` / `0x41ecd0` is hit on the original launcher path
    // - runtime then transitions `0 -> 2 -> 3 -> 8`
    // - while current helper vtable is `0x004b5208` (state `3`), `0x41c1f0` writes this owner
    //   snapshot block and switches helper state to `8`
    //
    // Structural writes:
    // - owner byte `+0xcc8` from input `+0x00` if `< 100`
    // - owner `+0xcd0 .. +0xd7f` from input `+0x04 .. +0xb3`
    // - then `CLTLoginMediator_SwitchHelperState(..., 8)`
    //
    // This now looks closely related to the already recovered arg6 `+0xec` / `+0xf4`
    // selection/config snapshot family, but exact semantic names remain provisional.
    if (input.slotOrSelectionIndex00 >= 100u) {
        return 0u;
    }

    state8SelectionContextSnapshotState_.slotOrSelectionIndexCc8 =
        static_cast<uint8_t>(input.slotOrSelectionIndex00);
    std::copy(input.block04.begin(), input.block04.end(), state8SelectionContextSnapshotState_.blockCd0.begin());
    std::copy(input.block14.begin(), input.block14.end(), state8SelectionContextSnapshotState_.blockCe0.begin());
    std::copy(input.block24.begin(), input.block24.end(), state8SelectionContextSnapshotState_.blockCf0.begin());
    std::copy(input.block34.begin(), input.block34.end(), state8SelectionContextSnapshotState_.blockD00.begin());
    std::copy(input.block44.begin(), input.block44.end(), state8SelectionContextSnapshotState_.blockD10.begin());
    std::copy(input.block54.begin(), input.block54.end(), state8SelectionContextSnapshotState_.blockD20.begin());
    std::copy(input.block64.begin(), input.block64.end(), state8SelectionContextSnapshotState_.blockD30.begin());
    std::copy(input.block74.begin(), input.block74.end(), state8SelectionContextSnapshotState_.blockD40.begin());
    std::copy(input.block84.begin(), input.block84.end(), state8SelectionContextSnapshotState_.blockD50.begin());
    std::copy(input.block94.begin(), input.block94.end(), state8SelectionContextSnapshotState_.blockD60.begin());
    std::copy(input.blockA4.begin(), input.blockA4.end(), state8SelectionContextSnapshotState_.blockD70.begin());

    if (scaffoldState8_ != nullptr) {
        SwitchHelperStateScaffold(8u, scaffoldState8_);
    }

    Log(
        "DIAGNOSTIC: PersistSelectionContextForState8 mirrored state3->8 selection snapshot slot=0x%02x blockCd0_0=0x%08x blockD70_3=0x%08x currentState=%s",
        (unsigned)state8SelectionContextSnapshotState_.slotOrSelectionIndexCc8,
        (unsigned)state8SelectionContextSnapshotState_.blockCd0[0],
        (unsigned)state8SelectionContextSnapshotState_.blockD70[3],
        currentState_ ? currentState_->DebugName() : "<unchanged>");
    return 0u;
}

// anchor: launcher.exe:0x41c3c0
uint32_t CLTLoginMediator::ProcessLoginCredentials(const ProcessLoginCredentialsInputSketch& input) {
    // Current best recovered writer for the helper11 owner source block.
    // Important current active-path caution from live original WineDbg runs:
    // - this is a real writer, but the observed password-submit branch first goes through
    //   `0x41ecd0 -> 0x41c1f0 -> state 8`
    // - so keep this method class-faithful and source-owned, but do not currently treat it as the
    //   default first target for launcher startup reconstruction
    // - newer `0x43e540` debug-printer review also confirms what this writer is feeding later:
    //   the 17 dwords copied to `+0x134..+0x177` are the appearance/customization family
    //   (`SkinToneID .. TraitID`), while `+0x178/+0x198/+0x1b8` become
    //   `RealFirstName/RealLastName/Background`
    // Original `0x41c3c0` writes:
    // - owner `+0x12c` from input `+0x24`
    // - owner `+0x134 .. +0x153` from input `+0x2c`
    // - owner `+0x154 .. +0x173` from input `+0x4c`
    // - owner `+0x174 .. +0x177` from input `+0x6c`
    // - owner strings/blocks at `+0x108 / +0x178 / +0x198 / +0x1b8`
    // - then switches helper state to `10`
    //
    // This source-owned mirror preserves the recovered data movement but does not yet claim the
    // exact upstream caller that feeds the original input blob.
    std::copy(input.string00.begin(), input.string00.end(), postAuthMarginLoadingState_.sourceLeadString108.begin());
    postAuthMarginLoadingState_.sourceField128 = input.field20;
    postAuthMarginLoadingState_.sourceField12c = input.field24;
    postAuthMarginLoadingState_.sourceField130 = input.field28;

    std::copy(input.dwords2c.begin(), input.dwords2c.end(), postAuthMarginLoadingState_.sourceDwords134.begin());
    std::copy(input.dwords4c.begin(), input.dwords4c.end(), postAuthMarginLoadingState_.sourceDwords134.begin() + 8);
    postAuthMarginLoadingState_.sourceDwords134[16] =
        static_cast<uint32_t>(input.bytes6c[0]) |
        (static_cast<uint32_t>(input.bytes6c[1]) << 8) |
        (static_cast<uint32_t>(input.bytes6c[2]) << 16) |
        (static_cast<uint32_t>(input.bytes6c[3]) << 24);

    std::copy(input.string70.begin(), input.string70.end(), postAuthMarginLoadingState_.sourceBlock178.begin());
    std::copy(input.string90.begin(), input.string90.end(), postAuthMarginLoadingState_.sourceBlock198.begin());
    std::copy(input.stringB0.begin(), input.stringB0.end(), postAuthMarginLoadingState_.sourceBlock1b8.begin());

    Log(
        "DIAGNOSTIC: ProcessLoginCredentials mirrored recovered helper11 source write name='%s' field12c=0x%08x firstDword134=0x%08x",
        postAuthMarginLoadingState_.sourceLeadString108.data(),
        (unsigned)postAuthMarginLoadingState_.sourceField12c,
        (unsigned)postAuthMarginLoadingState_.sourceDwords134[0]);
    return 0u;
}

// anchor: launcher.exe:0x41c5c0
uint32_t CLTLoginMediator::DispatchSecondaryMessageToOwnerCallback84(void* workItem) {
    // Current best read from `0x41c5c0`:
    // - if owner `+0x84` is null, return `1`
    // - otherwise derive the incoming secondary-message opcode through `0x41bc20`
    // - then call callback84 vtable `+0x0c(&opcodeStorage, workItem)`
    // Current source scaffold note:
    // - the replacement launcher does not yet materialize the original message wrapper object
    // - so this mirror derives the opcode from staged auth/margin bytes when possible
    if (!ownerCallback84_) {
        return 1u;
    }

    uint32_t opcodeStorage = 0u;
    if (!stagedIncomingMarginPacketBytes_.empty()) {
        opcodeStorage = ReadOpcodePrefixVariableWidth(
            stagedIncomingMarginPacketBytes_.data(),
            stagedIncomingMarginPacketBytes_.size());
    } else if (!stagedIncomingAuthPacketBytes_.empty()) {
        mxo::auth::FramedPacket framedPacket;
        if (mxo::auth::ParseVariableLengthPacket(
                stagedIncomingAuthPacketBytes_.data(),
                stagedIncomingAuthPacketBytes_.size(),
                &framedPacket) &&
            !framedPacket.payloadBytes.empty()) {
            opcodeStorage = ReadOpcodePrefixVariableWidth(
                framedPacket.payloadBytes.data(),
                framedPacket.payloadBytes.size());
        }
    }

    using OwnerCallback84DispatchSecondaryFn = uint32_t(__thiscall*)(void*, uint32_t*, void*);
    void** vtable = *reinterpret_cast<void***>(ownerCallback84_);
    if (!vtable || !vtable[3]) {
        return 1u;
    }

    const auto dispatchFn = reinterpret_cast<OwnerCallback84DispatchSecondaryFn>(vtable[3]);
    const uint32_t dispatchResult = dispatchFn(ownerCallback84_, &opcodeStorage, workItem);
    spdlog::info(
        "CLTLoginMediator::DispatchSecondaryMessageToOwnerCallback84 callback84={} opcode=0x{:04x} workItem={} -> dispatchResult=0x{:08x}",
        fmt::ptr(ownerCallback84_),
        static_cast<unsigned>(opcodeStorage & 0xffffu),
        fmt::ptr(workItem),
        static_cast<unsigned>(dispatchResult));
    return dispatchResult;
}

// anchor: launcher.exe:0x41c510
uint32_t CLTLoginMediator::SetState9OptionalField90AndSwitchToState13(uint32_t field90Value) {
    const uint32_t stateCode = currentState_ ? currentState_->DispatchPhaseCode() : 0u;
    switch (stateCode) {
        case 0u:
        case 1u:
        case 2u:
        case 3u:
            return 0x12000006u;
        case 4u:
        case 6u:
        case 7u:
        case 8u:
        case 9u:
        case 10u:
        case 11u:
            return 0x12000000u;
        case 12u:
            ownerOptionalField90_ = field90Value;
            if (scaffoldState13_ != nullptr) {
                SwitchHelperStateScaffold(0x0du, scaffoldState13_);
            }
            spdlog::info(
                "CLTLoginMediator::SetState9OptionalField90AndSwitchToState13 stored owner+0x90=0x{:08x} currentState={}",
                static_cast<unsigned>(ownerOptionalField90_),
                currentState_ ? currentState_->DebugName() : "<unchanged>");
            return 0u;
        default:
            return 1u;
    }
}

// anchor: launcher.exe:0x41de40
uint32_t CLTLoginMediator::State9SubmitFollowupScaffold(uint8_t helperByte4, uint16_t helperWord6) {
    // Current best read from `0x41de40` + `0x439780`:
    // - natural original now really does hit `0x41de40` immediately after `0x439780`
    // - representative live stop shape:
    //   - `0x439780`: helper byte `this+4 = 0`, helper word `this+6 = 0x2710`
    //   - `0x41de40`: `ECX = owner (0x4d4e38)`, `EAX = helperWord6 (0x2710)`,
    //     `EDX = state9 vtable (0x004b517c)`
    // - owner callback object `+0x84` fills two dwords through vtable `+0x38`
    // - owner `+0x1c + 0x24..0x30` seeds a local transport/address block
    // - `0x44afd0` / `0x44b0d0` derive a packet-like payload from helper word `+6`
    // - owner object `+0x88` chooses between direct send (`+0x28`) and handle-based send
    //   (`+0x1c`, `+0x18`, `+0x24`) after testing `(+0x44)->(+0x30)`
    // - representative natural-original stop also had non-null owner callback/object triple
    //   at `+0x84/+0x88/+0x8c`
    // - owner dword `+0x90` is only forwarded when helper byte `+4 != 0`
    // - owner dword `+0x147c` caches the acquired handle on the managed-send branch
    //
    // The concrete owner-side collaborators remain unresolved in source, so keep this helper
    // narrow and structural for now. Returning `0` preserves the observed `0x439780` success-side
    // event-post shape (`< 1` => post event `0x17`).
    const uint32_t forwardedArg90 = helperByte4 != 0u ? ownerOptionalField90_ : 0u;
    spdlog::info(
        "CLTLoginMediator::State9SubmitFollowupScaffold helperByte4=0x{:02x} helperWord6=0x{:04x} callback84={} object88={} object8c={} forwardedArg90=0x{:08x} cachedHandle147c={} (natural original now proven into 0x41de40; deeper owner collaborators unresolved)",
        static_cast<unsigned>(helperByte4),
        static_cast<unsigned>(helperWord6),
        fmt::ptr(ownerCallback84_),
        fmt::ptr(ownerObject88_),
        fmt::ptr(ownerObject8c_),
        static_cast<unsigned>(forwardedArg90),
        ownerCachedHandle147c_);
    return 0u;
}

// anchor: launcher.exe:0x41b420
uint32_t CLTLoginMediator::HandleState9Opcode11SuccessSideEffect() {
    // Current best read from `0x41b420`, reached by state9 slot 6 / `0x43c180` success:
    // - natural original is now live-proven onto the `0x43c180` success side too
    // - representative natural stop at `0x43c1c2` showed owner `+0x80 = 0` just before the
    //   vtable `+0x16c` call
    // - if owner `+0x1c` is null, return false-ish
    // - clear owner byte `+0xf14`
    // - set owner byte `+0x2d`
    // - if margin connection state `+0x34` is `1` or `2`, call connection vtable `+0x0c(1)`
    //
    // Keep the vtable `+0x0c` call explicit but unresolved at the class level for now; the
    // evidence-backed owner-side state mutation is enough to keep the active state9 path faithful.
    if (!marginConnection_) {
        return 0u;
    }

    postAuthMarginLoadingState_.state10SendGateFlagF14 = 0u;
    marginConnectionFlag2d_ = 1u;

    const uint32_t rawState = static_cast<uint32_t>(marginConnection_->State());
    const bool wouldCallConnectionVtable0c =
        rawState == static_cast<uint32_t>(mxo::liblttcp::LTTCPEngineConnectionState::kConnectActive) ||
        rawState == static_cast<uint32_t>(mxo::liblttcp::LTTCPEngineConnectionState::kUdpMonitorActive);

    spdlog::info(
        "CLTLoginMediator::HandleState9Opcode11SuccessSideEffect cleared owner+0xf14, set owner+0x2d, marginConnectionState={} wouldCallConnectionVtable0cArg1={}",
        rawState,
        wouldCallConnectionVtable0c ? 1u : 0u);
    return 1u;
}

// =============================================================================
// HELPER11: Post-Auth Margin/Loading State (launcher.exe:0x4f78b8)
// =============================================================================
// Recovered from Ghidra analysis of launcher.exe helper/state11 functions:
// - 0x43c020 = CLTLoginState_State11 slot-3 send body
//   Builds/sends margin packet with first payload byte 0x4d, posts event 0x15
// - 0x440320 = CLTLoginState_State11 slot-6 reply body
//   Handles MS_LoadCharacterReply (0x10), accumulates fragments into +0xf1c,
//   posts event 0x16 on completion
// =============================================================================

// anchor: launcher.exe:0x41b4b0
bool CLTLoginMediator::State10HasReadyConnectionState2() const {
    // Exact recovered gate from `0x41b4b0`:
    // - owner `+0x1c` must be non-null
    // - connection state field `+0x34` must equal `2`
    const mxo::liblttcp::CMessageConnection* connection = MarginConnection();
    return connection != nullptr &&
           connection->State() == mxo::liblttcp::LTTCPEngineConnectionState::kUdpMonitorActive;
}

// anchor: launcher.exe:0x440320
uint32_t CLTLoginMediator::State11HandleLoadCharacterReplyScaffold(const uint8_t* packetBytes, size_t packetSize) {
    // Evidence-backed current read:
    // - validates raw margin code `0x10`
    // - on first helper11 fragment it clears/seeds the owner `+0xf1c` family
    // - section selector `(byte+0x0d) - 2` drives later per-section writes
    // - cases `3/4/5/6` append bytes into owned buffers at `+0x1418/+0x1420/+0x1428/+0x1408`
    // - case `0` fills the `+0xf88/+0x13cc/+0x13d0` family plus three inline strings
    if (!packetBytes || packetSize < 0x10) {
        return 0u;
    }
    if (packetBytes[0] != 0x10) {
        return 0u;
    }

    const uint32_t parsedStatus = ReadU32LE(packetBytes + 1);
    const uint32_t parsedField05 = ReadU32LE(packetBytes + 5);
    const uint16_t parsedHandoffWord09 = ReadU16LE(packetBytes + 9);
    const uint8_t parsedExpectedCount0b = packetBytes[0x0b];
    const uint8_t parsedSeedCount0c = packetBytes[0x0c];
    const uint8_t parsedSelectorMinus2 = static_cast<uint8_t>(packetBytes[0x0d] - 2u);
    const uint16_t sectionOffset0e = ReadU16LE(packetBytes + 0x0e);

    postAuthMarginLoadingState_.worldListCountOrStatus80 = parsedStatus;
    if (parsedStatus >= 1u) {
        postAuthMarginLoadingState_.sourceLeadString108[0] = '\0';
        postAuthMarginLoadingState_.characterRouteIndexCc8 = 0xffu;
        Log(
            "DIAGNOSTIC: helper11 load-character scaffold observed non-success status=0x%08x handoffWord=0x%04x and mirrored failure-side owner clears",
            (unsigned)parsedStatus,
            (unsigned)parsedHandoffWord09);
        return 0u;
    }

    uint16_t sectionByteCount = 0u;
    const uint8_t* sectionData = nullptr;
    if (sectionOffset0e != 0u && static_cast<size_t>(sectionOffset0e) + 2u <= packetSize) {
        sectionByteCount = ReadU16LE(packetBytes + sectionOffset0e);
        const size_t sectionDataOffset = static_cast<size_t>(sectionOffset0e) + 2u;
        if (sectionDataOffset <= packetSize) {
            sectionData = packetBytes + sectionDataOffset;
            const size_t remaining = packetSize - sectionDataOffset;
            if (sectionByteCount > remaining) {
                sectionByteCount = static_cast<uint16_t>(remaining);
            }
        }
    }

    const bool firstFragment = (postAuthMarginLoadingState_.characterNameBufferF1c[0] == '\0');
    bool usedCurrentSlotRecord = false;
    if (firstFragment) {
        std::fill(
            std::begin(postAuthMarginLoadingState_.characterNameBufferF1c),
            std::end(postAuthMarginLoadingState_.characterNameBufferF1c),
            '\0');
        postAuthMarginLoadingState_.characterReplyFieldF3c = parsedField05;
        postAuthMarginLoadingState_.characterReplyFieldF40 = postAuthMarginLoadingState_.sourceField12c;
        std::fill(postAuthMarginLoadingState_.characterFlagsF48.begin(), postAuthMarginLoadingState_.characterFlagsF48.end(), 0u);
        std::fill(postAuthMarginLoadingState_.secondaryCharacterDataF68.begin(), postAuthMarginLoadingState_.secondaryCharacterDataF68.end(), 0u);
        std::fill(postAuthMarginLoadingState_.characterRecordPointersF88.begin(), postAuthMarginLoadingState_.characterRecordPointersF88.end(), 0u);
        std::fill(postAuthMarginLoadingState_.section0StringF8c.begin(), postAuthMarginLoadingState_.section0StringF8c.end(), '\0');
        std::fill(postAuthMarginLoadingState_.section0StringFac.begin(), postAuthMarginLoadingState_.section0StringFac.end(), '\0');
        std::fill(postAuthMarginLoadingState_.section0StringFcc.begin(), postAuthMarginLoadingState_.section0StringFcc.end(), '\0');
        postAuthMarginLoadingState_.replySectionData13cc = 0u;
        postAuthMarginLoadingState_.replySectionData13d0 = 0u;
        postAuthMarginLoadingState_.section0Flag13f6 = 0u;
        postAuthMarginLoadingState_.flag13fe = 1u;
        postAuthMarginLoadingState_.flag1406 = 1u;
        postAuthMarginLoadingState_.flag1416 = 1u;
        postAuthMarginLoadingState_.flag1448 = 1u;
        postAuthMarginLoadingState_.flag1452 = 1u;

        if (postAuthMarginLoadingState_.allocatedBuffer1418) {
            std::free(postAuthMarginLoadingState_.allocatedBuffer1418);
            postAuthMarginLoadingState_.allocatedBuffer1418 = nullptr;
        }
        if (postAuthMarginLoadingState_.allocatedBuffer1420) {
            std::free(postAuthMarginLoadingState_.allocatedBuffer1420);
            postAuthMarginLoadingState_.allocatedBuffer1420 = nullptr;
        }
        if (postAuthMarginLoadingState_.allocatedBuffer1428) {
            std::free(postAuthMarginLoadingState_.allocatedBuffer1428);
            postAuthMarginLoadingState_.allocatedBuffer1428 = nullptr;
        }
        if (postAuthMarginLoadingState_.allocatedBuffer1408) {
            std::free(postAuthMarginLoadingState_.allocatedBuffer1408);
            postAuthMarginLoadingState_.allocatedBuffer1408 = nullptr;
        }
        postAuthMarginLoadingState_.allocatedBufferLength141c = 0u;
        postAuthMarginLoadingState_.allocatedBufferLength1424 = 0u;
        postAuthMarginLoadingState_.allocatedBufferLength142c = 0u;
        postAuthMarginLoadingState_.allocatedBufferLength140c = 0u;
        postAuthMarginLoadingState_.allocatedBufferFlag141e = 0u;
        postAuthMarginLoadingState_.allocatedBufferFlag1426 = 0u;
        postAuthMarginLoadingState_.allocatedBufferFlag142e = 0u;
        postAuthMarginLoadingState_.allocatedBufferFlag140e = 0u;

        if (const SlotRecordState004b5328* currentSlotRecord = GetCurrentSlotRecord()) {
            const size_t copyCount = std::min(
                currentSlotRecord->heapString14.size(),
                sizeof(postAuthMarginLoadingState_.characterNameBufferF1c) - 1);
            std::copy_n(
                currentSlotRecord->heapString14.data(),
                copyCount,
                postAuthMarginLoadingState_.characterNameBufferF1c);
            postAuthMarginLoadingState_.characterNameBufferF1c[copyCount] = '\0';
            postAuthMarginLoadingState_.secondaryCharacterDataF68[0] = currentSlotRecord->worldId0c;
            postAuthMarginLoadingState_.secondaryCharacterDataF68[1] = currentSlotRecord->status0b;
            usedCurrentSlotRecord = true;
        } else {
            std::copy(
                postAuthMarginLoadingState_.sourceLeadString108.begin(),
                postAuthMarginLoadingState_.sourceLeadString108.end(),
                postAuthMarginLoadingState_.characterNameBufferF1c);
            postAuthMarginLoadingState_.secondaryCharacterDataF68[0] = postAuthMarginLoadingState_.sourceField12c;
            postAuthMarginLoadingState_.secondaryCharacterDataF68[1] = 0u;
        }

        std::copy_n(
            postAuthMarginLoadingState_.sourceDwords134.begin(),
            postAuthMarginLoadingState_.characterFlagsF48.size(),
            postAuthMarginLoadingState_.characterFlagsF48.begin());
    }

    switch (parsedSelectorMinus2) {
        case 0u:
            if (sectionData && sectionByteCount >= 0x44cu) {
                postAuthMarginLoadingState_.characterRecordPointersF88[0] = ReadU32LE(sectionData + 0x00);
                postAuthMarginLoadingState_.replySectionData13cc = ReadU32LE(sectionData + 0x444);
                postAuthMarginLoadingState_.replySectionData13d0 = ReadU32LE(sectionData + 0x448);
                CopyCStringIntoFixed(
                    postAuthMarginLoadingState_.section0StringF8c.data(),
                    postAuthMarginLoadingState_.section0StringF8c.size(),
                    sectionData + 0x04,
                    sectionByteCount - 0x04);
                CopyCStringIntoFixed(
                    postAuthMarginLoadingState_.section0StringFac.data(),
                    postAuthMarginLoadingState_.section0StringFac.size(),
                    sectionData + 0x24,
                    sectionByteCount > 0x24 ? sectionByteCount - 0x24 : 0u);
                CopyCStringIntoFixed(
                    postAuthMarginLoadingState_.section0StringFcc.data(),
                    postAuthMarginLoadingState_.section0StringFcc.size(),
                    sectionData + 0x44,
                    sectionByteCount > 0x44 ? sectionByteCount - 0x44 : 0u);
                postAuthMarginLoadingState_.section0Flag13f6 = 1u;
            }
            break;
        case 3u:
            AppendOwnedSectionBytes(
                postAuthMarginLoadingState_.allocatedBuffer1418,
                postAuthMarginLoadingState_.allocatedBufferLength141c,
                sectionData,
                sectionByteCount);
            postAuthMarginLoadingState_.allocatedBufferFlag141e = 1u;
            break;
        case 4u:
            AppendOwnedSectionBytes(
                postAuthMarginLoadingState_.allocatedBuffer1420,
                postAuthMarginLoadingState_.allocatedBufferLength1424,
                sectionData,
                sectionByteCount);
            postAuthMarginLoadingState_.allocatedBufferFlag1426 = 1u;
            break;
        case 5u:
            AppendOwnedSectionBytes(
                postAuthMarginLoadingState_.allocatedBuffer1428,
                postAuthMarginLoadingState_.allocatedBufferLength142c,
                sectionData,
                sectionByteCount);
            postAuthMarginLoadingState_.allocatedBufferFlag142e = 1u;
            break;
        case 6u:
            AppendOwnedSectionBytes(
                postAuthMarginLoadingState_.allocatedBuffer1408,
                postAuthMarginLoadingState_.allocatedBufferLength140c,
                sectionData,
                sectionByteCount);
            postAuthMarginLoadingState_.allocatedBufferFlag140e = 1u;
            break;
        case 0x0bu:
            Log(
                "DIAGNOSTIC: helper11 load-character scaffold observed section 0x0b with byteCount=%u; downstream `0x43f8c0` side effect still unresolved",
                (unsigned)sectionByteCount);
            break;
        default:
            break;
    }

    Log(
        "DIAGNOSTIC: helper11 load-character scaffold status=0x%08x field05=0x%08x handoffWord=0x%04x expectedCount=%u seedCount=%u section=%u sectionBytes=%u firstFragment=%u usedCurrentSlotRecord=%u name='%s'",
        (unsigned)parsedStatus,
        (unsigned)parsedField05,
        (unsigned)parsedHandoffWord09,
        (unsigned)parsedExpectedCount0b,
        (unsigned)parsedSeedCount0c,
        (unsigned)parsedSelectorMinus2,
        (unsigned)sectionByteCount,
        firstFragment ? 1u : 0u,
        usedCurrentSlotRecord ? 1u : 0u,
        postAuthMarginLoadingState_.characterNameBufferF1c);
    return 1u;
}

const char* CLTLoginMediator::ExpectedAuthRequestName() const {
    return expectedAuthRequestName_ ? expectedAuthRequestName_ : "";
}

const char* CLTLoginMediator::ExpectedMarginRequestName() const {
    return expectedMarginRequestName_ ? expectedMarginRequestName_ : "";
}

// anchor: launcher.exe:0x41f2e0
const CLTLoginMediator::SlotRecordState004b5328* CLTLoginMediator::GetSlotRecordByIndex(uint8_t slotIndex) const {
    if (slotIndex >= slotRecordValid688_.size() || !slotRecordValid688_[slotIndex]) {
        return nullptr;
    }
    return &slotRecords688_[slotIndex];
}

// anchor: launcher.exe:0x41f300
const CLTLoginMediator::SlotRecordState004b5328* CLTLoginMediator::GetCurrentSlotRecord() const {
    return GetSlotRecordByIndex(postAuthMarginLoadingState_.characterRouteIndexCc8);
}

// anchor: launcher.exe:0x41b220
const char* CLTLoginMediator::GetSlotRecordHeapStringByIndex(uint8_t slotIndex) const {
    const SlotRecordState004b5328* record = GetSlotRecordByIndex(slotIndex);
    if (!record || record->heapString14.empty()) {
        return nullptr;
    }
    return record->heapString14.c_str();
}

// anchor: launcher.exe:0x41f310
void* CLTLoginMediator::GetSessionCallbackHelper65c() const {
    // Tiny owner-vtable getter used by the later session-callback helper family.
    return sessionCallbackHelper65c_;
}

// anchor: launcher.exe:0x41f320
const char* CLTLoginMediator::GetGameSessionId664() const {
    return gameSessionId664_.empty() ? nullptr : gameSessionId664_.c_str();
}

// anchor: launcher.exe:0x41af70
uint32_t CLTLoginMediator::SendCurrentMarginPacketScaffold(const void* packetBytes, uint32_t packetByteCount) {
    // Fresh `0x41af70` + `0x41cf30` + `0x448cf0` + `0x448a00` tightening:
    // - original `0x41af70` is only a tiny forwarder
    // - it jumps through current margin connection vtable `+0x24`
    // - `0x41cf30` then extracts envelope `+0x08` and forwards it into vtable `+0x28`
    // - `0x448cf0` consumes that message/envelope object, may apply packet-agenda filtering, and
    //   only then reaches `0x448a00`
    // - `0x448a00` derives the final byte pointer/length from the inner message object header at
    //   `+0x0a/+0x0b`, rather than trusting already-framed caller bytes
    // Current source tightening therefore moves one step closer to the original launcher-owned
    // send bridge: state objects still own the payload bytes, but we now wrap them in a
    // source-owned envelope/message scaffold before submit instead of sending raw framed bytes
    // directly. The remaining explicit gap is the original packet-agenda / callback metadata.
    mxo::liblttcp::CMessageConnection* connection = MarginConnection();
    if (!connection) {
        connection = EnsureMarginConnectionObject();
    }
    if (!connection || !packetBytes || packetByteCount == 0u) {
        return 0u;
    }

    const mxo::liblttcp::CMessageConnectionEnvelopeScaffold envelope =
        mxo::liblttcp::CMessageConnection::BuildPayloadEnvelopeScaffold(packetBytes, packetByteCount, /*headerless=*/false);
    if (!envelope.sharedMessage) {
        return 0u;
    }

    const std::vector<uint8_t>& framedBytesFrom0a = envelope.sharedMessage->framedBytesFrom0a;
    const size_t previewOffset = framedBytesFrom0a.empty() ? 0u : (((framedBytesFrom0a[0] >> 7) == 0u) ? 1u : 0u);
    const std::string framedPreview =
        (previewOffset < framedBytesFrom0a.size())
            ? BuildHexPreview(framedBytesFrom0a.data() + previewOffset, framedBytesFrom0a.size() - previewOffset, 32u)
            : std::string();
    spdlog::info(
        "CLTLoginMediator::SendCurrentMarginPacketScaffold ForwardEnvelopeToSendPacket host='{}' state={} payloadBytes={} framedBytesFrom0a={} submitOffset={} preview={} agendaGap=packet-processing-metadata-still-missing",
        connection->RemoteHostName().empty() ? std::string("<empty>") : connection->RemoteHostName(),
        static_cast<unsigned>(connection->State()),
        static_cast<unsigned>(packetByteCount),
        static_cast<unsigned>(framedBytesFrom0a.size()),
        static_cast<unsigned>(previewOffset),
        framedPreview.empty() ? std::string("<empty>") : framedPreview);
    return connection->SendPacketEnvelopeScaffold(envelope);
}

// anchor: launcher.exe:0x41f270
void CLTLoginMediator::SetLaunchPadSourceBlock94FirstString(const char* value) {
    std::fill(authBootstrapSource38_.inlineString00.begin(), authBootstrapSource38_.inlineString00.end(), '\0');
    if (!value) {
        return;
    }

    const size_t copyCount = std::min(
        std::char_traits<char>::length(value),
        authBootstrapSource38_.inlineString00.size() - 1);
    std::copy_n(value, copyCount, authBootstrapSource38_.inlineString00.begin());
    authBootstrapSource38_.inlineString00[copyCount] = '\0';
}

// UNANCHORED: source-owned shared GameSessionID writer mirror for later session/play callback paths
void CLTLoginMediator::SetGameSessionId664(const char* value) {
    gameSessionId664_ = value ? value : "";
}

// anchor: launcher.exe:0x41f330
void CLTLoginMediator::SetSharedMarginPacketField660(uint32_t value) {
    sharedMarginPacketField660_ = value;
}

// anchor: launcher.exe:0x420d00
CLTLoginMediator::SessionCallbackHelper65cSketch* CLTLoginMediator::EnsureSessionCallbackHelper65c() {
    if (sessionCallbackHelper65c_ == nullptr) {
        sessionCallbackHelper65cState_ = SessionCallbackHelper65cSketch();
        sessionCallbackHelper65cState_.owner10 = this;
        sessionCallbackHelper65c_ = &sessionCallbackHelper65cState_;
    }
    return &sessionCallbackHelper65cState_;
}

// anchor: launcher.exe:0x420e70
uint32_t CLTLoginMediator::CommitSessionCallbackHelperGameSessionId664() {
    SessionCallbackHelper65cSketch* helper = EnsureSessionCallbackHelper65c();
    if (helper == nullptr) {
        return 0u;
    }

    if (helper->flag2D != 0) {
        Log(
            "DIAGNOSTIC: session helper GameSessionID commit deferred by helper flag2D helperString18='%s'",
            helper->string18.empty() ? "<empty>" : helper->string18.c_str());
        return 0u;
    }

    SetGameSessionId664(helper->string18.c_str());
    helper->field24 = 0;

    Log(
        "DIAGNOSTIC: committed helper GameSessionID owner660=0x%08x GameSessionID='%s'",
        (unsigned)sharedMarginPacketField660_,
        gameSessionId664_.empty() ? "<empty>" : gameSessionId664_.c_str());
    return 1u;
}

// source-owned shared helper for `CLTLoginState_State18` slot 3 / `0x421a50`
uint32_t CLTLoginMediator::RefreshSessionHelperGameSessionId664FromSourceBlock94() {
    // Current best source-owned mirror of the state18/session helper path:
    // - ensure owner helper `+0x65c`
    // - refresh helper string `+0x18` from owner `+0x94 + 0x60`
    // - then commit that helper string into owner `+0x664`
    SessionCallbackHelper65cSketch* helper = EnsureSessionCallbackHelper65c();
    if (helper == nullptr) {
        return 0u;
    }

    if (authBootstrapSource38_.string60.begin != nullptr && authBootstrapSource38_.string60.begin[0] != '\0') {
        helper->string18 = authBootstrapSource38_.string60.begin;
    }

    return CommitSessionCallbackHelperGameSessionId664();
}

// anchor: launcher.exe:0x41b260
const char* CLTLoginMediator::GetRouteHostPrefixBySlot(uint8_t slotIndex) const {
    if (slotIndex >= routeHostStrings818_.size()) {
        return nullptr;
    }
    const RouteHostStringTripleState& slot = routeHostStrings818_[slotIndex];
    return slot.text.empty() ? nullptr : slot.text.c_str();
}

// anchor: launcher.exe:0x41b2a0
uint8_t CLTLoginMediator::GetSlotRecordStatusByIndex(uint8_t slotIndex) const {
    const SlotRecordState004b5328* record = GetSlotRecordByIndex(slotIndex);
    return record ? record->status0b : 7u;
}

// anchor: launcher.exe:0x41b2e0
const char* CLTLoginMediator::GetDescriptorInlineNameByIndex(uint8_t slotIndex) const {
    if (slotIndex >= worldDescriptorValidD84_.size() || !worldDescriptorValidD84_[slotIndex]) {
        return nullptr;
    }
    const WorldDescriptorState004b533c& slot = worldDescriptorsD84_[slotIndex];
    return slot.inlineNamePlus03.empty() ? nullptr : slot.inlineNamePlus03.c_str();
}

// anchor: launcher.exe:0x41b320
uint8_t CLTLoginMediator::GetDescriptorField18ByIndex(uint8_t slotIndex) const {
    if (slotIndex >= worldDescriptorValidD84_.size() || !worldDescriptorValidD84_[slotIndex]) {
        return 0;
    }
    return worldDescriptorsD84_[slotIndex].type18;
}

// anchor: launcher.exe:0x41b360
uint8_t CLTLoginMediator::GetDescriptorField19ByIndex(uint8_t slotIndex) const {
    if (slotIndex >= worldDescriptorValidD84_.size() || !worldDescriptorValidD84_[slotIndex]) {
        return 0;
    }
    return static_cast<uint8_t>(worldDescriptorsD84_[slotIndex].serverVersion19 & 0xffu);
}

// anchor: launcher.exe:0x41b3a0
uint8_t CLTLoginMediator::GetDescriptorLowNibble1fByIndex(uint8_t slotIndex) const {
    if (slotIndex >= worldDescriptorValidD84_.size() || !worldDescriptorValidD84_[slotIndex]) {
        return 0;
    }
    const uint8_t value = worldDescriptorsD84_[slotIndex].populationLevel1f & 0x0f;
    return (value >= 1u && value <= 3u) ? value : 0u;
}

const char* CLTLoginMediator::ResolveMarginRouteFromCurrentCharacterSlot() const {
    // Address anchors:
    // - launcher.exe:0x439300 case `7/8/0xd`
    // - owner byte `+0xcc8`
    // - owner vtable `+0xe0`
    //
    // Current best source-owned mirror returns the reconstructed route-host string triple's first
    // string for the current slot/index.
    return GetRouteHostPrefixBySlot(postAuthMarginLoadingState_.characterRouteIndexCc8);
}

const char* CLTLoginMediator::ResolveMarginRouteFromDescriptorIndex(uint32_t descriptorIndex) const {
    // Address anchors:
    // - launcher.exe:0x439300 case `10`
    // - owner dword `+0x12c`
    // - owner vtable `+0xfc`
    //
    // Fresh tightening from `0x41c3c0` + `0x4401a0`:
    // - `0x41c3c0` bounds-checks input `+0x24` against owner vtable `+0xf8`
    // - later `0x4401a0` indexes owner `+0xd84` using owner `+0x12c`
    // - current best read is therefore that the state4 case-10 branch forwards a
    //   world-descriptor index/selector here, not a direct world-id payload
    if (descriptorIndex >= worldDescriptorCountD80_) {
        return nullptr;
    }
    return GetDescriptorInlineNameByIndex(static_cast<uint8_t>(descriptorIndex));
}

const char* CLTLoginMediator::ResolveMarginRouteFromWorldId(uint32_t worldId) const {
    // Address anchors:
    // - launcher.exe:0x439300 default branch
    // - owner vtable `+0xfc`
    //
    // Keep this narrower fallback helper for places where source still only has a recovered
    // world-id value and must rejoin it against the descriptor table.
    if (worldId > 0xffffu) {
        return nullptr;
    }

    const int descriptorIndex = FindRecoveredWorldDescriptorIndexByWorldId(static_cast<uint16_t>(worldId));
    if (descriptorIndex < 0) {
        return nullptr;
    }
    return GetDescriptorInlineNameByIndex(static_cast<uint8_t>(descriptorIndex));
}

const char* CLTLoginMediator::ResolveMarginRouteDescriptor() const {
    // Address anchors:
    // - launcher.exe:0x439300 case `6`
    // - owner vtable `+0x10c`
    //
    // Current best source-owned mirror of that branch:
    // - the original fetches an object through `+0x10c`
    // - then uses the first dword of that object as the string argument into `0x41e500`
    // - current scaffold keeps this narrow by preferring the already mirrored current-slot
    //   route-host string and only falling back to older diagnostic route text when needed
    if (const char* currentSlotRouteHost = GetRouteHostPrefixBySlot(postAuthMarginLoadingState_.characterRouteIndexCc8)) {
        return currentSlotRouteHost;
    }
    return marginRouteState_.routeHostPrefix.empty() ? nullptr : marginRouteState_.routeHostPrefix.c_str();
}

// anchor: launcher.exe:0x41e500
uint32_t CLTLoginMediator::BeginMarginConnectionScaffold(const char* routeHostText, uint8_t cachedRouteSelector) {
    // Narrow transport/init helper intentionally kept separate from the state4 `0x439300`
    // case split.
    //
    // Current best recovered `0x41e500` shape:
    // - allocate/configure a margin-specific connection object at owner `+0x1c`
    // - clear owner byte `+0x2d`
    // - if arg2 == 0, refresh owner `+0x30`, rebuild owner `+0x3c`, select owner `+0x7c`,
    //   and materialize owner `+0x6c`
    // - increment owner dword `+0x24`
    // - clear owner `+0x7c`
    // - call `connection->+0x1c(owner+0x6c)`
    mxo::liblttcp::CMessageConnection* connection = EnsureMarginConnectionObject();
    if (!connection) {
        spdlog::warn("CLTLoginMediator::BeginMarginConnectionScaffold failed to allocate margin connection");
        return 0u;
    }

    marginConnectionFlag2d_ = 0;

    const bool shouldRefreshRouteState = (cachedRouteSelector == 0u) ||
                                         (marginAddressList3c_.ipv4NetworkOrderList.empty()) ||
                                         (marginEndpoint_.ipv4NetworkOrder == 0u);
    if (shouldRefreshRouteState) {
        if (routeHostText && routeHostText[0] != '\0') {
            marginRouteState_.routeHostPrefix = routeHostText;
        }

        const std::string marginHost = ResolvedMarginHostName();
        if (marginHost.empty()) {
            spdlog::debug("CLTLoginMediator::BeginMarginConnectionScaffold has no resolved margin host");
            return 0u;
        }

        const bool routeChanged = (marginAddressList3c_.resolvedHostName != marginHost);
        if (routeChanged || marginAddressList3c_.ipv4NetworkOrderList.empty()) {
            if (!RebuildMarginAddressList()) {
                return 0u;
            }
        }

        if (marginSelectedIpv4_7c_ == 0u && !SelectMarginEndpointIpv4()) {
            spdlog::warn(
                "CLTLoginMediator::BeginMarginConnectionScaffold found no IPv4 candidates for '{}'",
                marginHost);
            return 0u;
        }

        BuildMarginEndpoint();
    }

    ++marginBeginCount24_;
    marginSelectedIpv4_7c_ = 0u;

    const std::string marginHost = ResolvedMarginHostName();
    if (!marginHost.empty()) {
        connection->SetRemoteHostName(marginHost.c_str());
    }
    connection->SetRemoteEndpoint(marginEndpoint_);

    const uint32_t result = connection->EnsureConnected();
    spdlog::info(
        "CLTLoginMediator::BeginMarginConnectionScaffold resolvedHost='{}' routeHostText='{}' selector=0x{:02x} beginCount={} selectedIpv4=0x{:08x} port={} ensureConnectedResult=0x{:08x}",
        marginHost.empty() ? std::string("<empty>") : marginHost,
        (routeHostText && routeHostText[0]) ? std::string(routeHostText) : std::string("<empty>"),
        static_cast<unsigned>(cachedRouteSelector),
        static_cast<unsigned>(marginBeginCount24_),
        static_cast<unsigned>(marginEndpoint_.ipv4NetworkOrder),
        static_cast<unsigned>(marginServerPortHostOrder_),
        static_cast<unsigned>(result));
    if (result == 0u) {
        spdlog::debug(
            "CLTLoginMediator::BeginMarginConnectionScaffold connect failed host='{}' port={} ip=0x{:08x} selector={} beginCount={}",
            marginHost.empty() ? std::string("<empty>") : marginHost,
            static_cast<unsigned>(marginServerPortHostOrder_),
            static_cast<unsigned>(marginEndpoint_.ipv4NetworkOrder),
            static_cast<unsigned>(cachedRouteSelector),
            static_cast<unsigned>(marginBeginCount24_));
    }
    return result;
}

// =============================================================================
// ARG7 SELECTION RESOLUTION (ILTLoginMediator sibling object at 0x4d3584)
// =============================================================================
// Address anchors from Ghidra analysis:
// - launcher.exe:0x40d6f0 = ILTLoginMediator_ResolveSelectionFromListCtrl (vtable access)
// - launcher.exe:0x40e480 = ILTLoginMediator_BuildWorldList (world list construction)
// - launcher.exe:0x40cd10 = ILTLoginMediator_GetWorldNameByIndex (fallback path)
// - launcher.exe:0x40cd60 = ILTLoginMediator_GetWorldNameByIndex_Fallback
// - launcher.exe:0x40e5b0 = ILTLoginMediator_GetWorldListCount
// - launcher.exe:0x40e560 = ILTLoginMediator_GetWorldListCount_Active
// - launcher.exe:0x40e670 = ILTLoginMediator_GetAvailableWorlds
// - launcher.exe:0x40e6c0 = ILTLoginMediator_GetAvailableWorldName
//
// VTABLE METHODS (at offset +0xc from object pointer):
// +0xfc = GetWorldNameByIndex(index) -> char* world name string
// +0x100 = GetWorldVariantByIndex(index) -> uint variant state (1,2,3,5)
// +0xe4 = ValidateWorldSelection(variant) -> 0 or 7 on valid
// +0xf8 = GetWorldListCount() -> uint total count
// +0xd8 = GetActiveWorldCount() -> uint active count
// +0xe0 = GetAvailableWorlds(index) -> bool (fallback path check)
// +0xdc = GetAvailableWorldName(index) -> char* (fallback path)
//
// ARG7 PACKING FORMAT:
// g_PackedArg7Selection = (high8bits << 24) | low24bits
//   high8bits = variant state from vtable[+0x100]
//   low24bits = world index from GetItemData low bits
// =============================================================================

// =============================================================================
// ARG6: ILTLoginMediator.Default at 0x4d2c58 - World List Provider
// =============================================================================
// Address anchors from Ghidra analysis:
// - launcher.exe:0x4d2c58 = ILTLoginMediator_Default (object pointer)
// - launcher.exe:0x4d3584 = ILTLoginMediator_SiblingObject (vtable root at +0xc)
// - launcher.exe:0x40d6f0 = ILTLoginMediator_ResolveSelectionFromListCtrl
// - launcher.exe:0x40e480 = ILTLoginMediator_BuildWorldList
// - launcher.exe:0x40cd10 = ILTLoginMediator_GetWorldNameByIndex (fallback)
// - launcher.exe:0x40cd60 = ILTLoginMediator_GetWorldNameByIndex_Fallback
// - launcher.exe:0x40e5b0 = ILTLoginMediator_GetWorldListCount
// - launcher.exe:0x40e560 = ILTLoginMediator_GetWorldListCount_Active
// - launcher.exe:0x40e670 = ILTLoginMediator_GetAvailableWorlds
// - launcher.exe:0x40e6c0 = ILTLoginMediator_GetAvailableWorldName
//
// VTABLE METHODS (at offset +0xc from object pointer at 0x4d3584):
//   +0xfc = GetWorldNameByIndex(index) -> char* world name string
//   +0x100 = GetWorldVariantByIndex(index) -> uint variant state (1,2,3,5)
//   +0xe4 = ValidateWorldSelection(variant) -> 0 or 7 on valid
//   +0xf8 = GetWorldListCount() -> uint total count
//   +0xd8 = GetActiveWorldCount() -> uint active count
//   +0xe0 = GetAvailableWorlds(index) -> bool (fallback path check)
//   +0xdc = GetAvailableWorldName(index) -> char* (fallback path)
// =============================================================================

// Address anchor: launcher.exe:0x4d2c58 = ILTLoginMediator_Default object (arg6)
void* CLTLoginMediator::WorldSlot(uint32_t index) const {
    // Faithful implementation should call:
    // - launcher.exe:0x40e670 = ILTLoginMediator_GetAvailableWorlds(index)
    // - launcher.exe:0x40cd10 = ILTLoginMediator_GetWorldNameByIndex (fallback)

    // Transitional stub preserves the slot structure for later completion.
    return worldSlots_[index];
}

// Address anchor: launcher.exe:0x4d2c58 = ILTLoginMediator_Default object (arg6)
void* CLTLoginMediator::WorldPayloadSlot(uint32_t index) const {
    // Faithful implementation should call:
    // - launcher.exe:0x40e6c0 = ILTLoginMediator_GetAvailableWorldName(index)
    // - launcher.exe:0x40d6f0 = ILTLoginMediator_ResolveSelectionFromListCtrl

    // Transitional stub preserves the payload structure for later completion.
    return worldPayloadSlots_[index];
}

// =============================================================================
// ARG6 FAITHFUL INITIALIZATION: ILTLoginMediator.Default at 0x4d2c58
// =============================================================================
// Address anchors from Ghidra analysis:
// - launcher.exe:0x4d2c58 = ILTLoginMediator_Default (object pointer)
// - launcher.exe:0x4d3584 = ILTLoginMediator_SiblingObject (vtable root at +0xc)
// - launcher.exe:0x40d6f0 = ILTLoginMediator_ResolveSelectionFromListCtrl
// - launcher.exe:0x40e480 = ILTLoginMediator_BuildWorldList
// - launcher.exe:0x40cd10 = ILTLoginMediator_GetWorldNameByIndex (fallback)
// - launcher.exe:0x40cd60 = ILTLoginMediator_GetWorldNameByIndex_Fallback
// - launcher.exe:0x40e5b0 = ILTLoginMediator_GetWorldListCount
// - launcher.exe:0x40e560 = ILTLoginMediator_GetWorldListCount_Active
// - launcher.exe:0x40e670 = ILTLoginMediator_GetAvailableWorlds
// - launcher.exe:0x40e6c0 = ILTLoginMediator_GetAvailableWorldName
// =============================================================================

// anchor: launcher.exe:0x40e480
// sibling slot/vtable family: launcher.exe:0x4d3584
void CLTLoginMediator::InitializeArg6DefaultObject() {
    arg6WorldList_.worldNames_ = {
        "Default", "Starter", "Classic", "Advanced", "Extreme"
    };
    arg6WorldList_.worldVariants_ = {1, 2, 3, 5, 1};
    arg6WorldList_.worldValid_ = {true, true, true, true, true, false, false, false, false, false};
    arg6WorldList_.available_ = {true, true, true, true, true, false, false, false, false, false};
    arg6WorldList_.totalCount_ = 5;
    arg6WorldList_.activeCount_ = 5;

    arg6Selection_ = Arg6SelectionConfig();

    Log(
        "DIAGNOSTIC: InitializeArg6DefaultObject populated arg6 defaults worlds=%u active=%u selectedWorld=0x%06x selectedVariant=0x%02x",
        (unsigned)arg6WorldList_.totalCount_,
        (unsigned)arg6WorldList_.activeCount_,
        (unsigned)arg6Selection_.selectedWorldIndexLow24_,
        (unsigned)arg6Selection_.selectedVariantIndexHigh8_);
}

void CLTLoginMediator::ConfigureArg6Selection(
    uint32_t worldUpperBoundExclusive,
    uint32_t variantUpperBoundExclusive,
    const char* mappedSelectionName,
    const char* mappedVariantName,
    uint32_t selectedWorldIndexLow24,
    uint32_t selectedVariantIndexHigh8,
    uint32_t selectedWorldType,
    uint32_t selectedVariantState) {
    arg6Selection_.worldUpperBoundExclusive_ = worldUpperBoundExclusive ? worldUpperBoundExclusive : 1u;
    arg6Selection_.variantUpperBoundExclusive_ = variantUpperBoundExclusive ? variantUpperBoundExclusive : 1u;
    arg6Selection_.selectedWorldIndexLow24_ = selectedWorldIndexLow24 & 0x00ffffffu;
    arg6Selection_.selectedVariantIndexHigh8_ = selectedVariantIndexHigh8 & 0xffu;
    arg6Selection_.selectedWorldType_ = selectedWorldType;
    arg6Selection_.selectedVariantState_ = selectedVariantState;
    arg6Selection_.mappedSelectionId_ = arg6Selection_.selectedWorldIndexLow24_;
    arg6Selection_.mappedSelectionName_ =
        (mappedSelectionName && mappedSelectionName[0]) ? mappedSelectionName : "standalone";
    arg6Selection_.mappedVariantName_ =
        (mappedVariantName && mappedVariantName[0]) ? mappedVariantName : arg6Selection_.mappedSelectionName_;
}

void CLTLoginMediator::SetArg6ProfileName(const char* profileName) {
    arg6Selection_.profileName_ = (profileName && profileName[0]) ? profileName : "resurrections";
}

void CLTLoginMediator::SetArg6AuthName(const char* authName) {
    arg6Selection_.authName_ = (authName && authName[0]) ? authName : arg6Selection_.profileName_;
}

void CLTLoginMediator::SetArg6AuthPassword(const char* authPassword) {
    arg6Selection_.authPassword_ = authPassword ? authPassword : "";
}

uint32_t CLTLoginMediator::Arg6WorldUpperBoundExclusive() const {
    return arg6Selection_.worldUpperBoundExclusive_;
}

uint32_t CLTLoginMediator::Arg6VariantUpperBoundExclusive() const {
    return arg6Selection_.variantUpperBoundExclusive_;
}

uint32_t CLTLoginMediator::Arg6SelectedWorldIndexLow24() const {
    return arg6Selection_.selectedWorldIndexLow24_;
}

uint32_t CLTLoginMediator::Arg6SelectedVariantIndexHigh8() const {
    return arg6Selection_.selectedVariantIndexHigh8_;
}

uint32_t CLTLoginMediator::Arg6SelectedWorldType() const {
    return arg6Selection_.selectedWorldType_;
}

uint32_t CLTLoginMediator::Arg6SelectedVariantState() const {
    return arg6Selection_.selectedVariantState_;
}

uint32_t CLTLoginMediator::Arg6MappedSelectionId() const {
    return arg6Selection_.mappedSelectionId_;
}

const char* CLTLoginMediator::Arg6MappedSelectionName() const {
    return arg6Selection_.mappedSelectionName_.c_str();
}

const char* CLTLoginMediator::Arg6MappedVariantName() const {
    return arg6Selection_.mappedVariantName_.c_str();
}

const char* CLTLoginMediator::Arg6ProfileName() const {
    return arg6Selection_.profileName_.c_str();
}

const char* CLTLoginMediator::Arg6AuthName() const {
    return arg6Selection_.authName_.c_str();
}

const char* CLTLoginMediator::Arg6AuthPassword() const {
    return arg6Selection_.authPassword_.c_str();
}

bool CLTLoginMediator::Arg6WorldIndexMatchesSelection(uint32_t worldIndex) const {
    return worldIndex == arg6Selection_.selectedWorldIndexLow24_;
}

bool CLTLoginMediator::Arg6VariantIndexMatchesSelection(uint32_t variantIndex) const {
    return variantIndex == arg6Selection_.selectedVariantIndexHigh8_;
}

uint32_t CLTLoginMediator::Arg6ExpectedSelectionDescriptorScratchRequest() const {
    const uint32_t variantHigh8 = (arg6Selection_.selectedVariantIndexHigh8_ & 0xffu) << 24;
    const uint32_t preservedMiddle16 = arg6Selection_.selectedWorldIndexLow24_ & 0x00ffff00u;
    const uint32_t lowByteOverwrittenWithVariant = arg6Selection_.selectedVariantIndexHigh8_ & 0xffu;
    return variantHigh8 | preservedMiddle16 | lowByteOverwrittenWithVariant;
}

bool CLTLoginMediator::Arg6SelectionDescriptorMatchesRequest(uint32_t selectionIndex) const {
    const uint32_t normalizedSelectionIndex = selectionIndex & 0xffffffffu;
    if ((normalizedSelectionIndex & 0x00ffffffu) == arg6Selection_.selectedWorldIndexLow24_) {
        return true;
    }
    return normalizedSelectionIndex == Arg6ExpectedSelectionDescriptorScratchRequest();
}

// =============================================================================
// ARG6 VTABLE METHODS (at offset +0xc from object pointer at 0x4d3584)
// =============================================================================

// anchor: launcher.exe:0x40cd10
// vtable: launcher.exe:0x4d3584 +0xfc
const char* CLTLoginMediator::Arg6GetWorldNameByIndex(uint32_t index) {
    if (index < Arg6WorldUpperBoundExclusive() && Arg6WorldIndexMatchesSelection(index)) {
        return Arg6MappedSelectionName();
    }
    return (index < arg6WorldList_.totalCount_) ? arg6WorldList_.worldNames_[index].c_str() : nullptr;
}

// anchor: launcher.exe:0x4d3584 +0x100
// vtable: launcher.exe:0x4d3584 +0x100
uint8_t CLTLoginMediator::Arg6GetWorldVariantByIndex(uint32_t index) {
    if (index < Arg6WorldUpperBoundExclusive() && Arg6WorldIndexMatchesSelection(index)) {
        return static_cast<uint8_t>(Arg6SelectedWorldType());
    }
    return (index < arg6WorldList_.totalCount_) ? arg6WorldList_.worldVariants_[index] : 0u;
}

// anchor: launcher.exe:0x4d3584 +0xe4
// vtable: launcher.exe:0x4d3584 +0xe4
uint8_t CLTLoginMediator::Arg6ValidateWorldSelection(uint8_t variant) {
    if (variant < Arg6VariantUpperBoundExclusive() && Arg6VariantIndexMatchesSelection(variant)) {
        return static_cast<uint8_t>(Arg6SelectedVariantState());
    }
    return 3u;
}

// anchor: launcher.exe:0x40e5b0
// vtable: launcher.exe:0x4d3584 +0xf8
uint32_t CLTLoginMediator::Arg6GetWorldListCount() const {
    return Arg6WorldUpperBoundExclusive();
}

// anchor: launcher.exe:0x40e560
// vtable: launcher.exe:0x4d3584 +0xd8
uint32_t CLTLoginMediator::Arg6GetActiveWorldListCount() const {
    return Arg6VariantUpperBoundExclusive();
}

// anchor: launcher.exe:0x40e670
// vtable: launcher.exe:0x4d3584 +0xe0
bool CLTLoginMediator::Arg6GetAvailableWorlds(uint32_t index) const {
    if (index < Arg6VariantUpperBoundExclusive() && Arg6VariantIndexMatchesSelection(index)) {
        return true;
    }
    return (index < arg6WorldList_.available_.size()) ? arg6WorldList_.available_[index] : false;
}

// anchor: launcher.exe:0x40cd60
// vtable: launcher.exe:0x4d3584 +0xdc
const char* CLTLoginMediator::Arg6GetAvailableWorldName(uint32_t index) {
    if (index < Arg6VariantUpperBoundExclusive() && Arg6VariantIndexMatchesSelection(index)) {
        return Arg6MappedVariantName();
    }
    return (index < arg6WorldList_.totalCount_) ? arg6WorldList_.worldNames_[index].c_str() : nullptr;
}

uint32_t CLTLoginMediator::SendAuthFramedPacket(
    const mxo::auth::FramedPacket& packet,
    const char* stepLabel) {
    mxo::liblttcp::CMessageConnection* connection = AuthConnection();
    if (!connection) {
        connection = EnsureAuthConnectionObject();
    }
    if (!connection || packet.bytes.empty()) {
        return 0;
    }

    const uint8_t rawCode = packet.payloadBytes.empty() ? 0u : packet.payloadBytes[0];
    const uint32_t sendResult = connection->SendPacket(
        packet.bytes.data(),
        static_cast<uint32_t>(packet.bytes.size()),
        nullptr);
    Log(
        "DIAGNOSTIC: launcher-owned auth send step='%s' rawCode=0x%02x message='%s' headerLen=%u payloadLen=%u byteCount=%u -> sendResult=0x%08x",
        (stepLabel && stepLabel[0]) ? stepLabel : "<unnamed>",
        (unsigned)rawCode,
        mxo::auth::AuthOpcodeName(rawCode),
        (unsigned)packet.headerBytes.size(),
        (unsigned)packet.payloadBytes.size(),
        (unsigned)packet.bytes.size(),
        (unsigned)sendResult);
    return sendResult;
}

uint32_t CLTLoginMediator::SendAuthGetPublicKeyRequest() {
    // Address anchors:
    // - launcher.exe:0x447eb0 = strongest current raw 0x06 send builder
    // - launcher.exe:0x448050 = upstream phase-2 bootstrap dispatcher
    mxo::auth::FramedPacket packet;
    if (!mxo::auth::BuildGetPublicKeyRequestPacket(
            authLauncherVersion_,
            authCurrentPublicKeyId_,
            mxo::auth::kFrameModeAuto,
            &packet)) {
        Log("DIAGNOSTIC: launcher-owned auth failed to build AS_GetPublicKeyRequest");
        return 0;
    }

    const uint32_t sendResult = SendAuthFramedPacket(packet, kMessageAsGetPublicKeyRequest);
    authGetPublicKeyRequestSent_ = (sendResult != 0u);
    return sendResult;
}

uint32_t CLTLoginMediator::SendAuthRequestFromReply(const mxo::auth::GetPublicKeyReply& reply) {
    // Address anchors:
    // - launcher.exe:0x4474f0 = strongest current raw 0x08 / AS_AuthRequest send builder
    // - launcher.exe:0x448050 = branch site selecting raw 0x06 vs raw 0x08 path
    // - launcher.exe:0x439210 = upstream BeginAuthBootstrap call site
    if (authUsername_.empty()) {
        Log("DIAGNOSTIC: launcher-owned auth cannot build AS_AuthRequest without a username");
        return 0;
    }
    if (!reply.hasEmbeddedPublicKey) {
        Log("DIAGNOSTIC: launcher-owned auth GetPublicKeyReply has no embedded public key material");
        return 0;
    }

    mxo::auth::AuthBlobLayout blobLayout;
    blobLayout.embeddedTime = static_cast<uint32_t>(std::time(nullptr));

    mxo::auth::AuthRequestLayout requestLayout;
    requestLayout.publicKeyId = reply.publicKeyId;
    requestLayout.loginType = authLoginType_;
    requestLayout.keyConfigMd5 = authKeyConfigMd5_;
    requestLayout.uiConfigMd5 = authUiConfigMd5_;
    requestLayout.rsaModulusBytes = reply.modulusBytes;
    requestLayout.rsaExponentBytes.assign(1u, reply.publicExponentByte);

    mxo::auth::AuthRequestBuildResult buildResult;
    if (!mxo::auth::BuildAuthRequestPacket(
            authUsername_,
            blobLayout,
            requestLayout,
            mxo::auth::kFrameModeAuto,
            &buildResult)) {
        Log("DIAGNOSTIC: launcher-owned auth failed to build AS_AuthRequest");
        return 0;
    }

    lastAuthRequestBuildResult_ = buildResult;
    const uint32_t sendResult = SendAuthFramedPacket(buildResult.packet, kMessageAsAuthRequest);
    authRequestSent_ = (sendResult != 0u);
    if (sendResult != 0u) {
        Log(
            "DIAGNOSTIC: launcher-owned auth built AS_AuthRequest publicKeyId=%u loginType=%u keySize=%u blobLen=%u usernameLengthField=%u usedReplyPublicKey=%u keyConfigMd5Len=%u uiConfigMd5Len=%u",
            (unsigned)reply.publicKeyId,
            (unsigned)authLoginType_,
            (unsigned)reply.keySize,
            (unsigned)buildResult.blobCiphertextBytes.size(),
            (unsigned)buildResult.usernameLengthField,
            buildResult.usedProvidedPublicKey ? 1u : 0u,
            (unsigned)buildResult.keyConfigMd5Bytes.size(),
            (unsigned)buildResult.uiConfigMd5Bytes.size());
    }
    return sendResult;
}

uint32_t CLTLoginMediator::SendAuthChallengeResponse(const mxo::auth::AuthChallenge& challenge) {
    // Address anchors:
    // - launcher.exe:0x429b0 = later challenge/material continuation anchor
    // - exact original raw 0x0a builder/send VA: [not yet isolated]
    // - launcher.exe:0x439210 = upstream BeginAuthBootstrap call site
    if (authPassword_.empty()) {
        Log("DIAGNOSTIC: launcher-owned auth received AS_AuthChallenge but has no password to send in AS_AuthChallengeResponse");
        return 0;
    }
    if (lastAuthRequestBuildResult_.twofishKeyBytes.size() != 16u) {
        Log("DIAGNOSTIC: launcher-owned auth missing Twofish key from AS_AuthRequest build result");
        return 0;
    }

    mxo::auth::AuthChallengeResponseLayout layout;
    mxo::auth::AuthChallengeResponseBuildResult buildResult;
    if (!mxo::auth::BuildAuthChallengeResponsePacket(
            challenge.encryptedChallengeBytes,
            lastAuthRequestBuildResult_.twofishKeyBytes,
            authPassword_,
            authPassword_,
            layout,
            mxo::auth::kFrameModeAuto,
            &buildResult)) {
        Log("DIAGNOSTIC: launcher-owned auth failed to build AS_AuthChallengeResponse");
        return 0;
    }

    const uint32_t sendResult = SendAuthFramedPacket(buildResult.packet, "AS_AuthChallengeResponse");
    authChallengeResponseSent_ = (sendResult != 0u);
    if (sendResult != 0u) {
        Log(
            "DIAGNOSTIC: launcher-owned auth built AS_AuthChallengeResponse passwordLengthField=%u soePasswordLengthField=%u plaintextLen=%u ciphertextLen=%u",
            (unsigned)buildResult.passwordLengthField,
            (unsigned)buildResult.soePasswordLengthField,
            (unsigned)buildResult.plaintextBytes.size(),
            (unsigned)buildResult.ciphertextBytes.size());
    }
    return sendResult;
}

void CLTLoginMediator::LogParsedAuthReply(const mxo::auth::AuthReply& reply) const {
    // Address anchors:
    // - launcher.exe:0x4401a0 = strongest current HandleAuthReply implementation
    // - launcher.exe:0x43a330 = concrete auth-reply parser object helper
    // - launcher.exe:0x43b830 = later auth-side GetWorldList sender (upstream after success)
    // - launcher.exe:0x439210 = upstream BeginAuthBootstrap call site
    if (reply.isErrorReply) {
        Log(
            "DIAGNOSTIC: launcher-owned auth parsed AS_AuthReply error errorCode=0x%08x zeroDword=0x%08x trailingWord=0x%04x",
            (unsigned)reply.errorCode,
            (unsigned)reply.zeroDword,
            (unsigned)reply.trailingWord);
        return;
    }

    Log(
        "DIAGNOSTIC: launcher-owned auth parsed AS_AuthReply success characterCount=%u worldCount=%u username='%s' authDataMarker=0x%04x signatureLen=%u encryptedPrivateExponentLen=%u",
        (unsigned)reply.characterCount,
        (unsigned)reply.worldCount,
        reply.username.text.empty() ? "<empty>" : reply.username.text.c_str(),
        (unsigned)reply.authDataMarker,
        (unsigned)reply.authSignatureBytes.size(),
        (unsigned)reply.encryptedPrivateExponentLength);

    for (size_t i = 0; i < reply.characters.size(); ++i) {
        const mxo::auth::AuthCharacterEntry& entry = reply.characters[i];
        Log(
            "DIAGNOSTIC: launcher-owned auth character[%u] handle='%s' characterId=%llu status=%u worldId=%u",
            (unsigned)i,
            entry.handle.text.empty() ? "<empty>" : entry.handle.text.c_str(),
            static_cast<unsigned long long>(entry.characterId),
            (unsigned)entry.status,
            (unsigned)entry.worldId);
    }

    for (size_t i = 0; i < reply.worlds.size(); ++i) {
        const mxo::auth::AuthWorldEntry& world = reply.worlds[i];
        Log(
            "DIAGNOSTIC: launcher-owned auth world[%u] id=%u name='%s' status=%u type=%u clientVersion=%u load='%c'",
            (unsigned)i,
            (unsigned)world.worldId,
            world.worldName.empty() ? "<empty>" : world.worldName.c_str(),
            (unsigned)world.status,
            (unsigned)world.type,
            (unsigned)world.clientVersion,
            world.load ? static_cast<char>(world.load) : '?');
    }

    std::vector<uint8_t> decryptedPrivateExponentBytes;
    if (mxo::auth::DecryptAuthReplyPrivateExponent(
            reply,
            lastAuthRequestBuildResult_.twofishKeyBytes,
            lastAuthChallenge_.encryptedChallengeBytes,
            &decryptedPrivateExponentBytes)) {
        Log(
            "DIAGNOSTIC: launcher-owned auth decrypted AS_AuthReply private exponent length=%u",
            (unsigned)decryptedPrivateExponentBytes.size());
    }
}

void CLTLoginMediator::SyncRecoveredAuthBootstrapFixedFieldsFromCurrentConfig() {
    authBootstrap680_.loginType28 = authLoginType_;
    authBootstrap680_.launcherVersion2C = authLauncherVersion_;
    authBootstrap680_.block30 = CopyPrefix16(authKeyConfigMd5_);
    authBootstrap680_.block40 = CopyPrefix16(authUiConfigMd5_);
    authBootstrap680_.currentPublicKeyId9C = authCurrentPublicKeyId_;
}

// UNANCHORED: source-owned table mirror fed from parsed auth reply worlds
void CLTLoginMediator::SeedRecoveredWorldDescriptorFromAuthReply(uint8_t worldIndex, const mxo::auth::AuthWorldEntry& world) {
    if (worldIndex >= worldDescriptorsD84_.size()) {
        return;
    }

    WorldDescriptorState004b533c& descriptor = worldDescriptorsD84_[worldIndex];
    descriptor.worldId01 = world.worldId;
    descriptor.inlineNamePlus03 = world.worldName;
    descriptor.status17 = static_cast<uint8_t>(world.status & 0xffu);
    descriptor.type18 = static_cast<uint8_t>(world.type & 0xffu);
    descriptor.serverVersion19 = world.clientVersion;
    descriptor.serverLanguage1d = static_cast<uint8_t>(world.unknown4 & 0xffu);
    descriptor.privateFlag1e = static_cast<uint8_t>((world.unknown4 >> 8) & 0xffu);
    descriptor.populationLevel1f = world.load;
    worldDescriptorValidD84_[worldIndex] = true;
}

// UNANCHORED: source-owned table mirror fed from parsed auth reply characters
void CLTLoginMediator::SeedRecoveredCharacterSlotRecordFromAuthReply(
    uint8_t characterIndex,
    const mxo::auth::AuthCharacterEntry& character) {
    if (characterIndex >= slotRecords688_.size()) {
        return;
    }

    SlotRecordState004b5328& slotRecord = slotRecords688_[characterIndex];
    slotRecord.heapString14 = character.handle.text;
    slotRecord.globalCharacterIdLow03 = static_cast<uint32_t>(character.characterId & 0xffffffffull);
    slotRecord.globalCharacterIdHigh07 = static_cast<uint32_t>((character.characterId >> 32) & 0xffffffffull);
    slotRecord.status0b = static_cast<uint8_t>(character.status & 0xffu);
    slotRecord.worldId0c = character.worldId;
    slotRecordValid688_[characterIndex] = true;
}

// UNANCHORED: source-owned lookup helper over the mirrored world-descriptor table
int CLTLoginMediator::FindRecoveredWorldDescriptorIndexByWorldId(uint16_t worldId) const {
    for (size_t i = 0; i < worldDescriptorsD84_.size(); ++i) {
        if (worldDescriptorValidD84_[i] && worldDescriptorsD84_[i].worldId01 == worldId) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// UNANCHORED: source-owned synthesis helper for active-branch scaffolding only
void CLTLoginMediator::SeedHelper11SourceBlockFromRecoveredPostAuthStateIfUnset() {
    // Transitional/source-owned synthesis only:
    // - newer packet debug printers now make one important negative result explicit:
    //   `+0x178/+0x198/+0x1b8` are not generic route/world strings but
    //   `RealFirstName/RealLastName/Background`
    // - so do **not** synthesize those fields from reconstructed route/world tables
    // - the only safe active-path seed we currently keep here is the current-slot character name
    //   and the paired selector/index dword at `+0x12c`
    //
    // Fresh tightening from `0x41c3c0` + `0x4401a0`:
    // - owner `+0x12c` should not be backfilled from slot-record `worldId0c`
    // - the active branch uses `+0x12c` as a world-descriptor index/selector
    const SlotRecordState004b5328* currentSlotRecord = GetCurrentSlotRecord();
    if (currentSlotRecord != nullptr) {
        if (postAuthMarginLoadingState_.sourceLeadString108[0] == '\0' && !currentSlotRecord->heapString14.empty()) {
            const size_t copyCount = std::min(
                currentSlotRecord->heapString14.size(),
                postAuthMarginLoadingState_.sourceLeadString108.size() - 1);
            std::copy_n(
                currentSlotRecord->heapString14.data(),
                copyCount,
                postAuthMarginLoadingState_.sourceLeadString108.begin());
            postAuthMarginLoadingState_.sourceLeadString108[copyCount] = '\0';
        }

        const int matchedWorldIndex = FindRecoveredWorldDescriptorIndexByWorldId(currentSlotRecord->worldId0c);
        if (matchedWorldIndex >= 0 &&
            (postAuthMarginLoadingState_.sourceField12c >= static_cast<uint32_t>(worldDescriptorCountD80_) ||
             (postAuthMarginLoadingState_.sourceField12c == 0u && matchedWorldIndex != 0))) {
            postAuthMarginLoadingState_.sourceField12c = static_cast<uint32_t>(matchedWorldIndex);
        }
    }
}

void CLTLoginMediator::AdoptAuthReplyIntoRecoveredMediatorState() {
    // Address anchors:
    // - launcher.exe:0x4401a0 = strongest current HandleAuthReply implementation
    // - broader writer/validator now in scope: `0x43f300 = CLTLoginState_AuthenticatePending_AuthMessageDispatch`
    // - important owner-side writeback areas on that combined path include:
    //   - `+0x80`  = auth-reply result/status / world-count family
    //   - `+0x684/+0x688` = character-slot record table
    //   - `+0x818` = per-character route-host string triple table
    //   - `+0xd80/+0xd84` = world-descriptor table
    //   - `+0xcc8` = current character/route index byte
    //
    // Current stronger source-owned read from `0x43f300`:
    // - the launcher first builds the `+0xd84` world-descriptor table from auth world entries
    // - then it builds the `+0x688` character-slot table from auth character entries
    // - then it seeds `+0x818` by matching each character record's `worldId0c` against the
    //   descriptor table's `worldId01` and copying the descriptor inline name
    //
    // Important remaining gap:
    // - helper11 later consumes the owner source block rooted at `+0x108`
    //   (string at `+0x108`, object/span at `+0x134`, follow-on blocks at `+0x178/+0x198/+0x1b8`)
    // - do not confuse that with the auth-reply character/world tables recovered here:
    //   this method *does* already reconstruct the auth-side slot/world data families
    //   (`+0x688/+0x818/+0xd84`, current slot/index, route/world joins)
    // - but this scaffold still does not reconstruct the upstream writer for the helper11-only
    //   human-name / appearance block
    worldSlots_.fill(nullptr);
    worldPayloadSlots_.fill(nullptr);
    slotRecordValid688_.fill(false);
    worldDescriptorValidD84_.fill(false);
    slotRecordCount684_ = 0;
    worldDescriptorCountD80_ = 0;
    for (RouteHostStringTripleState& routeString : routeHostStrings818_) {
        routeString.text.clear();
    }

    const size_t worldCount = std::min(worldSlots_.size(), lastAuthReply_.worlds.size());
    for (size_t i = 0; i < worldCount; ++i) {
        worldSlots_[i] = const_cast<mxo::auth::AuthWorldEntry*>(&lastAuthReply_.worlds[i]);
        worldPayloadSlots_[i] = const_cast<mxo::auth::AuthWorldEntry*>(&lastAuthReply_.worlds[i]);
        SeedRecoveredWorldDescriptorFromAuthReply(static_cast<uint8_t>(i), lastAuthReply_.worlds[i]);
    }
    worldDescriptorCountD80_ = static_cast<uint8_t>(worldCount);

    const size_t characterCount = std::min(slotRecords688_.size(), lastAuthReply_.characters.size());
    for (size_t i = 0; i < characterCount; ++i) {
        SeedRecoveredCharacterSlotRecordFromAuthReply(static_cast<uint8_t>(i), lastAuthReply_.characters[i]);
        const SlotRecordState004b5328& slotRecord = slotRecords688_[i];
        const int matchedWorldIndex = FindRecoveredWorldDescriptorIndexByWorldId(slotRecord.worldId0c);
        if (matchedWorldIndex >= 0) {
            // Current source-owned tightening for the active state-8 margin path:
            // preserve the original descriptor-name join, but lowercase the copied text so the
            // reconstructed `+0x818` family can feed DNS host-prefix use directly (`Reality`
            // -> `reality`).
            routeHostStrings818_[i].text =
                LowercaseAsciiString(worldDescriptorsD84_[static_cast<size_t>(matchedWorldIndex)].inlineNamePlus03);
        }
    }
    slotRecordCount684_ = static_cast<uint8_t>(characterCount);

    // Writeback to owner +0x80 (world list count/status family)
    postAuthMarginLoadingState_.worldListCountOrStatus80 = static_cast<uint32_t>(lastAuthReply_.worlds.size());

    // Writeback to owner +0xcc8 (current character/route index byte)
    postAuthMarginLoadingState_.characterRouteIndexCc8 = 0;
    marginRouteState_.currentCharacterOrRouteIndex = 0;

    if (characterCount != 0) {
        const SlotRecordState004b5328& currentSlotRecord = slotRecords688_[0];
        marginRouteState_.pendingWorldId = currentSlotRecord.worldId0c;
        marginRouteState_.currentWorldId = static_cast<int32_t>(currentSlotRecord.worldId0c);
    } else if (worldCount != 0) {
        const mxo::auth::AuthWorldEntry& firstWorld = lastAuthReply_.worlds[0];
        marginRouteState_.pendingWorldId = firstWorld.worldId;
        marginRouteState_.currentWorldId = static_cast<int32_t>(firstWorld.worldId);
    }

    if (const char* routeHostPrefix = GetRouteHostPrefixBySlot(postAuthMarginLoadingState_.characterRouteIndexCc8)) {
        marginRouteState_.routeHostPrefix = routeHostPrefix;
    } else {
        marginRouteState_.routeHostPrefix.clear();
    }

    SeedHelper11SourceBlockFromRecoveredPostAuthStateIfUnset();

    const char* currentDescriptorName = "<empty>";
    if (characterCount != 0) {
        const int matchedWorldIndex = FindRecoveredWorldDescriptorIndexByWorldId(slotRecords688_[0].worldId0c);
        if (matchedWorldIndex >= 0) {
            if (const char* name = GetDescriptorInlineNameByIndex(static_cast<uint8_t>(matchedWorldIndex))) {
                currentDescriptorName = name;
            }
        }
    } else if (worldCount != 0) {
        if (const char* name = GetDescriptorInlineNameByIndex(0)) {
            currentDescriptorName = name;
        }
    }

    Log(
        "DIAGNOSTIC: adopted AS_AuthReply into recovered mediator state worldCount=%u characterCount=%u currentCharacterOrRouteIndex=%u currentSlotWorldId=%u routeHostPrefix='%s' slotRecordHeapString='%s' currentWorldDescriptorName='%s'",
        (unsigned)worldCount,
        (unsigned)characterCount,
        (unsigned)marginRouteState_.currentCharacterOrRouteIndex,
        characterCount == 0 ? 0u : (unsigned)slotRecords688_[0].worldId0c,
        marginRouteState_.routeHostPrefix.empty() ? "<empty>" : marginRouteState_.routeHostPrefix.c_str(),
        GetSlotRecordHeapStringByIndex(postAuthMarginLoadingState_.characterRouteIndexCc8)
            ? GetSlotRecordHeapStringByIndex(postAuthMarginLoadingState_.characterRouteIndexCc8)
            : "<empty>",
        currentDescriptorName);
}

uint32_t CLTLoginMediator::HandleAuthPacketBytes(const uint8_t* packetBytes, size_t packetSize) {
    // Address anchors:
    // - launcher.exe:0x41bc20 = auth opcode read helper on later incoming path
    // - launcher.exe:0x4401a0 = strongest current AS_AuthReply handler
    // - launcher.exe:0x43a330 = concrete auth-reply parse/helper object builder
    mxo::auth::FramedPacket framedPacket;
    if (!packetBytes || !mxo::auth::ParseVariableLengthPacket(packetBytes, packetSize, &framedPacket) ||
        framedPacket.payloadBytes.empty()) {
        return 0;
    }

    const uint8_t rawCode = framedPacket.payloadBytes[0];
    switch (rawCode) {
        case kAuthRawCodeGetPublicKeyReply: {
            // Address anchor: launcher.exe:0x439210 = upstream BeginAuthBootstrap call site
            mxo::auth::GetPublicKeyReply reply;
            if (!mxo::auth::ParseGetPublicKeyReplyPacket(packetBytes, packetSize, &reply)) {
                Log("DIAGNOSTIC: launcher-owned auth failed to parse AS_GetPublicKeyReply");
                return 0;
            }

            lastAuthPublicKeyReply_ = reply;
            authCurrentPublicKeyId_ = reply.publicKeyId;
            SyncRecoveredAuthBootstrapFixedFieldsFromCurrentConfig();
            Log(
                "DIAGNOSTIC: launcher-owned auth parsed AS_GetPublicKeyReply status=%u currentTime=%u publicKeyId=%u keySize=%u modulusLength=%u signatureLength=%u exponentByte=0x%02x hasEmbeddedPublicKey=%u",
                (unsigned)reply.status,
                (unsigned)reply.currentTime,
                (unsigned)reply.publicKeyId,
                (unsigned)reply.keySize,
                (unsigned)reply.modulusLength,
                (unsigned)reply.signatureLength,
                (unsigned)reply.publicExponentByte,
                reply.hasEmbeddedPublicKey ? 1u : 0u);
            expectedAuthRequestName_ = kMessageAsAuthRequest;
            return SendAuthRequestFromReply(reply);
        }

        case 0x09: {
            // Address anchor: launcher.exe:0x439210 = upstream BeginAuthBootstrap call site
            mxo::auth::AuthChallenge challenge;
            if (!mxo::auth::ParseAuthChallengePacket(packetBytes, packetSize, &challenge)) {
                Log("DIAGNOSTIC: launcher-owned auth failed to parse AS_AuthChallenge");
                return 0;
            }

            lastAuthChallenge_ = challenge;
            Log(
                "DIAGNOSTIC: launcher-owned auth parsed AS_AuthChallenge encryptedChallengeLen=%u",
                (unsigned)challenge.encryptedChallengeBytes.size());
            expectedAuthRequestName_ = "AS_AuthChallengeResponse";
            const uint32_t sendResult = SendAuthChallengeResponse(challenge);
            const bool preserveExistingCharacterState8Path =
                currentState_ != nullptr && currentState_->DispatchPhaseCode() == 8u;
            if (sendResult != 0u && scaffoldState10_ != nullptr && !preserveExistingCharacterState8Path) {
                SwitchHelperStateScaffold(0x0au, scaffoldState10_);
            } else if (sendResult != 0u && preserveExistingCharacterState8Path) {
                spdlog::info(
                    "CLTLoginMediator::HandleAuthPacketBytes preserving current state8 through AS_AuthChallengeResponse for the existing-character path instead of forcing helperState=0x0a claim/create flow");
            }
            return sendResult;
        }

        case 0x0b: {
            // Address anchors:
            // - launcher.exe:0x4401a0 = state10 slot 6 / current best AS_AuthReply handler
            // - immediate post-success continuation there is not the later helper14
            //   `AS_GetWorldListRequest` sender at `0x43b830`
            // - instead it goes through helper11:
            //   `0x41b450(0x0b)` -> `0x43c020` (raw post-auth margin packet `0x4d`) -> later
            //   `0x440320` (`MS_LoadCharacterReply`)
            stagedIncomingAuthPacketBytes_.assign(packetBytes, packetBytes + packetSize);
            if (currentState_ != nullptr && currentState_->DispatchPhaseCode() == 8u) {
                spdlog::info(
                    "CLTLoginMediator::HandleAuthPacketBytes routing AS_AuthReply onto the existing-character state8 path; keeping currentState={} and skipping helper10/helper11 claim/create transition",
                    currentState_->DebugName());
                const uint32_t handled = HandleStagedAuthReplyPacketScaffold();
                if (handled != 0u) {
                    expectedMarginRequestName_ = "existing-character state8 raw-0x0f margin packet";
                }
                return handled;
            }
            if (currentState_ != nullptr) {
                return currentState_->Slot6_HandleSecondaryMessage(nullptr, this);
            }
            return HandleStagedAuthReplyPacketScaffold();
        }

        default:
            Log(
                "DIAGNOSTIC: launcher-owned auth received unhandled packet rawCode=0x%02x message='%s' payloadLen=%u",
                (unsigned)rawCode,
                mxo::auth::AuthOpcodeName(rawCode),
                (unsigned)framedPacket.payloadBytes.size());
            break;
    }

    return 0;
}

uint32_t CLTLoginMediator::HandleMarginPacketBytes(const uint8_t* packetBytes, size_t packetSize) {
    if (!packetBytes || packetSize < 1u) {
        return 0u;
    }

    stagedIncomingMarginPacketBytes_.assign(packetBytes, packetBytes + packetSize);
    const uint16_t rawCode = packetBytes[0];
    ++marginPacketReceiveCountScaffold_;
    lastMarginPacketOpcodeScaffold_ = rawCode;
    lastMarginPacketSizeScaffold_ = static_cast<uint32_t>(packetSize);

    spdlog::info(
        "CLTLoginMediator::HandleMarginPacketBytes rawCode=0x{:02x} packetSize={} currentState={} receiveCount={} filteredBeforeSlot6={} slot6DispatchCount={}",
        static_cast<unsigned>(rawCode),
        static_cast<unsigned>(packetSize),
        currentState_ ? currentState_->DebugName() : "<null>",
        static_cast<unsigned>(marginPacketReceiveCountScaffold_),
        static_cast<unsigned>(marginPacketFilteredBeforeSlot6CountScaffold_),
        static_cast<unsigned>(marginPacketSlot6DispatchCountScaffold_));

    // anchor: launcher.exe:0x44af20 -> 0x442d00 -> 0x41f260
    // Exact receive-side boundary now mirrored in source:
    // - decoded codes 2 / 4 / 5 are consumed by base margin dispatch
    // - only other decoded codes survive into owner +0x184 / current helper slot 6
    if (rawCode == 2u || rawCode == 4u || rawCode == 5u) {
        ++marginPacketFilteredBeforeSlot6CountScaffold_;
        spdlog::info(
            "CLTLoginMediator::HandleMarginPacketBytes rawCode=0x{:02x} packetSize={} would be consumed by base margin dispatch before helper slot6 receiveCount={} filteredBeforeSlot6={} currentState={}",
            static_cast<unsigned>(rawCode),
            static_cast<unsigned>(packetSize),
            static_cast<unsigned>(marginPacketReceiveCountScaffold_),
            static_cast<unsigned>(marginPacketFilteredBeforeSlot6CountScaffold_),
            currentState_ ? currentState_->DebugName() : "<null>");
        return 1u;
    }

    if (currentState_ != nullptr) {
        ++marginPacketSlot6DispatchCountScaffold_;
        spdlog::info(
            "CLTLoginMediator::HandleMarginPacketBytes rawCode=0x{:02x} packetSize={} routing to current helper slot6 dispatchCount={} currentState={}",
            static_cast<unsigned>(rawCode),
            static_cast<unsigned>(packetSize),
            static_cast<unsigned>(marginPacketSlot6DispatchCountScaffold_),
            currentState_->DebugName());
        return currentState_->Slot6_HandleSecondaryMessage(nullptr, this);
    }

    spdlog::info(
        "CLTLoginMediator::HandleMarginPacketBytes rawCode=0x{:02x} packetSize={} has no active helper state; using direct helper11 scaffold parser",
        static_cast<unsigned>(rawCode),
        static_cast<unsigned>(packetSize));
    return HandleStagedMarginLoadCharacterReplyPacketScaffold();
}

uint32_t CLTLoginMediator::HandleStagedAuthReplyPacketScaffold() {
    if (stagedIncomingAuthPacketBytes_.empty()) {
        return 0u;
    }

    mxo::auth::AuthReply reply;
    if (!mxo::auth::ParseAuthReplyPacket(
            stagedIncomingAuthPacketBytes_.data(),
            stagedIncomingAuthPacketBytes_.size(),
            &reply)) {
        Log("DIAGNOSTIC: launcher-owned auth failed to parse AS_AuthReply");
        return 0u;
    }

    lastAuthReply_ = reply;
    AdoptAuthReplyIntoRecoveredMediatorState();
    LogParsedAuthReply(reply);
    expectedAuthRequestName_ = nullptr;
    expectedMarginRequestName_ =
        (currentState_ != nullptr && currentState_->DispatchPhaseCode() == 8u)
            ? "post-AS_AuthReply existing-character state8 raw-0x0f margin packet"
            : "post-AS_AuthReply helper11 raw-0x4d margin packet";
    return 1u;
}

uint32_t CLTLoginMediator::HandleStagedMarginLoadCharacterReplyPacketScaffold() {
    if (stagedIncomingMarginPacketBytes_.empty()) {
        return 0u;
    }
    return State11HandleLoadCharacterReplyScaffold(
        stagedIncomingMarginPacketBytes_.data(),
        stagedIncomingMarginPacketBytes_.size());
}

const std::vector<uint8_t>& CLTLoginMediator::StagedIncomingAuthPacketBytes() const {
    return stagedIncomingAuthPacketBytes_;
}

const std::vector<uint8_t>& CLTLoginMediator::StagedIncomingMarginPacketBytes() const {
    return stagedIncomingMarginPacketBytes_;
}

void CLTLoginMediator::BuildAuthEndpoint() {
    // Placeholder only.
    // Original launcher currently appears to preserve host text in owner `+0x4c` and then
    // build a sockaddr-like endpoint block at owner `+0x5c` using the current auth port.
    authEndpoint_ = BuildLoopbackEndpoint(authServerPortHostOrder_);
}

void CLTLoginMediator::BuildMarginEndpoint() {
    marginEndpoint_ = {};
    marginEndpoint_.family = 2;
    marginEndpoint_.portNetworkOrder =
        static_cast<uint16_t>((marginServerPortHostOrder_ << 8) | (marginServerPortHostOrder_ >> 8));
    marginEndpoint_.ipv4NetworkOrder = marginSelectedIpv4_7c_;
}

bool CLTLoginMediator::RebuildMarginAddressList() {
    const std::string resolvedHostName = ResolvedMarginHostName();
    marginAddressList3c_.resolvedHostName = resolvedHostName;
    marginAddressList3c_.ipv4NetworkOrderList.clear();
    marginAddressList3c_.nextIndex = 0;

    if (resolvedHostName.empty()) {
        spdlog::debug("CLTLoginMediator::BeginMarginConnectionScaffold unresolved margin host");
        return false;
    }

    if (!ResolveAllIpv4Addresses(resolvedHostName.c_str(), &marginAddressList3c_.ipv4NetworkOrderList)) {
        spdlog::warn(
            "CLTLoginMediator::BeginMarginConnectionScaffold failed to resolve margin host '{}'",
            resolvedHostName);
        return false;
    }

    return true;
}

bool CLTLoginMediator::SelectMarginEndpointIpv4() {
    if (marginAddressList3c_.ipv4NetworkOrderList.empty()) {
        return false;
    }

    if (marginAddressList3c_.nextIndex >= marginAddressList3c_.ipv4NetworkOrderList.size()) {
        marginAddressList3c_.nextIndex = 0;
    }

    marginSelectedIpv4_7c_ = marginAddressList3c_.ipv4NetworkOrderList[marginAddressList3c_.nextIndex++];
    return true;
}

mxo::liblttcp::CMessageConnection* CLTLoginMediator::EnsureAuthConnectionObject() {
    if (engine_ && authConnectionContextKey_) {
        authConnection_ = engine_->GetOrCreateMessageConnection(authConnectionContextKey_);
        authConnectionOwnedByMediator_ = false;
    } else if (!authConnection_) {
        authConnection_ = new mxo::liblttcp::CMessageConnection(engine_);
        authConnectionOwnedByMediator_ = true;
    }

    if (authConnection_) {
        // anchor: launcher.exe:0x448960 via `0x41d170`
        // Auth connection startup config writes `+0x78 = 1` and `+0x70 = FUN_0041ce00`.
        // Current source scaffold keeps the packet-name family / packetized-mode side of that
        // connection metadata explicit even though the callback body itself is still only used as
        // evidence for naming, not as a live function pointer.
        authConnection_->ConfigurePacketNameFamilyScaffold(
            mxo::liblttcp::CMessageConnectionPacketNameFamilyScaffold::kAuth,
            /*packetizedMessagesEnabled=*/true);
    }
    return authConnection_;
}

mxo::liblttcp::CMessageConnection* CLTLoginMediator::EnsureMarginConnectionObject() {
    mxo::liblttcp::CMarginConnection* marginConnection =
        dynamic_cast<mxo::liblttcp::CMarginConnection*>(marginConnection_);
    if (!marginConnection) {
        if (marginConnectionOwnedByMediator_) {
            delete marginConnection_;
        }
        marginConnection = new mxo::liblttcp::CMarginConnection(engine_);
        if (!marginConnection) {
            marginConnection_ = nullptr;
            marginConnectionOwnedByMediator_ = false;
            return nullptr;
        }

        marginConnection->SetEngine(engine_);
        marginConnection->SetMarginEngine(engine_);
        marginConnection_ = marginConnection;
        marginConnectionOwnedByMediator_ = true;
    }

    marginConnection->SetEngine(engine_);
    marginConnection->SetMarginEngine(engine_);
    marginConnection->ConfigurePacketNameFamilyScaffold(
        mxo::liblttcp::CMessageConnectionPacketNameFamilyScaffold::kMargin,
        /*packetizedMessagesEnabled=*/true);
    if (marginConnectionContextKey_) {
        marginConnection->SetOwnerContext(marginConnectionContextKey_);
    }
    return marginConnection_;
}

// =============================================================================
// Private helper: Populate client.dll's world list view for InitClientDLL
// Address anchor: launcher.exe:0x4d3584 = ILTLoginMediator_SiblingObject
// =============================================================================
void CLTLoginMediator::PopulateClientWorldView() {
    // Populate the client's world list view with launcher-provided data
    // This ensures client.dll receives populated world data when InitClientDLL passes arg6
    Log("launcher-owned PopulateClientWorldView called");

    // Copy launcher-owned world list into the mediator's client-facing view
    for (uint32_t i = 0; i < kRecoveredWorldSlotCapacity && i < arg6WorldList_.totalCount_; ++i) {
        // Store world name at world slot
        worldSlots_[i] = const_cast<void*>(reinterpret_cast<const void*>(arg6WorldList_.worldNames_[i].c_str()));

        // Store world variant at payload slot  
        worldPayloadSlots_[i] = const_cast<void*>(reinterpret_cast<const void*>(&arg6WorldList_.worldVariants_[i]));

        // Mark world as valid/available
        arg6WorldList_.worldValid_[i] = true;
        arg6WorldList_.available_[i] = true;
    }

    Log("launcher-owned PopulateClientWorldView populated %u worlds", (unsigned)kRecoveredWorldSlotCapacity);
}

// =============================================================================
// ILTLoginMediator_BuildWorldList - Kept for reference/testing
// Note: The original launcher doesn't use a separate method call. It initializes
// arg6WorldList_ inline when the object is created. We keep this commented out for
// future reference/testing if needed.
// =============================================================================
// void CLTLoginMediator::BuildWorldList() {
//     PopulateClientWorldView();
// }

}  // namespace mxo::ltlogin
