#ifndef YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_GRAPHICS_CONTEXT_H_
#define YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_GRAPHICS_CONTEXT_H_

#include <functional>
#include <memory>

#include "app/editor/graphics/graphics_edit_context.h"
#include "app/gfx/types/snes_tile.h"
namespace yaze::zelda3 {
class Overworld;
class OverworldMap;
}  // namespace yaze::zelda3
namespace yaze::editor {
// Uses the selected map's loaded sheet slots and exact raw-index palette.
absl::StatusOr<GraphicsEditTarget> ResolveOverworldQuadrantTarget(
    const zelda3::OverworldMap& map, const gfx::Tile16& tile16, int tile16_id,
    int quadrant, const zelda3::GameData* owner);
// Opens the requested quadrant first and includes every other editable
// quadrant, retaining separate palette/flip context for each use.
absl::StatusOr<GraphicsEditContext> MakeOverworldGraphicsEditContext(
    const zelda3::OverworldMap& map, const gfx::Tile16& tile16, int tile16_id,
    int quadrant, const zelda3::GameData* owner);

// The overworld session a Graphics editor context reads back from while it
// stays open. `overworld` must outlive the context.
struct OverworldGraphicsLiveSource {
  zelda3::Overworld* overworld = nullptr;
  // Callbacks refuse to run once this no longer returns `owner`.
  std::function<const zelda3::GameData*()> current_game_data;
  const zelda3::GameData* owner = nullptr;
  // Reapplies project graphics previews before the live map reloads.
  std::function<absl::Status()> sync_project_preview;
  // Copy of the source map; keeps its resolved assignments and palette.
  std::shared_ptr<zelda3::OverworldMap> preview_map;
  int source_map_id = 0;
  int tile16_id = 0;
  int quadrant = 0;
};

// Sets the live-source, palette refresh, preview revision and preview render
// callbacks on a context made by MakeOverworldGraphicsEditContext.
void AttachOverworldLiveSource(GraphicsEditContext& context,
                               OverworldGraphicsLiveSource source);
}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_GRAPHICS_CONTEXT_H_
