#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/application.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/dungeon/ui/window/overlay_manager_panel.h"
#include "app/editor/editor_manager.h"
#include "app/editor/graphics/graphics_editor.h"
#include "app/editor/graphics/graphics_undo_actions.h"
#include "app/editor/graphics/screen_editor.h"
#include "app/gfx/backend/null_renderer.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/util/palette_manager.h"
#include "core/features.h"
#include "rom/snes.h"
#include "testing.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room.h"

#include "imgui/imgui.h"

namespace yaze::editor {

class GraphicsEditorSaveStoplossTestPeer {
 public:
  static void MarkSheetModified(GraphicsEditor* editor, uint16_t sheet_id) {
    editor->state_.MarkSheetModified(sheet_id);
  }
};

class ScreenEditorSaveStoplossTestPeer {
 public:
  static void MarkDungeonMapModified(ScreenEditor* editor) {
    editor->MarkDungeonMapModified();
  }

  static void MarkDungeonMapTile16Modified(ScreenEditor* editor) {
    editor->MarkDungeonMapTile16Modified();
  }

  static void MarkTitleScreenModified(ScreenEditor* editor) {
    editor->MarkTitleScreenModified();
  }

  static void MarkOverworldMapModified(ScreenEditor* editor) {
    editor->MarkOverworldMapModified();
  }

  static void MarkOverworldMapPaletteModified(ScreenEditor* editor) {
    editor->MarkOverworldMapPaletteModified();
  }

  static void ClearPendingChanges(ScreenEditor* editor) {
    editor->pending_dungeon_map_changes_ = false;
    editor->pending_dungeon_map_tile16_changes_ = false;
    editor->pending_title_screen_changes_ = false;
    editor->pending_overworld_map_changes_ = false;
    editor->pending_overworld_map_palette_changes_ = false;
  }

  static void CommitDungeonMapEdit(ScreenEditor* editor) {
    std::array<uint8_t, zelda3::kNumRooms> rooms{};
    rooms.fill(0x0F);
    std::array<uint8_t, zelda3::kNumRooms> graphics{};
    graphics.fill(0xFF);
    editor->dungeon_maps_.emplace_back(
        0xFFFF, 1, 0,
        std::vector<std::array<uint8_t, zelda3::kNumRooms>>{rooms},
        std::vector<std::array<uint8_t, zelda3::kNumRooms>>{graphics});
    editor->dungeon_map_labels_[0].emplace_back();
    editor->selected_dungeon = 0;
    editor->SaveDungeonMapUndoState("Edit dungeon map");
    editor->dungeon_maps_[0].boss_room = 0x0123;
    editor->CommitDungeonMapUndo();
  }

  static void CommitDungeonMapTile16Edit(ScreenEditor* editor) {
    editor->selected_tile16_ = 4;
    editor->current_tile16_info = {};
    editor->SaveTile16CompUndoState("Edit dungeon-map Tile16");
    editor->current_tile16_info[0] = gfx::TileInfo(1, 0, false, false, false);
    editor->CommitTile16CompUndo();
  }

  static void PrimeRomBackedState(ScreenEditor* editor) {
    editor->dungeon_maps_.emplace_back(
        0xFFFF, 0, 0, std::vector<std::array<uint8_t, zelda3::kNumRooms>>{},
        std::vector<std::array<uint8_t, zelda3::kNumRooms>>{});
    MarkDungeonMapModified(editor);
    MarkDungeonMapTile16Modified(editor);
    MarkTitleScreenModified(editor);
    MarkOverworldMapModified(editor);
    MarkOverworldMapPaletteModified(editor);
    editor->has_pending_dungeon_undo_ = true;
    editor->has_pending_tile16_undo_ = true;
    editor->pending_dungeon_desc_ = "pending dungeon edit";
    editor->pending_tile16_desc_ = "pending Tile16 edit";
    editor->dungeon_map_labels_[0].emplace_back();
    editor->binary_gfx_loaded_ = true;
    editor->inventory_loaded_ = true;
    editor->title_screen_loaded_ = true;
    editor->ow_map_loaded_ = true;
    editor->selected_room = 7;
    editor->selected_tile16_ = 8;
    editor->selected_tile8_ = 9;
    editor->selected_dungeon = 10;
    editor->floor_number = 11;
    editor->sheets_[0] = std::make_unique<gfx::Bitmap>();
    editor->tile16_blockset_.tile_cache.CacheTile(1, gfx::Bitmap{});
    editor->tile8_tilemap_.tile_cache.CacheTile(2, gfx::Bitmap{});
    editor->screen_canvas_.mutable_points()->push_back(ImVec2(1, 2));
    editor->tilesheet_canvas_.mutable_points()->push_back(ImVec2(3, 4));

    ScreenSnapshot before;
    ScreenSnapshot after;
    editor->undo_manager_.Push(std::make_unique<ScreenEditAction>(
        before, after, [](const ScreenSnapshot&) {}, "older screen edit"));
    editor->undo_manager_.Push(std::make_unique<ScreenEditAction>(
        before, after, [](const ScreenSnapshot&) {}, "newer screen edit"));
    editor->undo_manager_.Undo().IgnoreError();
  }

  static void ResetRomBackedStateForLoad(ScreenEditor* editor) {
    editor->ResetRomBackedStateForLoad();
  }

  static bool HasReloadResidue(const ScreenEditor& editor) {
    const bool has_labels =
        std::ranges::any_of(editor.dungeon_map_labels_,
                            [](const auto& labels) { return !labels.empty(); });
    return !editor.dungeon_maps_.empty() || has_labels ||
           editor.undo_manager_.CanUndo() || editor.undo_manager_.CanRedo() ||
           editor.has_pending_dungeon_undo_ ||
           editor.has_pending_tile16_undo_ ||
           !editor.pending_dungeon_desc_.empty() ||
           !editor.pending_tile16_desc_.empty() || editor.binary_gfx_loaded_ ||
           editor.inventory_loaded_ || editor.title_screen_loaded_ ||
           editor.ow_map_loaded_ || editor.selected_room != 0 ||
           editor.selected_tile16_ != 0 || editor.selected_tile8_ != 0 ||
           editor.selected_dungeon != 0 || editor.floor_number != 0 ||
           !editor.sheets_.empty() ||
           editor.tile16_blockset_.tile_cache.Size() != 0 ||
           editor.tile8_tilemap_.tile_cache.Size() != 0 ||
           !editor.screen_canvas_.points().empty() ||
           !editor.tilesheet_canvas_.points().empty() ||
           editor.HasPendingScreenChanges();
  }

  static absl::Status CreateTitleScreenModel(ScreenEditor* editor, Rom* rom,
                                             zelda3::GameData* game_data) {
    auto status = editor->title_screen_.Create(rom, game_data);
    editor->title_screen_loaded_ = status.ok();
    return status;
  }

  static absl::Status CreateOverworldMapModel(ScreenEditor* editor, Rom* rom) {
    auto status = editor->ow_map_screen_.Create(rom);
    editor->ow_map_loaded_ = status.ok();
    return status;
  }

  static size_t TitleScreenPaletteSize(ScreenEditor& editor) {
    return editor.title_screen_.palette().size();
  }

  static size_t LightWorldPaletteSize(ScreenEditor& editor) {
    return editor.ow_map_screen_.lw_palette().size();
  }

  static size_t DarkWorldPaletteSize(ScreenEditor& editor) {
    return editor.ow_map_screen_.dw_palette().size();
  }

  static absl::Status SaveTitleScreenToRom(ScreenEditor* editor) {
    return editor->SaveTitleScreenToRom();
  }

  static absl::Status SaveOverworldMapToRom(ScreenEditor* editor) {
    return editor->SaveOverworldMapToRom();
  }

  static bool HasPendingOverworldMapTileChanges(ScreenEditor& editor) {
    return editor.pending_overworld_map_changes_;
  }

  static bool HasPendingOverworldMapPaletteChanges(ScreenEditor& editor) {
    return editor.pending_overworld_map_palette_changes_;
  }
};

class DungeonEditorV2ReloadTestPeer {
 public:
  static DungeonCanvasViewer* GetViewerForRoom(DungeonEditorV2* editor,
                                               int room_id) {
    return editor->GetViewerForRoom(room_id);
  }

  static bool HasRoomViewer(const DungeonEditorV2& editor, int room_id) {
    return editor.room_viewers_.Contains(room_id);
  }

  static zelda3::RoomEntrance* GetRegularEntrance(DungeonEditorV2* editor,
                                                  int entrance_id) {
    return &editor->entrances_[zelda3::kNumDungeonSpawnPoints + entrance_id];
  }

  static zelda3::DungeonSpawnPoint* GetSpawnPoint(DungeonEditorV2* editor,
                                                  int spawn_id) {
    return &editor->spawn_points_[spawn_id];
  }

  static std::shared_ptr<zelda3::DungeonObjectEditor> GetObjectEditor(
      DungeonEditorV2* editor) {
    return editor->dungeon_editor_system_->GetObjectEditor();
  }

  static OverlayManagerPanel* GetOverlayManagerPanel(DungeonEditorV2* editor) {
    return editor->overlay_manager_panel_;
  }

  static bool* GetOverlayGridBinding(DungeonEditorV2* editor) {
    return editor->overlay_manager_panel_
               ? editor->overlay_manager_panel_->state_.show_grid
               : nullptr;
  }

  static void SyncPanelsToRoom(DungeonEditorV2* editor, int room_id) {
    editor->SyncPanelsToRoom(room_id);
  }
};

namespace {

struct FeatureFlagsGuard {
  core::FeatureFlags::Flags prev = core::FeatureFlags::get();
  ~FeatureFlagsGuard() { core::FeatureFlags::get() = prev; }
};

struct PaletteManagerGuard {
  PaletteManagerGuard() { gfx::PaletteManager::Get().ResetForTesting(); }
  ~PaletteManagerGuard() { gfx::PaletteManager::Get().ResetForTesting(); }
};

struct ScopedFileCleanup {
  std::filesystem::path path;
  ~ScopedFileCleanup() {
    std::error_code ec;
    std::filesystem::remove(path, ec);
  }
};

struct ScopedDirectoryCleanup {
  std::filesystem::path path;
  ~ScopedDirectoryCleanup() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
};

struct ScopedGraphicsSheetRestore {
  explicit ScopedGraphicsSheetRestore(gfx::Bitmap* sheet)
      : sheet(sheet), original(std::move(*sheet)) {}
  ~ScopedGraphicsSheetRestore() { *sheet = std::move(original); }

