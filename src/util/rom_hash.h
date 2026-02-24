#ifndef YAZE_UTIL_ROM_HASH_H
#define YAZE_UTIL_ROM_HASH_H

#include <cstddef>
#include <cstdint>
#include <string>

namespace yaze::util {

uint32_t CalculateCrc32(const uint8_t* data, size_t size);
std::string ComputeRomHash(const uint8_t* data, size_t size);

/// Compute SHA-1 hash of data, return lowercase hex string (40 chars).
std::string ComputeSha1Hex(const uint8_t* data, size_t size);

/// Compute SHA-1 hash of a file on disk, return lowercase hex string.
/// Returns empty string on read error.
std::string ComputeFileSha1Hex(const std::string& path);

}  // namespace yaze::util

#endif  // YAZE_UTIL_ROM_HASH_H
