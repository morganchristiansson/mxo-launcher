// String anchor: 0x004ae728
// Recovered scaffold for launcher.exe string console variables.
// Original source not available.

#include "consolevar.h"

#include <cstdio>

namespace mxo {
namespace libltbase {

// UNANCHORED: scaffold-only convenience ctor
CConsoleString::CConsoleString() = default;

// anchor: launcher.exe:0x4183b0
CConsoleString::CConsoleString(
    const char* name,
    const char* initialValue,
    const char* helpText,
    std::uint32_t flags) {
    SetRecoveredName(name);
    description_ = helpText ? helpText : "";
    flags_ = flags;
    defaultValue_ = initialValue ? initialValue : "";
    value_ = defaultValue_;
    RegisterSelf();
}

// anchor: launcher.exe:0x418380
CConsoleString::~CConsoleString() {
    DestroyBuffers();
}

// anchor: launcher.exe:0x4180a0
void CConsoleString::DestroyBuffers() {
    defaultValue_.clear();
    value_.clear();
}

// anchor: launcher.exe:0x418200
void CConsoleString::AssignValue(const char* valueText) {
    value_ = valueText ? valueText : "";
    initializedFromExternalSource_ = true;
}

// anchor: launcher.exe:0x418200
bool CConsoleString::ParseValue(const char* valueText) {
    AssignValue(valueText);
    return true;
}

// anchor: launcher.exe:0x418060
int CConsoleString::FormatValue(char* destination, int destinationCapacity) const {
    if (!destination || destinationCapacity <= 0) {
        return 0;
    }

    if (value_.empty()) {
        return 0;
    }

    destination[destinationCapacity - 1] = '\0';
    return std::snprintf(destination, destinationCapacity - 1, "\"%s\"", value_.c_str());
}

// anchor: launcher.exe:0x4182f0
void CConsoleString::Dump() const {
    // Original implementation writes to the global console log FILE* and records the source file / line
    // in the TLS-backed logging context before emitting:
    //   "%s [%X]: %s (ConsoleString)\n"
    // using the live string buffer at +0x34.
}

} // namespace libltbase
} // namespace mxo
