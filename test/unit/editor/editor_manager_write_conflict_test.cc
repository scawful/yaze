#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/editor_manager.h"
#include "app/editor/palette/palette_group_panel.h"
#include "app/gfx/backend/null_renderer.h"
#include "app/gfx/types/snes_color.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/util/palette_manager.h"
#include "app/startup_flags.h"
#include "core/features.h"
#include "rom/rom_diff.h"
#include "rom/snes.h"
#include "testing.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"

#include "imgui/imgui.h"

namespace yaze::editor {
namespace {

struct FeatureFlagsGuard {
  core::FeatureFlags::Flags prev = core::FeatureFlags::get();
  ~FeatureFlagsGuard() { core::FeatureFlags::get() = prev; }
};

struct ScopedFileCleanup {
  std::filesystem::path path;
  ~ScopedFileCleanup() {
    std::error_code ec;
    std::filesystem::remove(path, ec);
  }
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
  d.kSaveChests = false;
  d.kSavePotItems = false;
  d.kSavePalettes = false;

  auto& o = flags.overworld;
  o.kSaveOverworldMaps = false;
  o.kSaveOverworldEntrances = false;
  o.kSaveOverworldExits = false;
  o.kSaveOverworldItems = false;
  o.kSaveOverworldProperties = false;
}

std::filesystem::path CreateMinimalRomFile(const std::string& basename,
                                           const std::string& title,
                                           size_t rom_size = 512 * 1024) {
  const std::filesystem::path rom_path = MakeTempFilePath(basename);
  std::vector<uint8_t> rom_data(rom_size, 0x00);
  for (size_t i = 0; i < title.size() && (0x7FC0 + i) < rom_data.size(); ++i) {
    rom_data[0x7FC0 + i] = static_cast<uint8_t>(title[i]);
  }

  std::ofstream out(rom_path, std::ios::binary | std::ios::trunc);
  EXPECT_TRUE(out.is_open());
  out.write(reinterpret_cast<const char*>(rom_data.data()),
            static_cast<std::streamsize>(rom_data.size()));
  EXPECT_TRUE(out.good());
  return rom_path;
}

void MarkCurrentDungeonRoomPending(EditorManager* manager) {
  auto* editor_set = manager->GetCurrentEditorSet();
  ASSERT_NE(editor_set, nullptr);
  auto* dungeon =
      editor_set->GetEditorAs<DungeonEditorV2>(EditorType::kDungeon);
  ASSERT_NE(dungeon, nullptr);
  dungeon->rooms()[0].SetPalette(0x2A);
  EXPECT_EQ(dungeon->PendingRoomCount(), 1);
}

void SeedCurrentDungeonPalette(EditorManager* manager, uint16_t seed) {
  ASSERT_NE(manager, nullptr);
  auto* game_data = manager->GetCurrentGameData();
  ASSERT_NE(game_data, nullptr);
  auto* group = game_data->palette_groups.get_group("dungeon_main");
  ASSERT_NE(group, nullptr);
  group->clear();
  gfx::SnesPalette palette;
  palette.AddColor(gfx::SnesColor(seed));
  group->AddPalette(palette);
  gfx::PaletteManager::Get().Initialize(game_data);
}

void DrawPalettePanelOnce(DungeonMainPalettePanel* panel) {
  ASSERT_NE(panel, nullptr);
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(1280.0f, 720.0f);
  io.DeltaTime = 1.0f / 60.0f;
  ImGui::NewFrame();
  panel->Draw(nullptr);
  ImGui::EndFrame();
}

void UpdateSessionEditorsFrame(EditorManager* manager) {
  ASSERT_NE(manager, nullptr);
  ASSERT_NE(manager->session_coordinator(), nullptr);
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(1280.0f, 720.0f);
  io.DeltaTime = 1.0f / 60.0f;
  ImGui::NewFrame();
  manager->session_coordinator()->UpdateSessions();
  ImGui::EndFrame();
}

void WriteLongPointer(std::vector<uint8_t>* data, int offset, uint32_t value) {
  ASSERT_NE(data, nullptr);
  ASSERT_GE(offset, 0);
  ASSERT_LT(offset + 2, static_cast<int>(data->size()));
  (*data)[offset] = static_cast<uint8_t>(value & 0xFF);
  (*data)[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
  (*data)[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
}

void SetupSharedPotItemTable(std::vector<uint8_t>* data) {
  ASSERT_NE(data, nullptr);
  constexpr int kSharedStreamPc = 0xE000;
  const uint16_t pointer = static_cast<uint16_t>(PcToSnes(kSharedStreamPc));
  for (int room_id = 0; room_id < zelda3::kNumberOfRooms; ++room_id) {
    const int slot = zelda3::kRoomItemsPointers + room_id * 2;
    ASSERT_LT(slot + 1, static_cast<int>(data->size()));
    (*data)[slot] = pointer & 0xFF;
    (*data)[slot + 1] = (pointer >> 8) & 0xFF;
  }
  (*data)[kSharedStreamPc] = 0xFF;
  (*data)[kSharedStreamPc + 1] = 0xFF;
}

TEST(EditorManagerWriteConflictTest, SaveRomBlocksAndAllowsBypass) {
  FeatureFlagsGuard guard;

  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  // Create a minimal ROM file that satisfies RomFileManager's size checks.
  const std::filesystem::path rom_path =
      MakeTempFilePath("yaze_write_conflict_test.sfc");
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

  // OpenRomOrProject resets global flags from the project defaults. Override to
  // keep SaveRom() deterministic and to avoid unrelated save-time prompts.
  DisableRomWritesForTest();

  // Mark a project as opened so EditorManager's save-time write conflict
  // protection is active.
  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "WriteConflictTest";
  project->filepath = (rom_path.parent_path() / "project.yaze").string();
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.expected_hash.clear();  // Avoid hash-mismatch prompts.
  EXPECT_TRUE(project->project_opened());

  // Reserve a protected region that includes our modified PC address.
  constexpr uint32_t kPcOffset = 0x1234;
  const uint32_t snes_addr = PcToSnes(kPcOffset);
  const std::string manifest_json =
      absl::StrFormat(R"json(
{
  "manifest_version": 1,
  "hack_name": "unit_test",
  "protected_regions": {
    "total_hooks": 1,
    "regions": [
      { "start": "0x%06X", "end": "0x%06X", "hook_count": 1, "module": "unit_test" }
    ]
  }
}
)json",
                      snes_addr - 0x10, snes_addr + 0x10);
  ASSERT_OK(project->hack_manifest.LoadFromString(manifest_json));
  ASSERT_TRUE(project->hack_manifest.loaded());

  Rom* rom = manager->GetCurrentRom();
  ASSERT_NE(rom, nullptr);
  ASSERT_TRUE(rom->is_loaded());

  const uint8_t original = rom_data[kPcOffset];
  const uint8_t mutated = static_cast<uint8_t>(original ^ 0xFF);
  ASSERT_OK(rom->WriteByte(static_cast<int>(kPcOffset), mutated));

  // Sanity checks: the diff and the manifest analysis should both detect the
  // mutation at this PC offset.
  const auto diff = yaze::rom::ComputeDiffRanges(rom_data, rom->vector());
  ASSERT_FALSE(diff.ranges.empty());
  EXPECT_EQ(diff.ranges[0].first, kPcOffset);
  EXPECT_EQ(diff.ranges[0].second, kPcOffset + 1);

  const auto direct_conflicts =
      project->hack_manifest.AnalyzePcWriteRanges({{kPcOffset, kPcOffset + 1}});
  ASSERT_FALSE(direct_conflicts.empty());
  EXPECT_EQ(direct_conflicts[0].address, snes_addr);

  // Mirror EditorManager's preferred disk-vs-memory diff path.
  std::ifstream disk_file(rom->filename(), std::ios::binary);
  ASSERT_TRUE(disk_file.is_open());
  disk_file.seekg(0, std::ios::end);
  const std::streampos disk_end = disk_file.tellg();
  ASSERT_GE(disk_end, 0);
  std::vector<uint8_t> disk_data(static_cast<size_t>(disk_end));
  disk_file.seekg(0, std::ios::beg);
  disk_file.read(reinterpret_cast<char*>(disk_data.data()),
                 static_cast<std::streamsize>(disk_data.size()));
  ASSERT_TRUE(disk_file.good());

  // Sanity check: on-disk byte is still the original value before SaveRom().
  EXPECT_EQ(disk_data[kPcOffset], original);

  const auto disk_diff = yaze::rom::ComputeDiffRanges(disk_data, rom->vector());
  ASSERT_FALSE(disk_diff.ranges.empty());
  EXPECT_EQ(disk_diff.ranges[0].first, kPcOffset);
  EXPECT_EQ(disk_diff.ranges[0].second, kPcOffset + 1);

  const auto disk_conflicts =
      project->hack_manifest.AnalyzePcWriteRanges(disk_diff.ranges);
  ASSERT_FALSE(disk_conflicts.empty());
  EXPECT_EQ(disk_conflicts[0].address, snes_addr);

  // First save attempt should be blocked by write conflict protection.
  auto status = manager->SaveRom();
  EXPECT_EQ(status.code(), absl::StatusCode::kCancelled) << status;
  ASSERT_FALSE(manager->pending_write_conflicts().empty());
  EXPECT_EQ(manager->pending_write_conflicts()[0].address, snes_addr);

  // Ensure no on-disk write occurred.
  EXPECT_EQ(ReadByteAt(rom_path, static_cast<std::streamoff>(kPcOffset)),
            original);

  // Bypass once and retry: should save successfully.
  manager->BypassWriteConflictOnce();
  auto status2 = manager->SaveRom();
  EXPECT_TRUE(status2.ok()) << status2.message();
  EXPECT_TRUE(manager->pending_write_conflicts().empty());
  EXPECT_EQ(ReadByteAt(rom_path, static_cast<std::streamoff>(kPcOffset)),
            mutated);
}

TEST(EditorManagerWriteConflictTest,
     LateConflictRollsBackSerializedRomAndDungeonDirtyState) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  constexpr int kHeaderTablePc = 0x10000;
  constexpr int kHeaderPc = 0x12000;
  constexpr uint32_t kProtectedPc = 0x1234;
  const std::filesystem::path rom_path =
      MakeTempFilePath("yaze_transaction_rollback_test.sfc");
  ScopedFileCleanup cleanup{rom_path};
  std::vector<uint8_t> rom_data(512 * 1024, 0x00);
  WriteLongPointer(&rom_data, zelda3::kRoomHeaderPointer,
                   PcToSnes(kHeaderTablePc));
  const uint32_t header_snes = PcToSnes(kHeaderPc);
  rom_data[zelda3::kRoomHeaderPointerBank] =
      static_cast<uint8_t>((header_snes >> 16) & 0xFF);
  rom_data[kHeaderTablePc] = static_cast<uint8_t>(header_snes & 0xFF);
  rom_data[kHeaderTablePc + 1] =
      static_cast<uint8_t>((header_snes >> 8) & 0xFF);
  {
    std::ofstream out(rom_path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(out.is_open());
    out.write(reinterpret_cast<const char*>(rom_data.data()),
              static_cast<std::streamsize>(rom_data.size()));
    ASSERT_TRUE(out.good());
  }

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();
  core::FeatureFlags::get().dungeon.kSaveRoomHeaders = true;

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "TransactionRollbackTest";
  project->filepath = (rom_path.parent_path() / "project.yaze").string();
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.expected_hash.clear();
  const uint32_t protected_snes = PcToSnes(kProtectedPc);
  ASSERT_OK(project->hack_manifest.LoadFromString(absl::StrFormat(
      R"json({
        "manifest_version": 1,
        "hack_name": "transaction_test",
        "protected_regions": {
          "regions": [
            {"start": "0x%06X", "end": "0x%06X", "module": "test"}
          ]
        }
      })json",
      protected_snes, protected_snes + 1)));

