#ifndef YAZE_APP_EDITOR_GRAPHICS_GRAPHICS_PREVIEW_IMAGE_H_
#define YAZE_APP_EDITOR_GRAPHICS_GRAPHICS_PREVIEW_IMAGE_H_

#include <array>
#include <cstdint>
#include <vector>

#include "app/platform/sdl_compat.h"

namespace yaze::editor::usage_preview {

/// Display snapshot with an explicit pixel format. Source indices are retained
/// for uncomposited assets; color-math results use packed RGBA.
struct PreviewImage {
  enum class Format { kIndexed, kRgba };
  Format format = Format::kIndexed;
  int width = 0;
  int height = 0;
  std::vector<uint8_t> pixels;
  std::array<SDL_Color, 256> colors{};
  bool index0_transparent = false;

  bool empty() const { return width <= 0 || height <= 0; }
  /// RGBA8 rows; transparent pixels get alpha 0.
  std::vector<uint8_t> ToRgba() const;
  /// Number of pixels whose displayed color differs (sizes must match;
  /// returns -1 when they do not).
  int CountDifferentPixels(const PreviewImage& other) const;
};

}  // namespace yaze::editor::usage_preview

#endif  // YAZE_APP_EDITOR_GRAPHICS_GRAPHICS_PREVIEW_IMAGE_H_
