#ifndef YAZE_APP_EDITOR_GRAPHICS_SCREEN_GRAPHICS_CONTEXT_H_
#define YAZE_APP_EDITOR_GRAPHICS_SCREEN_GRAPHICS_CONTEXT_H_

#include "app/editor/graphics/graphics_edit_context.h"
namespace yaze::editor {
// Raw Dungeon Map / Tiles Gfx atlas, independent of Tile16 composition.
absl::StatusOr<usage_preview::PreviewImage> RenderDungeonMapTilesGfxPreview(
    const zelda3::GameData* owner);
absl::StatusOr<GraphicsEditContext> MakeDungeonMapTilesGfxEditContext(
    const zelda3::GameData* owner, int selected_tile8, bool binary_gfx_loaded);
}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_GRAPHICS_SCREEN_GRAPHICS_CONTEXT_H_
