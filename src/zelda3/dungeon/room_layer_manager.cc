#include "zelda3/dungeon/room_layer_manager.h"

#include <algorithm>

namespace yaze {
namespace zelda3 {

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

  // Get draw order (respects bg2_on_top_ setting)
  auto draw_order = GetDrawOrder();

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

    // Composite this layer onto output
    CompositeLayer(src_bitmap, output, blend_mode);
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
