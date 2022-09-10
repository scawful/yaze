#ifndef YAZE_APP_DELTA_VIEWER_H
#define YAZE_APP_DELTA_VIEWER_H

#include <ImGuiColorTextEdit/TextEditor.h>
#include <ImGuiFileDialog/ImGuiFileDialog.h>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <imgui_memory_editor.h>

#include "absl/status/status.h"
#include "app/core/constants.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"
#include "gui/canvas.h"
#include "gui/icons.h"
#include "gui/input.h"

namespace yaze {
namespace app {
namespace delta {
class Viewer {
 public:
  void Update();

 private:
  void DrawBranchTree();
};
}  // namespace delta
}  // namespace app
}  // namespace yaze

#endif