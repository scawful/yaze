#ifndef YAZE_APPLICATION_VIEW_EDITOR_H
#define YAZE_APPLICATION_VIEW_EDITOR_H

#include <ImGuiColorTextEdit/TextEditor.h>
#include <ImGuiFileDialog/ImGuiFileDialog.h>
#include <imgui/imgui.h>
#include <imgui/imgui_memory_editor.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include <memory>

#include "Core/Constants.h"
#include "Core/Icons.h"
#include "OverworldEditor.h"
#include "Utils/ROM.h"

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
  bool isLoaded = true;

  Utils::ROM rom;
  TextEditor asm_editor_;
  TextEditor::LanguageDefinition language65816Def;
  OverworldEditor overworld_editor_;

  Graphics::Scene current_scene_;
  Graphics::SNESPalette current_palette_;
  Graphics::TilePreset current_set_;

  std::vector<tile8> tiles_;

  ImGuiTableFlags toolset_table_flags = ImGuiTableFlags_SizingFixedFit;
};

}  // namespace Editor
}  // namespace Application
}  // namespace yaze

#endif  // YAZE_APPLICATION_VIEW_EDITOR_H