  gfx::Bitmap* sheet;
  gfx::Bitmap original;
};

struct ScopedTextureQueueClear {
  ScopedTextureQueueClear() { gfx::Arena::Get().ClearTextureQueue(); }
  ~ScopedTextureQueueClear() { gfx::Arena::Get().ClearTextureQueue(); }
};

struct ScopedImGuiContext {
  ImGuiContext* ctx = nullptr;
  ScopedImGuiContext() {
    ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(ctx);
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  }
  ~ScopedImGuiContext() {
    if (ctx) {
      ImGui::DestroyContext(ctx);
      ctx = nullptr;
    }
  }
};

std::filesystem::path MakeTempFilePath(const std::string& basename) {
  const auto nonce = static_cast<uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  return std::filesystem::temp_directory_path() /
         (basename + "_" + std::to_string(nonce));
}

uint8_t ReadByteAt(const std::filesystem::path& path, std::streamoff offset) {
  std::ifstream file(path, std::ios::binary);
  EXPECT_TRUE(file.is_open());
  file.seekg(offset, std::ios::beg);
  char b = 0;
  file.read(&b, 1);
  EXPECT_TRUE(file.good());
  return static_cast<uint8_t>(b);
}

void WriteTestRom(const std::filesystem::path& path,
                  const std::string& title = "YAZE TEST ROM") {
  std::vector<uint8_t> rom_data(512 * 1024, 0x00);
  for (size_t i = 0; i < title.size() && (0x7FC0 + i) < rom_data.size(); ++i) {
    rom_data[0x7FC0 + i] = static_cast<uint8_t>(title[i]);
  }
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(out.is_open());
  out.write(reinterpret_cast<const char*>(rom_data.data()),
            static_cast<std::streamsize>(rom_data.size()));
  ASSERT_TRUE(out.good());
}

void WriteScreenModelTestRom(const std::filesystem::path& path) {
  std::vector<uint8_t> rom_data(1024 * 1024, 0x00);
  const std::string kTitle = "SCREEN MODEL TEST";
  for (size_t i = 0; i < kTitle.size(); ++i) {
    rom_data[0x7FC0 + i] = static_cast<uint8_t>(kTitle[i]);
  }
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(out.is_open());
  out.write(reinterpret_cast<const char*>(rom_data.data()),
            static_cast<std::streamsize>(rom_data.size()));
  ASSERT_TRUE(out.good());
}

void PopulateTitleScreenTestGameData(zelda3::GameData* game_data) {
  ASSERT_NE(game_data, nullptr);
  game_data->graphics_buffer.resize(zelda3::kNumGfxSheets * 0x1000, 0x00);

  gfx::SnesPalette palette;
  for (uint16_t color = 0; color < 8; ++color) {
    palette.AddColor(gfx::SnesColor(color));
  }
  auto add_palettes = [&palette](gfx::PaletteGroup* group, size_t count) {
    ASSERT_NE(group, nullptr);
    for (size_t i = 0; i < count; ++i) {
      group->AddPalette(palette);
    }
  };
  add_palettes(&game_data->palette_groups.overworld_main, 6);
  add_palettes(&game_data->palette_groups.overworld_animated, 1);
  add_palettes(&game_data->palette_groups.overworld_aux, 4);
  add_palettes(&game_data->palette_groups.hud, 1);
  add_palettes(&game_data->palette_groups.sprites_aux1, 2);
}

constexpr int kDungeonHeaderTablePc = 0x0F6000;
constexpr int kDungeonRoom0HeaderPc = 0x114000;

void WriteDungeonHeaderTestRom(const std::filesystem::path& path,
                               uint8_t room_palette, uint16_t entrance_room_id,
                               uint16_t spawn_room_id) {
  std::vector<uint8_t> rom_data(2 * 1024 * 1024, 0x00);
  const std::string title = "DUNGEON RESTORE";
  for (size_t i = 0; i < title.size(); ++i) {
    rom_data[0x7FC0 + i] = static_cast<uint8_t>(title[i]);
  }

  const uint32_t header_table_snes = PcToSnes(kDungeonHeaderTablePc);
  rom_data[zelda3::kRoomHeaderPointer] = header_table_snes & 0xFF;
  rom_data[zelda3::kRoomHeaderPointer + 1] = (header_table_snes >> 8) & 0xFF;
  rom_data[zelda3::kRoomHeaderPointer + 2] = (header_table_snes >> 16) & 0xFF;

  const uint32_t room_header_snes = PcToSnes(kDungeonRoom0HeaderPc);
  rom_data[zelda3::kRoomHeaderPointerBank] = (room_header_snes >> 16) & 0xFF;
  rom_data[kDungeonHeaderTablePc] = room_header_snes & 0xFF;
  rom_data[kDungeonHeaderTablePc + 1] = (room_header_snes >> 8) & 0xFF;
  rom_data[kDungeonRoom0HeaderPc + 1] = room_palette;
  rom_data[zelda3::kEntranceRoom] = entrance_room_id & 0xFF;
  rom_data[zelda3::kEntranceRoom + 1] = (entrance_room_id >> 8) & 0xFF;
  rom_data[zelda3::kDungeonSpawnRoom] = spawn_room_id & 0xFF;
  rom_data[zelda3::kDungeonSpawnRoom + 1] = (spawn_room_id >> 8) & 0xFF;

  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(out.is_open());
  out.write(reinterpret_cast<const char*>(rom_data.data()),
            static_cast<std::streamsize>(rom_data.size()));
  ASSERT_TRUE(out.good());
}

void WriteTestProject(const std::filesystem::path& path,
                      const std::filesystem::path& rom_path) {
  std::ofstream out(path, std::ios::out | std::ios::trunc);
  ASSERT_TRUE(out.is_open());
  out << "[project]\n"
         "name=Backing File Identity Test\n\n"
         "[files]\n"
      << "rom_filename=" << rom_path.string() << "\n"
      << "code_folder=\n"
         "rom_backup_folder=backups\n"
         "output_folder=output\n"
         "labels_filename=\n"
         "symbols_filename=\n";
  ASSERT_TRUE(out.good());
}

void DisableRomWritesForTest() {
  auto& flags = core::FeatureFlags::get();
  flags.kSaveDungeonMaps = false;
  flags.kSaveGraphicsSheet = false;
  flags.kSaveMessages = false;

  auto& d = flags.dungeon;
  d.kSaveObjects = false;
  d.kSaveSprites = false;
  d.kSaveRoomHeaders = false;
  d.kSaveTorches = false;
  d.kSavePits = false;
  d.kSaveBlocks = false;
  d.kSaveCollision = false;
  d.kSaveWaterFillZones = false;
  d.kSaveChests = false;
  d.kSavePotItems = false;
  d.kSaveEntrances = false;
  d.kSavePalettes = false;

  auto& o = flags.overworld;
  o.kSaveOverworldMaps = false;
  o.kSaveOverworldEntrances = false;
  o.kSaveOverworldExits = false;
  o.kSaveOverworldItems = false;
  o.kSaveOverworldProperties = false;
}

TEST(EditorManagerBackingFileIdentityTest,
     RawRomOpenRejectsLexicalAliasOfActiveBackingFile) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto rom_path = MakeTempFilePath("yaze_lexical_identity.sfc");
  const auto alias_dir = MakeTempFilePath("yaze_lexical_alias_dir");
  ScopedFileCleanup rom_cleanup{rom_path};
  ScopedDirectoryCleanup alias_cleanup{alias_dir};
  WriteTestRom(rom_path, "LEXICAL IDENTITY");
  ASSERT_TRUE(std::filesystem::create_directory(alias_dir));

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  const auto alias_path = alias_dir / ".." / rom_path.filename();
  const auto status = manager->OpenRomOrProject(alias_path.string());

  EXPECT_EQ(status.code(), absl::StatusCode::kAlreadyExists) << status;
  EXPECT_NE(std::string(status.message()).find("already open"),
            std::string::npos);
  EXPECT_EQ(manager->GetActiveSessionCount(), 1u);
  ASSERT_NE(manager->GetCurrentRom(), nullptr);
  EXPECT_EQ(manager->GetCurrentRom()->filename(), rom_path.string());
}

TEST(EditorManagerBackingFileIdentityTest,
     RawRomOpenRejectsEquivalentHardLinkOfActiveBackingFile) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto rom_path = MakeTempFilePath("yaze_canonical_identity.sfc");
  const auto hard_link_path = MakeTempFilePath("yaze_canonical_alias.sfc");
  ScopedFileCleanup rom_cleanup{rom_path};
  ScopedFileCleanup hard_link_cleanup{hard_link_path};
  WriteTestRom(rom_path, "CANONICAL IDENTITY");

  std::error_code link_ec;
  std::filesystem::create_hard_link(rom_path, hard_link_path, link_ec);
  if (link_ec) {
    GTEST_SKIP() << "Hard links unavailable: " << link_ec.message();
  }

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  const auto status = manager->OpenRomOrProject(hard_link_path.string());

  EXPECT_EQ(status.code(), absl::StatusCode::kAlreadyExists) << status;
  EXPECT_EQ(manager->GetActiveSessionCount(), 1u);
}

TEST(EditorManagerBackingFileIdentityTest,
     ProjectOpenRejectsRomOwnedByActiveSessionWithoutReplacingContext) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto rom_path = MakeTempFilePath("yaze_project_identity.sfc");
  auto project_path = MakeTempFilePath("yaze_project_identity");
  project_path += ".yaze";
  ScopedFileCleanup rom_cleanup{rom_path};
  ScopedFileCleanup project_cleanup{project_path};
  WriteTestRom(rom_path, "PROJECT IDENTITY");
  WriteTestProject(project_path, rom_path);

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  Rom* const active_rom = manager->GetCurrentRom();
  const std::string active_project_path =
      manager->GetCurrentProject()->filepath;

  const auto status = manager->OpenRomOrProject(project_path.string());

  EXPECT_EQ(status.code(), absl::StatusCode::kAlreadyExists) << status;
  EXPECT_EQ(manager->GetActiveSessionCount(), 1u);
  EXPECT_EQ(manager->GetCurrentRom(), active_rom);
  EXPECT_EQ(manager->GetCurrentProject()->filepath, active_project_path);
}

TEST(EditorManagerBackingFileIdentityTest,
     SaveRomAsRejectsOtherSessionButAllowsCurrentBackingFileAlias) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = false;

  const auto rom_a = MakeTempFilePath("yaze_save_as_owner_a.sfc");
  const auto rom_b = MakeTempFilePath("yaze_save_as_owner_b.sfc");
  const auto alias_dir = MakeTempFilePath("yaze_save_as_alias_dir");
  ScopedFileCleanup cleanup_a{rom_a};
  ScopedFileCleanup cleanup_b{rom_b};
  ScopedDirectoryCleanup alias_cleanup{alias_dir};
  WriteTestRom(rom_a, "SAVE AS OWNER A");
  WriteTestRom(rom_b, "SAVE AS OWNER B");
  ASSERT_TRUE(std::filesystem::create_directory(alias_dir));
  const auto rom_a_alias = alias_dir / ".." / rom_a.filename();

  ASSERT_OK(manager->OpenRomOrProject(rom_a.string()));
  ASSERT_OK(manager->OpenRomOrProject(rom_b.string()));
  ASSERT_EQ(manager->GetCurrentSessionIndex(), 1u);
  Rom* const rom_b_buffer = manager->GetCurrentRom();
  ASSERT_NE(rom_b_buffer, nullptr);

  const auto collision = manager->SaveRomAs(rom_a_alias.string());
  EXPECT_EQ(collision.code(), absl::StatusCode::kAlreadyExists) << collision;
  EXPECT_EQ(rom_b_buffer->filename(), rom_b.string());
  auto* session_b =
      static_cast<RomSession*>(manager->session_coordinator()->GetSession(1));
  ASSERT_NE(session_b, nullptr);
  EXPECT_EQ(session_b->filepath, rom_b.string());

  manager->SwitchToSession(0);
  ASSERT_EQ(manager->GetCurrentSessionIndex(), 0u);
  DisableRomWritesForTest();
  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "Save As Current Owner";
  project->filepath =
      (rom_a.parent_path() / "save_as_current_owner.yaze").string();
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.write_policy = project::RomWritePolicy::kAllow;
  project->rom_metadata.expected_hash.clear();

  ASSERT_OK(manager->SaveRomAs(rom_a_alias.string()));
  EXPECT_EQ(manager->GetCurrentRom()->filename(), rom_a_alias.string());
}