  auto* editor_set = manager->GetCurrentEditorSet();
  ASSERT_NE(editor_set, nullptr);
  auto* dungeon =
      editor_set->GetEditorAs<DungeonEditorV2>(EditorType::kDungeon);
  ASSERT_NE(dungeon, nullptr);
  auto& room = dungeon->rooms()[0];
  room.SetLoaded(true);
  room.SetPalette(0x2A);
  ASSERT_TRUE(room.header_dirty());

  Rom* rom = manager->GetCurrentRom();
  ASSERT_NE(rom, nullptr);
  ASSERT_OK(rom->WriteByte(kProtectedPc, 0xA5));
  const auto before_save = rom->vector();

  const auto blocked = manager->SaveRom();
  EXPECT_EQ(blocked.code(), absl::StatusCode::kCancelled) << blocked;
  EXPECT_EQ(rom->vector(), before_save);
  EXPECT_TRUE(room.header_dirty());
  EXPECT_EQ(rom->data()[kHeaderPc + 1], 0x00);
  EXPECT_EQ(ReadByteAt(rom_path, kProtectedPc), 0x00);
  EXPECT_EQ(ReadByteAt(rom_path, kHeaderPc + 1), 0x00);

  manager->BypassWriteConflictOnce();
  ASSERT_OK(manager->SaveRom());
  EXPECT_FALSE(room.header_dirty());
  EXPECT_EQ(ReadByteAt(rom_path, kProtectedPc), 0xA5);
  EXPECT_EQ(ReadByteAt(rom_path, kHeaderPc + 1), 0x2A);
}

TEST(EditorManagerWriteConflictTest,
     UnreadableDiskUsesPreSerializationDungeonWriteRangeSnapshot) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  constexpr int kHeaderTablePc = 0x10000;
  constexpr int kHeaderPc = 0x12000;
  const std::filesystem::path rom_path =
      MakeTempFilePath("yaze_unreadable_disk_conflict_test.sfc");
  ScopedFileCleanup cleanup{rom_path};
  std::vector<uint8_t> rom_data(512 * 1024, 0x00);
  WriteLongPointer(&rom_data, zelda3::kRoomHeaderPointer,
                   PcToSnes(kHeaderTablePc));
  const uint32_t header_snes = PcToSnes(kHeaderPc);
  rom_data[zelda3::kRoomHeaderPointerBank] =
      static_cast<uint8_t>((header_snes >> 16) & 0xFF);
  rom_data[kHeaderTablePc] = static_cast<uint8_t>(header_snes & 0xFF);
  rom_data[kHeaderTablePc + 1] =
      static_cast<uint8_t>((header_snes >> 8) & 0xFF);
  {
    std::ofstream out(rom_path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(out.is_open());
    out.write(reinterpret_cast<const char*>(rom_data.data()),
              static_cast<std::streamsize>(rom_data.size()));
    ASSERT_TRUE(out.good());
  }

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();
  core::FeatureFlags::get().dungeon.kSaveRoomHeaders = true;

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "UnreadableDiskConflictTest";
  project->filepath = (rom_path.parent_path() / "project.yaze").string();
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.expected_hash.clear();
  ASSERT_OK(project->hack_manifest.LoadFromString(absl::StrFormat(
      R"json({
        "manifest_version": 1,
        "hack_name": "unreadable_disk_test",
        "protected_regions": {
          "regions": [
            {"start": "0x%06X", "end": "0x%06X", "module": "test"}
          ]
        }
      })json",
      header_snes, header_snes + 14)));

  auto* editor_set = manager->GetCurrentEditorSet();
  ASSERT_NE(editor_set, nullptr);
  auto* dungeon =
      editor_set->GetEditorAs<DungeonEditorV2>(EditorType::kDungeon);
  ASSERT_NE(dungeon, nullptr);
  auto& room = dungeon->rooms()[0];
  room.SetLoaded(true);
  room.SetPalette(0x2A);
  ASSERT_TRUE(room.header_dirty());
  ASSERT_FALSE(dungeon->CollectWriteRanges().empty());
  const auto before_save = manager->GetCurrentRom()->vector();

  std::error_code remove_error;
  ASSERT_TRUE(std::filesystem::remove(rom_path, remove_error));
  ASSERT_FALSE(remove_error) << remove_error.message();
  ASSERT_FALSE(std::filesystem::exists(rom_path));

  const auto blocked = manager->SaveRom();

  EXPECT_EQ(blocked.code(), absl::StatusCode::kCancelled) << blocked;
  ASSERT_FALSE(manager->pending_write_conflicts().empty());
  EXPECT_EQ(manager->pending_write_conflicts()[0].address, header_snes);
  EXPECT_EQ(manager->GetCurrentRom()->vector(), before_save);
  EXPECT_TRUE(room.header_dirty());
  EXPECT_FALSE(std::filesystem::exists(rom_path));
}

