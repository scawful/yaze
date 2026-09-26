#ifndef YAZE_UTIL_INDEXED_PNG_H
#define YAZE_UTIL_INDEXED_PNG_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/statusor.h"

namespace yaze::util {

/**
 * @brief A decoded PNG: palette indices for indexed files, RGBA otherwise.
 *
 * Indexed PNGs (color type 3, bit depth 1/2/4/8) keep their palette and
 * per-pixel indices, so art drawn with an 8-color palette round-trips index
 * for index. 8-bit grayscale, gray+alpha, RGB and RGBA PNGs decode to RGBA.
 * 16-bit and interlaced PNGs are rejected.
 */
struct PngImage {
  int width = 0;
  int height = 0;
  bool indexed = false;
  // indexed: one palette index per pixel.
  std::vector<uint8_t> indices;
  // indexed: PLTE entries with tRNS alpha (255 when absent).
  std::vector<std::array<uint8_t, 4>> palette;
  // not indexed: 4 bytes (RGBA) per pixel.
  std::vector<uint8_t> rgba;
};

/// Encodes an 8-bit indexed PNG. `palette` holds 1-256 RGBA entries; a tRNS
/// chunk is written when any entry has alpha below 255.
absl::StatusOr<std::vector<uint8_t>> EncodeIndexedPng(
    int width, int height, const std::vector<uint8_t>& indices,
    const std::vector<std::array<uint8_t, 4>>& palette);

absl::StatusOr<PngImage> DecodePng(const std::vector<uint8_t>& bytes);

absl::StatusOr<std::vector<uint8_t>> ReadBinaryFile(const std::string& path);
absl::Status WriteBinaryFile(const std::string& path,
                             const std::vector<uint8_t>& bytes);

}  // namespace yaze::util

#endif  // YAZE_UTIL_INDEXED_PNG_H
