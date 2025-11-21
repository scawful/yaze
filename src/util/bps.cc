#include "bps.h"

#include <cstring>
#include <vector>

#include "absl/status/status.h"

// zlib dependency removed

namespace yaze {
namespace util {

namespace {

// Simple CRC32 implementation (BPS patches disabled)
uint32_t crc32(const std::vector<uint8_t>& data) {
  static const uint32_t crc_table[256] = {
      0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
      0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
      0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
      0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
      0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
      0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
      0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
      0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
      0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
      0xd681feba, 0x55610c86, 0x3f8355c9, 0x48b2364b, 0xd80d2bda, 0xaf0a1b4c,
      0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
      0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703,
      0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04,
      0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d, 0x9b64c2b0, 0xec63f226,
      0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
      0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d,
      0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e,
      0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777, 0x88085ae6, 0xff0f6a70,
      0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
      0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7,
      0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0,
      0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9, 0xbdbdf21c, 0xcabac28a,
      0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
      0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1,
      0x5a05df1b, 0x2d02ef8d};

  uint32_t crc = 0xFFFFFFFF;
  for (uint8_t byte : data) {
    crc = crc_table[(crc ^ byte) & 0xFF] ^ (crc >> 8);
  }
  return crc ^ 0xFFFFFFFF;
}

void encode(uint64_t data, std::vector<uint8_t>& output) {
  while (true) {
    uint8_t x = data & 0x7f;
    data >>= 7;
    if (data == 0) {
      output.push_back(0x80 | x);
      break;
    }
    output.push_back(x);
  }
}

uint64_t decode(const std::vector<uint8_t>& data, size_t& offset) {
  uint64_t result = 0;
  int shift = 0;
  while (offset < data.size()) {
    uint8_t x = data[offset++];
    result |= (uint64_t)(x & 0x7f) << shift;
    if (x & 0x80)
      break;
    shift += 7;
  }
  return result;
}

}  // namespace

// BPS patch functionality disabled - returning error status
absl::Status ApplyBpsPatch(const std::vector<uint8_t>& source,
                           const std::vector<uint8_t>& patch,
                           std::vector<uint8_t>& output) {
  return absl::UnimplementedError("BPS patch functionality has been disabled");
}

absl::Status CreateBpsPatch(const std::vector<uint8_t>& source,
                            const std::vector<uint8_t>& target,
                            std::vector<uint8_t>& patch) {
  return absl::UnimplementedError("BPS patch functionality has been disabled");
}

}  // namespace util
}  // namespace yaze