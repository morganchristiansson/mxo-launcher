/**
 * CLTLoginMediator active-state-source / character-view helpers.
 *
 * Keep this TU narrow:
 * - generic registration for whichever mediator instance currently owns the live character/load state
 * - controller-adjacent character-view accessors used by diagnostics and wrapper-facing code
 *
 * Intentionally avoid answering the broader original-binary ownership split here.
 * The current runtime still lets diagnostics register a separate live controller instance, but the
 * interface stays generic so that ownership can move later without a broad rewrite.
 */

#include "loginmediator.h"

#include <string>

namespace mxo::ltlogin {
namespace {

static const char* NonEmptyOrNull(const char* value) {
    return (value && value[0]) ? value : nullptr;
}

static const char* PreferNonEmpty(const char* primary, const char* fallback) {
    return NonEmptyOrNull(primary) ? primary : NonEmptyOrNull(fallback);
}

static bool IsLikelyMiddleInitialOnly(const char* value) {
    return value != nullptr && std::char_traits<char>::length(value) == 1u;
}

}  // namespace

CLTLoginMediator* g_CurrentLoginMediator = nullptr;

// Stub globals for faithful Initialize implementation.
// These are populated by the launcher.exe path; replacement uses SetAuthServerConfig.
// anchor: launcher.exe:0x4d6304
void* g_pThreadPerClientTCPEngine = nullptr;
// anchor: launcher.exe:0x4f7b14
const char* g_qsAuthServerDNSName = "";
// anchor: launcher.exe:0x4d6780
uint32_t g_IgnoreHostsFileForAuth = 0;







CLTLoginMediator::ActiveCharacterStateViewScaffold
CLTLoginMediator::DescribeOwnCharacterStateScaffold() const {
    ActiveCharacterStateViewScaffold view = {};

    const SlotRecordState_0x4b5328* currentSlotRecord = GetCurrentSlotRecord();
    const SlotRecordState_0x4b5328* slotZeroRecord = GetSlotRecordByIndex(0u);

    if (currentSlotRecord) {
        view.characterIdLow = currentSlotRecord->characterIdLow32;
        view.characterIdHigh = currentSlotRecord->characterIdHigh36;
        if (currentSlotRecord->heapString14) {
            view.characterName = currentSlotRecord->heapString14;
        }
    }

    if ((view.characterIdLow == 0u && view.characterIdHigh == 0u) && slotZeroRecord) {
        view.characterIdLow = slotZeroRecord->characterIdLow32;
        view.characterIdHigh = slotZeroRecord->characterIdHigh36;
    }

    if (!NonEmptyOrNull(view.characterName) && slotZeroRecord && slotZeroRecord->heapString14) {
        view.characterName = slotZeroRecord->heapString14;
    }

    const auto& ownerState = postAuthMarginLoadingState_0xf14;
    view.characterName = PreferNonEmpty(view.characterName, ownerState.characterNameBufferF1c);
    view.characterName = PreferNonEmpty(
        view.characterName,
        ownerState.createCharacterData108.characterName00.data());

    const char* sourceBlock178 = NonEmptyOrNull(ownerState.createCharacterData108.realFirstName70.data());
    const char* sourceBlock198 = NonEmptyOrNull(ownerState.createCharacterData108.realLastName90.data());
    const char* sourceBlock1b8 = NonEmptyOrNull(ownerState.createCharacterData108.backgroundB0.data());
    const char* section0F8c = NonEmptyOrNull(ownerState.section0StringF8c.data());
    const char* section0Fac = NonEmptyOrNull(ownerState.section0StringFac.data());
    const char* section0Fcc = NonEmptyOrNull(ownerState.section0StringFcc.data());

    if (IsLikelyMiddleInitialOnly(section0F8c) && section0Fac && section0Fcc) {
        view.realFirstName = PreferNonEmpty(sourceBlock178, section0Fac);
        view.realLastName = PreferNonEmpty(sourceBlock198, section0Fcc);
        view.background = sourceBlock1b8;
    } else {
        view.realFirstName = PreferNonEmpty(sourceBlock178, section0F8c);
        view.realLastName = PreferNonEmpty(sourceBlock198, section0Fac);
        view.background = PreferNonEmpty(sourceBlock1b8, section0Fcc);
    }

    return view;
}

CLTLoginMediator::ActiveCharacterStateViewScaffold
CLTLoginMediator::DescribeActiveCharacterStateScaffold() const {
    // inside CLTLoginMediator method - just use this
    return DescribeOwnCharacterStateScaffold();
}

}  // namespace mxo::ltlogin
