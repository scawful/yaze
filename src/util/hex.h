#ifndef YAZE_UTIL_HEX_H
#define YAZE_UTIL_HEX_H

#include <cstdint>

namespace yaze {
namespace util {

struct HexStringParams {
  enum class Prefix { kNone, kDollar, kHash, k0x } prefix = Prefix::kDollar;
  bool uppercase = true;
};

std::string HexByte(uint8_t byte, HexStringParams params = {});
std::string HexWord(uint16_t word, HexStringParams params = {});
std::string HexLong(uint32_t dword, HexStringParams params = {});
std::string HexLongLong(uint64_t qword, HexStringParams params = {});

}  // namespace util
}  // namespace yaze