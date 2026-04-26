// Copyright (c) 2025, The Matrix Online Launcher Contributors
// See LICENSE for details

// anchor: launcher.exe:0x43d800 = GenerateClientChunkHashes
// Fidelity implementation using Crypto++ SHA1
//
// Summary from static RE (0x43d800):
// - Opens client.dll (derived from executable path)
// - Reads file in 0x100000 byte (1MB) chunks through DAT_004f79d8/DAT_004f79dc
// - Hashes each chunk with SHA1
// - Stores first 16 bytes of each digest in the caller-provided output container
// - Returns 1 on success, 0 on failure
//
// NOTE: In original 0x43d800, ALL logic is inlined in a single function.
// This implementation matches that structure - no separate helper functions.

#include "client_chunk_hashes.h"

#include <spdlog/spdlog.h>
#include <sha.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace mxo::ltlogin {

// Source-owned digest cache used by the replacement login flow.
// Fidelity note: this mirrors the growable output container passed as param_4 into
// launcher.exe:0x43d800, not the raw scratch-buffer globals at DAT_004f79d8/DAT_004f79dc.
ClientChunkHashStorage g_ClientChunkHashStorage;

// launcher.exe raw hashing scratch-buffer globals used directly by 0x43d800.
// Keep the original addresses in the source names so Ghidra/source can stay aligned.
// anchor: launcher.exe:DAT_004f79d8
static uint8_t* g_ClientChunkHashScratchBuffer_0x4f79d8 = nullptr;
// anchor: launcher.exe:DAT_004f79dc
static size_t g_ClientChunkHashScratchBufferSize_0x4f79dc = 0u;

// anchor: launcher.exe:0x43d800
bool GenerateClientChunkHashes(
    const std::vector<std::string>& filenames) {
  g_ClientChunkHashStorage.Clear();

  spdlog::info(
      "GenerateClientChunkHashes(): Starting hash generation for {} files",
      filenames.size());

  constexpr size_t kChunkSize = 0x100000U;
  if (g_ClientChunkHashScratchBufferSize_0x4f79dc != kChunkSize) {
    void* const resizedBuffer =
        std::realloc(g_ClientChunkHashScratchBuffer_0x4f79d8, kChunkSize);
    if (resizedBuffer == nullptr) {
      spdlog::warn(
          "GenerateClientChunkHashes(): Could not allocate 0x{:x}-byte scratch buffer",
          static_cast<unsigned>(kChunkSize));
      return false;
    }
    g_ClientChunkHashScratchBuffer_0x4f79d8 = static_cast<uint8_t*>(resizedBuffer);
    g_ClientChunkHashScratchBufferSize_0x4f79dc = kChunkSize;
  }

  // ORIGINAL 0x43d800: iterates over filename array, opens each file,
  // reads in 1MB chunks through DAT_004f79d8, SHA1 hashes each chunk, and stores the
  // first 16 digest bytes in the caller-provided output container.
  for (const auto& filename : filenames) {
    FILE* file = std::fopen(filename.c_str(), "rb");
    if (!file) {
      spdlog::warn("GenerateClientChunkHashes(): Could not open file '{}'", filename);
      return false;
    }

    // Get file size
    std::fseek(file, 0, SEEK_END);
    const long fileSize = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);

    if (fileSize <= 0) {
      std::fclose(file);
      spdlog::warn("GenerateClientChunkHashes(): Empty or invalid file '{}'", filename);
      continue;
    }

    // Process in 1MB chunks (0x100000 bytes)
    // anchor: launcher.exe:0x43d800 chunk size

    size_t totalBytesProcessed = 0;
    size_t chunkIndex = 0;

    while (totalBytesProcessed < static_cast<size_t>(fileSize)) {
      const size_t bytesToRead = std::min(
          kChunkSize,
          static_cast<size_t>(fileSize) - totalBytesProcessed);

      const size_t bytesRead =
          std::fread(g_ClientChunkHashScratchBuffer_0x4f79d8, 1, bytesToRead, file);
      if (bytesRead != bytesToRead) {
        std::fclose(file);
        spdlog::warn(
            "GenerateClientChunkHashes(): Short read at chunk {} (expected {}, got {})",
            chunkIndex, bytesToRead, bytesRead);
        return false;
      }

      // Compute SHA1 of this chunk
      // anchor: launcher.exe:0x43d800 SHA1 hash computation
      CryptoPP::SHA1 sha1;
      sha1.Update(g_ClientChunkHashScratchBuffer_0x4f79d8, static_cast<uint32_t>(bytesRead));

      uint8_t digest[20] = {};
      sha1.Final(digest);

      // Store first 16 bytes (4 uint32s) of digest
      // anchor: launcher.exe:0x43d800 stores only first 16 bytes
      ChunkHashResult result{};
      std::memcpy(result.hashWords.data(), digest, sizeof(result.hashWords));
      g_ClientChunkHashStorage.AddHash(result);

      // Log first 3 and last 3 chunks for diagnostics
      if (chunkIndex < 3 || chunkIndex + 3 >= (fileSize / kChunkSize)) {
        spdlog::debug(
            " [{}] chunk[{}] = {:08x} {:08x} {:08x} {:08x}",
            filename, chunkIndex,
            result.hashWords[0],
            result.hashWords[1],
            result.hashWords[2],
            result.hashWords[3]);
      }

      totalBytesProcessed += bytesRead;
      ++chunkIndex;
    }

    std::fclose(file);
    spdlog::debug(
        "GenerateClientChunkHashes(): Processed '{}' ({} bytes) into {} chunks",
        filename, fileSize, chunkIndex);
  }

  spdlog::info(
      "GenerateClientChunkHashes(): Generated {} total chunk hashes",
      g_ClientChunkHashStorage.GetHashes().size());

  return true;
}

// ClientChunkHashStorage implementation

void ClientChunkHashStorage::Clear() {
  hashes_.clear();
}

void ClientChunkHashStorage::AddHash(const ChunkHashResult& result) {
  hashes_.push_back(result);
}

const std::vector<ChunkHashResult>& ClientChunkHashStorage::GetHashes() const {
  return hashes_;
}


}  // namespace mxo::ltlogin