TEST(EditorManagerRomWritePolicyTest,
     SaveRomWarnsOnHashMismatchAndConfirmsOnce) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const std::filesystem::path rom_path =
      MakeTempFilePath("yaze_rom_write_policy_warn.sfc");
  ScopedFileCleanup cleanup{rom_path};

  std::vector<uint8_t> rom_data(512 * 1024, 0x00);
  const std::string title = "YAZE TEST ROM";
  for (size_t i = 0; i < title.size() && (0x7FC0 + i) < rom_data.size(); ++i) {
    rom_data[0x7FC0 + i] = static_cast<uint8_t>(title[i]);
  }
  {
    std::ofstream out(rom_path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(out.is_open());
    out.write(reinterpret_cast<const char*>(rom_data.data()),
              static_cast<std::streamsize>(rom_data.size()));
    ASSERT_TRUE(out.good());
  }

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();  // OpenRomOrProject resets feature flags.

  // Mark a project as opened so the save-time write policy check is active.
  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "RomWritePolicyWarn";
  project->filepath = (rom_path.parent_path() / "project.yaze").string();
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.write_policy = project::RomWritePolicy::kWarn;

  // Ensure we have a baseline ROM hash, then force a mismatch.
  ASSERT_FALSE(manager->GetCurrentRomHash().empty());
  project->rom_metadata.expected_hash = "deadbeef";
  ASSERT_TRUE(manager->IsRomHashMismatch());

  Rom* rom = manager->GetCurrentRom();
  ASSERT_NE(rom, nullptr);
  ASSERT_TRUE(rom->is_loaded());

  constexpr uint32_t kPcOffset = 0x1234;
  const uint8_t original = rom_data[kPcOffset];
  const uint8_t mutated = static_cast<uint8_t>(original ^ 0xFF);
  ASSERT_OK(rom->WriteByte(static_cast<int>(kPcOffset), mutated));

  // First save attempt should be blocked awaiting confirmation.
  auto status = manager->SaveRom();
  EXPECT_EQ(status.code(), absl::StatusCode::kCancelled) << status;
  EXPECT_TRUE(manager->IsRomWriteConfirmPending());
  EXPECT_EQ(ReadByteAt(rom_path, static_cast<std::streamoff>(kPcOffset)),
            original);

  // Confirm once and resume the bound request: should save successfully.
  manager->ConfirmRomWrite();
  EXPECT_FALSE(manager->IsRomWriteConfirmPending());

  auto status2 = manager->ResumePendingRomSave();
  EXPECT_TRUE(status2.ok()) << status2.message();
  EXPECT_EQ(ReadByteAt(rom_path, static_cast<std::streamoff>(kPcOffset)),
            mutated);
}

TEST(EditorManagerBackupRestoreTest,
     AutomaticBackupRestoreStagesAndCommitsAReversibleRoundTrip) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = true;

  const auto temp_dir = MakeTempFilePath("yaze_backup_restore_round_trip");
  ScopedDirectoryCleanup cleanup{temp_dir};
  ASSERT_TRUE(std::filesystem::create_directories(temp_dir));
  const auto rom_path = temp_dir / "oracle-copy.sfc";
  WriteTestRom(rom_path, "BACKUP RESTORE");
  {
    std::ofstream labels(rom_path.string() + ".labels",
                         std::ios::out | std::ios::trunc);
    ASSERT_TRUE(labels.is_open());
    labels << "[rooms]\n0=On-disk room label\n";
    ASSERT_TRUE(labels.good());
  }

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();

  Rom* const active_rom = manager->GetCurrentRom();
  ASSERT_NE(active_rom, nullptr);
  active_rom->resource_label()->EditLabel("rooms", "0", "In-memory room label");
  constexpr uint32_t kPcOffset = 0x1234;
  constexpr uint8_t kOriginal = 0x00;
  constexpr uint8_t kEdited = 0xA5;
  ASSERT_OK(active_rom->WriteByte(kPcOffset, kEdited));
  ASSERT_OK(manager->SaveRom());
  EXPECT_FALSE(active_rom->dirty());
  EXPECT_EQ(ReadByteAt(rom_path, kPcOffset), kEdited);

  const auto initial_backups = manager->GetRomBackups();
  ASSERT_EQ(initial_backups.size(), 1u);
  EXPECT_EQ(ReadByteAt(initial_backups.front().path, kPcOffset), kOriginal);

  ASSERT_OK(manager->RestoreRomBackup(initial_backups.front().path));
  EXPECT_EQ(manager->GetCurrentRom(), active_rom);
  EXPECT_EQ(active_rom->filename(),
            std::filesystem::absolute(rom_path).string());
  ASSERT_TRUE(active_rom->ReadByte(kPcOffset).ok());
  EXPECT_EQ(*active_rom->ReadByte(kPcOffset), kOriginal);
  EXPECT_TRUE(active_rom->dirty());
  EXPECT_EQ(active_rom->resource_label()->GetLabel("rooms", "0"),
            "In-memory room label");
  EXPECT_EQ(active_rom->resource_label()->filename_,
            std::filesystem::absolute(rom_path).string() + ".labels");
  // Restore is staged: the backing file remains untouched until Save ROM.
  EXPECT_EQ(ReadByteAt(rom_path, kPcOffset), kEdited);
  auto* const session = manager->session_coordinator()->GetActiveRomSession();
  ASSERT_NE(session, nullptr);
  EXPECT_TRUE(session->backup_restore_pending);

  // Autosave is enabled by default, but a staged restore must require an
  // explicit decision even when automatic backups are disabled.
  manager->user_settings().prefs().backup_before_save = false;
  const auto autosave_status = manager->AutosaveActiveSession();
  EXPECT_EQ(autosave_status.code(), absl::StatusCode::kCancelled)
      << autosave_status;
  EXPECT_TRUE(active_rom->dirty());
  EXPECT_TRUE(session->backup_restore_pending);
  EXPECT_EQ(ReadByteAt(rom_path, kPcOffset), kEdited);
  manager->user_settings().prefs().backup_before_save = true;

  // Backup names include millisecond precision; ensure this second save gets
  // its own entry even on very fast filesystems.
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  ASSERT_OK(manager->SaveRom());
  EXPECT_FALSE(active_rom->dirty());
  EXPECT_FALSE(session->backup_restore_pending);
  EXPECT_EQ(ReadByteAt(rom_path, kPcOffset), kOriginal);

  const auto final_backups = manager->GetRomBackups();
  ASSERT_EQ(final_backups.size(), 2u);
  bool preserved_original = false;
  bool preserved_edited = false;
  for (const auto& backup : final_backups) {
    const uint8_t value = ReadByteAt(backup.path, kPcOffset);
    preserved_original |= value == kOriginal;
    preserved_edited |= value == kEdited;
  }
  EXPECT_TRUE(preserved_original);
  EXPECT_TRUE(preserved_edited);
}

