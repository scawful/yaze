#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/editor_manager.h"
#include "app/gfx/backend/null_renderer.h"
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
                                           const std::string& title) {
  const std::filesystem::path rom_path = MakeTempFilePath(basename);
  std::vector<uint8_t> rom_data(512 * 1024, 0x00);
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

void WriteLongPointer(std::vector<uint8_t>* data, int offset, uint32_t value) {
  ASSERT_NE(data, nullptr);
  ASSERT_GE(offset, 0);
  ASSERT_LT(offset + 2, static_cast<int>(data->size()));
  (*data)[offset] = static_cast<uint8_t>(value & 0xFF);
  (*data)[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
  (*data)[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
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

}  // namespace
}  // namespace yaze::editor
