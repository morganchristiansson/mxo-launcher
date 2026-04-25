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
// - Server expects 9 total chunks (see MarginSocket.cpp)

#include "client_chunk_hashes.h"

#include <spdlog/spdlog.h>
#include <sha.h>
#include <cstdio>
#include <cstring>

namespace mxo::ltlogin {

// Global storage instance
// anchor: launcher.exe static storage pattern (DAT_004f79d8 / DAT_004f79dc)
ClientChunkHashStorage g_ClientChunkHashStorage;

namespace {

// Hash a single file in 1MB chunks
// anchor: launcher.exe:0x43d800 inner loop
bool ComputeChunkedFileHashes(
    const std::string& filename,
    std::vector<ChunkHashResult>* outResults) {
  if (!outResults) {
    return false;
  }

  outResults->clear();

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
    return false;
  }

  // Process in 1MB chunks (0x100000 bytes)
  // anchor: launcher.exe:0x43d800 chunk size = 0x100000
  constexpr size_t kChunkSize = 0x100000U;  // 1MB
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
      return false;
    }

    // Compute SHA1 of this chunk
    CryptoPP::SHA1 sha1;
    sha1.Update(buffer.data(), static_cast<uint32_t>(bytesRead));

    uint8_t digest[20] = {};
    sha1.Final(digest);

    // Store first 16 bytes (4 uint32s) of digest
    // anchor: launcher.exe:0x43d800 stores only first 16 bytes
    ChunkHashResult result{};
    std::memcpy(result.hashWords.data(), digest, sizeof(result.hashWords));
    outResults->push_back(result);

    totalBytesProcessed += bytesRead;
    ++chunkIndex;
  }

  std::fclose(file);

  spdlog::debug(
      "GenerateClientChunkHashes(): Processed '{}' ({} bytes) into {} chunks",
      filename, fileSize, outResults->size());

  return true;
}

}  // namespace

bool GenerateClientChunkHashes(
    const std::vector<std::string>& filenames) {
  // Use the global storage instance
  g_ClientChunkHashStorage.Clear();

  spdlog::info(
      "GenerateClientChunkHashes(): Starting hash generation for {} files",
      filenames.size());

  // The original hashes each file in 1MB chunks
  // Usually just client.dll, but supports multiple files
  for (const auto& filename : filenames) {
    std::vector<ChunkHashResult> fileResults;
    if (ComputeChunkedFileHashes(filename, &fileResults)) {
      // Add all chunks from this file to the total
      for (size_t i = 0; i < fileResults.size(); ++i) {
        g_ClientChunkHashStorage.AddHash(fileResults[i]);
        if (i < 3 || i >= fileResults.size() - 3) {
          // Log first 3 and last 3 chunks
          spdlog::debug(
              " [{}] chunk[{}] = {:08x} {:08x} {:08x} {:08x}",
              filename, i,
              fileResults[i].hashWords[0],
              fileResults[i].hashWords[1],
              fileResults[i].hashWords[2],
              fileResults[i].hashWords[3]);
        }
      }
    }
  }

  spdlog::info(
      "GenerateClientChunkHashes(): Generated {} total chunk hashes across all files",
      g_ClientChunkHashStorage.GetHashes().size());

  // Return true if at least one hash was generated
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
