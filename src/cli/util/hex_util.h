#ifndef YAZE_SRC_CLI_UTIL_HEX_UTIL_H_
#define YAZE_SRC_CLI_UTIL_HEX_UTIL_H_

#include <climits>
#include <cstdint>
#include <cstdlib>
#include <string>

#include "absl/strings/string_view.h"

namespace yaze {
namespace cli {
namespace util {

// Portable hex string parser - works across all abseil versions
// Replaces absl::SimpleHexAtoi which may not be available in older versions
inline bool ParseHexString(absl::string_view str, int* out) {
  if (str.empty()) return false;

  // Skip optional 0x prefix
  if (str.size() >= 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
    str = str.substr(2);
  }
  if (str.empty()) return false;

  char* end = nullptr;
  std::string str_copy(str.data(), str.size());
  long result = std::strtol(str_copy.c_str(), &end, 16);

  if (end == str_copy.c_str() || *end != '\0') return false;
  if (result < INT_MIN || result > INT_MAX) return false;

  *out = static_cast<int>(result);
  return true;
}

inline bool ParseHexString(absl::string_view str, uint32_t* out) {
  if (str.empty()) return false;

  // Skip optional 0x prefix
  if (str.size() >= 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
    str = str.substr(2);
  }
  if (str.empty()) return false;

  char* end = nullptr;
  std::string str_copy(str.data(), str.size());
  unsigned long result = std::strtoul(str_copy.c_str(), &end, 16);

  if (end == str_copy.c_str() || *end != '\0') return false;
  if (result > UINT32_MAX) return false;

  *out = static_cast<uint32_t>(result);
  return true;
}

}  // namespace util
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_UTIL_HEX_UTIL_H_
