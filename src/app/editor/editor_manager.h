#ifndef YAZE_APP_EDITOR_EDITOR_MANAGER_H
#define YAZE_APP_EDITOR_EDITOR_MANAGER_H

#define IMGUI_DEFINE_MATH_OPERATORS

#include "ImGuiColorTextEdit/TextEditor.h"
#include "ImGuiFileDialog/ImGuiFileDialog.h"
#include "absl/status/status.h"
#include "app/core/common.h"
#include "app/core/constants.h"
#include "app/core/project.h"
#include "app/editor/code/assembly_editor.h"
#include "app/editor/code/memory_editor.h"
#include "app/editor/dungeon/dungeon_editor.h"
#include "app/editor/graphics/graphics_editor.h"
#include "app/editor/graphics/palette_editor.h"
#include "app/editor/graphics/screen_editor.h"
#include "app/editor/message/message_editor.h"
#include "app/editor/music/music_editor.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/settings_editor.h"
#include "app/editor/sprite/sprite_editor.h"
#include "app/editor/system/extension_manager.h"
#include "app/editor/utils/gfx_context.h"
#include "app/emu/emulator.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/rom.h"
#include "ext/extension.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "imgui_memory_editor.h"
#include "yaze.h"

namespace yaze {
namespace app {
namespace editor {

/**
 * @class EditorManager
 * @brief The EditorManager class represents the main editor for a Rom in the
 * Yaze application.
 *
 * This class inherits from SharedRom, GfxContext, and ExperimentFlags, and
 * provides functionality for setting up the screen, updating the editor, and
 * shutting down the editor. It also includes methods for drawing various menus
 * and popups, saving the Rom, and managing editor-specific flags.
 *
 * The EditorManager class contains instances of various editor classes such as
 * AssemblyEditor, DungeonEditor, GraphicsEditor, MusicEditor, OverworldEditor,
 * PaletteEditor, ScreenEditor, and SpriteEditor. The current_editor_ member
 * variable points to the currently active editor in the tab view.
 *
 * @note This class assumes the presence of an SDL_Renderer object for rendering
 * graphics.
 */
class EditorManager : public SharedRom,
                      public context::GfxContext,
                      public core::ExperimentFlags {
 public:
  EditorManager() {
    current_editor_ = &overworld_editor_;
    active_editors_.push_back(&overworld_editor_);
    active_editors_.push_back(&dungeon_editor_);
    active_editors_.push_back(&graphics_editor_);
    active_editors_.push_back(&palette_editor_);
    active_editors_.push_back(&sprite_editor_);
    active_editors_.push_back(&message_editor_);
  }

  void SetupScreen(std::string filename = "");
  absl::Status Update();

  auto emulator() -> emu::Emulator& { return emulator_; }
  auto quit() { return quit_; }
  auto overworld_editor() -> OverworldEditor& { return overworld_editor_; }

 private:
  void ManageActiveEditors();
  void ManageKeyboardShortcuts();
  void OpenRomOrProject(const std::string& filename);

  void DrawFileDialog();
  void DrawStatusPopup();
  void DrawAboutPopup();
  void DrawInfoPopup();

  void DrawYazeMenu();
  void DrawYazeMenuBar();

  void LoadRom();
  void SaveRom();

  absl::Status OpenProject();

  bool quit_ = false;
  bool about_ = false;
  bool rom_info_ = false;
  bool backup_rom_ = false;
  bool save_new_auto_ = true;
  bool show_status_ = false;
  bool rom_assets_loaded_ = false;

  absl::Status status_;
  absl::Status prev_status_;

  emu::Emulator emulator_;

  Project current_project_;
  yaze_editor_context editor_context_;
  ExtensionManager extension_manager_;

  AssemblyEditor assembly_editor_;
  DungeonEditor dungeon_editor_;
  GraphicsEditor graphics_editor_;
  MusicEditor music_editor_;
  OverworldEditor overworld_editor_;
  PaletteEditor palette_editor_;
  ScreenEditor screen_editor_;
  SpriteEditor sprite_editor_;
  SettingsEditor settings_editor_;
  MessageEditor message_editor_;
  MemoryEditorWithDiffChecker memory_editor_;

  ImVector<int> active_tabs_;
  std::vector<Editor*> active_editors_;
  Editor* current_editor_ = nullptr;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_EDITOR_MANAGER_H
