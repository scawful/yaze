#ifndef YAZE_APP_EDITOR_SESSION_TYPES_H_
#define YAZE_APP_EDITOR_SESSION_TYPES_H_

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "app/editor/editor.h"
#include "core/features.h"
#include "rom/rom.h"
#include "zelda3/game_data.h"

namespace yaze::core {
class AsarWrapper;
}

namespace yaze::zelda3 {
class Overworld;
}

namespace yaze::editor {

class EditorDependencies;
class EditorRegistry;
class UserSettings;

// Forward declarations for legacy accessors
class AssemblyEditor;
class MemoryEditor;
class DungeonEditorV2;
class GraphicsEditor;
class ScreenEditor;
class MessageEditor;
class MusicEditor;
class OverworldEditor;
class PaletteEditor;
class SpriteEditor;
class SettingsPanel;

/**
 * @class EditorSet
 * @brief Contains a complete set of editors for a single ROM instance
 */
class EditorSet {
 public:
  explicit EditorSet(Rom* rom = nullptr, zelda3::GameData* game_data = nullptr,
                     UserSettings* user_settings = nullptr,
                     size_t session_id = 0,
                     EditorRegistry* editor_registry = nullptr);
  ~EditorSet();

  void set_user_settings(UserSettings* settings);

  void ApplyDependencies(const EditorDependencies& dependencies);

  size_t session_id() const { return session_id_; }

  // Generic accessors
  Editor* GetEditor(EditorType type) const;

  template <typename T>
  T* GetEditorAs(EditorType type) const {
    return static_cast<T*>(GetEditor(type));
  }

  // Convenience helpers that avoid requiring concrete editor headers.
  // Prefer these from higher-level systems (EditorManager, UI) to keep compile
  // dependencies minimal.
  void OpenAssemblyFolder(const std::string& folder_path) const;
  void ChangeActiveAssemblyFile(std::string_view path) const;
  core::AsarWrapper* GetAsarWrapper() const;
  int LoadedDungeonRoomCount() const;
  int TotalDungeonRoomCount() const;
  std::vector<std::pair<uint32_t, uint32_t>> CollectDungeonWriteRanges() const;
  zelda3::Overworld* GetOverworldData() const;

  // Deprecated named accessors (legacy compatibility)
  [[deprecated("Use GetEditorAs<AssemblyEditor>(EditorType::kAssembly)")]]
  AssemblyEditor* GetAssemblyEditor() const;
  [[deprecated("Use GetEditorAs<DungeonEditorV2>(EditorType::kDungeon)")]]
  DungeonEditorV2* GetDungeonEditor() const;
  [[deprecated("Use GetEditorAs<GraphicsEditor>(EditorType::kGraphics)")]]
  GraphicsEditor* GetGraphicsEditor() const;
  [[deprecated("Use GetEditorAs<MusicEditor>(EditorType::kMusic)")]]
  MusicEditor* GetMusicEditor() const;
  [[deprecated("Use GetEditorAs<OverworldEditor>(EditorType::kOverworld)")]]
  OverworldEditor* GetOverworldEditor() const;
  [[deprecated("Use GetEditorAs<PaletteEditor>(EditorType::kPalette)")]]
  PaletteEditor* GetPaletteEditor() const;
  [[deprecated("Use GetEditorAs<ScreenEditor>(EditorType::kScreen)")]]
  ScreenEditor* GetScreenEditor() const;
  [[deprecated("Use GetEditorAs<SpriteEditor>(EditorType::kSprite)")]]
  SpriteEditor* GetSpriteEditor() const;
  [[deprecated("Use GetEditorAs<SettingsPanel>(EditorType::kSettings)")]]
  SettingsPanel* GetSettingsPanel() const;
  [[deprecated("Use GetEditorAs<MessageEditor>(EditorType::kMessage)")]]
  MessageEditor* GetMessageEditor() const;
  [[deprecated("Use GetEditorAs<MemoryEditor>(EditorType::kHex)")]]
  MemoryEditor* GetMemoryEditor() const;

  std::vector<Editor*> active_editors_;

 private:
  size_t session_id_ = 0;
  zelda3::GameData* game_data_ = nullptr;

  std::unordered_map<EditorType, std::unique_ptr<Editor>> editors_;
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
                      size_t session_id = 0,
                      EditorRegistry* editor_registry = nullptr);

  // Get display name (custom name or ROM title)
  std::string GetDisplayName() const;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_SESSION_TYPES_H_
