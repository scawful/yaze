#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/editor_manager.h"
#include "app/gfx/backend/null_renderer.h"
#include "app/startup_flags.h"
#include "core/features.h"
#include "rom/rom_diff.h"
#include "rom/snes.h"
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

TEST(EditorManagerWriteConflictTest, SaveRomBlocksAndAllowsBypass) {
  FeatureFlagsGuard guard;

  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  // Create a minimal ROM file that satisfies RomFileManager's size checks.
  const std::filesystem::path rom_path = MakeTempFilePath("yaze_write_conflict_test.sfc");
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

}  // namespace
}  // namespace yaze::editor