TEST(EditorManagerWriteConflictTest,
     PotRepackLateConflictRestoresRomAndDirtyState) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const std::filesystem::path rom_path =
      MakeTempFilePath("yaze_pot_repack_rollback_test.sfc");
  ScopedFileCleanup cleanup{rom_path};
  std::vector<uint8_t> rom_data(512 * 1024, 0x00);
  SetupSharedPotItemTable(&rom_data);
  {
    std::ofstream out(rom_path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(out.is_open());
    out.write(reinterpret_cast<const char*>(rom_data.data()),
              static_cast<std::streamsize>(rom_data.size()));
    ASSERT_TRUE(out.good());
  }

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();
  core::FeatureFlags::get().dungeon.kSavePotItems = true;

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "PotRepackRollbackTest";
  project->filepath = (rom_path.parent_path() / "project.yaze").string();
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.expected_hash.clear();
  project->rom_metadata.write_policy = project::RomWritePolicy::kAllow;
  ASSERT_OK(project->hack_manifest.LoadFromString(R"json(
{
  "manifest_version": 3,
  "hack_name": "pot_repack_rollback_test",
  "protected_regions": {
    "regions": [
      {"start": "0x01E100", "end": "0x01E140", "module": "test"}
    ]
  },
  "dungeon_stream_regions": {
    "pot_items": {
      "pointer_table": "0x01DB69",
      "pointer_count": 296,
      "pointer_encoding": "bank16",
      "pointer_bank": "0x01",
      "strategy": "repack_all",
      "data_regions": [
        {"start": "0x01E000", "end": "0x01E140"}
      ],
      "allocation_regions": [
        {"start": "0x01E100", "end": "0x01E140"}
      ]
    }
  }
}
)json"));

  auto* editor_set = manager->GetCurrentEditorSet();
  ASSERT_NE(editor_set, nullptr);
  auto* dungeon =
      editor_set->GetEditorAs<DungeonEditorV2>(EditorType::kDungeon);
  ASSERT_NE(dungeon, nullptr);
  auto& room = dungeon->rooms()[0];
  room.GetPotItems().push_back(zelda3::PotItem{0x1234, 0x56});
  room.MarkPotItemsDirty();
  ASSERT_TRUE(room.pot_items_dirty());

  const auto predicted_ranges = dungeon->CollectWriteRanges();
  EXPECT_NE(std::find(predicted_ranges.begin(), predicted_ranges.end(),
                      std::pair<uint32_t, uint32_t>{0x00E100, 0x00E140}),
            predicted_ranges.end());
  EXPECT_NE(
      std::find(predicted_ranges.begin(), predicted_ranges.end(),
                std::pair<uint32_t, uint32_t>{
                    zelda3::kRoomItemsPointers,
                    zelda3::kRoomItemsPointers + zelda3::kNumberOfRooms * 2}),
      predicted_ranges.end());

  Rom* rom = manager->GetCurrentRom();
  ASSERT_NE(rom, nullptr);
  const auto before_save = rom->vector();

  const auto paused = manager->SaveRom();
  EXPECT_EQ(paused.code(), absl::StatusCode::kCancelled) << paused;
  ASSERT_TRUE(manager->HasPendingPotItemSaveConfirmation());

  std::error_code remove_error;
  ASSERT_TRUE(std::filesystem::remove(rom_path, remove_error));
  ASSERT_FALSE(remove_error) << remove_error.message();
  ASSERT_FALSE(std::filesystem::exists(rom_path));

  manager->ResolvePotItemSaveConfirmation(
      EditorManager::PotItemSaveDecision::kSaveWithPotItems);

  ASSERT_FALSE(manager->pending_write_conflicts().empty());
  EXPECT_EQ(manager->pending_write_conflicts()[0].address, 0x01E100u);
  EXPECT_EQ(rom->vector(), before_save);
  EXPECT_TRUE(room.pot_items_dirty());
  EXPECT_FALSE(std::filesystem::exists(rom_path));
}

TEST(EditorManagerWriteConflictTest,
     FrameSessionIterationPreservesSaveConfirmationAndActiveEditor) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto rom_a =
      CreateMinimalRomFile("yaze_frame_context_a.sfc", "FRAME CONTEXT A");
  const auto rom_b =
      CreateMinimalRomFile("yaze_frame_context_b.sfc", "FRAME CONTEXT B");
  ScopedFileCleanup cleanup_a{rom_a};
  ScopedFileCleanup cleanup_b{rom_b};

  ASSERT_OK(manager->OpenRomOrProject(rom_a.string()));
  ASSERT_OK(manager->OpenRomOrProject(rom_b.string()));

  auto* inactive_dungeon =
      manager->GetCurrentEditorSet()->GetEditorAs<DungeonEditorV2>(
          EditorType::kDungeon);
  ASSERT_NE(inactive_dungeon, nullptr);
  *inactive_dungeon->active() = true;

  manager->SwitchToSession(0);
  ASSERT_EQ(manager->GetCurrentSessionIndex(), 0u);
  auto* active_rom = manager->GetCurrentRom();
  auto* active_dungeon =
      manager->GetCurrentEditorSet()->GetEditorAs<DungeonEditorV2>(
          EditorType::kDungeon);
  ASSERT_NE(active_rom, nullptr);
  ASSERT_NE(active_dungeon, nullptr);
  manager->SetCurrentEditor(active_dungeon);

  DisableRomWritesForTest();
  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "FrameContextTest";
  project->filepath =
      (rom_a.parent_path() / "frame_context_project.yaze").string();
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.write_policy = project::RomWritePolicy::kWarn;
  project->rom_metadata.expected_hash = "deadbeef";
  ASSERT_TRUE(manager->IsRomHashMismatch());
  ASSERT_OK(active_rom->WriteByte(0x1234, 0x7F));

  const auto paused = manager->SaveRom();
  ASSERT_EQ(paused.code(), absl::StatusCode::kCancelled) << paused;
  ASSERT_TRUE(manager->IsRomWriteConfirmPending());

  UpdateSessionEditorsFrame(manager.get());

  EXPECT_EQ(manager->GetCurrentSessionIndex(), 0u);
  EXPECT_EQ(manager->GetCurrentRom(), active_rom);
  EXPECT_TRUE(manager->IsRomWriteConfirmPending());
  EXPECT_EQ(manager->GetCurrentEditor(), active_dungeon);
  EXPECT_EQ(ContentRegistry::Context::current_editor(), active_dungeon);
}

