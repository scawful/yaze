#ifndef YAZE_APP_EDITOR_GRAPHICS_PIXEL_CLIPBOARD_H
#define YAZE_APP_EDITOR_GRAPHICS_PIXEL_CLIPBOARD_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/statusor.h"

namespace yaze::editor {

/// The 8 colors a 3bpp sheet shows (index 0 is transparent in game).
using SheetColors = std::array<std::array<uint8_t, 3>, 8>;

/// Pixels as an indexed PNG in the sheet's colors, index 0 transparent, for
/// the system clipboard. Indices must be 0-7.
absl::StatusOr<std::vector<uint8_t>> EncodeClipboardPng(
    const std::vector<uint8_t>& indices, int width, int height,
    const SheetColors& colors);

/// A clipboard PNG mapped onto a sheet's colors.
struct ClipboardPaste {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> indices;  // 0-7, row-major
  int kept = 0;         // indexed pixels kept as they were (indices 0-7)
  int exact = 0;        // pixels whose color matched a sheet color exactly
  int remapped = 0;     // pixels given the nearest sheet color
  int transparent = 0;  // pixels with alpha below 128, mapped to 0
  int above_depth = 0;  // indexed pixels that used an index above 7
};

/// Maps a PNG onto the sheet's colors. An indexed PNG that only uses indices
/// 0-7 keeps its indices. Otherwise each pixel maps by color: transparent
/// pixels to 0, exact matches to that index, anything else to the nearest of
/// colors 1-7 (counted as remapped). Indexed pixels above 7 are also counted
/// in above_depth so the preview can flag them. Refuses images larger than
/// `max_width` x `max_height`.
absl::StatusOr<ClipboardPaste> MapClipboardPng(
    const std::vector<uint8_t>& png_bytes, const SheetColors& colors,
    int max_width = 128, int max_height = 32);

/// "16x16: 200 kept" / "16x8: 90 exact, 38 remapped, 4 above color 7".
std::string DescribeClipboardPaste(const ClipboardPaste& paste);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_GRAPHICS_PIXEL_CLIPBOARD_H
