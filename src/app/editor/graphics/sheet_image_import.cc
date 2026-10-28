#include "app/editor/graphics/sheet_image_import.h"

#include <algorithm>
#include <optional>

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "app/editor/graphics/palette_controls_panel_internal.h"
#include "app/gfx/types/snes_tile.h"

namespace yaze::editor {

namespace {

constexpr int kSwatchSize = 8;
// Strips repeat the palette across the width, so the shortest period that
// fits is the palette size: 4 (2bpp), 8 (3bpp) or 16 (4bpp) colors.
constexpr std::array<int, 3> kStripPaletteSizes = {4, 8, 16};
constexpr int kListedOffenders = 5;

using Rgba = std::array<uint8_t, 4>;

Rgba PixelAt(const util::PngImage& png, int x, int y) {
  const size_t at = (static_cast<size_t>(y) * png.width + x) * 4;
  return {png.rgba[at], png.rgba[at + 1], png.rgba[at + 2], png.rgba[at + 3]};
}

// Value of an 8x8 swatch when every pixel in it is the same, else nullopt.
template <typename T, typename Get>
std::optional<T> UniformSwatch(int swatch, int strip_top, Get get) {
  const int x0 = swatch * kSwatchSize;
  const T first = get(x0, strip_top);
  for (int y = strip_top; y < strip_top + kSwatchSize; ++y) {
    for (int x = x0; x < x0 + kSwatchSize; ++x) {
      if (get(x, y) != first) {
        return std::nullopt;
      }
    }
  }
  return first;
}

// The palette size whose repeats explain every swatch, or 0. `same(a, b)`
// compares swatch a with swatch b.
template <typename Same>
int StripPeriod(int swatches, Same same) {
  for (int size : kStripPaletteSizes) {
    if (swatches < size || swatches % size != 0) {
      continue;
    }
    bool repeats = true;
    for (int k = size; repeats && k < swatches; ++k) {
      repeats = same(k, k % size);
    }
    if (repeats) {
      return size;
    }
  }
  return 0;
}

bool CanHoldStrip(int width, int height) {
  return width % kSwatchSize == 0 && height % kSwatchSize == 0 &&
         height >= 2 * kSwatchSize;
}

int MaxIndex(const std::vector<uint8_t>& indices) {
  return indices.empty() ? 0
                         : *std::max_element(indices.begin(), indices.end());
}

absl::StatusOr<ImportedSheetImage> ImportIndexed(const util::PngImage& png) {
  ImportedSheetImage image;
  image.source = ImportedImageSource::kIndexedPng;
  image.width = png.width;
  image.height = png.height;
  image.palette = png.palette;
  image.indices = png.indices;

  // A strip drawn with the file's own indices (swatch k = index k) is
  // dropped the same way as an RGB strip.
  if (CanHoldStrip(png.width, png.height)) {
    const int strip_top = png.height - kSwatchSize;
    const int swatches = png.width / kSwatchSize;
    auto index_at = [&](int x, int y) {
      return png.indices[static_cast<size_t>(y) * png.width + x];
    };
    std::vector<int> swatch_index(swatches, -1);
    bool uniform = true;
    for (int s = 0; uniform && s < swatches; ++s) {
      const auto value = UniformSwatch<uint8_t>(s, strip_top, index_at);
      uniform = value.has_value();
      if (uniform) {
        swatch_index[s] = *value;
      }
    }
    const int size =
        uniform ? StripPeriod(swatches,
                              [&](int a, int b) {
                                return swatch_index[a] == swatch_index[b];
                              })
                : 0;
    bool in_order = size > 0;
    for (int k = 0; in_order && k < size; ++k) {
      in_order = swatch_index[k] == k;
    }
    if (in_order) {
      image.height = strip_top;
      image.indices.resize(static_cast<size_t>(strip_top) * png.width);
      image.strip_removed = true;
    }
  }
  image.max_index = MaxIndex(image.indices);
  return image;
}

absl::StatusOr<ImportedSheetImage> ImportPaletteStrip(
    const util::PngImage& png) {
  const auto no_strip = [&](std::string_view why) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "This %dx%d image is RGB and has no usable palette strip (%s). Use "
        "an indexed PNG, or add an 8-pixel-tall strip under the art: 8x8 "
        "swatches, swatch k holding color k, left to right.",
        png.width, png.height, why));
  };
  if (!CanHoldStrip(png.width, png.height)) {
    return no_strip("width and height must be multiples of 8, height 16+");
  }
  const int strip_top = png.height - kSwatchSize;
  const int swatches = png.width / kSwatchSize;
  auto pixel_at = [&](int x, int y) {
    return PixelAt(png, x, y);
  };

  std::vector<Rgba> swatch_color(swatches);
  for (int s = 0; s < swatches; ++s) {
    const auto value = UniformSwatch<Rgba>(s, strip_top, pixel_at);
    if (!value) {
      return no_strip(absl::StrFormat(
          "the 8x8 swatch at x=%d of the bottom 8 rows is not one color",
          s * kSwatchSize));
    }
    swatch_color[s] = *value;
  }
  const int size = StripPeriod(swatches, [&](int a, int b) {
    return swatch_color[a] == swatch_color[b];
  });
  if (size == 0) {
    return no_strip(absl::StrFormat(
        "%d swatches do not repeat a 4-, 8- or 16-color palette", swatches));
  }

  ImportedSheetImage image;
  image.source = ImportedImageSource::kPaletteStrip;
  image.width = png.width;
  image.height = strip_top;
  image.strip_removed = true;
  image.palette.assign(swatch_color.begin(), swatch_color.begin() + size);
  for (int k = 0; k < size; ++k) {
    const uint8_t alpha = image.palette[k][3];
    // Only index 0 may be a transparent swatch; it stands for transparency.
    if (alpha != 255 && !(k == 0 && alpha == 0)) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Palette strip color %d has alpha %d; strip colors must be opaque "
          "(color 0 may be fully transparent)",
          k, alpha));
    }
    for (int j = 0; j < k; ++j) {
      if (image.palette[j][3] == 255 && alpha == 255 &&
          std::equal(image.palette[j].begin(), image.palette[j].begin() + 3,
                     image.palette[k].begin())) {
        return absl::InvalidArgumentError(absl::StrFormat(
            "Palette strip colors %d and %d are both #%02X%02X%02X, so "
            "pixels of that color cannot be mapped to one index",
            j, k, image.palette[k][0], image.palette[k][1],
            image.palette[k][2]));
      }
    }
  }

  image.indices.resize(static_cast<size_t>(image.width) * image.height);
  std::vector<std::string> offenders;
  size_t offender_count = 0;
  for (int y = 0; y < image.height; ++y) {
    for (int x = 0; x < image.width; ++x) {
      const Rgba pixel = PixelAt(png, x, y);
      int index = -1;
      if (pixel[3] == 0) {
        index = 0;
      } else if (pixel[3] == 255) {
        for (int k = 0; k < size; ++k) {
          if (image.palette[k][3] == 255 &&
              std::equal(pixel.begin(), pixel.begin() + 3,
                         image.palette[k].begin())) {
            index = k;
            break;
          }
        }
      }
      if (index < 0) {
        if (offenders.size() < kListedOffenders) {
          offenders.push_back(absl::StrFormat("(%d, %d) #%02X%02X%02X a=%d", x,
                                              y, pixel[0], pixel[1], pixel[2],
                                              pixel[3]));
        }
        ++offender_count;
        continue;
      }
      image.indices[static_cast<size_t>(y) * image.width + x] =
          static_cast<uint8_t>(index);
    }
  }
  if (offender_count > 0) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "%zu pixel(s) are not a palette strip color (exact match required; "
        "partly transparent pixels are not allowed): %s%s",
        offender_count, absl::StrJoin(offenders, ", "),
        offender_count > offenders.size() ? ", ..." : ""));
  }
  image.max_index = MaxIndex(image.indices);
  return image;
}

}  // namespace

