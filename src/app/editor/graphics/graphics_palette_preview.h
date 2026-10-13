#ifndef YAZE_APP_EDITOR_GRAPHICS_GRAPHICS_PALETTE_PREVIEW_H_
#define YAZE_APP_EDITOR_GRAPHICS_GRAPHICS_PALETTE_PREVIEW_H_

#include <optional>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/graphics/graphics_editor_state.h"

namespace yaze::editor {

struct GraphicsPalettePreview {
  gfx::SnesPalette colors;  // Eight displayed indices, including transparency.
  int palette_count = 0;
  int row_count = 0;
};

absl::StatusOr<GraphicsPalettePreview> ResolveGraphicsPreviewPalette(
    zelda3::GameData& data, const GraphicsPaletteSelection& selection);

// Updates only the owning session's display copy. Null restores loaded colors.
// Invalid selections leave both the previous choice and display unchanged.
absl::Status SetGraphicsPreviewPalette(
    GraphicsEditorState& state, uint16_t sheet,
    std::optional<GraphicsPaletteSelection> selection);

// Restores choices after the session's Arena sheets are reloaded and reflects
// source palette edits without changing authored pixels or their history.
void RefreshGraphicsPreviewPalettes(GraphicsEditorState& state);

}  // namespace yaze::editor
#endif
