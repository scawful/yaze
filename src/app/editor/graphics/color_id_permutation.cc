#include "app/editor/graphics/color_id_permutation.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>

#include "absl/strings/str_format.h"

namespace yaze::editor {
namespace {

constexpr int kPageWidth = 128;
constexpr int kPageHeight = 32;
constexpr size_t kPagePixels = kPageWidth * kPageHeight;
constexpr int kTilesPerRow = kPageWidth / 8;
constexpr int kTilesPerPage = static_cast<int>(kPagePixels / 64);

bool SupportedDepth(int bpp) {
  return bpp == 2 || bpp == 3 || bpp == 4;
}

size_t TileOffset(uint16_t tile) {
  return (tile / kTilesPerRow) * 8 * kPageWidth + (tile % kTilesPerRow) * 8;
}

}  // namespace

ColorIdPermutation IdentityColorIdPermutation(int bpp) {
  ColorIdPermutation permutation;
  permutation.bpp = bpp;
  return permutation;
}

absl::Status ValidateColorIdPermutation(const ColorIdPermutation& permutation) {
  if (!SupportedDepth(permutation.bpp)) {
    return absl::InvalidArgumentError(
        "Color-ID permutations support 2, 3 or 4 bpp");
  }
  if (permutation.ids[0] != 0) {
    return absl::InvalidArgumentError(
        "Color-ID swaps keep ID 0 transparent; replacing transparency is a "
        "separate action");
  }
  const int colors = permutation.colors();
  std::array<bool, 16> seen{};
  for (int i = 0; i < 16; ++i) {
    const uint8_t value = permutation.ids[i];
    if (i >= colors) {
      if (value != i) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Color-ID permutation names ID %d outside %dbpp", i,
                            permutation.bpp));
      }
      continue;
    }
    if (value >= colors) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Color ID %d does not fit %dbpp (IDs 0-%d)", value,
                          permutation.bpp, colors - 1));
    }
    if (seen[value]) {
      return absl::InvalidArgumentError(
          "Color-ID permutation must map each ID exactly once");
    }
    seen[value] = true;
  }
  return absl::OkStatus();
}

absl::Status ValidateColorIdPermutationForBacking(
    const ColorIdPermutation& permutation, int backing_bpp) {
  if (auto status = ValidateColorIdPermutation(permutation); !status.ok()) {
    return status;
  }
  if (permutation.bpp != backing_bpp) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "A %dbpp color-ID permutation cannot edit a %dbpp backing",
        permutation.bpp, backing_bpp));
  }
  return absl::OkStatus();
}

absl::StatusOr<ColorIdPermutation> MakeColorIdSwap(int bpp, int from, int to) {
  if (!SupportedDepth(bpp)) {
    return absl::InvalidArgumentError(
        "Color-ID permutations support 2, 3 or 4 bpp");
  }
  const int colors = 1 << bpp;
  if (from == 0 || to == 0) {
    return absl::InvalidArgumentError(
        "Color-ID swaps keep ID 0 transparent; replacing transparency is a "
        "separate action");
  }
  if (from < 0 || to < 0 || from >= colors || to >= colors) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Swap IDs must be 1-%d for %dbpp", colors - 1, bpp));
  }
  auto permutation = IdentityColorIdPermutation(bpp);
  std::swap(permutation.ids[from], permutation.ids[to]);
  return permutation;
}

uint64_t PhysicalPixelMask(uint64_t displayed_mask, bool hflip, bool vflip) {
  uint64_t mask = 0;
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      if ((displayed_mask & (uint64_t{1} << (y * 8 + x))) == 0) {
        continue;
      }
      const int raw_x = hflip ? 7 - x : x;
      const int raw_y = vflip ? 7 - y : y;
      mask |= uint64_t{1} << (raw_y * 8 + raw_x);
    }
  }
  return mask;
}

