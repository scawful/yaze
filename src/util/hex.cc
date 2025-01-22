#include "hex.h"

#include <string>

#include "absl/strings/str_cat"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"

namespace yaze {
namespace util {

namespace {

void HandleHexStringParams(const std::string &hex,
                           const HexStringParams &params) {
  std::string result = hex;
  switch (params.prefix) {
    case HexStringParams::Prefix::kDollar:
      result = absl::StrCat("$", result);
      break;
    case HexStringParams::Prefix::kHash:
      result = absl::StrCat("#", result);
      break;
    case HexStringParams::Prefix::k0x:
      result = absl::StrCat("0x", result);
    case HexStringParams::Prefix::kNone:
    default:
      break;
  }
}
}  // namespace

std::string HexByte(uint8_t byte, HexStringParams params) {
  std::string result;
  const static std::string kLowerFormat = "%02x";
  const static std::string kUpperFormat = "%02X";
  if (params.uppercase) {
    result = absl::StrFormat(kUpperFormat.c_str(), byte);
  } else {
    result = absl::StrFormat(kLowerFormat.c_str(), byte);
  }
  HandleHexStringParams(result, params);
  return result;
}

std::string HexWord(uint16_t word, HexStringParams params) {
  std::string result;
  const static std::string kLowerFormat = "%04x";
  const static std::string kUpperFormat = "%04X";
  if (params.uppercase) {
    result = absl::StrFormat(kUpperFormat.c_str(), word);
  } else {
    result = absl::StrFormat(kLowerFormat.c_str(), word);
  }
  HandleHexStringParams(result, params);
  return result;
}

std::string HexLong(uint32_t dword, HexStringParams params) {
  std::string result;
  const static std::string kLowerFormat = "%06x";
  const static std::string kUpperFormat = "%06X";
  if (params.uppercase) {
    result = absl::StrFormat(kUpperFormat.c_str(), dword);
  } else {
    result = absl::StrFormat(kLowerFormat.c_str(), dword);
  }
  HandleHexStringParams(result, params);
  return result;
}

std::string HexLongLong(uint64_t qword, HexStringParams params) {
  std::string result;
  const static std::string kLowerFormat = "%08x";
  const static std::string kUpperFormat = "%08X";
  if (params.uppercase) {
    result = absl::StrFormat(kUpperFormat.c_str(), qword);
  } else {
    result = absl::StrFormat(kLowerFormat.c_str(), qword);
  }
  HandleHexStringParams(result, params);
  return result;
}

}  // namespace util
}  // namespace yaze