TEST(EditorManagerBackupRestoreTest,
     RestoreRefreshesMaterializedDungeonStateInPlace) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = true;

  const auto temp_dir = MakeTempFilePath("yaze_backup_restore_dungeon_state");
  ScopedDirectoryCleanup cleanup{temp_dir};
  ASSERT_TRUE(std::filesystem::create_directories(temp_dir));
  const auto rom_path = temp_dir / "oracle-copy.sfc";
  constexpr uint8_t kBackupPalette = 0x03;
  constexpr uint8_t kEditedPalette = 0x2A;
  constexpr uint16_t kBackupEntranceRoom = 0x0012;
  constexpr uint16_t kEditedEntranceRoom = 0x0023;
  constexpr uint16_t kBackupSpawnRoom = 0x0034;
  constexpr uint16_t kEditedSpawnRoom = 0x0045;
  WriteDungeonHeaderTestRom(rom_path, kBackupPalette, kBackupEntranceRoom,
                            kBackupSpawnRoom);

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();
  core::FeatureFlags::get().dungeon.kUseWorkbench = false;
  ASSERT_OK(manager->EnsureEditorAssetsLoaded(EditorType::kDungeon));

  Rom* const active_rom = manager->GetCurrentRom();
  ASSERT_NE(active_rom, nullptr);
  auto* dungeon = static_cast<DungeonEditorV2*>(
      manager->GetCurrentEditorSet()->GetEditor(EditorType::kDungeon));
  ASSERT_NE(dungeon, nullptr);

  dungeon->rooms()[0] = zelda3::LoadRoomHeaderFromRom(active_rom, 0);
  dungeon->rooms()[0].ClearSaveDirtyState();
  ASSERT_EQ(dungeon->rooms()[0].palette(), kBackupPalette);
  auto* const room_before = dungeon->rooms().GetIfMaterialized(0);
  auto* const viewer_before =
      DungeonEditorV2ReloadTestPeer::GetViewerForRoom(dungeon, 0);
  ASSERT_NE(room_before, nullptr);
  ASSERT_NE(viewer_before, nullptr);
  DungeonEditorV2ReloadTestPeer::SyncPanelsToRoom(dungeon, 0);
  auto* const overlay_panel_before =
      DungeonEditorV2ReloadTestPeer::GetOverlayManagerPanel(dungeon);
  ASSERT_NE(overlay_panel_before, nullptr);
  bool* const overlay_toggle_before = viewer_before->mutable_show_grid();
  ASSERT_NE(overlay_toggle_before, nullptr);
  ASSERT_EQ(DungeonEditorV2ReloadTestPeer::GetOverlayGridBinding(dungeon),
            overlay_toggle_before);
  ASSERT_TRUE(DungeonEditorV2ReloadTestPeer::HasRoomViewer(*dungeon, 0));
  ASSERT_OK(dungeon->RefreshRomBackedState());

  auto* const entrance_before =
      DungeonEditorV2ReloadTestPeer::GetRegularEntrance(dungeon, 0);
  auto* const spawn_before =
      DungeonEditorV2ReloadTestPeer::GetSpawnPoint(dungeon, 0);
  auto object_editor_before =
      DungeonEditorV2ReloadTestPeer::GetObjectEditor(dungeon);
  ASSERT_NE(object_editor_before, nullptr);
  ASSERT_EQ(dungeon->rooms().GetIfMaterialized(0), room_before);
  ASSERT_EQ(DungeonEditorV2ReloadTestPeer::GetViewerForRoom(dungeon, 0),
            viewer_before);
  ASSERT_EQ(viewer_before->mutable_show_grid(), overlay_toggle_before);
  ASSERT_EQ(object_editor_before->GetMutableRoom(), room_before);
  ASSERT_EQ(entrance_before->room_, kBackupEntranceRoom);
  ASSERT_EQ(spawn_before->room_id, kBackupSpawnRoom);

  ASSERT_OK(active_rom->WriteByte(kDungeonRoom0HeaderPc + 1, kEditedPalette));
  ASSERT_OK(active_rom->WriteShort(zelda3::kEntranceRoom, kEditedEntranceRoom));
  ASSERT_OK(
      active_rom->WriteShort(zelda3::kDungeonSpawnRoom, kEditedSpawnRoom));
  ASSERT_OK(manager->SaveRom());
  const auto backups = manager->GetRomBackups();
  ASSERT_EQ(backups.size(), 1u);
  ASSERT_EQ(ReadByteAt(backups.front().path, kDungeonRoom0HeaderPc + 1),
            kBackupPalette);

  ASSERT_OK(dungeon->RefreshRomBackedState());
  ASSERT_EQ(room_before->palette(), kEditedPalette);
  ASSERT_EQ(entrance_before->room_, kEditedEntranceRoom);
  ASSERT_EQ(spawn_before->room_id, kEditedSpawnRoom);
  ASSERT_EQ(dungeon->rooms().GetIfMaterialized(0), room_before);
  ASSERT_EQ(DungeonEditorV2ReloadTestPeer::GetViewerForRoom(dungeon, 0),
            viewer_before);
  ASSERT_TRUE(DungeonEditorV2ReloadTestPeer::HasRoomViewer(*dungeon, 0));

  ASSERT_OK(manager->RestoreRomBackup(backups.front().path));
  EXPECT_EQ(manager->GetCurrentRom(), active_rom);
  ASSERT_TRUE(active_rom->ReadByte(kDungeonRoom0HeaderPc + 1).ok());
  EXPECT_EQ(*active_rom->ReadByte(kDungeonRoom0HeaderPc + 1), kBackupPalette);
  EXPECT_EQ(dungeon->rooms().GetIfMaterialized(0), room_before);
  EXPECT_EQ(room_before->palette(), kBackupPalette);
  EXPECT_EQ(DungeonEditorV2ReloadTestPeer::GetViewerForRoom(dungeon, 0),
            viewer_before);
  EXPECT_EQ(viewer_before->mutable_show_grid(), overlay_toggle_before);
  ASSERT_OK(manager->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  ASSERT_EQ(DungeonEditorV2ReloadTestPeer::GetOverlayManagerPanel(dungeon),
            overlay_panel_before);
  EXPECT_EQ(DungeonEditorV2ReloadTestPeer::GetOverlayGridBinding(dungeon),
            overlay_toggle_before);
  EXPECT_EQ(viewer_before->rom(), active_rom);
  EXPECT_EQ(viewer_before->rooms(), &dungeon->rooms());
  auto object_editor_after =
      DungeonEditorV2ReloadTestPeer::GetObjectEditor(dungeon);
  EXPECT_EQ(object_editor_after.get(), object_editor_before.get());
  EXPECT_EQ(object_editor_after->GetMutableRoom(), room_before);
  EXPECT_EQ(DungeonEditorV2ReloadTestPeer::GetRegularEntrance(dungeon, 0),
            entrance_before);
  EXPECT_EQ(entrance_before->room_, kBackupEntranceRoom);
  EXPECT_EQ(DungeonEditorV2ReloadTestPeer::GetSpawnPoint(dungeon, 0),
            spawn_before);
  EXPECT_EQ(spawn_before->room_id, kBackupSpawnRoom);

  // Desktop/full asset loading reconstructs the dungeon panels and editor
  // system before applying the same in-place room/viewer refresh. Exercise
  // that path explicitly so the replacement panels cannot retain stale
  // bindings after restore.
  object_editor_before.reset();
  ASSERT_OK(active_rom->WriteByte(kDungeonRoom0HeaderPc + 1, kEditedPalette));
  ASSERT_OK(active_rom->WriteShort(zelda3::kEntranceRoom, kEditedEntranceRoom));
  ASSERT_OK(
      active_rom->WriteShort(zelda3::kDungeonSpawnRoom, kEditedSpawnRoom));
  ASSERT_OK(manager->SaveRom());
  AppConfig full_mode_config;
  full_mode_config.startup_editor = "dungeon";
  manager->SetStartupLoadHints(full_mode_config);
  manager->SetAssetLoadMode(AssetLoadMode::kFull);
  ASSERT_OK(manager->RestoreRomBackup(backups.front().path));

  EXPECT_EQ(dungeon->rooms().GetIfMaterialized(0), room_before);
  EXPECT_EQ(room_before->palette(), kBackupPalette);
  EXPECT_EQ(DungeonEditorV2ReloadTestPeer::GetViewerForRoom(dungeon, 0),
            viewer_before);
  auto* const full_mode_overlay_panel =
      DungeonEditorV2ReloadTestPeer::GetOverlayManagerPanel(dungeon);
  ASSERT_NE(full_mode_overlay_panel, nullptr);
  EXPECT_EQ(DungeonEditorV2ReloadTestPeer::GetOverlayGridBinding(dungeon),
            overlay_toggle_before);
  auto full_mode_object_editor =
      DungeonEditorV2ReloadTestPeer::GetObjectEditor(dungeon);
  ASSERT_NE(full_mode_object_editor, nullptr);
  EXPECT_EQ(full_mode_object_editor->GetMutableRoom(), room_before);
  EXPECT_EQ(entrance_before->room_, kBackupEntranceRoom);
  EXPECT_EQ(spawn_before->room_id, kBackupSpawnRoom);

  // A full reload can skip an already-initialized dungeon editor when another
  // startup surface is active. Its refreshed state must remain marked loaded,
  // otherwise the next activation reconstructs the preserved panels again.
  auto* const full_mode_object_editor_ptr = full_mode_object_editor.get();
  full_mode_object_editor.reset();
  ASSERT_OK(active_rom->WriteByte(kDungeonRoom0HeaderPc + 1, kEditedPalette));
  ASSERT_OK(active_rom->WriteShort(zelda3::kEntranceRoom, kEditedEntranceRoom));
  ASSERT_OK(
      active_rom->WriteShort(zelda3::kDungeonSpawnRoom, kEditedSpawnRoom));
  ASSERT_OK(manager->SaveRom());
  AppConfig inactive_dungeon_config;
  inactive_dungeon_config.startup_editor = "assembly";
  manager->SetStartupLoadHints(inactive_dungeon_config);
  ASSERT_OK(manager->RestoreRomBackup(backups.front().path));

  auto* const restored_session =
      manager->session_coordinator()->GetActiveRomSession();
  ASSERT_NE(restored_session, nullptr);
  const size_t dungeon_index = EditorTypeIndex(EditorType::kDungeon);
  EXPECT_TRUE(restored_session->editor_initialized[dungeon_index]);
  EXPECT_TRUE(restored_session->editor_assets_loaded[dungeon_index]);
  EXPECT_EQ(DungeonEditorV2ReloadTestPeer::GetOverlayManagerPanel(dungeon),
            full_mode_overlay_panel);
  ASSERT_OK(manager->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  EXPECT_EQ(DungeonEditorV2ReloadTestPeer::GetOverlayManagerPanel(dungeon),
            full_mode_overlay_panel);
  auto inactive_full_mode_object_editor =
      DungeonEditorV2ReloadTestPeer::GetObjectEditor(dungeon);
  EXPECT_EQ(inactive_full_mode_object_editor.get(),
            full_mode_object_editor_ptr);
  EXPECT_EQ(inactive_full_mode_object_editor->GetMutableRoom(), room_before);
  EXPECT_EQ(DungeonEditorV2ReloadTestPeer::GetOverlayGridBinding(dungeon),
            overlay_toggle_before);
  EXPECT_EQ(room_before->palette(), kBackupPalette);
  EXPECT_EQ(entrance_before->room_, kBackupEntranceRoom);
  EXPECT_EQ(spawn_before->room_id, kBackupSpawnRoom);
}

TEST(EditorManagerBackupRestoreTest,
     RestoreRejectsDirtySessionAndAllowsManagedSizeChange) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto temp_dir = MakeTempFilePath("yaze_backup_restore_guards");
  ScopedDirectoryCleanup cleanup{temp_dir};
  ASSERT_TRUE(std::filesystem::create_directories(temp_dir));
  const auto rom_path = temp_dir / "oracle-copy.sfc";
  const auto backup_path = temp_dir / "oracle-copy_backup_manual.sfc";
  const auto corrupt_backup_path = temp_dir / "oracle-copy_backup_corrupt.sfc";
  const auto expanded_backup_path =
      temp_dir / "oracle-copy_backup_size-change.sfc";
  const auto unrelated_path = temp_dir / "unrelated.sfc";
  WriteTestRom(rom_path, "RESTORE GUARDS");
  std::filesystem::copy_file(rom_path, backup_path);
  std::filesystem::copy_file(rom_path, unrelated_path);
  {
    std::ofstream corrupt(corrupt_backup_path,
                          std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(corrupt.is_open());
    corrupt.put('\0');
    ASSERT_TRUE(corrupt.good());
  }

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();
  Rom* const active_rom = manager->GetCurrentRom();
  ASSERT_NE(active_rom, nullptr);
  constexpr uint32_t kPcOffset = 0x1234;
  ASSERT_OK(active_rom->WriteByte(kPcOffset, 0xA5));

  const auto dirty_status = manager->RestoreRomBackup(backup_path.string());
  EXPECT_EQ(dirty_status.code(), absl::StatusCode::kFailedPrecondition)
      << dirty_status;
  ASSERT_TRUE(active_rom->ReadByte(kPcOffset).ok());
  EXPECT_EQ(*active_rom->ReadByte(kPcOffset), 0xA5);
  active_rom->ClearDirty();

  const auto unrelated_status =
      manager->RestoreRomBackup(unrelated_path.string());
  EXPECT_EQ(unrelated_status.code(), absl::StatusCode::kInvalidArgument)
      << unrelated_status;
  EXPECT_EQ(manager->GetCurrentRom(), active_rom);
  EXPECT_EQ(active_rom->size(), 512u * 1024u);

  const std::string title_before_corrupt_restore = active_rom->title();
  const auto corrupt_status =
      manager->RestoreRomBackup(corrupt_backup_path.string());
  EXPECT_EQ(corrupt_status.code(), absl::StatusCode::kInvalidArgument)
      << corrupt_status;
  EXPECT_EQ(manager->GetCurrentRom(), active_rom);
  EXPECT_EQ(active_rom->size(), 512u * 1024u);
  EXPECT_EQ(active_rom->title(), title_before_corrupt_restore);
  EXPECT_FALSE(active_rom->dirty());

  std::vector<uint8_t> expanded_data(1024 * 1024, 0x00);
  const std::string expanded_title = "RESTORE EXPANDED";
  for (size_t i = 0; i < expanded_title.size(); ++i) {
    expanded_data[0x7FC0 + i] = static_cast<uint8_t>(expanded_title[i]);
  }
  {
    std::ofstream out(expanded_backup_path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(out.is_open());
    out.write(reinterpret_cast<const char*>(expanded_data.data()),
              static_cast<std::streamsize>(expanded_data.size()));
    ASSERT_TRUE(out.good());
  }

  ASSERT_OK(manager->RestoreRomBackup(expanded_backup_path.string()));
  EXPECT_EQ(manager->GetCurrentRom(), active_rom);
  EXPECT_EQ(active_rom->size(), 1024u * 1024u);
  EXPECT_NE(active_rom->title().find(expanded_title), std::string::npos);
  EXPECT_TRUE(active_rom->dirty());
}

TEST(EditorManagerBackupRestoreTest,
     RestoreRejectsProjectBuildOutputBackingPath) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = true;

  const auto temp_dir = MakeTempFilePath("yaze_backup_restore_build_output");
  ScopedDirectoryCleanup cleanup{temp_dir};
  ASSERT_TRUE(std::filesystem::create_directories(temp_dir));
  const auto dev_rom_path = temp_dir / "oracle-dev.sfc";
  const auto build_rom_path = temp_dir / "oracle-build.sfc";
  WriteTestRom(dev_rom_path, "RESTORE DEV");
  WriteTestRom(build_rom_path, "RESTORE BUILD");

  ASSERT_OK(manager->OpenRomOrProject(build_rom_path.string()));
  DisableRomWritesForTest();
  Rom* const active_rom = manager->GetCurrentRom();
  ASSERT_NE(active_rom, nullptr);

  constexpr uint32_t kPcOffset = 0x1234;
  constexpr uint8_t kEdited = 0xA5;
  ASSERT_OK(active_rom->WriteByte(kPcOffset, kEdited));
  ASSERT_OK(manager->SaveRom());

  const auto backups = manager->GetRomBackups();
  ASSERT_EQ(backups.size(), 1u);

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "Backup Restore Build Output";
  project->filepath = (temp_dir / "project.yaze").string();
  project->rom_filename = dev_rom_path.filename().string();
  project->build_target = build_rom_path.filename().string();
  project->rom_metadata.write_policy = project::RomWritePolicy::kWarn;

  const auto restore_status = manager->RestoreRomBackup(backups.front().path);

  EXPECT_EQ(restore_status.code(), absl::StatusCode::kFailedPrecondition)
      << restore_status;
  EXPECT_NE(std::string(restore_status.message()).find("build output"),
            std::string::npos);
  EXPECT_EQ(manager->GetCurrentRom(), active_rom);
  ASSERT_TRUE(active_rom->ReadByte(kPcOffset).ok());
  EXPECT_EQ(*active_rom->ReadByte(kPcOffset), kEdited);
  EXPECT_FALSE(active_rom->dirty());
  EXPECT_FALSE(manager->IsRomBackupRestorePending());
  EXPECT_EQ(ReadByteAt(build_rom_path, kPcOffset), kEdited);
}

TEST(EditorManagerBackupRestoreTest,
     DiscardPendingRomBackupRestoreReloadsBackingWithoutWriting) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = true;

  const auto temp_dir = MakeTempFilePath("yaze_backup_restore_discard");
  ScopedDirectoryCleanup cleanup{temp_dir};
  ASSERT_TRUE(std::filesystem::create_directories(temp_dir));
  const auto rom_path = temp_dir / "oracle-copy.sfc";
  WriteTestRom(rom_path, "RESTORE DISCARD");
  {
    std::ofstream labels(rom_path.string() + ".labels",
                         std::ios::out | std::ios::trunc);
    ASSERT_TRUE(labels.is_open());
    labels << "[rooms]\n0=On-disk room label\n";
    ASSERT_TRUE(labels.good());
  }

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();
  Rom* const active_rom = manager->GetCurrentRom();
  ASSERT_NE(active_rom, nullptr);
  active_rom->resource_label()->EditLabel("rooms", "0", "In-memory room label");

  constexpr uint32_t kPcOffset = 0x1234;
  constexpr uint8_t kOriginal = 0x00;
  constexpr uint8_t kEdited = 0xA5;
  ASSERT_OK(active_rom->WriteByte(kPcOffset, kEdited));
  ASSERT_OK(manager->SaveRom());
  const std::string backing_hash = manager->GetCurrentRomHash();
  const size_t session_id = manager->GetCurrentSessionId();

  const auto no_restore_status = manager->DiscardPendingRomBackupRestore();
  EXPECT_EQ(no_restore_status.code(), absl::StatusCode::kFailedPrecondition)
      << no_restore_status;
  EXPECT_EQ(ReadByteAt(rom_path, kPcOffset), kEdited);

  const auto backups = manager->GetRomBackups();
  ASSERT_EQ(backups.size(), 1u);
  ASSERT_OK(manager->RestoreRomBackup(backups.front().path));
  ASSERT_TRUE(active_rom->ReadByte(kPcOffset).ok());
  EXPECT_EQ(*active_rom->ReadByte(kPcOffset), kOriginal);
  EXPECT_EQ(ReadByteAt(rom_path, kPcOffset), kEdited);
  EXPECT_NE(manager->GetCurrentRomHash(), backing_hash);

  auto* const session = manager->session_coordinator()->GetActiveRomSession();
  ASSERT_NE(session, nullptr);
  ASSERT_TRUE(session->backup_restore_pending);
  ASSERT_TRUE(active_rom->dirty());

  ASSERT_OK(manager->DiscardPendingRomBackupRestore());

  EXPECT_EQ(manager->GetCurrentRom(), active_rom);
  EXPECT_EQ(manager->GetCurrentSessionId(), session_id);
  EXPECT_EQ(active_rom->filename(),
            std::filesystem::absolute(rom_path).string());
  ASSERT_TRUE(active_rom->ReadByte(kPcOffset).ok());
  EXPECT_EQ(*active_rom->ReadByte(kPcOffset), kEdited);
  EXPECT_EQ(ReadByteAt(rom_path, kPcOffset), kEdited);
  EXPECT_FALSE(active_rom->dirty());
  EXPECT_FALSE(session->backup_restore_pending);
  EXPECT_EQ(manager->GetCurrentRomHash(), backing_hash);
  EXPECT_EQ(active_rom->resource_label()->GetLabel("rooms", "0"),
            "In-memory room label");
}

