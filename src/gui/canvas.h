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

  void Update();

 private:
  bool enable_grid_ = true;
  bool enable_context_menu_ = true;
  bool dragging_select_ = false;

  std::string title_;

  ImVector<ImVec2> points_;
  ImVec2 scrolling_;
};
}  // namespace gui
}  // namespace yaze

#endif