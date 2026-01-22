#ifndef YAZE_APP_EDITOR_SESSION_TYPES_H_
#define YAZE_APP_EDITOR_SESSION_TYPES_H_

#include <array>
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
#include "app/editor/ui/settings_panel.h"
#include "rom/rom.h"
#include "zelda3/game_data.h"
#include "core/features.h"

namespace yaze::editor {

class EditorDependencies;

/**
 * @class EditorSet
 * @brief Contains a complete set of editors for a single ROM instance
 */
class EditorSet {
 public:
  explicit EditorSet(Rom* rom = nullptr, zelda3::GameData* game_data = nullptr, UserSettings* user_settings = nullptr,
                     size_t session_id = 0);
  ~EditorSet();

  void set_user_settings(UserSettings* settings);

  void ApplyDependencies(const EditorDependencies& dependencies);

  size_t session_id() const { return session_id_; }

  // Accessors
  AssemblyEditor* GetAssemblyEditor() const { return assembly_editor_.get(); }
  DungeonEditorV2* GetDungeonEditor() const { return dungeon_editor_.get(); }
  GraphicsEditor* GetGraphicsEditor() const { return graphics_editor_.get(); }
  MusicEditor* GetMusicEditor() const { return music_editor_.get(); }
  OverworldEditor* GetOverworldEditor() const { return overworld_editor_.get(); }
  PaletteEditor* GetPaletteEditor() const { return palette_editor_.get(); }
  ScreenEditor* GetScreenEditor() const { return screen_editor_.get(); }
  SpriteEditor* GetSpriteEditor() const { return sprite_editor_.get(); }
  SettingsPanel* GetSettingsPanel() const { return settings_panel_.get(); }
  MessageEditor* GetMessageEditor() const { return message_editor_.get(); }
  MemoryEditor* GetMemoryEditor() const { return memory_editor_.get(); }

  std::vector<Editor*> active_editors_;

 private:
  size_t session_id_ = 0;
  zelda3::GameData* game_data_ = nullptr;

  std::unique_ptr<AssemblyEditor> assembly_editor_;
  std::unique_ptr<DungeonEditorV2> dungeon_editor_;
  std::unique_ptr<GraphicsEditor> graphics_editor_;
  std::unique_ptr<MusicEditor> music_editor_;
  std::unique_ptr<OverworldEditor> overworld_editor_;
  std::unique_ptr<PaletteEditor> palette_editor_;
  std::unique_ptr<ScreenEditor> screen_editor_;
  std::unique_ptr<SpriteEditor> sprite_editor_;
  std::unique_ptr<SettingsPanel> settings_panel_;
  std::unique_ptr<MessageEditor> message_editor_;
  std::unique_ptr<MemoryEditor> memory_editor_;
};

/**
 * @struct RomSession
 * @brief Represents a single session, containing a ROM and its associated
 * editors.
 */
struct RomSession {
  Rom rom;
  zelda3::GameData game_data;
  EditorSet editors;
  std::string custom_name;  // User-defined session name
  std::string filepath;     // ROM filepath for duplicate detection
  core::FeatureFlags::Flags feature_flags;  // Per-session feature flags
  bool game_data_loaded = false;
  std::array<bool, kEditorTypeCount> editor_initialized{};
  std::array<bool, kEditorTypeCount> editor_assets_loaded{};

  RomSession() = default;
  explicit RomSession(Rom&& r, UserSettings* user_settings = nullptr,
                      size_t session_id = 0);

  // Get display name (custom name or ROM title)
  std::string GetDisplayName() const;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_SESSION_TYPES_H_
