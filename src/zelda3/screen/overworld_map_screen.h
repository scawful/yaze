#ifndef YAZE_APP_ZELDA3_OVERWORLD_MAP_SCREEN_H
#define YAZE_APP_ZELDA3_OVERWORLD_MAP_SCREEN_H

#include <array>

#include "absl/status/status.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_palette.h"
#include "rom/rom.h"

namespace yaze {
namespace zelda3 {

/**
 * @brief OverworldMapScreen manages the overworld map (pause menu) graphics.
 *
 * The overworld map screen shows the mini-map when the player pauses.
 * It consists of:
 * - 64x64 tiles (8x8 pixels each) for Light World map
 * - 64x64 tiles (8x8 pixels each) for Dark World map
 * - Mode 7 graphics stored at 0x0C4000
 * - Tile data in interleaved format across 4 sections
 */
class OverworldMapScreen {
 public:
  /**
   * @brief Initialize and load overworld map data from ROM
   * @param rom ROM instance to read data from
   */
  absl::Status Create(Rom* rom);

  /**
   * @brief Save changes back to ROM
   * @param rom ROM instance to write data to
   */
  absl::Status Save(Rom* rom);

  // Accessors for tile data
  auto& lw_tiles() { return lw_map_tiles_; }
  auto& dw_tiles() { return dw_map_tiles_; }

  // Mutable accessors for editing
  auto& mutable_lw_tiles() { return lw_map_tiles_; }
  auto& mutable_dw_tiles() { return dw_map_tiles_; }

  // Bitmap accessors
  auto& tiles8_bitmap() { return tiles8_bitmap_; }
  auto& map_bitmap() { return map_bitmap_; }

  // Palette accessors
  auto& lw_palette() { return lw_palette_; }
  auto& dw_palette() { return dw_palette_; }

  /**
   * @brief Render map tiles into bitmap
   * @param use_dark_world If true, render DW tiles, otherwise LW tiles
   */
  absl::Status RenderMapLayer(bool use_dark_world);

  /**
   * @brief Load custom map from external binary file
   * @param file_path Path to .bin file containing 64×64 tile indices
   */
  absl::Status LoadCustomMap(const std::string& file_path);

  /**
   * @brief Save map data to external binary file
   * @param file_path Path to output .bin file
   * @param use_dark_world If true, save DW tiles, otherwise LW tiles
   */
  absl::Status SaveCustomMap(const std::string& file_path, bool use_dark_world);

 private:
  /**
   * @brief Load map tile data from ROM
   * Reads the interleaved tile format from 4 ROM sections
   */
  absl::Status LoadMapData(Rom* rom);

  std::array<uint8_t, 64 * 64> lw_map_tiles_;  // Light World tile indices
  std::array<uint8_t, 64 * 64> dw_map_tiles_;  // Dark World tile indices

  gfx::Bitmap tiles8_bitmap_;  // 128x128 tileset (mode 7 graphics)
  gfx::Bitmap map_bitmap_;     // 512x512 rendered map (64 tiles × 8 pixels)

  gfx::SnesPalette lw_palette_;  // Light World palette
  gfx::SnesPalette dw_palette_;  // Dark World palette
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_OVERWORLD_MAP_SCREEN_H
