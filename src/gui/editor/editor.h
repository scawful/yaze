#ifndef YAZE_APP_VIEW_EDITOR_H
#define YAZE_APP_VIEW_EDITOR_H

#include <ImGuiColorTextEdit/TextEditor.h>
#include <ImGuiFileDialog/ImGuiFileDialog.h>
#include <imgui/imgui.h>
#include <imgui/imgui_memory_editor.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include "core/constants.h"
#include "gfx/tile.h"
#include "gui/editor/overworld_editor.h"
#include "gui/icons.h"
#include "gui/input.h"
#include "rom.h"

namespace yaze {
namespace gui {
namespace editor {

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

  void DrawgfxSheet(int offset = 0);

  void DrawProjectEditor();
  void DrawOverworldEditor();
  void DrawDungeonEditor();
  void DrawgfxEditor();
  void DrawSpriteEditor();
  void DrawScreenEditor();
  void DrawHUDEditor();

  void *rom_data_;
  bool is_loaded_ = true;

  app::ROM rom_;
  TextEditor asm_editor_;
  TextEditor::LanguageDefinition language_65816_;
  OverworldEditor overworld_editor_;

  std::vector<tile8> tiles_;
  std::vector<std::vector<tile8>> arranged_tiles_;
  std::unordered_map<uint, SDL_Texture *> imagesCache;
  std::shared_ptr<SDL_Renderer> sdl_renderer_;

  ImVec4 current_palette_[8];
  app::gfx::TilePreset current_set_;

  ImGuiWindowFlags main_editor_flags_ =
      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar |
      ImGuiWindowFlags_NoTitleBar;
  ImGuiTableFlags toolset_table_flags_ = ImGuiTableFlags_SizingFixedFit;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_VIEW_EDITOR_H