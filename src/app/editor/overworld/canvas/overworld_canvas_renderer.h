#ifndef YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_CANVAS_RENDERER_H
#define YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_CANVAS_RENDERER_H

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "app/gfx/core/bitmap.h"
#include "app/gui/layout/adaptive_sheet_layout.h"
#include "imgui/imgui.h"
#include "zelda3/overworld/tile16_usage_index.h"

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

  const std::vector<int>& tile16_usage_counts() const {
    return tile16_usage_counts_;
  }
  int tile16_usage_count(int tile_id) const {
    if (tile_id >= 0 &&
        tile_id < static_cast<int>(tile16_usage_counts_.size())) {
      return tile16_usage_counts_[tile_id];
    }
    return 0;
  }
  std::string Tile16ScopeLabel() const;

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
  void DrawEntityHoverTooltip();

  enum class Tile16UsageScope { kAllMaps, kCurrentWorld, kCurrentArea, kArea };
  void DrawTile16ViewControls();
  void DrawTile16AreaPicker();
  std::vector<int> Tile16ScopeMaps() const;
  /// Recounts placements at most twice a second, and never while the mouse is
  /// down or over the grid, so tiles don't move under the cursor.
  void RefreshTile16Order(bool grid_hovered);
  /// Shows every tile in ID order and forces a recount on the next refresh.
  void ResetTile16Order();

  // =========================================================================
  // Data
  // =========================================================================

  OverworldEditor* editor_;  ///< Non-owning pointer to the parent editor
  std::optional<ImVec2> entity_drag_origin_;
  gui::AdaptiveSheetScaleMode tile16_scale_mode_ =
      gui::AdaptiveSheetScaleMode::kFit;
  gui::AdaptiveSheetScaleMode area_graphics_scale_mode_ =
      gui::AdaptiveSheetScaleMode::kFit;

  zelda3::Tile16SortMode tile16_sort_ = zelda3::Tile16SortMode::kUsage;
  Tile16UsageScope tile16_scope_ = Tile16UsageScope::kAllMaps;
  int tile16_scope_area_ = 0;
  bool tile16_hide_unused_ = false;
  bool tile16_order_dirty_ = true;
  bool tile16_grid_hovered_ = false;
  double tile16_last_count_time_ = -1.0;
  // Scope inputs the cached counts were built for.
  int tile16_counted_world_ = -1;
  int tile16_counted_parent_ = -1;
  std::vector<int> tile16_usage_counts_;
  char tile16_area_search_[64] = {};
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_CANVAS_RENDERER_H
