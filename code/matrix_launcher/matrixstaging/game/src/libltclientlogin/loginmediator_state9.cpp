#include "loginmediator.h"

#include "loginmediator_state9_submit_scaffold.h"
#include "loginstate.h"
#include "../../../runtime/src/libltcrypto/auth_internal.h"
#include <spdlog/spdlog.h>

#include <array>
#include <cstring>
#include <string>

namespace mxo::ltlogin {
namespace state9submit = mxo::ltlogin::state9submit_scaffold;
namespace {

static uint32_t InvokeMarginConnectionVtable0cWithArg1(
    mxo::liblttcp::CMessageConnection* marginConnection) {
    // anchor: launcher.exe:0x41b448 / vtable+0x0c call with arg 1
    // Direct vtable dispatch: (*marginConnection->vtable)[0x3](marginConnection, 1)
    // Corresponds to CMessageConnection::Close(true) in our implementation
    if (!marginConnection) {
        return 0u;
    }
    return marginConnection->Close(/*graceful=*/true);
}

}  // namespace

// anchor: launcher.exe:0x41f1d0 / owner-side mirror of the startup triple into +0x84/+0x88/+0x8c
void CLTLoginMediator::SetState9CallbackObjectTriple84_88_8c(void* callback84, void* object88, void* object8c) {
    ownerCallback84_ = callback84;
    ownerObject88_ = object88;
    ownerObject8c_ = object8c;
    spdlog::info(
        "CLTLoginMediator::SetState9CallbackObjectTriple84_88_8c (owner-side mirror) callback84={} object88={} object8c={} (active bounded launcher scope still reads this as init-zero at 0x41ee60, startup triple store at 0x41f1d0, then submit-side reads at 0x41de40)",
        fmt::ptr(ownerCallback84_),
        fmt::ptr(ownerObject88_),
        fmt::ptr(ownerObject8c_));
}

// anchor: launcher.exe:0x41c5c0
// anchor: launcher.exe:0x41bc20 -> CMessageConnectionMessageRef_DecodeMessageCode
uint32_t CLTLoginMediator::DispatchSecondaryMessageToOwnerCallback84(void* workItem) {
    // Faithful read from `0x41c5c0`:
    // - if owner `+0x84` is null, return `1`
    // - otherwise cast workItem to CMessageConnectionMessageRef* and call
    //   `CMessageConnectionMessageRef_DecodeMessageCode` at `0x41bc20` to get the message opcode
    // - construct the opcodeStorage dword (low word = decoded message code)
    // - call callback84 vtable `+0x0c(&opcodeStorage, workItem)`
    //
    // Original assembly:
    //   uVar2 = CMessageConnectionMessageRef_DecodeMessageCode((CMessageConnectionMessageRef*)param_1);
    //   param_1 = (int*)CONCAT22(extraout_var, uVar2);  // opcode in low word
    //   uVar3 = (**(code**)(*(int*)this->ownerCallback84 + 0xc))(&param_1, piVar1);
    if (!ownerCallback84_) {
        return 1u;
    }

    // Cast workItem to message-ref and decode the message code using the original helper
    auto* messageRef = static_cast<mxo::liblttcp::CMessageConnectionMessageRef*>(workItem);
    uint16_t decodedMessageCode = 0u;
    bool usedHeaderless = false;
    if (!mxo::liblttcp::CMessageConnection_DecodeMessageCodeScaffold(
            *messageRef,
            &decodedMessageCode,
            &usedHeaderless)) {
        spdlog::warn(
            "CLTLoginMediator::DispatchSecondaryMessageToOwnerCallback84 failed to decode message code from messageRef this={}",
            fmt::ptr(this));
        return 1u;
    }

    // Construct opcodeStorage dword: low 16 bits = decoded message code
    uint32_t opcodeStorage = static_cast<uint32_t>(decodedMessageCode);

    // Call callback84 vtable+0x0c with (&opcodeStorage, workItem)
    using OwnerCallback84DispatchSecondaryFn = uint32_t(__thiscall*)(void*, uint32_t*, void*);
    void** callbackVtable = *reinterpret_cast<void***>(ownerCallback84_);
    if (!callbackVtable || !callbackVtable[3]) {
        return 1u;
    }

    const auto dispatchFn = reinterpret_cast<OwnerCallback84DispatchSecondaryFn>(callbackVtable[3]);
    const uint32_t dispatchResult = dispatchFn(ownerCallback84_, &opcodeStorage, workItem);
    spdlog::info(
        "CLTLoginMediator::DispatchSecondaryMessageToOwnerCallback84 callback84={} decodedMessageCode=0x{:04x} opcodeStorage=0x{:08x} workItem={} -> dispatchResult=0x{:08x}",
        fmt::ptr(ownerCallback84_),
        static_cast<unsigned>(decodedMessageCode),
        static_cast<unsigned>(opcodeStorage),
        fmt::ptr(workItem),
        static_cast<unsigned>(dispatchResult));
    return dispatchResult;
}

// anchor: launcher.exe:0x41de40
uint32_t CLTLoginMediator::State9SubmitFollowupScaffold(uint8_t helperByte4, uint16_t helperWord6) {
    // Current focused state9 submit read:
    // - natural original now proves `0x439780 -> 0x41de40 -> 0x43c180`
    // - `0x41de40` first queries callback84 `+0x38`, then builds submit target text from the
    //   margin connection address, then branches through object88 `(+0x44)->(+0x30)` into:
    //   - direct `+0x28`
    //   - or managed `+0x1c / +0x18 / +0x24`
    // - the remaining gap is now much narrower than generic "missing collaborators":
    //   - callback84 pair generation is source-owned via `+0x18c`
    //   - owner `+0x88` provenance is now bounded tightly enough to preserve the startup-provided
    //     netMgr wrapper on the active path
    // - current bounded provenance answer to preserve while doing that work:
    //   - owner `+0x84/+0x88/+0x8c` are zeroed at `0x41ee60`
    //   - set from the startup `arg6->+0x124(netShell, netMgr, distrObjExecutive)` triple by
    //     `0x41f1d0`
    //   - live original now also proves owner `+0x88` stays unchanged from that store through
    //     `0x439780 -> 0x41de40 -> 0x43c180`
    // - callback84 is also now tighter than a generic opaque blob provider:
    //   launcher `+0x18c / 0x41e690` seeds blob `+0x10` from owner `+0xf18`, then runs the
    //   16-byte region through `0x41df60 / 0x44b190 / 0x44b570`, and the active one-block result is
    //   now live-matched as a Twofish zero-IV block transform over `[ownerF18, 0, 0, 0]`
    const uint32_t forwardedArg90 = helperByte4 != 0u ? ownerOptionalField90_ : 0u;
    uint32_t callbackOutLow = 0u;
    uint32_t callbackOutHigh = 0u;
    const bool callbackPairReady =
        state9submit::TryCallback84FillPair(ownerCallback84_, &callbackOutLow, &callbackOutHigh);

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
        submitTargetText = state9submit::BuildSubmitTargetText(
            endpoint,
            helperWord6,
            /*appendPort=*/true);
        submitTargetReady = !submitTargetText.empty();
    }

