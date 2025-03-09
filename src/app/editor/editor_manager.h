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
#include "yaze_config.h"

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
class EditorManager : public SharedRom {
 public:
  EditorManager() {
    current_editor_ = &overworld_editor_;
    active_editors_ = {&overworld_editor_, &dungeon_editor_, &graphics_editor_,
                       &palette_editor_,   &sprite_editor_,  &message_editor_,
                       &screen_editor_,    &settings_editor_};
    std::stringstream ss;
    ss << YAZE_VERSION_MAJOR << "." << YAZE_VERSION_MINOR << "."
       << YAZE_VERSION_PATCH;
    ss >> version_;
  }

  void Initialize(const std::string &filename = "");
  absl::Status Update();

  auto emulator() -> emu::Emulator & { return emulator_; }
  auto quit() const { return quit_; }

 private:
  void ManageActiveEditors();

  void DrawPopups();
  void DrawHomepage();

  void DrawMenuBar();

  void LoadRom();
  void SaveRom();

  void OpenRomOrProject(const std::string &filename);
  absl::Status OpenProject();

  bool quit_ = false;
  bool about_ = false;
  bool rom_info_ = false;
  bool backup_rom_ = false;
  bool save_new_auto_ = true;
  bool save_as_menu_ = false;
  bool show_emulator_ = false;
  bool show_memory_editor_ = false;
  bool show_asm_editor_ = false;
  bool show_imgui_metrics_ = false;
  bool show_imgui_demo_ = false;
  bool show_palette_editor_ = false;
  bool show_resource_label_manager = false;
  bool open_supported_features = false;
  bool open_rom_help = false;
  bool open_manage_project = false;
  bool rom_assets_loaded_ = false;

  std::string version_ = "";

  absl::Status status_;
  emu::Emulator emulator_;
  std::vector<Editor *> active_editors_;
  std::vector<std::unique_ptr<Rom>> roms_;
  Rom *current_rom_ = nullptr;

  Project current_project_;
  EditorContext context_;

  Editor *current_editor_ = nullptr;
  AssemblyEditor assembly_editor_;
  DungeonEditor dungeon_editor_;
  GraphicsEditor graphics_editor_;
  MusicEditor music_editor_;
  OverworldEditor overworld_editor_{*rom()};
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
