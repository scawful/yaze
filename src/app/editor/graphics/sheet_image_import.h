#ifndef YAZE_APP_EDITOR_GRAPHICS_SHEET_IMAGE_IMPORT_H
#define YAZE_APP_EDITOR_GRAPHICS_SHEET_IMAGE_IMPORT_H

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"
#include "util/indexed_png.h"

namespace yaze::editor {

/**
 * @file sheet_image_import.h
 * @brief Image import for the Graphics editor that keeps the image's own
 *        palette order and pixel indices.
 *
 * Two inputs are accepted, from a PNG file or the clipboard:
 *  - Indexed (palette-mode) PNGs: the pixel indices and PLTE order are kept
 *    as they are.
 *  - RGB/RGBA PNGs with a palette strip: an 8-row strip under the art made
 *    of 8x8 swatches, swatch k holding color k (the strip may repeat, e.g.
 *    8 colors twice across a 128-pixel sheet). Each art pixel maps to the
 *    strip index of its exact color; fully transparent pixels map to index
 *    0. A color missing from the strip is an error, never a nearest match.
 *
 * Indexed PNGs that carry the same strip (swatch k filled with index k) have
 * it removed too. Nothing here touches the ROM or editor state.
 */

enum class ImportedImageSource {
  kIndexedPng,    // indices and PLTE order taken from the file
  kPaletteStrip,  // RGB pixels matched exactly against a palette strip
};

struct ImportedSheetImage {
  int width = 0;
  int height = 0;                // art only, without the palette strip
  std::vector<uint8_t> indices;  // width * height, row-major
  // Index order. For a palette strip, the strip's colors; for an indexed PNG,
  // its PLTE (with tRNS alpha).
  std::vector<std::array<uint8_t, 4>> palette;
  ImportedImageSource source = ImportedImageSource::kIndexedPng;
  bool strip_removed = false;
  int max_index = 0;  // highest index the art uses
};

/// Converts a decoded PNG without re-quantizing it.
absl::StatusOr<ImportedSheetImage> ImportSheetImage(const util::PngImage& png);

/// Decodes PNG bytes (file or clipboard) and calls ImportSheetImage.
absl::StatusOr<ImportedSheetImage> ImportSheetImagePng(
    const std::vector<uint8_t>& png_bytes);

/// Packs the art into SNES planar tiles (3bpp: 24 bytes, 4bpp: 32 bytes per
/// 8x8 tile), tiles in row-major order. The image must be a multiple of 8 on
/// both sides; indices that do not fit the depth are an error, never masked.
absl::StatusOr<std::vector<uint8_t>> EncodeImportedSheetTiles(
    const ImportedSheetImage& image, int bpp);

/// Re-encodes the art (without the strip) as an indexed PNG in index order.
absl::StatusOr<std::vector<uint8_t>> EncodeImportedSheetPng(
    const ImportedSheetImage& image);

/// "128x32 indexed PNG, 8 colors (strip removed), highest index 7".
std::string DescribeImportedSheetImage(const ImportedSheetImage& image);

/// Palette color slots that a sheet preview shows as indices 1..`count` for
/// row `row` of a palette in `group_name` (same mapping as the Graphics
/// editor's preview palette). Index 0 is transparent and has no slot.
absl::StatusOr<std::vector<int>> PreviewPaletteSlots(
    std::string_view group_name, size_t palette_size, int row, int count);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_GRAPHICS_SHEET_IMAGE_IMPORT_H
