#ifndef YAZE_APP_EDITOR_GRAPHICS_GRAPHICS_PREVIEW_IMAGE_H_
#define YAZE_APP_EDITOR_GRAPHICS_GRAPHICS_PREVIEW_IMAGE_H_

#include <array>
#include <cstdint>
#include <vector>

#include "app/platform/sdl_compat.h"

namespace yaze::editor::usage_preview {

/// An 8bpp image with its own 256-color table. Index 0 is transparent when
/// `index0_transparent` is set (sprites, tile16s).
struct IndexedImage {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> pixels;
  std::array<SDL_Color, 256> colors{};
  bool index0_transparent = false;

  bool empty() const { return width <= 0 || height <= 0; }
  /// RGBA8 rows; transparent pixels get alpha 0.
  std::vector<uint8_t> ToRgba() const;
  /// Number of pixels whose index differs from `other` (sizes must match;
  /// returns -1 when they do not).
  int CountDifferentPixels(const IndexedImage& other) const;
};

}  // namespace yaze::editor::usage_preview

#endif  // YAZE_APP_EDITOR_GRAPHICS_GRAPHICS_PREVIEW_IMAGE_H_
