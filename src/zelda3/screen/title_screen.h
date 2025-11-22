#ifndef YAZE_APP_ZELDA3_SCREEN_H
#define YAZE_APP_ZELDA3_SCREEN_H

#include "absl/status/status.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/render/tilemap.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/types/snes_tile.h"
#include "app/rom.h"

namespace yaze {
namespace zelda3 {

/**
 * @brief TitleScreen manages the title screen graphics and tilemap data.
 *
 * The title screen consists of three layers:
 * - BG1: Main logo and graphics
 * - BG2: Background elements
 * - OAM: Sprite layer (sword, etc.)
 *
 * Each layer is stored as a 32x32 tilemap (0x400 tiles = 0x1000 bytes as words)
 */
class TitleScreen {
 public:
  /**
   * @brief Initialize and load title screen data from ROM
   * @param rom ROM instance to read data from
   */
  absl::Status Create(Rom* rom);

  // Accessors for layer data
  auto& bg1_buffer() { return tiles_bg1_buffer_; }
  auto& bg2_buffer() { return tiles_bg2_buffer_; }
  auto& oam_buffer() { return oam_data_; }

  // Mutable accessors for editing
  auto& mutable_bg1_buffer() { return tiles_bg1_buffer_; }
  auto& mutable_bg2_buffer() { return tiles_bg2_buffer_; }

  // Accessors for bitmaps
  auto& bg1_bitmap() { return tiles_bg1_bitmap_; }
  auto& bg2_bitmap() { return tiles_bg2_bitmap_; }
  auto& oam_bitmap() { return oam_bg_bitmap_; }
  auto& tiles8_bitmap() { return tiles8_bitmap_; }
  auto& composite_bitmap() { return title_composite_bitmap_; }
  auto& blockset() { return tile16_blockset_; }

  // Palette access
  auto& palette() { return palette_; }

  // Save changes back to ROM
  absl::Status Save(Rom* rom);

  /**
   * @brief Render BG1 tilemap into bitmap pixels
   * Converts tile IDs from tiles_bg1_buffer_ into pixel data
   */
  absl::Status RenderBG1Layer();

  /**
   * @brief Render BG2 tilemap into bitmap pixels
   * Converts tile IDs from tiles_bg2_buffer_ into pixel data
   */
  absl::Status RenderBG2Layer();

  /**
   * @brief Render composite layer with BG1 on top of BG2 with transparency
   * @param show_bg1 Whether to include BG1 layer
   * @param show_bg2 Whether to include BG2 layer
   */
  absl::Status RenderCompositeLayer(bool show_bg1, bool show_bg2);

 private:
  /**
   * @brief Build the tile16 blockset from ROM graphics
   * @param rom ROM instance to read graphics from
   */
  absl::Status BuildTileset(Rom* rom);

  /**
   * @brief Load title screen tilemap data from ROM
   * @param rom ROM instance to read tilemap from
   */
  absl::Status LoadTitleScreen(Rom* rom);

  int pal_selected_ = 2;

  std::array<uint16_t, 0x1000> tiles_bg1_buffer_;  // BG1 tilemap (32x32 tiles)
  std::array<uint16_t, 0x1000> tiles_bg2_buffer_;  // BG2 tilemap (32x32 tiles)

  gfx::OamTile oam_data_[10];

  gfx::Bitmap tiles_bg1_bitmap_;        // Rendered BG1 layer
  gfx::Bitmap tiles_bg2_bitmap_;        // Rendered BG2 layer
  gfx::Bitmap oam_bg_bitmap_;           // Rendered OAM layer
  gfx::Bitmap tiles8_bitmap_;           // 8x8 tile graphics
  gfx::Bitmap title_composite_bitmap_;  // Composite BG1+BG2 with transparency

  gfx::Tilemap tile16_blockset_;  // 16x16 tile blockset
  gfx::SnesPalette palette_;      // Title screen palette
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_SCREEN_H
