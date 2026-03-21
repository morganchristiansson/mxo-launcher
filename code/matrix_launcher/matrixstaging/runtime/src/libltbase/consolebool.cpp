// String anchor: 0x004b99f0
// Recovered scaffold for launcher.exe boolean console variables.
// Original source not available.

#include "consolevar.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace mxo {
namespace libltbase {

namespace {
std::string LowercaseCopy(const char* text) {
    std::string out;
    if (!text) {
        return out;
    }

    while (*text) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(*text))));
        ++text;
    }
    return out;
}
} // namespace

// anchor: launcher.exe:0x452990
CConsoleBool::CConsoleBool() = default;

// anchor: launcher.exe:0x4529e0
bool CConsoleBool::ParseValue(const char* valueText) {
    if (!valueText) {
        return false;
    }

    while (*valueText && std::isspace(static_cast<unsigned char>(*valueText))) {
        ++valueText;
    }

    if (std::isdigit(static_cast<unsigned char>(*valueText))) {
        value_ = (std::atoi(valueText) != 0);
        initializedFromExternalSource_ = true;
        return true;
    }

    const std::string lowered = LowercaseCopy(valueText);
    if (lowered == "true" || lowered == "yes") {
        value_ = true;
        initializedFromExternalSource_ = true;
        return true;
    }
    if (lowered == "false" || lowered == "no") {
        value_ = false;
        initializedFromExternalSource_ = true;
        return true;
    }

    return false;
}

// anchor: launcher.exe:0x4529a0
int CConsoleBool::FormatValue(char* destination, int destinationCapacity) const {
    if (!destination || destinationCapacity <= 0) {
        return 0;
    }

    destination[destinationCapacity - 1] = '\0';
    return std::snprintf(destination, destinationCapacity - 1, "%s", value_ ? "true" : "false");
}

// anchor: launcher.exe:0x452ae0
void CConsoleBool::Dump() const {
    // Original implementation still formats the value through the ConsoleInt dump string:
    //   "%s [%X]: %i (ConsoleInt)\n"
    // while reading the boolean byte from +0x31.
}

} // namespace libltbase
} // namespace mxo
