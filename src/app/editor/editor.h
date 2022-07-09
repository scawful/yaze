#ifndef YAZE_APP_VIEW_EDITOR_H
#define YAZE_APP_VIEW_EDITOR_H

#include <ImGuiColorTextEdit/TextEditor.h>
#include <ImGuiFileDialog/ImGuiFileDialog.h>
#include <imgui/imgui.h>
#include <imgui/imgui_memory_editor.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include "app/core/constants.h"
#include "app/editor/overworld_editor.h"
#include "app/gfx/tile.h"
#include "app/editor/assembly_editor.h"
#include "app/rom.h"
#include "gui/icons.h"
#include "gui/input.h"

namespace yaze {
namespace app {
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

  void DrawGraphicsSheet(int offset = 0);

  void DrawProjectEditor();
  void DrawOverworldEditor();
  void DrawDungeonEditor();
  void DrawgfxEditor();
  void DrawSpriteEditor();
  void DrawScreenEditor();
  void DrawHUDEditor();

  bool is_loaded_ = true;
  bool asm_is_loaded = false;

  rom::ROM rom_;
  gfx::TilePreset current_set_;

  TextEditor asm_editor_;
  AssemblyEditor assembly_editor_;
  OverworldEditor overworld_editor_;
  std::shared_ptr<SDL_Renderer> sdl_renderer_;
  std::unordered_map<uint, SDL_Texture *> image_cache_;

  ImVec4 current_palette_[8];

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