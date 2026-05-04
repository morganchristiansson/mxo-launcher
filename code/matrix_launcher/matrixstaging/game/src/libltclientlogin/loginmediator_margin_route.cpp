#include "loginmediator.h"
#include "loginstate.h"

#include <spdlog/spdlog.h>

namespace mxo::ltlogin {
namespace {

}  // namespace

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
        // - build a temporary concatenated string through the `0x403f20 / 0x4043b0` helper family
        const char* const newRouteDescriptor = routeHostText ? routeHostText : "";
        const char* const currentRouteDescriptor = routeDescriptor30_.empty() ? "" : routeDescriptor30_.data();
        if (_stricmp(currentRouteDescriptor, newRouteDescriptor) != 0) {
            const char* sourceEnd = newRouteDescriptor;
            while (*sourceEnd != '\0') {
                ++sourceEnd;
            }
            routeDescriptor30_.assign(newRouteDescriptor, sourceEnd);
            routeDescriptor30_.push_back('\0');
        }

        // Original reads global pointer slot `DAT_004d6814`, materializes it through
        // `0x403f20`, then concatenates `owner+0x30` with that global string through
        // `0x4043b0` before calling `CLTIPAddressList_Reinit`.
        //
        // Replacement fidelity correction:
        // - the recovered launcher config path exposes that global as the margin DNS suffix
        //   (`MarginServerDNSSuffix`), mirrored here as `g_marginServerDNSName`
        // - public-server flow needs `routePrefix + marginSuffix` (for example
        //   `reality` + `.lith.thematrixonline.net`), while localhost works even with the
        //   earlier infidel plain-prefix model
        // - keep the concat shape explicit instead of trial-and-error special casing
        // anchor: launcher.exe:0x41e5c8..0x41e61a / DAT_004d6814 -> 0x403f20 -> 0x4043b0 -> CLTIPAddressList_Reinit
        std::string rebuiltAddressListInput = routeDescriptor30_.empty()
            ? std::string{}
            : std::string(routeDescriptor30_.data());
        if (g_marginServerDNSName && g_marginServerDNSName[0] != '\0') {
            rebuiltAddressListInput += g_marginServerDNSName;
        }

        if (const char* const rebuiltBegin = StringBeginOrNull(rebuiltAddressListInput);
            rebuiltBegin != nullptr) {
            marginHost = rebuiltBegin;
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
        marginEndpoint6c_.family = 2;
        marginEndpoint6c_.portNetworkOrder =
            static_cast<uint16_t>((portHostOrder << 8) | (portHostOrder >> 8));
        marginEndpoint6c_.ipv4NetworkOrder = marginSelectedIpv4_7c_;
    } else if (const char* const routeDescriptorBegin = routeDescriptor30_.empty() ? nullptr : routeDescriptor30_.data();
               routeDescriptorBegin != nullptr && routeDescriptorBegin[0] != '\0' && g_marginServerDNSName && g_marginServerDNSName[0]) {
        marginHost = std::string(routeDescriptorBegin) + g_marginServerDNSName;
    }

    ++marginBeginCount24_;
    marginSelectedIpv4_7c_ = 0u;

    // Original does NOT call SetRemoteHostName - only sets endpoint
    connection->remoteEndpoint_ = marginEndpoint6c_;

    // Call Connect via vtable+0x1c (same as BeginAuthConnection)
    const uint32_t result = connection->Connect(marginEndpoint6c_);
    spdlog::info(
        "CLTLoginMediator::BeginMarginConnection resolvedHost='{}' routeHostText='{}' selector=0x{:02x} beginCount={} selectedIpv4=0x{:08x} port={} ensureConnectedResult=0x{:08x}",
        marginHost.empty() ? std::string("<empty>") : marginHost,
        (routeHostText && routeHostText[0]) ? std::string(routeHostText) : std::string("<empty>"),
        cachedRouteSelector,
        marginBeginCount24_,
        marginEndpoint6c_.ipv4NetworkOrder,
        portHostOrder,
        result);
    if (result == 0u) {
        spdlog::debug(
            "CLTLoginMediator::BeginMarginConnection connect failed host='{}' port={} ip=0x{:08x} selector={} beginCount={}",
            marginHost.empty() ? std::string("<empty>") : marginHost,
            marginServerPortHostOrder_,
            marginEndpoint6c_.ipv4NetworkOrder,
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
    const Packet_AsAuthReply_0x4b5328* record = &selectionRouteState684_.slotRecordTable04_[slotIndex];
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
    const std::string& slot = selectionRouteState684_.routeHostStrings194_[slotIndex];
    return StringBeginOrNull(slot);
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
    const Packet_AsAuthReply_0x4b5328* record =
        const_cast<CLTLoginMediator*>(this)->GetAuthReplyPacketByIndex40(
            static_cast<uint32_t>(slotIndex));
    return (record && record->payloadAlias10)
        ? static_cast<const uint8_t*>(record->payloadAlias10)[0x0bu]
        : 7u;
}

// anchor: launcher.exe:0x41b2e0
const char* CLTLoginMediator::GetWorldListNameByIndex(uint8_t slotIndex) const {
    if (slotIndex >= worldListPacketCountD80_ || slotIndex >= worldListPacketsD84_.size()) {
        return nullptr;
    }
    const Packet_WorldList_0x4b533c* const slot = worldListPacketsD84_[slotIndex];
    if (!slot) {
        return nullptr;
    }
    return slot->inlineNamePlus03.empty() ? nullptr : slot->inlineNamePlus03.c_str();
}

// anchor: launcher.exe:0x41b320
// Ghidra/disassembly correction:
// - this owner reader returns descriptor payload byte `+0x17` (Status)
// - earlier docs/source had an off-by-one stale guess that put Type here
uint8_t CLTLoginMediator::GetWorldListStatusByIndex(uint8_t slotIndex) const {
    if (slotIndex >= worldListPacketCountD80_ || slotIndex >= worldListPacketsD84_.size()) {
        return 0;
    }
    const Packet_WorldList_0x4b533c* const slot = worldListPacketsD84_[slotIndex];
    return slot ? slot->status17 : 0;
}

// anchor: launcher.exe:0x41b360
// Ghidra/disassembly correction:
// - this owner reader returns descriptor payload byte `+0x18` (Type)
// - earlier docs/source had an off-by-one stale guess that put server-version low byte here
uint8_t CLTLoginMediator::GetWorldListTypeByIndex(uint8_t slotIndex) const {
    if (slotIndex >= worldListPacketCountD80_ || slotIndex >= worldListPacketsD84_.size()) {
        return 0;
    }
    const Packet_WorldList_0x4b533c* const slot = worldListPacketsD84_[slotIndex];
    return slot ? slot->type18 : 0;
}

// anchor: launcher.exe:0x41b3a0
uint8_t CLTLoginMediator::GetWorldListPopulationByIndex(uint8_t slotIndex) const {
    if (slotIndex >= worldListPacketCountD80_ || slotIndex >= worldListPacketsD84_.size()) {
        return 0;
    }
    const Packet_WorldList_0x4b533c* const slot = worldListPacketsD84_[slotIndex];
    const uint8_t value = slot ? (slot->populationLevel1f & 0x0f) : 0u;
    return (value >= 1u && value <= 3u) ? value : 0u;
}

}  // namespace mxo::ltlogin
