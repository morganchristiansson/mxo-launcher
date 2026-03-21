#include "loginmediator.h"

#include "../../../../src/diagnostics.h"

namespace mxo::ltlogin {

// Focused arg6/selection split:
// - keep `ILTLoginMediator.Default` world-list/selection scaffolding out of the main mediator TU
// - this lets active auth/state8/state9 work avoid rereading arg6 startup-selection code
// - canonical RE reference remains:
//   `../../../../docs/launcher.exe/startup_objects/0x4d2c58_ILTLoginMediator_Default.md`

// Address anchor: launcher.exe:0x4d2c58 = ILTLoginMediator_Default object (arg6)
void* CLTLoginMediator::WorldSlot(uint32_t index) const {
    // Faithful implementation should call:
    // - launcher.exe:0x40e670 = ILTLoginMediator_GetAvailableWorlds(index)
    // - launcher.exe:0x40cd10 = ILTLoginMediator_GetWorldNameByIndex (fallback)

    // Transitional stub preserves the slot structure for later completion.
    return worldSlots_[index];
}

// Address anchor: launcher.exe:0x4d2c58 = ILTLoginMediator_Default object (arg6)
void* CLTLoginMediator::WorldPayloadSlot(uint32_t index) const {
    // Faithful implementation should call:
    // - launcher.exe:0x40e6c0 = ILTLoginMediator_GetAvailableWorldName(index)
    // - launcher.exe:0x40d6f0 = ILTLoginMediator_ResolveSelectionFromListCtrl

    // Transitional stub preserves the payload structure for later completion.
    return worldPayloadSlots_[index];
}

// anchor: launcher.exe:0x40e480
// sibling slot/vtable family: launcher.exe:0x4d3584
void CLTLoginMediator::InitializeArg6DefaultObject() {
    arg6WorldList_.worldNames_ = {
        "Default", "Starter", "Classic", "Advanced", "Extreme"
    };
    arg6WorldList_.worldVariants_ = {1, 2, 3, 5, 1};
    arg6WorldList_.worldValid_ = {true, true, true, true, true, false, false, false, false, false};
    arg6WorldList_.available_ = {true, true, true, true, true, false, false, false, false, false};
    arg6WorldList_.totalCount_ = 5;
    arg6WorldList_.activeCount_ = 5;

    arg6Selection_ = Arg6SelectionConfig();

    Log(
        "DIAGNOSTIC: InitializeArg6DefaultObject populated arg6 defaults worlds=%u active=%u selectedWorld=0x%06x selectedVariant=0x%02x",
        (unsigned)arg6WorldList_.totalCount_,
        (unsigned)arg6WorldList_.activeCount_,
        (unsigned)arg6Selection_.selectedWorldIndexLow24_,
        (unsigned)arg6Selection_.selectedVariantIndexHigh8_);
}

void CLTLoginMediator::ConfigureArg6Selection(
    uint32_t worldUpperBoundExclusive,
    uint32_t variantUpperBoundExclusive,
    const char* mappedSelectionName,
    const char* mappedVariantName,
    uint32_t selectedWorldIndexLow24,
    uint32_t selectedVariantIndexHigh8,
    uint32_t selectedWorldType,
    uint32_t selectedVariantState) {
    arg6Selection_.worldUpperBoundExclusive_ = worldUpperBoundExclusive ? worldUpperBoundExclusive : 1u;
    arg6Selection_.variantUpperBoundExclusive_ = variantUpperBoundExclusive ? variantUpperBoundExclusive : 1u;
    arg6Selection_.selectedWorldIndexLow24_ = selectedWorldIndexLow24 & 0x00ffffffu;
    arg6Selection_.selectedVariantIndexHigh8_ = selectedVariantIndexHigh8 & 0xffu;
    arg6Selection_.selectedWorldType_ = selectedWorldType;
    arg6Selection_.selectedVariantState_ = selectedVariantState;
    arg6Selection_.mappedSelectionId_ = arg6Selection_.selectedWorldIndexLow24_;
    arg6Selection_.mappedSelectionName_ =
        (mappedSelectionName && mappedSelectionName[0]) ? mappedSelectionName : "standalone";
    arg6Selection_.mappedVariantName_ =
        (mappedVariantName && mappedVariantName[0]) ? mappedVariantName : arg6Selection_.mappedSelectionName_;
}

void CLTLoginMediator::SetArg6ProfileName(const char* profileName) {
    arg6Selection_.profileName_ = (profileName && profileName[0]) ? profileName : "resurrections";
}

void CLTLoginMediator::SetArg6AuthName(const char* authName) {
    arg6Selection_.authName_ = (authName && authName[0]) ? authName : arg6Selection_.profileName_;
}

void CLTLoginMediator::SetArg6AuthPassword(const char* authPassword) {
    arg6Selection_.authPassword_ = authPassword ? authPassword : "";
}

uint32_t CLTLoginMediator::Arg6WorldUpperBoundExclusive() const {
    return arg6Selection_.worldUpperBoundExclusive_;
}

uint32_t CLTLoginMediator::Arg6VariantUpperBoundExclusive() const {
    return arg6Selection_.variantUpperBoundExclusive_;
}

uint32_t CLTLoginMediator::Arg6SelectedWorldIndexLow24() const {
    return arg6Selection_.selectedWorldIndexLow24_;
}

uint32_t CLTLoginMediator::Arg6SelectedVariantIndexHigh8() const {
    return arg6Selection_.selectedVariantIndexHigh8_;
}

uint32_t CLTLoginMediator::Arg6SelectedWorldType() const {
    return arg6Selection_.selectedWorldType_;
}

uint32_t CLTLoginMediator::Arg6SelectedVariantState() const {
    return arg6Selection_.selectedVariantState_;
}

uint32_t CLTLoginMediator::Arg6MappedSelectionId() const {
    return arg6Selection_.mappedSelectionId_;
}

const char* CLTLoginMediator::Arg6MappedSelectionName() const {
    return arg6Selection_.mappedSelectionName_.c_str();
}

const char* CLTLoginMediator::Arg6MappedVariantName() const {
    return arg6Selection_.mappedVariantName_.c_str();
}

const char* CLTLoginMediator::Arg6ProfileName() const {
    return arg6Selection_.profileName_.c_str();
}

const char* CLTLoginMediator::Arg6AuthName() const {
    return arg6Selection_.authName_.c_str();
}

const char* CLTLoginMediator::Arg6AuthPassword() const {
    return arg6Selection_.authPassword_.c_str();
}

bool CLTLoginMediator::Arg6WorldIndexMatchesSelection(uint32_t worldIndex) const {
    return worldIndex == arg6Selection_.selectedWorldIndexLow24_;
}

bool CLTLoginMediator::Arg6VariantIndexMatchesSelection(uint32_t variantIndex) const {
    return variantIndex == arg6Selection_.selectedVariantIndexHigh8_;
}

uint32_t CLTLoginMediator::Arg6ExpectedSelectionDescriptorScratchRequest() const {
    const uint32_t variantHigh8 = (arg6Selection_.selectedVariantIndexHigh8_ & 0xffu) << 24;
    const uint32_t preservedMiddle16 = arg6Selection_.selectedWorldIndexLow24_ & 0x00ffff00u;
    const uint32_t lowByteOverwrittenWithVariant = arg6Selection_.selectedVariantIndexHigh8_ & 0xffu;
    return variantHigh8 | preservedMiddle16 | lowByteOverwrittenWithVariant;
}

bool CLTLoginMediator::Arg6SelectionDescriptorMatchesRequest(uint32_t selectionIndex) const {
    const uint32_t normalizedSelectionIndex = selectionIndex & 0xffffffffu;
    if ((normalizedSelectionIndex & 0x00ffffffu) == arg6Selection_.selectedWorldIndexLow24_) {
        return true;
    }
    return normalizedSelectionIndex == Arg6ExpectedSelectionDescriptorScratchRequest();
}

// anchor: launcher.exe:0x40cd10
// vtable: launcher.exe:0x4d3584 +0xfc
const char* CLTLoginMediator::Arg6GetWorldNameByIndex(uint32_t index) {
    if (index < Arg6WorldUpperBoundExclusive() && Arg6WorldIndexMatchesSelection(index)) {
        return Arg6MappedSelectionName();
    }
    return (index < arg6WorldList_.totalCount_) ? arg6WorldList_.worldNames_[index].c_str() : nullptr;
}

// anchor: launcher.exe:0x4d3584 +0x100
// vtable: launcher.exe:0x4d3584 +0x100
uint8_t CLTLoginMediator::Arg6GetWorldVariantByIndex(uint32_t index) {
    if (index < Arg6WorldUpperBoundExclusive() && Arg6WorldIndexMatchesSelection(index)) {
        return static_cast<uint8_t>(Arg6SelectedWorldType());
    }
    return (index < arg6WorldList_.totalCount_) ? arg6WorldList_.worldVariants_[index] : 0u;
}

// anchor: launcher.exe:0x4d3584 +0xe4
// vtable: launcher.exe:0x4d3584 +0xe4
uint8_t CLTLoginMediator::Arg6ValidateWorldSelection(uint8_t variant) {
    if (variant < Arg6VariantUpperBoundExclusive() && Arg6VariantIndexMatchesSelection(variant)) {
        return static_cast<uint8_t>(Arg6SelectedVariantState());
    }
    return 3u;
}

// anchor: launcher.exe:0x40e5b0
// vtable: launcher.exe:0x4d3584 +0xf8
uint32_t CLTLoginMediator::Arg6GetWorldListCount() const {
    return Arg6WorldUpperBoundExclusive();
}

// anchor: launcher.exe:0x40e560
// vtable: launcher.exe:0x4d3584 +0xd8
uint32_t CLTLoginMediator::Arg6GetActiveWorldListCount() const {
    return Arg6VariantUpperBoundExclusive();
}

// anchor: launcher.exe:0x40e670
// vtable: launcher.exe:0x4d3584 +0xe0
bool CLTLoginMediator::Arg6GetAvailableWorlds(uint32_t index) const {
    if (index < Arg6VariantUpperBoundExclusive() && Arg6VariantIndexMatchesSelection(index)) {
        return true;
    }
    return (index < arg6WorldList_.available_.size()) ? arg6WorldList_.available_[index] : false;
}

// anchor: launcher.exe:0x40cd60
// vtable: launcher.exe:0x4d3584 +0xdc
const char* CLTLoginMediator::Arg6GetAvailableWorldName(uint32_t index) {
    if (index < Arg6VariantUpperBoundExclusive() && Arg6VariantIndexMatchesSelection(index)) {
        return Arg6MappedVariantName();
    }
    return (index < arg6WorldList_.totalCount_) ? arg6WorldList_.worldNames_[index].c_str() : nullptr;
}

// =============================================================================
// Private helper: Populate client.dll's world list view for InitClientDLL
// Address anchor: launcher.exe:0x4d3584 = ILTLoginMediator_SiblingObject
// =============================================================================
void CLTLoginMediator::PopulateClientWorldView() {
    // Populate the client's world list view with launcher-provided data
    // This ensures client.dll receives populated world data when InitClientDLL passes arg6
    Log("launcher-owned PopulateClientWorldView called");

    // Copy launcher-owned world list into the mediator's client-facing view
    for (uint32_t i = 0; i < kRecoveredWorldSlotCapacity && i < arg6WorldList_.totalCount_; ++i) {
        worldSlots_[i] = const_cast<void*>(reinterpret_cast<const void*>(arg6WorldList_.worldNames_[i].c_str()));
        worldPayloadSlots_[i] = const_cast<void*>(reinterpret_cast<const void*>(&arg6WorldList_.worldVariants_[i]));
        arg6WorldList_.worldValid_[i] = true;
        arg6WorldList_.available_[i] = true;
    }

    Log("launcher-owned PopulateClientWorldView populated %u worlds", (unsigned)kRecoveredWorldSlotCapacity);
}

}  // namespace mxo::ltlogin