    const state9submit::Object88SubmitPlan object88SubmitPlan =
        state9submit::BuildObject88SubmitPlan(
            ownerObject88_,
            callbackPairReady,
            submitTargetReady,
            forwardedArg90,
            ownerCachedHandle147c_);

    uint32_t submitResult = 0u;
    const bool shouldExecuteSubmit =
        object88SubmitPlan.modeQueryReady && callbackPairReady && submitTargetReady &&
        (object88SubmitPlan.wouldCallDirectSend28 || object88SubmitPlan.wouldCallManagedSend24);
    if (shouldExecuteSubmit) {
        submitResult = state9submit::ExecuteObject88Submit(
            ownerObject88_,
            object88SubmitPlan.managedSendMode,
            &ownerCachedHandle147c_,
            submitTargetText.c_str(),
            callbackOutLow,
            callbackOutHigh,
            forwardedArg90);
    }

    spdlog::info(
        "CLTLoginMediator::State9SubmitFollowupScaffold helperByte4=0x{:02x} helperWord6=0x{:04x} ownerF18=0x{:08x} callback84={} object88={} object8c={} forwardedArg90=0x{:08x} cachedHandle147c={} callbackPairReady={} callbackOutLow=0x{:08x} callbackOutHigh=0x{:08x} object88ModeQueryReady={} managedSendMode={} object88Route='{}' wouldReleaseCachedHandle147c={} wouldAcquireManagedHandle18={} wouldCallDirectSend28={} wouldCallManagedSend24={} submitTargetReady={} submitTargetIpv4=0x{:08x} submitTarget='{}' remoteHost='{}' executedSubmit={} submitResult=0x{:08x} (state9 now preserves startup-provided +0x124 callback84/object88 provenance, uses live +0x18c callback blob fill, and executes the natural direct-vs-managed object88 branch only when all proven prerequisites are present)",
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
        state9submit::Object88SubmitRouteName(object88SubmitPlan.route),
        object88SubmitPlan.wouldReleaseCachedHandle147c ? 1u : 0u,
        object88SubmitPlan.wouldAcquireManagedHandle18 ? 1u : 0u,
        object88SubmitPlan.wouldCallDirectSend28 ? 1u : 0u,
        object88SubmitPlan.wouldCallManagedSend24 ? 1u : 0u,
        submitTargetReady ? 1u : 0u,
        static_cast<unsigned>(submitTargetIpv4NetworkOrder),
        submitTargetText,
        remoteHostName,
        shouldExecuteSubmit ? 1u : 0u,
        static_cast<unsigned>(submitResult));
    if (shouldExecuteSubmit && submitResult == 3u) {
        spdlog::warn(
            "CLTLoginMediator::State9SubmitFollowupScaffold managed join returned 0x00000003 (client.dll: CUDPDriver_ReallyJoinSession timeout path) target='{}' callbackBlobPtr=0x{:08x} callbackBlobLen=0x{:08x} cachedHandle147c={} -- current boundary is before state9 raw-0x11 success / event=0x18",
            submitTargetText,
            static_cast<unsigned>(callbackOutLow),
            static_cast<unsigned>(callbackOutHigh),
            ownerCachedHandle147c_);
    }
    return submitResult;
}

