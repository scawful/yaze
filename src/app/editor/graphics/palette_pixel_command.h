#ifndef YAZE_APP_EDITOR_GRAPHICS_PALETTE_PIXEL_COMMAND_H_
#define YAZE_APP_EDITOR_GRAPHICS_PALETTE_PIXEL_COMMAND_H_

#include <array>
#include <functional>
#include <vector>

#include "app/editor/graphics/graphics_edit_context.h"
#include "app/editor/graphics/graphics_sheet_sync.h"
#include "app/gfx/util/palette_manager.h"

namespace yaze::editor {
class UndoManager;
using PalettePermutation = std::array<uint8_t, 8>;

// Pure, inspectable plan. Selected artwork is preserved; other uses of the
// same physical tiles/palettes are intentionally affected, not duplicated.
struct PalettePixelPlan {
  GraphicsEditContext source;
  std::vector<size_t> targets;
  std::vector<gfx::PaletteColorChange> colors;
  std::vector<SheetPixelDiff> sheets;
  size_t physical_tiles = 0;
};
absl::StatusOr<PalettePixelPlan> PlanPalettePixelPermutation(
    const GraphicsEditContext& source, const std::vector<size_t>& targets,
    const PalettePermutation& permutation);

// Low-level atomic executor used by the single Graphics undo action.
// after_sheet_write is a failure-injection seam; production callers omit it.
absl::Status ExecutePalettePixelPlan(
    GraphicsEditorState& state, const PalettePixelPlan& plan, bool redo,
    const std::function<absl::Status(size_t)>& after_sheet_write = {});

// Product command: plan, authoritative live-source preflight, atomic apply,
// and exactly one push to the existing Graphics editor history.
absl::Status ApplyPalettePixelPermutation(
    GraphicsEditorState& state, UndoManager& history,
    const std::vector<size_t>& targets, const PalettePermutation& permutation);
}  // namespace yaze::editor
#endif