absl::StatusOr<ColorIdEditPlan> PlanColorIdEdit(
    const std::vector<ColorIdBacking>& backings,
    const std::vector<ColorIdTarget>& targets,
    const ColorIdPermutation& permutation) {
  if (auto status = ValidateColorIdPermutation(permutation); !status.ok()) {
    return status;
  }
  std::map<uint32_t, const ColorIdBacking*> by_range;
  for (const auto& backing : backings) {
    if (!by_range.emplace(backing.range, &backing).second) {
      return absl::InvalidArgumentError("Duplicate color-ID backing range");
    }
  }
  if (targets.empty()) {
    return absl::InvalidArgumentError("Select pixels to remap");
  }
  std::map<std::pair<uint32_t, uint16_t>, uint64_t> masks;
  for (const auto& target : targets) {
    const auto backing = by_range.find(target.range);
    if (backing == by_range.end()) {
      return absl::NotFoundError("Selected color-ID range is unavailable");
    }
    if (target.tile_index >= kTilesPerPage) {
      return absl::OutOfRangeError("Selected tile is outside its page");
    }
    const uint64_t mask =
        PhysicalPixelMask(target.pixel_mask, target.hflip, target.vflip);
    if (mask != 0) {
      masks[{target.range, target.tile_index}] |= mask;
    }
  }
  if (masks.empty()) {
    return absl::InvalidArgumentError("Select at least one pixel");
  }

  std::set<uint32_t> used_ranges;
  for (const auto& [key, mask] : masks) {
    used_ranges.insert(key.first);
  }
  for (const auto range : used_ranges) {
    const auto& backing = *by_range.at(range);
    if (auto status =
            ValidateColorIdPermutationForBacking(permutation, backing.bpp);
        !status.ok()) {
      return status;
    }
    const int colors = 1 << backing.bpp;
    if (backing.pixels.size() != kPagePixels ||
        std::any_of(backing.pixels.begin(), backing.pixels.end(),
                    [colors](uint8_t pixel) { return pixel >= colors; })) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Backing range %u is not a 128x32 page of %dbpp IDs",
                          range, backing.bpp));
    }
  }

  ColorIdEditPlan plan;
  plan.permutation = permutation;
  std::map<uint32_t, ColorIdRangeEdit> edits;
  for (const auto& [key, mask] : masks) {
    const auto [range, tile] = key;
    const auto& pixels = by_range.at(range)->pixels;
    plan.targets.push_back({range, tile, mask});
    const size_t start = TileOffset(tile);
    for (int y = 0; y < 8; ++y) {
      for (int x = 0; x < 8; ++x) {
        if ((mask & (uint64_t{1} << (y * 8 + x))) == 0) {
          continue;
        }
        ++plan.selected_pixels;
        const size_t offset = start + y * kPageWidth + x;
        const uint8_t before = pixels[offset];
        const uint8_t after = permutation.ids[before];
        if (before == after) {
          continue;
        }
        auto& edit = edits[range];
        edit.range = range;
        edit.offsets.push_back(static_cast<uint16_t>(offset));
        edit.before.push_back(before);
        edit.after.push_back(after);
      }
    }
  }
  plan.physical_tiles = masks.size();
  for (auto& [range, edit] : edits) {
    plan.ranges.push_back(std::move(edit));
  }
  return plan;
}

absl::Status ApplyColorIdRangeEdit(const ColorIdRangeEdit& edit, int bpp,
                                   bool forward, std::vector<uint8_t>& pixels) {
  if (!SupportedDepth(bpp) || pixels.size() != kPagePixels ||
      edit.offsets.empty() || edit.offsets.size() != edit.before.size() ||
      edit.offsets.size() != edit.after.size()) {
    return absl::InvalidArgumentError("Invalid color-ID range edit");
  }
  const int colors = 1 << bpp;
  auto next = pixels;
  std::set<uint16_t> seen;
  for (size_t i = 0; i < edit.offsets.size(); ++i) {
    const auto offset = edit.offsets[i];
    if (offset >= next.size() || !seen.insert(offset).second ||
        edit.before[i] >= colors || edit.after[i] >= colors ||
        edit.before[i] == edit.after[i] || edit.before[i] == 0 ||
        edit.after[i] == 0) {
      return absl::InvalidArgumentError("Invalid color-ID pixel edit");
    }
    const uint8_t expected = forward ? edit.before[i] : edit.after[i];
    if (next[offset] != expected) {
      return absl::FailedPreconditionError(
          "Pixels changed since this color-ID edit");
    }
    next[offset] = forward ? edit.after[i] : edit.before[i];
  }
  pixels = std::move(next);
  return absl::OkStatus();
}

}  // namespace yaze::editor
