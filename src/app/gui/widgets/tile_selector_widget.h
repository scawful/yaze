#ifndef YAZE_APP_GUI_WIDGETS_TILE_SELECTOR_WIDGET_H
#define YAZE_APP_GUI_WIDGETS_TILE_SELECTOR_WIDGET_H

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/gfx/core/bitmap.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/drag_drop.h"
#include "imgui/imgui.h"

namespace yaze::gui {

/**
 * @brief Reusable tile selector built on top of Canvas.
 *
 * Minimal mutable state, designed for reuse across editors and automation.
 */
class TileSelectorWidget {
 public:
  struct Config {
    int tile_size = 16;
    float display_scale = 2.0f;
    int tiles_per_row = 8;
    int total_tiles = 512;
    ImVec2 draw_offset = {2.0f, 0.0f};
    bool show_tile_ids = false;
    bool enable_drag = false;
    bool show_hover_tooltip = false;
    int drag_source_map_id = -1;
    ImVec4 highlight_color = {1.0f, 0.85f, 0.35f, 1.0f};
  };

  struct RenderResult {
    bool tile_clicked = false;
    bool tile_double_clicked = false;
    bool tile_right_clicked = false;
    bool selection_changed = false;
    bool tile_dragging = false;
    int selected_tile = -1;
  };

  enum class JumpToTileResult {
    kSuccess = 0,
    kInvalidFormat,
    kOutOfRange,
  };

  explicit TileSelectorWidget(std::string widget_id);
  TileSelectorWidget(std::string widget_id, Config config);

  void AttachCanvas(Canvas* canvas);
  void SetTileCount(int total_tiles);
  /// Snaps down so each tile cell is a whole number of screen pixels.
  void SetDisplayScale(float display_scale);
  float display_scale() const { return config_.display_scale; }
  /// On-screen size of one tile; the atlas, grid, highlight and hit testing
  /// all use this value.
  float TileCellSize() const;
  void SetTilesPerRow(int tiles_per_row);
  int tiles_per_row() const { return config_.tiles_per_row; }
  void SetSelectedTile(int tile_id);
  int GetSelectedTileID() const { return selected_tile_id_; }
  int GetMaxTileId() const { return total_tiles_ > 0 ? total_tiles_ - 1 : 0; }
  ImVec2 GetGridContentSize() const;
  /// Grid width plus the current style's vertical scrollbar. This is the
  /// width of the scrolling grid child, not of any surrounding panel.
  float GetPreferredViewportWidth() const;
  /// Same as GetPreferredViewportWidth() for a config that has no widget yet
  /// (panels report their preferred dock width before the widget exists).
  static float PreferredViewportWidth(const Config& config,
                                      float scrollbar_size);
  /// Current style scrollbar size, or ImGui's default without a context.
  static float CurrentScrollbarSize();

  RenderResult Render(gfx::Bitmap& atlas, bool atlas_ready);

  /// "Copy Tile ID" item for the attached canvas's editor menu. Right-click
  /// selects the tile under the cursor, so the item copies the selected tile
  /// ID ("0x01A") at click time. Callers add it next to their own items,
  /// after ClearContextMenuItems(), so nothing accumulates across frames.
  CanvasMenuItem CopyTileIdMenuItem() const;

  /// Draw a lookup/filter popup button beside the grid controls. Returns true if
  /// the user jumped to a tile (selection + scroll triggered).
  bool DrawFilterBar();

  /// Parse a tile ID string and jump selection to that tile.
  /// Accepted forms:
  /// - Hex (default): "1A", "0x1A"
  /// - Decimal (explicit): "d:26"
  /// Returns a parse/result status for UI and automation.
  JumpToTileResult JumpToTileFromInput(std::string_view input);

  void ScrollToTile(int tile_id, bool use_imgui_scroll = true);
  ImVec2 TileOrigin(int tile_id) const;

  // Range filter accessors (for tests and automation)
  bool has_active_range_filter() const { return filter_range_active_; }
  int filter_range_min() const { return filter_range_min_; }
  int filter_range_max() const { return filter_range_max_; }
  void SetRangeFilter(int min_id, int max_id);
  void ClearRangeFilter();

  /// Grid cell i shows tile order[i]; tiles not listed are hidden. Invalid and
  /// duplicate IDs are dropped. Without an order, cells follow tile ID order.
  void SetDisplayOrder(std::vector<int> order);
  void ClearDisplayOrder();
  bool has_display_order() const { return display_order_active_; }
  const std::vector<int>& display_order() const { return display_order_; }
  /// Tile shown in a grid cell, or -1 past the last cell.
  int TileAtCell(int cell) const;
  /// Grid cell showing a tile, or -1 when the display order hides it.
  int CellOfTile(int tile_id) const;
  /// Extra hover tooltip line for a tile (empty string adds nothing).
  void SetHoverDetailProvider(std::function<std::string(int)> provider) {
    hover_detail_ = std::move(provider);
  }

 private:
  bool DrawFilterControls();
  int CellCount() const;
  RenderResult HandleInteraction(float tile_display_size);
  int ResolveTileAtCursor(float tile_display_size) const;
  ImVec2 GridScreenOrigin() const;
  // Visible grid rows [first, last) inside the current clip rect.
  std::pair<int, int> VisibleRows(float tile_display_size) const;
  void DrawOrderedTiles(gfx::Bitmap& atlas, float tile_display_size) const;
  void DrawGridLines(float tile_display_size) const;
  void DrawHighlight(float tile_display_size) const;
  void DrawTileIdLabels(float tile_display_size) const;
  bool IsValidTileId(int tile_id) const;
  bool IsInFilterRange(int tile_id) const;

  Canvas* canvas_ = nullptr;
  Config config_{};
  int selected_tile_id_ = 0;
  int total_tiles_ = 0;
  std::string widget_id_;

  // Deferred scroll state (for when ScrollToTile is called outside render
  // context)
  mutable int pending_scroll_tile_id_ = -1;
  mutable bool pending_scroll_use_imgui_ = true;

  // Filter bar state
  char filter_buf_[8] = {};

  // Range filter state
  bool filter_range_active_ = false;
  int filter_range_min_ = 0;
  int filter_range_max_ = 0;
  char filter_min_buf_[8] = {};
  char filter_max_buf_[8] = {};
  // Set when the last range input was invalid (min > max).
  bool filter_range_error_ = false;
  // Set when both parsed values exceed total_tiles_ so SetRangeFilter returned
  // without activating (empty interval after clamping).
  bool filter_out_of_range_ = false;
  // Status from the most recent jump-to-ID entry attempt.
  JumpToTileResult last_jump_result_ = JumpToTileResult::kSuccess;

  bool display_order_active_ = false;
  std::vector<int> display_order_;
  // Inverse of display_order_, sized total_tiles_; -1 marks hidden tiles.
  std::vector<int> cell_of_tile_;
  std::function<std::string(int)> hover_detail_;
};

}  // namespace yaze::gui

#endif  // YAZE_APP_GUI_WIDGETS_TILE_SELECTOR_WIDGET_H