TEST(EditorManagerBackupRestoreTest,
     DiscardPendingRomBackupRestoreRejectsPendingDungeonMetadata) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = true;

  const auto temp_dir =
      MakeTempFilePath("yaze_backup_restore_discard_pending_room");
  ScopedDirectoryCleanup cleanup{temp_dir};
  ASSERT_TRUE(std::filesystem::create_directories(temp_dir));
  const auto rom_path = temp_dir / "oracle-copy.sfc";
  WriteTestRom(rom_path, "DISCARD PENDING ROOM");

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();
  Rom* const active_rom = manager->GetCurrentRom();
  ASSERT_NE(active_rom, nullptr);

  constexpr uint32_t kPcOffset = 0x1234;
  constexpr uint8_t kOriginal = 0x00;
  constexpr uint8_t kEdited = 0xA5;
  ASSERT_OK(active_rom->WriteByte(kPcOffset, kEdited));
  ASSERT_OK(manager->SaveRom());

  const auto backups = manager->GetRomBackups();
  ASSERT_EQ(backups.size(), 1u);
  ASSERT_OK(manager->RestoreRomBackup(backups.front().path));

  auto* const session = manager->session_coordinator()->GetActiveRomSession();
  ASSERT_NE(session, nullptr);
  ASSERT_TRUE(session->backup_restore_pending);
  const std::string staged_hash = manager->GetCurrentRomHash();

  auto* const game_data = manager->GetCurrentGameData();
  ASSERT_NE(game_data, nullptr);
  game_data->pit_damage_table.MarkDirty();
  ASSERT_TRUE(game_data->pit_damage_table.dirty());
  auto* const dungeon =
      manager->GetCurrentEditorSet()->GetEditorAs<DungeonEditorV2>(
          EditorType::kDungeon);
  ASSERT_NE(dungeon, nullptr);
  ASSERT_EQ(dungeon->PendingRoomCount(), 0);
  ASSERT_TRUE(dungeon->HasPendingDungeonChanges());

  const auto discard_status = manager->DiscardPendingRomBackupRestore();

  EXPECT_EQ(discard_status.code(), absl::StatusCode::kFailedPrecondition)
      << discard_status;
  EXPECT_NE(std::string(discard_status.message())
                .find("pending graphics, screen, dungeon, or palette edits"),
            std::string::npos);
  EXPECT_EQ(manager->GetCurrentRom(), active_rom);
  ASSERT_TRUE(active_rom->ReadByte(kPcOffset).ok());
  EXPECT_EQ(*active_rom->ReadByte(kPcOffset), kOriginal);
  EXPECT_EQ(manager->GetCurrentRomHash(), staged_hash);
  EXPECT_TRUE(active_rom->dirty());
  EXPECT_TRUE(session->backup_restore_pending);
  EXPECT_TRUE(game_data->pit_damage_table.dirty());
  EXPECT_TRUE(dungeon->HasPendingDungeonChanges());
  EXPECT_EQ(ReadByteAt(rom_path, kPcOffset), kEdited);
}

TEST(EditorManagerBackupRestoreTest,
     DiscardPendingRomBackupRestoreRejectsPendingPaletteWrite) {
  FeatureFlagsGuard guard;
  PaletteManagerGuard palette_guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = true;

  const auto temp_dir =
      MakeTempFilePath("yaze_backup_restore_discard_pending_palette");
  ScopedDirectoryCleanup cleanup{temp_dir};
  ASSERT_TRUE(std::filesystem::create_directories(temp_dir));
  const auto rom_path = temp_dir / "oracle-copy.sfc";
  WriteTestRom(rom_path, "DISCARD PENDING PALETTE");

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();
  Rom* const active_rom = manager->GetCurrentRom();
  ASSERT_NE(active_rom, nullptr);

  constexpr uint32_t kPcOffset = 0x1234;
  constexpr uint8_t kOriginal = 0x00;
  constexpr uint8_t kEdited = 0xA5;
  ASSERT_OK(active_rom->WriteByte(kPcOffset, kEdited));
  ASSERT_OK(manager->SaveRom());

  const auto backups = manager->GetRomBackups();
  ASSERT_EQ(backups.size(), 1u);
  ASSERT_OK(manager->RestoreRomBackup(backups.front().path));

  auto* const session = manager->session_coordinator()->GetActiveRomSession();
  ASSERT_NE(session, nullptr);
  ASSERT_TRUE(session->backup_restore_pending);
  const std::string staged_hash = manager->GetCurrentRomHash();

  auto* const game_data = manager->GetCurrentGameData();
  ASSERT_NE(game_data, nullptr);
  auto* const group = game_data->palette_groups.get_group("dungeon_main");
  ASSERT_NE(group, nullptr);
  group->clear();
  gfx::SnesPalette palette;
  palette.AddColor(gfx::SnesColor(0x0100));
  group->AddPalette(palette);
  gfx::PaletteManager::Get().Initialize(game_data);
  ASSERT_OK(gfx::PaletteManager::Get().SetColor("dungeon_main", 0, 0,
                                                gfx::SnesColor(0x1111)));
  ASSERT_TRUE(gfx::PaletteManager::Get().HasUnsavedChanges(game_data));

  const auto discard_status = manager->DiscardPendingRomBackupRestore();

  EXPECT_EQ(discard_status.code(), absl::StatusCode::kFailedPrecondition)
      << discard_status;
  EXPECT_NE(std::string(discard_status.message())
                .find("pending graphics, screen, dungeon, or palette edits"),
            std::string::npos);
  EXPECT_EQ(manager->GetCurrentRom(), active_rom);
  ASSERT_TRUE(active_rom->ReadByte(kPcOffset).ok());
  EXPECT_EQ(*active_rom->ReadByte(kPcOffset), kOriginal);
  EXPECT_EQ(manager->GetCurrentRomHash(), staged_hash);
  EXPECT_TRUE(active_rom->dirty());
  EXPECT_TRUE(session->backup_restore_pending);
  EXPECT_TRUE(gfx::PaletteManager::Get().HasUnsavedChanges(game_data));
  EXPECT_EQ(gfx::PaletteManager::Get().GetColor("dungeon_main", 0, 0).snes(),
            0x1111);
  EXPECT_EQ(ReadByteAt(rom_path, kPcOffset), kEdited);
}

TEST(EditorManagerBackupRestoreTest,
     DiscardPendingRomBackupRestoreFailureKeepsStagedState) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = true;

  const auto temp_dir = MakeTempFilePath("yaze_backup_restore_discard_failure");
  ScopedDirectoryCleanup cleanup{temp_dir};
  ASSERT_TRUE(std::filesystem::create_directories(temp_dir));
  const auto rom_path = temp_dir / "oracle-copy.sfc";
  const auto moved_rom_path = temp_dir / "oracle-copy-moved.sfc";
  WriteTestRom(rom_path, "DISCARD FAILURE");

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();
  Rom* const active_rom = manager->GetCurrentRom();
  ASSERT_NE(active_rom, nullptr);

  constexpr uint32_t kPcOffset = 0x1234;
  constexpr uint8_t kOriginal = 0x00;
  constexpr uint8_t kEdited = 0xA5;
  ASSERT_OK(active_rom->WriteByte(kPcOffset, kEdited));
  ASSERT_OK(manager->SaveRom());

  const auto backups = manager->GetRomBackups();
  ASSERT_EQ(backups.size(), 1u);
  ASSERT_OK(manager->RestoreRomBackup(backups.front().path));
  auto* const session = manager->session_coordinator()->GetActiveRomSession();
  ASSERT_NE(session, nullptr);
  ASSERT_TRUE(session->backup_restore_pending);
  const std::string staged_hash = manager->GetCurrentRomHash();
  const std::string staged_title = active_rom->title();
  const size_t staged_size = active_rom->size();

  std::filesystem::rename(rom_path, moved_rom_path);
  const auto discard_status = manager->DiscardPendingRomBackupRestore();

  EXPECT_FALSE(discard_status.ok()) << discard_status;
  EXPECT_EQ(manager->GetCurrentRom(), active_rom);
  EXPECT_EQ(active_rom->filename(),
            std::filesystem::absolute(rom_path).string());
  ASSERT_TRUE(active_rom->ReadByte(kPcOffset).ok());
  EXPECT_EQ(*active_rom->ReadByte(kPcOffset), kOriginal);
  EXPECT_EQ(active_rom->title(), staged_title);
  EXPECT_EQ(active_rom->size(), staged_size);
  EXPECT_EQ(manager->GetCurrentRomHash(), staged_hash);
  EXPECT_TRUE(active_rom->dirty());
  EXPECT_TRUE(session->backup_restore_pending);
  EXPECT_EQ(ReadByteAt(moved_rom_path, kPcOffset), kEdited);
}

