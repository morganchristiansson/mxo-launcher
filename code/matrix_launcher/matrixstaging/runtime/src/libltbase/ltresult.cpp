// anchor: launcher.exe:0x417650
// Recovered bounded LTRESULT-name lookup used by `CLTTCPConnection::OnReceive` generic terminal
// parser-error logging.
//
// Current high-confidence registration coverage mirrored from launcher.exe:
// - `0x4964b0..0x4965f0` register the base positive `LT_*` results in family `0x00`
// - `0x493e00..0x493f70` register the positive `LTTCP_*` results in family `0x07`
// - zero (`LT_OK`) is registered through the default/non-positive family at `0x4964b0`
//
// The original helper strips a leading `LT` / `LT_` prefix before returning the printable name.
// This source mirror keeps that exact trimming behavior.

#include "ltresult.h"

#include <cstddef>
#include <cstdint>

namespace mxo {
namespace libltbase {
namespace {

struct ResultFamilyEntry {
    std::uint32_t family;
    const char* const* names;
    std::size_t nameCount;
};

constexpr const char* kUnknownLtResultName = "UNKNOWN_LTRESULT";

// anchor: launcher.exe:0x4964b0..0x4965f0 LT family registration thunks
constexpr const char* kPositiveLtFamily00Names[] = {
    nullptr,
    "LT_ERROR",
    "LT_UNSUPPORTED",
    "LT_TIMEOUT",
    "LT_INVALIDPARAMS",
    "LT_OUTOFMEMORY",
    "LT_INVALIDPOINTER",
    "LT_EXCEPTION",
    "LT_DISKFULL",
    "LT_DOESNOTEXIST",
    "LT_ALREADYEXISTS",
};

// anchor: launcher.exe:0x493e00..0x493f70 LTTCP family registration thunks
constexpr const char* kPositiveLtFamily07Names[] = {
    "LTTCP_NEEDMOREDATA",
    "LTTCP_ALREADYCONNECTED",
    "LTTCP_NOTCONNECTED",
    "LTTCP_PORTALREADYMONITORED",
    "LTTCP_PORTNOTMONITORED",
    "LTTCP_PORTALREADYINUSE",
    "LTTCP_QUEUEISAUTOMATED",
    "LTTCP_ALREADYCLOSED",
    "LTTCP_ACCES",
    "LTTCP_ADDRINUSE",
    "LTTCP_OPQUEUEISEMPTY",
    "LTTCP_STREAMCORRUPTED",
};

constexpr ResultFamilyEntry kPositiveFamilies[] = {
    {0x00u, kPositiveLtFamily00Names, sizeof(kPositiveLtFamily00Names) / sizeof(kPositiveLtFamily00Names[0])},
    {0x07u, kPositiveLtFamily07Names, sizeof(kPositiveLtFamily07Names) / sizeof(kPositiveLtFamily07Names[0])},
};

const char* TrimLtResultPrefix(const char* resultName) {
    if (!resultName) {
        return kUnknownLtResultName;
    }
    if (resultName[0] == 'L' && resultName[1] == 'T') {
        return (resultName[2] == '_') ? (resultName + 3) : (resultName + 2);
    }
    return resultName;
}

const char* LookupPositiveLtResultName(std::uint32_t resultCode) {
    const std::uint32_t family = (resultCode >> 24u) & 0x7fu;
    const std::uint32_t index = resultCode & 0x00ffffffu;
    for (const ResultFamilyEntry& entry : kPositiveFamilies) {
        if (entry.family != family) {
            continue;
        }
        if (index >= entry.nameCount) {
            return kUnknownLtResultName;
        }
        return TrimLtResultPrefix(entry.names[index]);
    }
    return kUnknownLtResultName;
}

}  // namespace

const char* CResultNameArrayItem_GetResultName(std::uint32_t resultCode) {
    // `0x417650` chooses the default/non-positive table unless the LTRESULT is strictly positive.
    if (static_cast<std::int32_t>(resultCode) <= 0) {
        return (resultCode == 0u) ? TrimLtResultPrefix("LT_OK") : kUnknownLtResultName;
    }
    return LookupPositiveLtResultName(resultCode);
}

}  // namespace libltbase
}  // namespace mxo
