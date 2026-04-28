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
    // when the per-slot route table is not populated yet, reuse the owner `+0x30`
    // route-descriptor string directly.
    return routeDescriptor30_.BeginOrNull();
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

// anchor: launcher.exe:0x41e500
uint32_t CLTLoginMediator::BeginMarginConnection(const char* routeHostText, uint8_t cachedRouteSelector) {
    // Narrow reusable transport/init helper kept on the mediator after moving the `0x439300`
    // case split back into `CLTLoginState_State4_0x4b503c::Slot3_BeginOrContinue`.
    //
    // Current best recovered `0x41e500` shape:
    // - allocate/configure a margin-specific connection object at owner `+0x1c`
    // - clear owner byte `+0x2d`
    // - if arg2 == 0, refresh owner `+0x30`, rebuild owner `+0x3c`, select owner `+0x7c`,
    //   and materialize owner `+0x6c`
    // - increment owner dword `+0x24`
    // - clear owner `+0x7c`
    // - call `connection->+0x1c(owner++0x6c)`

    // Fidelity correction from `0x41e500`:
    // - arg1 is compared against owner `+0x30`
    // - on change, owner `+0x30` is reassigned from that caller-supplied route text
    // - the later address-list rebuild path then consumes owner `+0x30`, not a sidecar prefix
    // Use global port for endpoint construction.
    const uint16_t portHostOrder = g_marginServerPort != 0u ? g_marginServerPort : marginServerPortHostOrder_;

    // anchor: launcher.exe:0x41e500 / margin connection allocation and initialization
    auto* connection = new mxo::liblttcp::CMarginConnection_0x4aff38(engine_);
    if (!connection) {
        marginConnection_ = nullptr;
        spdlog::warn("CLTLoginMediator::BeginMarginConnection failed to allocate margin connection");
        return 0u;
    }
    connection->SetOwnerContext(this);
    connection->ConfigurePacketNameFamily(
        mxo::liblttcp::CMessageConnectionPacketNameFamily::kMargin,
        /*packetizedMessagesEnabled=*/true);
    marginConnection_ = connection;

    marginConnectionFlag2d_ = 0;

    std::string marginHost;
    if (cachedRouteSelector == 0u) {
        // `0x41e500` exact route refresh shape:
        // - compare caller arg1 against owner `+0x30`
        // - when different, assign `[arg1, terminator)` into owner `+0x30`
        // - rebuild owner `+0x3c` from owner `+0x30` plus the global suffix
        const char* const newRouteDescriptor = routeHostText ? routeHostText : "";
        if (_stricmp(routeDescriptor30_.begin ? routeDescriptor30_.begin : "", newRouteDescriptor) != 0) {
            const char* sourceEnd = newRouteDescriptor;
            while (*sourceEnd != '\0') {
                ++sourceEnd;
            }
            routeDescriptor30_.AssignFromRange(newRouteDescriptor, sourceEnd);
        }

        if (const char* const routeDescriptorBegin = routeDescriptor30_.BeginOrNull();
            routeDescriptorBegin != nullptr && g_marginServerDNSName && g_marginServerDNSName[0]) {
            marginHost = std::string(routeDescriptorBegin) + g_marginServerDNSName;
        }
        if (marginHost.empty()) {
            spdlog::debug("CLTLoginMediator::BeginMarginConnection has no resolved margin host");
            return 0u;
        }

        const bool routeChanged = (marginAddressListResolvedHostName3c_ != marginHost);
        if (routeChanged || marginAddressList3c_.Empty()) {
            marginAddressListResolvedHostName3c_ = marginHost;
            uint32_t flags = mxo::liblttcp::CLTIPAddressList::kFlagShuffle;
            if (ignoreHostsFileForMargin_) {
                flags |= mxo::liblttcp::CLTIPAddressList::kFlagIgnoreHostsFile;
            }
            if (!marginAddressList3c_.Reinit(marginHost.c_str(), flags)) {
                return 0u;
            }
        }

        if (marginSelectedIpv4_7c_ == 0u) {
            marginSelectedIpv4_7c_ = marginAddressList3c_.GetNextAddress(/*wrap=*/true);
        }
        if (marginSelectedIpv4_7c_ == 0u) {
            spdlog::warn(
                "CLTLoginMediator::BeginMarginConnection found no IPv4 candidates for '{}'",
                marginHost);
            return 0u;
        }
        marginEndpoint_.family = 2;
        marginEndpoint_.portNetworkOrder =
            static_cast<uint16_t>((portHostOrder << 8) | (portHostOrder >> 8));
        marginEndpoint_.ipv4NetworkOrder = marginSelectedIpv4_7c_;
    } else if (const char* const routeDescriptorBegin = routeDescriptor30_.BeginOrNull();
               routeDescriptorBegin != nullptr && g_marginServerDNSName && g_marginServerDNSName[0]) {
        marginHost = std::string(routeDescriptorBegin) + g_marginServerDNSName;
    }

    ++marginBeginCount24_;
    marginSelectedIpv4_7c_ = 0u;

    // Original does NOT call SetRemoteHostName - only sets endpoint
    connection->remoteEndpoint_ = marginEndpoint_;

    // Call Connect via vtable+0x1c (same as BeginAuthConnection)
    const uint32_t result = connection->Connect(marginEndpoint_);
    spdlog::info(
        "CLTLoginMediator::BeginMarginConnection resolvedHost='{}' routeHostText='{}' selector=0x{:02x} beginCount={} selectedIpv4=0x{:08x} port={} ensureConnectedResult=0x{:08x}",
        marginHost.empty() ? std::string("<empty>") : marginHost,
        (routeHostText && routeHostText[0]) ? std::string(routeHostText) : std::string("<empty>"),
        cachedRouteSelector,
        marginBeginCount24_,
        marginEndpoint_.ipv4NetworkOrder,
        portHostOrder,
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
    const Packet_MsClaimCharacterNameReply_0x4b5328* record = &selectionRouteState684_.slotRecordTable04_[slotIndex];
    if (!record) {
        return nullptr;
    }
    return record->debugString14;
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
    const StringTriple_0x403f90& slot = selectionRouteState684_.routeHostStringTriples194_[slotIndex];
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
    const Packet_MsClaimCharacterNameReply_0x4b5328* record =
        const_cast<CLTLoginMediator*>(this)->GetAuthReplyPacketByIndex40(
            static_cast<uint32_t>(slotIndex));
    return record ? record->packetType1a : 7u;
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
