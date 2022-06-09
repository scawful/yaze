#ifndef YAZE_APPLICATION_VIEW_EDITOR_H
#define YAZE_APPLICATION_VIEW_EDITOR_H

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/backends/imgui_impl_sdl.h"
#include "imgui/backends/imgui_impl_sdlrenderer.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "ImGuiFileDialog/ImGuiFileDialog.h"

namespace yaze {
namespace Application {
namespace View {

class Editor {
 public:
  void UpdateScreen() const;

 private:
  void DrawYazeMenu() const;
  void DrawFileMenu() const;
  void DrawEditMenu() const;
};

}  // namespace View
}  // namespace Application
}  // namespace yaze

#endif  // YAZE_APPLICATION_VIEW_EDITOR_H