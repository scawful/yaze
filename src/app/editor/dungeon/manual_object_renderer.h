#ifndef YAZE_APP_EDITOR_DUNGEON_MANUAL_OBJECT_RENDERER_H
#define YAZE_APP_EDITOR_DUNGEON_MANUAL_OBJECT_RENDERER_H

#include <vector>
#include <memory>

#include "app/gfx/snes_palette.h"
#include "app/gui/canvas.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/room_object.h"

namespace yaze {
namespace editor {

/**
 * @brief Manual object renderer for debugging and testing basic object rendering
 * 
 * This class provides simple, manual rendering of basic dungeon objects
 * to help debug the graphics pipeline and understand object data structures.
 * 
 * Features:
 * - Manual tile creation for simple objects
 * - Direct graphics sheet access
 * - Palette debugging and testing
 * - Simple pattern rendering (solid blocks, lines, etc.)
 */
class ManualObjectRenderer {
 public:
  explicit ManualObjectRenderer(gui::Canvas* canvas, Rom* rom);
  
  /**
   * @brief Render a simple solid block object manually
   * @param object_id Object ID to render
   * @param x X position in pixels
   * @param y Y position in pixels
   * @param palette Current palette to use
   * @return Status of the rendering operation
   */
  absl::Status RenderSimpleBlock(uint16_t object_id, int x, int y, 
                                const gfx::SnesPalette& palette);
  
  /**
   * @brief Render a test pattern to verify graphics pipeline
   * @param x X position in pixels
   * @param y Y position in pixels
   * @param width Width in pixels
   * @param height Height in pixels
   * @param color_index Color index to use
   */
  void RenderTestPattern(int x, int y, int width, int height, uint8_t color_index);
  
  /**
   * @brief Debug graphics sheet loading and display info
   * @param sheet_index Graphics sheet index to examine
   */
  void DebugGraphicsSheet(int sheet_index);
  
  /**
   * @brief Test palette rendering with different colors
   * @param x X position in pixels
   * @param y Y position in pixels
   */
  void TestPaletteRendering(int x, int y);
  
  /**
   * @brief Create a simple 16x16 tile manually
   * @param tile_id Tile ID to create
   * @param palette Palette to apply
   * @return Created bitmap
   */
  std::unique_ptr<gfx::Bitmap> CreateSimpleTile(uint16_t tile_id, 
                                               const gfx::SnesPalette& palette);

 private:
  gui::Canvas* canvas_;
  Rom* rom_;
  
  // Simple tile creation helpers
  std::vector<uint8_t> CreateSolidTile(uint8_t color_index);
  std::vector<uint8_t> CreatePatternTile(uint8_t pattern_type, uint8_t color_index);
  
  // Graphics debugging
  void LogGraphicsInfo(int sheet_index);
  void LogPaletteInfo(const gfx::SnesPalette& palette);
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_MANUAL_OBJECT_RENDERER_H
