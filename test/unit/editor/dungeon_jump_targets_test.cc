#include "app/editor/dungeon/dungeon_jump_targets.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "core/project.h"
#include "rom/rom.h"
#include "zelda3/dungeon/dungeon_spawn_point.h"
#include "zelda3/dungeon/room_entrance.h"

namespace yaze::editor {
namespace {

std::vector<uint8_t> BlankEntranceRom() {
  std::vector<uint8_t> data(0x200000, 0);
  for (int i = 0; i < zelda3::kNumRegularDungeonEntrances; ++i) {
    data[zelda3::kEntranceDungeon + i] = 0xFF;
  }
  return data;
}

void SetEntrance(std::vector<uint8_t>& data, int entrance, uint16_t room,
                 uint8_t dungeon) {
  data[zelda3::kEntranceRoom + entrance * 2] = room & 0xFF;
  data[zelda3::kEntranceRoom + entrance * 2 + 1] = room >> 8;
  data[zelda3::kEntranceDungeon + entrance] = dungeon;
}

TEST(DungeonJumpTargetsTest, VanillaRomUsesFirstEntrancePerDungeonByte) {
  auto data = BlankEntranceRom();
  SetEntrance(data, 0x04, 0x060, 0x02);
  SetEntrance(data, 0x08, 0x0C9, 0x04);
  SetEntrance(data, 0x09, 0x0B9, 0x04);
  SetEntrance(data, 0x0A, 0x050, 0x22);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(data).ok());

  const auto targets = BuildDungeonJumpTargets(&rom, nullptr);

  ASSERT_EQ(targets.size(), 3u);
  EXPECT_EQ(targets[0].name, "Hyrule Castle");
  EXPECT_EQ(targets[0].room_id, 0x060);
  EXPECT_EQ(targets[1].name, "Eastern Palace");
  EXPECT_EQ(targets[1].room_id, 0x0C9);
  EXPECT_EQ(targets[2].name, "Dungeon 22");
  EXPECT_EQ(targets[2].room_id, 0x050);
}

TEST(DungeonJumpTargetsTest, NoRomAndNoProjectYieldsNoTargets) {
  EXPECT_TRUE(BuildDungeonJumpTargets(nullptr, nullptr).empty());
}

TEST(DungeonJumpTargetsTest, ProjectRegistryPrefersEntranceRooms) {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("yaze_dungeon_jump_test_" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  const std::filesystem::path planning = root / "Docs" / "Dev" / "Planning";
  std::filesystem::create_directories(planning);
  std::ofstream(planning / "dungeons.json") << R"json({
    "dungeons": [
      {"id": "D1", "name": "Mushroom Grotto", "rooms": [
        {"id": "0x10", "name": "Hall"},
        {"id": "0x12", "name": "Gate", "type": "entrance"}
      ]},
      {"id": "D2", "name": "Tail Palace", "rooms": [
        {"id": "0x30", "name": "Lobby"}
      ]},
      {"id": "D3", "name": "Empty", "rooms": []}
    ]
  })json";

  project::YazeProject project;
  project.name = "Oracle of Secrets";
  project.filepath = (root / "Oracle-of-Secrets.yaze").string();
  ASSERT_TRUE(project.hack_manifest
                  .LoadFromString(R"json({
                    "manifest_version": 1,
                    "hack_name": "Oracle of Secrets"
                  })json")
                  .ok());
  ASSERT_TRUE(project.hack_manifest.LoadProjectRegistry(root.string()).ok());

  // The registry wins over ROM entrances when it names dungeons.
  auto data = BlankEntranceRom();
  SetEntrance(data, 0x04, 0x060, 0x02);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(data).ok());

  const auto targets = BuildDungeonJumpTargets(&rom, &project);

  ASSERT_EQ(targets.size(), 2u);
  EXPECT_EQ(targets[0].name, "D1 Mushroom Grotto");
  EXPECT_EQ(targets[0].room_id, 0x12);
  EXPECT_EQ(targets[1].name, "D2 Tail Palace");
  EXPECT_EQ(targets[1].room_id, 0x30);

  std::error_code ec;
  std::filesystem::remove_all(root, ec);
}

}  // namespace
}  // namespace yaze::editor