TEST(EditorManagerBackupRestoreTest,
     SingleSessionCloseDoesNotOfferImpossibleRestoreDiscard) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = true;

  const auto temp_dir = MakeTempFilePath("yaze_backup_restore_single_close");
  ScopedDirectoryCleanup cleanup{temp_dir};
  ASSERT_TRUE(std::filesystem::create_directories(temp_dir));
  const auto rom_path = temp_dir / "oracle-copy.sfc";
  WriteTestRom(rom_path, "RESTORE CLOSE");

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();
  Rom* const active_rom = manager->GetCurrentRom();
  ASSERT_NE(active_rom, nullptr);

  constexpr uint32_t kPcOffset = 0x1234;
  constexpr uint8_t kOriginal = 0x00;
  constexpr uint8_t kEdited = 0xA5;
  ASSERT_OK(active_rom->WriteByte(kPcOffset, kEdited));
  ASSERT_OK(manager->SaveRom());

  const auto backups = manager->GetRomBackups();
  ASSERT_EQ(backups.size(), 1u);
  ASSERT_OK(manager->RestoreRomBackup(backups.front().path));
  auto* const session = manager->session_coordinator()->GetActiveRomSession();
  ASSERT_NE(session, nullptr);
  ASSERT_TRUE(session->backup_restore_pending);

  manager->CloseCurrentSession();

  EXPECT_EQ(manager->GetActiveSessionCount(), 1u);
  EXPECT_FALSE(manager->HasPendingUnsavedSessionAction());
  EXPECT_EQ(manager->GetCurrentRom(), active_rom);
  ASSERT_TRUE(active_rom->ReadByte(kPcOffset).ok());
  EXPECT_EQ(*active_rom->ReadByte(kPcOffset), kOriginal);
  EXPECT_TRUE(active_rom->dirty());
  EXPECT_TRUE(session->backup_restore_pending);
  EXPECT_EQ(ReadByteAt(rom_path, kPcOffset), kEdited);
}

TEST(EditorManagerRomWritePolicyTest,
     SaveRomBlocksOnHashMismatchWhenPolicyBlock) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const std::filesystem::path rom_path =
      MakeTempFilePath("yaze_rom_write_policy_block.sfc");
  ScopedFileCleanup cleanup{rom_path};

  std::vector<uint8_t> rom_data(512 * 1024, 0x00);
  const std::string title = "YAZE TEST ROM";
  for (size_t i = 0; i < title.size() && (0x7FC0 + i) < rom_data.size(); ++i) {
    rom_data[0x7FC0 + i] = static_cast<uint8_t>(title[i]);
  }
  {
    std::ofstream out(rom_path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(out.is_open());
    out.write(reinterpret_cast<const char*>(rom_data.data()),
              static_cast<std::streamsize>(rom_data.size()));
    ASSERT_TRUE(out.good());
  }

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "RomWritePolicyBlock";
  project->filepath = (rom_path.parent_path() / "project.yaze").string();
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.write_policy = project::RomWritePolicy::kBlock;

  ASSERT_FALSE(manager->GetCurrentRomHash().empty());
  project->rom_metadata.expected_hash = "deadbeef";
  ASSERT_TRUE(manager->IsRomHashMismatch());

  Rom* rom = manager->GetCurrentRom();
  ASSERT_NE(rom, nullptr);

  constexpr uint32_t kPcOffset = 0x1234;
  const uint8_t original = rom_data[kPcOffset];
  const uint8_t mutated = static_cast<uint8_t>(original ^ 0xFF);
  ASSERT_OK(rom->WriteByte(static_cast<int>(kPcOffset), mutated));

  auto status = manager->SaveRom();
  EXPECT_EQ(status.code(), absl::StatusCode::kPermissionDenied) << status;
  EXPECT_FALSE(manager->IsRomWriteConfirmPending());
  EXPECT_EQ(ReadByteAt(rom_path, static_cast<std::streamoff>(kPcOffset)),
            original);
}

TEST(EditorManagerRomWritePolicyTest,
     EditableProjectTargetBypassesHashMismatchChecks) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const std::filesystem::path dev_rom_path =
      MakeTempFilePath("yaze_project_dev_target.sfc");
  const std::filesystem::path build_rom_path =
      MakeTempFilePath("yaze_project_build_target.sfc");
  ScopedFileCleanup dev_cleanup{dev_rom_path};
  ScopedFileCleanup build_cleanup{build_rom_path};
  WriteTestRom(dev_rom_path, "YAZE DEV TARGET");
  WriteTestRom(build_rom_path, "YAZE BUILD OUT");

  ASSERT_OK(manager->OpenRomOrProject(dev_rom_path.string()));
  DisableRomWritesForTest();

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "EditableProjectTarget";
  project->filepath = (dev_rom_path.parent_path() / "project.yaze").string();
  project->workspace_settings.backup_on_save = false;
  project->rom_filename = dev_rom_path.filename().string();
  project->build_target = build_rom_path.filename().string();
  project->rom_metadata.write_policy = project::RomWritePolicy::kWarn;
  ASSERT_TRUE(
      project->hack_manifest
          .LoadFromString(absl::StrFormat(R"json(
{
  "manifest_version": 2,
  "hack_name": "Editable Project Target",
  "build_pipeline": {
    "dev_rom": "%s",
    "patched_rom": "%s"
  }
}
)json",
                                          dev_rom_path.filename().string(),
                                          build_rom_path.filename().string()))
          .ok());

  ASSERT_FALSE(manager->GetCurrentRomHash().empty());
  project->rom_metadata.expected_hash = "deadbeef";
  EXPECT_FALSE(manager->IsRomHashMismatch());
}

TEST(EditorManagerRomWritePolicyTest,
     SaveRomBlocksWhenLoadedRomMatchesProjectBuildOutput) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const std::filesystem::path dev_rom_path =
      MakeTempFilePath("yaze_build_output_editable.sfc");
  const std::filesystem::path build_rom_path =
      MakeTempFilePath("yaze_build_output_loaded.sfc");
  ScopedFileCleanup dev_cleanup{dev_rom_path};
  ScopedFileCleanup build_cleanup{build_rom_path};
  WriteTestRom(dev_rom_path, "YAZE DEV TARGET");
  WriteTestRom(build_rom_path, "YAZE BUILD OUT");

  ASSERT_OK(manager->OpenRomOrProject(build_rom_path.string()));
  DisableRomWritesForTest();

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "BuildOutputLoaded";
  project->filepath = (build_rom_path.parent_path() / "project.yaze").string();
  project->workspace_settings.backup_on_save = false;
  project->rom_filename = dev_rom_path.filename().string();
  project->build_target = build_rom_path.filename().string();
  project->rom_metadata.write_policy = project::RomWritePolicy::kWarn;
  ASSERT_TRUE(
      project->hack_manifest
          .LoadFromString(absl::StrFormat(R"json(
{
  "manifest_version": 2,
  "hack_name": "Build Output Loaded",
  "build_pipeline": {
    "dev_rom": "%s",
    "patched_rom": "%s"
  }
}
)json",
                                          dev_rom_path.filename().string(),
                                          build_rom_path.filename().string()))
          .ok());

  Rom* rom = manager->GetCurrentRom();
  ASSERT_NE(rom, nullptr);
  ASSERT_TRUE(rom->is_loaded());

  constexpr uint32_t kPcOffset = 0x1234;
  const uint8_t original =
      ReadByteAt(build_rom_path, static_cast<std::streamoff>(kPcOffset));
  const uint8_t mutated = static_cast<uint8_t>(original ^ 0xFF);
  ASSERT_OK(rom->WriteByte(static_cast<int>(kPcOffset), mutated));

  auto status = manager->SaveRom();
  EXPECT_EQ(status.code(), absl::StatusCode::kPermissionDenied) << status;
  EXPECT_FALSE(manager->IsRomWriteConfirmPending());
  EXPECT_EQ(ReadByteAt(build_rom_path, static_cast<std::streamoff>(kPcOffset)),
            original);
}

TEST(EditorManagerRomWritePolicyTest,
     SaveRomAsPreservesTargetAcrossWriteConfirmation) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto source_path = MakeTempFilePath("yaze_save_as_confirm_source.sfc");
  const auto target_path = MakeTempFilePath("yaze_save_as_confirm_target.sfc");
  ScopedFileCleanup source_cleanup{source_path};
  ScopedFileCleanup target_cleanup{target_path};
  WriteTestRom(source_path);

  ASSERT_OK(manager->OpenRomOrProject(source_path.string()));
  DisableRomWritesForTest();

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "SaveAsConfirmation";
  project->filepath = (source_path.parent_path() / "project.yaze").string();
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.write_policy = project::RomWritePolicy::kWarn;
  project->rom_metadata.expected_hash = "deadbeef";

  Rom* rom = manager->GetCurrentRom();
  ASSERT_NE(rom, nullptr);
  constexpr uint32_t kPcOffset = 0x1234;
  constexpr uint8_t kMutated = 0xA5;
  ASSERT_OK(rom->WriteByte(kPcOffset, kMutated));

  auto status = manager->SaveRomAs(target_path.string());
  EXPECT_EQ(status.code(), absl::StatusCode::kCancelled) << status;
  EXPECT_TRUE(manager->IsRomWriteConfirmPending());
  EXPECT_EQ(ReadByteAt(source_path, kPcOffset), 0x00);
  EXPECT_FALSE(std::filesystem::exists(target_path));

  manager->ConfirmRomWrite();
  auto resumed = manager->ResumePendingRomSave();
  ASSERT_OK(resumed);

  EXPECT_EQ(ReadByteAt(source_path, kPcOffset), 0x00);
  EXPECT_EQ(ReadByteAt(target_path, kPcOffset), kMutated);
  EXPECT_EQ(rom->filename(), target_path.string());
}

TEST(EditorManagerRomWritePolicyTest, SaveRomAsCannotTargetProjectBuildOutput) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto dev_rom_path = MakeTempFilePath("yaze_save_as_policy_dev.sfc");
  const auto build_rom_path = MakeTempFilePath("yaze_save_as_policy_build.sfc");
  ScopedFileCleanup dev_cleanup{dev_rom_path};
  ScopedFileCleanup build_cleanup{build_rom_path};
  WriteTestRom(dev_rom_path, "YAZE DEV TARGET");
  WriteTestRom(build_rom_path, "YAZE BUILD OUT");

  ASSERT_OK(manager->OpenRomOrProject(dev_rom_path.string()));
  DisableRomWritesForTest();

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "SaveAsBuildOutputPolicy";
  project->filepath = (dev_rom_path.parent_path() / "project.yaze").string();
  project->workspace_settings.backup_on_save = false;
  project->rom_filename = dev_rom_path.filename().string();
  project->rom_metadata.write_policy = project::RomWritePolicy::kWarn;
  project->rom_metadata.expected_hash.clear();
  ASSERT_OK(project->hack_manifest.LoadFromString(absl::StrFormat(
      R"json({
        "manifest_version": 2,
        "hack_name": "Save As Build Output Policy",
        "build_pipeline": {
          "dev_rom": "%s",
          "patched_rom": "%s"
        }
      })json",
      dev_rom_path.filename().string(), build_rom_path.filename().string())));

  Rom* rom = manager->GetCurrentRom();
  ASSERT_NE(rom, nullptr);
  constexpr uint32_t kPcOffset = 0x1234;
  ASSERT_OK(rom->WriteByte(kPcOffset, 0xA5));

  auto status = manager->SaveRomAs(build_rom_path.string());
  EXPECT_EQ(status.code(), absl::StatusCode::kPermissionDenied) << status;
  EXPECT_EQ(ReadByteAt(dev_rom_path, kPcOffset), 0x00);
  EXPECT_EQ(ReadByteAt(build_rom_path, kPcOffset), 0x00);
  EXPECT_EQ(rom->filename(), dev_rom_path.string());
}

