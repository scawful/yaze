#include "common.h"

#include <zlib.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <stack>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "imgui/imgui.h"

namespace yaze {
namespace app {
namespace core {

namespace {

void encode(uint64_t data, std::vector<uint8_t> &output) {
  while (true) {
    uint8_t x = data & 0x7f;
    data >>= 7;
    if (data == 0) {
      output.push_back(0x80 | x);
      break;
    }
    output.push_back(x);
    data--;
  }
}

uint64_t decode(const std::vector<uint8_t> &input, size_t &offset) {
  uint64_t data = 0;
  uint64_t shift = 1;
  while (true) {
    uint8_t x = input[offset++];
    data += (x & 0x7f) * shift;
    if (x & 0x80) break;
    shift <<= 7;
    data += shift;
  }
  return data;
}

}  // namespace

std::shared_ptr<ExperimentFlags::Flags> ExperimentFlags::flags_;

constexpr uint32_t kFastRomRegion = 0x808000;

inline uint32_t SnesToPc(uint32_t addr) noexcept {
  if (addr >= kFastRomRegion) {
    addr -= kFastRomRegion;
  }
  uint32_t temp = (addr & 0x7FFF) + ((addr / 2) & 0xFF8000);
  return (temp + 0x0);
}

inline uint32_t PcToSnes(uint32_t addr) {
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

int AddressFromBytes(uint8_t bank, uint8_t high, uint8_t low) noexcept {
  return (bank << 16) | (high << 8) | low;
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

uint32_t Get24LocalFromPC(uint8_t *data, int addr, bool pc) {
  uint32_t ret =
      (PcToSnes(addr) & 0xFF0000) | (data[addr + 1] << 8) | data[addr];
  if (pc) {
    return SnesToPc(ret);
  }
  return ret;
}

uint32_t crc32(const std::vector<uint8_t> &data) {
  uint32_t crc = ::crc32(0L, Z_NULL, 0);
  return ::crc32(crc, data.data(), data.size());
}

void CreateBpsPatch(const std::vector<uint8_t> &source,
                    const std::vector<uint8_t> &target,
                    std::vector<uint8_t> &patch) {
  patch.clear();
  patch.insert(patch.end(), {'B', 'P', 'S', '1'});

  encode(source.size(), patch);
  encode(target.size(), patch);
  encode(0, patch);  // No metadata

  size_t sourceOffset = 0;
  size_t targetOffset = 0;
  int64_t sourceRelOffset = 0;
  int64_t targetRelOffset = 0;

  while (targetOffset < target.size()) {
    if (sourceOffset < source.size() &&
        source[sourceOffset] == target[targetOffset]) {
      size_t length = 0;
      while (sourceOffset + length < source.size() &&
             targetOffset + length < target.size() &&
             source[sourceOffset + length] == target[targetOffset + length]) {
        length++;
      }
      encode((length - 1) << 2 | 0, patch);  // SourceRead
      sourceOffset += length;
      targetOffset += length;
    } else {
      size_t length = 0;
      while (targetOffset + length < target.size() &&
             (sourceOffset + length >= source.size() ||
              source[sourceOffset + length] != target[targetOffset + length])) {
        length++;
      }
      if (length > 0) {
        encode((length - 1) << 2 | 1, patch);  // TargetRead
        for (size_t i = 0; i < length; i++) {
          patch.push_back(target[targetOffset + i]);
        }
        targetOffset += length;
      }
    }

    // SourceCopy
    if (sourceOffset < source.size()) {
      size_t length = 0;
      int64_t offset = sourceOffset - sourceRelOffset;
      while (sourceOffset + length < source.size() &&
             targetOffset + length < target.size() &&
             source[sourceOffset + length] == target[targetOffset + length]) {
        length++;
      }
      if (length > 0) {
        encode((length - 1) << 2 | 2, patch);
        encode((offset < 0 ? 1 : 0) | (abs(offset) << 1), patch);
        sourceOffset += length;
        targetOffset += length;
        sourceRelOffset = sourceOffset;
      }
    }

    // TargetCopy
    if (targetOffset > 0) {
      size_t length = 0;
      int64_t offset = targetOffset - targetRelOffset;
      while (targetOffset + length < target.size() &&
             target[targetOffset - 1] == target[targetOffset + length]) {
        length++;
      }
      if (length > 0) {
        encode((length - 1) << 2 | 3, patch);
        encode((offset < 0 ? 1 : 0) | (abs(offset) << 1), patch);
        targetOffset += length;
        targetRelOffset = targetOffset;
      }
    }
  }

  patch.resize(patch.size() + 12);  // Make space for the checksums
  uint32_t sourceChecksum = crc32(source);
  uint32_t targetChecksum = crc32(target);
  uint32_t patchChecksum = crc32(patch);

  memcpy(patch.data() + patch.size() - 12, &sourceChecksum, sizeof(uint32_t));
  memcpy(patch.data() + patch.size() - 8, &targetChecksum, sizeof(uint32_t));
  memcpy(patch.data() + patch.size() - 4, &patchChecksum, sizeof(uint32_t));
}

void ApplyBpsPatch(const std::vector<uint8_t> &source,
                   const std::vector<uint8_t> &patch,
                   std::vector<uint8_t> &target) {
  if (patch.size() < 4 || patch[0] != 'B' || patch[1] != 'P' ||
      patch[2] != 'S' || patch[3] != '1') {
    throw std::runtime_error("Invalid patch format");
  }

  size_t patchOffset = 4;
  uint64_t sourceSize = decode(patch, patchOffset);
  uint64_t targetSize = decode(patch, patchOffset);
  uint64_t metadataSize = decode(patch, patchOffset);
  patchOffset += metadataSize;

  target.resize(targetSize);
  size_t sourceOffset = 0;
  size_t targetOffset = 0;
  int64_t sourceRelOffset = 0;
  int64_t targetRelOffset = 0;

  while (patchOffset < patch.size() - 12) {
    uint64_t data = decode(patch, patchOffset);
    uint64_t command = data & 3;
    uint64_t length = (data >> 2) + 1;

    switch (command) {
      case 0:  // SourceRead
        while (length--) {
          target[targetOffset++] = source[sourceOffset++];
        }
        break;
      case 1:  // TargetRead
        while (length--) {
          target[targetOffset++] = patch[patchOffset++];
        }
        break;
      case 2:  // SourceCopy
      {
        int64_t offsetData = decode(patch, patchOffset);
        sourceRelOffset += (offsetData & 1 ? -1 : +1) * (offsetData >> 1);
        while (length--) {
          target[targetOffset++] = source[sourceRelOffset++];
        }
      } break;
      case 3:  // TargetCopy
      {
        uint64_t offsetData = decode(patch, patchOffset);
        targetRelOffset += (offsetData & 1 ? -1 : +1) * (offsetData >> 1);
        while (length--) {
          target[targetOffset++] = target[targetRelOffset++];
        }
      }
      default:
        throw std::runtime_error("Invalid patch command");
    }
  }

  uint32_t sourceChecksum;
  uint32_t targetChecksum;
  uint32_t patchChecksum;
  memcpy(&sourceChecksum, patch.data() + patch.size() - 12, sizeof(uint32_t));
  memcpy(&targetChecksum, patch.data() + patch.size() - 8, sizeof(uint32_t));
  memcpy(&patchChecksum, patch.data() + patch.size() - 4, sizeof(uint32_t));

  if (sourceChecksum != crc32(source) || targetChecksum != crc32(target) ||
      patchChecksum !=
          crc32(std::vector<uint8_t>(patch.begin(), patch.end() - 4))) {
    throw std::runtime_error("Checksum mismatch");
  }
}

}  // namespace core
}  // namespace app
}  // namespace yaze
