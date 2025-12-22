#ifndef YAZE_APP_EDITOR_SPRITE_SPRITE_DRAWER_H
#define YAZE_APP_EDITOR_SPRITE_SPRITE_DRAWER_H

#include <cstdint>
#include <vector>

#include "app/editor/sprite/zsprite.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_palette.h"

namespace yaze {
namespace editor {

/**
 * @brief Draws sprite OAM tiles to bitmaps for preview rendering
 *
 * This class handles static rendering of sprite graphics for both:
 * - ZSM custom sprites (using OamTile definitions from .zsm files)
 * - Vanilla ROM sprites (using hardcoded OAM layouts)
 *
 * Architecture:
 * - Graphics buffer is 8BPP linear format (same as room graphics)
 * - 16 tiles per row (128 bytes per scanline)
 * - Each tile is 8x8 pixels
 * - OamTile.size determines 8x8 or 16x16 mode
 */
class SpriteDrawer {
 public:
  /**
   * @brief Construct a SpriteDrawer with graphics buffer
   * @param sprite_gfx_buffer Pointer to 8BPP graphics data (0x10000 bytes)
   */
  explicit SpriteDrawer(const uint8_t* sprite_gfx_buffer = nullptr);

  /**
   * @brief Set the graphics buffer for tile lookup
   * @param buffer Pointer to 8BPP graphics data
   */
  void SetGraphicsBuffer(const uint8_t* buffer) { sprite_gfx_ = buffer; }

  /**
   * @brief Set the palette group for color mapping
   * @param palettes Palette group containing sprite palettes
   */
  void SetPalettes(const gfx::PaletteGroup* palettes) {
    sprite_palettes_ = palettes;
  }

  /**
   * @brief Draw a single ZSM OAM tile to bitmap
   * @param bitmap Target bitmap to draw to
   * @param tile OAM tile definition from ZSM file
   * @param origin_x X origin offset (center of sprite canvas)
   * @param origin_y Y origin offset (center of sprite canvas)
   */
  void DrawOamTile(gfx::Bitmap& bitmap, const zsprite::OamTile& tile,
                   int origin_x, int origin_y);

  /**
   * @brief Draw all tiles in a ZSM frame
   * @param bitmap Target bitmap to draw to
   * @param frame Frame containing OAM tile list
   * @param origin_x X origin offset
   * @param origin_y Y origin offset
   */
  void DrawFrame(gfx::Bitmap& bitmap, const zsprite::Frame& frame,
                 int origin_x, int origin_y);

  /**
   * @brief Clear the bitmap with transparent color
   * @param bitmap Bitmap to clear
   */
  void ClearBitmap(gfx::Bitmap& bitmap);

  /**
   * @brief Check if drawer is ready to render
   * @return true if graphics buffer and palettes are set
   */
  bool IsReady() const { return sprite_gfx_ != nullptr; }

 private:
  /**
   * @brief Draw an 8x8 tile to bitmap
   * @param bitmap Target bitmap
   * @param tile_id Tile ID in graphics buffer (0-1023)
   * @param x Destination X coordinate in bitmap
   * @param y Destination Y coordinate in bitmap
   * @param flip_x Horizontal mirror flag
   * @param flip_y Vertical mirror flag
   * @param palette Palette index (0-7)
   */
  void DrawTile8x8(gfx::Bitmap& bitmap, uint16_t tile_id, int x, int y,
                   bool flip_x, bool flip_y, uint8_t palette);

  /**
   * @brief Draw a 16x16 tile (4 8x8 tiles) to bitmap
   * @param bitmap Target bitmap
   * @param tile_id Base tile ID (top-left of 2x2 grid)
   * @param x Destination X coordinate in bitmap
   * @param y Destination Y coordinate in bitmap
   * @param flip_x Horizontal mirror flag
   * @param flip_y Vertical mirror flag
   * @param palette Palette index (0-7)
   *
   * 16x16 tile layout (unmirrored):
   *   [tile_id + 0]  [tile_id + 1]
   *   [tile_id + 16] [tile_id + 17]
   */
  void DrawTile16x16(gfx::Bitmap& bitmap, uint16_t tile_id, int x, int y,
                     bool flip_x, bool flip_y, uint8_t palette);

  /**
   * @brief Get pixel value from graphics buffer
   * @param tile_id Tile ID
   * @param px Pixel X within tile (0-7)
   * @param py Pixel Y within tile (0-7)
   * @return Pixel value (0 = transparent, 1-15 = color index)
   */
  uint8_t GetTilePixel(uint16_t tile_id, int px, int py) const;

  const uint8_t* sprite_gfx_ = nullptr;
  const gfx::PaletteGroup* sprite_palettes_ = nullptr;

  // Graphics buffer layout constants
  static constexpr int kTilesPerRow = 16;
  static constexpr int kTileSize = 8;
  static constexpr int kRowStride = 128;   // 16 tiles * 8 pixels
  static constexpr int kTileRowSize = 1024; // 8 scanlines * 128 bytes
  static constexpr int kMaxTileId = 1023;  // 64 rows * 16 columns - 1
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SPRITE_SPRITE_DRAWER_H
