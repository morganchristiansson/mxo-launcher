#pragma once

#include <cstdint>

namespace mxo {
namespace libltbase {

// anchor: launcher.exe:0x417650
// Bounded source mirror of the LTRESULT-name lookup used by `CLTTCPConnection::OnReceive`.
// Current recovered coverage:
// - default/non-positive family entry for `0x00000000 = LT_OK`
// - positive family `0x00` base results from `0x4964b0..0x4965f0`
// - positive family `0x07` LTTCP results from `0x493e00..0x493f70`
// The helper mirrors the original prefix trimming too:
// - `LT_ERROR` -> `ERROR`
// - `LTTCP_STREAMCORRUPTED` -> `TCP_STREAMCORRUPTED`
const char* CResultNameArrayItem_GetResultName(std::uint32_t resultCode);

}  // namespace libltbase
}  // namespace mxo
