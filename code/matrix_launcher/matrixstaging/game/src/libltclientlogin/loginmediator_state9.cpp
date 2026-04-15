#include "loginmediator.h"

#include "loginmediator_state9_submit.h"
#include "loginstate.h"
#include "../../../runtime/src/libltcrypto/auth_internal.h"
#include <spdlog/spdlog.h>

#include <array>
#include <cstring>
#include <string>

namespace mxo::ltlogin {
// using mxo::ltlogin directly
namespace {
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
uint32_t CLTLoginMediator::State9SubmitFollowup(uint8_t helperByte4, uint16_t helperWord6) {
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
    // call through callback84 vtable+0x38 (client.dll ClientNetShell)
    const bool callbackPairReady =
        mxo::ltlogin::TryCallback84FillPair(ownerCallback84_, &callbackOutLow, &callbackOutHigh);

    std::string submitTargetText;
    std::string remoteHostName = "<empty>";
    bool submitTargetReady = false;
    uint32_t submitTargetIpv4NetworkOrder = 0u;
    if (marginConnection_ != nullptr) {
        // Mirror assembly at 0x41de40: copy endpoint +0x24..+0x30 to local 16-byte block,
        // then call class methods at 0x44afd0 / 0x44b0d0
        const mxo::liblttcp::LTTCPEndpointKey& remoteEndpoint = marginConnection_->RemoteEndpoint();
        submitTargetIpv4NetworkOrder = remoteEndpoint.ipv4NetworkOrder;
        if (!marginConnection_->RemoteHostName().empty()) {
            remoteHostName = marginConnection_->RemoteHostName();
        }
        // Fidelity to static-RE: use SubmitAddressBlock class matching original 16-byte layout
        // anchor: launcher.exe:0x44afd0 / 0x44b0d0
        mxo::ltlogin::SubmitAddressBlock localBlock{};
        localBlock.family_ = remoteEndpoint.family;
        localBlock.portNetworkOrder_ = remoteEndpoint.portNetworkOrder;
        localBlock.ipv4NetworkOrder_ = remoteEndpoint.ipv4NetworkOrder;
        localBlock.SetPortFromHelperWord(helperWord6);
        submitTargetText = localBlock.FormatHostPortString(/*appendPortFlag=*/1);
        submitTargetReady = !submitTargetText.empty();
    }

    // call through object88 vtable+0x44 (client.dll INetMgr)
    const mxo::ltlogin::Object88SubmitPlan object88Plan =
        mxo::ltlogin::BuildObject88SubmitPlan(
            ownerObject88_,
            callbackPairReady,
            submitTargetReady,
            forwardedArg90,
            ownerCachedHandle147c_);

    uint32_t submitResult = 0u;
    const bool shouldExecuteSubmit =
        object88Plan.modeQueryReady && callbackPairReady && submitTargetReady &&
        (object88Plan.wouldCallDirectSend28 || object88Plan.wouldCallManagedSend24);
    if (shouldExecuteSubmit) {
        submitResult = mxo::ltlogin::ExecuteObject88Submit(
            ownerObject88_,
            object88Plan.managedSendMode,
            &ownerCachedHandle147c_,
            submitTargetText.c_str(),
            callbackOutLow,
            callbackOutHigh,
            forwardedArg90);
    }

    spdlog::info(
        "CLTLoginMediator::State9SubmitFollowup helperByte4=0x{:02x} helperWord6=0x{:04x} ownerF18=0x{:08x} callback84={} object88={} object8c={} forwardedArg90=0x{:08x} cachedHandle147c={} callbackPairReady={} callbackOutLow=0x{:08x} callbackOutHigh=0x{:08x} managedSendMode={} submitTargetReady={} submitTargetIpv4=0x{:08x} submitTarget='{}' remoteHost='{}' executedSubmit={} submitResult=0x{:08x}",
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
        object88Plan.managedSendMode ? 1u : 0u,
        submitTargetReady ? 1u : 0u,
        static_cast<unsigned>(submitTargetIpv4NetworkOrder),
        submitTargetText,
        remoteHostName,
        shouldExecuteSubmit ? 1u : 0u,
        static_cast<unsigned>(submitResult));
    if (shouldExecuteSubmit && submitResult == 3u) {
        spdlog::warn(
            "CLTLoginMediator::State9SubmitFollowup managed join returned 0x00000003 (client.dll: CUDPDriver_ReallyJoinSession timeout path) target='{}' callbackBlobPtr=0x{:08x} callbackBlobLen=0x{:08x} cachedHandle147c={} -- current boundary is before state9 raw-0x11 success / event=0x18",
            submitTargetText,
            static_cast<unsigned>(callbackOutLow),
            static_cast<unsigned>(callbackOutHigh),
            ownerCachedHandle147c_);
    }
    return submitResult;
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
    if (!marginConnection_) {
        return reinterpret_cast<uintptr_t>(this) & 0xffffff00;
    }
    // anchor: launcher.exe:0x41b42c / clear owner+0xf14
    postAuthMarginLoadingState_.state10SendGateFlagF14 = 0u;
    // anchor: launcher.exe:0x41b433 / set owner+0x2d
    marginConnectionFlag2d_ = 1u;
    // anchor: launcher.exe:0x41b437 / query margin connection state at +0x34
    const uint32_t rawState = static_cast<uint32_t>(marginConnection_->State());
    // anchor: launcher.exe:0x41b43a-0x41b448 / state check (1 or 2) and vtable+0x0c(1) call
    const uint32_t closeResult =
        (rawState == 1 || rawState == 2) && marginConnection_
            ? marginConnection_->Close(/*graceful=*/true)
            : 0u;

    spdlog::info(
        "CLTLoginMediator::HandleState9Opcode11SuccessSideEffect cleared owner+0xf14, set owner+0x2d, marginConnectionState={} wouldCallConnectionClose0cArg1={} closeResult=0x{:08x} expectedLaterTail=0x41afc0->0x438df0->0x41cfb0(0x0f)",
        rawState,
        (rawState == 1 || rawState == 2) ? 1u : 0u,
        static_cast<unsigned>(closeResult));
    return 1u;
}

}  // namespace mxo::ltlogin
