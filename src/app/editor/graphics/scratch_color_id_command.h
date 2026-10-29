#ifndef YAZE_APP_EDITOR_GRAPHICS_SCRATCH_COLOR_ID_COMMAND_H_
#define YAZE_APP_EDITOR_GRAPHICS_SCRATCH_COLOR_ID_COMMAND_H_

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/graphics/color_id_permutation.h"
#include "app/editor/graphics/graphics_scratch_asset.h"

namespace yaze::editor {

// Scratch assets are the writable 2/3/4bpp backing for detached and BIN-bound
// pages; each asset is one range keyed by its library ID.
struct ScratchColorIdTarget {
  uint64_t asset_id = 0;
  uint16_t tile_index = 0;
  uint64_t pixel_mask = ~uint64_t{0};
  bool hflip = false;
  bool vflip = false;
};

struct ScratchColorIdPlan {
  ColorIdEditPlan edit;
  // edit range i addresses asset_ids[i].
  std::vector<uint64_t> asset_ids;
};

std::vector<ScratchColorIdTarget> WholeScratchAssetSelection(uint64_t asset_id);

absl::StatusOr<ScratchColorIdPlan> PlanScratchColorIdPermutation(
    const GraphicsScratchLibrary& library,
    const std::vector<ScratchColorIdTarget>& targets,
    const ColorIdPermutation& permutation);

// All affected assets change or none do. Assets must still hold the planned
// prior pixels and depth.
absl::Status ApplyScratchColorIdPlan(GraphicsScratchLibrary& library,
                                     const ScratchColorIdPlan& plan);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_GRAPHICS_SCRATCH_COLOR_ID_COMMAND_H_
