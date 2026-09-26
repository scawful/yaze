#ifndef YAZE_APP_EDITOR_OVERWORLD_PAINTING_TILE_PAINTING_MANAGER_H
#define YAZE_APP_EDITOR_OVERWORLD_PAINTING_TILE_PAINTING_MANAGER_H

#include <array>
#include <functional>
#include <vector>

#include "app/editor/overworld/painting/tile_brush.h"
#include "app/editor/overworld/ui/ui_constants.h"
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
  EditingMode* current_mode = nullptr;
  Rom* rom = nullptr;
  Tile16Editor* tile16_editor = nullptr;
};

/// @brief Callbacks for undo integration and map refresh.
struct TilePaintingCallbacks {
  std::function<void(int map_id, int world, int x, int y, int old_tile_id)>
      create_undo_point;
  std::function<void()> finalize_paint_operation;
  std::function<void(int map_index)> refresh_overworld_map_on_demand;
  std::function<void()> scroll_blockset_to_current_tile;
  /// When set, eyedropper routes here (guarded `RequestTileSwitch` path).
  std::function<void(int tile_id)> request_tile16_selection;
};

/// @brief Manages tile painting, fill, selection, and eyedropper operations.
///
/// Owns captured brush contents and canvas gestures. All destinations use
/// world Tile16 coordinates; callbacks publish undo and refresh affected maps.
class TilePaintingManager {
 public:
  TilePaintingManager(const TilePaintingDependencies& deps,
                      const TilePaintingCallbacks& callbacks);

  /// @brief Main entry point: check for tile edits (paint, fill, stamp).
  void CheckForOverworldEdits();

  /// @brief Draw and create the tile16 IDs that are currently selected.
  void CheckForSelectRectangle();

  /// The captured selection, independent of its source or preview position.
  const TileBrush* selection_brush() const;

  /// Paste a validated pattern at the current canvas cursor, using the same
  /// clipping, per-map refresh, and undo path as rectangle painting.
  absl::Status PasteBrush(const TileBrush& brush);

  /// @brief Update bitmap pixels after a single tile paint.
  void RenderUpdatedMapBitmap(const ImVec2& click_position,
                              const std::vector<uint8_t>& tile_data);

  /// @brief Eyedropper: pick the tile16 under the hovered canvas position.
  bool PickTile16FromHoveredCanvas();

  /// @brief Toggle between DRAW_TILE and MOUSE modes.
  void ToggleBrushTool();

  /// @brief Toggle FILL_TILE mode on/off.
  void ActivateFillTool();

  /// Last brush preview piece built for @p map_id: the brush drawn with that
  /// map's own tile16 graphics and palette (tests and diagnostics).
  const gfx::Bitmap& map_brush_preview(int map_id) const {
    return map_brush_previews_.at(map_id);
  }

 private:
  struct TilePosition {
    int x;
    int y;
  };
  using ChangedMaps = std::array<bool, zelda3::kNumOverworldMaps>;

  TilePosition HoveredTile() const;
  bool IsValidTile(TilePosition position) const;
  std::vector<std::vector<uint16_t>>& WorldTiles() const;
  void CaptureSelection();
  // Draws `brush` at `anchor` with each destination map's own graphics and
  // palette. `mark_selection` also moves the canvas selection rectangle.
  void DrawBrushPreview(const TileBrush& brush, TilePosition anchor,
                        bool mark_selection, int alpha);
  // Pixels of `tile_id` as `map_id` draws it (its own tile16 blockset);
  // falls back to the shared blockset when that map has none built.
  std::vector<uint8_t> Tile16PixelsForMap(int map_id, int tile_id) const;
  void PaintPattern(const TileBrush& brush, TilePosition anchor, int width,
                    int height, bool finalize = true);
  bool PaintTile(TilePosition position, int tile_id, ChangedMaps& changed_maps);
  void RenderMapTile(int map_id, TilePosition position,
                     const std::vector<uint8_t>& tile_data);

  TilePaintingDependencies deps_;
  TilePaintingCallbacks callbacks_;
  bool paint_gesture_owned_ = false;
  bool single_paint_pending_ = false;
  bool selection_gesture_owned_ = false;
  TileBrush brush_;
  std::array<gfx::Bitmap, zelda3::kNumOverworldMaps> map_brush_previews_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_PAINTING_TILE_PAINTING_MANAGER_H
