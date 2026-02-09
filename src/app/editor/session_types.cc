#include "app/editor/session_types.h"

#include "app/editor/code/assembly_editor.h"
#include "app/editor/code/memory_editor.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/editor.h"
#include "app/editor/graphics/graphics_editor.h"
#include "app/editor/graphics/screen_editor.h"
#include "app/editor/message/message_data.h"
#include "app/editor/message/message_editor.h"
#include "app/editor/music/music_editor.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/palette/palette_editor.h"
#include "app/editor/sprite/sprite_editor.h"
#include "app/editor/system/editor_registry.h"
#include "app/editor/system/user_settings.h"
#include "app/editor/ui/settings_panel.h"

namespace yaze::editor {

EditorSet::EditorSet(Rom* rom, zelda3::GameData* game_data,
                     UserSettings* user_settings, size_t session_id,
                     EditorRegistry* editor_registry)
    : session_id_(session_id), game_data_(game_data) {
  auto create_editor = [&](EditorType type,
                           auto&& fallback) -> std::unique_ptr<Editor> {
    if (editor_registry) {
      if (auto created = editor_registry->CreateEditor(type, rom)) {
        return created;
      }
    }
    return fallback();
  };

  editors_[EditorType::kAssembly] =
      create_editor(EditorType::kAssembly,
                    [&]() { return std::make_unique<AssemblyEditor>(rom); });
  editors_[EditorType::kDungeon] =
      create_editor(EditorType::kDungeon,
                    [&]() { return std::make_unique<DungeonEditorV2>(rom); });
  editors_[EditorType::kGraphics] =
      create_editor(EditorType::kGraphics,
                    [&]() { return std::make_unique<GraphicsEditor>(rom); });
  editors_[EditorType::kMusic] =
      create_editor(EditorType::kMusic,
                    [&]() { return std::make_unique<MusicEditor>(rom); });
  editors_[EditorType::kOverworld] =
      create_editor(EditorType::kOverworld,
                    [&]() { return std::make_unique<OverworldEditor>(rom); });
  editors_[EditorType::kPalette] =
      create_editor(EditorType::kPalette,
                    [&]() { return std::make_unique<PaletteEditor>(rom); });
  editors_[EditorType::kScreen] =
      create_editor(EditorType::kScreen,
                    [&]() { return std::make_unique<ScreenEditor>(rom); });
  editors_[EditorType::kSprite] =
      create_editor(EditorType::kSprite,
                    [&]() { return std::make_unique<SpriteEditor>(rom); });
  editors_[EditorType::kMessage] =
      create_editor(EditorType::kMessage,
                    [&]() { return std::make_unique<MessageEditor>(rom); });
  editors_[EditorType::kHex] =
      create_editor(EditorType::kHex,
                    [&]() { return std::make_unique<MemoryEditor>(rom); });
  editors_[EditorType::kSettings] =
      create_editor(EditorType::kSettings,
                    [&]() { return std::make_unique<SettingsPanel>(); });

  // Propagate game_data to editors that need it
  if (game_data) {
    GetDungeonEditor()->SetGameData(game_data);
    GetGraphicsEditor()->SetGameData(game_data);
    GetOverworldEditor()->SetGameData(game_data);
  }

  active_editors_ = {
      GetEditor(EditorType::kOverworld), GetEditor(EditorType::kDungeon),
      GetEditor(EditorType::kGraphics),  GetEditor(EditorType::kPalette),
      GetEditor(EditorType::kSprite),    GetEditor(EditorType::kMessage),
      GetEditor(EditorType::kMusic),     GetEditor(EditorType::kScreen),
      GetEditor(EditorType::kAssembly)};
}

EditorSet::~EditorSet() = default;

void EditorSet::set_user_settings(UserSettings* settings) {
  GetSettingsPanel()->SetUserSettings(settings);
}

void EditorSet::ApplyDependencies(const EditorDependencies& dependencies) {
  for (auto& [type, editor] : editors_) {
    editor->SetDependencies(dependencies);
  }
}

Editor* EditorSet::GetEditor(EditorType type) const {
  auto it = editors_.find(type);
  if (it != editors_.end()) {
    return it->second.get();
  }
  return nullptr;
}

// Deprecated named accessors
AssemblyEditor* EditorSet::GetAssemblyEditor() const {
  return GetEditorAs<AssemblyEditor>(EditorType::kAssembly);
}
DungeonEditorV2* EditorSet::GetDungeonEditor() const {
  return GetEditorAs<DungeonEditorV2>(EditorType::kDungeon);
}
GraphicsEditor* EditorSet::GetGraphicsEditor() const {
  return GetEditorAs<GraphicsEditor>(EditorType::kGraphics);
}
MusicEditor* EditorSet::GetMusicEditor() const {
  return GetEditorAs<MusicEditor>(EditorType::kMusic);
}
OverworldEditor* EditorSet::GetOverworldEditor() const {
  return GetEditorAs<OverworldEditor>(EditorType::kOverworld);
}
PaletteEditor* EditorSet::GetPaletteEditor() const {
  return GetEditorAs<PaletteEditor>(EditorType::kPalette);
}
ScreenEditor* EditorSet::GetScreenEditor() const {
  return GetEditorAs<ScreenEditor>(EditorType::kScreen);
}
SpriteEditor* EditorSet::GetSpriteEditor() const {
  return GetEditorAs<SpriteEditor>(EditorType::kSprite);
}
SettingsPanel* EditorSet::GetSettingsPanel() const {
  return GetEditorAs<SettingsPanel>(EditorType::kSettings);
}
MessageEditor* EditorSet::GetMessageEditor() const {
  return GetEditorAs<MessageEditor>(EditorType::kMessage);
}
MemoryEditor* EditorSet::GetMemoryEditor() const {
  return GetEditorAs<MemoryEditor>(EditorType::kHex);
}

RomSession::RomSession(Rom&& r, UserSettings* user_settings, size_t session_id,
                       EditorRegistry* editor_registry)
    : rom(std::move(r)),
      game_data(&rom),
      editors(&rom, &game_data, user_settings, session_id, editor_registry) {
  filepath = rom.filename();
  feature_flags = core::FeatureFlags::Flags{};
}

std::string RomSession::GetDisplayName() const {
  if (!custom_name.empty()) {
    return custom_name;
  }
  return rom.title().empty() ? "Untitled Session" : rom.title();
}

}  // namespace yaze::editor