bool CLTLoginMediator::PrepareMarginConnectionCloseWaitEvent0fScaffold(
    uint32_t* outConnectionState,
    bool* outWouldCallConnectionClose0c,
    bool clearState10SendGateF14) {
    if (outConnectionState) {
        *outConnectionState = 0u;
    }
    if (outWouldCallConnectionClose0c) {
        *outWouldCallConnectionClose0c = false;
    }
    if (!marginConnection_) {
        return false;
    }

    if (clearState10SendGateF14) {
        postAuthMarginLoadingState_.state10SendGateFlagF14 = 0u;
    }
    marginConnectionFlag2d_ = 1u;

    const uint32_t rawState = static_cast<uint32_t>(marginConnection_->State());
    const bool wouldCallConnectionClose0c =
        rawState == static_cast<uint32_t>(mxo::liblttcp::LTTCPEngineConnectionState::kConnectActive) ||
        rawState == static_cast<uint32_t>(mxo::liblttcp::LTTCPEngineConnectionState::kUdpMonitorActive);

    if (outConnectionState) {
        *outConnectionState = rawState;
    }
    if (outWouldCallConnectionClose0c) {
        *outWouldCallConnectionClose0c = wouldCallConnectionClose0c;
    }
    return true;
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
    // - that graceful close is what later enables the natural `0x41afc0 -> 0x438df0`
    //   completion-fallback re-entry into shared slot 2 and event `0x0f`
    //
    // Keep the wrapper/owner split explicit in source too:
    // - wrapper-facing arg6 `+0x16c` is teardown-visible close/wait-event-`0x0f`
    // - owner-side `0x41b420` is still the concrete state9 opcode-`0x11` success-side effect
    uint32_t rawState = 0u;
    bool wouldCallConnectionClose0c = false;
    const bool armed = PrepareMarginConnectionCloseWaitEvent0fScaffold(
        &rawState,
        &wouldCallConnectionClose0c,
        /*clearState10SendGateF14=*/true);
    // anchor: launcher.exe:0x41b43a-0x41b448 / state check and vtable+0x0c(1) call
    const uint32_t closeResult =
        (rawState == 1 || rawState == 2) && marginConnection_
            ? InvokeMarginConnectionVtable0cWithArg1(marginConnection_)
            : 0u;

    spdlog::info(
        "CLTLoginMediator::HandleState9Opcode11SuccessSideEffect cleared owner+0xf14, set owner+0x2d, marginConnectionState={} wouldCallConnectionClose0cArg1={} closeResult=0x{:08x} expectedLaterTail=0x41afc0->0x438df0->0x41cfb0(0x0f)",
        rawState,
        wouldCallConnectionClose0c ? 1u : 0u,
        static_cast<unsigned>(closeResult));
    return armed ? 1u : 0u;
}

}  // namespace mxo::ltlogin
