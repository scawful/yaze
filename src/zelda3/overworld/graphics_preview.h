#ifndef YAZE_ZELDA3_OVERWORLD_GRAPHICS_PREVIEW_H_
#define YAZE_ZELDA3_OVERWORLD_GRAPHICS_PREVIEW_H_

#include <array>
#include <cstdint>
#include <map>
#include <span>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze::zelda3 {

using PreviewTile8 = std::array<uint8_t, 64>;
using AreaGraphicsPreview = std::map<uint16_t, PreviewTile8>;
using OverworldGraphicsPreview = std::map<int, AreaGraphicsPreview>;

// Decode raw SNES 4bpp DMA bytes into BG character IDs. VRAM addresses are
// words; character data is 32 bytes per tile. No ROM or shared sheet is changed.
inline absl::StatusOr<AreaGraphicsPreview> DecodeGraphicsPreviewTransfer(
    std::span<const uint8_t> bytes, int destination_word, int bg_base_word) {
  if (bg_base_word < 0 || bg_base_word > 0x4000 || destination_word < 0 ||
      destination_word > 0x7FFF) {
    return absl::InvalidArgumentError("Preview VRAM word address out of range");
  }
  const int offset = (destination_word - bg_base_word) * 2;
  if (offset < 0 || offset % 32 != 0 || bytes.empty() ||
      bytes.size() % 32 != 0 || bytes.size() > 0x8000 ||
      static_cast<size_t>(offset) + bytes.size() > 0x8000 ||
      destination_word * 2 + bytes.size() > 0x10000) {
    return absl::InvalidArgumentError(
        "Preview DMA must contain aligned 4bpp BG characters within VRAM");
  }
  AreaGraphicsPreview result;
  for (size_t tile = 0; tile < bytes.size() / 32; ++tile) {
    PreviewTile8 pixels{};
    for (int y = 0; y < 8; ++y) {
      for (int x = 0; x < 8; ++x) {
        for (int plane = 0; plane < 4; ++plane) {
          const size_t source =
              tile * 32 + (plane / 2) * 16 + y * 2 + plane % 2;
          pixels[y * 8 + x] |= ((bytes[source] >> (7 - x)) & 1) << plane;
        }
      }
    }
    result.emplace(static_cast<uint16_t>(offset / 32 + tile), pixels);
  }
  return result;
}

inline void ApplyGraphicsPreview(const AreaGraphicsPreview& preview,
                                 std::vector<uint8_t>& graphics) {
  constexpr int kWidth = 128;
  for (const auto& [id, pixels] : preview) {
    const size_t origin = (id / 16 * 8) * kWidth + (id % 16) * 8;
    if (origin + 7 * kWidth + 8 > graphics.size())
      continue;
    for (int y = 0; y < 8; ++y) {
      for (int x = 0; x < 8; ++x)
        graphics[origin + y * kWidth + x] = pixels[y * 8 + x];
    }
  }
}

}  // namespace yaze::zelda3
#endif