TEST(EditorManagerWriteConflictTest,
     OpenRomOrProjectDefersWhileSessionHasPendingDungeonChanges) {
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto rom_a = CreateMinimalRomFile("yaze_guard_a.sfc", "GUARD ROM A");
  const auto rom_b = CreateMinimalRomFile("yaze_guard_b.sfc", "GUARD ROM B");
  ScopedFileCleanup cleanup_a{rom_a};
  ScopedFileCleanup cleanup_b{rom_b};

  ASSERT_OK(manager->OpenRomOrProject(rom_a.string()));
  MarkCurrentDungeonRoomPending(manager.get());

  ASSERT_OK(manager->OpenRomOrProject(rom_b.string()));
  EXPECT_TRUE(manager->HasPendingUnsavedSessionAction());
  ASSERT_NE(manager->popup_manager(), nullptr);
  EXPECT_TRUE(
      manager->popup_manager()->IsVisible(PopupID::kUnsavedSessionChanges));
  EXPECT_EQ(manager->GetActiveSessionCount(), 1u);

  manager->ConfirmPendingUnsavedSessionActionDiscardAndContinue();

  EXPECT_FALSE(manager->HasPendingUnsavedSessionAction());
  EXPECT_EQ(manager->GetActiveSessionCount(), 2u);
  ASSERT_NE(manager->GetCurrentRom(), nullptr);
  EXPECT_EQ(manager->GetCurrentRom()->filename(), rom_b.string());
}

TEST(EditorManagerWriteConflictTest,
     SwitchAndCloseSessionDeferWhileCurrentSessionHasPendingDungeonChanges) {
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto rom_a =
      CreateMinimalRomFile("yaze_guard_switch_a.sfc", "GUARD SWITCH A");
  const auto rom_b =
      CreateMinimalRomFile("yaze_guard_switch_b.sfc", "GUARD SWITCH B");
  ScopedFileCleanup cleanup_a{rom_a};
  ScopedFileCleanup cleanup_b{rom_b};

  ASSERT_OK(manager->OpenRomOrProject(rom_a.string()));
  ASSERT_OK(manager->OpenRomOrProject(rom_b.string()));
  ASSERT_EQ(manager->GetCurrentSessionIndex(), 1u);

  MarkCurrentDungeonRoomPending(manager.get());

  manager->SwitchToSession(0);
  EXPECT_TRUE(manager->HasPendingUnsavedSessionAction());
  EXPECT_EQ(manager->GetCurrentSessionIndex(), 1u);

  manager->ConfirmPendingUnsavedSessionActionDiscardAndContinue();
  EXPECT_EQ(manager->GetCurrentSessionIndex(), 0u);

  manager->SwitchToSession(1);
  EXPECT_EQ(manager->GetCurrentSessionIndex(), 1u);
  MarkCurrentDungeonRoomPending(manager.get());

  manager->CloseCurrentSession();
  EXPECT_TRUE(manager->HasPendingUnsavedSessionAction());
  EXPECT_EQ(manager->GetActiveSessionCount(), 2u);

  manager->ConfirmPendingUnsavedSessionActionDiscardAndContinue();
  EXPECT_EQ(manager->GetActiveSessionCount(), 1u);
}

TEST(EditorManagerWriteConflictTest,
     QuitDefersWhileAnySessionHasPendingDungeonChanges) {
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto rom_a =
      CreateMinimalRomFile("yaze_guard_quit_a.sfc", "GUARD QUIT A");
  ScopedFileCleanup cleanup_a{rom_a};

  ASSERT_OK(manager->OpenRomOrProject(rom_a.string()));
  MarkCurrentDungeonRoomPending(manager.get());

  manager->Quit();
  EXPECT_FALSE(manager->quit());
  EXPECT_TRUE(manager->HasPendingUnsavedSessionAction());

  manager->ConfirmPendingUnsavedSessionActionDiscardAndContinue();
  EXPECT_TRUE(manager->quit());
}

