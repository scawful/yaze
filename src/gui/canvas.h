#ifndef YAZE_GUI_CANVAS_H
#define YAZE_GUI_CANVAS_H

#include <imgui/imgui.h>

#include <cmath>
#include <string>

namespace yaze {
namespace gui {

class Canvas {
 public:
  Canvas() = default;
  Canvas(ImVec2 canvas_size)
      : custom_canvas_size_(true), canvas_sz_(canvas_size) {}

  void Update();

  void DrawBackground(ImVec2 canvas_size = ImVec2(0, 0));
  void UpdateContext();
  void DrawGrid(float grid_step = 64.0f);
  void DrawOverlay();  // last

  void SetCanvasSize(ImVec2 canvas_size) {
    canvas_sz_ = canvas_size;
    custom_canvas_size_ = true;
  }
  auto GetDrawList() const { return draw_list_; }
  auto GetZeroPoint() const { return canvas_p0_; }

 private:
  bool enable_grid_ = true;
  bool enable_context_menu_ = true;
  bool dragging_select_ = false;
  bool custom_canvas_size_ = false;

  ImDrawList* draw_list_;
  ImVector<ImVec2> points_;
  ImVec2 scrolling_;
  ImVec2 canvas_sz_;
  ImVec2 canvas_p0_;
  ImVec2 canvas_p1_;

  std::string title_;
};

}  // namespace gui
}  // namespace yaze

#endif