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
    // - otherwise cast workItem to CMessageConnectionMessageRef_0x4ba23c* and call
    //   `CMessageConnectionMessageRef_DecodeMessageCode` at `0x41bc20` to get the message opcode
    // - construct the opcodeStorage dword (low word = decoded message code)
    // - call callback84 vtable `+0x0c(&opcodeStorage, workItem)`
    //
    // Original assembly:
    //   uVar2 = CMessageConnectionMessageRef_DecodeMessageCode((CMessageConnectionMessageRef_0x4ba23c*)param_1);
    //   param_1 = (int*)CONCAT22(extraout_var, uVar2);  // opcode in low word
    //   uVar3 = (**(code**)(*(int*)this->ownerCallback84 + 0xc))(&param_1, piVar1);
    if (!ownerCallback84_) {
        return 1u;
    }

    // Cast workItem to message-ref and decode the message code using the original helper
    auto* messageRef = static_cast<mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c*>(workItem);
    uint16_t decodedMessageCode = 0u;
    bool usedHeaderless = false;
    if (!mxo::liblttcp::CMessageConnection_0x4b7928_DecodeMessageCode(
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
  // Ghidra decompile analysis (0x41de40 - 0x41df50):
  // Method: State9SubmitFollowup(this, helperByte4, helperWord6)
  // Stack frame: SUB ESP, 0x24 (36 bytes local)
  // - EBP-0x4: callbackOutHigh (dword)
  // - EBP-0x8: callbackOutLow (dword)
  // - EBP-0x14: submitTargetString (std::string ptr - from 0x44b0d0)
  // - EBP-0x24: local submitAddressBlock (16 bytes copied from marginConnection+0x24)
  //
  // Assembly flow (verified):
  // 0x41de4c: ECX = [ESI + 0x84] (ownerCallback84)
  // 0x41de67: CALL [EAX + 0x38] (callback84 vtable slot +0x38)
  // 0x41de6a: EAX = [ESI + 0x1c] (marginConnection1c)
  // 0x41de6d: ADD EAX, 0x24 (endpoint offset)
  // 0x41de70-0x41de84: Copy 16 bytes from marginConnection+0x24 into local block at EBP-0x24
  // 0x41de8e: CALL 0x44afd0 (SubmitAddress_SetPortFromHelperWord)
  // 0x41de9c: CALL 0x44b0d0 (SubmitAddressBlock_FormatHostPortString)
  // 0x41dea1: ECX = [ESI + 0x88] (ownerObject88)
  // 0x41dea9: CALL [EDX + 0x44] (INetMgr::GetSocket)
  // 0x41deb0: CALL [EDX + 0x30] (isSocketManaged)
  // 0x41deb5: JZ 0x41df0d (branch to direct send if not managed)
  //
  // Managed send path (0x41deb7 - 0x41df0b):
  // - If ownerCachedHandle147c != -1: call [EDX + 0x1c](handle) to close old
  // - Call [EDX + 0x18](0) to get new handle, store in ownerCachedHandle147c
  // - Call [EDX + 0x24](handle, targetString, callbackOutLow, callbackOutHigh, forwardedArg90)
  //
  // Direct send path (0x41df0d - 0x41df36):
  // - Call [EDX + 0x28](targetString, callbackOutLow, callbackOutHigh, forwardedArg90)
  //
  // String cleanup:
  // 0x41df38-0x41df4e: If submitTargetString != NULL, call FUN_00403c20 to free

  const uint32_t forwardedArg90 = helperByte4 != 0u ? ownerOptionalField90_ : 0u;
  uint32_t callbackOutLow = 0u;
  uint32_t callbackOutHigh = 0u;

  // vtable+0x38 on callback84 (client.dll ClientNetShell)
  // Returns a callback pair (low/high) for the session
  const bool callbackPairReady =
  mxo::ltlogin::TryCallback84FillPair(ownerCallback84_, &callbackOutLow, &callbackOutHigh);

  std::string submitTargetText;
  std::string remoteHostName = "<empty>";
  bool submitTargetReady = false;
  uint32_t submitTargetIpv4NetworkOrder = 0u;

  if (marginConnection_ != nullptr) {
    // Copy endpoint from marginConnection+0x24 (matches 0x41de6a pattern)
    // anchor: launcher.exe:0x41de6a
    // Original copies 16 bytes: family, port, ipv4, reserved
    if (!marginConnection_->RemoteHostName().empty()) {
      remoteHostName = marginConnection_->RemoteHostName();
    }

    // Build submit address block from margin connection endpoint
    // anchor: launcher.exe:0x41de6a-0x41de84
    mxo::ltlogin::SubmitAddressBlock localBlock{};
    localBlock.family_ = marginConnection_->remoteEndpoint_.family;
    localBlock.portNetworkOrder_ = marginConnection_->remoteEndpoint_.portNetworkOrder;
    localBlock.ipv4NetworkOrder_ = marginConnection_->remoteEndpoint_.ipv4NetworkOrder;
    submitTargetIpv4NetworkOrder = localBlock.ipv4NetworkOrder_;

    // Original at 0x44afd0: unconditionally sets port from helperWord6 via SetPortFromHelperWord
    // anchor: launcher.exe:0x44afd0
    localBlock.SetPortFromHelperWord(helperWord6);

    // 0x44b0d0: Format host:port string, stores result at EBP-0x14
    // anchor: launcher.exe:0x44b0d0
    submitTargetText = localBlock.FormatHostPortString(/*appendPortFlag=*/1);
    submitTargetReady = !submitTargetText.empty();
  }

  // Query managed send mode from object88
  // anchor: launcher.exe:0x41dea1
  bool managedSendMode = false;
  const bool modeQueryReady = mxo::ltlogin::TryObject88QueryManagedSendMode(ownerObject88_, &managedSendMode);

  // Execute submit based on mode
  // anchor: launcher.exe:0x41dea9-0x41df36
  uint32_t submitResult = 0u;
  if (modeQueryReady && callbackPairReady && submitTargetReady) {
    if (!managedSendMode) {
      // Direct send path: +0x28(submitTarget, callbackOutLow, callbackOutHigh, forwardedArg90)
      // anchor: launcher.exe:0x41df0d
      void** object88Vtable = *reinterpret_cast<void***>(ownerObject88_);
      if (object88Vtable && object88Vtable[10]) {
        using DirectSubmitFn = uint32_t(__thiscall*)(void*, const char*, uint32_t, uint32_t, uint32_t);
        const auto directSubmitFn = reinterpret_cast<DirectSubmitFn>(object88Vtable[10]); // +0x28
        submitResult = directSubmitFn(
            ownerObject88_,
            submitTargetText.c_str(),
            callbackOutLow,
            callbackOutHigh,
            forwardedArg90);
      }
    } else {
      // Managed send path: +0x1c(close old), +0x18(get new handle), +0x24(send)
      // anchor: launcher.exe:0x41deb7
      void** object88Vtable = *reinterpret_cast<void***>(ownerObject88_);
      if (!object88Vtable) {
        return 1u;
      }

      // Release old handle if present
      // anchor: launcher.exe:0x41debb
      if (ownerCachedHandle147c_ != -1) {
        if (object88Vtable[7]) {
          using ReleaseHandleFn = void(__thiscall*)(void*, int32_t);
          const auto releaseHandleFn = reinterpret_cast<ReleaseHandleFn>(object88Vtable[7]); // +0x1c
          releaseHandleFn(ownerObject88_, ownerCachedHandle147c_);
        }
      }

      // Acquire new handle
      // anchor: launcher.exe:0x41dec1
      if (!object88Vtable[6]) {
        return 1u;
      }
      using AcquireHandleFn = uint32_t(__thiscall*)(void*, uint32_t);
      const auto acquireHandleFn = reinterpret_cast<AcquireHandleFn>(object88Vtable[6]); // +0x18
      const uint32_t acquiredHandle = acquireHandleFn(ownerObject88_, 0u);
      ownerCachedHandle147c_ = static_cast<int32_t>(acquiredHandle);

      // Managed send with new handle
      // anchor: launcher.exe:0x41dece
      if (object88Vtable[9]) {
        using ManagedSubmitFn = uint32_t(__thiscall*)(void*, uint32_t, const char*, uint32_t, uint32_t, uint32_t);
        const auto managedSubmitFn = reinterpret_cast<ManagedSubmitFn>(object88Vtable[9]); // +0x24
        submitResult = managedSubmitFn(
            ownerObject88_,
            acquiredHandle,
            submitTargetText.c_str(),
            callbackOutLow,
            callbackOutHigh,
            forwardedArg90);
      }
    }
  }

    spdlog::info(
        "CLTLoginMediator::State9SubmitFollowup helperByte4=0x{:02x} helperWord6=0x{:04x} ownerF18=0x{:08x} callback84={} object88={} object8c={} forwardedArg90=0x{:08x} cachedHandle147c={} callbackPairReady={} callbackOutLow=0x{:08x} callbackOutHigh=0x{:08x} managedSendMode={} submitTargetReady={} submitTargetIpv4=0x{:08x} submitTarget='{}' remoteHost='{}' executedSubmit={} submitResult=0x{:08x}",
        static_cast<unsigned>(helperByte4),
        static_cast<unsigned>(helperWord6),
        static_cast<unsigned>(state6UdpSessionSecretF18_),
        fmt::ptr(ownerCallback84_),
        fmt::ptr(ownerObject88_),
        fmt::ptr(ownerObject8c_),
        static_cast<unsigned>(forwardedArg90),
        ownerCachedHandle147c_,
        callbackPairReady ? 1u : 0u,
        static_cast<unsigned>(callbackOutLow),
        static_cast<unsigned>(callbackOutHigh),
        managedSendMode ? 1u : 0u,
        submitTargetReady ? 1u : 0u,
        static_cast<unsigned>(submitTargetIpv4NetworkOrder),
        submitTargetText,
        remoteHostName,
        (modeQueryReady && callbackPairReady && submitTargetReady) ? 1u : 0u,
        static_cast<unsigned>(submitResult));

  if (submitResult == 3u) {
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
    postAuthMarginLoadingState_0xf14.state10SendGateFlagF14 = 0u;
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
