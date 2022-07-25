#ifndef YAZE_APP_EDITOR_DUNGEONEDITOR_H
#define YAZE_APP_EDITOR_DUNGEONEDITOR_H

#include <imgui/imgui.h>

#include "gui/canvas.h"
#include "gui/icons.h"

namespace yaze {
namespace app {
namespace editor {
class DungeonEditor {
 public:
  void Update();

 private:
  void DrawToolset();

  gui::Canvas canvas_;
  ImGuiTableFlags toolset_table_flags_ = ImGuiTableFlags_SizingFixedFit;
};
}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif
