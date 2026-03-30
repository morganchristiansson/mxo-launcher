#include "loginmediator.h"

#include <spdlog/spdlog.h>

namespace mxo::ltlogin {

uint8_t CLTLoginMediator::CurrentCharacterRouteIndexCc8Scaffold() const {
    // The current source still carries two bounded mirrors for original owner byte `+0xcc8`:
    // - `state8SelectionContextSnapshotState_` for the earlier state3(wait)->state8 path
    // - `postAuthMarginLoadingState_` for the later post-AS_AuthReply path
    // Fidelity correction:
    // - startup defaults already populate world/variant-selection tables before auth reply
    // - so world/descriptor count alone is not a safe discriminator here
    // - on the live existing-character path, state4/state6/state8 margin work before
    //   `AS_AuthReply` still needs the earlier selection-context snapshot index (slot `0x05` in the
    //   current log family), not the later post-auth mirror defaulting to `0`
    if ((!lastAuthReply_.valid || lastAuthReply_.isErrorReply) && selectionContext0ecCopyValid_) {
        return state8SelectionContextSnapshotState_.slotOrSelectionIndexCc8;
    }
    return postAuthMarginLoadingState_.characterRouteIndexCc8;
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

    // Current bounded bridge for the still-unrecovered earlier producer of owner
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
uint32_t CLTLoginMediator::BeginMarginConnectionScaffold(const char* routeHostText, uint8_t cachedRouteSelector) {
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
    // Current bounded bridge correction:
    // - the active existing-character state8 -> state4 path reaches here directly, not through the
    //   outer `BeginLauncherMarginConnectionScaffold()` wrapper
    // - so this helper itself must ensure the launcher bridge context/sidecar connection is
    //   present before `EnsureConnected()` re-enters the engine connect slot with the connection
    //   object as context key
    if (engine_ != nullptr) {
        CLTLoginMediatorConnectionContextScaffold* context =
            engine_->EnsureLauncherConnectionContextScaffold(
                &marginConnectionContextScaffold_,
                this,
                "MarginConnection",
                /*isMarginConnection=*/true);
        if (context) {
            context->peerCloseQueued = false;
            SetMarginConnectionContextKey(context);
        }
    }

    mxo::liblttcp::CMessageConnection* connection = EnsureMarginConnectionObject();
    if (marginConnectionContextScaffold_ != nullptr) {
        marginConnectionContextScaffold_->sidecarConnection = connection;
    }
    if (!connection) {
        spdlog::warn("CLTLoginMediator::BeginMarginConnectionScaffold failed to allocate margin connection");
        return 0u;
    }

    marginConnectionFlag2d_ = 0;

    const bool shouldRefreshRouteState = (cachedRouteSelector == 0u);
    const bool needsBoundedNonZeroSelectorMaterialization =
        (cachedRouteSelector != 0u) && (marginEndpoint_.ipv4NetworkOrder == 0u);
    if (shouldRefreshRouteState || needsBoundedNonZeroSelectorMaterialization) {
        if (!routeHostText || routeHostText[0] == '\0') {
            spdlog::debug(
                "CLTLoginMediator::BeginMarginConnectionScaffold selector=0x{:02x} requires routeHostText but received <empty>",
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
            spdlog::debug("CLTLoginMediator::BeginMarginConnectionScaffold has no resolved margin host");
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
        cachedRouteSelector,
        marginBeginCount24_,
        marginEndpoint_.ipv4NetworkOrder,
        marginServerPortHostOrder_,
        result);
    if (result == 0u) {
        spdlog::debug(
            "CLTLoginMediator::BeginMarginConnectionScaffold connect failed host='{}' port={} ip=0x{:08x} selector={} beginCount={}",
            marginHost.empty() ? std::string("<empty>") : marginHost,
            marginServerPortHostOrder_,
            marginEndpoint_.ipv4NetworkOrder,
            cachedRouteSelector,
            marginBeginCount24_);
    }
    return result;
}

// anchor: launcher.exe:0x41f2e0
const SlotRecordState004b5328* CLTLoginMediator::GetSlotRecordByIndex(uint8_t slotIndex) const {
    if (slotIndex >= slotRecordValid688_.size() || !slotRecordValid688_[slotIndex]) {
        return nullptr;
    }
    return &slotRecords688_[slotIndex];
}

// anchor: launcher.exe:0x41f300
const SlotRecordState004b5328* CLTLoginMediator::GetCurrentSlotRecord() const {
    return GetSlotRecordByIndex(CurrentCharacterRouteIndexCc8Scaffold());
}

// anchor: launcher.exe:0x41b220
const char* CLTLoginMediator::LookupSlotRecordHeapStringByIndex(uint8_t slotIndex) const {
    const SlotRecordState004b5328* record = GetSlotRecordByIndex(slotIndex);
    if (!record || record->heapString14.empty()) {
        return nullptr;
    }
    return record->heapString14.c_str();
}

// anchor: launcher.exe:0x41f2c0
RouteDescriptor30SmallStringLikeSketch* CLTLoginMediator::GetRouteDescriptor30() {
    // Keep the wrapper-facing arg6 `+0x10c` small-string object explicit.
    // The owner-side route-text resolution still lives in `ResolveMarginRouteDescriptor()`.
    const char* routeDescriptor = ResolveMarginRouteDescriptor();
    routeDescriptor30Owned_ = routeDescriptor ? routeDescriptor : "";
    routeDescriptor30_.begin = routeDescriptor30Owned_.c_str();
    routeDescriptor30_.current = routeDescriptor30_.begin + routeDescriptor30Owned_.size();
    routeDescriptor30_.capacity = routeDescriptor30_.current;

    spdlog::info(
        "CLTLoginMediator::GetRouteDescriptor30(+0x10c) -> begin={} current={} text='{}'",
        fmt::ptr(routeDescriptor30_.begin),
        fmt::ptr(routeDescriptor30_.current),
        routeDescriptor30Owned_.empty() ? "<empty>" : routeDescriptor30Owned_.c_str());
    return &routeDescriptor30_;
}

// anchor: launcher.exe:0x41b260
const char* CLTLoginMediator::LookupRouteHostPrefixBySlot(uint8_t slotIndex) const {
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
