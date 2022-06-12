#ifndef YAZE_APPLICATION_VIEW_EDITOR_H
#define YAZE_APPLICATION_VIEW_EDITOR_H

#include <memory>

#include "Core/Icons.h"
#include "ImGuiColorTextEdit/TextEditor.h"
#include "ImGuiFileDialog/ImGuiFileDialog.h"
#include "OverworldEditor.h"
#include "Utils/ROM.h"
#include "imgui/backends/imgui_impl_sdl.h"
#include "imgui/backends/imgui_impl_sdlrenderer.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_memory_editor.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

namespace yaze {
namespace Application {
namespace Editor {

class Editor {
 public:
  Editor();
  void UpdateScreen();

 private:
  void DrawYazeMenu();
  void DrawFileMenu() const;
  void DrawEditMenu() const;
  void DrawViewMenu();
  void DrawHelpMenu() const;

  void DrawOverworldEditor();
  void DrawDungeonEditor();
  void DrawGraphicsEditor();
  void DrawSpriteEditor();
  void DrawScreenEditor();
  void DrawROMInfo();

  void *rom_data_;
  bool isLoaded = true;

  std::string title_ = "YAZE";

  Utils::ROM rom;
  TextEditor asm_editor_;
  TextEditor::LanguageDefinition language65816Def;
  OverworldEditor overworld_editor_;

  ImGuiTableFlags toolset_table_flags = ImGuiTableFlags_SizingFixedFit;
};

}  // namespace Editor
}  // namespace Application
}  // namespace yaze

#endif  // YAZE_APPLICATION_VIEW_EDITOR_H