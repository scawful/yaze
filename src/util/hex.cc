#include "hex.h"

#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"

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

}  // namespace util
}  // namespace yaze