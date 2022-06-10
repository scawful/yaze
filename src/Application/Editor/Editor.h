#ifndef YAZE_APPLICATION_VIEW_EDITOR_H
#define YAZE_APPLICATION_VIEW_EDITOR_H

#include <memory>

#include "Data/Overworld.h"
#include "ImGuiFileDialog/ImGuiFileDialog.h"
#include "Utils/ROM.h"
#include "imgui/backends/imgui_impl_sdl.h"
#include "imgui/backends/imgui_impl_sdlrenderer.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

namespace yaze {
namespace Application {
namespace View {

class Editor {
 public:
  void UpdateScreen();

 private:
  void DrawYazeMenu();
  void DrawFileMenu() const;
  void DrawEditMenu() const;

  void DrawOverworldEditor();

  Data::Overworld overworld;
  Utils::ROM rom;
};

}  // namespace View
}  // namespace Application
}  // namespace yaze

#endif  // YAZE_APPLICATION_VIEW_EDITOR_H