TEST(EditorManagerWriteConflictTest,
     PaletteOnlyChangesPromptAndSurviveSwitchAndCloseLifecycle) {
  ScopedImGuiContext imgui;
  gfx::PaletteManager::Get().ResetForTesting();

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto rom_a =
      CreateMinimalRomFile("yaze_palette_guard_a.sfc", "PALETTE GUARD A");
  const auto rom_b =
      CreateMinimalRomFile("yaze_palette_guard_b.sfc", "PALETTE GUARD B");
  ScopedFileCleanup cleanup_a{rom_a};
  ScopedFileCleanup cleanup_b{rom_b};

  ASSERT_OK(manager->OpenRomOrProject(rom_a.string()));
  auto* first_game_data = manager->GetCurrentGameData();
  SeedCurrentDungeonPalette(manager.get(), 0x0100);
  ASSERT_TRUE(gfx::PaletteManager::Get()
                  .SetColor("dungeon_main", 0, 0, gfx::SnesColor(0x1111))
                  .ok());

  ASSERT_OK(manager->OpenRomOrProject(rom_b.string()));
  EXPECT_TRUE(manager->HasPendingUnsavedSessionAction());
  EXPECT_NE(manager->GetPendingUnsavedSessionActionPrompt().find(
                "1 unapplied palette color"),
            std::string::npos);
  EXPECT_EQ(manager->GetActiveSessionCount(), 1u);

  manager->ConfirmPendingUnsavedSessionActionDiscardAndContinue();
  ASSERT_EQ(manager->GetActiveSessionCount(), 2u);
  ASSERT_EQ(manager->GetCurrentSessionIndex(), 1u);
  auto* second_game_data = manager->GetCurrentGameData();
  SeedCurrentDungeonPalette(manager.get(), 0x0200);
  ASSERT_TRUE(gfx::PaletteManager::Get()
                  .SetColor("dungeon_main", 0, 0, gfx::SnesColor(0x2222))
                  .ok());

  manager->SwitchToSession(0);
  EXPECT_TRUE(manager->HasPendingUnsavedSessionAction());
  EXPECT_EQ(manager->GetCurrentSessionIndex(), 1u);
  manager->ConfirmPendingUnsavedSessionActionDiscardAndContinue();

  ASSERT_EQ(manager->GetCurrentSessionIndex(), 0u);
  EXPECT_TRUE(gfx::PaletteManager::Get().IsManaging(first_game_data));
  EXPECT_EQ(gfx::PaletteManager::Get().GetColor("dungeon_main", 0, 0).snes(),
            0x1111);
  EXPECT_TRUE(gfx::PaletteManager::Get().HasUnsavedChanges(first_game_data));
  EXPECT_TRUE(gfx::PaletteManager::Get().HasUnsavedChanges(second_game_data));

  manager->SwitchToSession(1);
  EXPECT_TRUE(manager->HasPendingUnsavedSessionAction());
  manager->ConfirmPendingUnsavedSessionActionDiscardAndContinue();
  ASSERT_EQ(manager->GetCurrentSessionIndex(), 1u);
  EXPECT_TRUE(gfx::PaletteManager::Get().IsManaging(second_game_data));
  EXPECT_EQ(gfx::PaletteManager::Get().GetColor("dungeon_main", 0, 0).snes(),
            0x2222);

  manager->CloseCurrentSession();
  EXPECT_TRUE(manager->HasPendingUnsavedSessionAction());
  manager->ConfirmPendingUnsavedSessionActionDiscardAndContinue();

  ASSERT_EQ(manager->GetActiveSessionCount(), 1u);
  ASSERT_EQ(manager->GetCurrentSessionIndex(), 0u);
  EXPECT_TRUE(gfx::PaletteManager::Get().IsManaging(first_game_data));
  EXPECT_EQ(gfx::PaletteManager::Get().GetColor("dungeon_main", 0, 0).snes(),
            0x1111);
  EXPECT_TRUE(gfx::PaletteManager::Get().HasUnsavedChanges(first_game_data));

  manager.reset();
  gfx::PaletteManager::Get().ResetForTesting();
}

TEST(EditorManagerWriteConflictTest,
     ActiveCloseFullyRebindsSurvivingSessionContext) {
  ScopedImGuiContext imgui;
  gfx::PaletteManager::Get().ResetForTesting();

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto rom_a =
      CreateMinimalRomFile("yaze_close_rebind_a.sfc", "CLOSE REBIND A");
  const auto rom_b =
      CreateMinimalRomFile("yaze_close_rebind_b.sfc", "CLOSE REBIND B");
  ScopedFileCleanup cleanup_a{rom_a};
  ScopedFileCleanup cleanup_b{rom_b};

  ASSERT_OK(manager->OpenRomOrProject(rom_a.string()));
  SeedCurrentDungeonPalette(manager.get(), 0x0100);
  Rom* first_rom = manager->GetCurrentRom();
  auto* first_game_data = manager->GetCurrentGameData();
  Editor* first_dungeon =
      manager->GetCurrentEditorSet()->GetEditor(EditorType::kDungeon);
  ASSERT_NE(first_rom, nullptr);
  ASSERT_NE(first_game_data, nullptr);
  ASSERT_NE(first_dungeon, nullptr);

  ASSERT_OK(manager->OpenRomOrProject(rom_b.string()));
  SeedCurrentDungeonPalette(manager.get(), 0x0200);
  Rom* second_rom = manager->GetCurrentRom();
  auto* second_game_data = manager->GetCurrentGameData();
  Editor* second_dungeon =
      manager->GetCurrentEditorSet()->GetEditor(EditorType::kDungeon);
  auto* second_settings = manager->GetCurrentEditorSet()->GetSettingsPanel();
  ASSERT_NE(second_rom, nullptr);
  ASSERT_NE(second_game_data, nullptr);
  ASSERT_NE(second_dungeon, nullptr);
  ASSERT_NE(second_settings, nullptr);

  manager->SwitchToSession(0);
  ASSERT_EQ(manager->GetCurrentRom(), first_rom);
  manager->window_manager().SetActiveCategory("Dungeon", /*notify=*/false);
  manager->SetCurrentEditor(first_dungeon);
  ASSERT_NE(manager->right_drawer_manager(), nullptr);
  auto* properties_panel = manager->right_drawer_manager()->properties_panel();
  ASSERT_NE(properties_panel, nullptr);
  int closing_selection = 0;
  SelectionContext selection;
  selection.type = SelectionType::kDungeonObject;
  selection.data = &closing_selection;
  properties_panel->SetSelection(selection);
  ASSERT_TRUE(properties_panel->HasSelection());

  manager->CloseCurrentSession();

  ASSERT_EQ(manager->GetActiveSessionCount(), 1u);
  EXPECT_EQ(manager->GetCurrentSessionIndex(), 0u);
  EXPECT_EQ(manager->GetCurrentSessionId(), 1u);
  EXPECT_EQ(manager->window_manager().GetActiveSessionId(), 1u);
  EXPECT_EQ(manager->GetCurrentRom(), second_rom);
  EXPECT_EQ(manager->GetCurrentGameData(), second_game_data);
  EXPECT_EQ(ContentRegistry::Context::rom(), second_rom);
  EXPECT_EQ(ContentRegistry::Context::game_data(), second_game_data);
  EXPECT_EQ(manager->GetCurrentEditor(), second_dungeon);
  EXPECT_EQ(ContentRegistry::Context::current_editor(), second_dungeon);
  EXPECT_EQ(ContentRegistry::Context::editor_window_context("Dungeon"),
            second_dungeon);
  EXPECT_EQ(manager->right_drawer_manager()->settings_panel(), second_settings);
  EXPECT_FALSE(properties_panel->HasSelection());
  EXPECT_TRUE(gfx::PaletteManager::Get().IsManaging(second_game_data));

  manager.reset();
  gfx::PaletteManager::Get().ResetForTesting();
}

