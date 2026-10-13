#include "app/editor/graphics/palette_pixel_command.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <tuple>

#include "app/editor/graphics/graphics_editor_state.h"
#include "app/editor/registry/undo_manager.h"
#include "util/macro.h"
#include "zelda3/game_data.h"
#include "zelda3/gfx_sheet_storage.h"

namespace yaze::editor {
namespace {
using AddressKey = std::tuple<std::string, int, int>;
AddressKey Key(const zelda3::PaletteColorAddress& a) {
  return {zelda3::PaletteAddressGroupName(a.group), a.palette_index,
          a.color_index};
}
AddressKey Key(const gfx::PaletteColorChange& c) {
  return {c.group_name, c.palette_index, c.color_index};
}

absl::Status CheckLiveSource(const PalettePixelPlan& plan, bool redo) {
  if (plan.targets.empty())
    return absl::InvalidArgumentError(
        "Palette/pixel plan has no selected targets");
  if (!plan.source.resolve_live_source) {
    return absl::UnimplementedError(
        "This source has no authoritative live palette/pixel resolver");
  }
  GraphicsEditContext fresh;
  RETURN_IF_ERROR(plan.source.resolve_live_source(fresh));
  if (fresh.owner != plan.source.owner ||
      fresh.resolved.surface != plan.source.resolved.surface ||
      fresh.resolved.id != plan.source.resolved.id ||
      fresh.resolved.parent_id != plan.source.resolved.parent_id) {
    return absl::FailedPreconditionError(
        "Palette/pixel source session or area changed");
  }
  for (const auto index : plan.targets) {
    if (index >= fresh.targets.size() || index >= plan.source.targets.size())
      return absl::FailedPreconditionError("Selected source target changed");
    const auto& old = plan.source.targets[index];
    const auto& now = fresh.targets[index];
    RETURN_IF_ERROR(ValidateGraphicsPaletteAddressing(now, fresh.owner));
    if (!old.writable_palette || old.palette.size() != 8 ||
        old.sheet_id != now.sheet_id || old.tile_index != now.tile_index ||
        old.source_orientation_mask != now.source_orientation_mask ||
        old.writable_palette->row != now.writable_palette->row ||
        old.writable_palette->half_offset !=
            now.writable_palette->half_offset ||
        old.writable_palette->colors != now.writable_palette->colors) {
      return absl::FailedPreconditionError(
          "Source assignments changed; reopen Graphics from the source");
    }
    for (int raw = 0; raw < 8; ++raw) {
      auto expected = old.palette[raw];
      if (!redo && raw != 0) {
        const auto key = Key(*old.writable_palette->colors[raw]);
        for (const auto& change : plan.colors)
          if (Key(change) == key)
            expected = change.new_color;
      }
      if (now.palette[raw].snes() != expected.snes())
        return absl::FailedPreconditionError(
            "Source colors changed since planning");
    }
  }
  return absl::OkStatus();
}

class PalettePixelAction final : public UndoAction {
 public:
  PalettePixelAction(GraphicsEditorState& state, PalettePixelPlan plan)
      : state_(&state), plan_(std::move(plan)) {}
  absl::Status Undo() override {
    return ExecutePalettePixelPlan(*state_, plan_, false);
  }
  absl::Status Redo() override {
    return ExecutePalettePixelPlan(*state_, plan_, true);
  }
  std::string Description() const override {
    return "Reorder palette and selected pixels";
  }
  size_t MemoryUsage() const override {
    size_t size = plan_.colors.size() * sizeof(gfx::PaletteColorChange);
    for (const auto& diff : plan_.sheets)
      size += diff.MemoryUsage();
    return size;
  }

