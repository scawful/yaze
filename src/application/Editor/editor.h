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
#include "Graphics/tile.h"

namespace yaze {
namespace application {
namespace Editor {

class Editor {
 public:
  Editor();
  ~Editor();
  void SetupScreen(std::shared_ptr<SDL_Renderer> renderer);
  void UpdateScreen();

 private:
  void DrawYazeMenu();
  void DrawFileMenu() const;
  void DrawEditMenu() const;
  void DrawViewMenu();
  void DrawHelpMenu() const;

  void DrawGraphicsSheet(int offset = 0);

  void DrawProjectEditor();
  void DrawOverworldEditor();
  void DrawDungeonEditor();
  void DrawGraphicsEditor();
  void DrawSpriteEditor();
  void DrawScreenEditor();
  void DrawHUDEditor();

  void *rom_data_;
  bool is_loaded_ = true;

  std::vector<tile8> tiles_;
  std::vector<std::vector<tile8>> arranged_tiles_;
  std::unordered_map<unsigned int, std::shared_ptr<SDL_Texture>> texture_cache_;
  std::unordered_map<unsigned int, SDL_Texture *> imagesCache;

  std::shared_ptr<SDL_Renderer> sdl_renderer_;

  Data::ROM rom_;
  TextEditor asm_editor_;
  TextEditor::LanguageDefinition language_65816_;
  OverworldEditor overworld_editor_;

  ImVec4 current_palette_[8];
  Graphics::TilePreset current_set_;

  ImGuiWindowFlags main_editor_flags_ =
      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar |
      ImGuiWindowFlags_NoTitleBar;
  ImGuiTableFlags toolset_table_flags_ = ImGuiTableFlags_SizingFixedFit;
};

}  // namespace Editor
}  // namespace application
}  // namespace yaze

#endif  // YAZE_APPLICATION_VIEW_EDITOR_H