#include "app/editor/graphics/scratch_color_id_command.h"

#include <map>
#include <set>
#include <utility>

#include "util/macro.h"

namespace yaze::editor {
namespace {

template <typename Library>
auto* FindAsset(Library& library, uint64_t id) {
  decltype(&library.assets.front()) found = nullptr;
  for (auto& asset : library.assets) {
    if (asset.id == id) {
      found = &asset;
      break;
    }
  }
  return found;
}

}  // namespace

std::vector<ScratchColorIdTarget> WholeScratchAssetSelection(
    uint64_t asset_id) {
  std::vector<ScratchColorIdTarget> targets;
  for (uint16_t tile = 0; tile < 64; ++tile) {
    targets.push_back({asset_id, tile});
  }
  return targets;
}

absl::StatusOr<ScratchColorIdPlan> PlanScratchColorIdPermutation(
    const GraphicsScratchLibrary& library,
    const std::vector<ScratchColorIdTarget>& targets,
    const ColorIdPermutation& permutation) {
  ScratchColorIdPlan plan;
  std::map<uint64_t, uint32_t> ranges;
  std::vector<ColorIdBacking> backings;
  std::vector<ColorIdTarget> edit_targets;
  for (const auto& target : targets) {
    auto [range, inserted] = ranges.emplace(
        target.asset_id, static_cast<uint32_t>(plan.asset_ids.size()));
    if (inserted) {
      const auto* asset = FindAsset(library, target.asset_id);
      if (!asset) {
        return absl::NotFoundError("Selected scratch asset is unavailable");
      }
      RETURN_IF_ERROR(asset->Validate());
      plan.asset_ids.push_back(asset->id);
      backings.push_back({range->second, asset->bpp, asset->pixels});
    }
    edit_targets.push_back({range->second, target.tile_index, target.pixel_mask,
                            target.hflip, target.vflip});
  }
  ASSIGN_OR_RETURN(plan.edit,
                   PlanColorIdEdit(backings, edit_targets, permutation));
  return plan;
}

absl::Status ApplyScratchColorIdPlan(GraphicsScratchLibrary& library,
                                     const ScratchColorIdPlan& plan) {
  std::vector<std::pair<GraphicsScratchAsset*, std::vector<uint8_t>>> next;
  std::set<uint32_t> seen;
  for (const auto& edit : plan.edit.ranges) {
    if (edit.range >= plan.asset_ids.size() ||
        !seen.insert(edit.range).second) {
      return absl::InvalidArgumentError("Invalid scratch color-ID range");
    }
    auto* asset = FindAsset(library, plan.asset_ids[edit.range]);
    if (!asset) {
      return absl::FailedPreconditionError(
          "Scratch asset was removed after planning");
    }
    if (asset->bpp != plan.edit.permutation.bpp) {
      return absl::FailedPreconditionError(
          "Scratch asset depth changed after planning");
    }
    auto pixels = asset->pixels;
    RETURN_IF_ERROR(ApplyColorIdRangeEdit(edit, asset->bpp, true, pixels));
    next.emplace_back(asset, std::move(pixels));
  }
  for (auto& [asset, pixels] : next) {
    asset->pixels = std::move(pixels);
  }
  return absl::OkStatus();
}

}  // namespace yaze::editor
