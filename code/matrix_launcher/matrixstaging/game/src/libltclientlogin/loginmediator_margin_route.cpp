#include "loginmediator.h"
#include "loginstate.h"

#include <spdlog/spdlog.h>

namespace mxo::ltlogin {

uint8_t CLTLoginMediator::CurrentCharacterRouteIndexCc8Scaffold() const {
    // anchor relation: launcher.exe:0x41f300 / owner vtable +0x44
    // Owner `+0x44` reads the embedded selection-route subobject byte directly and forwards it
    // into owner `+0x40`.
    return selectionRouteState684_.CurrentSlotOrSelectionIndex644();
}

void CLTLoginMediator::SetCurrentCharacterRouteIndexCc8Scaffold(uint8_t slotIndex) {
    selectionRouteState684_.SetCurrentSlotOrSelectionIndex644(slotIndex);
    postAuthMarginLoadingState_0xf14.characterRouteIndexCc8 = slotIndex;
    marginRouteState_.currentCharacterOrRouteIndex = slotIndex;
}

const char* CLTLoginMediator::ResolveMarginRouteFromCurrentCharacterSlot() const {
    // Address anchors:
    // - launcher.exe:0x439300 case `7/8/0xd`
    // - owner byte `+0xcc8`
    // - owner vtable `+0xe0`
    //
    // Current best source-owned mirror returns the reconstructed route-host string triple's first
    // string for the current owner `+0xcc8` slot/index.
    if (const char* routeHost = LookupRouteHostPrefixBySlot(CurrentCharacterRouteIndexCc8Scaffold())) {
        return routeHost;
    }

    // Current bounded stand-in for the still-unrecovered earlier producer of owner
    // `+0x30/+0x3c/+0x6c` on the existing-character path:
    // when the per-slot route table is not populated yet, reuse the launcher-selected route host
    // prefix already mirrored on the owner.
    return marginRouteState_.routeHostPrefix.empty() ? nullptr : marginRouteState_.routeHostPrefix.c_str();
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
    if (const char* currentSlotRouteHost = LookupRouteHostPrefixBySlot(CurrentCharacterRouteIndexCc8Scaffold())) {
        return currentSlotRouteHost;
    }
    return marginRouteState_.routeHostPrefix.empty() ? nullptr : marginRouteState_.routeHostPrefix.c_str();
}

// anchor: launcher.exe:0x41e500
uint32_t CLTLoginMediator::BeginMarginConnection(const char* routeHostText, uint8_t cachedRouteSelector) {
    // Narrow reusable transport/init helper kept on the mediator after moving the `0x439300`
    // case split back into `CLTLoginState_State4::Slot3_BeginOrContinue`.
    //
    // Current best recovered `0x41e500` shape:
    // - allocate/configure a margin-specific connection object at owner `+0x1c`
    // - clear owner byte `+0x2d`
    // - if arg2 == 0, refresh owner `+0x30`, rebuild owner `+0x3c`, select owner `+0x7c`,
    //   and materialize owner `+0x6c`
    // - increment owner dword `+0x24`
    // - clear owner `+0x7c`
    // - call `connection->+0x1c(owner+0x6c)`
    //
    // Fidelity improvement: sync from globals when instance fields are empty
    // This removes need for infidel SetMarginServerConfig scaffold methods.
    // The original reads globals directly (anchor: launcher.exe:0x4f7b14 equivalent).
    if (marginServerDnsSuffix_.empty() && g_marginServerDNSName && g_marginServerDNSName[0]) {
        marginServerDnsSuffix_ = g_marginServerDNSName;
    }
    if (marginServerPortHostOrder_ == 0u && g_marginServerPort != 0u) {
        marginServerPortHostOrder_ = g_marginServerPort;
    }

    // Current bounded active-path correction:
    // - the existing-character state8 -> state4 path reaches here directly
    // - keep that path connection-centric: `EnsureConnected()` re-enters the engine connect slot
    //   with the direct margin connection object as the context key
    marginPeerCloseQueuedScaffold_ = false;

    mxo::liblttcp::CMessageConnection_0x4b7928* connection = EnsureMarginConnectionObject();
    if (!connection) {
        spdlog::warn("CLTLoginMediator::BeginMarginConnection failed to allocate margin connection");
        return 0u;
    }

    marginConnectionFlag2d_ = 0;

    const bool shouldRefreshRouteState = (cachedRouteSelector == 0u);
    const bool needsBoundedNonZeroSelectorMaterialization =
        (cachedRouteSelector != 0u) && (marginEndpoint_.ipv4NetworkOrder == 0u);
    if (shouldRefreshRouteState || needsBoundedNonZeroSelectorMaterialization) {
        if (!routeHostText || routeHostText[0] == '\0') {
            spdlog::debug(
                "CLTLoginMediator::BeginMarginConnection selector=0x{:02x} requires routeHostText but received <empty>",
                cachedRouteSelector);
            return 0u;
        }

        // `0x41e500` only refreshes owner `+0x30/+0x3c/+0x6c` on the exact `arg2 == 0` path.
        // The extra `selector != 0 && endpoint still zero` branch below is a bounded source-owned
        // stand-in for the still-unrecovered earlier producer that should already have
        // materialized owner `+0x6c` before the non-zero-selector state4/state8 path reaches here.
        if (shouldRefreshRouteState || marginRouteState_.routeHostPrefix.empty()) {
            marginRouteState_.routeHostPrefix = routeHostText;
        }

        const std::string marginHost = ResolvedMarginHostName();
        if (marginHost.empty()) {
            spdlog::debug("CLTLoginMediator::BeginMarginConnection has no resolved margin host");
            return 0u;
        }

        const bool routeChanged = (marginAddressListResolvedHostName3c_ != marginHost);
        if (routeChanged || marginAddressList3c_.Empty()) {
            if (!RebuildMarginAddressList()) {
                return 0u;
            }
        }

        if (marginSelectedIpv4_7c_ == 0u && !SelectMarginEndpointIpv4()) {
            spdlog::warn(
                "CLTLoginMediator::BeginMarginConnection found no IPv4 candidates for '{}'",
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
    connection->remoteEndpoint_ = marginEndpoint_;

    const uint32_t result = connection->EnsureConnected();
    spdlog::info(
        "CLTLoginMediator::BeginMarginConnection resolvedHost='{}' routeHostText='{}' selector=0x{:02x} beginCount={} selectedIpv4=0x{:08x} port={} ensureConnectedResult=0x{:08x}",
        marginHost.empty() ? std::string("<empty>") : marginHost,
        (routeHostText && routeHostText[0]) ? std::string(routeHostText) : std::string("<empty>"),
        cachedRouteSelector,
        marginBeginCount24_,
        marginEndpoint_.ipv4NetworkOrder,
        marginServerPortHostOrder_,
        result);
    if (result == 0u) {
        spdlog::debug(
            "CLTLoginMediator::BeginMarginConnection connect failed host='{}' port={} ip=0x{:08x} selector={} beginCount={}",
            marginHost.empty() ? std::string("<empty>") : marginHost,
            marginServerPortHostOrder_,
            marginEndpoint_.ipv4NetworkOrder,
            cachedRouteSelector,
            marginBeginCount24_);
    }
    return result;
}

// anchor: launcher.exe:0x41f2e0 / owner vtable +0x40
// Original body: direct field access without validation
const SlotRecordState_0x4b5328* CLTLoginMediator::GetSlotRecordByIndex(uint8_t slotIndex) const {
    if (slotIndex != 0xffu) {
        return &selectionRouteState684_.slotRecordTable04_[slotIndex];
    }
    return nullptr;
}

// anchor: launcher.exe:0x41f300 / owner vtable +0x44
// Original body: direct field access
const SlotRecordState_0x4b5328* CLTLoginMediator::GetCurrentSlotRecord() const {
    const uint8_t currentSlot = selectionRouteState684_.currentSlotOrSelectionIndex644_;
    if (currentSlot != 0xffu) {
        return &selectionRouteState684_.slotRecordTable04_[currentSlot];
    }
    return nullptr;
}

// anchor: launcher.exe:0x41b220
// Original body: calls GetStateId(), checks result > 2, directly indexes slotRecordTable04[slotIndex], returns *(char**)(ptr + 0x14)
const char* CLTLoginMediator::LookupSlotRecordHeapStringByIndex(uint8_t slotIndex) const {
    if (!currentState_) {
        return nullptr;
    }
    const uint32_t stateId = currentState_->GetStateId();
    if (stateId <= 2u) {
        return nullptr;
    }
    if (slotIndex >= 100u) {
        return nullptr;
    }
    const SlotRecordState_0x4b5328* record = &selectionRouteState684_.slotRecordTable04_[slotIndex];
    if (!record) {
        return nullptr;
    }
    return record->heapString14;
}

// anchor: launcher.exe:0x41b260
// Original body calls currentState_->GetStateId(), checks result > 2, then accesses field
const char* CLTLoginMediator::LookupRouteHostPrefixBySlot(uint8_t slotIndex) const {
    if (!currentState_) {
        return nullptr;
    }
    const uint32_t stateId = currentState_->GetStateId();
    if (stateId <= 2u) {
        return nullptr;
    }
    if (slotIndex >= 100u) {
        return nullptr;
    }
    const RouteHostStringTripleState& slot = selectionRouteState684_.routeHostStringTriples194_[slotIndex];
    return slot.BeginOrNull();
}

// anchor: launcher.exe:0x41b2a0
// Original body calls currentState_->GetStateId(), checks result > 2, then accesses field
uint8_t CLTLoginMediator::GetSlotRecordStatusByIndex(uint8_t slotIndex) const {
    if (!currentState_) {
        return 7u;
    }
    const uint32_t stateId = currentState_->GetStateId();
    if (stateId <= 2u) {
        return 7u;
    }
    if (slotIndex >= 100u) {
        return 7u;
    }
    const SlotRecordState_0x4b5328* record = GetSlotRecordByIndex(slotIndex);
    return record ? record->status3a : 7u;
}

// anchor: launcher.exe:0x41b2e0
const char* CLTLoginMediator::GetDescriptorInlineNameByIndex(uint8_t slotIndex) const {
    if (slotIndex >= worldDescriptorValidD84_.size() || !worldDescriptorValidD84_[slotIndex]) {
        return nullptr;
    }
    const WorldDescriptorState_0x4b533c& slot = worldDescriptorsD84_[slotIndex];
    return slot.inlineNamePlus03.empty() ? nullptr : slot.inlineNamePlus03.c_str();
}

// anchor: launcher.exe:0x41b320
// Ghidra/disassembly correction:
// - this owner reader returns descriptor payload byte `+0x17` (Status)
// - earlier docs/source had an off-by-one stale guess that put Type here
uint8_t CLTLoginMediator::GetDescriptorStatusByIndex(uint8_t slotIndex) const {
    if (slotIndex >= worldDescriptorValidD84_.size() || !worldDescriptorValidD84_[slotIndex]) {
        return 0;
    }
    return worldDescriptorsD84_[slotIndex].status17;
}

// anchor: launcher.exe:0x41b360
// Ghidra/disassembly correction:
// - this owner reader returns descriptor payload byte `+0x18` (Type)
// - earlier docs/source had an off-by-one stale guess that put server-version low byte here
uint8_t CLTLoginMediator::GetDescriptorTypeByIndex(uint8_t slotIndex) const {
    if (slotIndex >= worldDescriptorValidD84_.size() || !worldDescriptorValidD84_[slotIndex]) {
        return 0;
    }
    return worldDescriptorsD84_[slotIndex].type18;
}

// anchor: launcher.exe:0x41b3a0
uint8_t CLTLoginMediator::GetDescriptorPopulationNibbleByIndex(uint8_t slotIndex) const {
    if (slotIndex >= worldDescriptorValidD84_.size() || !worldDescriptorValidD84_[slotIndex]) {
        return 0;
    }
    const uint8_t value = worldDescriptorsD84_[slotIndex].populationLevel1f & 0x0f;
    return (value >= 1u && value <= 3u) ? value : 0u;
}

}  // namespace mxo::ltlogin
