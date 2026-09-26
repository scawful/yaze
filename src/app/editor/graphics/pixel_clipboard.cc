#include "app/editor/graphics/pixel_clipboard.h"

#include <limits>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "util/indexed_png.h"
#include "util/macro.h"

namespace yaze::editor {

absl::StatusOr<std::vector<uint8_t>> EncodeClipboardPng(
    const std::vector<uint8_t>& indices, int width, int height,
    const SheetColors& colors) {
  if (width <= 0 || height <= 0 ||
      indices.size() != static_cast<size_t>(width) * height) {
    return absl::InvalidArgumentError("Clipboard image has no pixels");
  }
  std::vector<std::array<uint8_t, 4>> palette;
  for (size_t i = 0; i < colors.size(); ++i) {
    palette.push_back({colors[i][0], colors[i][1], colors[i][2],
                       static_cast<uint8_t>(i == 0 ? 0 : 255)});
  }
  return util::EncodeIndexedPng(width, height, indices, palette);
}

absl::StatusOr<ClipboardPaste> MapClipboardPng(
    const std::vector<uint8_t>& png_bytes, const SheetColors& colors,
    int max_width, int max_height) {
  ASSIGN_OR_RETURN(const util::PngImage image, util::DecodePng(png_bytes));
  if (image.width > max_width || image.height > max_height) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "The clipboard image is %dx%d; a sheet holds at most %dx%d",
        image.width, image.height, max_width, max_height));
  }
  ClipboardPaste paste;
  paste.width = image.width;
  paste.height = image.height;
  const size_t count = static_cast<size_t>(image.width) * image.height;
  paste.indices.resize(count);

  bool indices_fit = image.indexed;
  if (image.indexed) {
    for (uint8_t index : image.indices) {
      if (index > 7) {
        indices_fit = false;
        ++paste.above_depth;
      }
    }
  }
  if (indices_fit) {
    paste.indices = image.indices;
    paste.kept = static_cast<int>(count);
    return paste;
  }

  for (size_t i = 0; i < count; ++i) {
    std::array<uint8_t, 4> rgba;
    if (image.indexed) {
      const uint8_t index = image.indices[i];
      rgba = index < image.palette.size() ? image.palette[index]
                                          : std::array<uint8_t, 4>{0, 0, 0, 0};
    } else {
      rgba = {image.rgba[i * 4], image.rgba[i * 4 + 1], image.rgba[i * 4 + 2],
              image.rgba[i * 4 + 3]};
    }
    if (rgba[3] < 128) {
      paste.indices[i] = 0;
      ++paste.transparent;
      continue;
    }
    int best = -1;
    int best_distance = std::numeric_limits<int>::max();
    for (int c = 0; c < 8; ++c) {
      const int dr = rgba[0] - colors[c][0];
      const int dg = rgba[1] - colors[c][1];
      const int db = rgba[2] - colors[c][2];
      const int distance = dr * dr + dg * dg + db * db;
      if (distance == 0) {
        best = c;
        best_distance = 0;
        break;
      }
      // Color 0 is transparent in game; only an exact match maps to it.
      if (c != 0 && distance < best_distance) {
        best = c;
        best_distance = distance;
      }
    }
    paste.indices[i] = static_cast<uint8_t>(best);
    if (best_distance == 0) {
      ++paste.exact;
    } else {
      ++paste.remapped;
    }
  }
  return paste;
}

std::string DescribeClipboardPaste(const ClipboardPaste& paste) {
  std::vector<std::string> parts;
  if (paste.kept > 0) {
    parts.push_back(absl::StrFormat("%d kept", paste.kept));
  }
  if (paste.exact > 0) {
    parts.push_back(absl::StrFormat("%d exact", paste.exact));
  }
  if (paste.remapped > 0) {
    parts.push_back(absl::StrFormat("%d remapped", paste.remapped));
  }
  if (paste.transparent > 0) {
    parts.push_back(absl::StrFormat("%d transparent", paste.transparent));
  }
  if (paste.above_depth > 0) {
    parts.push_back(absl::StrFormat("%d above color 7", paste.above_depth));
  }
  return absl::StrFormat("%dx%d: %s", paste.width, paste.height,
                         absl::StrJoin(parts, ", "));
}

}  // namespace yaze::editor
