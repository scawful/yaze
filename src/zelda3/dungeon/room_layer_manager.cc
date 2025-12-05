#include "zelda3/dungeon/room_layer_manager.h"

#include <algorithm>
#include <vector>

#include "SDL.h"
#include "app/gfx/types/snes_palette.h"

namespace yaze {
namespace zelda3 {

namespace {

// Helper to copy SDL palette from source surface to destination bitmap
// Uses vector extraction + SetPalette for reliable palette application
void ApplySDLPaletteToBitmap(SDL_Surface* src_surface, gfx::Bitmap& dst_bitmap) {
  if (!src_surface || !src_surface->format) return;

  SDL_Palette* src_pal = src_surface->format->palette;
  if (!src_pal || src_pal->ncolors == 0) return;

  // Extract palette colors into a vector
  std::vector<SDL_Color> colors(256);
  int colors_to_copy = std::min(src_pal->ncolors, 256);
  for (int i = 0; i < colors_to_copy; ++i) {
    colors[i] = src_pal->colors[i];
  }

  // Fill remaining with transparent black (prevents undefined colors)
  for (int i = colors_to_copy; i < 256; ++i) {
    colors[i] = {0, 0, 0, 0};
  }

  // IMPORTANT: Do NOT force palette[0] to transparent!
  // In the dungeon system, pixel value 0 is never drawn (skipped in IsTransparent),
  // but palette[0] contains the FIRST actual color. Pixel value 1 maps to palette[0].
  // Only index 255 needs to be transparent (fill color for empty areas).
  colors[255] = {0, 0, 0, 0};

  // Apply palette to destination bitmap using the reliable method
  dst_bitmap.SetPalette(colors);
}

}  // namespace

void RoomLayerManager::CompositeToOutput(Room& room,
                                          gfx::Bitmap& output) const {
  constexpr int kWidth = 512;
  constexpr int kHeight = 512;
  constexpr int kPixelCount = kWidth * kHeight;

  // Ensure output bitmap is properly sized
  if (output.width() != kWidth || output.height() != kHeight) {
    output.Create(kWidth, kHeight, 8,
                  std::vector<uint8_t>(kPixelCount, 255));
  } else {
    // Clear to transparent (255)
    output.Fill(255);
  }

  // Track if we've copied the palette yet
  bool palette_copied = false;

  // Get all 4 layer buffers
  auto& bg1_layout = GetLayerBuffer(room, LayerType::BG1_Layout);
  auto& bg1_objects = GetLayerBuffer(room, LayerType::BG1_Objects);
  auto& bg2_layout = GetLayerBuffer(room, LayerType::BG2_Layout);
  auto& bg2_objects = GetLayerBuffer(room, LayerType::BG2_Objects);

  // Copy palette from first available visible layer
  auto CopyPaletteIfNeeded = [&](const gfx::Bitmap& src_bitmap) {
    if (!palette_copied && src_bitmap.surface()) {
      ApplySDLPaletteToBitmap(src_bitmap.surface(), output);
      palette_copied = true;
    }
  };

  // Helper to composite a single layer using simple back-to-front ordering
  // Based on SNES ASM analysis: BG2 is drawn first, then BG1 overwrites on top
  // This matches the SNES tilemap buffer architecture:
  //   $7E4000 = Lower layer (BG2) - drawn first
  //   $7E2000 = Upper layer (BG1) - drawn on top
  auto CompositeLayer = [&](gfx::BackgroundBuffer& buffer, LayerType layer_type) {
    if (!IsLayerVisible(layer_type)) return;
    
    const auto& src_bitmap = buffer.bitmap();
    if (!src_bitmap.is_active() || src_bitmap.width() == 0) return;
    
    LayerBlendMode blend_mode = GetLayerBlendMode(layer_type);
    if (blend_mode == LayerBlendMode::Off) return;
    
    CopyPaletteIfNeeded(src_bitmap);
    
    const auto& src_data = src_bitmap.data();
    auto& dst_data = output.mutable_data();
    
    // Simple back-to-front compositing: later layers overwrite earlier layers
    // Non-transparent pixels always overwrite whatever is beneath them
    for (int idx = 0; idx < kPixelCount; ++idx) {
      uint8_t src_pixel = src_data[idx];
      
      // Skip transparent pixels (255 = fill color for undrawn areas)
      if (IsTransparent(src_pixel)) continue;
      
      // Overwrite destination pixel
      dst_data[idx] = src_pixel;
    }
  };

  // Process all layers in back-to-front order (matching SNES hardware)
  // BG2 is the lower/background layer, BG1 is the upper/foreground layer
  // This is the standard SNES Mode 1 rendering order
  
  // 1. BG2 Layout (floor tiles, background)
  CompositeLayer(bg2_layout, LayerType::BG2_Layout);
  
  // 2. BG2 Objects (objects drawn to background layer)
  CompositeLayer(bg2_objects, LayerType::BG2_Objects);
  
  // 3. BG1 Layout (main room structure) - overwrites BG2
  CompositeLayer(bg1_layout, LayerType::BG1_Layout);
  
  // 4. BG1 Objects (objects drawn to foreground layer) - overwrites all
  CompositeLayer(bg1_objects, LayerType::BG1_Objects);

  // If no palette was copied from layers, try to get it from bg1_buffer directly
  if (!palette_copied) {
    const auto& bg1_bitmap = room.bg1_buffer().bitmap();
    if (bg1_bitmap.surface()) {
      ApplySDLPaletteToBitmap(bg1_bitmap.surface(), output);
    }
  }

  // Sync pixel data to SDL surface for texture creation
  output.UpdateSurfacePixels();

  // Set up transparency and effects for the composite output
  if (output.surface()) {
    // IMPORTANT: Use the same transparency setup as room.cc's set_dungeon_palette
    // Color key on index 255 (unused in 90-color dungeon palette)
    SDL_SetColorKey(output.surface(), SDL_TRUE, 255);
    SDL_SetSurfaceBlendMode(output.surface(), SDL_BLENDMODE_BLEND);

    // Apply DarkRoom effect if merge type is 0x08
    // This simulates the SNES master brightness reduction for unlit rooms
    if (current_merge_type_id_ == 0x08) {
      // Apply color modulation to darken the output (50% brightness)
      SDL_SetSurfaceColorMod(output.surface(), 128, 128, 128);
    } else {
      // Reset to full brightness for non-dark rooms
      SDL_SetSurfaceColorMod(output.surface(), 255, 255, 255);
    }
  }

  // Mark output as modified for texture update
  output.set_modified(true);
}

}  // namespace zelda3
}  // namespace yaze
