#ifndef YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_GRAPHICS_CONTEXT_H_
#define YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_GRAPHICS_CONTEXT_H_

#include "app/editor/graphics/graphics_edit_context.h"
#include "app/gfx/types/snes_tile.h"
namespace yaze::zelda3 {
class OverworldMap;
}
namespace yaze::editor {
// Uses the selected map's loaded sheet slots and exact raw-index palette.
absl::StatusOr<GraphicsEditTarget> ResolveOverworldQuadrantTarget(
    const zelda3::OverworldMap& map, const gfx::Tile16& tile16, int tile16_id,
    int quadrant, const zelda3::GameData* owner);
absl::StatusOr<GraphicsEditContext> MakeOverworldGraphicsEditContext(
    const zelda3::OverworldMap& map, const gfx::Tile16& tile16, int tile16_id,
    int quadrant, const zelda3::GameData* owner);
}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_GRAPHICS_CONTEXT_H_