TEST(EditorManagerWriteConflictTest,
     SavingAndClosingInactiveSessionPreservesActiveStableSession) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = false;

  const auto rom_a =
      CreateMinimalRomFile("yaze_inactive_close_a.sfc", "INACTIVE CLOSE A");
  const auto rom_b =
      CreateMinimalRomFile("yaze_inactive_close_b.sfc", "INACTIVE CLOSE B");
  const auto rom_c =
      CreateMinimalRomFile("yaze_inactive_close_c.sfc", "INACTIVE CLOSE C");
  ScopedFileCleanup cleanup_a{rom_a};
  ScopedFileCleanup cleanup_b{rom_b};
  ScopedFileCleanup cleanup_c{rom_c};

  ASSERT_OK(manager->OpenRomOrProject(rom_a.string()));
  ASSERT_OK(manager->OpenRomOrProject(rom_b.string()));
  ASSERT_OK(manager->OpenRomOrProject(rom_c.string()));
  ASSERT_EQ(manager->GetCurrentSessionId(), 2u);

  manager->SwitchToSession(1);
  ASSERT_EQ(manager->GetCurrentSessionId(), 1u);
  ASSERT_OK(manager->GetCurrentRom()->WriteByte(0x2345, 0x6A));
  manager->SwitchToSession(2);
  ASSERT_TRUE(manager->HasPendingUnsavedSessionAction());
  manager->ConfirmPendingUnsavedSessionActionDiscardAndContinue();
  ASSERT_EQ(manager->GetCurrentSessionId(), 2u);
  Rom* active_rom = manager->GetCurrentRom();
  Editor* active_dungeon =
      manager->GetCurrentEditorSet()->GetEditor(EditorType::kDungeon);
  ASSERT_NE(active_rom, nullptr);
  ASSERT_NE(active_dungeon, nullptr);
  manager->window_manager().SetActiveCategory("Dungeon", /*notify=*/false);
  manager->SetCurrentEditor(active_dungeon);

  DisableRomWritesForTest();
  manager->RemoveSession(1);
  ASSERT_TRUE(manager->HasPendingUnsavedSessionAction());
  manager->ConfirmPendingUnsavedSessionActionSaveAndContinue();

  EXPECT_FALSE(manager->HasPendingUnsavedSessionAction());
  EXPECT_EQ(manager->GetActiveSessionCount(), 2u);
  EXPECT_EQ(manager->GetCurrentSessionIndex(), 1u);
  EXPECT_EQ(manager->GetCurrentSessionId(), 2u);
  EXPECT_EQ(manager->window_manager().GetActiveSessionId(), 2u);
  EXPECT_EQ(manager->GetCurrentRom(), active_rom);
  EXPECT_EQ(manager->GetCurrentRom()->filename(), rom_c.string());
  EXPECT_EQ(manager->GetCurrentEditor(), active_dungeon);
  EXPECT_EQ(ContentRegistry::Context::current_editor(), active_dungeon);
  EXPECT_EQ(ReadByteAt(rom_b, 0x2345), 0x6A);
}

TEST(EditorManagerWriteConflictTest,
     InactiveSaveAndClosePreservesResumableWriteConfirmation) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = false;

  const auto rom_a =
      CreateMinimalRomFile("yaze_inactive_confirm_a.sfc", "INACTIVE CONFIRM A");
  const auto rom_b =
      CreateMinimalRomFile("yaze_inactive_confirm_b.sfc", "INACTIVE CONFIRM B");
  ScopedFileCleanup cleanup_a{rom_a};
  ScopedFileCleanup cleanup_b{rom_b};

  ASSERT_OK(manager->OpenRomOrProject(rom_a.string()));
  ASSERT_OK(manager->OpenRomOrProject(rom_b.string()));

  manager->SwitchToSession(0);
  ASSERT_OK(manager->GetCurrentRom()->WriteByte(0x2345, 0x6A));
  manager->SwitchToSession(1);
  ASSERT_TRUE(manager->HasPendingUnsavedSessionAction());
  manager->ConfirmPendingUnsavedSessionActionDiscardAndContinue();
  ASSERT_EQ(manager->GetCurrentSessionId(), 1u);

  DisableRomWritesForTest();
  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "InactiveConfirmationTest";
  project->filepath =
      (rom_a.parent_path() / "inactive_confirmation_project.yaze").string();
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.write_policy = project::RomWritePolicy::kWarn;
  project->rom_metadata.expected_hash = "deadbeef";

  manager->RemoveSession(0);
  ASSERT_TRUE(manager->HasPendingUnsavedSessionAction());
  manager->ConfirmPendingUnsavedSessionActionSaveAndContinue();

  EXPECT_FALSE(manager->HasPendingUnsavedSessionAction());
  EXPECT_EQ(manager->GetActiveSessionCount(), 2u);
  EXPECT_EQ(manager->GetCurrentSessionId(), 0u);
  EXPECT_TRUE(manager->IsRomWriteConfirmPending());
  ASSERT_NE(manager->popup_manager(), nullptr);
  EXPECT_TRUE(manager->popup_manager()->IsVisible(PopupID::kRomWriteConfirm));

  manager->ConfirmRomWrite();
  ASSERT_OK(manager->ResumePendingRomSave());
  EXPECT_FALSE(manager->IsRomWriteConfirmPending());
  EXPECT_EQ(manager->GetCurrentSessionId(), 0u);
  EXPECT_EQ(ReadByteAt(rom_a, 0x2345), 0x6A);
  EXPECT_EQ(ReadByteAt(rom_b, 0x2345), 0x00);
}

TEST(EditorManagerWriteConflictTest,
     PendingInactiveCloseTracksStableTargetAcrossIndexCompaction) {
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto rom_a =
      CreateMinimalRomFile("yaze_stable_close_a.sfc", "STABLE CLOSE A");
  const auto rom_b =
      CreateMinimalRomFile("yaze_stable_close_b.sfc", "STABLE CLOSE B");
  const auto rom_c =
      CreateMinimalRomFile("yaze_stable_close_c.sfc", "STABLE CLOSE C");
  ScopedFileCleanup cleanup_a{rom_a};
  ScopedFileCleanup cleanup_b{rom_b};
  ScopedFileCleanup cleanup_c{rom_c};

  ASSERT_OK(manager->OpenRomOrProject(rom_a.string()));
  ASSERT_OK(manager->OpenRomOrProject(rom_b.string()));
  ASSERT_OK(manager->OpenRomOrProject(rom_c.string()));
  Rom* surviving_rom = manager->GetCurrentRom();
  ASSERT_NE(surviving_rom, nullptr);

  manager->SwitchToSession(1);
  ASSERT_OK(manager->GetCurrentRom()->WriteByte(0x2345, 0x6A));
  manager->SwitchToSession(0);
  ASSERT_TRUE(manager->HasPendingUnsavedSessionAction());
  manager->ConfirmPendingUnsavedSessionActionDiscardAndContinue();
  ASSERT_EQ(manager->GetCurrentSessionId(), 0u);

  manager->RemoveSession(1);
  ASSERT_TRUE(manager->HasPendingUnsavedSessionAction());

  // The pending target is stable session 1. Closing stable session 0 compacts
  // that target from UI index 1 to UI index 0 while the popup remains open.
  manager->CloseCurrentSession();
  ASSERT_TRUE(manager->HasPendingUnsavedSessionAction());
  ASSERT_EQ(manager->GetActiveSessionCount(), 2u);
  ASSERT_EQ(manager->GetCurrentSessionId(), 1u);

  manager->ConfirmPendingUnsavedSessionActionDiscardAndContinue();

  EXPECT_FALSE(manager->HasPendingUnsavedSessionAction());
  EXPECT_EQ(manager->GetActiveSessionCount(), 1u);
  EXPECT_EQ(manager->GetCurrentSessionId(), 2u);
  EXPECT_EQ(manager->GetCurrentRom(), surviving_rom);
  EXPECT_EQ(manager->GetCurrentRom()->filename(), rom_c.string());
  EXPECT_EQ(ReadByteAt(rom_b, 0x2345), 0x00);
}

