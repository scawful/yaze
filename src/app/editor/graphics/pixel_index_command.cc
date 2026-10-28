#include "app/editor/graphics/pixel_index_command.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>

#include "app/editor/graphics/graphics_editor_state.h"
#include "app/editor/registry/undo_manager.h"
#include "util/macro.h"
#include "zelda3/game_data.h"
#include "zelda3/gfx_sheet_storage.h"
#include "zelda3/graphics_sheet_store.h"

namespace yaze::editor {
namespace {

using TileKey = std::pair<uint16_t, uint16_t>;
using PixelOffsets = std::map<uint16_t, std::set<uint16_t>>;
using SourceCellKey = std::tuple<int, int, size_t, bool, bool>;
constexpr int kSheetWidth = zelda3::GraphicsSheetStore::kSheetWidth;
constexpr int kSheetHeight = zelda3::GraphicsSheetStore::kSheetHeight;
constexpr int kTilesPerRow = kSheetWidth / 8;
constexpr int kTilesPerSheet = kSheetWidth * kSheetHeight / 64;

absl::Status ValidatePermutation(const PixelIndexPermutation& permutation) {
  std::array<bool, 8> seen{};
  for (const auto value : permutation) {
    if (value >= seen.size() || seen[value]) {
      return absl::InvalidArgumentError(
          "Pixel permutation must be a bijection on 0..7 with zero fixed");
    }
    seen[value] = true;
  }
  if (permutation[0] != 0) {
    return absl::InvalidArgumentError(
        "Pixel permutation must keep transparent index zero fixed");
  }
  return absl::OkStatus();
}

uint64_t PhysicalMask(const PixelIndexTarget& target) {
  return PhysicalPixelMask(target.pixel_mask, target.hflip, target.vflip);
}

bool IsRaw3bpp(absl::Span<const uint8_t> pixels) {
  return pixels.size() == zelda3::GraphicsSheetStore::kSheetBytes &&
         std::all_of(pixels.begin(), pixels.end(),
                     [](uint8_t value) { return value <= 7; });
}

void RefreshSelectionPixels(GraphicsEditorState& state) {
  auto& selection = state.selection;
  const auto* store = ActiveSheetStore(state);
  if (!selection.is_active || selection.is_floating || !store ||
      !store->HasSheet(state.current_sheet_id) || selection.width <= 0 ||
      selection.height <= 0 || selection.width > kSheetWidth ||
      selection.height > kSheetHeight || selection.x < 0 || selection.y < 0 ||
      selection.x > kSheetWidth - selection.width ||
      selection.y > kSheetHeight - selection.height) {
    return;
  }
  const auto pixels = store->Sheet(state.current_sheet_id);
  selection.pixel_data.resize(selection.width * selection.height);
  for (int y = 0; y < selection.height; ++y) {
    for (int x = 0; x < selection.width; ++x) {
      selection.pixel_data[y * selection.width + x] =
          pixels[(selection.y + y) * kSheetWidth + selection.x + x];
    }
  }
}

absl::StatusOr<PixelOffsets> SelectionOffsets(const PixelIndexPlan& plan) {
  if (plan.targets.empty() || plan.sheet_revisions.empty() ||
      plan.sheet_revisions.size() != plan.before_sheets.size()) {
    return absl::InvalidArgumentError("Pixel edit plan has no valid selection");
  }
  std::set<TileKey> target_keys;
  PixelOffsets selected_offsets;
  for (const auto& target : plan.targets) {
    const auto snapshot = plan.before_sheets.find(target.sheet_id);
    if (target.sheet_id >= zelda3::GraphicsSheetStore::kSheetCount ||
        target.tile_index >= kTilesPerSheet || target.pixel_mask == 0 ||
        target.hflip || target.vflip ||
        !plan.sheet_revisions.contains(target.sheet_id) ||
        snapshot == plan.before_sheets.end() || !IsRaw3bpp(snapshot->second) ||
        !target_keys.emplace(target.sheet_id, target.tile_index).second ||
        zelda3::GetGfxSheetStorageKind(target.sheet_id) ==
            zelda3::GfxSheetStorageKind::kCompressed2bpp) {
      return absl::InvalidArgumentError(
          "Invalid physical pixel edit selection");
    }
    const int start = (target.tile_index / kTilesPerRow) * 8 * kSheetWidth +
                      (target.tile_index % kTilesPerRow) * 8;
    for (int y = 0; y < 8; ++y) {
      for (int x = 0; x < 8; ++x) {
        if ((target.pixel_mask & (uint64_t{1} << (y * 8 + x))) != 0) {
          selected_offsets[target.sheet_id].insert(start + y * kSheetWidth + x);
        }
      }
    }
  }
  return selected_offsets;
}

absl::StatusOr<std::vector<SourceCellKey>> SelectedSourceCells(
    const GraphicsEditContext& source, const std::set<TileKey>& selected) {
  std::vector<SourceCellKey> cells;
  for (const auto& cell : source.pattern_cells) {
    if (cell.target >= source.targets.size()) {
      return absl::FailedPreconditionError(
          "Graphics source pattern has an unavailable tile target");
    }
    const auto& target = source.targets[cell.target];
    if (selected.contains({target.sheet_id, target.tile_index})) {
      cells.emplace_back(cell.x, cell.y, cell.target, cell.hflip, cell.vflip);
    }
  }
  std::sort(cells.begin(), cells.end());
  return cells;
}

absl::Status CheckLiveSource(const PixelIndexPlan& plan) {
  if (!plan.source) {
    if (!plan.source_targets.empty()) {
      return absl::InvalidArgumentError("Pixel edit source guard is missing");
    }
    return absl::OkStatus();
  }
  if (plan.source->owner != plan.owner || plan.source_targets.empty() ||
      !plan.source->resolve_live_source) {
    return absl::FailedPreconditionError(
        "This graphics source cannot resolve live pixel assignments");
  }
  GraphicsEditContext fresh;
  RETURN_IF_ERROR(plan.source->resolve_live_source(fresh));
  if (fresh.owner != plan.owner ||
      fresh.resolved.surface != plan.source->resolved.surface ||
      fresh.resolved.id != plan.source->resolved.id ||
      fresh.resolved.parent_id != plan.source->resolved.parent_id) {
    return absl::FailedPreconditionError(
        "Pixel edit source session or area changed");
  }
  std::set<TileKey> selected;
  for (const auto& target : plan.targets) {
    selected.emplace(target.sheet_id, target.tile_index);
  }
  std::vector<size_t> fresh_targets;
  for (size_t i = 0; i < fresh.targets.size(); ++i) {
    const auto& target = fresh.targets[i];
    if (selected.contains({target.sheet_id, target.tile_index})) {
      fresh_targets.push_back(i);
    }
  }
  if (fresh_targets != plan.source_targets) {
    return absl::FailedPreconditionError(
        "Selected graphics source tile aliases changed");
  }
  for (const auto index : plan.source_targets) {
    if (index >= plan.source->targets.size() || index >= fresh.targets.size()) {
      return absl::FailedPreconditionError("Selected graphics source changed");
    }
    const auto& old = plan.source->targets[index];
    const auto& now = fresh.targets[index];
    if (old.sheet_id != now.sheet_id || old.tile_index != now.tile_index ||
        old.source_orientation_mask != now.source_orientation_mask) {
      return absl::FailedPreconditionError(
          "Source tile assignments changed; reopen Graphics from the source");
    }
  }
  ASSIGN_OR_RETURN(const auto old_cells,
                   SelectedSourceCells(*plan.source, selected));
  ASSIGN_OR_RETURN(const auto new_cells, SelectedSourceCells(fresh, selected));
  if (fresh.pattern_width != plan.source->pattern_width ||
      fresh.pattern_height != plan.source->pattern_height ||
      old_cells != new_cells) {
    return absl::FailedPreconditionError(
        "Source pattern positions or orientations changed; reopen Graphics "
        "from the source");
  }
  return absl::OkStatus();
}

absl::Status CaptureSourceGuard(const GraphicsEditorState& state,
                                PixelIndexPlan& plan) {
  if (!state.source_context) {
    return absl::OkStatus();
  }
  if (state.source_context->owner != plan.owner) {
    return absl::FailedPreconditionError("Graphics source owner changed");
  }
  std::set<TileKey> selected;
  for (const auto& target : plan.targets) {
    selected.emplace(target.sheet_id, target.tile_index);
  }
  for (size_t i = 0; i < state.source_context->targets.size(); ++i) {
    const auto& target = state.source_context->targets[i];
    if (selected.contains({target.sheet_id, target.tile_index})) {
      plan.source_targets.push_back(i);
    }
  }
  if (!plan.source_targets.empty()) {
    plan.source = state.source_context;
    RETURN_IF_ERROR(CheckLiveSource(plan));
  }
  return absl::OkStatus();
}

class PixelIndexAction final : public UndoAction {
 public:
  PixelIndexAction(GraphicsEditorState& state, const PixelIndexPlan& plan)
      : state_(&state), plan_(plan) {}

