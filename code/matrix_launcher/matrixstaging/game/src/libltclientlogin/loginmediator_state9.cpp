#include "loginmediator.h"

#include "loginmediator_state9_submit_scaffold.h"
#include "loginstate.h"
#include "../../../runtime/src/libltcrypto/auth_internal.h"
#include "spdlog/spdlog.h"

#include <array>
#include <cstring>
#include <string>
#include <vector>

namespace mxo::ltlogin {
namespace state9submit = mxo::ltlogin::state9submit_scaffold;

// Focused late-login/state9 split:
// - `loginmediator_state9.cpp` now keeps only the mediator-owned state9 methods
// - callback84/object88/submit-helper detail lives in
//   `loginmediator_state9_submit_scaffold.h`
// - this keeps future INetMgr.Default / CUDPDriver::JoinSession work scoped to the active
//   late-login surface instead of forcing rereads of broader mediator/auth files

// anchor: launcher.exe:0x41f1d0
void CLTLoginMediator::SetState9CallbackObjectTriple84_88_8c(void* callback84, void* object88, void* object8c) {
    ownerCallback84_ = callback84;
    ownerObject88_ = object88;
    ownerObject8c_ = object8c;
    spdlog::info(
        "CLTLoginMediator::SetState9CallbackObjectTriple84_88_8c callback84={} object88={} object8c={} (active bounded launcher scope still reads this as init-zero at 0x41ee60, startup triple store at 0x41f1d0, then submit-side reads at 0x41de40)",
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
        opcodeStorage = state9submit::ReadOpcodePrefixVariableWidth(
            stagedIncomingMarginPacketBytes_.data(),
            stagedIncomingMarginPacketBytes_.size());
    } else if (!stagedIncomingAuthPacketBytes_.empty()) {
        mxo::auth::FramedPacket framedPacket;
        if (mxo::auth::ParseVariableLengthPacket(
                stagedIncomingAuthPacketBytes_.data(),
                stagedIncomingAuthPacketBytes_.size(),
                &framedPacket) &&
            !framedPacket.payloadBytes.empty()) {
            opcodeStorage = state9submit::ReadOpcodePrefixVariableWidth(
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

// anchor: launcher.exe:0x41e690
uint32_t CLTLoginMediator::FillState9CallbackBlob18cScaffold(uint32_t* outDwords, uint32_t arg2, uint32_t arg3) {
    if (!outDwords) {
        return 1u;
    }

    const uint32_t stateCode = currentState_ ? currentState_->DispatchPhaseCode() : 0u;
    if (stateCode != 9u) {
        return 0x12000009u;
    }

    const SlotRecordState004b5328* currentSlotRecord = GetCurrentSlotRecord();
    if (!currentSlotRecord) {
        std::memset(outDwords, 0, 0x20u);
        spdlog::info(
            "CLTLoginMediator::FillState9CallbackBlob18cScaffold missing current slot record while state9-gated; zeroed 0x20-byte blob and returned generic failure");
        return 1u;
    }

    outDwords[0] = currentSlotRecord->globalCharacterIdLow03;
    outDwords[1] = currentSlotRecord->globalCharacterIdHigh07;
    outDwords[2] = arg2;
    outDwords[3] = arg3;

    std::array<uint8_t, 16> transformInput{};
    const uint32_t ownerF18 = State6UdpSessionSecretF18();
    std::memcpy(transformInput.data(), &ownerF18, sizeof(ownerF18));

    std::array<uint8_t, 16> marginTwofishKey{};
    if (!CopyMarginBootstrapTwofishKeyScaffold(&marginTwofishKey)) {
        std::memset(outDwords + 4, 0, 16u);
        spdlog::info(
            "CLTLoginMediator::FillState9CallbackBlob18cScaffold missing 16-byte margin Twofish key; zeroed blob tail ownerF18=0x{:08x}",
            static_cast<unsigned>(ownerF18));
        return 1u;
    }

    std::vector<uint8_t> ciphertext;
    if (!mxo::auth::internal::TwofishCbcProcessNoPadding(
            std::vector<uint8_t>(transformInput.begin(), transformInput.end()),
            std::vector<uint8_t>(marginTwofishKey.begin(), marginTwofishKey.end()),
            /*encrypt=*/true,
            &ciphertext) ||
        ciphertext.size() != 16u) {
        std::memset(outDwords + 4, 0, 16u);
        spdlog::info(
            "CLTLoginMediator::FillState9CallbackBlob18cScaffold Twofish block transform failed ownerF18=0x{:08x}",
            static_cast<unsigned>(ownerF18));
        return 1u;
    }

    std::memcpy(outDwords + 4, ciphertext.data(), 16u);
    spdlog::info(
        "CLTLoginMediator::FillState9CallbackBlob18cScaffold built blob currentSlotLow=0x{:08x} currentSlotHigh=0x{:08x} arg2=0x{:08x} arg3=0x{:08x} ownerF18=0x{:08x} tail10=0x{:08x} tail14=0x{:08x} tail18=0x{:08x} tail1c=0x{:08x} (AssemblyTwofish + zero-IV one-block transform over [ownerF18,0,0,0])",
        static_cast<unsigned>(outDwords[0]),
        static_cast<unsigned>(outDwords[1]),
        static_cast<unsigned>(outDwords[2]),
        static_cast<unsigned>(outDwords[3]),
        static_cast<unsigned>(ownerF18),
        static_cast<unsigned>(outDwords[4]),
        static_cast<unsigned>(outDwords[5]),
        static_cast<unsigned>(outDwords[6]),
        static_cast<unsigned>(outDwords[7]));
    return 0u;
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
