#ifndef YAZE_APP_GUI_WIDGETS_TILE_SELECTOR_WIDGET_H
#define YAZE_APP_GUI_WIDGETS_TILE_SELECTOR_WIDGET_H

#include <string>
#include <string_view>

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
  void SetSelectedTile(int tile_id);
  int GetSelectedTileID() const { return selected_tile_id_; }
  int GetMaxTileId() const { return total_tiles_ > 0 ? total_tiles_ - 1 : 0; }

  RenderResult Render(gfx::Bitmap& atlas, bool atlas_ready);

  /// Draw a compact filter/search bar above the tile grid. Returns true if
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

 private:
  RenderResult HandleInteraction(int tile_display_size);
  int ResolveTileAtCursor(int tile_display_size) const;
  void DrawHighlight(int tile_display_size) const;
  void DrawTileIdLabels(int tile_display_size) const;
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
};

}  // namespace yaze::gui

#endif  // YAZE_APP_GUI_WIDGETS_TILE_SELECTOR_WIDGET_H
