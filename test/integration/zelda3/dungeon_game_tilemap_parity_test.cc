// Compares yaze's room tilemaps with tilemaps captured from the real game.
//
// Needs a vanilla ROM and a capture folder written by
// scripts/agents/capture-game-room-tilemaps.py for that same ROM:
//   YAZE_GAME_TILEMAP_DIR=<dir> yaze_test_integration \
//       --gtest_filter='DungeonGameTilemapParityTest.*'
// Without the folder the test skips. Captures are ROM-derived data and must
// not be committed.

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "absl/strings/str_format.h"
#include "rom/rom.h"
#include "test_utils.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/editor_dungeon_state.h"
#include "zelda3/dungeon/game_tilemap_comparison.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/game_data.h"

namespace yaze::test {
namespace {

std::vector<uint8_t> ReadFile(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), {});
}

class DungeonGameTilemapParityTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* dir = std::getenv("YAZE_GAME_TILEMAP_DIR");
    if (dir == nullptr) {
      GTEST_SKIP() << "Set YAZE_GAME_TILEMAP_DIR to a capture folder.";
    }
    capture_dir_ = dir;
    YAZE_SKIP_IF_ROM_MISSING(RomRole::kVanilla, "DungeonGameTilemapParityTest");
    rom_ = std::make_unique<Rom>();
    ASSERT_TRUE(
        rom_->LoadFromFile(TestRomManager::GetRomPath(RomRole::kVanilla)).ok());
    ASSERT_TRUE(zelda3::LoadGameData(*rom_, game_data_).ok());
  }

  std::filesystem::path capture_dir_;
  std::unique_ptr<Rom> rom_;
  zelda3::GameData game_data_;
};

TEST_F(DungeonGameTilemapParityTest, CompareCapturedRooms) {
  int rooms_compared = 0;
  int rooms_exact = 0;
  long tiles_matching = 0;
  long tiles_total = 0;
  long unowned_differences = 0;
  std::map<int, std::pair<int, int>> by_object;  // id -> {checked, differ}
  std::map<uint16_t, int> difference_kinds;

  for (int room_id = 0; room_id < zelda3::kNumberOfRooms; ++room_id) {
    const auto path =
        capture_dir_ / absl::StrFormat("room_%03X.tilemap", room_id);
    if (!std::filesystem::exists(path)) {
      continue;
    }
    auto game = zelda3::ParseGameRoomTilemaps(ReadFile(path));
    ASSERT_TRUE(game.ok()) << path << ": " << game.status();

    zelda3::Room room = zelda3::LoadRoomFromRom(rom_.get(), room_id);
    room.SetGameData(&game_data_);
    // YAZE_GAME_ROOM_FLAGS=0xFFFF matches a capture made with
    // --room-flags 0xFF,0xFF: every chest, door, floor and wall flag set.
    const bool all_flags = std::getenv("YAZE_GAME_ROOM_FLAGS") != nullptr;
    if (all_flags) {
      if (auto* state = dynamic_cast<zelda3::EditorDungeonState*>(
              room.GetDungeonState())) {
        for (int i = 0; i < 16; ++i) {
          state->SetChestOpen(room_id, i, true);
          state->SetDoorOpen(room_id, i, true);
        }
        state->SetBigChestOpen(true);
        state->SetDoorSwitchActive(room_id, true);
        state->SetWaterFaceActive(room_id, true);
        state->SetDamFloodgateOpen(room_id, true);
        state->SetWallMoved(room_id, true);
        state->SetFloorBombable(room_id, true);
        state->SetRupeeFloorCleared(room_id, true);
      }
    }
    room.RenderRoomGraphics();
    const auto yaze = zelda3::ComposeYazeRoomTilemaps(room);
    const auto& objects = room.GetTileObjects();
    std::vector<bool> hidden(objects.size());
    for (size_t i = 0; i < objects.size(); ++i) {
      hidden[i] = !all_flags && zelda3::GameHidesObjectOnRoomLoad(
                                    static_cast<int>(room.tag1()),
                                    static_cast<int>(room.tag2()), objects[i]);
    }
    const auto owners = zelda3::ComputeObjectTileOwners(rom_.get(), room_id,
                                                        objects, yaze, hidden);
    const auto check =
        zelda3::CompareRoomTilemaps(room_id, *game, yaze, owners, objects);

    if (std::getenv("YAZE_GAME_TILEMAP_VERBOSE") != nullptr) {
      std::cout << absl::StrFormat(
          "hdr room=%03X merge=%d effect=%d tag1=%d tag2=%d diffs=%zu\n",
          room_id, room.layer_merging().ID, static_cast<int>(room.effect()),
          static_cast<int>(room.tag1()), static_cast<int>(room.tag2()),
          check.differences.size());
    }
    ++rooms_compared;
    const int matching = check.bg1_matching + check.bg2_matching;
    tiles_matching += matching;
    tiles_total += 2 * static_cast<long>(zelda3::kRoomTilemapWords);
    unowned_differences += check.unowned_differences;
    if (check.differences.empty()) {
      ++rooms_exact;
    }
    for (const auto& difference : check.differences) {
      ++difference_kinds[difference.game ^ difference.yaze];
      if (std::getenv("YAZE_GAME_TILEMAP_VERBOSE") != nullptr) {
        std::cout << absl::StrFormat(
            "diff room=%03X bg%d x=%d y=%d game=%04X yaze=%04X owner=%d "
            "id=%03X\n",
            room_id, difference.layer, difference.x, difference.y,
            difference.game, difference.yaze, difference.owner,
            difference.owner >= 0
                ? objects[static_cast<size_t>(difference.owner)].id_
                : 0);
      }
    }
    for (const auto& placement : check.placements) {
      if (placement.tiles_owned == 0) {
        continue;
      }
      auto& counts = by_object[placement.object_id];
      ++counts.first;
      if (placement.tiles_different > 0) {
        ++counts.second;
      }
    }
  }
  if (rooms_compared == 0) {
    GTEST_SKIP() << "No room_XXX.tilemap files in " << capture_dir_;
  }

  int objects_clean = 0;
  for (const auto& [id, counts] : by_object) {
    if (counts.second == 0) {
      ++objects_clean;
    }
  }
  std::cout << absl::StrFormat(
      "rooms compared %d, exact %d; tiles matching %ld/%ld; unowned "
      "differing tiles %ld; object IDs checked %zu, all placements match %d\n",
      rooms_compared, rooms_exact, tiles_matching, tiles_total,
      unowned_differences, by_object.size(), objects_clean);
  std::vector<std::pair<int, uint16_t>> kinds;
  for (const auto& [bits, count] : difference_kinds) {
    kinds.push_back({count, bits});
  }
  std::sort(kinds.rbegin(), kinds.rend());
  for (size_t i = 0; i < kinds.size() && i < 8; ++i) {
    std::cout << absl::StrFormat(
        "  xor %04X (%s): %d tiles\n", kinds[i].second,
        zelda3::DescribeTileWordDifference(kinds[i].second), kinds[i].first);
  }
  for (const auto& [id, counts] : by_object) {
    if (counts.second > 0) {
      std::cout << absl::StrFormat(
          "  object 0x%03X: %d of %d placements differ\n", id, counts.second,
          counts.first);
    }
  }
  RecordProperty("rooms_compared", rooms_compared);
  RecordProperty("rooms_exact", rooms_exact);
}

}  // namespace
}  // namespace yaze::test
