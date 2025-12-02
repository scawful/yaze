#include "app/gfx/util/zspr_loader.h"

#include <cstring>
#include <fstream>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "rom/rom.h"
#include "util/log.h"

namespace yaze {
namespace gfx {

absl::StatusOr<ZsprData> ZsprLoader::LoadFromFile(const std::string& path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    return absl::NotFoundError(
        absl::StrFormat("Could not open ZSPR file: %s", path));
  }

  // Get file size
  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  // Read entire file
  std::vector<uint8_t> data(size);
  if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
    return absl::InternalError(
        absl::StrFormat("Failed to read ZSPR file: %s", path));
  }

  return LoadFromData(data);
}

absl::StatusOr<ZsprData> ZsprLoader::LoadFromData(
    const std::vector<uint8_t>& data) {
  // Minimum header size check (magic + version + checksum + offsets + type)
  constexpr size_t kMinHeaderSize = 0x13;
  if (data.size() < kMinHeaderSize) {
    return absl::InvalidArgumentError(
        absl::StrFormat("ZSPR file too small: %zu bytes (minimum %zu)",
                        data.size(), kMinHeaderSize));
  }

  // Check magic bytes "ZSPR"
  if (data[0] != 'Z' || data[1] != 'S' || data[2] != 'P' || data[3] != 'R') {
    return absl::InvalidArgumentError(
        "Invalid ZSPR file: missing magic bytes 'ZSPR'");
  }

  ZsprData zspr;

  // Parse header
  zspr.metadata.version = data[0x04];
  uint32_t checksum = ReadU32LE(&data[0x05]);
  uint16_t sprite_offset = ReadU16LE(&data[0x09]);
  uint16_t sprite_size = ReadU16LE(&data[0x0B]);
  uint16_t palette_offset = ReadU16LE(&data[0x0D]);
  uint16_t palette_size = ReadU16LE(&data[0x0F]);
  zspr.metadata.sprite_type = ReadU16LE(&data[0x11]);

  LOG_INFO("ZsprLoader", "ZSPR v%d: sprite@0x%04X (%d bytes), palette@0x%04X (%d bytes), type=%d",
           zspr.metadata.version, sprite_offset, sprite_size,
           palette_offset, palette_size, zspr.metadata.sprite_type);

  // Validate checksum (covers sprite and palette data)
  // Note: Some ZSPR files may have checksum=0 if not computed
  if (checksum != 0) {
    size_t checksum_start = sprite_offset;
    size_t checksum_length = sprite_size + palette_size + 4;  // +4 for glove colors
    if (checksum_start + checksum_length <= data.size()) {
      if (!ValidateChecksum(
              std::vector<uint8_t>(data.begin() + checksum_start,
                                   data.begin() + checksum_start + checksum_length),
              checksum)) {
        LOG_WARN("ZsprLoader", "ZSPR checksum mismatch (expected 0x%08X)", checksum);
        // Continue anyway - some files have incorrect checksums
      }
    }
  }

  // Parse null-terminated strings starting at offset 0x13
  size_t string_offset = 0x13;
  size_t bytes_read = 0;

  // Display name
  if (string_offset < data.size()) {
    zspr.metadata.display_name = ReadNullTerminatedString(
        &data[string_offset], data.size() - string_offset, bytes_read);
    string_offset += bytes_read;
  }

  // Author name
  if (string_offset < data.size()) {
    zspr.metadata.author = ReadNullTerminatedString(
        &data[string_offset], data.size() - string_offset, bytes_read);
    string_offset += bytes_read;
  }

  // Author ROM name (optional in some files)
  if (string_offset < data.size() && string_offset < sprite_offset) {
    zspr.metadata.author_rom_name = ReadNullTerminatedString(
        &data[string_offset], data.size() - string_offset, bytes_read);
    string_offset += bytes_read;
  }

  LOG_INFO("ZsprLoader", "ZSPR: '%s' by %s",
           zspr.metadata.display_name.c_str(),
           zspr.metadata.author.c_str());

  // Extract sprite data
  if (sprite_offset + sprite_size > data.size()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("ZSPR sprite data extends beyond file: offset=%d, size=%d, file_size=%zu",
                        sprite_offset, sprite_size, data.size()));
  }
  zspr.sprite_data.assign(data.begin() + sprite_offset,
                          data.begin() + sprite_offset + sprite_size);

  // Validate sprite data size for Link sprites
  if (zspr.is_link_sprite() && sprite_size != kExpectedSpriteDataSize) {
    LOG_WARN("ZsprLoader", "Unexpected sprite data size for Link sprite: %d (expected %zu)",
             sprite_size, kExpectedSpriteDataSize);
  }

  // Extract palette data
  if (palette_offset + palette_size > data.size()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("ZSPR palette data extends beyond file: offset=%d, size=%d, file_size=%zu",
                        palette_offset, palette_size, data.size()));
  }
  zspr.palette_data.assign(data.begin() + palette_offset,
                           data.begin() + palette_offset + palette_size);

  // Extract glove colors (4 bytes after palette data)
  size_t glove_offset = palette_offset + palette_size;
  if (glove_offset + 4 <= data.size()) {
    zspr.glove_colors[0] = ReadU16LE(&data[glove_offset]);
    zspr.glove_colors[1] = ReadU16LE(&data[glove_offset + 2]);
    LOG_INFO("ZsprLoader", "Glove colors: 0x%04X, 0x%04X",
             zspr.glove_colors[0], zspr.glove_colors[1]);
  }

  return zspr;
}

