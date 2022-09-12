#ifndef YAZE_GUI_CANVAS_H
#define YAZE_GUI_CANVAS_H

#include <imgui/imgui.h>

#include <cmath>
#include <string>

#include "app/gfx/bitmap.h"
#include "app/rom.h"

namespace yaze {
namespace gui {

using app::gfx::Bitmap;

class Canvas {
 public:
  Canvas() = default;
  Canvas(ImVec2 canvas_size)
      : custom_canvas_size_(true), canvas_sz_(canvas_size) {}

  void DrawBackground(ImVec2 canvas_size = ImVec2(0, 0));
  void DrawContextMenu();
  void DrawTilesFromUser(app::ROM& rom, Bytes& tile,
                         app::gfx::SNESPalette& pal);
  void DrawBitmap(const Bitmap& bitmap, int border_offset = 0);
  void DrawBitmap(const Bitmap& bitmap, int x_offset, int y_offset);
  void DrawOutline(int x, int y, int w, int h);
  void DrawText(std::string text, int x, int y);
  void DrawGrid(float grid_step = 64.0f);
  void DrawOverlay();  // last

  auto GetDrawList() const { return draw_list_; }
  auto GetZeroPoint() const { return canvas_p0_; }
  void SetCanvasSize(ImVec2 canvas_size) {
    canvas_sz_ = canvas_size;
    custom_canvas_size_ = true;
  }

 private:
  bool enable_grid_ = true;
  bool enable_context_menu_ = true;
  bool custom_canvas_size_ = false;
  bool is_hovered_ = false;

  ImDrawList* draw_list_;
  ImVector<ImVec2> points_;
  ImVec2 scrolling_;
  ImVec2 canvas_sz_;
  ImVec2 canvas_p0_;
  ImVec2 canvas_p1_;
  ImVec2 mouse_pos_in_canvas_;

  std::vector<app::gfx::Bitmap> changed_tiles_;

  std::string title_;
};

}  // namespace gui
}  // namespace yaze

#endif