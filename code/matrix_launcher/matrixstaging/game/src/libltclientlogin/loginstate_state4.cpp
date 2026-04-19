#include "loginstate.h"
#include "loginmediator.h"

#include <spdlog/spdlog.h>

namespace mxo::ltlogin {
namespace {

// Convert Windows/WSA error codes to human-readable strings for logging
static const char* WindowsErrorToString(uint32_t errorCode) {
    switch (errorCode) {
        case 0:
            return "SUCCESS";
        case 10004:
            return "WSAEINTR";
        case 10009:
            return "WSAEBADF";
        case 10013:
            return "WSAEACCES";
        case 10014:
            return "WSAEFAULT";
        case 10022:
            return "WSAEINVAL";
        case 10024:
            return "WSAEMFILE";
        case 10035:
            return "WSAEWOULDBLOCK";
        case 10036:
            return "WSAEINPROGRESS";
        case 10037:
            return "WSAEALREADY";
        case 10038:
            return "WSAENOTSOCK";
        case 10039:
            return "WSAEDESTADDRREQ";
        case 10040:
            return "WSAEMSGSIZE";
        case 10041:
            return "WSAEPROTOTYPE";
        case 10042:
            return "WSAENOPROTOOPT";
        case 10043:
            return "WSAEPROTONOSUPPORT";
        case 10044:
            return "WSAESOCKTNOSUPPORT";
        case 10045:
            return "WSAEOPNOTSUPP";
        case 10046:
            return "WSAEPFNOSUPPORT";
        case 10047:
            return "WSAEAFNOSUPPORT";
        case 10048:
            return "WSAEADDRINUSE";
        case 10049:
            return "WSAEADDRNOTAVAIL";
        case 10050:
            return "WSAENETDOWN";
        case 10051:
            return "WSAENETUNREACH";
        case 10052:
            return "WSAENETRESET";
        case 10053:
            return "WSAECONNABORTED";
        case 10054:
            return "WSAECONNRESET";
        case 10055:
            return "WSAENOBUFS";
        case 10056:
            return "WSAEISCONN";
        case 10057:
            return "WSAENOTCONN";
        case 10058:
            return "WSAESHUTDOWN";
        case 10059:
            return "WSAETOOMANYREFS";
        case 10060:
            return "WSAETIMEDOUT";
        case 10061:
            return "WSAECONNREFUSED";
        case 10062:
            return "WSAELOOP";
        case 10063:
            return "WSAENAMETOOLONG";
        case 10064:
            return "WSAEHOSTDOWN";
        case 10065:
            return "WSAEHOSTUNREACH";
        case 10066:
            return "WSAENOTEMPTY";
        case 10067:
            return "WSAEPROCLIM";
        case 10068:
            return "WSAEUSERS";
        case 10069:
            return "WSAEDQUOT";
        case 10070:
            return "WSAESTALE";
        case 10071:
            return "WSAEREMOTE";
        default:
            return nullptr;  // Unknown error, caller should use hex format
    }
}

static uint32_t BeginMarginConnectionForState4Case(
    CLTLoginMediator* mediator,
    const char* routeHostText,
    uint8_t cachedRouteSelector) {
    if (!mediator) {
        return 0u;
    }
    return mediator->BeginMarginConnection(routeHostText, cachedRouteSelector);
}

}  // namespace

// anchor: launcher.exe vtable 0x004b503c
const char* CLTLoginState_State4_0x4b503c::DebugName() const {
    return "CLTLoginState_State4_0x4b503c";
}

// anchor: launcher.exe:0x004393f0 (vtable 0x004b503c slot 2)
uint32_t CLTLoginState_State4_0x4b503c::Slot2_HandleSecondaryGate(void* workItem) {
    CLTLoginMediator* mediator = g_CurrentLoginMediator;
    if (!workItem || !mediator) {
        return 0u;
    }

    // anchor: launcher.exe:0x4393fa-0x439401 - call workItem->GetWorkType() == 2?
    const auto* workItemHeader =
        static_cast<const mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768_WorkItemHeader*>(workItem);
    if (workItemHeader->workType !=
        mxo::liblttcp::CLTThreadPerClientTCPEngine_0x4b2768::kWorkTypeConnectionStatus) {
        return CLTLoginState::Slot2_HandleSecondaryGate(workItem);
    }

    // anchor: launcher.exe:0x439411-0x439420 - call GetStatusOrPayloadDword() twice (faithful to original)
    // First call: store to mediator->worldListCountOrStatus80
    const auto* workItemPayload = static_cast<const uint32_t*>(workItem);
    const uint32_t statusFirst = workItemPayload[2];  // offset 0x8
    mediator->worldListCountOrStatus80 = statusFirst;
    // Second call: test for zero (original calls the getter again, faithful to 0x439420)
    const uint32_t status = workItemPayload[2];  // offset 0x8, same as first call

    if (status != 0u) {
        mediator->marginConnectionFlag2d_ = 1;
        if (mediator->marginBeginCount24_ < static_cast<uint32_t>(mediator->marginAddressList3c_.Count())) {
            Slot3_BeginOrContinue(cachedUpstreamOrArg_0x4);
            const char* errorName = WindowsErrorToString(status);
            spdlog::info(
                "CLTLoginState_State4_0x4b503c::Slot2_HandleSecondaryGate non-zero status=0x{:08x}{} cachedUpstream={} attemptCount24={} candidateCount={} owner+0x2d=1 -> retry slot3",
                static_cast<unsigned>(status),
                errorName ? std::string(" (") + errorName + std::string(")") : "",
                fmt::ptr(cachedUpstreamOrArg_0x4),
                static_cast<unsigned>(mediator->marginBeginCount24_),
                static_cast<unsigned>(mediator->marginAddressList3c_.Count()));
            return 1u;
        }

        const uint32_t nextHelperStateId = RecoverCachedUpstreamPhaseCode(cachedUpstreamOrArg_0x4);
        mediator->ResetMarginConnectAttemptCountScaffold();
        if (nextHelperStateId != 13u) {
            (void)mediator->SetCurrentState(3u);
        }
        mediator->PostError(6u);
        const char* errorName = WindowsErrorToString(status);
        spdlog::info(
            "CLTLoginState_State4_0x4b503c::Slot2_HandleSecondaryGate non-zero status=0x{:08x}{} retry exhausted cachedUpstream={} upstreamPhaseCode={} -> currentState={} then PostError(0x06)",
            static_cast<unsigned>(status),
            errorName ? std::string(" (") + errorName + std::string(")") : "",
            fmt::ptr(cachedUpstreamOrArg_0x4),
            static_cast<unsigned>(nextHelperStateId),
            mediator->currentState_ ? mediator->currentState_->DebugName() : "<null>");
        return 1u;
    }

    // Ghidra/disassembly recheck for `0x439495..0x4394c8`:
    // - read cached upstream from `this+4`
    // - call cached upstream vtable `+0x18`
    // - clear `this+4 = 0`
    // - write owner `+0x104 = -1`
    // - switch helper through `0x41b450`
    // - post event `0x0e`
    const uint32_t nextHelperStateId = RecoverCachedUpstreamPhaseCode(cachedUpstreamOrArg_0x4);
    cachedUpstreamOrArg_0x4 = nullptr;
    mediator->marginRouteState_.currentWorldId = -1;
    const uint32_t switchDispatchResult = mediator->SetCurrentState(nextHelperStateId);
    mediator->PostEvent(0x0eu);
    const char* errorName = WindowsErrorToString(status);
    spdlog::info(
        "CLTLoginState_State4_0x4b503c::Slot2_HandleSecondaryGate status=0x{:08x}{} cachedUpstreamPhaseCode={} -> currentState={} switchDispatchResult=0x{:08x} owner+0x104=-1 then PostEvent(0x0e)",
        static_cast<unsigned>(status),
        errorName ? std::string(" (") + errorName + std::string(")") : "",
        static_cast<unsigned>(nextHelperStateId),
        mediator->currentState_ ? mediator->currentState_->DebugName() : "<null>",
        static_cast<unsigned>(switchDispatchResult));
    return 1u;
}

// anchor: launcher.exe:0x00439300 (vtable 0x004b503c slot 3)
void CLTLoginState_State4_0x4b503c::Slot3_BeginOrContinue(CLTLoginState* upstreamOrArg) {
    CLTLoginMediator* mediator = g_CurrentLoginMediator;
    if (!mediator) {
        return;
    }

    // Faithfulness/ownership correction from the fresh `0x439300` disassembly review:
    // - `0x439300` belongs to `CLTLoginState_State4_0x4b503c` vtable `0x004b503c` slot 3
    // - this object caches the first incoming upstream/helper pointer at `this+4`
    // - it then calls that cached object's vtable `+0x18` and uses the returned phase/state code
    //   for the real case split
    // - only the narrow owner-side route getters and `0x41e500` transport/init stay on the
    //   mediator
    if (cachedUpstreamOrArg_0x4 == nullptr) {
        cachedUpstreamOrArg_0x4 = upstreamOrArg;
    }

    const uint32_t upstreamPhaseCode = RecoverCachedUpstreamPhaseCode(cachedUpstreamOrArg_0x4);
    switch (upstreamPhaseCode) {
        case 6: {
            BeginMarginConnectionForState4Case(
                mediator,
                mediator->ResolveMarginRouteDescriptor(),
                0u);
            return;
        }

        case 7:
        case 8:
        case 13: {
            // Exact `0x439300 -> 0x41e500` consequence to preserve:
            // - this branch forwards owner byte `+0xcc8` as arg2
            // - `0x41e500` only refreshes route/address state on `arg2 == 0`
            // - so on the live state8/state13 continuation path the returned route-text pointer is
            //   forwarded even when current source still has no populated route-string table entry
            BeginMarginConnectionForState4Case(
                mediator,
                mediator->ResolveMarginRouteFromCurrentCharacterSlot(),
                mediator->CurrentCharacterRouteIndexCc8Scaffold());
            return;
        }

        case 10: {
            BeginMarginConnectionForState4Case(
                mediator,
                mediator->ResolveMarginRouteFromDescriptorIndex(
                    mediator->postAuthMarginLoadingState_0xf14.createCharacterData108.selectedWorldField24),
                static_cast<uint8_t>(
                    mediator->postAuthMarginLoadingState_0xf14.createCharacterData108.selectedWorldField24 & 0xffu));
            return;
        }

        default: {
            // Current source-owned mirror for the default branch's owner `+0x104` dword remains
            // `marginRouteState_.currentWorldId`; keep the field meaning provisional and
            // only preserve the original `!= -1 -> owner vtable +0xfc -> if non-null call 0x41e500`
            // structure here.
            const int32_t field104Value = mediator->marginRouteState_.currentWorldId;
            if (field104Value == -1) {
                return;
            }
            const char* const routeHostText =
                mediator->ResolveMarginRouteFromWorldId(static_cast<uint32_t>(field104Value));
            if (routeHostText == nullptr) {
                return;
            }
            BeginMarginConnectionForState4Case(
                mediator,
                routeHostText,
                0u);
        }
    }
}

// anchor: launcher.exe:0x00439190 (vtable 0x004b503c slot 6)
uint32_t CLTLoginState_State4_0x4b503c::Slot6_HandleSecondaryMessage(mxo::liblttcp::CMessageConnectionMessageRef_0x4ba23c* workItem) {
    (void)workItem;
    return 0;
}

// anchor: launcher.exe:0x004686b0 (vtable 0x004b503c slot 7)
uint32_t CLTLoginState_State4_0x4b503c::GetStateId() const {
    return 4;
}

}  // namespace mxo::ltlogin
