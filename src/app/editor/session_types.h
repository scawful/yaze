#ifndef YAZE_APP_EDITOR_SESSION_TYPES_H_
#define YAZE_APP_EDITOR_SESSION_TYPES_H_

#include <string>
#include <vector>

#include "app/editor/code/assembly_editor.h"
#include "app/editor/code/memory_editor.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/graphics/graphics_editor.h"
#include "app/editor/graphics/screen_editor.h"
#include "app/editor/message/message_editor.h"
#include "app/editor/music/music_editor.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/palette/palette_editor.h"
#include "app/editor/sprite/sprite_editor.h"
#include "app/editor/system/settings_editor.h"
#include "app/rom.h"
#include "core/features.h"

namespace yaze::editor {

class EditorDependencies;

/**
 * @class EditorSet
 * @brief Contains a complete set of editors for a single ROM instance
 */
class EditorSet {
 public:
  explicit EditorSet(Rom* rom = nullptr, UserSettings* user_settings = nullptr,
                     size_t session_id = 0);

  void set_user_settings(UserSettings* settings);

  void ApplyDependencies(const EditorDependencies& dependencies);

  size_t session_id() const { return session_id_; }

  AssemblyEditor assembly_editor_;
  DungeonEditorV2 dungeon_editor_;
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

 private:
  size_t session_id_ = 0;
};

/**
 * @struct RomSession
 * @brief Represents a single session, containing a ROM and its associated
 * editors.
 */
struct RomSession {
  Rom rom;
  EditorSet editors;
  std::string custom_name;  // User-defined session name
  std::string filepath;     // ROM filepath for duplicate detection
  core::FeatureFlags::Flags feature_flags;  // Per-session feature flags

  RomSession() = default;
  explicit RomSession(Rom&& r, UserSettings* user_settings = nullptr,
                      size_t session_id = 0);

  // Get display name (custom name or ROM title)
  std::string GetDisplayName() const;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_SESSION_TYPES_H_
