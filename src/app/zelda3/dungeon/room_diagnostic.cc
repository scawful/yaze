#include "room.h"

#include <cstdio>
#include <vector>

#include "app/gfx/arena.h"
#include "app/gfx/bitmap.h"

namespace yaze {
namespace zelda3 {

void DiagnoseRoomRendering(Room& room, int room_id) {
  std::printf("\n========== ROOM RENDERING DIAGNOSTIC ==========\n");
  std::printf("Room ID: %d\n\n", room_id);
  
  // Step 1: Check ROM and graphics buffer
  std::printf("=== Step 1: ROM and Graphics Buffer ===\n");
  auto* rom = room.rom();
  if (!rom) {
    std::printf("❌ ROM pointer is NULL\n");
    return;
  }
  std::printf("✓ ROM pointer valid\n");
  std::printf("  ROM loaded: %s\n", rom->is_loaded() ? "YES" : "NO");
  std::printf("  ROM size: %zu bytes\n", rom->size());
  
  auto* gfx_buffer = rom->mutable_graphics_buffer();
  if (!gfx_buffer || gfx_buffer->empty()) {
    std::printf("❌ Graphics buffer empty\n");
    return;
  }
  std::printf("✓ Graphics buffer loaded: %zu bytes\n", gfx_buffer->size());
  
  // Step 2: Check room blocks
  std::printf("\n=== Step 2: Room Graphics Blocks ===\n");
  auto blocks = room.blocks();
  std::printf("  Blocks loaded: %s\n", blocks.empty() ? "NO" : "YES");
  if (!blocks.empty()) {
    std::printf("  Block indices: ");
    for (int i = 0; i < 16; i++) {
      std::printf("%d ", blocks[i]);
      if (i == 7) std::printf("\n                 ");
    }
    std::printf("\n");
  }
  
  // Step 3: Check current_gfx16_ buffer
  std::printf("\n=== Step 3: current_gfx16_ Buffer ===\n");
  // Sample first 100 bytes to check if populated
  bool has_data = false;
  int non_zero_count = 0;
  for (int i = 0; i < 100 && i < 32768; i++) {
    // Access through room's internal buffer would require making it accessible
    // For now, just note this step
  }
  std::printf("  Note: current_gfx16_ is internal, assuming populated after CopyRoomGraphicsToBuffer()\n");
  
  // Step 4: Check background buffers in arena
  std::printf("\n=== Step 4: Background Buffers (Arena) ===\n");
  auto& bg1 = gfx::Arena::Get().bg1();
  auto& bg2 = gfx::Arena::Get().bg2();
  auto bg1_buffer = bg1.buffer();
  auto bg2_buffer = bg2.buffer();
  
  std::printf("BG1 Buffer:\n");
  std::printf("  Size: %zu\n", bg1_buffer.size());
  int bg1_non_ff = 0;
  int bg1_non_zero = 0;
  for (const auto& word : bg1_buffer) {
    if (word != 0xFFFF) bg1_non_ff++;
    if (word != 0) bg1_non_zero++;
  }
  std::printf("  Non-0xFFFF tiles: %d / %zu\n", bg1_non_ff, bg1_buffer.size());
  std::printf("  Non-zero tiles: %d / %zu\n", bg1_non_zero, bg1_buffer.size());
  
  // Sample first 10 non-0xFFFF tiles
  std::printf("  Sample tiles (first 10 non-0xFFFF): ");
  int sample_count = 0;
  for (size_t i = 0; i < bg1_buffer.size() && sample_count < 10; i++) {
    if (bg1_buffer[i] != 0xFFFF) {
      std::printf("0x%04X ", bg1_buffer[i]);
      sample_count++;
    }
  }
  std::printf("\n");
  
  std::printf("\nBG2 Buffer:\n");
  std::printf("  Size: %zu\n", bg2_buffer.size());
  int bg2_non_ff = 0;
  int bg2_non_zero = 0;
  for (const auto& word : bg2_buffer) {
    if (word != 0xFFFF) bg2_non_ff++;
    if (word != 0) bg2_non_zero++;
  }
  std::printf("  Non-0xFFFF tiles: %d / %zu\n", bg2_non_ff, bg2_buffer.size());
  std::printf("  Non-zero tiles: %d / %zu\n", bg2_non_zero, bg2_buffer.size());
  
  // Step 5: Check bitmaps
  std::printf("\n=== Step 5: Bitmaps ===\n");
  auto& bg1_bitmap = bg1.bitmap();
  auto& bg2_bitmap = bg2.bitmap();
  
  std::printf("BG1 Bitmap:\n");
  std::printf("  Active: %s\n", bg1_bitmap.is_active() ? "YES" : "NO");
  std::printf("  Dimensions: %dx%d\n", bg1_bitmap.width(), bg1_bitmap.height());
  std::printf("  Data size: %zu bytes\n", bg1_bitmap.vector().size());
  std::printf("  Modified: %s\n", bg1_bitmap.modified() ? "YES" : "NO");
  
  if (!bg1_bitmap.vector().empty()) {
    // Sample first 100 pixels
    int non_zero_pixels = 0;
    std::vector<uint8_t> unique_colors;
    for (size_t i = 0; i < 100 && i < bg1_bitmap.vector().size(); i++) {
      uint8_t pixel = bg1_bitmap.vector()[i];
      if (pixel != 0) non_zero_pixels++;
      if (std::find(unique_colors.begin(), unique_colors.end(), pixel) == unique_colors.end()) {
        unique_colors.push_back(pixel);
      }
    }
    std::printf("  First 100 pixels: %d non-zero, %zu unique colors\n", 
                non_zero_pixels, unique_colors.size());
    std::printf("  Unique colors: ");
    for (size_t i = 0; i < std::min<size_t>(10, unique_colors.size()); i++) {
      std::printf("%d ", unique_colors[i]);
    }
    std::printf("\n");
  }
  
  std::printf("\nBG2 Bitmap:\n");
  std::printf("  Active: %s\n", bg2_bitmap.is_active() ? "YES" : "NO");
  std::printf("  Dimensions: %dx%d\n", bg2_bitmap.width(), bg2_bitmap.height());
  std::printf("  Data size: %zu bytes\n", bg2_bitmap.vector().size());
  
  // Step 6: Check textures
  std::printf("\n=== Step 6: SDL Textures ===\n");
  std::printf("BG1 Texture:\n");
  std::printf("  Texture pointer: %p\n", (void*)bg1_bitmap.texture());
  std::printf("  Texture valid: %s\n", bg1_bitmap.texture() != nullptr ? "YES" : "NO");
  
  std::printf("\nBG2 Texture:\n");
  std::printf("  Texture pointer: %p\n", (void*)bg2_bitmap.texture());
  std::printf("  Texture valid: %s\n", bg2_bitmap.texture() != nullptr ? "YES" : "NO");
  
  // Step 7: Check palette
  std::printf("\n=== Step 7: Palette ===\n");
  auto& palette = bg1_bitmap.palette();
  std::printf("  Palette size: %zu colors\n", palette.size());
  if (!palette.empty()) {
    std::printf("  First 8 colors (SNES format): ");
    for (size_t i = 0; i < 8 && i < palette.size(); i++) {
      std::printf("0x%04X ", palette[i].snes());
    }
    std::printf("\n");
    
    // Check the actual colors being used by the bitmap pixels
    std::printf("  Colors at pixel indices being used:\n");
    std::vector<uint8_t> sample_indices;
    for (size_t i = 0; i < 100 && i < bg1_bitmap.vector().size(); i++) {
      uint8_t idx = bg1_bitmap.vector()[i];
      if (std::find(sample_indices.begin(), sample_indices.end(), idx) == sample_indices.end()) {
        sample_indices.push_back(idx);
      }
    }
    for (size_t i = 0; i < std::min<size_t>(10, sample_indices.size()); i++) {
      uint8_t idx = sample_indices[i];
      if (idx < palette.size()) {
        auto color = palette[idx];
        auto rgb_vec = color.rgb();
        std::printf("    [%d] = SNES:0x%04X RGB:(%.0f,%.0f,%.0f)\n", 
                    idx, color.snes(), rgb_vec.x * 255, rgb_vec.y * 255, rgb_vec.z * 255);
      }
    }
  }
  
  // Step 8: Check room objects
  std::printf("\n=== Step 8: Room Objects ===\n");
  const auto& objects = room.GetTileObjects();
  std::printf("  Object count: %zu\n", objects.size());
  if (!objects.empty()) {
    std::printf("  First 5 objects:\n");
    for (size_t i = 0; i < 5 && i < objects.size(); i++) {
      const auto& obj = objects[i];
      std::printf("    [%zu] ID=0x%03X, Pos=(%d,%d), Size=%d, Layer=%d\n",
                  i, obj.id_, obj.x_, obj.y_, obj.size_, obj.GetLayerValue());
    }
  }
  
  std::printf("\n========== DIAGNOSTIC COMPLETE ==========\n\n");
}

}  // namespace zelda3
}  // namespace yaze

