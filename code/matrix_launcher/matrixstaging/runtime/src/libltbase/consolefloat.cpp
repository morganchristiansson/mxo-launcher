// String anchor: 0x004c093c
// Recovered scaffold for launcher.exe float console variables.
// Original source not available.

#include "consolevar.h"

#include <cstdio>
#include <cstdlib>

// anchor: launcher.exe:0x48cce0
CConsoleFloat::CConsoleFloat() = default;

// anchor: launcher.exe:0x48cd30
bool CConsoleFloat::ParseValue(const char* valueText) {
    if (!valueText) {
        return false;
    }

    value_ = std::strtof(valueText, nullptr);
    initializedFromExternalSource_ = true;
    return true;
}

// anchor: launcher.exe:0x48ccf0
int CConsoleFloat::FormatValue(char* destination, int destinationCapacity) const {
    if (!destination || destinationCapacity <= 0) {
        return 0;
    }

    destination[destinationCapacity - 1] = '\0';
    return std::snprintf(destination, destinationCapacity - 1, "%g", static_cast<double>(value_));
}

// anchor: launcher.exe:0x48cd70
void CConsoleFloat::Dump() const {
    // Original implementation writes to the global console log FILE* and records the source file / line
    // in the TLS-backed logging context before emitting:
    //   "%s [%X]: %f (ConsoleFloat)\n"
    // from the float slot at +0x34.
}


