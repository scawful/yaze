#ifndef YAZE_APP_EDITOR_MASTER_EDITOR_H
#define YAZE_APP_EDITOR_MASTER_EDITOR_H

#include <ImGuiColorTextEdit/TextEditor.h>
#include <ImGuiFileDialog/ImGuiFileDialog.h>
#include <imgui/imgui.h>
#include <imgui/imgui_memory_editor.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include "absl/status/status.h"
#include "app/core/constants.h"
#include "app/editor/assembly_editor.h"
#include "app/editor/dungeon_editor.h"
#include "app/editor/overworld_editor.h"
#include "app/editor/screen_editor.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"
#include "gui/canvas.h"
#include "gui/icons.h"
#include "gui/input.h"

namespace yaze {
namespace app {
namespace editor {

class MasterEditor {
 public:
  ~MasterEditor();
  void SetupScreen(std::shared_ptr<SDL_Renderer> renderer);
  void UpdateScreen();

 private:
  void DrawYazeMenu();
  void DrawFileMenu() const;
  void DrawEditMenu() const;
  void DrawViewMenu();
  void DrawHelpMenu() const;

  void DrawOverworldEditor();
  void DrawDungeonEditor();
  void DrawScreenEditor();
  void DrawSpriteEditor();

  ROM rom_;
  AssemblyEditor assembly_editor_;
  OverworldEditor overworld_editor_;
  DungeonEditor dungeon_editor_;
  ScreenEditor screen_editor_;

  absl::Status status_;

  ImVec4 current_palette_[8];

  ImGuiWindowFlags main_editor_flags_ =
      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar |
      ImGuiWindowFlags_NoTitleBar;
  ImGuiTableFlags toolset_table_flags_ = ImGuiTableFlags_SizingFixedFit;

  std::shared_ptr<SDL_Renderer> sdl_renderer_;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MASTER_EDITOR_H