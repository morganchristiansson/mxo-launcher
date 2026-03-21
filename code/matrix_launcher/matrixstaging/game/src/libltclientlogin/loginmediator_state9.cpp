#include "loginmediator.h"

#include "loginstate.h"
#include "spdlog/spdlog.h"

#include <string>

namespace mxo::ltlogin {
namespace {

// Focused late-login/state9 split:
// - keep the active callback84/object88/submit path in its own TU so future INetMgr/CUDPDriver
//   work does not require loading the full auth/bootstrap/margin file first.

static uint16_t ByteSwap16State9(uint16_t value) {
    return static_cast<uint16_t>((value << 8) | (value >> 8));
}

static std::string FormatIpv4StoredDwordLittleEndianDottedQuad(uint32_t storedIpv4Dword) {
    // anchor: launcher.exe:0x44b0d0
    // Important correction from a natural-original WineDbg stop inside `0x41de40`:
    // - the copied submit-address dword at bytes `+4..+7` is formatted in memory-byte order
    // - representative natural values at the `0x41df0b` boundary were:
    //   - stored dword `0x3d7a3025`
    //   - formatted text `"37.48.122.61:10000"`
    // - so the previous source-owned big-endian dotted-quad read was backwards
    return fmt::format(
        "{}.{}.{}.{}",
        static_cast<unsigned>(storedIpv4Dword & 0xffu),
        static_cast<unsigned>((storedIpv4Dword >> 8) & 0xffu),
        static_cast<unsigned>((storedIpv4Dword >> 16) & 0xffu),
        static_cast<unsigned>((storedIpv4Dword >> 24) & 0xffu));
}

static std::string BuildState9SubmitTargetTextScaffold(
    const mxo::liblttcp::LTTCPEndpointKey& endpoint,
    uint16_t helperWord6,
    bool appendPort) {
    // anchor: launcher.exe:0x44afd0 / 0x44b0d0
    // Current best read from the original helpers:
    // - `0x44afd0` copies helper word `+6` into sockaddr-like bytes `+2..+3` after endian swap
    // - `0x44b0d0` formats the IPv4 string from bytes `+4..+7`
    // - when requested, it appends `":%d"` using the host-order port decoded back from `+2..+3`
    if (endpoint.family != 2u || endpoint.ipv4NetworkOrder == 0u) {
        return std::string();
    }

    const uint16_t portNetworkOrder = ByteSwap16State9(helperWord6);
    const uint16_t portHostOrder = ByteSwap16State9(portNetworkOrder);
    std::string out = FormatIpv4StoredDwordLittleEndianDottedQuad(endpoint.ipv4NetworkOrder);
    if (appendPort) {
        out += fmt::format(":{}", static_cast<unsigned>(portHostOrder));
    }
    return out;
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

static bool TryState9Callback84FillPair(void* callback84, uint32_t* outLow, uint32_t* outHigh) {
    if (outLow) {
        *outLow = 0u;
    }
    if (outHigh) {
        *outHigh = 0u;
    }
    if (!callback84) {
        return false;
    }

    // New client-side callback84 tightening from `client.dll`:
    // - the transplanted callback84 object currently resolves to the `ClientNetShell` family
    // - its vtable `+0x38` callee is `client.dll:0x62006580`
    // - that method is **not** self-contained on the callback object itself
    // - it first checks the client-side resolved `ILTLoginMediator.Default` global at `0x629df7f0`
    //   through vtable `+0x10`
    // - then it calls that global's vtable `+0x18c(&DAT_629e0284, 900, 0)`
    //   - launcher-side implementation of that slot is now tightened too:
    //     `CLTLoginMediator_FillState9CallbackBlob18c` / `0x41e690`
    //   - it is state9-gated and fills a fixed `0x20`-byte blob from:
    //     - current slot id low/high
    //     - caller args
    //     - owner dword `+0xf18` copied into blob `+0x10`
    //     - trailing material rooted at mediator `+0xd4 = 0x41b4f0 -> owner +0x1c + 0x85`
    //   - the helper family used to materialize that tail (`0x41df60 / 0x44b190`) is not unique
    //     to state9; the same family is also reused by `AuthBootstrap680_SendAuthRequest`
    //     and carries string-backed `ValueNames` / `FeedbackSize` parameters, so current best
    //     read is a shared Crypto++-style parameterized transform wrapper rather than ad-hoc
    //     state9-only launcher glue
    // - only after that does it return the pair `(&DAT_629e0284, 0x20)`
    // Practical consequence for launcher-side state9 work:
    // direct raw reuse of the captured `+0x124` callback84 object is insufficient by itself;
    // the real dependency chain also includes the client-side binder-managed mediator/global state
    // behind `0x629df7f0` and its later `+0x18c` writer.
    void** vtable = *reinterpret_cast<void***>(callback84);
    if (!vtable || !vtable[14]) {
        return false;
    }

    using FillPairFn = void(__thiscall*)(void*, uint32_t*, uint32_t*);
    const auto fillPairFn = reinterpret_cast<FillPairFn>(vtable[14]); // vtable +0x38
    fillPairFn(callback84, outLow, outHigh);
    return true;
}

static bool TryState9Object88QueryManagedSendMode(void* object88, bool* outManagedSendMode) {
    if (outManagedSendMode) {
        *outManagedSendMode = false;
    }
    if (!object88) {
        return false;
    }

    void** vtable = *reinterpret_cast<void***>(object88);
    if (!vtable || !vtable[17]) {
        return false;
    }

    using GetModeObjectFn = void*(__thiscall*)(void*);
    const auto getModeObjectFn = reinterpret_cast<GetModeObjectFn>(vtable[17]); // vtable +0x44
    void* modeObject = getModeObjectFn(object88);
    if (!modeObject) {
        return false;
    }

    void** modeVtable = *reinterpret_cast<void***>(modeObject);
    if (!modeVtable || !modeVtable[12]) {
        return false;
    }

    using QueryManagedSendModeFn = uint8_t(__thiscall*)(void*);
    const auto queryManagedSendModeFn = reinterpret_cast<QueryManagedSendModeFn>(modeVtable[12]); // vtable +0x30
    if (outManagedSendMode) {
        *outManagedSendMode = (queryManagedSendModeFn(modeObject) != 0u);
    }
    return true;
}

enum class State9Object88SubmitRouteScaffold {
    kUnavailable = 0,
    kDirectSlot28,
    kManagedSlots18_1c_24,
};

struct State9Object88SubmitPlanScaffold {
    State9Object88SubmitRouteScaffold route = State9Object88SubmitRouteScaffold::kUnavailable;
    bool modeQueryReady = false;
    bool managedSendMode = false;
    bool callbackPairReady = false;
    bool submitTargetReady = false;
    bool wouldReleaseCachedHandle147c = false;
    bool wouldAcquireManagedHandle18 = false;
    bool wouldCallDirectSend28 = false;
    bool wouldCallManagedSend24 = false;
    uint32_t forwardedArg90 = 0u;
};

static const char* State9Object88SubmitRouteName(State9Object88SubmitRouteScaffold route) {
    switch (route) {
        case State9Object88SubmitRouteScaffold::kDirectSlot28:
            return "direct:+0x28";
        case State9Object88SubmitRouteScaffold::kManagedSlots18_1c_24:
            return "managed:+0x1c/+0x18/+0x24";
        default:
            return "unavailable";
    }
}

static State9Object88SubmitPlanScaffold BuildState9Object88SubmitPlanScaffold(
    void* object88,
    bool callbackPairReady,
    bool submitTargetReady,
    uint32_t forwardedArg90,
    int32_t cachedHandle147c) {
    State9Object88SubmitPlanScaffold plan = {};
    plan.callbackPairReady = callbackPairReady;
    plan.submitTargetReady = submitTargetReady;
    plan.forwardedArg90 = forwardedArg90;
    plan.modeQueryReady = TryState9Object88QueryManagedSendMode(object88, &plan.managedSendMode);
    if (!plan.modeQueryReady) {
        return plan;
    }

    if (plan.managedSendMode) {
        plan.route = State9Object88SubmitRouteScaffold::kManagedSlots18_1c_24;
        plan.wouldReleaseCachedHandle147c = (cachedHandle147c != -1);
        plan.wouldAcquireManagedHandle18 = true;
        plan.wouldCallManagedSend24 = callbackPairReady && submitTargetReady;
    } else {
        plan.route = State9Object88SubmitRouteScaffold::kDirectSlot28;
        plan.wouldCallDirectSend28 = callbackPairReady && submitTargetReady;
    }
    return plan;
}

}  // namespace

// anchor: launcher.exe:0x41f1d0
void CLTLoginMediator::SetState9CallbackObjectTriple84_88_8c(void* callback84, void* object88, void* object8c) {
    ownerCallback84_ = callback84;
    ownerObject88_ = object88;
    ownerObject8c_ = object8c;
    spdlog::info(
        "CLTLoginMediator::SetState9CallbackObjectTriple84_88_8c callback84={} object88={} object8c={} (strongest current origin: owner/arg6 vtable +0x124 startup triple)",
        fmt::ptr(ownerCallback84_),
        fmt::ptr(ownerObject88_),
        fmt::ptr(ownerObject8c_));
}

void CLTLoginMediator::CaptureDeferredState9CallbackObjectTriple84_88_8c_Scaffold(
    void* callback84,
    void* object88,
    void* object8c) {
    const bool anyCaptured =
        callback84 != nullptr || object88 != nullptr || object8c != nullptr;
    spdlog::info(
        "CLTLoginMediator::CaptureDeferredState9CallbackObjectTriple84_88_8c_Scaffold callback84={} object88={} object8c={} valid={} (captured from owner/arg6 vtable +0x124 without touching live state yet)",
        fmt::ptr(callback84),
        fmt::ptr(object88),
        fmt::ptr(object8c),
        anyCaptured ? 1u : 0u);
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
    // - owner `+0x1c + 0x24..0x30` seeds a local 16-byte sockaddr-like submit-address block
    // - `0x44afd0` / `0x44b0d0` then turn that block plus helper word `+6` into a host:port
    //   submit target, not an opaque packet blob
    // - owner object `+0x88` then splits exactly as:
    //   - test mode through `(+0x44)->(+0x30)`
    //   - direct branch: `+0x28(submitTargetString, callbackOutLow, callbackOutHigh, optionalArg90)`
    //   - managed branch:
    //     - if owner `+0x147c != -1`, first `+0x1c(cachedHandle147c)`
    //     - then `handle = +0x18(0)`
    //     - cache owner `+0x147c = handle`
    //     - then `+0x24(handle, submitTargetString, callbackOutLow, callbackOutHigh, optionalArg90)`
    // - representative natural-original stop also had non-null owner callback/object triple
    //   at `+0x84/+0x88/+0x8c`
    // - newer natural-original + client-cross-check now also tightens the concrete object88 branch:
    //   - owner `+0x88` was `INetMgr.Default` wrapper `0x62999968`
    //   - wrapper `+0x44` returned inner object `+0x04 = 0x08814860`
    //   - inner vtable `+0x30 = 0x623b3800` returned `1`, so the natural run took the
    //     managed-submit branch
    //   - managed `+0x18(0)` then returned handle `0x224`, which `0x41de40` stored into
    //     owner `+0x147c` before calling managed submit `+0x24`
    //   - client-side string/debug anchors on that path now read as `CUDPDriver` / `JoinSession`
    // - strongest current source/runtime origin for that triple is now narrower too:
    //   deeper client init calls owner/arg6 vtable `+0x124(netShell, netMgr, distrObjExecutive)`,
    //   and `0x41f1d0` stores those three parameters directly into `+0x84/+0x88/+0x8c`
    // - no earlier launcher.exe-side producer on the active path is isolated yet beyond that
    //   startup capture; current replacement source keeps the triple launcher-owned and explicit
    //   instead of silently transplanting client runtime objects
    // - owner dword `+0x90` is only forwarded when helper byte `+4 != 0`
    // - owner dword `+0x147c` caches the acquired handle on the managed-send branch
    //
    // Current source-owned tightening deliberately stops one step short of a fake submit:
    // - it can remember the client-side arg6 `+0x124` startup triple as a possible provenance clue
    // - but direct replacement-side reuse of that captured triple is now crash-proven unsafe:
    //   crashdump `MatrixOnline_0.0_crash_69.dmp` stopped at `0x4230dd -> callback84 vtable +0x38`
    //   with top frame `client:0x629ddfc8`, before any later object88 `(+0x44)->(+0x30)` work ran
    // - newer client-side static tightening now also explains *why* that direct reuse is too weak:
    //   - callback84 currently resolves to `ClientNetShell` vtable `+0x38` / `client.dll:0x62006580`
    //   - that wrapper does not answer from object-local state alone
    //   - it checks client global `0x629df7f0 = resolved ILTLoginMediator.Default`
    //   - then calls global vtable `+0x18c(&DAT_629e0284, 900, 0)`
    //     - launcher-side `+0x18c` is now tightened as `0x41e690`
    //     - that slot is state9-gated and fills a fixed `0x20`-byte callback blob from the
    //       current slot id pair, caller args, owner `+0xf18`, and trailing material sourced via
    //       mediator `+0xd4 = 0x41b4f0 -> owner +0x1c + 0x85`
    //     - current best tail-side read is slightly stronger too:
    //       `0x41df60 / 0x44b190` is part of a shared `ValueNames` / `FeedbackSize`
    //       parameterized transform family also reused by `AuthBootstrap680_SendAuthRequest`
    //   - and only then returns pair `(&DAT_629e0284, 0x20)` to the launcher-side submit path
    // - practical consequence: the active state9 problem is not just “fill nulls”; it is to
    //   reconstruct the correct launcher-owned collaborator/wrapper state behind `0x41de40`
    // - source now owns the host:port builder plus the exact object88 direct-vs-managed submit
    //   split as an explicit scaffold plan/log boundary
    // - but it still does **not** claim that callback84/object88/object8c are valid live launcher-
    //   owned collaborators yet, and it still does not perform the actual `+0x28 / +0x1c / +0x18 /
    //   +0x24` calls on the current deliberate path
    // Returning `0` still preserves the observed `0x439780` success-side event-post shape
    // (`< 1` => post event `0x17`) while narrowing the remaining blocker beyond mere null collaborators.
    const uint32_t forwardedArg90 = helperByte4 != 0u ? ownerOptionalField90_ : 0u;
    uint32_t callbackOutLow = 0u;
    uint32_t callbackOutHigh = 0u;
    const bool callbackPairReady = TryState9Callback84FillPair(ownerCallback84_, &callbackOutLow, &callbackOutHigh);

    std::string submitTargetText;
    std::string remoteHostName = "<empty>";
    bool submitTargetReady = false;
    uint32_t submitTargetIpv4NetworkOrder = 0u;
    if (marginConnection_ != nullptr) {
        const mxo::liblttcp::LTTCPEndpointKey& endpoint = marginConnection_->RemoteEndpoint();
        submitTargetIpv4NetworkOrder = endpoint.ipv4NetworkOrder;
        if (!marginConnection_->RemoteHostName().empty()) {
            remoteHostName = marginConnection_->RemoteHostName();
        }
        submitTargetText = BuildState9SubmitTargetTextScaffold(endpoint, helperWord6, /*appendPort=*/true);
        submitTargetReady = !submitTargetText.empty();
    }

    const State9Object88SubmitPlanScaffold object88SubmitPlan = BuildState9Object88SubmitPlanScaffold(
        ownerObject88_,
        callbackPairReady,
        submitTargetReady,
        forwardedArg90,
        ownerCachedHandle147c_);

    spdlog::info(
        "CLTLoginMediator::State9SubmitFollowupScaffold helperByte4=0x{:02x} helperWord6=0x{:04x} ownerF18=0x{:08x} callback84={} object88={} object8c={} forwardedArg90=0x{:08x} cachedHandle147c={} callbackPairReady={} callbackOutLow=0x{:08x} callbackOutHigh=0x{:08x} object88ModeQueryReady={} managedSendMode={} object88Route='{}' wouldReleaseCachedHandle147c={} wouldAcquireManagedHandle18={} wouldCallDirectSend28={} wouldCallManagedSend24={} submitTargetReady={} submitTargetIpv4=0x{:08x} submitTarget='{}' remoteHost='{}' (source now mirrors 0x44afd0/0x44b0d0 host:port formatting plus the object88 direct-vs-managed submit split; remaining gap stays on valid launcher-owned callback84/object88 collaborators)",
        static_cast<unsigned>(helperByte4),
        static_cast<unsigned>(helperWord6),
        static_cast<unsigned>(State6UdpSessionSecretF18()),
        fmt::ptr(ownerCallback84_),
        fmt::ptr(ownerObject88_),
        fmt::ptr(ownerObject8c_),
        static_cast<unsigned>(forwardedArg90),
        ownerCachedHandle147c_,
        callbackPairReady ? 1u : 0u,
        static_cast<unsigned>(callbackOutLow),
        static_cast<unsigned>(callbackOutHigh),
        object88SubmitPlan.modeQueryReady ? 1u : 0u,
        object88SubmitPlan.managedSendMode ? 1u : 0u,
        State9Object88SubmitRouteName(object88SubmitPlan.route),
        object88SubmitPlan.wouldReleaseCachedHandle147c ? 1u : 0u,
        object88SubmitPlan.wouldAcquireManagedHandle18 ? 1u : 0u,
        object88SubmitPlan.wouldCallDirectSend28 ? 1u : 0u,
        object88SubmitPlan.wouldCallManagedSend24 ? 1u : 0u,
        submitTargetReady ? 1u : 0u,
        static_cast<unsigned>(submitTargetIpv4NetworkOrder),
        submitTargetText,
        remoteHostName);
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

}  // namespace mxo::ltlogin
