#ifndef YAZE_APP_GFX_ZSPR_LOADER_H
#define YAZE_APP_GFX_ZSPR_LOADER_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze {

class Rom;

namespace gfx {

/**
 * @brief Metadata from a ZSPR file header
 */
struct ZsprMetadata {
  std::string display_name;
  std::string author;
  std::string author_rom_name;
  uint8_t version = 0;
  uint16_t sprite_type = 0;  // 0 = Link, 1 = Other
};

/**
 * @brief Complete data loaded from a ZSPR file
 *
 * ZSPR files contain Link sprite replacement data used by the ALttP Randomizer
 * community. The format includes:
 * - 896 tiles (28672 bytes) of 4BPP sprite graphics
 * - 120 bytes of palette data (4 palettes × 15 colors × 2 bytes)
 * - 2 glove color values
 */
struct ZsprData {
  ZsprMetadata metadata;
  std::vector<uint8_t> sprite_data;   // 28672 bytes (14 sheets × 2048 bytes)
  std::vector<uint8_t> palette_data;  // 120 bytes
  std::array<uint16_t, 2> glove_colors = {0, 0};

  // Convenience accessors
  bool is_link_sprite() const { return metadata.sprite_type == 0; }
  size_t tile_count() const { return sprite_data.size() / 32; }  // 32 bytes per 4BPP tile
};

/**
 * @brief Loader for ZSPR (ALttP Randomizer) sprite files
 *
 * ZSPR Format (v1):
 * ```
 * Offset  Size  Description
 * ------  ----  -----------
 * 0x00    4     Magic: "ZSPR"
 * 0x04    1     Version (currently 1)
 * 0x05    4     Checksum (Adler-32)
 * 0x09    2     Sprite data offset (little-endian)
 * 0x0B    2     Sprite data size (little-endian)
 * 0x0D    2     Palette data offset (little-endian)
 * 0x0F    2     Palette data size (little-endian)
 * 0x11    2     Sprite type (0 = Link, 1 = Other)
 * 0x13    var   Display name (null-terminated UTF-8)
 * ...     var   Author name (null-terminated UTF-8)
 * ...     var   Author ROM name (null-terminated)
 * ...     28672 Sprite data (896 tiles × 32 bytes/tile, 4BPP)
 * ...     120   Palette data (15 colors × 4 palettes × 2 bytes)
 * ...     4     Glove colors (2 colors × 2 bytes)
 * ```
 */
class ZsprLoader {
 public:
  static constexpr uint32_t kZsprMagic = 0x5250535A;  // "ZSPR" little-endian
  static constexpr size_t kExpectedSpriteDataSize = 28672;  // 896 tiles × 32 bytes
  static constexpr size_t kExpectedPaletteDataSize = 120;   // 15 × 4 × 2 bytes
  static constexpr size_t kTilesPerSheet = 64;              // 8×8 tiles per sheet
  static constexpr size_t kBytesPerTile = 32;               // 4BPP 8×8 tile
  static constexpr size_t kLinkSheetCount = 14;

  /**
   * @brief Load ZSPR data from a file path
   * @param path Path to the .zspr file
   * @return ZsprData on success, or error status
   */
  static absl::StatusOr<ZsprData> LoadFromFile(const std::string& path);

  /**
   * @brief Load ZSPR data from a byte buffer
   * @param data Raw file contents
   * @return ZsprData on success, or error status
   */
  static absl::StatusOr<ZsprData> LoadFromData(const std::vector<uint8_t>& data);

  /**
   * @brief Apply loaded ZSPR sprite data to ROM's Link graphics
   *
   * Writes the sprite data to the ROM at Link's graphics sheet locations.
   * The ZSPR 4BPP data is converted to the ROM's expected format.
   *
   * @param rom ROM to modify
   * @param zspr ZSPR data to apply
   * @return Status indicating success or failure
   */
  static absl::Status ApplyToRom(Rom& rom, const ZsprData& zspr);

  /**
   * @brief Apply ZSPR palette data to ROM
   *
   * Writes the sprite palette data to the appropriate ROM locations.
   *
   * @param rom ROM to modify
   * @param zspr ZSPR data containing palette
   * @return Status indicating success or failure
   */
  static absl::Status ApplyPaletteToRom(Rom& rom, const ZsprData& zspr);

 private:
  /**
   * @brief Validate ZSPR checksum (Adler-32)
   */
  static bool ValidateChecksum(const std::vector<uint8_t>& data,
                               uint32_t expected_checksum);

  /**
   * @brief Calculate Adler-32 checksum
   */
  static uint32_t CalculateAdler32(const uint8_t* data, size_t length);

  /**
   * @brief Read null-terminated string from buffer
   */
  static std::string ReadNullTerminatedString(const uint8_t* data,
                                              size_t max_length,
                                              size_t& bytes_read);

  /**
   * @brief Read 16-bit little-endian value
   */
  static uint16_t ReadU16LE(const uint8_t* data);

  /**
   * @brief Read 32-bit little-endian value
   */
  static uint32_t ReadU32LE(const uint8_t* data);
};

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_APP_GFX_ZSPR_LOADER_H
