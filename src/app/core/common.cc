#include "common.h"

#include <imgui/imgui.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <stack>
#include <string>

#include "absl/strings/str_format.h"

namespace yaze {
namespace app {
namespace core {

std::shared_ptr<ExperimentFlags::Flags> ExperimentFlags::flags_;

std::string UppercaseHexByte(uint8_t byte, bool leading) {
  if (leading) {
    std::string result = absl::StrFormat("0x%02X", byte);
    return result;
  }
  std::string result = absl::StrFormat("%02X", byte);
  return result;
}
std::string UppercaseHexWord(uint16_t word) {
  std::string result = absl::StrFormat("0x%04X", word);
  return result;
}
std::string UppercaseHexLong(uint32_t dword) {
  std::string result = absl::StrFormat("0x%06X", dword);
  return result;
}

uint32_t SnesToPc(uint32_t addr) {
  if (addr >= 0x808000) {
    addr -= 0x808000;
  }
  uint32_t temp = (addr & 0x7FFF) + ((addr / 2) & 0xFF8000);
  return (temp + 0x0);
}

uint32_t PcToSnes(uint32_t addr) {
  uint8_t *b = reinterpret_cast<uint8_t *>(&addr);
  b[2] = static_cast<uint8_t>(b[2] * 2);

  if (b[1] >= 0x80) {
    b[2] += 1;
  } else {
    b[1] += 0x80;
  }

  return addr;
}

uint32_t MapBankToWordAddress(uint8_t bank, uint16_t addr) {
  uint32_t result = 0;
  result = (bank << 16) | addr;
  return result;
}

int AddressFromBytes(uint8_t addr1, uint8_t addr2, uint8_t addr3) {
  return (addr1 << 16) | (addr2 << 8) | addr3;
}

// hextodec has been imported from SNESDisasm to parse hex numbers
int HexToDec(char *input, int length) {
  int result = 0;
  int value;
  int ceiling = length - 1;
  int power16 = 16;

  int j = ceiling;

  for (; j >= 0; j--) {
    if (input[j] >= 'A' && input[j] <= 'F') {
      value = input[j] - 'F';
      value += 15;
    } else {
      value = input[j] - '9';
      value += 9;
    }

    if (j == ceiling) {
      result += value;
      continue;
    }

    result += (value * power16);
    power16 *= 16;
  }

  return result;
}

bool StringReplace(std::string &str, const std::string &from,
                   const std::string &to) {
  size_t start = str.find(from);
  if (start == std::string::npos) return false;

  str.replace(start, from.length(), to);
  return true;
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
void stle16b(uint8_t *const p_arr, uint16_t const p_val) {
  stle0(p_arr, p_val);
  stle1(p_arr, p_val);
}
// "Store little endian 16-bit value using a byte pointer, offset by an
// index before dereferencing"
void stle16b_i(uint8_t *const p_arr, size_t const p_index,
               uint16_t const p_val) {
  stle16b(p_arr + (p_index * 2), p_val);
}
// "load little endian value at the given byte offset and shift to get its
// value relative to the base offset (powers of 256, essentially)"
unsigned ldle(uint8_t const *const p_arr, unsigned const p_index) {
  uint32_t v = p_arr[p_index];

  v <<= (8 * p_index);

  return v;
}

// Helper function to get the first byte in a little endian number
uint32_t ldle0(uint8_t const *const p_arr) { return ldle(p_arr, 0); }

// Helper function to get the second byte in a little endian number
uint32_t ldle1(uint8_t const *const p_arr) { return ldle(p_arr, 1); }

// Helper function to get the third byte in a little endian number
uint32_t ldle2(uint8_t const *const p_arr) { return ldle(p_arr, 2); }

// Helper function to get the third byte in a little endian number
uint32_t ldle3(uint8_t const *const p_arr) { return ldle(p_arr, 3); }
// Load little endian halfword (16-bit) dereferenced from
uint16_t ldle16b(uint8_t const *const p_arr) {
  uint16_t v = 0;

  v |= (ldle0(p_arr) | ldle1(p_arr));

  return v;
}
// Load little endian halfword (16-bit) dereferenced from an arrays of bytes.
// This version provides an index that will be multiplied by 2 and added to the
// base address.
uint16_t ldle16b_i(uint8_t const *const p_arr, size_t const p_index) {
  return ldle16b(p_arr + (2 * p_index));
}

// Initialize the static member
std::stack<ImGuiID> ImGuiIdIssuer::idStack;

int Get24LocalFromPC(uint8_t *data, int addr, bool pc) {
  int ret = (PcToSnes(addr) & 0xFF0000) | (data[addr + 1] << 8) | data[addr];
  if (pc) {
    return SnesToPc(ret);
  } else {
    return ret;
  }
}

}  // namespace core
}  // namespace app
}  // namespace yaze
