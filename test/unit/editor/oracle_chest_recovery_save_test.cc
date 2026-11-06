#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "absl/cleanup/cleanup.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/dungeon/dungeon_room_transfer.h"
#include "app/editor/editor_manager.h"
#include "app/gfx/resource/arena.h"
#include "core/features.h"
#include "core/project.h"
#include "core/source_artifact_publisher.h"
#include "editor_test_support.h"
#include "framework/headless_editor_test.h"
#include "framework/rom_save_fault.h"
#include "unique_temp_path.h"
#include "zelda3/dungeon/room.h"

namespace yaze::editor {
namespace {
std::vector<uint8_t> ReadRecoveryBytes(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}
void WriteRecoveryBytes(const std::filesystem::path& path,
                        const std::vector<uint8_t>& bytes) {
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}
std::string RecoveryHash(const std::vector<uint8_t>& bytes) {
  return core::ComputeSourceArtifactSha256(std::string_view(
      reinterpret_cast<const char*>(bytes.data()), bytes.size()));
}
auto RecoveryRewards(const std::vector<uint8_t>& bytes) {
  std::map<uint16_t, std::vector<std::pair<bool, uint8_t>>> rooms;
  const size_t length = bytes[0xEBF6] | (bytes[0xEBF7] << 8);
  for (size_t at = 0xE96E; at < 0xE96E + length; at += 3) {
    const uint16_t word = bytes[at] | (bytes[at + 1] << 8);
    rooms[word & 0x7FFF].emplace_back((word & 0x8000) != 0, bytes[at + 2]);
  }
  return rooms;
}

class OracleChestRecoverySaveTest : public test::HeadlessEditorTest {};

// This opt-in case consumes the immutable recovered user ROM through MM's
// existing private-ROM fixture contract. It never accesses the user's project
// or running editor. CI without that private fixture reports an explicit skip.
TEST_F(OracleChestRecoverySaveTest,
       Room26DeletionPreservesPinnedRewardsAndUserGeometry) {
  const char* fixture_path = std::getenv("YAZE_TEST_ROM_OOS");
  if (!fixture_path || !std::filesystem::exists(fixture_path))
    GTEST_SKIP() << "Provide the immutable Oracle room26 recovery ROM";
  const auto source_bytes = ReadRecoveryBytes(fixture_path);
  constexpr auto kSourceHash =
      "889422eae326331eba6ed6ea7d1db0f456e547f50b729e93e40f4c3220079542";
  if (RecoveryHash(source_bytes) != kSourceHash)
    GTEST_SKIP() << "This case requires the SHA256-bound room26 recovery ROM";
  ASSERT_EQ(source_bytes.size(), 0x200000u);
  ASSERT_EQ(source_bytes[0xEBF6] | (source_bytes[0xEBF7] << 8), 501);
  const auto root = test::UniqueTempPath("ocr");
  std::filesystem::create_directories(root);
  auto cleanup = absl::MakeCleanup([&] { std::filesystem::remove_all(root); });
  const auto target = root / "saved.sfc";
  WriteRecoveryBytes(target, source_bytes);
  const auto manifest_path = root / "hack_manifest.json";
  {
    std::ofstream manifest(manifest_path);
    manifest << R"({"manifest_version":3,"protected_regions":{"total_hooks":1,
      "regions":[{"start":"0x01E9E8","end":"0x01E9FF","size":23,
      "hook_count":1,"module":"Core"}]}})";
  }
  project::YazeProject project;
  project.name = "Private Oracle chest recovery";
  project.filepath = (root / "p.yaze").string();
  project.rom_filename = target.string();
  project.hack_manifest_file = manifest_path.string();
  project.rom_metadata.write_policy = project::RomWritePolicy::kBlock;
  ASSERT_TRUE(project.Save().ok());
  gfx::Arena::Get().ClearTextureQueue();
  auto cleanup_graphics =
      absl::MakeCleanup([] { gfx::Arena::Get().ClearTextureQueue(); });
  const auto old_flags = core::FeatureFlags::get();
  auto restore_flags =
      absl::MakeCleanup([&] { core::FeatureFlags::get() = old_flags; });
  auto manager = std::make_unique<EditorManager>();
  test::InitializeWithIsolatedSettings(*manager, renderer_.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = false;
  ASSERT_TRUE(manager->OpenRomOrProject(project.filepath).ok());
  ASSERT_TRUE(manager->EnsureEditorAssetsLoaded(EditorType::kDungeon).ok());
  ASSERT_TRUE(manager->GetCurrentProject()->hack_manifest.loaded());
  auto* dungeon = manager->GetCurrentEditorSet()->GetDungeonEditor();
  ASSERT_NE(dungeon, nullptr);
  dungeon->add_room(0x26);
  auto& room = dungeon->rooms()[0x26];
  room.EnsureObjectsLoaded();
  room.LoadChests();
  ASSERT_EQ(room.GetTileObjects().size(), 128u);
  ASSERT_EQ(room.GetChests().size(), 1u);
  ASSERT_EQ(room.GetChests()[0].id, 0x24);
  auto document = CaptureDungeonRoomDocument(room);
  document.contents.chests.clear();
  const auto serialized = SerializeDungeonRoomDocument(document);
  ASSERT_TRUE(serialized.ok()) << serialized.status();
  DungeonRoomTransferOptions options;
  options.domains = kTransferObjects;
  const auto plan =
      dungeon->PreviewRoomTransfer(0x26, -1, *serialized, options);
  ASSERT_TRUE(plan.ok()) << plan.status();
  const auto applied = dungeon->ApplyRoomTransfer(*plan);
  ASSERT_TRUE(applied.ok()) << applied;
  ASSERT_TRUE(room.GetChests().empty());
  auto& flags = core::FeatureFlags::get();
  flags.kSaveMessages = false;
  flags.kSaveGraphicsSheet = false;
  flags.kSaveAllPalettes = false;
  flags.kSaveGfxGroups = false;
  flags.kSaveDungeonMaps = false;
  flags.dungeon.kSaveObjects = true;
  flags.dungeon.kSaveChests = true;
  flags.dungeon.kSaveSprites = false;
  flags.dungeon.kSaveRoomHeaders = false;
  flags.dungeon.kSaveTorches = false;
  flags.dungeon.kSavePits = false;
  flags.dungeon.kSaveBlocks = false;
  flags.dungeon.kSaveCollision = false;
  flags.dungeon.kSaveWaterFillZones = false;
  flags.dungeon.kSavePotItems = false;
  flags.dungeon.kSaveEntrances = false;
  flags.dungeon.kSavePalettes = false;
  flags.overworld.kSaveOverworldMaps = false;
  flags.overworld.kSaveOverworldProperties = false;
  flags.overworld.kSaveOverworldSprites = false;
  flags.overworld.kSaveOverworldEntrances = false;
  flags.overworld.kSaveOverworldExits = false;
  flags.overworld.kSaveOverworldItems = false;
  {
    test::ScopedRomStagingFailure fault;
    EXPECT_FALSE(manager->SaveCurrentSession(false).ok());
  }
  EXPECT_EQ(manager->GetCurrentRom()->vector(), source_bytes);
  const auto saved = manager->SaveCurrentSession(false);
  ASSERT_TRUE(saved.ok()) << saved;
  const auto persisted = ReadRecoveryBytes(target);
  ASSERT_EQ(persisted.size(), source_bytes.size());
  auto expected_rewards = RecoveryRewards(source_bytes);
  expected_rewards.erase(0x26);
  EXPECT_EQ(RecoveryRewards(persisted), expected_rewards);
  EXPECT_EQ(persisted[0xEBF6] | (persisted[0xEBF7] << 8), 498);
  for (size_t pc = 0; pc < source_bytes.size(); ++pc)
    if (persisted[pc] != source_bytes[pc])
      EXPECT_TRUE(pc == 0xE992 || pc == 0xE994 || pc == 0xEBF6 ||
                  (pc >= 0x7FDC && pc < 0x7FE0))
          << std::hex << pc;
  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromFile(target.string()).ok());
  auto fresh = zelda3::LoadRoomFromRom(&reopened, 0x26);
  fresh.LoadChests();
  EXPECT_TRUE(fresh.GetChests().empty());
  EXPECT_EQ(fresh.EncodeObjects(), room.EncodeObjects());
  EXPECT_EQ(fresh.GetTileObjects().size(), 128u);
  ASSERT_TRUE(manager->SaveCurrentSession(false).ok());
  EXPECT_EQ(ReadRecoveryBytes(target), persisted);
  // Retain the first normal Save output only in MM's explicitly owned output
  // directory. It is a private qualification artifact, never a bundled ROM.
  const auto fixture = std::filesystem::path(fixture_path);
  const auto artifacts = fixture.parent_path() / "out";
  if (fixture.filename() == "test-rom.sfc" &&
      std::filesystem::is_directory(artifacts)) {
    WriteRecoveryBytes(artifacts / "oracle-chest-recovery-saved.sfc",
                       persisted);
    std::ofstream receipt(artifacts / "oracle-chest-recovery-save.json");
    receipt << nlohmann::json{{"source_sha256", kSourceHash},
                              {"saved_sha256", RecoveryHash(persisted)},
                              {"room26_objects", 128},
                              {"chest_byte_length", 498}}
                   .dump(2);
  }
  ASSERT_TRUE(dungeon->Undo().ok());
  ASSERT_TRUE(manager->SaveCurrentSession(false).ok());
  EXPECT_EQ(RecoveryRewards(ReadRecoveryBytes(target)),
            RecoveryRewards(source_bytes));
  ASSERT_TRUE(dungeon->Redo().ok());
  ASSERT_TRUE(manager->SaveCurrentSession(false).ok());
  EXPECT_EQ(RecoveryRewards(ReadRecoveryBytes(target)), expected_rewards);
  EXPECT_EQ(ReadRecoveryBytes(fixture_path), source_bytes);
}
}  // namespace
}  // namespace yaze::editor
