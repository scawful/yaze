#ifndef YAZE_APP_EDITOR_GRAPHICS_PIXEL_INDEX_COMMAND_H_
#define YAZE_APP_EDITOR_GRAPHICS_PIXEL_INDEX_COMMAND_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "app/editor/graphics/graphics_edit_context.h"
#include "app/editor/graphics/graphics_sheet_sync.h"

namespace yaze::zelda3 {
struct GameData;
}  // namespace yaze::zelda3

namespace yaze::editor {
class UndoManager;

using PixelIndexPermutation = std::array<uint8_t, 8>;

// Bit y * 8 + x selects a displayed pixel. H/V are the displayed tile's
// orientation; planning maps the mask back to the physical, unflipped tile.
// Aliases of the same physical tile are unioned before any pixels change.
struct PixelIndexTarget {
  uint16_t sheet_id = 0;
  uint16_t tile_index = 0;
  uint64_t pixel_mask = ~uint64_t{0};
  bool hflip = false;
  bool vflip = false;
};

// Pure helper for a rectangle within one 128x32 sheet. Does not clip bounds.
absl::StatusOr<std::vector<PixelIndexTarget>> MakePixelIndexRectangleSelection(
    uint16_t sheet_id, int x, int y, int width, int height);

enum class PixelIndexOperation { kPermutation, kArtReplacement };

// Inspectable fixed-palette edit. Palette data is never part of the mutation.
// Targets are canonical physical masks (H/V false). Full initial snapshots
// also detect an in-progress writer whose store revision has not committed.
struct PixelIndexPlan {
  PixelIndexOperation operation = PixelIndexOperation::kPermutation;
  bool keep_transparency = true;
  std::string description = "Remap selected pixel indices";
  const zelda3::GameData* owner = nullptr;
  uint64_t source_context_revision = 0;
  std::map<uint16_t, uint64_t> sheet_revisions;
  std::map<uint16_t, std::vector<uint8_t>> before_sheets;
  std::vector<PixelIndexTarget> targets;
  std::vector<SheetPixelDiff> sheets;
  // Existing source context is retained only for a read-only live assignment
  // guard. A palette address is never required. Unrelated sheet selections
  // do not acquire a source guard.
  std::optional<GraphicsEditContext> source;
  std::vector<size_t> source_targets;
  size_t physical_tiles = 0;
  size_t selected_pixels = 0;
};

absl::StatusOr<PixelIndexPlan> PlanPixelIndexPermutation(
    const GraphicsEditorState& state,
    const std::vector<PixelIndexTarget>& targets,
    const PixelIndexPermutation& permutation);

// Stages new artwork through the same pixel transaction. Each selected sheet
// needs one full 4096-byte result; any change outside the target masks is
// refused. Explicit artwork replacement can change zero to/from nonzero.
absl::StatusOr<PixelIndexPlan> PlanPixelIndexReplacement(
    const GraphicsEditorState& state,
    const std::vector<PixelIndexTarget>& targets,
    const std::map<uint16_t, std::vector<uint8_t>>& after_sheets,
    std::string description = "Apply art to object source");

enum class PixelIndexExecution { kApply, kUndo, kRedo };

// All sheets preflight before mutation. A failed write restores the store
// before refreshing Arena copies or publishing dirty callbacks.
// Initial apply checks captured revisions; history replay checks the owner
// and expected affected pixels, allowing ordinary later edits to be undone.
// Source-bound edits also re-resolve physical assignments and logical cells.
// after_sheet_write is a failure-injection seam; production callers omit it.
absl::Status ExecutePixelIndexPlan(
    GraphicsEditorState& state, const PixelIndexPlan& plan,
    PixelIndexExecution execution,
    const std::function<absl::Status(size_t)>& after_sheet_write = {});

// Apply a staged plan and push exactly one Graphics history action. A no-op
// performs no write, dirty notification, revision change, or history push.
absl::Status ApplyPixelIndexPlan(GraphicsEditorState& state,
                                 UndoManager& history,
                                 const PixelIndexPlan& plan);

absl::Status ApplyPixelIndexPermutation(
    GraphicsEditorState& state, UndoManager& history,
    const std::vector<PixelIndexTarget>& targets,
    const PixelIndexPermutation& permutation);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_GRAPHICS_PIXEL_INDEX_COMMAND_H_
