// clang-format off
#ifdef __EMSCRIPTEN__

#include "app/platform/wasm/wasm_patch_export.h"

#include <algorithm>
#include <cstring>
#include <emscripten.h>
#include <emscripten/html5.h>

#include "absl/strings/str_format.h"

namespace yaze {
namespace platform {

// JavaScript interop for downloading patch files
EM_JS(void, downloadPatchFile_impl, (const char* filename, const uint8_t* data, size_t size, const char* mime_type), {
  var dataArray = HEAPU8.subarray(data, data + size);
  var blob = new Blob([dataArray], { type: UTF8ToString(mime_type) });
  var url = URL.createObjectURL(blob);
  var a = document.createElement('a');
  a.href = url;
  a.download = UTF8ToString(filename);
  a.style.display = 'none';
  document.body.appendChild(a);
  a.click();
  setTimeout(function() {
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  }, 100);
});
// clang-format on

// CRC32 implementation
static const uint32_t kCRC32Table[256] = {
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
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d};

uint32_t WasmPatchExport::CalculateCRC32(const std::vector<uint8_t>& data) {
  return CalculateCRC32(data.data(), data.size());
}

uint32_t WasmPatchExport::CalculateCRC32(const uint8_t* data, size_t size) {
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < size; ++i) {
    crc = kCRC32Table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
  }
  return ~crc;
}

void WasmPatchExport::WriteVariableLength(std::vector<uint8_t>& output,
                                          uint64_t value) {
  while (true) {
    uint8_t byte = value & 0x7F;
    value >>= 7;
    if (value == 0) {
      output.push_back(byte | 0x80);
      break;
    }
    output.push_back(byte);
  }
}

void WasmPatchExport::WriteIPS24BitOffset(std::vector<uint8_t>& output,
                                          uint32_t offset) {
  output.push_back((offset >> 16) & 0xFF);
  output.push_back((offset >> 8) & 0xFF);
  output.push_back(offset & 0xFF);
}

void WasmPatchExport::WriteIPS16BitSize(std::vector<uint8_t>& output,
                                        uint16_t size) {
  output.push_back((size >> 8) & 0xFF);
  output.push_back(size & 0xFF);
}

std::vector<std::pair<size_t, size_t>> WasmPatchExport::FindChangedRegions(
    const std::vector<uint8_t>& original,
    const std::vector<uint8_t>& modified) {
  std::vector<std::pair<size_t, size_t>> regions;

  size_t min_size = std::min(original.size(), modified.size());
  size_t i = 0;

  while (i < min_size) {
    // Skip unchanged bytes
    while (i < min_size && original[i] == modified[i]) {
      ++i;
    }

    if (i < min_size) {
      // Found a change, record the start
      size_t start = i;

      // Find the end of the changed region
      while (i < min_size && original[i] != modified[i]) {
        ++i;
      }

      regions.push_back({start, i - start});
    }
  }

  // Handle case where modified ROM is larger
  if (modified.size() > original.size()) {
    regions.push_back({original.size(), modified.size() - original.size()});
  }

  return regions;
}

std::vector<uint8_t> WasmPatchExport::GenerateBPSPatch(
    const std::vector<uint8_t>& source, const std::vector<uint8_t>& target) {
  std::vector<uint8_t> patch;

  // BPS header "BPS1"
  patch.push_back('B');
  patch.push_back('P');
  patch.push_back('S');
  patch.push_back('1');

  // Source size (variable-length encoding)
  WriteVariableLength(patch, source.size());

  // Target size (variable-length encoding)
  WriteVariableLength(patch, target.size());

  // Metadata size (0 for no metadata)
  WriteVariableLength(patch, 0);

  // BPS action types:
  // 0 = SourceRead: copy n bytes from source at outputOffset
  // 1 = TargetRead: copy n literal bytes from patch
  // 2 = SourceCopy: copy n bytes from source at sourceRelativeOffset
  // 3 = TargetCopy: copy n bytes from target at targetRelativeOffset

  size_t output_offset = 0;
  int64_t source_relative_offset = 0;
  int64_t target_relative_offset = 0;

  while (output_offset < target.size()) {
    // Check if we can use SourceRead (bytes match at current position)
    size_t source_read_len = 0;
    if (output_offset < source.size()) {
      while (output_offset + source_read_len < target.size() &&
             output_offset + source_read_len < source.size() &&
             source[output_offset + source_read_len] ==
                 target[output_offset + source_read_len]) {
        ++source_read_len;
      }
    }

    // Try to find a better match elsewhere in source (SourceCopy)
    size_t best_source_copy_offset = 0;
    size_t best_source_copy_len = 0;
    if (source_read_len < 4) {  // Only search if SourceRead isn't good enough
      for (size_t i = 0; i < source.size(); ++i) {
        size_t match_len = 0;
        while (i + match_len < source.size() &&
               output_offset + match_len < target.size() &&
               source[i + match_len] == target[output_offset + match_len]) {
          ++match_len;
        }
        if (match_len > best_source_copy_len && match_len >= 4) {
          best_source_copy_len = match_len;
          best_source_copy_offset = i;
        }
      }
    }

    // Decide which action to use
    if (source_read_len >= 4 ||
        (source_read_len > 0 && source_read_len >= best_source_copy_len)) {
      // Use SourceRead
      uint64_t action = ((source_read_len - 1) << 2) | 0;
      WriteVariableLength(patch, action);
      output_offset += source_read_len;
    } else if (best_source_copy_len >= 4) {
      // Use SourceCopy
      uint64_t action = ((best_source_copy_len - 1) << 2) | 2;
      WriteVariableLength(patch, action);

      // Write relative offset (signed, encoded as unsigned with sign bit)
      int64_t relative = static_cast<int64_t>(best_source_copy_offset) -
                         source_relative_offset;
      uint64_t encoded_offset =
          (relative < 0) ? ((static_cast<uint64_t>(-relative - 1) << 1) | 1)
                         : (static_cast<uint64_t>(relative) << 1);
      WriteVariableLength(patch, encoded_offset);

      source_relative_offset = best_source_copy_offset + best_source_copy_len;
      output_offset += best_source_copy_len;
    } else {
      // Use TargetRead - find how many bytes to write literally
      size_t target_read_len = 1;
      while (output_offset + target_read_len < target.size()) {
        // Check if next position has a good match
        bool has_good_match = false;

        // Check SourceRead at next position
        if (output_offset + target_read_len < source.size() &&
            source[output_offset + target_read_len] ==
                target[output_offset + target_read_len]) {
          size_t match_len = 0;
          while (output_offset + target_read_len + match_len < target.size() &&
                 output_offset + target_read_len + match_len < source.size() &&
                 source[output_offset + target_read_len + match_len] ==
                     target[output_offset + target_read_len + match_len]) {
            ++match_len;
          }
          if (match_len >= 4) {
            has_good_match = true;
          }
        }

        if (has_good_match) {
          break;
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

  // Write checksums (all CRC32, little-endian)
  uint32_t source_crc = CalculateCRC32(source);
  uint32_t target_crc = CalculateCRC32(target);

  // Source CRC32
  patch.push_back(source_crc & 0xFF);
  patch.push_back((source_crc >> 8) & 0xFF);
  patch.push_back((source_crc >> 16) & 0xFF);
  patch.push_back((source_crc >> 24) & 0xFF);

  // Target CRC32
  patch.push_back(target_crc & 0xFF);
  patch.push_back((target_crc >> 8) & 0xFF);
  patch.push_back((target_crc >> 16) & 0xFF);
  patch.push_back((target_crc >> 24) & 0xFF);

  // Patch CRC32 (includes everything before this point)
  uint32_t patch_crc = CalculateCRC32(patch.data(), patch.size());
  patch.push_back(patch_crc & 0xFF);
  patch.push_back((patch_crc >> 8) & 0xFF);
  patch.push_back((patch_crc >> 16) & 0xFF);
  patch.push_back((patch_crc >> 24) & 0xFF);

  return patch;
}

std::vector<uint8_t> WasmPatchExport::GenerateIPSPatch(
    const std::vector<uint8_t>& source, const std::vector<uint8_t>& target) {
  std::vector<uint8_t> patch;

  // IPS header
  patch.push_back('P');
  patch.push_back('A');
  patch.push_back('T');
  patch.push_back('C');
  patch.push_back('H');

  // Find all changed regions
  auto regions = FindChangedRegions(source, target);

  for (const auto& region : regions) {
    size_t offset = region.first;
    size_t length = region.second;

    // IPS has a maximum offset of 16MB (0xFFFFFF)
    if (offset > 0xFFFFFF) {
      break;  // Can't encode offsets beyond 16MB
    }

    // Process the region, splitting if necessary (max 65535 bytes per record)
    size_t remaining = length;
    size_t current_offset = offset;

    while (remaining > 0) {
      size_t chunk_size = std::min(remaining, static_cast<size_t>(0xFFFF));

      // Check for RLE opportunity (same byte repeated)
      bool use_rle = false;
      uint8_t rle_byte = 0;

      if (chunk_size >= 3 && current_offset < target.size()) {
        rle_byte = target[current_offset];
        use_rle = true;

        for (size_t i = 1; i < chunk_size; ++i) {
          if (current_offset + i >= target.size() ||
              target[current_offset + i] != rle_byte) {
            use_rle = false;
            break;
          }
        }
      }

      if (use_rle && chunk_size >= 3) {
        // RLE record
        WriteIPS24BitOffset(patch, current_offset);
        WriteIPS16BitSize(patch, 0);  // RLE marker
        WriteIPS16BitSize(patch, chunk_size);
        patch.push_back(rle_byte);
      } else {
        // Normal record
        WriteIPS24BitOffset(patch, current_offset);
        WriteIPS16BitSize(patch, chunk_size);

        for (size_t i = 0; i < chunk_size; ++i) {
          if (current_offset + i < target.size()) {
            patch.push_back(target[current_offset + i]);
          } else {
            patch.push_back(0);  // Pad with zeros if target is shorter
          }
        }
      }

      current_offset += chunk_size;
      remaining -= chunk_size;
    }
  }

  // IPS EOF marker
  patch.push_back('E');
  patch.push_back('O');
  patch.push_back('F');

  return patch;
}

absl::Status WasmPatchExport::DownloadPatchFile(
    const std::string& filename, const std::vector<uint8_t>& data,
    const std::string& mime_type) {
  if (data.empty()) {
    return absl::InvalidArgumentError("Cannot download empty patch file");
  }

  if (filename.empty()) {
    return absl::InvalidArgumentError("Filename cannot be empty");
  }

  // clang-format off
  downloadPatchFile_impl(filename.c_str(), data.data(), data.size(), mime_type.c_str());
  // clang-format on

  return absl::OkStatus();
}

absl::Status WasmPatchExport::ExportBPS(const std::vector<uint8_t>& original,
                                        const std::vector<uint8_t>& modified,
                                        const std::string& filename) {
  if (original.empty()) {
    return absl::InvalidArgumentError("Original ROM data is empty");
  }

  if (modified.empty()) {
    return absl::InvalidArgumentError("Modified ROM data is empty");
  }

  if (filename.empty()) {
    return absl::InvalidArgumentError("Filename cannot be empty");
  }

  // Generate the BPS patch
  std::vector<uint8_t> patch = GenerateBPSPatch(original, modified);

  if (patch.empty()) {
    return absl::InternalError("Failed to generate BPS patch");
  }

  // Download the patch file
  return DownloadPatchFile(filename, patch, "application/octet-stream");
}

absl::Status WasmPatchExport::ExportIPS(const std::vector<uint8_t>& original,
                                        const std::vector<uint8_t>& modified,
                                        const std::string& filename) {
  if (original.empty()) {
    return absl::InvalidArgumentError("Original ROM data is empty");
  }

  if (modified.empty()) {
    return absl::InvalidArgumentError("Modified ROM data is empty");
  }

  if (filename.empty()) {
    return absl::InvalidArgumentError("Filename cannot be empty");
  }

  // Check for IPS size limitation
  if (modified.size() > 0xFFFFFF) {
    return absl::InvalidArgumentError(
        absl::StrFormat("ROM size (%zu bytes) exceeds IPS format limit (16MB)",
                        modified.size()));
  }

  // Generate the IPS patch
  std::vector<uint8_t> patch = GenerateIPSPatch(original, modified);

  if (patch.empty()) {
    return absl::InternalError("Failed to generate IPS patch");
  }

  // Download the patch file
  return DownloadPatchFile(filename, patch, "application/octet-stream");
}

PatchInfo WasmPatchExport::GetPatchPreview(
    const std::vector<uint8_t>& original,
    const std::vector<uint8_t>& modified) {
  PatchInfo info;
  info.changed_bytes = 0;
  info.num_regions = 0;

  if (original.empty() || modified.empty()) {
    return info;
  }

  // Find all changed regions
  info.changed_regions = FindChangedRegions(original, modified);
  info.num_regions = info.changed_regions.size();

  // Calculate total changed bytes
  for (const auto& region : info.changed_regions) {
    info.changed_bytes += region.second;
  }

  return info;
}

}  // namespace platform
}  // namespace yaze

#endif  // __EMSCRIPTEN__