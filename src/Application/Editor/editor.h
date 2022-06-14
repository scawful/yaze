#ifndef YAZE_APPLICATION_VIEW_EDITOR_H
#define YAZE_APPLICATION_VIEW_EDITOR_H

#include <ImGuiColorTextEdit/TextEditor.h>
#include <ImGuiFileDialog/ImGuiFileDialog.h>
#include <imgui/imgui.h>
#include <imgui/imgui_memory_editor.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include "Core/constants.h"
#include "Core/input.h"
#include "Data/rom.h"
#include "Editor/overworld_editor.h"
#include "Graphics/icons.h"

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

  void DrawProjectEditor();
  void DrawOverworldEditor();
  void DrawDungeonEditor();
  void DrawGraphicsEditor();
  void DrawSpriteEditor();
  void DrawScreenEditor();

  void *rom_data_;
  bool is_loaded_ = true;

  std::vector<tile8> tiles_;

  Data::ROM rom_;
  TextEditor asm_editor_;
  TextEditor::LanguageDefinition language_65816_;
  OverworldEditor overworld_editor_;

  Graphics::Scene current_scene_;
  Graphics::SNESPalette current_palette_;
  Graphics::TilePreset current_set_;

  ImGuiWindowFlags main_editor_flags_ =
      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar;
  ImGuiTableFlags toolset_table_flags_ = ImGuiTableFlags_SizingFixedFit;
};

}  // namespace Editor
}  // namespace Application
}  // namespace yaze

#endif  // YAZE_APPLICATION_VIEW_EDITOR_H