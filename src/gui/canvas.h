#ifndef YAZE_GUI_CANVAS_H
#define YAZE_GUI_CANVAS_H

#include <imgui/imgui.h>

#include <cmath>
#include <memory>
#include <string>

namespace yaze {
namespace gui {
class Canvas {
 public:
  Canvas() = default;
  Canvas(ImVec2 canvas_size)
      : canvas_sz_(canvas_size), custom_canvas_size_(true) {}

  void Update();

  void DrawBackground();
  void UpdateContext();
  void DrawGrid();
  void DrawOverlay();  // last

 private:
  bool enable_grid_ = true;
  bool enable_context_menu_ = true;
  bool dragging_select_ = false;
  bool custom_canvas_size_ = false;

  std::string title_;

  ImDrawList* draw_list_;
  ImVector<ImVec2> points_;
  ImVec2 scrolling_;
  ImVec2 canvas_sz_;
  ImVec2 canvas_p0_;
  ImVec2 canvas_p1_;
};
}  // namespace gui
}  // namespace yaze

#endif