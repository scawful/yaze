#ifndef YAZE_APPLICATION_VIEW_EDITOR_H
#define YAZE_APPLICATION_VIEW_EDITOR_H

#include <memory>

#include "Core/Icons.h"
#include "Data/Overworld.h"
#include "OverworldEditor.h"
#include "ImGuiFileDialog/ImGuiFileDialog.h"
#include "Utils/ROM.h"
#include "imgui/backends/imgui_impl_sdl.h"
#include "imgui/backends/imgui_impl_sdlrenderer.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

namespace yaze {
namespace Application {
namespace Editor {

class Editor {
 public:
  void UpdateScreen();

 private:
  void DrawYazeMenu();
  void DrawFileMenu() const;
  void DrawEditMenu() const;
  void DrawViewMenu() const;

  void DrawOverworldEditor();
  void DrawDungeonEditor();
  void DrawScreenEditor();
  void DrawROMInfo();

  bool isLoaded = false;
  bool doneLoaded = false;
  GLuint *overworld_texture;
  Data::Overworld overworld;
  ::yaze::Application::Editor::OverworldEditor owEditor;
  Utils::ROM rom;


  ImGuiTableFlags toolset_table_flags = ImGuiTableFlags_SizingFixedFit;
};

}  // namespace View
}  // namespace Application
}  // namespace yaze

#endif  // YAZE_APPLICATION_VIEW_EDITOR_H