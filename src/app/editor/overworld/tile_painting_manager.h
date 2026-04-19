#ifndef YAZE_APP_EDITOR_OVERWORLD_TILE_PAINTING_MANAGER_H
#define YAZE_APP_EDITOR_OVERWORLD_TILE_PAINTING_MANAGER_H

#include <array>
#include <functional>
#include <vector>

#include "app/editor/overworld/ui_constants.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/render/tilemap.h"
#include "app/gui/canvas/canvas.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "zelda3/overworld/overworld.h"

namespace yaze {
namespace editor {

class Tile16Editor;

/// @brief Shared state for the tile painting system.
///
/// All pointers are non-owning references into OverworldEditor members.
struct TilePaintingDependencies {
  gui::Canvas* ow_map_canvas = nullptr;
  zelda3::Overworld* overworld = nullptr;
  std::array<gfx::Bitmap, zelda3::kNumOverworldMaps>* maps_bmp = nullptr;
  gfx::Tilemap* tile16_blockset = nullptr;
  int* current_tile16 = nullptr;
  std::vector<int>* selected_tile16_ids = nullptr;
  int* current_map = nullptr;
  int* current_world = nullptr;
  int* current_parent = nullptr;
  EditingMode* current_mode = nullptr;
  int* game_state = nullptr;
  Rom* rom = nullptr;
  Tile16Editor* tile16_editor = nullptr;
};

/// @brief Callbacks for undo integration and map refresh.
struct TilePaintingCallbacks {
  std::function<void(int map_id, int world, int x, int y, int old_tile_id)>
      create_undo_point;
  std::function<void()> finalize_paint_operation;
  std::function<void()> refresh_overworld_map;
  std::function<void(int map_index)> refresh_overworld_map_on_demand;
  std::function<void()> scroll_blockset_to_current_tile;
  /// When set, eyedropper routes here (guarded `RequestTileSwitch` path).
  std::function<void(int tile_id)> request_tile16_selection;
};

/// @brief Manages tile painting, fill, selection, and eyedropper operations.
///
/// Extracted from OverworldEditor to encapsulate the ~349 lines of tile
/// editing logic. Operates on shared state through TilePaintingDependencies
/// and delegates undo/refresh through TilePaintingCallbacks.
class TilePaintingManager {
 public:
  TilePaintingManager(const TilePaintingDependencies& deps,
                      const TilePaintingCallbacks& callbacks);

  /// @brief Main entry point: check for tile edits (paint, fill, stamp).
  void CheckForOverworldEdits();

  /// @brief Draw and create the tile16 IDs that are currently selected.
  void CheckForSelectRectangle();

  /// @brief Update bitmap pixels after a single tile paint.
  void RenderUpdatedMapBitmap(const ImVec2& click_position,
                              const std::vector<uint8_t>& tile_data);

  /// @brief Eyedropper: pick the tile16 under the hovered canvas position.
  bool PickTile16FromHoveredCanvas();

  /// @brief Toggle between DRAW_TILE and MOUSE modes.
  void ToggleBrushTool();

  /// @brief Toggle FILL_TILE mode on/off.
  void ActivateFillTool();

 private:
  /// @brief Handle the actual drawing of a single tile (called by
  /// CheckForOverworldEdits when DrawTilemapPainter triggers).
  void DrawOverworldEdits();

  TilePaintingDependencies deps_;
  TilePaintingCallbacks callbacks_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_TILE_PAINTING_MANAGER_H
