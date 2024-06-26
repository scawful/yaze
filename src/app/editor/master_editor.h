#ifndef YAZE_APP_EDITOR_MASTER_EDITOR_H
#define YAZE_APP_EDITOR_MASTER_EDITOR_H

#define IMGUI_DEFINE_MATH_OPERATORS 1

#include <ImGuiColorTextEdit/TextEditor.h>
#include <ImGuiFileDialog/ImGuiFileDialog.h>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <imgui_memory_editor.h>

#include "absl/status/status.h"
#include "app/core/common.h"
#include "app/core/constants.h"
#include "app/editor/context/gfx_context.h"
#include "app/editor/dungeon_editor.h"
#include "app/editor/graphics_editor.h"
#include "app/editor/modules/assembly_editor.h"
#include "app/editor/modules/music_editor.h"
#include "app/editor/modules/palette_editor.h"
#include "app/editor/overworld_editor.h"
#include "app/editor/screen_editor.h"
#include "app/editor/sprite_editor.h"
#include "app/emu/emulator.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/gui/pipeline.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace editor {

/**
 * @class MasterEditor
 * @brief The MasterEditor class represents the main editor for a Rom in the
 * Yaze application.
 *
 * This class inherits from SharedRom, GfxContext, and ExperimentFlags, and
 * provides functionality for setting up the screen, updating the editor, and
 * shutting down the editor. It also includes methods for drawing various menus
 * and popups, saving the Rom, and managing editor-specific flags.
 *
 * The MasterEditor class contains instances of various editor classes such as
 * AssemblyEditor, DungeonEditor, GraphicsEditor, MusicEditor, OverworldEditor,
 * PaletteEditor, ScreenEditor, and SpriteEditor. The current_editor_ member
 * variable points to the currently active editor in the tab view.
 *
 * @note This class assumes the presence of an SDL_Renderer object for rendering
 * graphics.
 */
class MasterEditor : public SharedRom,
                     public context::GfxContext,
                     public core::ExperimentFlags {
 public:
  MasterEditor() { current_editor_ = &overworld_editor_; }

  void SetupScreen(std::shared_ptr<SDL_Renderer> renderer,
                   std::string filename = "");
  absl::Status Update();

  auto emulator() -> emu::Emulator& { return emulator_; }
  auto quit() { return quit_; }

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

  void LoadRom();
  void SaveRom();

  bool quit_ = false;
  bool about_ = false;
  bool rom_info_ = false;
  bool backup_rom_ = false;
  bool save_new_auto_ = true;
  bool show_status_ = false;
  bool rom_assets_loaded_ = false;

  absl::Status status_;
  absl::Status prev_status_;

  std::shared_ptr<SDL_Renderer> sdl_renderer_;

  emu::Emulator emulator_;

  AssemblyEditor assembly_editor_;
  DungeonEditor dungeon_editor_;
  GraphicsEditor graphics_editor_;
  MusicEditor music_editor_;
  OverworldEditor overworld_editor_;
  PaletteEditor palette_editor_;
  ScreenEditor screen_editor_;
  SpriteEditor sprite_editor_;

  Editor* current_editor_ = nullptr;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MASTER_EDITOR_H