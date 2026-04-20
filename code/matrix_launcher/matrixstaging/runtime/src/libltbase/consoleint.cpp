// String anchor: 0x004addf4
// Recovered scaffold for launcher.exe integer console variables.
// Original source not available.

#include "consolevar.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>

// anchor: launcher.exe:0x414e70
CConsoleInt::CConsoleInt() = default;

// anchor: launcher.exe:0x415010
bool CConsoleInt::ParseValue(const char* valueText) {
    if (!valueText) {
        return false;
    }

    while (*valueText && std::isspace(static_cast<unsigned char>(*valueText))) {
        ++valueText;
    }

    if (valueText[0] == '0' && (valueText[1] == 'x' || valueText[1] == 'X')) {
        formatAsHex_ = true;
        value_ = static_cast<std::int32_t>(std::strtol(valueText + 2, nullptr, 16));
    } else {
        formatAsHex_ = false;
        value_ = static_cast<std::int32_t>(std::strtol(valueText, nullptr, 10));
    }

    initializedFromExternalSource_ = true;
    return true;
}

// anchor: launcher.exe:0x414e80
int CConsoleInt::FormatValue(char* destination, int destinationCapacity) const {
    if (!destination || destinationCapacity <= 0) {
        return 0;
    }

    destination[destinationCapacity - 1] = '\0';
    if (formatAsHex_) {
        return std::snprintf(destination, destinationCapacity - 1, "0x%x", static_cast<unsigned>(value_));
    }

    return std::snprintf(destination, destinationCapacity - 1, "%d", value_);
}

// anchor: launcher.exe:0x4150d0
void CConsoleInt::Dump() const {
    // Original implementation writes to the global console log FILE* and records the source file / line
    // in the TLS-backed logging context before emitting:
    //   "%s [%X]: %i (ConsoleInt)\n"
    // Keep this as a scaffold until the replacement owns an equivalent console-log sink.
}


