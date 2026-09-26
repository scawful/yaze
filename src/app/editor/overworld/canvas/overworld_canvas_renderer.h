#ifndef YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_CANVAS_RENDERER_H
#define YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_CANVAS_RENDERER_H

#include <cstdint>
#include <unordered_map>

#include "absl/status/status.h"
#include "app/gfx/core/bitmap.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

class OverworldEditor;

/**
 * @class OverworldCanvasRenderer
 * @brief Handles all canvas drawing and panel rendering for the overworld
 * editor.
 *
 * Extracted from OverworldEditor to separate rendering concerns from editing
 * logic. This class handles:
 * - Main overworld canvas drawing (map bitmaps, entities, tile edits)
 * - Panel rendering (tile16 selector, tile8 selector, area graphics, etc.)
 * - Properties panel rendering
 *
 * All state is accessed through a pointer to the owning OverworldEditor.
 * The renderer is declared as a friend class of OverworldEditor for direct
 * member access.
 */
class OverworldCanvasRenderer {
 public:
  explicit OverworldCanvasRenderer(OverworldEditor* editor);

  // =========================================================================
  // Main Canvas Drawing
  // =========================================================================

  /// @brief Draw the main overworld canvas with toolbar, maps, and entities.
  /// This is the primary entry point called from the OverworldCanvasPanel.
  void DrawOverworldCanvas();

  // =========================================================================
  // Panel Drawing Methods
  // =========================================================================

  /// @brief Draw the tile16 selector panel
  absl::Status DrawTile16Selector();

  /// @brief Draw the tile8 selector panel (graphics bin)
  void DrawTile8Selector();

  /// @brief Draw the area graphics panel
  absl::Status DrawAreaGraphics();

  /// @brief Draw the v3 settings panel
  void DrawV3Settings();

  /// @brief Draw the map properties panel (sidebar-based)
  void DrawMapProperties();

  /// @brief Draw the overworld properties grid (debug/info view)
  void DrawOverworldProperties();

 private:
  // =========================================================================
  // Internal Canvas Drawing Helpers
  // =========================================================================

  /// @brief Render the 64 overworld map bitmaps to the canvas
  void DrawOverworldMaps();
  void DrawMapWithAreaLayers(int map_index, int map_x, int map_y, float scale);

  // Subscreen overlay layer per map (sky, fog, lava, canopy, rain), built in
  // the map's palette and refreshed when the map's pixels change.
  struct OverlayLayer {
    gfx::Bitmap bitmap;
    uint64_t bitmap_serial = 0;
    bool background = false;
    bool has_overlay = false;
  };
  OverlayLayer* EnsureOverlayLayer(int map_index);
  std::unordered_map<int, OverlayLayer> overlay_layers_;
  bool PrepareContextMenu(const ImVec2& screen_position);
  /// In an entity mode only that entity type responds to hover/drag/menu.
  void FilterHoveredEntityByMode();

  // =========================================================================
  // Data
  // =========================================================================

  OverworldEditor* editor_;  ///< Non-owning pointer to the parent editor
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_CANVAS_RENDERER_H