  absl::Status Undo() override {
    return ExecutePixelIndexPlan(*state_, plan_, PixelIndexExecution::kUndo);
  }
  absl::Status Redo() override {
    return ExecutePixelIndexPlan(*state_, plan_, PixelIndexExecution::kRedo);
  }
  std::string Description() const override { return plan_.description; }
  size_t MemoryUsage() const override {
    size_t size = plan_.targets.size() * sizeof(PixelIndexTarget) +
                  plan_.description.size();
    for (const auto& [sheet, pixels] : plan_.before_sheets) {
      size += pixels.size();
    }
    for (const auto& diff : plan_.sheets) {
      size += diff.MemoryUsage();
    }
    return size;
  }

 private:
  GraphicsEditorState* state_;
  PixelIndexPlan plan_;
};

}  // namespace

absl::StatusOr<std::vector<PixelIndexTarget>> MakePixelIndexRectangleSelection(
    uint16_t sheet_id, int x, int y, int width, int height) {
  if (sheet_id >= zelda3::GraphicsSheetStore::kSheetCount || width <= 0 ||
      height <= 0 || width > kSheetWidth || height > kSheetHeight || x < 0 ||
      y < 0 || x > kSheetWidth - width || y > kSheetHeight - height) {
    return absl::InvalidArgumentError(
        "Pixel selection must fit within one loaded 128x32 sheet");
  }
  std::map<uint16_t, uint64_t> masks;
  for (int yy = y; yy < y + height; ++yy) {
    for (int xx = x; xx < x + width; ++xx) {
      const uint16_t tile = (yy / 8) * kTilesPerRow + xx / 8;
      masks[tile] |= uint64_t{1} << ((yy % 8) * 8 + xx % 8);
    }
  }
  std::vector<PixelIndexTarget> targets;
  for (const auto& [tile, mask] : masks) {
    targets.push_back({sheet_id, tile, mask});
  }
  return targets;
}

absl::StatusOr<PixelIndexPlan> PlanPixelIndexPermutation(
    const GraphicsEditorState& state,
    const std::vector<PixelIndexTarget>& targets,
    const PixelIndexPermutation& permutation) {
  RETURN_IF_ERROR(ValidatePermutation(permutation));
  const auto* store = ActiveSheetStore(state);
  if (!store || state.selection.is_floating) {
    return absl::FailedPreconditionError(
        "Load this session's graphics and finish the floating paste first");
  }
  if (targets.empty()) {
    return absl::InvalidArgumentError("Select tile pixels to remap");
  }

  PixelIndexPlan plan;
  plan.owner = state.game_data;
  plan.source_context_revision = state.source_context_revision;
  std::map<TileKey, uint64_t> masks;
  for (const auto& target : targets) {
    if (target.tile_index >= kTilesPerSheet ||
        !store->HasSheet(target.sheet_id)) {
      return absl::OutOfRangeError("Selected graphics tile is unavailable");
    }
    if (zelda3::GetGfxSheetStorageKind(target.sheet_id) ==
        zelda3::GfxSheetStorageKind::kCompressed2bpp) {
      return absl::UnimplementedError(
          "Pixel index permutations require a 3bpp graphics sheet");
    }
    const uint64_t mask = PhysicalMask(target);
    if (mask != 0) {
      masks[{target.sheet_id, target.tile_index}] |= mask;
    }
  }
  if (masks.empty()) {
    return absl::InvalidArgumentError("Select at least one tile pixel");
  }

  std::map<uint16_t, std::vector<uint8_t>> next;
  for (const auto& [key, mask] : masks) {
    const auto [sheet, tile] = key;
    if (!plan.before_sheets.contains(sheet)) {
      const auto pixels = store->Sheet(sheet);
      if (!IsRaw3bpp(pixels)) {
        return absl::FailedPreconditionError(
            "Selected graphics sheet is not loaded as raw 3bpp indices");
      }
      plan.sheet_revisions[sheet] = store->Revision(sheet);
      plan.before_sheets[sheet] =
          std::vector<uint8_t>(pixels.begin(), pixels.end());
      next[sheet] = plan.before_sheets[sheet];
    }
    plan.targets.push_back({sheet, tile, mask});
    const int start =
        (tile / kTilesPerRow) * 8 * kSheetWidth + (tile % kTilesPerRow) * 8;
    for (int y = 0; y < 8; ++y) {
      for (int x = 0; x < 8; ++x) {
        if ((mask & (uint64_t{1} << (y * 8 + x))) != 0) {
          auto& pixel = next[sheet][start + y * kSheetWidth + x];
          pixel = permutation[pixel];
          ++plan.selected_pixels;
        }
      }
    }
  }
  plan.physical_tiles = masks.size();
  RETURN_IF_ERROR(CaptureSourceGuard(state, plan));
  for (const auto& [sheet, pixels] : next) {
    auto diff = MakeSheetPixelDiff(sheet, plan.before_sheets.at(sheet), pixels);
    if (!diff.empty()) {
      plan.sheets.push_back(std::move(diff));
    }
  }
  return plan;
}

absl::StatusOr<PixelIndexPlan> PlanPixelIndexReplacement(
    const GraphicsEditorState& state,
    const std::vector<PixelIndexTarget>& targets,
    const std::map<uint16_t, std::vector<uint8_t>>& after_sheets,
    std::string description) {
  constexpr PixelIndexPermutation identity{0, 1, 2, 3, 4, 5, 6, 7};
  ASSIGN_OR_RETURN(auto plan,
                   PlanPixelIndexPermutation(state, targets, identity));
  if (after_sheets.size() != plan.before_sheets.size()) {
    return absl::InvalidArgumentError(
        "Artwork replacement needs exactly the selected graphics sheets");
  }
  ASSIGN_OR_RETURN(const auto selected_offsets, SelectionOffsets(plan));
  for (const auto& [sheet, before] : plan.before_sheets) {
    const auto after = after_sheets.find(sheet);
    if (after == after_sheets.end() || !IsRaw3bpp(after->second)) {
      return absl::InvalidArgumentError(
          "Artwork replacement needs full 4096-byte sheets with indices 0..7");
    }
    auto diff = MakeSheetPixelDiff(sheet, before, after->second);
    for (const auto offset : diff.offsets) {
      if (!selected_offsets.at(sheet).contains(offset)) {
        return absl::InvalidArgumentError(
            "Artwork replacement changes pixels outside the selected mask");
      }
    }
    if (!diff.empty()) {
      plan.sheets.push_back(std::move(diff));
    }
  }
  plan.operation = PixelIndexOperation::kArtReplacement;
  plan.keep_transparency = false;
  if (!description.empty()) {
    plan.description = std::move(description);
  } else {
    plan.description = "Apply art to object source";
  }
  return plan;
}

absl::Status ExecutePixelIndexPlan(
    GraphicsEditorState& state, const PixelIndexPlan& plan,
    PixelIndexExecution execution,
    const std::function<absl::Status(size_t)>& after_sheet_write) {
  auto* store = ActiveSheetStore(state);
  if (!store || !plan.owner || state.game_data != plan.owner ||
      state.selection.is_floating) {
    return absl::FailedPreconditionError(
        "Pixel edit session changed or a floating paste is pending");
  }
  if (execution != PixelIndexExecution::kApply &&
      execution != PixelIndexExecution::kUndo &&
      execution != PixelIndexExecution::kRedo) {
    return absl::InvalidArgumentError("Invalid pixel edit execution mode");
  }
  if ((plan.operation != PixelIndexOperation::kPermutation &&
       plan.operation != PixelIndexOperation::kArtReplacement) ||
      (plan.operation == PixelIndexOperation::kPermutation &&
       !plan.keep_transparency)) {
    return absl::InvalidArgumentError("Invalid pixel edit operation");
  }
  ASSIGN_OR_RETURN(const auto selected_offsets, SelectionOffsets(plan));
  RETURN_IF_ERROR(CheckLiveSource(plan));
  if (execution == PixelIndexExecution::kApply) {
    if (state.source_context_revision != plan.source_context_revision) {
      return absl::FailedPreconditionError(
          "Graphics source changed since the pixel edit was planned");
    }
    for (const auto& [sheet, revision] : plan.sheet_revisions) {
      const auto snapshot = plan.before_sheets.find(sheet);
      const auto pixels = store->Sheet(sheet);
      if (snapshot == plan.before_sheets.end() ||
          !IsRaw3bpp(snapshot->second)) {
        return absl::InvalidArgumentError("Invalid pixel edit sheet snapshot");
      }
      if (!store->HasSheet(sheet) || store->Revision(sheet) != revision ||
          !std::equal(pixels.begin(), pixels.end(), snapshot->second.begin(),
                      snapshot->second.end())) {
        return absl::FailedPreconditionError(
            "Graphics pixels changed since the pixel edit was planned");
      }
    }
  }

  const bool redo = execution != PixelIndexExecution::kUndo;
  std::map<uint16_t, std::vector<uint8_t>> next;
  for (const auto& diff : plan.sheets) {
    if (!store->HasSheet(diff.sheet) ||
        !plan.sheet_revisions.contains(diff.sheet) ||
        !plan.before_sheets.contains(diff.sheet) || diff.offsets.empty() ||
        diff.offsets.size() != diff.before.size() ||
        diff.offsets.size() != diff.after.size() || next.contains(diff.sheet)) {
      return absl::InvalidArgumentError("Invalid pixel edit sheet diff");
    }
    const auto current = store->Sheet(diff.sheet);
    if (!IsRaw3bpp(current)) {
      return absl::FailedPreconditionError(
          "Graphics sheet no longer contains raw 3bpp indices");
    }
    auto& pixels = next[diff.sheet];
    pixels.assign(current.begin(), current.end());
    std::set<uint16_t> offsets;
    for (size_t i = 0; i < diff.offsets.size(); ++i) {
      const auto offset = diff.offsets[i];
      if (offset >= pixels.size() || !offsets.insert(offset).second ||
          !selected_offsets.contains(diff.sheet) ||
          !selected_offsets.at(diff.sheet).contains(offset) ||
          diff.before[i] > 7 || diff.after[i] > 7 ||
          diff.before[i] == diff.after[i] ||
          (plan.keep_transparency &&
           (diff.before[i] == 0 || diff.after[i] == 0)) ||
          diff.before[i] != plan.before_sheets.at(diff.sheet)[offset]) {
        return absl::InvalidArgumentError("Invalid pixel edit pixel diff");
      }
      if (pixels[offset] != (redo ? diff.before[i] : diff.after[i])) {
        return absl::FailedPreconditionError(
            "Edited pixels changed since this history action");
      }
      pixels[offset] = redo ? diff.after[i] : diff.before[i];
    }
  }

  const auto before = *store;
  size_t written = 0;
  for (const auto& [sheet, pixels] : next) {
    auto status = store->WriteSheet(sheet, pixels, 7);
    if (status.ok() && after_sheet_write) {
      status = after_sheet_write(++written);
    }
    if (!status.ok()) {
      *store = before;
      return status;
    }
  }
  if (next.contains(state.current_sheet_id)) {
    RefreshSelectionPixels(state);
  }
  for (const auto& [sheet, pixels] : next) {
    RefreshArenaSheet(state, sheet);
    state.MarkSheetModified(sheet);
  }
  return absl::OkStatus();
}

absl::Status ApplyPixelIndexPlan(GraphicsEditorState& state,
                                 UndoManager& history,
                                 const PixelIndexPlan& plan) {
  RETURN_IF_ERROR(
      ExecutePixelIndexPlan(state, plan, PixelIndexExecution::kApply));
  if (!plan.sheets.empty()) {
    history.Push(std::make_unique<PixelIndexAction>(state, plan));
  }
  return absl::OkStatus();
}

absl::Status ApplyPixelIndexPermutation(
    GraphicsEditorState& state, UndoManager& history,
    const std::vector<PixelIndexTarget>& targets,
    const PixelIndexPermutation& permutation) {
  ASSIGN_OR_RETURN(const auto plan,
                   PlanPixelIndexPermutation(state, targets, permutation));
  return ApplyPixelIndexPlan(state, history, plan);
}

absl::StatusOr<PixelIndexPlan> PlanColorIdPermutation(
    const GraphicsEditorState& state,
    const std::vector<PixelIndexTarget>& targets,
    const ColorIdPermutation& permutation) {
  for (const auto& target : targets) {
    if (target.sheet_id < zelda3::GraphicsSheetStore::kSheetCount &&
        zelda3::GetGfxSheetStorageKind(target.sheet_id) ==
            zelda3::GfxSheetStorageKind::kCompressed2bpp) {
      return absl::FailedPreconditionError(
          "2bpp ROM sheets are read-only; swap IDs in a 2bpp scratch BIN");
    }
  }
  RETURN_IF_ERROR(ValidateColorIdPermutationForBacking(permutation, 3));
  PixelIndexPermutation legacy;
  std::copy_n(permutation.ids.begin(), legacy.size(), legacy.begin());
  return PlanPixelIndexPermutation(state, targets, legacy);
}

absl::Status ApplyColorIdPermutation(
    GraphicsEditorState& state, UndoManager& history,
    const std::vector<PixelIndexTarget>& targets,
    const ColorIdPermutation& permutation) {
  ASSIGN_OR_RETURN(const auto plan,
                   PlanColorIdPermutation(state, targets, permutation));
  return ApplyPixelIndexPlan(state, history, plan);
}

}  // namespace yaze::editor
