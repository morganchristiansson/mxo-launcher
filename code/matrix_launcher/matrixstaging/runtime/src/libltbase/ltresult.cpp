// anchor: launcher.exe:0x417650
// Recovered bounded LTRESULT-name lookup used by `CLTTCPConnection::OnReceive` generic terminal
// parser-error logging.
//
// Current high-confidence registration coverage mirrored from launcher.exe:
// - `0x417e90 = CResultNameArray_InitializeFamilyTables`
// - `0x417f80 = CResultNameArray_RegisterResultName`
// - `0x4964b0..0x4965f0` register the base positive `LT_*` results in family `0x00`
// - `0x493d60..0x493df0` register the currently proven `LTAUTH_*` results in family `0x19`
// - `0x493e00..0x493f70` register the core positive `LTTCP_*` results in family `0x07`
// - `0x493f80..0x4943d0` register the Winsock-style positive `LTTCP_*` results in family `0x07`
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

struct ResultFamilyRangeEntry {
    std::uint32_t family;
    std::uint32_t firstIndex;
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

// anchor: launcher.exe:0x493d60..0x493df0 LTAUTH family registration thunks
constexpr const char* kPositiveLtAuthFamily19Names[] = {
    "LTAUTH_INVALIDCERTIFICATE",
    "LTAUTH_EXPIREDCERTIFICATE",
    "LTAUTH_LOGINTYPENOTACCEPTED",
    "LTAUTH_ALREADYCONNECTED",
    "LTAUTH_AUTHKEYSIGINVALID",
};

// anchor: launcher.exe:0x493e00..0x493f70 LTTCP family registration thunks
constexpr const char* kPositiveLtTcpFamily07Names[] = {
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

// anchor: launcher.exe:0x493f80..0x4943d0 Winsock-style LTTCP family registration thunks
constexpr const char* kPositiveLtTcpWinsockFamily07Names[] = {
    "LTTCP_ADDRNOTAVAIL",
    "LTTCP_AFNOSUPPORT",
    "LTTCP_ALREADY",
    "LTTCP_BADF",
    "LTTCP_CONNABORTED",
    "LTTCP_CONNREFUSED",
    "LTTCP_CONNRESET",
    "LTTCP_DESTADDRREQ",
    "LTTCP_FAULT",
    "LTTCP_HOSTDOWN",
    "LTTCP_HOSTUNREACH",
    "LTTCP_INPROGRESS",
    "LTTCP_INTR",
    "LTTCP_INVAL",
    "LTTCP_ISCONN",
    "LTTCP_LOOP",
    "LTTCP_MFILE",
    "LTTCP_MSGSIZE",
    "LTTCP_NAMETOOLONG",
    "LTTCP_NETDOWN",
    "LTTCP_NETRESET",
    "LTTCP_NETUNREACH",
    "LTTCP_NOBUFS",
    "LTTCP_NOPROTOOPT",
    "LTTCP_NOTCONN",
    "LTTCP_NOTSOCK",
    "LTTCP_OPNOTSUPP",
    "LTTCP_PFNOSUPPORT",
    "LTTCP_PROTONOSUPPORT",
    "LTTCP_PROTOTYPE",
    "LTTCP_SHUTDOWN",
    "LTTCP_SOCKTNOSUPPORT",
    "LTTCP_TIMEDOUT",
    "LTTCP_TOOMANYREFS",
    "LTTCP_WOULDBLOCK",
};

constexpr ResultFamilyRangeEntry kPositiveFamilyRanges[] = {
    {0x00u, 0x000000u, kPositiveLtFamily00Names, sizeof(kPositiveLtFamily00Names) / sizeof(kPositiveLtFamily00Names[0])},
    {0x07u, 0x000000u, kPositiveLtTcpFamily07Names, sizeof(kPositiveLtTcpFamily07Names) / sizeof(kPositiveLtTcpFamily07Names[0])},
    {0x07u, 0x0003e8u, kPositiveLtTcpWinsockFamily07Names, sizeof(kPositiveLtTcpWinsockFamily07Names) / sizeof(kPositiveLtTcpWinsockFamily07Names[0])},
    {0x19u, 0x000000u, kPositiveLtAuthFamily19Names, sizeof(kPositiveLtAuthFamily19Names) / sizeof(kPositiveLtAuthFamily19Names[0])},
};

}  // namespace

const char* CResultNameArrayItem_GetResultName(std::uint32_t resultCode) {
    const char* resultName = kUnknownLtResultName;

    // `0x417650` chooses the default/non-positive table unless the LTRESULT is strictly positive.
    if (static_cast<std::int32_t>(resultCode) <= 0) {
        resultName = (resultCode == 0u) ? "LT_OK" : kUnknownLtResultName;
    } else {
        const std::uint32_t family = (resultCode >> 24u) & 0x7fu;
        const std::uint32_t index = resultCode & 0x00ffffffu;
        for (const ResultFamilyRangeEntry& entry : kPositiveFamilyRanges) {
            if (entry.family != family || index < entry.firstIndex) {
                continue;
            }
            const std::uint32_t rangeOffset = index - entry.firstIndex;
            if (rangeOffset >= entry.nameCount) {
                continue;
            }
            resultName = entry.names[rangeOffset] ? entry.names[rangeOffset] : kUnknownLtResultName;
            break;
        }
    }

    if (resultName[0] == 'L' && resultName[1] == 'T') {
        return (resultName[2] == '_') ? (resultName + 3) : (resultName + 2);
    }
    return resultName;
}

}  // namespace libltbase
}  // namespace mxo
