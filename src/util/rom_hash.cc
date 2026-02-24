#include "util/rom_hash.h"

#include <cstring>
#include <fstream>
#include <ios>
#include <vector>

#include "absl/strings/str_format.h"

namespace yaze::util {

namespace {

// CRC32 lookup table (polynomial 0xEDB88320).
constexpr uint32_t kCrc32Table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdede86c5, 0x47d7857f, 0x30d095e9,
    0xbddc8a1c, 0xcadbb48a, 0x53d39330, 0x24d4a3a6, 0xba906995, 0xcdcf094b,
    0x54c65941, 0x23c3b9d7, 0xb364a7ae, 0xc4632738, 0x5d6a1682, 0x2a6d0614,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d};

}  // namespace

uint32_t CalculateCrc32(const uint8_t* data, size_t size) {
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < size; ++i) {
    crc = kCrc32Table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
  }
  return crc ^ 0xFFFFFFFF;
}

std::string ComputeRomHash(const uint8_t* data, size_t size) {
  return absl::StrFormat("%08x", CalculateCrc32(data, size));
}

// ---------------------------------------------------------------------------
// SHA-1 computation (portable, no platform dependencies)
//
// Based on RFC 3174. This is a minimal self-contained implementation so that
// ComputeSha1Hex always returns a 40-character lowercase hex digest on every
// platform (macOS, Linux, Windows).
// ---------------------------------------------------------------------------

namespace {

struct Sha1State {
  uint32_t h[5];
  uint64_t total_bytes;
  uint8_t buf[64];
  size_t buf_len;
};

inline uint32_t LeftRotate(uint32_t val, unsigned bits) {
  return (val << bits) | (val >> (32 - bits));
}

void Sha1ProcessBlock(Sha1State& state, const uint8_t block[64]) {
  uint32_t w[80];
  for (int i = 0; i < 16; ++i) {
    w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
            (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
            (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
            (static_cast<uint32_t>(block[i * 4 + 3]));
  }
  for (int i = 16; i < 80; ++i) {
    w[i] = LeftRotate(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
  }
  uint32_t a = state.h[0], b = state.h[1], c = state.h[2];
  uint32_t d = state.h[3], e = state.h[4];
  for (int i = 0; i < 80; ++i) {
    uint32_t f, k;
    if (i < 20) {
      f = (b & c) | ((~b) & d);
      k = 0x5A827999;
    } else if (i < 40) {
      f = b ^ c ^ d;
      k = 0x6ED9EBA1;
    } else if (i < 60) {
      f = (b & c) | (b & d) | (c & d);
      k = 0x8F1BBCDC;
    } else {
      f = b ^ c ^ d;
      k = 0xCA62C1D6;
    }
    uint32_t temp = LeftRotate(a, 5) + f + e + k + w[i];
    e = d;
    d = c;
    c = LeftRotate(b, 30);
    b = a;
    a = temp;
  }
  state.h[0] += a;
  state.h[1] += b;
  state.h[2] += c;
  state.h[3] += d;
  state.h[4] += e;
}

void Sha1Init(Sha1State& state) {
  state.h[0] = 0x67452301;
  state.h[1] = 0xEFCDAB89;
  state.h[2] = 0x98BADCFE;
  state.h[3] = 0x10325476;
  state.h[4] = 0xC3D2E1F0;
  state.total_bytes = 0;
  state.buf_len = 0;
}

void Sha1Update(Sha1State& state, const uint8_t* data, size_t len) {
  state.total_bytes += len;
  size_t offset = 0;
  if (state.buf_len > 0) {
    size_t fill = 64 - state.buf_len;
    if (len < fill) {
      std::memcpy(state.buf + state.buf_len, data, len);
      state.buf_len += len;
      return;
    }
    std::memcpy(state.buf + state.buf_len, data, fill);
    Sha1ProcessBlock(state, state.buf);
    offset = fill;
    state.buf_len = 0;
  }
  while (offset + 64 <= len) {
    Sha1ProcessBlock(state, data + offset);
    offset += 64;
  }
  if (offset < len) {
    state.buf_len = len - offset;
    std::memcpy(state.buf, data + offset, state.buf_len);
  }
}

void Sha1Final(Sha1State& state, uint8_t digest[20]) {
  uint64_t total_bits = state.total_bytes * 8;
  uint8_t pad = 0x80;
  Sha1Update(state, &pad, 1);
  pad = 0x00;
  while (state.buf_len != 56) {
    Sha1Update(state, &pad, 1);
  }
  uint8_t len_be[8];
  for (int i = 7; i >= 0; --i) {
    len_be[i] = static_cast<uint8_t>(total_bits & 0xFF);
    total_bits >>= 8;
  }
  Sha1Update(state, len_be, 8);
  for (int i = 0; i < 5; ++i) {
    digest[i * 4] = static_cast<uint8_t>((state.h[i] >> 24) & 0xFF);
    digest[i * 4 + 1] = static_cast<uint8_t>((state.h[i] >> 16) & 0xFF);
    digest[i * 4 + 2] = static_cast<uint8_t>((state.h[i] >> 8) & 0xFF);
    digest[i * 4 + 3] = static_cast<uint8_t>(state.h[i] & 0xFF);
  }
}

}  // namespace

std::string ComputeSha1Hex(const uint8_t* data, size_t size) {
  if (size > 0 && data == nullptr) {
    return {};
  }
  Sha1State state;
  Sha1Init(state);
  Sha1Update(state, data, size);
  uint8_t digest[20];
  Sha1Final(state, digest);
  std::string result;
  result.reserve(40);
  for (int i = 0; i < 20; ++i) {
    result += absl::StrFormat("%02x", digest[i]);
  }
  return result;
}

std::string ComputeFileSha1Hex(const std::string& path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    return {};
  }
  auto size = file.tellg();
  if (size < 0) {
    return {};
  }
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> buffer(static_cast<size_t>(size));
  if (size > 0) {
    file.read(reinterpret_cast<char*>(buffer.data()), size);
  }
  if (!file && !file.eof()) {
    return {};
  }
  const uint8_t* data = buffer.empty() ? nullptr : buffer.data();
  return ComputeSha1Hex(data, buffer.size());
}

}  // namespace yaze::util