TEST(EditorManagerWriteConflictTest,
     SaveAndContinueRefusesToDiscardDisabledPaletteWrites) {
  ScopedImGuiContext imgui;
  FeatureFlagsGuard flags_guard;
  gfx::PaletteManager::Get().ResetForTesting();

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = false;

  const auto rom_a =
      CreateMinimalRomFile("yaze_palette_disabled_a.sfc", "PALETTE DISABLED A");
  const auto rom_b =
      CreateMinimalRomFile("yaze_palette_disabled_b.sfc", "PALETTE DISABLED B");
  ScopedFileCleanup cleanup_a{rom_a};
  ScopedFileCleanup cleanup_b{rom_b};

  ASSERT_OK(manager->OpenRomOrProject(rom_a.string()));
  ASSERT_OK(manager->OpenRomOrProject(rom_b.string()));
  manager->SwitchToSession(0);
  ASSERT_EQ(manager->GetCurrentSessionIndex(), 0u);
  DisableRomWritesForTest();

  auto* first_game_data = manager->GetCurrentGameData();
  SeedCurrentDungeonPalette(manager.get(), 0x0100);
  ASSERT_TRUE(gfx::PaletteManager::Get()
                  .SetColor("dungeon_main", 0, 0, gfx::SnesColor(0x1111))
                  .ok());
  ASSERT_OK(manager->SaveRom());
  ASSERT_TRUE(gfx::PaletteManager::Get().HasUnsavedChanges(first_game_data));

  manager->SwitchToSession(1);
  ASSERT_TRUE(manager->HasPendingUnsavedSessionAction());
  manager->ConfirmPendingUnsavedSessionActionSaveAndContinue();
  EXPECT_FALSE(manager->HasPendingUnsavedSessionAction());
  EXPECT_EQ(manager->GetCurrentSessionIndex(), 0u);
  EXPECT_EQ(manager->GetActiveSessionCount(), 2u);
  EXPECT_TRUE(gfx::PaletteManager::Get().HasUnsavedChanges(first_game_data));

  manager->CloseCurrentSession();
  ASSERT_TRUE(manager->HasPendingUnsavedSessionAction());
  manager->ConfirmPendingUnsavedSessionActionSaveAndContinue();
  EXPECT_EQ(manager->GetCurrentSessionIndex(), 0u);
  EXPECT_EQ(manager->GetActiveSessionCount(), 2u);
  EXPECT_TRUE(gfx::PaletteManager::Get().HasUnsavedChanges(first_game_data));

  manager->Quit();
  ASSERT_TRUE(manager->HasPendingUnsavedSessionAction());
  manager->ConfirmPendingUnsavedSessionActionSaveAndContinue();
  EXPECT_FALSE(manager->quit());
  EXPECT_EQ(manager->GetActiveSessionCount(), 2u);
  EXPECT_TRUE(gfx::PaletteManager::Get().HasUnsavedChanges(first_game_data));

  manager.reset();
  gfx::PaletteManager::Get().ResetForTesting();
}

TEST(EditorManagerWriteConflictTest,
     PalettePanelsRemainSessionOwnedAcrossSwitchAndCloseDraw) {
  ScopedImGuiContext imgui;
  gfx::PaletteManager::Get().ResetForTesting();

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto rom_a = CreateMinimalRomFile("yaze_palette_panels_a.sfc",
                                          "PALETTE PANELS A", 1024 * 1024);
  const auto rom_b = CreateMinimalRomFile("yaze_palette_panels_b.sfc",
                                          "PALETTE PANELS B", 1024 * 1024);
  ScopedFileCleanup cleanup_a{rom_a};
  ScopedFileCleanup cleanup_b{rom_b};

  ASSERT_OK(manager->OpenRomOrProject(rom_a.string()));
  ASSERT_OK(manager->EnsureEditorAssetsLoaded(EditorType::kPalette));
  auto* first_panel = dynamic_cast<DungeonMainPalettePanel*>(
      manager->window_manager().GetWindowContent(0, "palette.dungeon_main"));
  ASSERT_NE(first_panel, nullptr);

  ASSERT_OK(manager->OpenRomOrProject(rom_b.string()));
  ASSERT_OK(manager->EnsureEditorAssetsLoaded(EditorType::kPalette));
  auto* second_panel = dynamic_cast<DungeonMainPalettePanel*>(
      manager->window_manager().GetWindowContent(1, "palette.dungeon_main"));
  ASSERT_NE(second_panel, nullptr);
  EXPECT_NE(first_panel, second_panel);

  manager->SwitchToSession(0);
  ASSERT_EQ(manager->GetCurrentSessionIndex(), 0u);
  EXPECT_EQ(
      manager->window_manager().GetWindowContent(0, "palette.dungeon_main"),
      first_panel);

  manager->SwitchToSession(1);
  ASSERT_EQ(manager->GetCurrentSessionIndex(), 1u);
  manager->CloseCurrentSession();
  ASSERT_EQ(manager->GetActiveSessionCount(), 1u);
  ASSERT_EQ(manager->GetCurrentSessionIndex(), 0u);
  EXPECT_EQ(
      manager->window_manager().GetWindowContent(0, "palette.dungeon_main"),
      first_panel);

  DrawPalettePanelOnce(first_panel);

  manager.reset();
  gfx::PaletteManager::Get().ResetForTesting();
}

