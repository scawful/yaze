#include "common.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"

namespace yaze {
namespace core {

namespace {

// "load little endian value at the given byte offset and shift to get its
// value relative to the base offset (powers of 256, essentially)"
unsigned ldle(uint8_t const *const p_arr, unsigned const p_index) {
  uint32_t v = p_arr[p_index];
  v <<= (8 * p_index);
  return v;
}

void stle(uint8_t *const p_arr, size_t const p_index, unsigned const p_val) {
  uint8_t v = (p_val >> (8 * p_index)) & 0xff;
  p_arr[p_index] = v;
}

void stle0(uint8_t *const p_arr, unsigned const p_val) {
  stle(p_arr, 0, p_val);
}

void stle1(uint8_t *const p_arr, unsigned const p_val) {
  stle(p_arr, 1, p_val);
}

void stle2(uint8_t *const p_arr, unsigned const p_val) {
  stle(p_arr, 2, p_val);
}

void stle3(uint8_t *const p_arr, unsigned const p_val) {
  stle(p_arr, 3, p_val);
}

// Helper function to get the first byte in a little endian number
uint32_t ldle0(uint8_t const *const p_arr) { return ldle(p_arr, 0); }

// Helper function to get the second byte in a little endian number
uint32_t ldle1(uint8_t const *const p_arr) { return ldle(p_arr, 1); }

// Helper function to get the third byte in a little endian number
uint32_t ldle2(uint8_t const *const p_arr) { return ldle(p_arr, 2); }

// Helper function to get the third byte in a little endian number
uint32_t ldle3(uint8_t const *const p_arr) { return ldle(p_arr, 3); }

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

bool StringReplace(std::string &str, const std::string &from,
                   const std::string &to) {
  size_t start = str.find(from);
  if (start == std::string::npos) return false;

  str.replace(start, from.length(), to);
  return true;
}

uint32_t Get24LocalFromPC(uint8_t *data, int addr, bool pc) {
  uint32_t ret =
      (PcToSnes(addr) & 0xFF0000) | (data[addr + 1] << 8) | data[addr];
  if (pc) {
    return SnesToPc(ret);
  }
  return ret;
}

void stle16b_i(uint8_t *const p_arr, size_t const p_index,
               uint16_t const p_val) {
  stle16b(p_arr + (p_index * 2), p_val);
}

void stle16b(uint8_t *const p_arr, uint16_t const p_val) {
  stle0(p_arr, p_val);
  stle1(p_arr, p_val);
}

uint16_t ldle16b(uint8_t const *const p_arr) {
  uint16_t v = 0;
  v |= (ldle0(p_arr) | ldle1(p_arr));
  return v;
}

uint16_t ldle16b_i(uint8_t const *const p_arr, size_t const p_index) {
  return ldle16b(p_arr + (2 * p_index));
}

}  // namespace core
}  // namespace yaze
