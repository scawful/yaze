
#include "bps.h"

#include <zlib.h>

#include <cstdint>
#include <vector>

namespace yaze {
namespace util {

namespace {

uint32_t crc32(const std::vector<uint8_t> &data) {
  uint32_t crc = ::crc32(0L, Z_NULL, 0);
  return ::crc32(crc, data.data(), data.size());
}

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

void CreateBpsPatch(const std::vector<uint8_t> &source,
                    const std::vector<uint8_t> &target,
                    std::vector<uint8_t> &patch) {
  patch.clear();
  patch.insert(patch.end(), {'B', 'P', 'S', '1'});

  encode(source.size(), patch);
  encode(target.size(), patch);
  encode(0, patch);  // No metadata

  size_t source_offset = 0;
  size_t target_offset = 0;
  int64_t source_rel_offset = 0;
  int64_t target_rel_offset = 0;

  while (target_offset < target.size()) {
    if (source_offset < source.size() &&
        source[source_offset] == target[target_offset]) {
      size_t length = 0;
      while (source_offset + length < source.size() &&
             target_offset + length < target.size() &&
             source[source_offset + length] == target[target_offset + length]) {
        length++;
      }
      encode((length - 1) << 2 | 0, patch);  // SourceRead
      source_offset += length;
      target_offset += length;
    } else {
      size_t length = 0;
      while (
          target_offset + length < target.size() &&
          (source_offset + length >= source.size() ||
           source[source_offset + length] != target[target_offset + length])) {
        length++;
      }
      if (length > 0) {
        encode((length - 1) << 2 | 1, patch);  // TargetRead
        for (size_t i = 0; i < length; i++) {
          patch.push_back(target[target_offset + i]);
        }
        target_offset += length;
      }
    }

    // SourceCopy
    if (source_offset < source.size()) {
      size_t length = 0;
      int64_t offset = source_offset - source_rel_offset;
      while (source_offset + length < source.size() &&
             target_offset + length < target.size() &&
             source[source_offset + length] == target[target_offset + length]) {
        length++;
      }
      if (length > 0) {
        encode((length - 1) << 2 | 2, patch);
        encode((offset < 0 ? 1 : 0) | (abs(offset) << 1), patch);
        source_offset += length;
        target_offset += length;
        source_rel_offset = source_offset;
      }
    }

    // TargetCopy
    if (target_offset > 0) {
      size_t length = 0;
      int64_t offset = target_offset - target_rel_offset;
      while (target_offset + length < target.size() &&
             target[target_offset - 1] == target[target_offset + length]) {
        length++;
      }
      if (length > 0) {
        encode((length - 1) << 2 | 3, patch);
        encode((offset < 0 ? 1 : 0) | (abs(offset) << 1), patch);
        target_offset += length;
        target_rel_offset = target_offset;
      }
    }
  }

  patch.resize(patch.size() + 12);  // Make space for the checksums
  uint32_t source_checksum = crc32(source);
  uint32_t target_checksum = crc32(target);
  uint32_t patch_checksum = crc32(patch);

  memcpy(patch.data() + patch.size() - 12, &source_checksum, sizeof(uint32_t));
  memcpy(patch.data() + patch.size() - 8, &target_checksum, sizeof(uint32_t));
  memcpy(patch.data() + patch.size() - 4, &patch_checksum, sizeof(uint32_t));
}

void ApplyBpsPatch(const std::vector<uint8_t> &source,
                   const std::vector<uint8_t> &patch,
                   std::vector<uint8_t> &target) {
  if (patch.size() < 4 || patch[0] != 'B' || patch[1] != 'P' ||
      patch[2] != 'S' || patch[3] != '1') {
    throw std::runtime_error("Invalid patch format");
  }

  size_t patch_offset = 4;
  uint64_t target_size = decode(patch, patch_offset);
  uint64_t metadata_size = decode(patch, patch_offset);
  patch_offset += metadata_size;

  target.resize(target_size);
  size_t source_offset = 0;
  size_t target_offset = 0;
  int64_t source_rel_offset = 0;
  int64_t target_rel_offset = 0;

  while (patch_offset < patch.size() - 12) {
    uint64_t data = decode(patch, patch_offset);
    uint64_t command = data & 3;
    uint64_t length = (data >> 2) + 1;

    switch (command) {
      case 0:  // SourceRead
        while (length--) {
          target[target_offset++] = source[source_offset++];
        }
        break;
      case 1:  // TargetRead
        while (length--) {
          target[target_offset++] = patch[patch_offset++];
        }
        break;
      case 2:  // SourceCopy
      {
        int64_t offsetData = decode(patch, patch_offset);
        source_rel_offset += (offsetData & 1 ? -1 : +1) * (offsetData >> 1);
        while (length--) {
          target[target_offset++] = source[source_rel_offset++];
        }
      } break;
      case 3:  // TargetCopy
      {
        uint64_t offsetData = decode(patch, patch_offset);
        target_rel_offset += (offsetData & 1 ? -1 : +1) * (offsetData >> 1);
        while (length--) {
          target[target_offset++] = target[target_rel_offset++];
        }
      }
      default:
        throw std::runtime_error("Invalid patch command");
    }
  }

  uint32_t source_checksum;
  uint32_t target_checksum;
  uint32_t patch_checksum;
  memcpy(&source_checksum, patch.data() + patch.size() - 12, sizeof(uint32_t));
  memcpy(&target_checksum, patch.data() + patch.size() - 8, sizeof(uint32_t));
  memcpy(&patch_checksum, patch.data() + patch.size() - 4, sizeof(uint32_t));

  if (source_checksum != crc32(source) || target_checksum != crc32(target) ||
      patch_checksum !=
          crc32(std::vector<uint8_t>(patch.begin(), patch.end() - 4))) {
    throw std::runtime_error("Checksum mismatch");
  }
}

}  // namespace util
}  // namespace yaze