TEST(EditorManagerRomWritePolicyTest, SaveRomAsRefreshesLifecycleHashAndPath) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto source_path = MakeTempFilePath("yaze_save_as_hash_source.sfc");
  const auto target_path = MakeTempFilePath("yaze_save_as_hash_target.sfc");
  ScopedFileCleanup source_cleanup{source_path};
  ScopedFileCleanup target_cleanup{target_path};
  WriteTestRom(source_path);

  ASSERT_OK(manager->OpenRomOrProject(source_path.string()));
  DisableRomWritesForTest();

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "SaveAsLifecycleRefresh";
  project->filepath = (source_path.parent_path() / "project.yaze").string();
  project->workspace_settings.backup_on_save = false;
  project->rom_filename = source_path.string();
  project->rom_metadata.write_policy = project::RomWritePolicy::kAllow;
  project->rom_metadata.expected_hash = manager->GetCurrentRomHash();
  EXPECT_FALSE(manager->IsRomHashMismatch());

  Rom* rom = manager->GetCurrentRom();
  ASSERT_NE(rom, nullptr);
  ASSERT_OK(rom->WriteByte(0x1234, 0xA5));
  ASSERT_OK(manager->SaveRomAs(target_path.string()));

  EXPECT_EQ(rom->filename(), target_path.string());
  EXPECT_TRUE(manager->IsRomHashMismatch());
}

TEST(EditorManagerRomWritePolicyTest,
     FailedResumeConsumesHashConfirmationBypass) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto source_path = MakeTempFilePath("yaze_resume_failure_source.sfc");
  const auto target_path = MakeTempFilePath("yaze_resume_failure_target.sfc");
  ScopedFileCleanup source_cleanup{source_path};
  ScopedFileCleanup target_cleanup{target_path};
  WriteTestRom(source_path);

  ASSERT_OK(manager->OpenRomOrProject(source_path.string()));
  DisableRomWritesForTest();

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "ResumeFailureConsumesBypass";
  project->filepath = (source_path.parent_path() / "project.yaze").string();
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.write_policy = project::RomWritePolicy::kWarn;
  project->rom_metadata.expected_hash = "deadbeef";
  project->hack_manifest.Clear();
  project->hack_manifest_file =
      (source_path.parent_path() / "missing-save-manifest.json").string();

  Rom* rom = manager->GetCurrentRom();
  ASSERT_NE(rom, nullptr);
  ASSERT_OK(rom->WriteByte(0x1234, 0xA5));

  EXPECT_TRUE(absl::IsCancelled(manager->SaveRomAs(target_path.string())));
  manager->ConfirmRomWrite();
  const auto failed_resume = manager->ResumePendingRomSave();
  EXPECT_EQ(failed_resume.code(), absl::StatusCode::kFailedPrecondition)
      << failed_resume;

  // Removing the downstream failure must not let the next, unrelated save
  // inherit the prior confirmation.
  project->hack_manifest_file.clear();
  const auto retry = manager->SaveRom();
  EXPECT_TRUE(absl::IsCancelled(retry)) << retry;
  EXPECT_TRUE(manager->IsRomWriteConfirmPending());
  EXPECT_EQ(ReadByteAt(source_path, 0x1234), 0x00);
  EXPECT_FALSE(std::filesystem::exists(target_path));
}

TEST(EditorManagerRomWritePolicyTest,
     CancellingLaterPromptConsumesHashConfirmationBypass) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto source_path = MakeTempFilePath("yaze_prompt_cancel_source.sfc");
  const auto target_path = MakeTempFilePath("yaze_prompt_cancel_target.sfc");
  ScopedFileCleanup source_cleanup{source_path};
  ScopedFileCleanup target_cleanup{target_path};
  WriteTestRom(source_path);

  ASSERT_OK(manager->OpenRomOrProject(source_path.string()));
  DisableRomWritesForTest();
  core::FeatureFlags::get().dungeon.kSavePotItems = true;
  ASSERT_NE(manager->GetCurrentEditorSet()->GetEditor(EditorType::kDungeon),
            nullptr);

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "LaterPromptCancelConsumesBypass";
  project->filepath = (source_path.parent_path() / "project.yaze").string();
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.write_policy = project::RomWritePolicy::kWarn;
  project->rom_metadata.expected_hash = "deadbeef";

  Rom* rom = manager->GetCurrentRom();
  ASSERT_NE(rom, nullptr);
  ASSERT_OK(rom->WriteByte(0x1234, 0xA5));

  EXPECT_TRUE(absl::IsCancelled(manager->SaveRomAs(target_path.string())));
  manager->ConfirmRomWrite();
  EXPECT_TRUE(absl::IsCancelled(manager->ResumePendingRomSave()));
  EXPECT_TRUE(manager->HasPendingPotItemSaveConfirmation());

  manager->ResolvePotItemSaveConfirmation(
      EditorManager::PotItemSaveDecision::kCancel);
  core::FeatureFlags::get().dungeon.kSavePotItems = false;

  const auto retry = manager->SaveRom();
  EXPECT_TRUE(absl::IsCancelled(retry)) << retry;
  EXPECT_TRUE(manager->IsRomWriteConfirmPending());
  EXPECT_EQ(ReadByteAt(source_path, 0x1234), 0x00);
  EXPECT_FALSE(std::filesystem::exists(target_path));
}

TEST(EditorManagerRomWritePolicyTest,
     PendingSaveAsCannotResumeAfterSessionSwitch) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto source_a = MakeTempFilePath("yaze_session_a.sfc");
  const auto source_b = MakeTempFilePath("yaze_session_b.sfc");
  const auto target_a = MakeTempFilePath("yaze_session_a_target.sfc");
  ScopedFileCleanup cleanup_a{source_a};
  ScopedFileCleanup cleanup_b{source_b};
  ScopedFileCleanup cleanup_target{target_a};
  WriteTestRom(source_a, "YAZE SESSION A");
  WriteTestRom(source_b, "YAZE SESSION B");

  ASSERT_OK(manager->OpenRomOrProject(source_a.string()));
  const size_t session_a = manager->GetCurrentSessionIndex();
  ASSERT_OK(manager->OpenRomOrProject(source_b.string()));
  const size_t session_b = manager->GetCurrentSessionIndex();
  const std::string session_b_hash = manager->GetCurrentRomHash();
  ASSERT_NE(session_a, session_b);

  manager->SwitchToSession(session_a);
  DisableRomWritesForTest();

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "PendingSaveSessionBinding";
  project->filepath = (source_a.parent_path() / "project.yaze").string();
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.write_policy = project::RomWritePolicy::kWarn;
  project->rom_metadata.expected_hash = "deadbeef";

  ASSERT_OK(manager->GetCurrentRom()->WriteByte(0x1234, 0xA5));
  EXPECT_TRUE(absl::IsCancelled(manager->SaveRomAs(target_a.string())));
  EXPECT_TRUE(manager->IsRomWriteConfirmPending());

  manager->SwitchToSession(session_b);
  ASSERT_TRUE(manager->HasPendingUnsavedSessionAction());
  manager->ConfirmPendingUnsavedSessionActionDiscardAndContinue();
  ASSERT_EQ(manager->GetCurrentSessionIndex(), session_b);
  EXPECT_EQ(manager->GetCurrentRomHash(), session_b_hash);

  // Simulate a stale confirmation callback after the session switch. It must
  // not serialize session B or write it to session A's Save As target.
  manager->ConfirmRomWrite();
  const auto stale_resume = manager->ResumePendingRomSave();
  EXPECT_EQ(stale_resume.code(), absl::StatusCode::kFailedPrecondition)
      << stale_resume;
  EXPECT_FALSE(std::filesystem::exists(target_a));
  EXPECT_EQ(ReadByteAt(source_a, 0x1234), 0x00);
  EXPECT_EQ(ReadByteAt(source_b, 0x1234), 0x00);
}

TEST(GraphicsSaveStoplossTest,
     CleanSaveDoesNotMaterializeTheLazyGraphicsEditor) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = false;

  const auto rom_path = MakeTempFilePath("yaze_clean_graphics_save.sfc");
  ScopedFileCleanup cleanup{rom_path};
  WriteTestRom(rom_path, "CLEAN GRAPHICS SAVE");

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();
  core::FeatureFlags::get().kSaveGraphicsSheet = true;

  auto* editor_set = manager->GetCurrentEditorSet();
  ASSERT_NE(editor_set, nullptr);
  EXPECT_EQ(editor_set->GetExistingEditor(EditorType::kGraphics), nullptr);
  EXPECT_FALSE(editor_set->HasPendingGraphicsChanges());
  EXPECT_EQ(editor_set->GetExistingEditor(EditorType::kGraphics), nullptr);

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.expected_hash.clear();

  ASSERT_OK(manager->SaveRom());
  EXPECT_EQ(editor_set->GetExistingEditor(EditorType::kGraphics), nullptr);
}

TEST(GraphicsSaveStoplossTest,
     PendingSheetsBlockBothSaveScopeStatesBeforeSerialization) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto rom_path = MakeTempFilePath("yaze_pending_graphics_save.sfc");
  ScopedFileCleanup cleanup{rom_path};
  WriteTestRom(rom_path, "PENDING GRAPHICS");

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.expected_hash.clear();

  auto* editor_set = manager->GetCurrentEditorSet();
  ASSERT_NE(editor_set, nullptr);
  auto* graphics =
      editor_set->GetEditorAs<GraphicsEditor>(EditorType::kGraphics);
  ASSERT_NE(graphics, nullptr);
  GraphicsEditorSaveStoplossTestPeer::MarkSheetModified(graphics, 0x20);
  ASSERT_TRUE(editor_set->HasPendingGraphicsChanges());

  Rom* rom = manager->GetCurrentRom();
  ASSERT_NE(rom, nullptr);
  EXPECT_FALSE(rom->dirty());
  EXPECT_TRUE(manager->session_coordinator()->IsSessionModified(
      manager->GetCurrentSessionIndex()));

  manager->Quit();
  EXPECT_TRUE(manager->HasPendingUnsavedSessionAction());
  manager->CancelPendingUnsavedSessionAction();

  const bool screen_existed =
      editor_set->GetExistingEditor(EditorType::kScreen) != nullptr;
  const bool dungeon_existed =
      editor_set->GetExistingEditor(EditorType::kDungeon) != nullptr;
  const bool overworld_existed =
      editor_set->GetExistingEditor(EditorType::kOverworld) != nullptr;

  constexpr uint32_t kPcOffset = 0x1234;
  constexpr uint8_t kPendingRomByte = 0xA5;
  ASSERT_OK(rom->WriteByte(kPcOffset, kPendingRomByte));

  for (const bool graphics_scope_enabled : {false, true}) {
    core::FeatureFlags::get().kSaveGraphicsSheet = graphics_scope_enabled;
    const auto status = manager->SaveRom();

    EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition) << status;
    EXPECT_NE(
        std::string(status.message()).find("graphics sheet edits are pending"),
        std::string::npos);
    EXPECT_NE(std::string(status.message())
                  .find(graphics_scope_enabled ? "enabled" : "disabled"),
              std::string::npos);
    EXPECT_EQ(ReadByteAt(rom_path, kPcOffset), 0x00);
    ASSERT_TRUE(rom->ReadByte(kPcOffset).ok());
    EXPECT_EQ(*rom->ReadByte(kPcOffset), kPendingRomByte);
    EXPECT_TRUE(editor_set->HasPendingGraphicsChanges());
  }

  EXPECT_EQ(editor_set->GetExistingEditor(EditorType::kScreen) != nullptr,
            screen_existed);
  EXPECT_EQ(editor_set->GetExistingEditor(EditorType::kDungeon) != nullptr,
            dungeon_existed);
  EXPECT_EQ(editor_set->GetExistingEditor(EditorType::kOverworld) != nullptr,
            overworld_existed);
}

