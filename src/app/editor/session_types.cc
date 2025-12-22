#include "app/editor/session_types.h"

#include "app/editor/editor.h"  // For EditorDependencies, needed by ApplyDependencies
#include "app/editor/system/user_settings.h"  // For UserSettings forward decl in header

namespace yaze::editor {

EditorSet::EditorSet(Rom* rom, zelda3::GameData* game_data,
                     UserSettings* user_settings, size_t session_id)
    : session_id_(session_id), game_data_(game_data) {
  assembly_editor_ = std::make_unique<AssemblyEditor>(rom);
  dungeon_editor_ = std::make_unique<DungeonEditorV2>(rom);
  graphics_editor_ = std::make_unique<GraphicsEditor>(rom);
  music_editor_ = std::make_unique<MusicEditor>(rom);
  overworld_editor_ = std::make_unique<OverworldEditor>(rom);
  palette_editor_ = std::make_unique<PaletteEditor>(rom);
  screen_editor_ = std::make_unique<ScreenEditor>(rom);
  sprite_editor_ = std::make_unique<SpriteEditor>(rom);
  message_editor_ = std::make_unique<MessageEditor>(rom);
  memory_editor_ = std::make_unique<MemoryEditor>(rom);
  settings_panel_ = std::make_unique<SettingsPanel>();

  // Propagate game_data to editors that need it
  if (game_data) {
    dungeon_editor_->SetGameData(game_data);
    graphics_editor_->SetGameData(game_data);
    overworld_editor_->SetGameData(game_data);
  }

  active_editors_ = {overworld_editor_.get(), dungeon_editor_.get(),
                     graphics_editor_.get(),  palette_editor_.get(),
                     sprite_editor_.get(),    message_editor_.get(),
                     music_editor_.get(),     screen_editor_.get(),
                     assembly_editor_.get()};
}

EditorSet::~EditorSet() = default;

void EditorSet::set_user_settings(UserSettings* settings) {
  settings_panel_->SetUserSettings(settings);
}

void EditorSet::ApplyDependencies(const EditorDependencies& dependencies) {
  for (auto* editor : active_editors_) {
    editor->SetDependencies(dependencies);
  }
  memory_editor_->SetRom(dependencies.rom);
  if (music_editor_) {
    music_editor_->SetProject(dependencies.project);
  }

  // MusicEditor needs emulator for audio playback
  if (dependencies.emulator) {
    music_editor_->set_emulator(dependencies.emulator);
  }

  // Configure SettingsPanel
  if (settings_panel_) {
    settings_panel_->SetRom(dependencies.rom);
    settings_panel_->SetUserSettings(dependencies.user_settings);
    settings_panel_->SetPanelRegistry(dependencies.panel_manager);
    settings_panel_->SetShortcutManager(dependencies.shortcut_manager);
  }
}

RomSession::RomSession(Rom&& r, UserSettings* user_settings, size_t session_id)
    : rom(std::move(r)),
      game_data(&rom),
      editors(&rom, &game_data, user_settings, session_id) {
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
