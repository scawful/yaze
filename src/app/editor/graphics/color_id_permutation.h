#ifndef YAZE_APP_EDITOR_GRAPHICS_COLOR_ID_PERMUTATION_H_
#define YAZE_APP_EDITOR_GRAPHICS_COLOR_ID_PERMUTATION_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"

namespace yaze::editor {

// Whole-selection color-ID map for one SNES planar depth (2, 3 or 4 bpp).
// ids[i] is the new ID of original ID i. Entries at or past 2^bpp must stay
// identity so a permutation can never name an ID its backing cannot store.
struct ColorIdPermutation {
  int bpp = 3;
  std::array<uint8_t, 16> ids{0, 1, 2,  3,  4,  5,  6,  7,
                              8, 9, 10, 11, 12, 13, 14, 15};

  int colors() const { return 1 << bpp; }
};

ColorIdPermutation IdentityColorIdPermutation(int bpp);

// Bijection on 0..2^bpp-1 that keeps ID 0 fixed. Changing transparency is a
// separate explicit replacement and is always refused here.
absl::Status ValidateColorIdPermutation(const ColorIdPermutation& permutation);

// As above, and the permutation depth must equal the backing's depth.
absl::Status ValidateColorIdPermutationForBacking(
    const ColorIdPermutation& permutation, int backing_bpp);

// Swaps two nonzero IDs inside the depth.
absl::StatusOr<ColorIdPermutation> MakeColorIdSwap(int bpp, int from, int to);

// Maps a mask over a displayed (possibly flipped) 8x8 tile to the physical,
// unflipped tile. Bit y * 8 + x selects a pixel.
uint64_t PhysicalPixelMask(uint64_t displayed_mask, bool hflip, bool vflip);

// One row-major 128x32 indexed page and its declared planar depth.
struct ColorIdBacking {
  uint32_t range = 0;
  int bpp = 3;
  absl::Span<const uint8_t> pixels;
};

struct ColorIdTarget {
  uint32_t range = 0;
  uint16_t tile_index = 0;
  uint64_t pixel_mask = ~uint64_t{0};
  bool hflip = false;
  bool vflip = false;
};

struct ColorIdRangeEdit {
  uint32_t range = 0;
  std::vector<uint16_t> offsets;
  std::vector<uint8_t> before;
  std::vector<uint8_t> after;
};

// Targets are canonical physical masks (H/V false), one per physical tile.
// ranges lists only ranges whose pixels change.
struct ColorIdEditPlan {
  ColorIdPermutation permutation;
  std::vector<ColorIdTarget> targets;
  std::vector<ColorIdRangeEdit> ranges;
  size_t physical_tiles = 0;
  size_t selected_pixels = 0;
};

// Pure planning across any number of backings. Every backing must match the
// permutation depth and hold only IDs below 2^bpp. Aliases of one physical
// tile are unioned first, so each original pixel is mapped exactly once.
absl::StatusOr<ColorIdEditPlan> PlanColorIdEdit(
    const std::vector<ColorIdBacking>& backings,
    const std::vector<ColorIdTarget>& targets,
    const ColorIdPermutation& permutation);

// Writes one range edit (forward = after) into a copy of its page. Refuses
// when any affected pixel differs from the expected prior value.
absl::Status ApplyColorIdRangeEdit(const ColorIdRangeEdit& edit, int bpp,
                                   bool forward, std::vector<uint8_t>& pixels);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_GRAPHICS_COLOR_ID_PERMUTATION_H_