absl::StatusOr<ImportedSheetImage> ImportSheetImage(const util::PngImage& png) {
  if (png.width <= 0 || png.height <= 0) {
    return absl::InvalidArgumentError("Image is empty");
  }
  const size_t pixels = static_cast<size_t>(png.width) * png.height;
  if (png.indexed) {
    if (png.indices.size() != pixels) {
      return absl::InvalidArgumentError("Indexed image data is truncated");
    }
    return ImportIndexed(png);
  }
  if (png.rgba.size() != pixels * 4) {
    return absl::InvalidArgumentError("RGBA image data is truncated");
  }
  return ImportPaletteStrip(png);
}

absl::StatusOr<ImportedSheetImage> ImportSheetImagePng(
    const std::vector<uint8_t>& png_bytes) {
  auto png = util::DecodePng(png_bytes);
  if (!png.ok()) {
    return png.status();
  }
  return ImportSheetImage(*png);
}

absl::StatusOr<std::vector<uint8_t>> EncodeImportedSheetTiles(
    const ImportedSheetImage& image, int bpp) {
  if (bpp != 2 && bpp != 3 && bpp != 4) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Unsupported tile depth %dbpp", bpp));
  }
  if (image.width % 8 != 0 || image.height % 8 != 0 || image.width == 0 ||
      image.height == 0) {
    return absl::InvalidArgumentError(
        absl::StrFormat("A %dx%d image does not split into 8x8 tiles",
                        image.width, image.height));
  }
  const int limit = 1 << bpp;
  const auto high =
      std::find_if(image.indices.begin(), image.indices.end(),
                   [limit](uint8_t index) { return index >= limit; });
  if (high != image.indices.end()) {
    const size_t at = static_cast<size_t>(high - image.indices.begin());
    return absl::InvalidArgumentError(absl::StrFormat(
        "Index %d at (%zu, %zu) does not fit %dbpp (indices 0-%d)", *high,
        at % image.width, at / image.width, bpp, limit - 1));
  }

  const int tiles_wide = image.width / 8;
  const int tiles_high = image.height / 8;
  std::vector<uint8_t> out;
  out.reserve(static_cast<size_t>(tiles_wide) * tiles_high * bpp * 8);
  for (int ty = 0; ty < tiles_high; ++ty) {
    for (int tx = 0; tx < tiles_wide; ++tx) {
      snes_tile8 tile{};
      for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
          tile.data[row * 8 + col] =
              image.indices[static_cast<size_t>(ty * 8 + row) * image.width +
                            tx * 8 + col];
        }
      }
      const auto packed = gfx::PackBppTile(tile, bpp);
      out.insert(out.end(), packed.begin(), packed.end());
    }
  }
  return out;
}

