#include "hex.h"

#include <cctype>
#include <limits>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"

namespace yaze {
namespace util {

namespace {

void HandleHexStringParams(std::string& hex, const HexStringParams& params) {
  switch (params.prefix) {
    case HexStringParams::Prefix::kDollar:
      hex = absl::StrCat("$", hex);
      break;
    case HexStringParams::Prefix::kHash:
      hex = absl::StrCat("#", hex);
      break;
    case HexStringParams::Prefix::k0x:
      hex = absl::StrCat("0x", hex);
    case HexStringParams::Prefix::kNone:
    default:
      break;
  }
}
}  // namespace

std::string HexByte(uint8_t byte, HexStringParams params) {
  std::string result;
  if (params.uppercase) {
    result = absl::StrFormat("%02X", byte);
  } else {
    result = absl::StrFormat("%02x", byte);
  }
  HandleHexStringParams(result, params);
  return result;
}

std::string HexWord(uint16_t word, HexStringParams params) {
  std::string result;
  if (params.uppercase) {
    result = absl::StrFormat("%04X", word);
  } else {
    result = absl::StrFormat("%04x", word);
  }
  HandleHexStringParams(result, params);
  return result;
}

std::string HexLong(uint32_t dword, HexStringParams params) {
  std::string result;
  if (params.uppercase) {
    result = absl::StrFormat("%06X", dword);
  } else {
    result = absl::StrFormat("%06x", dword);
  }
  HandleHexStringParams(result, params);
  return result;
}

std::string HexLongLong(uint64_t qword, HexStringParams params) {
  std::string result;
  if (params.uppercase) {
    result = absl::StrFormat("%08X", qword);
  } else {
    result = absl::StrFormat("%08x", qword);
  }
  HandleHexStringParams(result, params);
  return result;
}

namespace {

// Parses the whole string as hexadecimal. No sign is accepted: a negative
// value would wrap into the unsigned result, which is how out-of-range
// manifest addresses used to load as plausible ones.
bool ParseHexDigits(absl::string_view str, uint64_t max_value, uint64_t* out) {
  // Trim surrounding ASCII whitespace, matching the previous strtoul-based
  // parsers, which skipped leading whitespace.
  while (!str.empty() &&
         std::isspace(static_cast<unsigned char>(str.front()))) {
    str.remove_prefix(1);
  }
  while (!str.empty() && std::isspace(static_cast<unsigned char>(str.back()))) {
    str.remove_suffix(1);
  }

  if (str.empty()) {
    return false;
  }
  if (str.front() == '$') {
    str.remove_prefix(1);
  } else if (str.size() >= 2 && str[0] == '0' &&
             (str[1] == 'x' || str[1] == 'X')) {
    str.remove_prefix(2);
  }
  if (str.empty()) {
    return false;
  }

  uint64_t value = 0;
  for (const char c : str) {
    const auto byte = static_cast<unsigned char>(c);
    if (!std::isxdigit(byte)) {
      return false;
    }
    int digit = 0;
    if (byte >= '0' && byte <= '9') {
      digit = byte - '0';
    } else if (byte >= 'a' && byte <= 'f') {
      digit = byte - 'a' + 10;
    } else {
      digit = byte - 'A' + 10;
    }
    // Reject before the shift so the overflow cannot wrap.
    if (value > (max_value - static_cast<uint64_t>(digit)) / 16) {
      return false;
    }
    value = value * 16 + static_cast<uint64_t>(digit);
  }

  *out = value;
  return true;
}

}  // namespace

bool ParseHexString(absl::string_view str, uint64_t* out) {
  uint64_t value = 0;
  if (!ParseHexDigits(str, std::numeric_limits<uint64_t>::max(), &value)) {
    return false;
  }
  *out = value;
  return true;
}

bool ParseHexString(absl::string_view str, uint32_t* out) {
  uint64_t value = 0;
  if (!ParseHexDigits(str, std::numeric_limits<uint32_t>::max(), &value)) {
    return false;
  }
  *out = static_cast<uint32_t>(value);
  return true;
}

bool ParseHexString(absl::string_view str, int* out) {
  uint64_t value = 0;
  if (!ParseHexDigits(str,
                      static_cast<uint64_t>(std::numeric_limits<int>::max()),
                      &value)) {
    return false;
  }
  *out = static_cast<int>(value);
  return true;
}

}  // namespace util
}  // namespace yaze
