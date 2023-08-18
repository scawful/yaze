#include "cli/patch.h"

#include <zlib.h>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

namespace yaze {
namespace cli {
  
void encode(uint64_t data, std::vector<uint8_t>& output) {
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

uint64_t decode(const std::vector<uint8_t>& input, size_t& offset) {
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

uint32_t crc32(const std::vector<uint8_t>& data) {
  uint32_t crc = ::crc32(0L, Z_NULL, 0);
  return ::crc32(crc, data.data(), data.size());
}

void CreateBpsPatch(const std::vector<uint8_t>& source,
                    const std::vector<uint8_t>& target,
                    std::vector<uint8_t>& patch) {
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

void ApplyBpsPatch(const std::vector<uint8_t>& source,
                   const std::vector<uint8_t>& patch,
                   std::vector<uint8_t>& target) {
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

}  // namespace cli
}  // namespace yaze