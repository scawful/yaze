#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "app/editor/editor_manager.h"
#include "app/gfx/backend/null_renderer.h"
#include "core/features.h"
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

struct ScopedDirCleanup {
  std::filesystem::path path;
  ~ScopedDirCleanup() {
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

std::filesystem::path MakeTempPath(const std::string& basename) {
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

TEST(EditorManagerOracleRomSafetyTest,
     SaveRomBlocksWhenCollisionOverlapsWaterFillReservedRegion) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  // Create a ROM large enough to contain the expanded collision bank tail where
  // the Oracle WaterFill table is reserved.
  const std::filesystem::path rom_path = MakeTempPath("yaze_oracle_rom_safety.sfc");
  ScopedFileCleanup cleanup{rom_path};
  std::vector<uint8_t> rom_data(2 * 1024 * 1024, 0x00);
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

  // Mark a project as opened and load a minimal hack manifest + project
  // registry so Oracle guardrails are active.
  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "OracleRomSafetyTest";
  project->filepath = (rom_path.parent_path() / "project.yaze").string();
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.expected_hash.clear();  // Avoid hash-mismatch prompts.
  ASSERT_OK(project->hack_manifest.LoadFromString(
      R"json({ "manifest_version": 1, "hack_name": "oracle_unit_test" })json"));

  const std::filesystem::path code_dir = MakeTempPath("yaze_oracle_code");
  ScopedDirCleanup code_cleanup{code_dir};
  std::filesystem::create_directories(code_dir / "Docs" / "Dev" / "Planning");
  const std::filesystem::path story_events =
      code_dir / "Docs" / "Dev" / "Planning" / "story_events.json";
  {
    std::ofstream out(story_events, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(out.is_open());
    out << R"json({ "events": [ { "id": "EV-001", "name": "test" } ], "edges": [] })json";
    ASSERT_TRUE(out.good());
  }
  ASSERT_OK(project->hack_manifest.LoadProjectRegistry(code_dir.string()));
  ASSERT_TRUE(project->hack_manifest.HasProjectRegistry());

  Rom* rom = manager->GetCurrentRom();
  ASSERT_NE(rom, nullptr);
  ASSERT_TRUE(rom->is_loaded());

  // Corrupt the collision pointer table to point into the reserved WaterFill
  // table region. SaveRom must refuse to write this to disk.
  const uint32_t snes_ptr = PcToSnes(zelda3::kWaterFillTableStart);
  const int ptr_offset = zelda3::kCustomCollisionRoomPointers;  // room 0
  ASSERT_OK(rom->WriteByte(ptr_offset + 0, static_cast<uint8_t>(snes_ptr & 0xFF)));
  ASSERT_OK(
      rom->WriteByte(ptr_offset + 1, static_cast<uint8_t>((snes_ptr >> 8) & 0xFF)));
  ASSERT_OK(
      rom->WriteByte(ptr_offset + 2, static_cast<uint8_t>((snes_ptr >> 16) & 0xFF)));

  // Also mutate an unrelated byte so we can verify the disk write is blocked.
  constexpr uint32_t kPcOffset = 0x1234;
  const uint8_t original = rom_data[kPcOffset];
  const uint8_t mutated = static_cast<uint8_t>(original ^ 0xFF);
  ASSERT_OK(rom->WriteByte(static_cast<int>(kPcOffset), mutated));

  auto status = manager->SaveRom();
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition) << status;
  EXPECT_EQ(ReadByteAt(rom_path, static_cast<std::streamoff>(kPcOffset)),
            original);
}

}  // namespace
}  // namespace yaze::editor