absl::StatusOr<std::vector<uint8_t>> EncodeImportedSheetPng(
    const ImportedSheetImage& image) {
  if (image.palette.empty() ||
      image.max_index >= static_cast<int>(image.palette.size())) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Image uses index %d but its palette has %zu colors",
                        image.max_index, image.palette.size()));
  }
  return util::EncodeIndexedPng(image.width, image.height, image.indices,
                                image.palette);
}

std::string DescribeImportedSheetImage(const ImportedSheetImage& image) {
  return absl::StrFormat(
      "%dx%d %s, %zu colors%s, highest index %d", image.width, image.height,
      image.source == ImportedImageSource::kIndexedPng ? "indexed PNG"
                                                       : "RGB PNG + strip",
      image.palette.size(), image.strip_removed ? " (strip removed)" : "",
      image.max_index);
}

absl::StatusOr<std::vector<int>> PreviewPaletteSlots(
    std::string_view group_name, size_t palette_size, int row, int count) {
  const auto layout = internal::GetPaletteRowLayout(group_name, palette_size);
  const int rows =
      internal::GetPaletteRowCount(palette_size, layout.colors_per_row);
  if (row < 0 || row >= rows) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Palette row %d is out of range (0-%d)", row, rows - 1));
  }
  const size_t skip = layout.has_explicit_transparent ? 1 : 0;
  const size_t offset = static_cast<size_t>(row) * layout.colors_per_row + skip;
  const size_t available =
      offset >= palette_size
          ? 0
          : std::min<size_t>(7, std::min<size_t>(layout.colors_per_row - skip,
                                                 palette_size - offset));
  if (count < 0 || static_cast<size_t>(count) > available) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Palette row %d holds %zu colors after transparency; the image needs "
        "%d",
        row, available, count));
  }
  std::vector<int> slots;
  for (int i = 0; i < count; ++i) {
    slots.push_back(static_cast<int>(offset) + i);
  }
  return slots;
}

}  // namespace yaze::editor
