#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/editor_manager.h"
#include "app/gfx/backend/null_renderer.h"
#include "core/features.h"
#include "testing.h"

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

struct ScopedDirectoryCleanup {
  std::filesystem::path path;
  ~ScopedDirectoryCleanup() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
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

}  // namespace
}  // namespace yaze::editor
