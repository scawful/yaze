#include "app/editor/session_types.h"

#include <algorithm>

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
    : session_id_(session_id),
      rom_(rom),
      game_data_(game_data),
      user_settings_(user_settings),
      editor_registry_(editor_registry) {
  auto register_factory = [&](EditorType type, auto&& fallback) {
    editor_factories_[type] =
        [this, type,
         fallback = std::forward<decltype(fallback)>(
             fallback)]() mutable -> std::unique_ptr<Editor> {
      if (editor_registry_) {
        if (auto created = editor_registry_->CreateEditor(type, rom_)) {
          return created;
        }
      }
      return fallback();
    };
  };

  register_factory(EditorType::kAssembly,
                   [this]() { return std::make_unique<AssemblyEditor>(rom_); });
  register_factory(EditorType::kDungeon, [this]() {
    return std::make_unique<DungeonEditorV2>(rom_);
  });
  register_factory(EditorType::kGraphics,
                   [this]() { return std::make_unique<GraphicsEditor>(rom_); });
  register_factory(EditorType::kMusic,
                   [this]() { return std::make_unique<MusicEditor>(rom_); });
  register_factory(EditorType::kOverworld, [this]() {
    return std::make_unique<OverworldEditor>(rom_);
  });
  register_factory(EditorType::kPalette,
                   [this]() { return std::make_unique<PaletteEditor>(rom_); });
  register_factory(EditorType::kScreen,
                   [this]() { return std::make_unique<ScreenEditor>(rom_); });
  register_factory(EditorType::kSprite,
                   [this]() { return std::make_unique<SpriteEditor>(rom_); });
  register_factory(EditorType::kMessage,
                   [this]() { return std::make_unique<MessageEditor>(rom_); });
  register_factory(EditorType::kHex,
                   [this]() { return std::make_unique<MemoryEditor>(rom_); });
  register_factory(EditorType::kSettings,
                   []() { return std::make_unique<SettingsPanel>(); });
}

EditorSet::~EditorSet() = default;

void EditorSet::set_user_settings(UserSettings* settings) {
  user_settings_ = settings;
  if (auto* panel = GetSettingsPanel()) {
    panel->SetUserSettings(settings);
  }
}

void EditorSet::SetGameData(zelda3::GameData* game_data) {
  game_data_ = game_data;
  for (auto& [type, editor] : editors_) {
    (void)type;
    editor->SetGameData(game_data_);
  }
}

void EditorSet::ApplyDependencies(const EditorDependencies& dependencies) {
  dependencies_ = dependencies;
  for (auto& [type, editor] : editors_) {
    editor->SetDependencies(dependencies);
  }
}

Editor* EditorSet::GetEditor(EditorType type) const {
  return EnsureEditorCreated(type);
}

Editor* EditorSet::FindEditor(EditorType type) const {
  if (type == EditorType::kUnknown) {
    return nullptr;
  }

  if (auto it = editors_.find(type); it != editors_.end()) {
    return it->second.get();
  }
  return nullptr;
}

Editor* EditorSet::EnsureEditorCreated(EditorType type) const {
  if (type == EditorType::kUnknown) {
    return nullptr;
  }

  if (auto* editor = FindEditor(type)) {
    return editor;
  }

  auto factory_it = editor_factories_.find(type);
  if (factory_it == editor_factories_.end()) {
    return nullptr;
  }

  auto editor = factory_it->second();
  if (!editor) {
    return nullptr;
  }

  if (dependencies_.has_value()) {
    editor->SetDependencies(*dependencies_);
  }
  if (game_data_ != nullptr) {
    editor->SetGameData(game_data_);
  }
  if (type == EditorType::kSettings && user_settings_ != nullptr) {
    if (auto* settings_panel = static_cast<SettingsPanel*>(editor.get())) {
      settings_panel->SetUserSettings(user_settings_);
    }
  }

  Editor* editor_ptr = editor.get();
  editors_[type] = std::move(editor);

  if (ShouldTrackAsActiveEditor(type)) {
    const_cast<EditorSet*>(this)->TrackActiveEditor(editor_ptr);
  }

  return editor_ptr;
}

bool EditorSet::ShouldTrackAsActiveEditor(EditorType type) const {
  switch (type) {
    case EditorType::kAssembly:
    case EditorType::kDungeon:
    case EditorType::kGraphics:
    case EditorType::kMessage:
    case EditorType::kMusic:
    case EditorType::kOverworld:
    case EditorType::kPalette:
    case EditorType::kScreen:
    case EditorType::kSprite:
      return true;
    default:
      return false;
  }
}

void EditorSet::TrackActiveEditor(Editor* editor) {
  if (editor == nullptr) {
    return;
  }
  if (std::find(active_editors_.begin(), active_editors_.end(), editor) ==
      active_editors_.end()) {
    active_editors_.push_back(editor);
  }
}

void EditorSet::OpenAssemblyFolder(const std::string& folder_path) const {
  if (auto* editor = GetAssemblyEditor()) {
    editor->OpenFolder(folder_path);
  }
}

void EditorSet::ChangeActiveAssemblyFile(std::string_view path) const {
  if (auto* editor = GetAssemblyEditor()) {
    editor->ChangeActiveFile(path);
  }
}

core::AsarWrapper* EditorSet::GetAsarWrapper() const {
  if (auto* editor =
          static_cast<AssemblyEditor*>(FindEditor(EditorType::kAssembly))) {
    return editor->asar_wrapper();
  }
  return nullptr;
}

int EditorSet::LoadedDungeonRoomCount() const {
  if (auto* editor =
          static_cast<DungeonEditorV2*>(FindEditor(EditorType::kDungeon))) {
    return editor->LoadedRoomCount();
  }
  return 0;
}

int EditorSet::TotalDungeonRoomCount() const {
  if (auto* editor =
          static_cast<DungeonEditorV2*>(FindEditor(EditorType::kDungeon))) {
    return editor->TotalRoomCount();
  }
  return 0;
}

std::vector<std::pair<uint32_t, uint32_t>>
EditorSet::CollectDungeonWriteRanges() const {
  if (auto* editor =
          static_cast<DungeonEditorV2*>(FindEditor(EditorType::kDungeon))) {
    return editor->CollectWriteRanges();
  }
  return {};
}

zelda3::Overworld* EditorSet::GetOverworldData() const {
  if (auto* editor =
          static_cast<OverworldEditor*>(FindEditor(EditorType::kOverworld))) {
    return &editor->overworld();
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
