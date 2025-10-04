#ifndef YAZE_APP_ZELDA3_DUNGEON_ROOM_VISUAL_DIAGNOSTIC_H
#define YAZE_APP_ZELDA3_DUNGEON_ROOM_VISUAL_DIAGNOSTIC_H

#include "imgui/imgui.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/background_buffer.h"

namespace yaze {
namespace zelda3 {
namespace dungeon {

/**
 * @brief Visual diagnostic tool for dungeon rendering using ImGui
 * 
 * Provides interactive visualization of:
 * - Texture previews (BG1, BG2)
 * - Bitmap data with zoom
 * - Tile buffer contents
 * - Palette colors
 * - Pixel value inspection
 */
class RoomVisualDiagnostic {
 public:
  /**
   * @brief Draw the diagnostic window
   * @param bg1_buffer Background 1 buffer reference
   * @param bg2_buffer Background 2 buffer reference  
   * @param palette Current palette being used
   * @param gfx16_data Graphics data buffer
   */
  static void DrawDiagnosticWindow(
      bool* p_open,
      gfx::BackgroundBuffer& bg1_buffer,
      gfx::BackgroundBuffer& bg2_buffer,
      const gfx::SnesPalette& palette,
      const std::vector<uint8_t>& gfx16_data);

 private:
  static void DrawTexturePreview(const gfx::Bitmap& bitmap, const char* label);
  static void DrawPaletteInspector(const gfx::SnesPalette& palette);
  static void DrawTileBufferInspector(gfx::BackgroundBuffer& buffer, const gfx::SnesPalette& palette);
  static void DrawPixelInspector(const gfx::Bitmap& bitmap, const gfx::SnesPalette& palette);
  static void DrawTileDecoder(const std::vector<uint8_t>& gfx16_data, int tile_id);
};

}  // namespace dungeon
}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_DUNGEON_ROOM_VISUAL_DIAGNOSTIC_H

