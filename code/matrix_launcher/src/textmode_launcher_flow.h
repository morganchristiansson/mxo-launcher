#pragma once

namespace mxo::launcher::replacement {

// Replacement-only pre-client text-mode launcher flow state.
// This is intentionally separate from recovered launcher-owned method bodies so launcher.cpp can
// stay focused on anchored startup coordination while the console UI scaffold lives under src/.
bool TextModePreClientFlowCompleted();
void ResetTextModePreClientFlowCompleted();

} // namespace mxo::launcher::replacement
