// Copyright (c) 2025, The Matrix Online Launcher Contributors
// See LICENSE for details

// anchor: launcher.exe:0x43d800 = GenerateClientChunkHashes
// Fidelity: Uses Crypto++ SHA1 for hash computation per static-RE evidence.
// See ../../docs/launcher.exe/CRYPTOPP.md SHA1 family notes for the `0x4ba258` mapping.

#ifndef MATRIXSTAGING_GAME_SRC_LIBLTCLIENTLOGIN_CLIENT_CHUNK_HASHES_H_
#define MATRIXSTAGING_GAME_SRC_LIBLTCLIENTLOGIN_CLIENT_CHUNK_HASHES_H_

#include <array>
#include <cstdint>
#include <vector>
#include <string>

namespace mxo::ltlogin {

// 16-byte hash result (4 x uint32) as used in the original binary
// anchor: launcher.exe:0x43d800 output format
struct ChunkHashResult {
  std::array<uint32_t, 4> hashWords{};
};

// Generates SHA1 hashes of client file(s)
// anchor: launcher.exe:0x43d800
//
// ORIGINAL FIDELITY: In original 0x43d800, ALL logic is inlined in a single function.
// This implementation matches that structure - no separate helper functions exist.
//
// Parameters:
//   filenames - list of file paths to hash
// Returns:
//   bool - true on the original success path; launcher.exe:0x43d800 uses these digests for the
//          opcode-0x08 challenge-response path, not as a state8 selection-context rewrite
bool GenerateClientChunkHashes(
    const std::vector<std::string>& filenames);

// Stores chunk hashes for later retrieval by the state6 opcode-0x07/0x08 path.
class ClientChunkHashStorage {
 public:
  void Clear();
  void AddHash(const ChunkHashResult& result);
  const std::vector<ChunkHashResult>& GetHashes() const;

  void SetHashes(const std::vector<ChunkHashResult>& hashes);

 private:
  std::vector<ChunkHashResult> hashes_;
};

// Source-owned digest cache used by the replacement login flow.
// Fidelity note: launcher.exe DAT_004f79d8 / DAT_004f79dc are the raw scratch-buffer globals
// inside GenerateClientChunkHashes, while this container mirrors the growable output aggregate.
extern ClientChunkHashStorage g_ClientChunkHashStorage;

}  // namespace mxo::ltlogin

#endif  // MATRIXSTAGING_GAME_SRC_LIBLTCLIENTLOGIN_CLIENT_CHUNK_HASHES_H_
