#include "launcher_replacement_support.h"

#include <windows.h>
#include <cstring>

#include <spdlog/spdlog.h>

namespace {

constexpr char kLauncherRegistryKeyPath[] = "Software\\Monolith Productions\\The Matrix Online\\";
constexpr uint32_t kRecoveredSelectionWorldIndexLow24 = 0x00002au;

const mxo::launcher::replacement::RecoveredLauncherSelectionRecord kRecoveredLauncherSelectionRecords[] = {
    {"Reality", "reality", 1u, 0u},
};

void LowercaseAsciiCopy(char* destination, size_t destinationSize, const char* source) {
    if (!destination || destinationSize == 0u) {
        return;
    }
    destination[0] = '\0';
    if (!source) {
        return;
    }

    size_t write = 0u;
    for (size_t i = 0u; source[i] && write + 1u < destinationSize; ++i) {
        const unsigned char c = static_cast<unsigned char>(source[i]);
        destination[write++] =
            (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : static_cast<char>(c);
    }
    destination[write] = '\0';
}

void CanonicalizeLauncherSelectionLookupName(
    char* destination,
    size_t destinationSize,
    const char* source) {
    if (!destination || destinationSize == 0u) {
        return;
    }
    destination[0] = '\0';
    if (!source) {
        return;
    }

    while (*source == ' ' || *source == '\t' || *source == '\r' || *source == '\n') {
        ++source;
    }

    size_t sourceLength = std::strlen(source);
    while (sourceLength > 0u) {
        const char c = source[sourceLength - 1u];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            break;
        }
        --sourceLength;
    }

    char trimmed[128] = {};
    const size_t copyLength =
        (sourceLength < sizeof(trimmed) - 1u) ? sourceLength : (sizeof(trimmed) - 1u);
    if (copyLength != 0u) {
        std::memcpy(trimmed, source, copyLength);
        trimmed[copyLength] = '\0';
    }
    LowercaseAsciiCopy(destination, destinationSize, trimmed);
}

} // namespace

namespace mxo::launcher::replacement {

const RecoveredLauncherSelectionRecord* DefaultRecoveredLauncherSelectionRecord() {
    constexpr size_t kRecordCount =
        sizeof(kRecoveredLauncherSelectionRecords) / sizeof(kRecoveredLauncherSelectionRecords[0]);
    return (kRecordCount != 0u) ? &kRecoveredLauncherSelectionRecords[0] : nullptr;
}

const RecoveredLauncherSelectionRecord* FindRecoveredLauncherSelectionRecord(const char* selectionName) {
    if (!selectionName || !selectionName[0]) {
        return nullptr;
    }

    char normalizedInput[128] = {};
    CanonicalizeLauncherSelectionLookupName(normalizedInput, sizeof(normalizedInput), selectionName);
    if (!normalizedInput[0]) {
        return nullptr;
    }

    constexpr size_t kRecordCount =
        sizeof(kRecoveredLauncherSelectionRecords) / sizeof(kRecoveredLauncherSelectionRecords[0]);
    for (size_t i = 0u; i < kRecordCount; ++i) {
        const RecoveredLauncherSelectionRecord& record = kRecoveredLauncherSelectionRecords[i];

        char normalizedRecordWorld[128] = {};
        CanonicalizeLauncherSelectionLookupName(
            normalizedRecordWorld,
            sizeof(normalizedRecordWorld),
            record.selectionName);
        if (normalizedRecordWorld[0] && std::strcmp(normalizedRecordWorld, normalizedInput) == 0) {
            return &record;
        }

        char normalizedRoutePrefix[128] = {};
        CanonicalizeLauncherSelectionLookupName(
            normalizedRoutePrefix,
            sizeof(normalizedRoutePrefix),
            record.routeHostPrefix);
        if (normalizedRoutePrefix[0] && std::strcmp(normalizedRoutePrefix, normalizedInput) == 0) {
            return &record;
        }
    }

    return nullptr;
}

uint32_t RecoveredSelectionWorldIndexLow24() {
    return kRecoveredSelectionWorldIndexLow24;
}

bool LoadLastWorldNameFromRegistry(char* out, size_t outSize) {
    if (!out || outSize < 2u) {
        return false;
    }
    out[0] = '\0';

    HKEY key = nullptr;
    const LONG openResult =
        RegOpenKeyExA(HKEY_LOCAL_MACHINE, kLauncherRegistryKeyPath, 0, KEY_QUERY_VALUE, &key);
    if (openResult != ERROR_SUCCESS) {
        spdlog::info("DIAGNOSTIC: HKLM Last_WorldName key open failed ({})", openResult);
        return false;
    }

    DWORD type = 0u;
    DWORD size = static_cast<DWORD>(outSize);
    const LONG queryResult =
        RegQueryValueExA(key, "Last_WorldName", nullptr, &type, reinterpret_cast<LPBYTE>(out), &size);
    RegCloseKey(key);
    if (queryResult != ERROR_SUCCESS) {
        spdlog::info("DIAGNOSTIC: HKLM Last_WorldName query failed ({})", queryResult);
        out[0] = '\0';
        return false;
    }
    if (type != REG_SZ && type != REG_EXPAND_SZ) {
        spdlog::info("DIAGNOSTIC: HKLM Last_WorldName unexpected registry type {}", type);
        out[0] = '\0';
        return false;
    }

    out[outSize - 1u] = '\0';
    spdlog::info("DIAGNOSTIC: loaded HKLM Last_WorldName='{}'", out);
    return out[0] != '\0';
}

bool StoreLastWorldNameInRegistry(const char* selectionName) {
    if (!selectionName || !selectionName[0]) {
        return false;
    }

    HKEY key = nullptr;
    DWORD disposition = 0u;
    const LONG createResult = RegCreateKeyExA(
        HKEY_LOCAL_MACHINE,
        kLauncherRegistryKeyPath,
        0,
        nullptr,
        0,
        KEY_SET_VALUE,
        nullptr,
        &key,
        &disposition);
    if (createResult != ERROR_SUCCESS) {
        spdlog::warn(
            "DIAGNOSTIC: HKLM Last_WorldName key create/open for write failed ({})",
            static_cast<long>(createResult));
        return false;
    }

    const size_t byteCount = std::strlen(selectionName) + 1u;
    const LONG setResult = RegSetValueExA(
        key,
        "Last_WorldName",
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(selectionName),
        static_cast<DWORD>(byteCount));
    RegCloseKey(key);
    if (setResult != ERROR_SUCCESS) {
        spdlog::warn(
            "DIAGNOSTIC: HKLM Last_WorldName write failed ({})",
            static_cast<long>(setResult));
        return false;
    }

    spdlog::info(
        "DIAGNOSTIC: persisted HKLM Last_WorldName='{}'{}",
        selectionName,
        (disposition == REG_CREATED_NEW_KEY) ? " (created key)" : "");
    return true;
}

} // namespace mxo::launcher::replacement
