#include "app/editor/session_types.h"

#include "zelda3/game_data.h"

#include <algorithm>
#include <filesystem>

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
#include "app/editor/planning/planning_editor.h"
#include "app/editor/shell/windows/settings_panel.h"
#include "app/editor/sprite/sprite_editor.h"
#include "app/editor/system/editor_registry.h"
#include "app/editor/system/session/user_settings.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/util/palette_manager.h"
#include "core/version_manager.h"

namespace yaze::editor {

EditorSet::EditorSet(Rom* rom, zelda3::GameData* game_data,
                     UserSettings* user_settings, size_t session_id,
                     EditorRegistry* editor_registry)
    : session_id_(session_id),
      rom_(rom),
      game_data_(game_data),
      user_settings_(user_settings),
      editor_registry_(editor_registry),
      gfx_group_workspace_(std::make_unique<GfxGroupWorkspaceState>()) {
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
  register_factory(EditorType::kPlanning,
                   []() { return std::make_unique<PlanningEditor>(); });
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

void EditorSet::PrepareForSessionTeardown() {
  if (session_teardown_prepared_) {
    return;
  }
  session_teardown_prepared_ = true;
  for (auto& [type, editor] : editors_) {
    (void)type;
    editor->PrepareForSessionTeardown();
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
  if (auto* dungeon = dynamic_cast<DungeonEditorV2*>(editor.get())) {
    dungeon->SetRoomGraphicsOverworldProvider(
        [this]() { return GetOverworldData(); });
  }
  auto main_usage =
      [this](int group,
             int slot) -> absl::StatusOr<zelda3::MainGroupUsageReport> {
    if (!rom_ || !game_data_ || session_teardown_prepared_) {
      return absl::FailedPreconditionError("No loaded graphics session");
    }
    auto rooms = zelda3::CollectRoomGfx(*rom_);
    std::map<int, uint8_t> explicit_main;
    if (const auto* dungeon = dynamic_cast<const DungeonEditorV2*>(
            GetExistingEditor(EditorType::kDungeon))) {
      dungeon->rooms().ForEachLoaded([&](int id, const zelda3::Room& room) {
        rooms[id] = {room.blockset(), room.spriteset()};
        explicit_main[id] = room.render_entrance_blockset();
      });
    }
    return zelda3::DescribeMainGroupUsage(*rom_, *game_data_, group, slot,
                                          rooms, explicit_main,
                                          GetOverworldData());
  };
  auto roomset_usage =
      [this](int group) -> absl::StatusOr<zelda3::RoomsetUsageReport> {
    if (!rom_ || !game_data_ || session_teardown_prepared_)
      return absl::FailedPreconditionError("No loaded graphics session");
    if (const auto* dungeon = dynamic_cast<const DungeonEditorV2*>(
            GetExistingEditor(EditorType::kDungeon)))
      return dungeon->GetRoomGraphicsUsage(group);
    return zelda3::DescribeRoomsetUsage(*rom_, *game_data_, group,
                                        zelda3::CollectRoomGfx(*rom_),
                                        GetOverworldData());
  };
  if (auto* graphics = dynamic_cast<GraphicsEditor*>(editor.get())) {
    graphics->SetRoomUsageProvider([this]() {
      usage_preview::LiveRoomContexts contexts;
      if (session_teardown_prepared_ || !rom_ || !game_data_)
        return contexts;
      if (auto* dungeon = static_cast<DungeonEditorV2*>(
              GetExistingEditor(EditorType::kDungeon))) {
        dungeon->VisitGraphicsUsageRooms([&](int id, zelda3::Room& room,
                                             const std::string& provenance,
                                             bool current, int entrance_slot) {
          contexts.emplace(id, usage_preview::LiveRoomContext{
                                   &room, provenance, current, entrance_slot});
        });
      }
      return contexts;
    });
    graphics->SetOverworldUsageProvider([this]() -> zelda3::Overworld* {
      if (session_teardown_prepared_ || !rom_ || !game_data_)
        return nullptr;
      auto* overworld = GetOverworldData();
      return overworld && overworld->is_loaded() && overworld->rom() == rom_
                 ? overworld
                 : nullptr;
    });
    graphics->SetMainGraphicsUsageCallback(main_usage);
    graphics->SetRoomsetGraphicsUsageCallback(roomset_usage);
  }
  if (auto* overworld = dynamic_cast<OverworldEditor*>(editor.get())) {
    overworld->SetMainGraphicsUsageCallback(main_usage);
    overworld->SetRoomsetGraphicsUsageCallback(roomset_usage);
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
    case EditorType::kPlanning:
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

const std::map<std::string, core::AsarSymbol>& EditorSet::GetAssemblySymbols()
    const {
  static const std::map<std::string, core::AsarSymbol> kEmpty;
  if (auto* editor =
          static_cast<AssemblyEditor*>(FindEditor(EditorType::kAssembly))) {
    return editor->symbols();
  }
  return kEmpty;
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

bool RomSession::HasPendingGfxGroupChanges() const {
  if (!game_data_loaded || !rom.is_loaded()) {
    return false;
  }
  auto diff = zelda3::DiffGfxGroups(rom, game_data);
  return diff.ok() && diff->any();
}

bool EditorSet::HasPendingGraphicsChanges() const {
  if (auto* editor =
          static_cast<GraphicsEditor*>(FindEditor(EditorType::kGraphics))) {
    return editor->HasPendingGraphicsChanges();
  }
  return false;
}

bool EditorSet::HasPendingSpriteChanges() const {
  if (auto* editor =
          static_cast<SpriteEditor*>(FindEditor(EditorType::kSprite)))
    return editor->HasPendingSpriteChanges();
  return false;
}

bool EditorSet::HasPendingScreenChanges() const {
  if (auto* editor =
          static_cast<ScreenEditor*>(FindEditor(EditorType::kScreen))) {
    return editor->HasPendingScreenChanges();
  }
  return false;
}

void EditorSet::InvalidateScreenRomBackedState() {
  if (auto* editor =
          static_cast<ScreenEditor*>(FindEditor(EditorType::kScreen))) {
    editor->InvalidateRomBackedState();
  }
}

bool EditorSet::HasPendingProjectDraftChanges() const {
  return HasPendingPlanningProjectDraftChanges() ||
         HasPendingBuildProjectDraftChanges();
}

bool EditorSet::HasPendingBuildProjectDraftChanges() const {
  if (HasPendingSpriteChanges())
    return true;
  if (auto* editor =
          static_cast<DungeonEditorV2*>(FindEditor(EditorType::kDungeon))) {
    if (editor->HasPendingProjectDraftChanges())
      return true;
  }
  if (auto* editor =
          static_cast<OverworldEditor*>(FindEditor(EditorType::kOverworld))) {
    return editor->HasPendingProjectDraftChanges();
  }
  return false;
}

bool EditorSet::HasPendingPlanningProjectDraftChanges() const {
  auto* editor =
      static_cast<PlanningEditor*>(FindEditor(EditorType::kPlanning));
  return editor && editor->HasPendingProjectDraftChanges();
}

absl::Status EditorSet::PrepareProjectSave(bool publish_lyra) {
  if (auto* editor =
          static_cast<SpriteEditor*>(FindEditor(EditorType::kSprite)))
    RETURN_IF_ERROR(editor->PrepareProjectSave());
  if (auto* editor =
          static_cast<DungeonEditorV2*>(FindEditor(EditorType::kDungeon))) {
    RETURN_IF_ERROR(editor->PrepareProjectSave());
  }
  if (publish_lyra) {
    if (auto* editor =
            static_cast<OverworldEditor*>(FindEditor(EditorType::kOverworld))) {
      RETURN_IF_ERROR(editor->PrepareProjectSave());
    }
  }
  if (auto* editor =
          static_cast<PlanningEditor*>(FindEditor(EditorType::kPlanning))) {
    RETURN_IF_ERROR(editor->Save());
  }
  return absl::OkStatus();
}

void EditorSet::ProjectSaveCommitted() {
  if (auto* editor =
          static_cast<SpriteEditor*>(FindEditor(EditorType::kSprite)))
    editor->ProjectSaveCommitted();
  if (auto* editor =
          static_cast<OverworldEditor*>(FindEditor(EditorType::kOverworld))) {
    editor->ProjectSaveCommitted();
  }
}

void EditorSet::DiscardPlanningProjectDraft() {
  if (auto* editor =
          static_cast<PlanningEditor*>(FindEditor(EditorType::kPlanning))) {
    editor->DiscardProjectDraftChanges();
  }
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

RomSession::RomSession(UserSettings* user_settings, size_t session_id,
                       EditorRegistry* editor_registry)
    : game_data(&rom),
      editors(&rom, &game_data, user_settings, session_id, editor_registry) {}

RomSession::RomSession(Rom&& r, UserSettings* user_settings, size_t session_id,
                       EditorRegistry* editor_registry)
    : rom(std::move(r)),
      game_data(&rom),
      editors(&rom, &game_data, user_settings, session_id, editor_registry) {
  filepath = rom.filename();
  feature_flags = core::FeatureFlags::Flags{};
  CaptureRomIdentity();
}

void RomSession::CaptureRomIdentity(bool preserve_editing_source) {
  if (!rom.is_loaded()) {
    rom_baseline.reset();
    return;
  }
  auto path = std::filesystem::path(rom.filename()).lexically_normal();
  path.make_preferred();
  const std::string editing_source = preserve_editing_source && rom_baseline
                                         ? rom_baseline->editing_source_path
                                         : path.string();
  rom_baseline = RomBaselineIdentity{
      util::ComputeRomIdentityHashes(rom.data(), rom.size()), path.string(),
      editing_source};
}

void RomSession::ReloadGraphicsDisplay() {
  ResetGraphicsDisplay();
  graphics_display_ =
      std::make_unique<std::array<gfx::Bitmap, zelda3::kNumGfxSheets>>(
          game_data.gfx_bitmaps);
  // Rehydration always reads the authoring store, including unsaved pixels.
  for (uint16_t i = 0; i < graphics_display_->size(); ++i) {
    auto& bitmap = graphics_display_->at(i);
    if (!bitmap.is_active() || !game_data.sheet_store.HasSheet(i))
      continue;
    const auto pixels = game_data.sheet_store.Sheet(i);
    bitmap.set_data(std::vector<uint8_t>(pixels.begin(), pixels.end()));
  }
  ActivateGraphicsDisplay();
}

void RomSession::ActivateGraphicsDisplay() {
  gfx::Arena::Get().BindGraphicsSheets(graphics_display_.get(), &game_data);
}

void RomSession::ResetGraphicsDisplay() {
  auto& arena = gfx::Arena::Get();
  if (arena.gfx_sheets_owner() == &game_data) {
    arena.BindGraphicsSheets(nullptr, nullptr);
  }
  if (graphics_display_) {
    for (auto& bitmap : *graphics_display_)
      arena.RetireBitmap(bitmap);
    graphics_display_.reset();
  }
}

RomSession::~RomSession() {
  ResetGraphicsDisplay();
  gfx::PaletteManager::Get().ReleaseSession(&game_data);
}

std::string RomSession::GetDisplayName() const {
  if (!custom_name.empty()) {
    return custom_name;
  }
  return rom.title().empty() ? "Untitled Session" : rom.title();
}

}  // namespace yaze::editor
