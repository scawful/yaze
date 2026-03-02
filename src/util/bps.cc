#include "bps.h"

#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"

namespace yaze {
namespace util {

namespace {

uint32_t CalculateCrc32(const uint8_t* data, size_t size) {
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < size; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) {
      if (crc & 1) {
        crc = (crc >> 1) ^ 0xEDB88320u;
      } else {
        crc >>= 1;
      }
    }
  }
  return ~crc;
}

uint32_t CalculateCrc32(const std::vector<uint8_t>& data) {
  return CalculateCrc32(data.data(), data.size());
}

void WriteVariableLength(std::vector<uint8_t>& output, uint64_t value) {
  while (true) {
    uint8_t byte = value & 0x7F;
    value >>= 7;
    if (value == 0) {
      output.push_back(byte | 0x80);
      break;
    }
    output.push_back(byte);
    value--;
  }
}

uint64_t ReadVariableLength(const uint8_t* data, size_t size, size_t& offset) {
  uint64_t result = 0;
  uint64_t shift = 1;
  while (offset < size) {
    uint8_t byte = data[offset++];
    result += (byte & 0x7F) * shift;
    if (byte & 0x80)
      break;
    shift <<= 7;
    result += shift;
  }
  return result;
}

uint32_t ReadLE32(const uint8_t* data) {
  return static_cast<uint32_t>(data[0]) |
         (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) |
         (static_cast<uint32_t>(data[3]) << 24);
}

void WriteLE32(std::vector<uint8_t>& output, uint32_t value) {
  output.push_back(value & 0xFF);
  output.push_back((value >> 8) & 0xFF);
  output.push_back((value >> 16) & 0xFF);
  output.push_back((value >> 24) & 0xFF);
}

}  // namespace

absl::Status ApplyBpsPatch(const std::vector<uint8_t>& source,
                           const std::vector<uint8_t>& patch,
                           std::vector<uint8_t>& output) {
  if (source.empty()) {
    return absl::InvalidArgumentError("Source data is empty");
  }
  if (patch.size() < 16) {
    return absl::InvalidArgumentError("Patch data is too small to be valid");
  }

  // Verify BPS1 header
  if (patch[0] != 'B' || patch[1] != 'P' || patch[2] != 'S' ||
      patch[3] != '1') {
    return absl::InvalidArgumentError(
        "Invalid BPS patch header (expected BPS1)");
  }

  // Verify patch CRC32 (last 4 bytes are the patch checksum, computed over
  // everything before them)
  size_t patch_data_size = patch.size() - 4;
  uint32_t stored_patch_crc = ReadLE32(&patch[patch_data_size]);
  uint32_t computed_patch_crc = CalculateCrc32(patch.data(), patch_data_size);
  if (stored_patch_crc != computed_patch_crc) {
    return absl::DataLossError("Patch CRC32 mismatch (corrupt patch file)");
  }

  // Read header fields
  size_t offset = 4;  // skip "BPS1"
  uint64_t source_size = ReadVariableLength(patch.data(), patch.size(), offset);
  uint64_t target_size = ReadVariableLength(patch.data(), patch.size(), offset);
  uint64_t metadata_size =
      ReadVariableLength(patch.data(), patch.size(), offset);

  // Skip metadata
  offset += metadata_size;

  // Validate source size
  if (source.size() != source_size) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Source size mismatch: expected %llu, got %zu",
                        source_size, source.size()));
  }

  // Verify source CRC32
  uint32_t stored_source_crc = ReadLE32(&patch[patch_data_size - 8]);
  uint32_t computed_source_crc = CalculateCrc32(source);
  if (stored_source_crc != computed_source_crc) {
    return absl::DataLossError("Source ROM CRC32 mismatch");
  }

  // Prepare output
  output.resize(target_size);
  size_t output_offset = 0;
  int64_t source_relative_offset = 0;
  int64_t target_relative_offset = 0;

  // The last 12 bytes are: source CRC32 (4) + target CRC32 (4) + patch CRC32
  // (4)
  size_t actions_end = patch.size() - 12;

  while (offset < actions_end) {
    uint64_t data = ReadVariableLength(patch.data(), patch.size(), offset);
    uint64_t command = data & 3;
    uint64_t length = (data >> 2) + 1;

    switch (command) {
      case 0: {
        // SourceRead: copy length bytes from source at outputOffset
        for (uint64_t i = 0; i < length; ++i) {
          if (output_offset >= target_size) {
            return absl::InternalError("SourceRead: output overflow");
          }
          if (output_offset < source.size()) {
            output[output_offset] = source[output_offset];
          } else {
            output[output_offset] = 0;
          }
          output_offset++;
        }
        break;
      }
      case 1: {
        // TargetRead: copy length literal bytes from patch data
        for (uint64_t i = 0; i < length; ++i) {
          if (output_offset >= target_size || offset >= patch.size()) {
            return absl::InternalError("TargetRead: overflow");
          }
          output[output_offset++] = patch[offset++];
        }
        break;
      }
      case 2: {
        // SourceCopy: copy length bytes from source at sourceRelativeOffset
        uint64_t offset_data =
            ReadVariableLength(patch.data(), patch.size(), offset);
        int64_t relative = (offset_data & 1)
                               ? -(static_cast<int64_t>(offset_data >> 1) + 1)
                               : static_cast<int64_t>(offset_data >> 1);
        source_relative_offset += relative;
        for (uint64_t i = 0; i < length; ++i) {
          if (output_offset >= target_size) {
            return absl::InternalError("SourceCopy: output overflow");
          }
          if (source_relative_offset >= 0 &&
              static_cast<size_t>(source_relative_offset) < source.size()) {
            output[output_offset] = source[source_relative_offset];
          } else {
            output[output_offset] = 0;
          }
          output_offset++;
          source_relative_offset++;
        }
        break;
      }
      case 3: {
        // TargetCopy: copy length bytes from output at targetRelativeOffset
        uint64_t offset_data =
            ReadVariableLength(patch.data(), patch.size(), offset);
        int64_t relative = (offset_data & 1)
                               ? -(static_cast<int64_t>(offset_data >> 1) + 1)
                               : static_cast<int64_t>(offset_data >> 1);
        target_relative_offset += relative;
        for (uint64_t i = 0; i < length; ++i) {
          if (output_offset >= target_size) {
            return absl::InternalError("TargetCopy: output overflow");
          }
          if (target_relative_offset >= 0 &&
              static_cast<size_t>(target_relative_offset) < output_offset) {
            output[output_offset] = output[target_relative_offset];
          } else {
            output[output_offset] = 0;
          }
          output_offset++;
          target_relative_offset++;
        }
        break;
      }
      default:
        return absl::InternalError(
            absl::StrFormat("Unknown BPS command: %llu", command));
    }
  }

  // Verify target CRC32
  uint32_t stored_target_crc = ReadLE32(&patch[patch_data_size - 4]);
  uint32_t computed_target_crc = CalculateCrc32(output);
  if (stored_target_crc != computed_target_crc) {
    return absl::DataLossError("Target CRC32 mismatch after patch application");
  }

  return absl::OkStatus();
}