TEST(GraphicsSaveStoplossTest, PixelUndoAndRedoRemarkTheSheetDirty) {
  constexpr uint16_t kSheetId = 0x20;
  const std::vector<uint8_t> before_data = {0x01, 0x02, 0x03, 0x04};
  const std::vector<uint8_t> after_data = {0x05, 0x06, 0x07, 0x08};

  GraphicsEditorState state;
  auto& sheet = gfx::Arena::Get().mutable_gfx_sheets()->at(kSheetId);
  ScopedGraphicsSheetRestore restore_sheet(&sheet);
  sheet.set_data(after_data);

  GraphicsPixelEditAction action(
      kSheetId, before_data, after_data, "Edit pixels",
      [&state](uint16_t sheet_id) { state.MarkSheetModified(sheet_id); });

  ASSERT_FALSE(state.HasUnsavedChanges());
  ASSERT_OK(action.Undo());
  EXPECT_EQ(sheet.vector(), before_data);
  EXPECT_TRUE(state.HasUnsavedChanges());
  EXPECT_TRUE(state.modified_sheets.contains(kSheetId));

  state.ClearModifiedSheets();
  ASSERT_OK(action.Redo());
  EXPECT_EQ(sheet.vector(), after_data);
  EXPECT_TRUE(state.HasUnsavedChanges());
  EXPECT_TRUE(state.modified_sheets.contains(kSheetId));
}

TEST(ScreenSaveStoplossTest,
     PendingQueryDoesNotMaterializeTheLazyScreenEditor) {
  EditorSet editor_set;

  EXPECT_EQ(editor_set.GetExistingEditor(EditorType::kScreen), nullptr);
  EXPECT_FALSE(editor_set.HasPendingScreenChanges());
  EXPECT_EQ(editor_set.GetExistingEditor(EditorType::kScreen), nullptr);
}

TEST(ScreenSaveStoplossTest,
     EveryPendingDomainBlocksLifecycleForBothScopeStates) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = false;

  const auto rom_path = MakeTempFilePath("yaze_pending_screen_save.sfc");
  ScopedFileCleanup cleanup{rom_path};
  WriteTestRom(rom_path, "PENDING SCREEN");

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.expected_hash.clear();

  auto* editor_set = manager->GetCurrentEditorSet();
  ASSERT_NE(editor_set, nullptr);
  auto* screen = editor_set->GetEditorAs<ScreenEditor>(EditorType::kScreen);
  ASSERT_NE(screen, nullptr);

  Rom* rom = manager->GetCurrentRom();
  ASSERT_NE(rom, nullptr);
  EXPECT_FALSE(rom->dirty());

  constexpr uint32_t kPcOffset = 0x1234;
  constexpr uint8_t kPendingRomByte = 0xA5;

  enum class PendingDomain {
    kDungeonMap,
    kDungeonMapTile16,
    kTitle,
    kOverworld,
    kOverworldPalette
  };
  for (const PendingDomain domain :
       {PendingDomain::kDungeonMap, PendingDomain::kDungeonMapTile16,
        PendingDomain::kTitle, PendingDomain::kOverworld,
        PendingDomain::kOverworldPalette}) {
    SCOPED_TRACE(static_cast<int>(domain));
    ScreenEditorSaveStoplossTestPeer::ClearPendingChanges(screen);
    switch (domain) {
      case PendingDomain::kDungeonMap:
        ScreenEditorSaveStoplossTestPeer::MarkDungeonMapModified(screen);
        break;
      case PendingDomain::kDungeonMapTile16:
        ScreenEditorSaveStoplossTestPeer::MarkDungeonMapTile16Modified(screen);
        break;
      case PendingDomain::kTitle:
        ScreenEditorSaveStoplossTestPeer::MarkTitleScreenModified(screen);
        break;
      case PendingDomain::kOverworld:
        ScreenEditorSaveStoplossTestPeer::MarkOverworldMapModified(screen);
        break;
      case PendingDomain::kOverworldPalette:
        ScreenEditorSaveStoplossTestPeer::MarkOverworldMapPaletteModified(
            screen);
        break;
    }

    ASSERT_TRUE(editor_set->HasPendingScreenChanges());
    EXPECT_EQ(screen->HasPendingDungeonMapChanges(),
              domain == PendingDomain::kDungeonMap);
    EXPECT_EQ(screen->HasPendingDungeonMapTile16Changes(),
              domain == PendingDomain::kDungeonMapTile16);
    EXPECT_EQ(screen->HasPendingTitleScreenChanges(),
              domain == PendingDomain::kTitle);
    EXPECT_EQ(screen->HasPendingOverworldMapChanges(),
              domain == PendingDomain::kOverworld ||
                  domain == PendingDomain::kOverworldPalette);
    EXPECT_TRUE(manager->session_coordinator()->IsSessionModified(
        manager->GetCurrentSessionIndex()));

    manager->Quit();
    ASSERT_TRUE(manager->HasPendingUnsavedSessionAction());
    EXPECT_NE(manager->GetPendingUnsavedSessionActionPrompt().find(
                  "unapplied Screen Editor edits"),
              std::string::npos);
    manager->CancelPendingUnsavedSessionAction();

    const auto autosave_status = manager->AutosaveActiveSession();
    EXPECT_EQ(autosave_status.code(), absl::StatusCode::kFailedPrecondition)
        << autosave_status;
    EXPECT_NE(std::string(autosave_status.message())
                  .find("Screen Editor edits are pending"),
              std::string::npos);

    auto* session = manager->session_coordinator()->GetActiveRomSession();
    ASSERT_NE(session, nullptr);
    session->backup_restore_pending = true;
    const auto discard_status = manager->DiscardPendingRomBackupRestore();
    EXPECT_EQ(discard_status.code(), absl::StatusCode::kFailedPrecondition)
        << discard_status;
    EXPECT_NE(std::string(discard_status.message())
                  .find("pending graphics, screen, dungeon, or palette edits"),
              std::string::npos);
    EXPECT_TRUE(session->backup_restore_pending);
    session->backup_restore_pending = false;

    ASSERT_OK(rom->WriteByte(kPcOffset, kPendingRomByte));
    for (const bool screen_scope_enabled : {false, true}) {
      core::FeatureFlags::get().kSaveDungeonMaps = screen_scope_enabled;
      const auto save_status = manager->SaveRom();

      EXPECT_EQ(save_status.code(), absl::StatusCode::kFailedPrecondition)
          << save_status;
      EXPECT_NE(std::string(save_status.message())
                    .find("Screen Editor edits are pending"),
                std::string::npos);
      EXPECT_NE(std::string(save_status.message())
                    .find(screen_scope_enabled ? "enabled" : "disabled"),
                std::string::npos);
      EXPECT_EQ(ReadByteAt(rom_path, kPcOffset), 0x00);
      ASSERT_TRUE(rom->ReadByte(kPcOffset).ok());
      EXPECT_EQ(*rom->ReadByte(kPcOffset), kPendingRomByte);
      EXPECT_TRUE(editor_set->HasPendingScreenChanges());
    }
  }
}

TEST(ScreenSaveStoplossTest, DirectEditorSaveFailsClosedForEveryDomain) {
  ScreenEditor editor;

  const std::array mark_pending = {
      &ScreenEditorSaveStoplossTestPeer::MarkDungeonMapModified,
      &ScreenEditorSaveStoplossTestPeer::MarkDungeonMapTile16Modified,
      &ScreenEditorSaveStoplossTestPeer::MarkTitleScreenModified,
      &ScreenEditorSaveStoplossTestPeer::MarkOverworldMapModified,
      &ScreenEditorSaveStoplossTestPeer::MarkOverworldMapPaletteModified,
  };
  for (const auto mark : mark_pending) {
    ScreenEditorSaveStoplossTestPeer::ClearPendingChanges(&editor);
    mark(&editor);
    const auto status = editor.Save();
    EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition) << status;
  }
}

TEST(ScreenSaveStoplossTest, MutationUndoAndRedoRemarkEachDomainPending) {
  ScreenEditor dungeon_map_editor;
  ScreenEditorSaveStoplossTestPeer::CommitDungeonMapEdit(&dungeon_map_editor);
  ASSERT_TRUE(dungeon_map_editor.HasPendingDungeonMapChanges());

  ScreenEditorSaveStoplossTestPeer::ClearPendingChanges(&dungeon_map_editor);
  ASSERT_OK(dungeon_map_editor.Undo());
  EXPECT_TRUE(dungeon_map_editor.HasPendingDungeonMapChanges());

  ScreenEditorSaveStoplossTestPeer::ClearPendingChanges(&dungeon_map_editor);
  ASSERT_OK(dungeon_map_editor.Redo());
  EXPECT_TRUE(dungeon_map_editor.HasPendingDungeonMapChanges());

  ScreenEditor tile16_editor;
  ScreenEditorSaveStoplossTestPeer::CommitDungeonMapTile16Edit(&tile16_editor);
  ASSERT_TRUE(tile16_editor.HasPendingDungeonMapTile16Changes());

  ScreenEditorSaveStoplossTestPeer::ClearPendingChanges(&tile16_editor);
  ASSERT_OK(tile16_editor.Undo());
  EXPECT_TRUE(tile16_editor.HasPendingDungeonMapTile16Changes());

  ScreenEditorSaveStoplossTestPeer::ClearPendingChanges(&tile16_editor);
  ASSERT_OK(tile16_editor.Redo());
  EXPECT_TRUE(tile16_editor.HasPendingDungeonMapTile16Changes());
}

TEST(ScreenSaveStoplossTest, ReloadResetClearsRomBackedStateAndHistory) {
  ScreenEditor editor;
  ScreenEditorSaveStoplossTestPeer::PrimeRomBackedState(&editor);
  ASSERT_TRUE(ScreenEditorSaveStoplossTestPeer::HasReloadResidue(editor));

  ScreenEditorSaveStoplossTestPeer::ResetRomBackedStateForLoad(&editor);

  EXPECT_FALSE(ScreenEditorSaveStoplossTestPeer::HasReloadResidue(editor));
}

TEST(ScreenSaveStoplossTest,
     RepeatedScreenModelCreateDoesNotAccumulatePaletteState) {
  ScopedImGuiContext imgui;
  ScopedTextureQueueClear texture_queue;

  const auto rom_path = MakeTempFilePath("yaze_screen_model_reload.sfc");
  ScopedFileCleanup cleanup{rom_path};
  WriteScreenModelTestRom(rom_path);

  Rom rom;
  ASSERT_OK(rom.LoadFromFile(rom_path.string()));
  zelda3::GameData game_data(&rom);
  PopulateTitleScreenTestGameData(&game_data);
  ScreenEditor editor(&rom);

  for (int load = 0; load < 2; ++load) {
    if (load != 0) {
      ScreenEditorSaveStoplossTestPeer::ResetRomBackedStateForLoad(&editor);
    }
    ASSERT_OK(ScreenEditorSaveStoplossTestPeer::CreateTitleScreenModel(
        &editor, &rom, &game_data));
    ASSERT_OK(ScreenEditorSaveStoplossTestPeer::CreateOverworldMapModel(&editor,
                                                                        &rom));
    EXPECT_EQ(ScreenEditorSaveStoplossTestPeer::TitleScreenPaletteSize(editor),
              64u);
    EXPECT_EQ(ScreenEditorSaveStoplossTestPeer::LightWorldPaletteSize(editor),
              128u);
    EXPECT_EQ(ScreenEditorSaveStoplossTestPeer::DarkWorldPaletteSize(editor),
              128u);
  }

  ScreenEditorSaveStoplossTestPeer::MarkTitleScreenModified(&editor);
  ASSERT_OK(ScreenEditorSaveStoplossTestPeer::SaveTitleScreenToRom(&editor));
  EXPECT_FALSE(editor.HasPendingTitleScreenChanges());

  ScreenEditorSaveStoplossTestPeer::MarkOverworldMapModified(&editor);
  ScreenEditorSaveStoplossTestPeer::MarkOverworldMapPaletteModified(&editor);
  ASSERT_OK(ScreenEditorSaveStoplossTestPeer::SaveOverworldMapToRom(&editor));
  EXPECT_FALSE(
      ScreenEditorSaveStoplossTestPeer::HasPendingOverworldMapTileChanges(
          editor));
  EXPECT_TRUE(
      ScreenEditorSaveStoplossTestPeer::HasPendingOverworldMapPaletteChanges(
          editor));
  EXPECT_TRUE(editor.HasPendingOverworldMapChanges());
}

}  // namespace
}  // namespace yaze::editor
