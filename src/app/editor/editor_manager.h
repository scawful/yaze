#ifndef YAZE_APP_EDITOR_EDITOR_MANAGER_H
#define YAZE_APP_EDITOR_EDITOR_MANAGER_H

#define IMGUI_DEFINE_MATH_OPERATORS

#include <deque>
#include <vector>

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
#include "app/editor/system/popup_manager.h"
#include "app/editor/system/settings_editor.h"
#include "app/emu/emulator.h"
#include "app/rom.h"
#include "yaze_config.h"

namespace yaze {
namespace editor {

/**
 * @class EditorSet
 * @brief Contains a complete set of editors for a single ROM instance
 */
class EditorSet {
 public:
  explicit EditorSet(Rom* rom = nullptr)
      : assembly_editor_(rom),
        dungeon_editor_(rom),
        graphics_editor_(rom),
        music_editor_(rom),
        overworld_editor_(rom),
        palette_editor_(rom),
        screen_editor_(rom),
        sprite_editor_(rom),
        settings_editor_(rom),
        message_editor_(rom),
        memory_editor_(rom) {
    active_editors_ = {&overworld_editor_, &dungeon_editor_, &graphics_editor_,
                       &palette_editor_,   &sprite_editor_,  &message_editor_,
                       &music_editor_,     &screen_editor_,  &settings_editor_,
                       &assembly_editor_};
  }

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

  std::vector<Editor*> active_editors_;
};

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
class EditorManager {
 public:
  EditorManager() {
    std::stringstream ss;
    ss << YAZE_VERSION_MAJOR << "." << YAZE_VERSION_MINOR << "."
       << YAZE_VERSION_PATCH;
    ss >> version_;
    context_.popup_manager = popup_manager_.get();
  }

  void Initialize(const std::string& filename = "");
  absl::Status Update();
  void DrawMenuBar();

  auto emulator() -> emu::Emulator& { return emulator_; }
  auto quit() const { return quit_; }
  auto version() const { return version_; }

  absl::Status SetCurrentRom(Rom* rom);
  auto GetCurrentRom() -> Rom* { return current_rom_; }
  auto GetCurrentEditorSet() -> EditorSet* { return current_editor_set_; }

 private:
  void DrawHomepage();
  absl::Status DrawRomSelector();
  absl::Status LoadRom();
  absl::Status LoadAssets();
  absl::Status SaveRom();
  absl::Status OpenRomOrProject(const std::string& filename);
  absl::Status OpenProject();
  absl::Status SaveProject();

  bool quit_ = false;
  bool backup_rom_ = false;
  bool save_new_auto_ = true;
  bool new_project_menu = false;

  bool show_emulator_ = false;
  bool show_memory_editor_ = false;
  bool show_asm_editor_ = false;
  bool show_imgui_metrics_ = false;
  bool show_imgui_demo_ = false;
  bool show_palette_editor_ = false;
  bool show_resource_label_manager = false;
  bool show_workspace_layout = false;
  bool show_homepage_ = true;

  std::string version_ = "";

  absl::Status status_;
  emu::Emulator emulator_;

  struct RomSession {
    Rom rom;
    EditorSet editors;

    RomSession() = default;
    explicit RomSession(Rom&& r)
        : rom(std::move(r)), editors(&rom) {}
  };

  std::deque<RomSession> sessions_;
  Rom* current_rom_ = nullptr;
  EditorSet* current_editor_set_ = nullptr;
  Editor* current_editor_ = nullptr;
  EditorSet blank_editor_set_{};

  Project current_project_;
  EditorContext context_;
  std::unique_ptr<PopupManager> popup_manager_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_EDITOR_MANAGER_H
