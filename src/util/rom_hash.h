#ifndef YAZE_UTIL_ROM_HASH_H
#define YAZE_UTIL_ROM_HASH_H

#include <cstddef>
#include <cstdint>
#include <string>

namespace yaze::util {

uint32_t CalculateCrc32(const uint8_t* data, size_t size);
std::string ComputeRomHash(const uint8_t* data, size_t size);

}  // namespace yaze::util

#endif  // YAZE_UTIL_ROM_HASH_H