 private:
  GraphicsEditorState* state_;
  PalettePixelPlan plan_;
};
}  // namespace

absl::StatusOr<PalettePixelPlan> PlanPalettePixelPermutation(
    const GraphicsEditContext& source, const std::vector<size_t>& targets,
    const PalettePermutation& permutation) {
  std::set<uint8_t> values(permutation.begin(), permutation.end());
  if (permutation[0] != 0 || values.size() != 8 || *values.rbegin() != 7)
    return absl::InvalidArgumentError(
        "Permutation must be a bijection on 0..7 with zero fixed");
  if (!source.owner || targets.empty())
    return absl::InvalidArgumentError(
        "Select source tiles in a loaded session");
  PalettePixelPlan plan;
  plan.source = source;
  std::set<size_t> selected(targets.begin(), targets.end());
  plan.targets.assign(selected.begin(), selected.end());
  std::map<AddressKey, gfx::PaletteColorChange> colors;
  std::set<std::pair<uint16_t, uint16_t>> tiles;
  std::map<uint16_t, std::vector<uint8_t>> sheets;
  for (const auto index : plan.targets) {
    if (index >= source.targets.size())
      return absl::OutOfRangeError("Selected source target is unavailable");
    const auto& target = source.targets[index];
    RETURN_IF_ERROR(ValidateGraphicsPaletteAddressing(target, source.owner));
    if (target.tile_index >= 64 ||
        !source.owner->sheet_store.HasSheet(target.sheet_id) ||
        zelda3::GetGfxSheetStorageKind(target.sheet_id) ==
            zelda3::GfxSheetStorageKind::kCompressed2bpp) {
      return absl::UnimplementedError(
          "Only loaded static 3bpp tiles can be permuted");
    }
    for (int old_raw = 1; old_raw < 8; ++old_raw) {
      const int new_raw = permutation[old_raw];
      const auto key = Key(*target.writable_palette->colors[new_raw]);
      gfx::PaletteColorChange change{
          std::get<0>(key),        std::get<1>(key),        std::get<2>(key),
          target.palette[new_raw], target.palette[old_raw], 0};
      const auto [at, inserted] = colors.emplace(key, change);
      if (!inserted &&
          (at->second.original_color.snes() != change.original_color.snes() ||
           at->second.new_color.snes() != change.new_color.snes()))
        return absl::FailedPreconditionError(
            "Selected palette bindings require conflicting writes");
    }
    if (!tiles.emplace(target.sheet_id, target.tile_index).second)
      continue;
    const auto before = source.owner->sheet_store.Sheet(target.sheet_id);
    auto [at, inserted] =
        sheets.try_emplace(target.sheet_id, before.begin(), before.end());
    const int start =
        (target.tile_index / 16) * 8 * 128 + (target.tile_index % 16) * 8;
    for (int y = 0; y < 8; ++y) {
      for (int x = 0; x < 8; ++x) {
        auto& pixel = at->second[start + y * 128 + x];
        if (pixel > 7)
          return absl::FailedPreconditionError("Selected tile is not raw 3bpp");
        pixel = permutation[pixel];
      }
    }
  }
  plan.physical_tiles = tiles.size();
  for (const auto& [key, change] : colors)
    if (change.original_color.snes() != change.new_color.snes())
      plan.colors.push_back(change);
  for (const auto& [sheet, after] : sheets) {
    auto diff = MakeSheetPixelDiff(
        sheet, source.owner->sheet_store.Sheet(sheet), after);
    if (!diff.empty())
      plan.sheets.push_back(std::move(diff));
  }
  return plan;
}

absl::Status ExecutePalettePixelPlan(
    GraphicsEditorState& state, const PalettePixelPlan& plan, bool redo,
    const std::function<absl::Status(size_t)>& after_sheet_write) {
  auto* store = ActiveSheetStore(state);
  if (!store || state.game_data != plan.source.owner ||
      state.selection.is_floating)
    return absl::FailedPreconditionError(
        "Palette/pixel command session changed or paste is pending");
  RETURN_IF_ERROR(CheckLiveSource(plan, redo));
  std::map<uint16_t, std::vector<uint8_t>> next;
  for (const auto& diff : plan.sheets) {
    if (!store->HasSheet(diff.sheet) ||
        diff.offsets.size() != diff.before.size() ||
        diff.offsets.size() != diff.after.size() || next.contains(diff.sheet))
      return absl::InvalidArgumentError("Invalid command sheet diff");
    const auto current = store->Sheet(diff.sheet);
    auto& pixels = next[diff.sheet];
    pixels.assign(current.begin(), current.end());
    for (size_t i = 0; i < diff.offsets.size(); ++i) {
      const auto offset = diff.offsets[i];
      if (offset >= pixels.size() || diff.before[i] > 7 || diff.after[i] > 7)
        return absl::InvalidArgumentError("Invalid command pixel diff");
      if (pixels[offset] != (redo ? diff.before[i] : diff.after[i]))
        return absl::FailedPreconditionError(
            "Selected pixels changed since planning");
      pixels[offset] = redo ? diff.after[i] : diff.before[i];
    }
    if (std::any_of(pixels.begin(), pixels.end(), [](auto p) { return p > 7; }))
      return absl::FailedPreconditionError("Sheet is not raw 3bpp");
  }
  auto changes = plan.colors;
  if (!redo)
    for (auto& c : changes)
      std::swap(c.original_color, c.new_color);
  const auto before = *store;
  return gfx::PaletteManager::Get().ApplyExternalChanges(
      state.game_data, changes, [&] {
        size_t written = 0;
        for (const auto& [sheet, pixels] : next) {
          auto status = store->WriteSheet(sheet, pixels, 7);
          if (status.ok() && after_sheet_write)
            status = after_sheet_write(++written);
          if (!status.ok()) {
            *store = before;
            return status;
          }
        }
        // No fallible operations remain. Observers now see both completed models.
        for (const auto& [sheet, pixels] : next) {
          RefreshArenaSheet(state, sheet);
          state.MarkSheetModified(sheet);
        }
        return absl::OkStatus();
      });
}

absl::Status ApplyPalettePixelPermutation(
    GraphicsEditorState& state, UndoManager& history,
    const std::vector<size_t>& targets, const PalettePermutation& permutation) {
  if (!state.source_context || state.source_context->owner != state.game_data)
    return absl::FailedPreconditionError(
        "Open Graphics from a supported live source");
  ASSIGN_OR_RETURN(auto plan, PlanPalettePixelPermutation(
                                  *state.source_context, targets, permutation));
  RETURN_IF_ERROR(ExecutePalettePixelPlan(state, plan, true));
  if (!plan.colors.empty() || !plan.sheets.empty())
    history.Push(std::make_unique<PalettePixelAction>(state, std::move(plan)));
  return absl::OkStatus();
}
}  // namespace yaze::editor
