#include "zelda3/dungeon/room_layer_manager.h"

#include <algorithm>
#include <vector>

#include "SDL.h"
#include "app/gfx/types/snes_palette.h"
#include "util/log.h"

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

  // Dungeon rendering uses palette index 255 as the "undrawn/transparent" fill.
  // Palette index 0 is not written by our tile renderer (pixel value 0 is skipped),
  // so we reserve it as an opaque backdrop color for the composited output.
  colors[0] = {0, 0, 0, 255};
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

  // Log layer visibility and blend modes (once per room)
  static int last_room_id = -1;
  if (room.id() != last_room_id) {
    last_room_id = room.id();
    LOG_DEBUG("LayerManager", "Room %03X: BG1_Layout(vis=%d,blend=%d) "
              "BG1_Objects(vis=%d,blend=%d) BG2_Layout(vis=%d,blend=%d) "
              "BG2_Objects(vis=%d,blend=%d) MergeType=%d",
              room.id(),
              IsLayerVisible(LayerType::BG1_Layout),
              static_cast<int>(GetLayerBlendMode(LayerType::BG1_Layout)),
              IsLayerVisible(LayerType::BG1_Objects),
              static_cast<int>(GetLayerBlendMode(LayerType::BG1_Objects)),
              IsLayerVisible(LayerType::BG2_Layout),
              static_cast<int>(GetLayerBlendMode(LayerType::BG2_Layout)),
              IsLayerVisible(LayerType::BG2_Objects),
              static_cast<int>(GetLayerBlendMode(LayerType::BG2_Objects)),
              current_merge_type_id_);
  }

  // Ensure output bitmap is properly sized
  if (output.width() != kWidth || output.height() != kHeight) {
    output.Create(kWidth, kHeight, 8,
                  std::vector<uint8_t>(kPixelCount, 0));
  } else {
    // Clear to backdrop (0). Transparent pixels (255) from layers will reveal
    // this backdrop, matching SNES behavior when all layers are transparent.
    output.Fill(0);
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

  // Helper to composite a single layer with blend mode support
  // Based on SNES ASM analysis: BG2 is drawn first, then BG1 overwrites on top
  // This matches the SNES tilemap buffer architecture:
  //   $7E4000 = Lower layer (BG2) - drawn first
  //   $7E2000 = Upper layer (BG1) - drawn on top
  //
  // Blend Modes (from LayerMergeType):
  // - Normal: Opaque pixels overwrite destination (standard)
  // - Translucent: 50% alpha blend with destination
  // - Addition: Additive color blending (SNES color math)
  // - Dark: Darkened blend (reduced brightness)
  // - Off: Layer is hidden
  //
  // IMPORTANT: Transparent pixels (255) in BG1 layers ALWAYS reveal BG2 beneath.
  // This is how pits work: mask objects write 255 to BG1, allowing BG2 to show.
  auto CompositeLayer = [&](gfx::BackgroundBuffer& buffer, LayerType layer_type) {
    if (!IsLayerVisible(layer_type)) return;
    
    const auto& src_bitmap = buffer.bitmap();
    if (!src_bitmap.is_active() || src_bitmap.width() == 0) return;
    
    LayerBlendMode blend_mode = GetLayerBlendMode(layer_type);
    if (blend_mode == LayerBlendMode::Off) return;
    
    CopyPaletteIfNeeded(src_bitmap);
    
    const auto& src_data = src_bitmap.data();
    auto& dst_data = output.mutable_data();
    
    // Get layer alpha for translucent/dark blending
    uint8_t layer_alpha = GetLayerAlpha(layer_type);
    
    for (int idx = 0; idx < kPixelCount; ++idx) {
      uint8_t src_pixel = src_data[idx];
      
      // Skip transparent pixels (255 = fill color for undrawn areas)
      // This is CRITICAL for pits: transparent BG1 pixels reveal BG2 beneath
      if (IsTransparent(src_pixel)) continue;
      
      // Apply blend mode
      switch (blend_mode) {
        case LayerBlendMode::Normal:
          // Standard opaque overwrite
          dst_data[idx] = src_pixel;
          break;
          
        case LayerBlendMode::Translucent:
          // 50% alpha blend: only overwrite if destination is transparent,
          // otherwise blend colors using palette index averaging (simplified)
          // For indexed color mode, we can't truly blend - use alpha threshold
          if (IsTransparent(dst_data[idx]) || layer_alpha > 180) {
            dst_data[idx] = src_pixel;
          }
          // If layer_alpha <= 180, destination shows through (simplified blend)
          break;
          
        case LayerBlendMode::Addition:
          // Additive blending: in indexed mode, just use source if visible
          // True additive would need RGB values from palette
          if (IsTransparent(dst_data[idx])) {
            dst_data[idx] = src_pixel;
          }
          // Non-transparent dest + source: in indexed mode, just overwrite
          // (True additive blend would require palette lookup and RGB math)
          else {
            dst_data[idx] = src_pixel;
          }
          break;
          
        case LayerBlendMode::Dark:
          // Darkened blend: overwrite but surface will be color-modulated later
          dst_data[idx] = src_pixel;
          break;
          
        case LayerBlendMode::Off:
          // Layer hidden - should not reach here due to early return
          break;
      }
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
