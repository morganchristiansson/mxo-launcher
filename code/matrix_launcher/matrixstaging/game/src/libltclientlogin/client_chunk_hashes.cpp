// Copyright (c) 2025, The Matrix Online Launcher Contributors
// See LICENSE for details

// anchor: launcher.exe:0x43d800 = GenerateClientChunkHashes
// Fidelity implementation using Crypto++ SHA1
//
// Summary from static RE (0x43d800):
// - Opens client.dll (derived from executable path)
// - Reads file in 0x100000 byte (1MB) chunks
// - Hashes each chunk with SHA1
// - Stores first 16 bytes of each digest in global storage
// - Returns 1 on success, 0 on failure
//
// NOTE: In original 0x43d800, ALL logic is inlined in a single function.
// This implementation matches that structure - no separate helper functions.

#include "client_chunk_hashes.h"

#include <spdlog/spdlog.h>
#include <sha.h>
#include <cstdio>
#include <cstring>

namespace mxo::ltlogin {

// Global storage instance
// anchor: launcher.exe static storage pattern (DAT_004f79d8 / DAT_004f79dc)
ClientChunkHashStorage g_ClientChunkHashStorage;

// anchor: launcher.exe:0x43d800
bool GenerateClientChunkHashes(
    const std::vector<std::string>& filenames) {
  g_ClientChunkHashStorage.Clear();

  spdlog::info(
      "GenerateClientChunkHashes(): Starting hash generation for {} files",
      filenames.size());

  // ORIGINAL 0x43d800: iterates over filename array, opens each file,
  // reads in 1MB chunks, SHA1 hashes each chunk, stores in global.
  // Matching that flow exactly now.
  for (const auto& filename : filenames) {
    FILE* file = std::fopen(filename.c_str(), "rb");
    if (!file) {
      spdlog::warn("GenerateClientChunkHashes(): Could not open file '{}'", filename);
      continue;  // Continue to next file
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
    constexpr size_t kChunkSize = 0x100000U;
    std::vector<uint8_t> buffer(kChunkSize);

    size_t totalBytesProcessed = 0;
    size_t chunkIndex = 0;

    while (totalBytesProcessed < static_cast<size_t>(fileSize)) {
      const size_t bytesToRead = std::min(
          kChunkSize,
          static_cast<size_t>(fileSize) - totalBytesProcessed);

      const size_t bytesRead = std::fread(buffer.data(), 1, bytesToRead, file);
      if (bytesRead != bytesToRead) {
        std::fclose(file);
        spdlog::warn(
            "GenerateClientChunkHashes(): Short read at chunk {} (expected {}, got {})",
            chunkIndex, bytesToRead, bytesRead);
        break;
      }

      // Compute SHA1 of this chunk
      // anchor: launcher.exe:0x43d800 SHA1 hash computation
      CryptoPP::SHA1 sha1;
      sha1.Update(buffer.data(), static_cast<uint32_t>(bytesRead));

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

  // Return true if at least 9 hashes (what server expects)
  return g_ClientChunkHashStorage.HasValidHashCount();
}

// ClientChunkHashStorage implementation

void ClientChunkHashStorage::Clear() {
  hashes_.clear();
}

void ClientChunkHashStorage::AddHash(const ChunkHashResult& result) {
  hashes_.push_back(result);
}

void ClientChunkHashStorage::SetHashes(const std::vector<ChunkHashResult>& hashes) {
  hashes_ = hashes;
}

const std::vector<ChunkHashResult>& ClientChunkHashStorage::GetHashes() const {
  return hashes_;
}

bool ClientChunkHashStorage::HasValidHashCount() const {
  // Server expects 9 matching hashes in MS_LoadCharacterRequest
  // See MarginSocket.cpp: "Strange counter was not 9"
  return hashes_.size() >= 9;
}

}  // namespace mxo::ltlogin