absl::Status CreateBpsPatch(const std::vector<uint8_t>& source,
                            const std::vector<uint8_t>& target,
                            std::vector<uint8_t>& patch) {
  if (source.empty()) {
    return absl::InvalidArgumentError("Source data is empty");
  }
  if (target.empty()) {
    return absl::InvalidArgumentError("Target data is empty");
  }

  patch.clear();

  // BPS header "BPS1"
  patch.push_back('B');
  patch.push_back('P');
  patch.push_back('S');
  patch.push_back('1');

  // Source size
  WriteVariableLength(patch, source.size());
  // Target size
  WriteVariableLength(patch, target.size());
  // Metadata size (0 = no metadata)
  WriteVariableLength(patch, 0);

  // Generate patch actions using SourceRead and TargetRead
  //
  // BPS action types:
  //   0 = SourceRead: copy n bytes from source at current outputOffset
  //   1 = TargetRead: copy n literal bytes from the patch stream
  //   2 = SourceCopy: copy n bytes from source at sourceRelativeOffset
  //   3 = TargetCopy: copy n bytes from target at targetRelativeOffset
  //
  // We use a straightforward algorithm:
  //   - Where source and target match at the same offset, use SourceRead
  //   - Where they differ, use TargetRead with literal bytes

  size_t output_offset = 0;

  while (output_offset < target.size()) {
    // Check how many bytes match at the current position (SourceRead)
    size_t source_read_len = 0;
    if (output_offset < source.size()) {
      while (output_offset + source_read_len < target.size() &&
             output_offset + source_read_len < source.size() &&
             source[output_offset + source_read_len] ==
                 target[output_offset + source_read_len]) {
        ++source_read_len;
      }
    }

    if (source_read_len >= 4 ||
        (source_read_len > 0 &&
         output_offset + source_read_len >= target.size())) {
      // Use SourceRead for a run of matching bytes
      uint64_t action = ((source_read_len - 1) << 2) | 0;
      WriteVariableLength(patch, action);
      output_offset += source_read_len;
    } else {
      // Use TargetRead for literal bytes until the next good SourceRead match
      size_t target_read_len = 1;
      while (output_offset + target_read_len < target.size()) {
        // Check if the next position has a good SourceRead match
        if (output_offset + target_read_len < source.size()) {
          size_t match_len = 0;
          while (output_offset + target_read_len + match_len < target.size() &&
                 output_offset + target_read_len + match_len < source.size() &&
                 source[output_offset + target_read_len + match_len] ==
                     target[output_offset + target_read_len + match_len]) {
            ++match_len;
          }
          if (match_len >= 4) {
            break;  // Found a good match ahead, stop the literal run
          }
        }
        ++target_read_len;
      }

      // Write TargetRead action
      uint64_t action = ((target_read_len - 1) << 2) | 1;
      WriteVariableLength(patch, action);

      // Write the literal bytes
      for (size_t i = 0; i < target_read_len; ++i) {
        patch.push_back(target[output_offset + i]);
      }
      output_offset += target_read_len;
    }
  }

  // Write checksums (CRC32, little-endian)
  WriteLE32(patch, CalculateCrc32(source));  // source CRC32
  WriteLE32(patch, CalculateCrc32(target));  // target CRC32
  WriteLE32(patch,
            CalculateCrc32(patch));  // patch CRC32 (over everything so far)

  return absl::OkStatus();
}

}  // namespace util
}  // namespace yaze
