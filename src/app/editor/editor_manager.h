#ifndef YAZE_APP_EDITOR_EDITOR_MANAGER_H
#define YAZE_APP_EDITOR_EDITOR_MANAGER_H

#define IMGUI_DEFINE_MATH_OPERATORS

#include "absl/status/status.h"
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
#include "app/editor/sprite/sprite_editor.h"
#include "app/editor/system/settings_editor.h"
#include "app/emu/emulator.h"
#include "app/gui/input.h"
#include "app/rom.h"

namespace yaze {
namespace editor {

/**
 * @class EditorManager
 * @brief The EditorManager controls the main editor window and manages the
 * various editor classes.
 *
 * The EditorManager class contains instances of various editor classes such as
 * AssemblyEditor, DungeonEditor, GraphicsEditor, MusicEditor, OverworldEditor,
 * PaletteEditor, ScreenEditor, and SpriteEditor. The current_editor_ member
 * variable points to the currently active editor in the tab view.
 *
 */
class EditorManager : public SharedRom, public core::ExperimentFlags {
 public:
  EditorManager() {
    current_editor_ = &overworld_editor_;
    active_editors_.push_back(&overworld_editor_);
    active_editors_.push_back(&dungeon_editor_);
    active_editors_.push_back(&graphics_editor_);
    active_editors_.push_back(&palette_editor_);
    active_editors_.push_back(&sprite_editor_);
    active_editors_.push_back(&message_editor_);
    active_editors_.push_back(&screen_editor_);
  }

  void SetupScreen(std::string filename = "");
  absl::Status Update();

  auto emulator() -> emu::Emulator & { return emulator_; }
  auto quit() { return quit_; }

 private:
  void ManageActiveEditors();
  void ManageKeyboardShortcuts();
  void InitializeCommands();

  void DrawStatusPopup();
  void DrawAboutPopup();
  void DrawInfoPopup();

  void DrawYazeMenu();
  void DrawYazeMenuBar();

  void LoadRom();
  void SaveRom();

  void OpenRomOrProject(const std::string &filename);
  absl::Status OpenProject();

  bool quit_ = false;
  bool about_ = false;
  bool rom_info_ = false;
  bool backup_rom_ = false;
  bool save_new_auto_ = true;
  bool show_status_ = false;
  bool rom_assets_loaded_ = false;

  absl::Status status_;
  emu::Emulator emulator_;
  std::vector<Editor *> active_editors_;

  Project current_project_;
  EditorContext editor_context_;

  Editor *current_editor_ = nullptr;
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
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_EDITOR_MANAGER_H
