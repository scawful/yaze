#ifndef YAZE_APP_EDITOR_MASTER_EDITOR_H
#define YAZE_APP_EDITOR_MASTER_EDITOR_H

#include <ImGuiColorTextEdit/TextEditor.h>
#include <ImGuiFileDialog/ImGuiFileDialog.h>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <imgui_memory_editor.h>

#include "absl/status/status.h"
#include "app/core/constants.h"
#include "app/core/emulator.h"
#include "app/core/pipeline.h"
#include "app/editor/assembly_editor.h"
#include "app/editor/dungeon_editor.h"
#include "app/editor/graphics_editor.h"
#include "app/editor/music_editor.h"
#include "app/editor/overworld_editor.h"
#include "app/editor/palette_editor.h"
#include "app/editor/screen_editor.h"
#include "app/editor/sprite_editor.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace editor {

class MasterEditor : public SharedROM {
 public:
  void SetupScreen(std::shared_ptr<SDL_Renderer> renderer);
  void UpdateScreen();

 private:
  void DrawFileDialog();
  void DrawStatusPopup();
  void DrawAboutPopup();
  void DrawInfoPopup();

  void DrawYazeMenu();
  void DrawFileMenu();
  void DrawEditMenu();
  void DrawViewMenu();
  void DrawHelpMenu();

  void DrawOverworldEditor();
  void DrawDungeonEditor();
  void DrawGraphicsEditor();
  void DrawPaletteEditor();
  void DrawMusicEditor();
  void DrawScreenEditor();
  void DrawSpriteEditor();

  bool about_ = false;
  bool rom_info_ = false;
  bool backup_rom_ = true;
  bool show_status_ = false;

  absl::Status status_;
  absl::Status prev_status_;

  std::shared_ptr<SDL_Renderer> sdl_renderer_;

  core::Emulator emulator_;
  AssemblyEditor assembly_editor_;
  DungeonEditor dungeon_editor_;
  GraphicsEditor graphics_editor_;
  MusicEditor music_editor_;
  OverworldEditor overworld_editor_;
  PaletteEditor palette_editor_;
  ScreenEditor screen_editor_;
  SpriteEditor sprite_editor_;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MASTER_EDITOR_H