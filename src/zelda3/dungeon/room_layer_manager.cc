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

  // Get draw order (SNES Mode 1: BG2 behind, BG1 on top)
  auto draw_order = GetDrawOrder();

  // Track if we've copied the palette yet
  bool palette_copied = false;

  // Process each layer in order (back to front)
  for (auto layer_type : draw_order) {
    // Skip invisible layers
    if (!IsLayerVisible(layer_type)) {
      continue;
    }

    // Get layer buffer
    auto& buffer = GetLayerBuffer(room, layer_type);
    const auto& src_bitmap = buffer.bitmap();

    // Skip if bitmap not active or empty
    if (!src_bitmap.is_active() || src_bitmap.width() == 0) {
      continue;
    }

    // Get blend mode for this layer
    LayerBlendMode blend_mode = GetLayerBlendMode(layer_type);

    // Skip if layer is completely hidden
    if (blend_mode == LayerBlendMode::Off) {
      continue;
    }

    // Copy SDL palette from first visible layer's surface to output bitmap
    // Room applies palettes via SetPalette(vector<SDL_Color>) which only sets
    // SDL surface palette, NOT the internal SnesPalette member
    if (!palette_copied && src_bitmap.surface()) {
      ApplySDLPaletteToBitmap(src_bitmap.surface(), output);
      palette_copied = true;
    }

    // Composite this layer onto output
    CompositeLayer(src_bitmap, output, blend_mode);
  }

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

void RoomLayerManager::CompositeLayer(const gfx::Bitmap& src,
                                       gfx::Bitmap& dst,
                                       LayerBlendMode mode) const {
  const int width = std::min(src.width(), dst.width());
  const int height = std::min(src.height(), dst.height());

  auto& dst_data = dst.mutable_data();
  const auto& src_data = src.data();

  // Simple pixel-by-pixel compositing
  // For indexed color, all blend modes use simple replacement
  // since true blending requires expensive RGB palette lookups.
  // The visual blend effect is handled at display time via alpha.
  for (int y = 0; y < height; ++y) {
    const int row_offset = y * width;
    for (int x = 0; x < width; ++x) {
      const int idx = row_offset + x;
      const uint8_t src_pixel = src_data[idx];

      // Skip transparent pixels (index 0 = SNES transparent, 255 = fill color)
      if (IsTransparent(src_pixel)) {
        continue;
      }

      // Replace destination pixel with source
      // Blend mode effects are visual-only (handled at display time)
      dst_data[idx] = src_pixel;
    }
  }
}

}  // namespace zelda3
}  // namespace yaze
