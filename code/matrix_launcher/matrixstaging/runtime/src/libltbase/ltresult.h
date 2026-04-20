#pragma once

#include <cstdint>

// anchor: launcher.exe:0x417650
// Bounded source mirror of the LTRESULT-name lookup used by `CLTTCPConnection::OnReceive`.
// Current recovered coverage:
// - default/non-positive family entry for `0x00000000 = LT_OK`
// - positive family `0x00` base results from `0x4964b0..0x4965f0`
// - positive family `0x07` core LTTCP results from `0x493e00..0x493f70`
// - positive family `0x07` Winsock-style LTTCP results from `0x493f80..0x4943d0`
// - positive family `0x19` LTAUTH results from `0x493d60..0x493df0`
// The helper mirrors the original prefix trimming too:
// - `LT_ERROR` -> `ERROR`
// - `LTTCP_STREAMCORRUPTED` -> `TCP_STREAMCORRUPTED`
// - `LTAUTH_AUTHKEYSIGINVALID` -> `AUTH_AUTHKEYSIGINVALID`
const char* CResultNameArrayItem_GetResultName(std::uint32_t resultCode);


