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

static CLTLoginMediator* g_ActiveStateSourceScaffold = nullptr;

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

void CLTLoginMediator::RegisterActiveStateSourceScaffold(CLTLoginMediator* mediator) {
    g_ActiveStateSourceScaffold = mediator;
}

bool CLTLoginMediator::UnregisterActiveStateSourceScaffold(const CLTLoginMediator* mediator) {
    if (g_ActiveStateSourceScaffold != mediator) {
        return false;
    }

    g_ActiveStateSourceScaffold = nullptr;
    return true;
}

CLTLoginMediator* CLTLoginMediator::ActiveStateSourceScaffold() {
    return g_ActiveStateSourceScaffold;
}

const CLTLoginMediator* CLTLoginMediator::ResolveActiveStateSourceScaffold() const {
    return g_ActiveStateSourceScaffold ? g_ActiveStateSourceScaffold : this;
}

CLTLoginMediator::ActiveCharacterStateViewScaffold
CLTLoginMediator::DescribeOwnCharacterStateScaffold() const {
    ActiveCharacterStateViewScaffold view = {};

    const SlotRecordState004b5328* currentSlotRecord = GetCurrentSlotRecord();
    const SlotRecordState004b5328* slotZeroRecord = GetSlotRecordByIndex(0u);

    if (currentSlotRecord) {
        view.characterIdLow = currentSlotRecord->globalCharacterIdLow03;
        view.characterIdHigh = currentSlotRecord->globalCharacterIdHigh07;
        if (!currentSlotRecord->heapString14.empty()) {
            view.characterName = currentSlotRecord->heapString14.c_str();
        }
    }

    if ((view.characterIdLow == 0u && view.characterIdHigh == 0u) && slotZeroRecord) {
        view.characterIdLow = slotZeroRecord->globalCharacterIdLow03;
        view.characterIdHigh = slotZeroRecord->globalCharacterIdHigh07;
    }

    if (!NonEmptyOrNull(view.characterName) && slotZeroRecord && !slotZeroRecord->heapString14.empty()) {
        view.characterName = slotZeroRecord->heapString14.c_str();
    }

    const auto& ownerState = PostAuthMarginLoadingStateView();
    view.characterName = PreferNonEmpty(view.characterName, ownerState.characterNameBufferF1c);
    view.characterName = PreferNonEmpty(view.characterName, SourceLeadString108().data());

    const char* sourceBlock178 = NonEmptyOrNull(reinterpret_cast<const char*>(SourceBlock178().data()));
    const char* sourceBlock198 = NonEmptyOrNull(reinterpret_cast<const char*>(SourceBlock198().data()));
    const char* sourceBlock1b8 = NonEmptyOrNull(reinterpret_cast<const char*>(SourceBlock1b8().data()));
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
    const CLTLoginMediator* activeSource = ResolveActiveStateSourceScaffold();
    if (activeSource && activeSource != this) {
        return activeSource->DescribeOwnCharacterStateScaffold();
    }
    return DescribeOwnCharacterStateScaffold();
}

}  // namespace mxo::ltlogin