TEST(EditorManagerWriteConflictTest,
     PalettePanelsSurviveCloseFirstThenOpenNewSession) {
  ScopedImGuiContext imgui;
  gfx::PaletteManager::Get().ResetForTesting();

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto rom_a = CreateMinimalRomFile("yaze_palette_close_first_a.sfc",
                                          "PALETTE CLOSE FIRST A", 1024 * 1024);
  const auto rom_b = CreateMinimalRomFile("yaze_palette_close_first_b.sfc",
                                          "PALETTE CLOSE FIRST B", 1024 * 1024);
  const auto rom_c = CreateMinimalRomFile("yaze_palette_close_first_c.sfc",
                                          "PALETTE CLOSE FIRST C", 1024 * 1024);
  ScopedFileCleanup cleanup_a{rom_a};
  ScopedFileCleanup cleanup_b{rom_b};
  ScopedFileCleanup cleanup_c{rom_c};

  ASSERT_OK(manager->OpenRomOrProject(rom_a.string()));
  ASSERT_OK(manager->EnsureEditorAssetsLoaded(EditorType::kPalette));
  ASSERT_NE(manager->window_manager().GetWindowContent(
                manager->GetCurrentSessionId(), "palette.dungeon_main"),
            nullptr);

  ASSERT_OK(manager->OpenRomOrProject(rom_b.string()));
  ASSERT_OK(manager->EnsureEditorAssetsLoaded(EditorType::kPalette));
  ASSERT_EQ(manager->GetCurrentSessionIndex(), 1u);
  ASSERT_EQ(manager->GetCurrentSessionId(), 1u);
  auto* second_panel = dynamic_cast<DungeonMainPalettePanel*>(
      manager->window_manager().GetWindowContent(1, "palette.dungeon_main"));
  ASSERT_NE(second_panel, nullptr);

  manager->RemoveSession(0);
  ASSERT_EQ(manager->GetActiveSessionCount(), 1u);
  ASSERT_EQ(manager->GetCurrentSessionIndex(), 0u);
  ASSERT_EQ(manager->GetCurrentSessionId(), 1u);
  EXPECT_EQ(manager->window_manager().GetActiveSessionId(), 1u);
  EXPECT_EQ(
      manager->window_manager().GetWindowContent(1, "palette.dungeon_main"),
      second_panel);
  EXPECT_EQ(
      manager->window_manager().GetWindowContent(0, "palette.dungeon_main"),
      nullptr);

  ASSERT_OK(manager->OpenRomOrProject(rom_c.string()));
  ASSERT_OK(manager->EnsureEditorAssetsLoaded(EditorType::kPalette));
  ASSERT_EQ(manager->GetCurrentSessionIndex(), 1u);
  ASSERT_EQ(manager->GetCurrentSessionId(), 2u);
  auto* third_panel = dynamic_cast<DungeonMainPalettePanel*>(
      manager->window_manager().GetWindowContent(2, "palette.dungeon_main"));
  ASSERT_NE(third_panel, nullptr);
  EXPECT_NE(third_panel, second_panel);
  EXPECT_EQ(
      manager->window_manager().GetWindowContent(1, "palette.dungeon_main"),
      second_panel);

  manager->SwitchToSession(0);
  ASSERT_EQ(manager->GetCurrentSessionId(), 1u);
  DrawPalettePanelOnce(second_panel);

  manager.reset();
  gfx::PaletteManager::Get().ResetForTesting();
}

TEST(EditorManagerWriteConflictTest,
     PalettePanelsSurviveCloseMiddleThenOpenNewSession) {
  ScopedImGuiContext imgui;
  gfx::PaletteManager::Get().ResetForTesting();

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto rom_a = CreateMinimalRomFile(
      "yaze_palette_close_middle_a.sfc", "PALETTE CLOSE MIDDLE A", 1024 * 1024);
  const auto rom_b = CreateMinimalRomFile(
      "yaze_palette_close_middle_b.sfc", "PALETTE CLOSE MIDDLE B", 1024 * 1024);
  const auto rom_c = CreateMinimalRomFile(
      "yaze_palette_close_middle_c.sfc", "PALETTE CLOSE MIDDLE C", 1024 * 1024);
  const auto rom_d = CreateMinimalRomFile(
      "yaze_palette_close_middle_d.sfc", "PALETTE CLOSE MIDDLE D", 1024 * 1024);
  ScopedFileCleanup cleanup_a{rom_a};
  ScopedFileCleanup cleanup_b{rom_b};
  ScopedFileCleanup cleanup_c{rom_c};
  ScopedFileCleanup cleanup_d{rom_d};

  ASSERT_OK(manager->OpenRomOrProject(rom_a.string()));
  ASSERT_OK(manager->EnsureEditorAssetsLoaded(EditorType::kPalette));
  auto* first_panel = dynamic_cast<DungeonMainPalettePanel*>(
      manager->window_manager().GetWindowContent(0, "palette.dungeon_main"));
  ASSERT_NE(first_panel, nullptr);

  ASSERT_OK(manager->OpenRomOrProject(rom_b.string()));
  ASSERT_OK(manager->EnsureEditorAssetsLoaded(EditorType::kPalette));
  ASSERT_NE(
      manager->window_manager().GetWindowContent(1, "palette.dungeon_main"),
      nullptr);

  ASSERT_OK(manager->OpenRomOrProject(rom_c.string()));
  ASSERT_OK(manager->EnsureEditorAssetsLoaded(EditorType::kPalette));
  auto* third_panel = dynamic_cast<DungeonMainPalettePanel*>(
      manager->window_manager().GetWindowContent(2, "palette.dungeon_main"));
  ASSERT_NE(third_panel, nullptr);
  ASSERT_EQ(manager->GetCurrentSessionIndex(), 2u);
  ASSERT_EQ(manager->GetCurrentSessionId(), 2u);

  manager->RemoveSession(1);
  ASSERT_EQ(manager->GetActiveSessionCount(), 2u);
  ASSERT_EQ(manager->GetCurrentSessionIndex(), 1u);
  ASSERT_EQ(manager->GetCurrentSessionId(), 2u);
  EXPECT_EQ(manager->window_manager().GetActiveSessionId(), 2u);
  EXPECT_EQ(
      manager->window_manager().GetWindowContent(0, "palette.dungeon_main"),
      first_panel);
  EXPECT_EQ(
      manager->window_manager().GetWindowContent(2, "palette.dungeon_main"),
      third_panel);
  EXPECT_EQ(
      manager->window_manager().GetWindowContent(1, "palette.dungeon_main"),
      nullptr);

  ASSERT_OK(manager->OpenRomOrProject(rom_d.string()));
  ASSERT_OK(manager->EnsureEditorAssetsLoaded(EditorType::kPalette));
  ASSERT_EQ(manager->GetCurrentSessionIndex(), 2u);
  ASSERT_EQ(manager->GetCurrentSessionId(), 3u);
  auto* fourth_panel = dynamic_cast<DungeonMainPalettePanel*>(
      manager->window_manager().GetWindowContent(3, "palette.dungeon_main"));
  ASSERT_NE(fourth_panel, nullptr);
  EXPECT_NE(fourth_panel, third_panel);
  EXPECT_EQ(
      manager->window_manager().GetWindowContent(2, "palette.dungeon_main"),
      third_panel);

  manager->SwitchToSession(1);
  ASSERT_EQ(manager->GetCurrentSessionId(), 2u);
  DrawPalettePanelOnce(third_panel);

  manager.reset();
  gfx::PaletteManager::Get().ResetForTesting();
}

}  // namespace
}  // namespace yaze::editor