absl::Status ZsprLoader::ApplyToRom(Rom& rom, const ZsprData& zspr) {
  if (!rom.is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  if (!zspr.is_link_sprite()) {
    return absl::InvalidArgumentError("ZSPR is not a Link sprite (type != 0)");
  }

  if (zspr.sprite_data.size() != kExpectedSpriteDataSize) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid sprite data size: %zu (expected %zu)",
                        zspr.sprite_data.size(), kExpectedSpriteDataSize));
  }

  // Link's graphics are stored at specific ROM offsets
  // The ZSPR data is already in 4BPP SNES format, so we write directly
  //
  // Link graphics locations (US ROM):
  // Sheet 0: 0x80000 - 0x807FF (2048 bytes)
  // Sheet 1: 0x80800 - 0x80FFF
  // ... (14 sheets total)
  //
  // These addresses may vary by ROM version, so we use the version constants

  constexpr uint32_t kLinkGfxBaseUS = 0x80000;
  constexpr size_t kBytesPerSheet = 2048;  // 64 tiles × 32 bytes

  LOG_INFO("ZsprLoader", "Applying ZSPR '%s' to ROM (%zu bytes of sprite data)",
           zspr.metadata.display_name.c_str(), zspr.sprite_data.size());

  // Write each sheet to ROM
  for (size_t sheet = 0; sheet < kLinkSheetCount; sheet++) {
    uint32_t rom_offset = kLinkGfxBaseUS + (sheet * kBytesPerSheet);
    size_t data_offset = sheet * kBytesPerSheet;

    // Bounds check
    if (data_offset + kBytesPerSheet > zspr.sprite_data.size()) {
      LOG_WARN("ZsprLoader", "Sheet %zu data incomplete, stopping", sheet);
      break;
    }

    // Write sheet data to ROM
    for (size_t i = 0; i < kBytesPerSheet; i++) {
      auto status = rom.WriteByte(rom_offset + i, zspr.sprite_data[data_offset + i]);
      if (!status.ok()) {
        return absl::InternalError(
            absl::StrFormat("Failed to write byte at 0x%06X: %s",
                            rom_offset + i, status.message()));
      }
    }

    LOG_INFO("ZsprLoader", "Wrote Link sheet %zu at ROM offset 0x%06X",
             sheet, rom_offset);
  }

  return absl::OkStatus();
}

absl::Status ZsprLoader::ApplyPaletteToRom(Rom& rom, const ZsprData& zspr) {
  if (!rom.is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  if (zspr.palette_data.size() < kExpectedPaletteDataSize) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid palette data size: %zu (expected %zu)",
                        zspr.palette_data.size(), kExpectedPaletteDataSize));
  }

  // Link palette locations in US ROM:
  // Green Mail: 0x0DD308 (30 bytes = 15 colors × 2)
  // Blue Mail:  0x0DD318
  // Red Mail:   0x0DD328
  // Bunny:      0x0DD338
  //
  // Glove colors are at separate locations

  constexpr uint32_t kLinkPaletteBase = 0x0DD308;
  constexpr size_t kPaletteSize = 30;  // 15 colors × 2 bytes
  constexpr size_t kNumPalettes = 4;

  LOG_INFO("ZsprLoader", "Applying ZSPR palette data (%zu bytes)",
           zspr.palette_data.size());

  // Write each palette
  for (size_t pal = 0; pal < kNumPalettes; pal++) {
    uint32_t rom_offset = kLinkPaletteBase + (pal * kPaletteSize);
    size_t data_offset = pal * kPaletteSize;

    for (size_t i = 0; i < kPaletteSize; i++) {
      auto status = rom.WriteByte(rom_offset + i, zspr.palette_data[data_offset + i]);
      if (!status.ok()) {
        return absl::InternalError(
            absl::StrFormat("Failed to write palette byte at 0x%06X", rom_offset + i));
      }
    }

    LOG_INFO("ZsprLoader", "Wrote palette %zu at ROM offset 0x%06X", pal, rom_offset);
  }

  // Write glove colors
  // Glove 1: 0x0DExx (power glove)
  // Glove 2: 0x0DExx (titan's mitt)
  // TODO: Find exact glove color offsets for US ROM
  LOG_INFO("ZsprLoader", "Glove colors loaded but not yet written (TODO: find offsets)");

  return absl::OkStatus();
}

bool ZsprLoader::ValidateChecksum(const std::vector<uint8_t>& data,
                                  uint32_t expected_checksum) {
  uint32_t computed = CalculateAdler32(data.data(), data.size());
  return computed == expected_checksum;
}

uint32_t ZsprLoader::CalculateAdler32(const uint8_t* data, size_t length) {
  constexpr uint32_t MOD_ADLER = 65521;
  uint32_t a = 1, b = 0;

  for (size_t i = 0; i < length; i++) {
    a = (a + data[i]) % MOD_ADLER;
    b = (b + a) % MOD_ADLER;
  }

  return (b << 16) | a;
}

std::string ZsprLoader::ReadNullTerminatedString(const uint8_t* data,
                                                  size_t max_length,
                                                  size_t& bytes_read) {
  std::string result;
  bytes_read = 0;

  for (size_t i = 0; i < max_length; i++) {
    bytes_read++;
    if (data[i] == 0) {
      break;
    }
    result += static_cast<char>(data[i]);
  }

  return result;
}

uint16_t ZsprLoader::ReadU16LE(const uint8_t* data) {
  return static_cast<uint16_t>(data[0]) |
         (static_cast<uint16_t>(data[1]) << 8);
}

uint32_t ZsprLoader::ReadU32LE(const uint8_t* data) {
  return static_cast<uint32_t>(data[0]) |
         (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) |
         (static_cast<uint32_t>(data[3]) << 24);
}

}  // namespace gfx
}  // namespace yaze
