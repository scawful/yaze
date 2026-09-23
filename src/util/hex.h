#ifndef YAZE_UTIL_HEX_H
#define YAZE_UTIL_HEX_H

#include <cstdint>
#include <string>

#include "absl/strings/string_view.h"

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

// Strict hexadecimal parsing shared by the CLI, core manifest loading, and the
// editor. The whole string must be a hexadecimal number, so a value that only
// starts with digits is rejected instead of silently truncating:
//
//   "0x1E8000"   -> 0x1E8000        "0x1E80zz"  -> rejected
//   " 0x1E8000 " -> 0x1E8000        "1E 80"     -> rejected
//   "$1E8000"    -> 0x1E8000        ""   / "0x" -> rejected
//   "0X1E8000"   -> 0x1E8000        "-1"        -> rejected
//
// Surrounding ASCII whitespace is ignored. A value that does not fit the
// requested type is rejected rather than wrapped or truncated. Returns false
// and leaves *out untouched when the string does not parse.
bool ParseHexString(absl::string_view str, uint64_t* out);
bool ParseHexString(absl::string_view str, uint32_t* out);
bool ParseHexString(absl::string_view str, int* out);

}  // namespace util
}  // namespace yaze

#endif