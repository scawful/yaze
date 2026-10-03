#ifndef YAZE_UTIL_ROM_HASH_H
#define YAZE_UTIL_ROM_HASH_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace yaze::util {

// Standard IEEE CRC32; new identities always use this algorithm.
uint32_t CalculateCrc32(const uint8_t* data, size_t size);
std::string ComputeRomHash(const uint8_t* data, size_t size);

enum class RomHashMatch { kMismatch, kCrc32, kLegacyCrc32, kSha1 };

// Hashes of one loaded-ROM identity, captured before pending edits. The legacy
// value exists only to verify old untagged project hashes, never to mint new ones.
struct RomIdentityHashes {
  std::string crc32;
  std::string legacy_crc32;
  std::string sha1;

  RomHashMatch Match(std::string_view expected) const;
};
RomIdentityHashes ComputeRomIdentityHashes(const uint8_t* data, size_t size);

/// Compute SHA-1 hash of data, return lowercase hex string (40 chars).
std::string ComputeSha1Hex(const uint8_t* data, size_t size);

/// Compute SHA-1 hash of a file on disk, return lowercase hex string.
/// Returns empty string on read error.
std::string ComputeFileSha1Hex(const std::string& path);

}  // namespace yaze::util

#endif  // YAZE_UTIL_ROM_HASH_H
