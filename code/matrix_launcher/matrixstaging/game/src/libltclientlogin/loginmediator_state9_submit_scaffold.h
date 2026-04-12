#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "../../../runtime/src/liblttcp/lttcpconnection.h"
#include "spdlog/fmt/fmt.h"

namespace mxo::ltlogin::state9submit_scaffold {

// Focused late-login helper split:
// - keep the state9 callback84/object88/submit helpers out of `loginmediator_state9.cpp`
// - future INetMgr.Default / CUDPDriver::JoinSession work can now stay in this one header + the
//   small state9 TU instead of reopening broader login/auth files
// - active anchors:
//   - launcher.exe:0x41de40 = CLTLoginMediator_State9SubmitFollowup
//   - launcher.exe:0x41e690 = CLTLoginMediator_FillState9CallbackBlob18c
//   - launcher.exe:0x41df60 = FeedbackSizeTransformAdapter_ConstructSmall
//   - launcher.exe:0x44b190 = FeedbackSizeTransformAdapter_InvokeConfigure40
//   - launcher.exe:0x44b570 = FeedbackSizeTransformAdapter_TransformBuffer
//   - client.dll:0x620065e0 = ClientNetShell_BuildState9SeedObject3c
//   - launcher.exe string anchor `AssemblyTwofish` now closes the active one-block transform enough
//     to source-own it as Twofish over a zero-IV single block

inline uint16_t ByteSwap16(uint16_t value) {
    return static_cast<uint16_t>((value << 8) | (value >> 8));
}

inline std::string FormatIpv4StoredDwordLittleEndianDottedQuad(uint32_t storedIpv4Dword) {
    // anchor: launcher.exe:0x44b0d0
    // Natural-original correction from the `0x41df0b` submit stop:
    // - the copied IPv4 dword is rendered in memory-byte order
    // - representative natural values:
    //   - stored dword `0x3d7a3025`
    //   - formatted text `"37.48.122.61:10000"`
    return fmt::format(
        "{}.{}.{}.{}",
        static_cast<unsigned>(storedIpv4Dword & 0xffu),
        static_cast<unsigned>((storedIpv4Dword >> 8) & 0xffu),
        static_cast<unsigned>((storedIpv4Dword >> 16) & 0xffu),
        static_cast<unsigned>((storedIpv4Dword >> 24) & 0xffu));
}

inline std::string BuildSubmitTargetText(
    const mxo::liblttcp::LTTCPEndpointKey& endpoint,
    uint16_t helperWord6,
    bool appendPort) {
    // anchors: launcher.exe:0x44afd0 / 0x44b0d0
    // Current best source-owned contract:
    // - `0x44afd0` endian-swaps helper word `+6` into submit-address bytes `+2..+3`
    // - `0x44b0d0` formats bytes `+4..+7` into dotted-quad text
    // - when requested, it appends `":%d"` using the decoded host-order port
    if (endpoint.family != 2u || endpoint.ipv4NetworkOrder == 0u) {
        return std::string();
    }

    const uint16_t portNetworkOrder = ByteSwap16(helperWord6);
    const uint16_t portHostOrder = ByteSwap16(portNetworkOrder);
    std::string out = FormatIpv4StoredDwordLittleEndianDottedQuad(endpoint.ipv4NetworkOrder);
    if (appendPort) {
        out += fmt::format(":{}", static_cast<unsigned>(portHostOrder));
    }
    return out;
}

// UNUSED: inline uint16_t ReadOpcodePrefixVariableWidth

inline bool TryCallback84FillPair(void* callback84, uint32_t* outLow, uint32_t* outHigh) {
    if (outLow) {
        *outLow = 0u;
    }
    if (outHigh) {
        *outHigh = 0u;
    }
    if (!callback84) {
        return false;
    }

    // Current best callback84 chain:
    // - owner `+0x84` is set by `0x41f1d0` from the deeper startup `arg6->+0x124(...)` triple
    // - natural original then queries callback84 vtable `+0x38` from `0x41de40`
    // - client cross-check tightens that callee as `ClientNetShell +0x38 / 0x62006580`
    // - that wrapper is not self-contained on the callback object:
    //   it re-enters resolved client `ILTLoginMediator.Default`, calls `+0x18c(&blob, 900, 0)`,
    //   then returns pair `(&blob, 0x20)`
    // - launcher-side `+0x18c / 0x41e690` fills:
    //   - current slot id low/high
    //   - caller args
    //   - blob `+0x10..+0x1f` via an in-place 16-byte FeedbackSize transform
    // - the transform contract is now tighter too:
    //   - `0x41e690` seeds blob `+0x10` from owner `+0xf18`
    //   - `0x41e690` then calls mediator `+0xd4`
    //   - `0x41b4f0` is the tiny live-pointer getter `owner +0x1c + 0x85`
    //   - replacement still keeps the older launcher-owned bootstrap sidecar as explicit
    //     fallback glue on that slot when the live connection mirror is absent at runtime
    //   - `0x41df60` constructs a small FeedbackSize adapter around that source
    //   - `0x44b190` dispatches that adapter's configure call
    //   - `0x44b570` then transforms one 16-byte block in place
    //   - newer runtime+string proof now closes the active one-block algorithm enough to test/live-own:
    //     - string anchor `AssemblyTwofish`
    //     - parameter names `IV` + `FeedbackSize`
    //     - zero-IV source at `0x4d4d50`
    //     - two independent natural-original samples matched exactly under a one-block Twofish
    //       ECB/CBC-zero-IV encryption of input `[ownerF18, 0, 0, 0]`
    void** vtable = *reinterpret_cast<void***>(callback84);
    if (!vtable || !vtable[14]) {
        return false;
    }

    using FillPairFn = void(__thiscall*)(void*, uint32_t*, uint32_t*);
    const auto fillPairFn = reinterpret_cast<FillPairFn>(vtable[14]); // vtable +0x38
    fillPairFn(callback84, outLow, outHigh);
    return true;
}

inline bool TryObject88QueryManagedSendMode(void* object88, bool* outManagedSendMode) {
    if (outManagedSendMode) {
        *outManagedSendMode = false;
    }
    if (!object88) {
        return false;
    }

    // Current bounded provenance answer for owner `+0x88` on the active state9 path:
    // - launcher owner init `0x41ee60` zeroes `+0x84/+0x88/+0x8c`
    // - `0x41f1d0` is the concrete launcher-side triple writer
    // - `0x41de40` later only reads `+0x88`
    // - no later launcher-side write to owner `+0x88` is isolated yet inside the bounded active
    //   mediator/state9 scope, so the current best read stays: preserve the startup-provided netMgr
    //   wrapper instead of assuming a later launcher-owned reconstruction step
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

enum class Object88SubmitRoute {
    kUnavailable = 0,
    kDirectSlot28,
    kManagedSlots18_1c_24,
};

struct Object88SubmitPlan {
    Object88SubmitRoute route = Object88SubmitRoute::kUnavailable;
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

inline const char* Object88SubmitRouteName(Object88SubmitRoute route) {
    switch (route) {
        case Object88SubmitRoute::kDirectSlot28:
            return "direct:+0x28";
        case Object88SubmitRoute::kManagedSlots18_1c_24:
            return "managed:+0x1c/+0x18/+0x24";
        default:
            return "unavailable";
    }
}

inline Object88SubmitPlan BuildObject88SubmitPlan(
    void* object88,
    bool callbackPairReady,
    bool submitTargetReady,
    uint32_t forwardedArg90,
    int32_t cachedHandle147c) {
    Object88SubmitPlan plan = {};
    plan.callbackPairReady = callbackPairReady;
    plan.submitTargetReady = submitTargetReady;
    plan.forwardedArg90 = forwardedArg90;
    plan.modeQueryReady = TryObject88QueryManagedSendMode(object88, &plan.managedSendMode);
    if (!plan.modeQueryReady) {
        return plan;
    }

    if (plan.managedSendMode) {
        plan.route = Object88SubmitRoute::kManagedSlots18_1c_24;
        plan.wouldReleaseCachedHandle147c = (cachedHandle147c != -1);
        plan.wouldAcquireManagedHandle18 = true;
        plan.wouldCallManagedSend24 = callbackPairReady && submitTargetReady;
    } else {
        plan.route = Object88SubmitRoute::kDirectSlot28;
        plan.wouldCallDirectSend28 = callbackPairReady && submitTargetReady;
    }
    return plan;
}

inline uint32_t ExecuteObject88Submit(
    void* object88,
    bool managedSendMode,
    int32_t* inOutCachedHandle147c,
    const char* submitTargetText,
    uint32_t callbackOutLow,
    uint32_t callbackOutHigh,
    uint32_t forwardedArg90) {
    if (!object88 || !submitTargetText || !inOutCachedHandle147c) {
        return 1u;
    }

    void** vtable = *reinterpret_cast<void***>(object88);
    if (!vtable) {
        return 1u;
    }

    if (!managedSendMode) {
        if (!vtable[10]) {
            return 1u;
        }
        using DirectSubmitFn = uint32_t(__thiscall*)(void*, const char*, uint32_t, uint32_t, uint32_t);
        const auto directSubmitFn = reinterpret_cast<DirectSubmitFn>(vtable[10]); // +0x28
        return directSubmitFn(object88, submitTargetText, callbackOutLow, callbackOutHigh, forwardedArg90);
    }

    if (*inOutCachedHandle147c != -1) {
        if (!vtable[7]) {
            return 1u;
        }
        using ReleaseHandleFn = void(__thiscall*)(void*, int32_t);
        const auto releaseHandleFn = reinterpret_cast<ReleaseHandleFn>(vtable[7]); // +0x1c
        releaseHandleFn(object88, *inOutCachedHandle147c);
    }

    if (!vtable[6] || !vtable[9]) {
        return 1u;
    }

    using AcquireHandleFn = uint32_t(__thiscall*)(void*, uint32_t);
    using ManagedSubmitFn = uint32_t(__thiscall*)(void*, uint32_t, const char*, uint32_t, uint32_t, uint32_t);
    const auto acquireHandleFn = reinterpret_cast<AcquireHandleFn>(vtable[6]); // +0x18
    const auto managedSubmitFn = reinterpret_cast<ManagedSubmitFn>(vtable[9]); // +0x24
    const uint32_t acquiredHandle = acquireHandleFn(object88, 0u);
    *inOutCachedHandle147c = static_cast<int32_t>(acquiredHandle);
    return managedSubmitFn(
        object88,
        acquiredHandle,
        submitTargetText,
        callbackOutLow,
        callbackOutHigh,
        forwardedArg90);
}

}  // namespace mxo::ltlogin::state9submit_scaffold
