#include "manual_object_renderer.h"

#include <cstdio>
#include <cstring>

#include "app/gfx/arena.h"
#include "app/gfx/bitmap.h"

namespace yaze {
namespace editor {

ManualObjectRenderer::ManualObjectRenderer(gui::Canvas* canvas, Rom* rom)
    : canvas_(canvas), rom_(rom) {}

absl::Status ManualObjectRenderer::RenderSimpleBlock(uint16_t object_id, int x, int y,
                                                   const gfx::SnesPalette& palette) {
  if (!canvas_ || !rom_) {
    return absl::InvalidArgumentError("Canvas or ROM not initialized");
  }

  printf("[ManualRenderer] Rendering object 0x%04X at (%d, %d)\n", object_id, x, y);

  // Create a simple 16x16 tile manually
  auto tile_bitmap = CreateSimpleTile(object_id, palette);
  if (!tile_bitmap) {
    return absl::InternalError("Failed to create simple tile");
  }

  // Draw directly to canvas
  canvas_->DrawBitmap(*tile_bitmap, x, y, 1.0f, 255);
  
  return absl::OkStatus();
}

void ManualObjectRenderer::RenderTestPattern(int x, int y, int width, int height, 
                                           uint8_t color_index) {
  if (!canvas_) return;

  printf("[ManualRenderer] Drawing test pattern: %dx%d at (%d,%d) color=%d\n", 
         width, height, x, y, color_index);

  // Create a simple colored rectangle using ImGui
  ImVec4 color = ImVec4(
    (color_index & 0x01) ? 1.0f : 0.0f,  // Red bit
    (color_index & 0x02) ? 1.0f : 0.0f,  // Green bit  
    (color_index & 0x04) ? 1.0f : 0.0f,  // Blue bit
    1.0f  // Alpha
  );
  
  // Draw using ImGui primitives for testing
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  if (draw_list) {
    ImVec2 p1 = ImVec2(x, y);
    ImVec2 p2 = ImVec2(x + width, y + height);
    draw_list->AddRectFilled(p1, p2, ImGui::ColorConvertFloat4ToU32(color));
  }
}

void ManualObjectRenderer::DebugGraphicsSheet(int sheet_index) {
  if (!rom_ || sheet_index < 0 || sheet_index >= 223) {
    printf("[ManualRenderer] Invalid sheet index: %d\n", sheet_index);
    return;
  }

  auto& arena = gfx::Arena::Get();
  const auto& sheet = arena.gfx_sheet(sheet_index);
  
  printf("[ManualRenderer] Graphics Sheet %d Debug Info:\n", sheet_index);
  printf("  - Is Active: %s\n", sheet.is_active() ? "YES" : "NO");
  printf("  - Width: %d\n", sheet.width());
  printf("  - Height: %d\n", sheet.height());
  printf("  - Has Surface: %s\n", sheet.surface() ? "YES" : "NO");
  printf("  - Has Texture: %s\n", sheet.texture() ? "YES" : "NO");
  
  if (sheet.is_active() && sheet.width() > 0 && sheet.height() > 0) {
    printf("  - Format: %s\n", sheet.surface() && sheet.surface()->format ? 
           SDL_GetPixelFormatName(sheet.surface()->format->format) : "Unknown");
  }
}

void ManualObjectRenderer::TestPaletteRendering(int x, int y) {
  printf("[ManualRenderer] Testing palette rendering at (%d, %d)\n", x, y);
  
  // Draw test squares with different color indices
  for (int i = 0; i < 8; i++) {
    int test_x = x + (i * 20);
    int test_y = y;
    RenderTestPattern(test_x, test_y, 16, 16, i);
  }
}

std::unique_ptr<gfx::Bitmap> ManualObjectRenderer::CreateSimpleTile(uint16_t tile_id,
                                                                   const gfx::SnesPalette& palette) {
  // Fill with a simple pattern based on tile_id
  uint8_t base_color = tile_id & 0x07;  // Use lower 3 bits for color
  
  // Create tile data manually
  auto tile_data = CreateSolidTile(base_color);
  if (tile_data.empty()) {
    printf("[ManualRenderer] Failed to create tile data\n");
    return nullptr;
  }

  // Create a 16x16 bitmap with the tile data
  auto bitmap = std::make_unique<gfx::Bitmap>();
  bitmap->Create(16, 16, 8, tile_data);  // 16x16 pixels, 8-bit depth
  
  if (!bitmap->is_active()) {
    printf("[ManualRenderer] Failed to create bitmap\n");
    return nullptr;
  }

  // Apply palette
  bitmap->SetPalette(palette);
  
  printf("[ManualRenderer] Created simple tile: ID=0x%04X, color=%d\n", tile_id, base_color);

  return bitmap;
}

std::vector<uint8_t> ManualObjectRenderer::CreateSolidTile(uint8_t color_index) {
  std::vector<uint8_t> tile_data(16 * 16, color_index);
  return tile_data;
}

std::vector<uint8_t> ManualObjectRenderer::CreatePatternTile(uint8_t pattern_type, 
                                                            uint8_t color_index) {
  std::vector<uint8_t> tile_data(16 * 16);
  
  switch (pattern_type) {
    case 0:  // Solid
      std::fill(tile_data.begin(), tile_data.end(), color_index);
      break;
      
    case 1:  // Checkerboard
      for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
          tile_data[y * 16 + x] = ((x + y) % 2) ? color_index : 0;
        }
      }
      break;
      
    case 2:  // Horizontal stripes
      for (int y = 0; y < 16; y++) {
        uint8_t color = (y % 4 < 2) ? color_index : 0;
        for (int x = 0; x < 16; x++) {
          tile_data[y * 16 + x] = color;
        }
      }
      break;
      
    case 3:  // Vertical stripes
      for (int x = 0; x < 16; x++) {
        uint8_t color = (x % 4 < 2) ? color_index : 0;
        for (int y = 0; y < 16; y++) {
          tile_data[y * 16 + x] = color;
        }
      }
      break;
      
    default:
      std::fill(tile_data.begin(), tile_data.end(), color_index);
      break;
  }
  
  return tile_data;
}

}  // namespace editor
}  // namespace yaze
