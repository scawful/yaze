#ifndef YAZE_APP_GUI_WIDGETS_TILE_SELECTOR_WIDGET_H
#define YAZE_APP_GUI_WIDGETS_TILE_SELECTOR_WIDGET_H

#include <string>

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

  explicit TileSelectorWidget(std::string widget_id);
  TileSelectorWidget(std::string widget_id, Config config);

  void AttachCanvas(Canvas* canvas);
  void SetTileCount(int total_tiles);
  void SetSelectedTile(int tile_id);
  int GetSelectedTileID() const { return selected_tile_id_; }

  RenderResult Render(gfx::Bitmap& atlas, bool atlas_ready);

  void ScrollToTile(int tile_id, bool use_imgui_scroll = true);
  ImVec2 TileOrigin(int tile_id) const;

 private:
  RenderResult HandleInteraction(int tile_display_size);
  int ResolveTileAtCursor(int tile_display_size) const;
  void DrawHighlight(int tile_display_size) const;
  void DrawTileIdLabels(int tile_display_size) const;
  bool IsValidTileId(int tile_id) const;

  Canvas* canvas_ = nullptr;
  Config config_{};
  int selected_tile_id_ = 0;
  int total_tiles_ = 0;
  std::string widget_id_;

  // Deferred scroll state (for when ScrollToTile is called outside render
  // context)
  mutable int pending_scroll_tile_id_ = -1;
  mutable bool pending_scroll_use_imgui_ = true;
};

}  // namespace yaze::gui

#endif  // YAZE_APP_GUI_WIDGETS_TILE_SELECTOR_WIDGET_H
