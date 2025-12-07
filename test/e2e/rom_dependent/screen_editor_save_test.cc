#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "e2e/rom_dependent/editor_save_test_base.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "testing.h"
#include "zelda3/game_data.h"
#include "zelda3/screen/dungeon_map.h"

namespace yaze {
namespace test {

/**
 * @brief E2E Test Suite for ScreenEditor (DungeonMap) Save Operations
 *
 * Validates the complete dungeon map editing workflow:
 * 1. Load ROM and dungeon map data
 * 2. Modify floor/room assignments
 * 3. Save changes to ROM
 * 4. Reload ROM and verify edits persisted
 * 5. Verify no data corruption occurred
 */
class ScreenEditorSaveTest : public EditorSaveTestBase {
 protected:
  void SetUp() override {
    EditorSaveTestBase::SetUp();

    // Load the test ROM
    rom_ = std::make_unique<Rom>();
    auto load_result = rom_->LoadFromFile(test_rom_path_);
    if (!load_result.ok()) {
      GTEST_SKIP() << "Failed to load test ROM: " << load_result.message();
    }

    // Load game data
    game_data_ = std::make_unique<zelda3::GameData>();
    auto gd_result = zelda3::LoadGameData(*rom_, *game_data_);
    if (!gd_result.ok()) {
      GTEST_SKIP() << "Failed to load game data: " << gd_result.message();
    }

    // Load dungeon maps
    auto maps_result = zelda3::LoadDungeonMaps(*rom_, dungeon_map_labels_);
    if (!maps_result.ok()) {
      GTEST_SKIP() << "Failed to load dungeon maps: " 
                   << maps_result.status().message();
    }
    dungeon_maps_ = std::move(*maps_result);
  }

  // Helper to read dungeon map room data directly from ROM
  uint8_t ReadDungeonMapRoom(Rom& rom, int dungeon_id, int floor, int room) {
    int ptr = zelda3::kDungeonMapRoomsPtr + (dungeon_id * 2);
    int pc_ptr = SnesToPc(ptr);
    auto byte = rom.ReadByte(pc_ptr + room + (floor * zelda3::kNumRooms));
    return byte.ok() ? *byte : 0;
  }

  // Helper to write dungeon map room data to ROM
  absl::Status WriteDungeonMapRoom(Rom& rom, int dungeon_id, int floor, 
                                   int room, uint8_t value) {
    int ptr = zelda3::kDungeonMapRoomsPtr + (dungeon_id * 2);
    int pc_ptr = SnesToPc(ptr);
    return rom.WriteByte(pc_ptr + room + (floor * zelda3::kNumRooms), value);
  }

  // Helper to read dungeon map GFX data from ROM
  uint8_t ReadDungeonMapGfx(Rom& rom, int dungeon_id, int floor, int room) {
    int ptr = zelda3::kDungeonMapGfxPtr + (dungeon_id * 2);
    int pc_ptr = SnesToPc(ptr);
    // Note: GFX pointer increments differently (see SaveDungeonMaps)
    auto byte = rom.ReadByte(pc_ptr + room + (floor * zelda3::kNumRooms));
    return byte.ok() ? *byte : 0;
  }

  std::unique_ptr<Rom> rom_;
  std::unique_ptr<zelda3::GameData> game_data_;
  std::vector<zelda3::DungeonMap> dungeon_maps_;
  zelda3::DungeonMapLabels dungeon_map_labels_;
};

// Test 1: Single dungeon map floor room modification
TEST_F(ScreenEditorSaveTest, SingleFloorRoom_SaveAndReload) {
  if (dungeon_maps_.empty()) {
    GTEST_SKIP() << "No dungeon maps loaded";
  }

  // Test with first dungeon (Hyrule Castle)
  const int dungeon_id = 0;
  const int floor = 0;
  const int room = 0;

  // Record original value
  uint8_t original_room = dungeon_maps_[dungeon_id].floor_rooms[floor][room];

  // Modify the room assignment
  uint8_t new_room = (original_room + 1) % 0xFF;
  dungeon_maps_[dungeon_id].floor_rooms[floor][room] = new_room;

  // Save via SaveDungeonMaps
  ASSERT_OK(zelda3::SaveDungeonMaps(*rom_, dungeon_maps_));

  // Save ROM to disk
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload and verify
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  zelda3::DungeonMapLabels reloaded_labels;
  auto reloaded_maps = zelda3::LoadDungeonMaps(*reloaded_rom, reloaded_labels);
  ASSERT_TRUE(reloaded_maps.ok());

  EXPECT_EQ((*reloaded_maps)[dungeon_id].floor_rooms[floor][room], new_room)
      << "Dungeon map room modification should persist";
}

// Test 2: Multiple dungeon modifications
TEST_F(ScreenEditorSaveTest, MultipleDungeons_SaveAndReload) {
  if (dungeon_maps_.size() < 3) {
    GTEST_SKIP() << "Not enough dungeons for multi-dungeon test";
  }

  // Modify rooms in dungeons 0, 1, and 2
  const std::vector<int> test_dungeons = {0, 1, 2};
  std::map<int, uint8_t> original_rooms;
  std::map<int, uint8_t> modified_rooms;

  for (int d : test_dungeons) {
    if (dungeon_maps_[d].nbr_of_floor > 0) {
      original_rooms[d] = dungeon_maps_[d].floor_rooms[0][0];
      modified_rooms[d] = (original_rooms[d] + d + 1) % 0xFF;
      dungeon_maps_[d].floor_rooms[0][0] = modified_rooms[d];
    }
  }

  // Save all modifications
  ASSERT_OK(zelda3::SaveDungeonMaps(*rom_, dungeon_maps_));
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload and verify all changes
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  zelda3::DungeonMapLabels reloaded_labels;
  auto reloaded_maps = zelda3::LoadDungeonMaps(*reloaded_rom, reloaded_labels);
  ASSERT_TRUE(reloaded_maps.ok());

  for (int d : test_dungeons) {
    if (modified_rooms.count(d) > 0) {
      EXPECT_EQ((*reloaded_maps)[d].floor_rooms[0][0], modified_rooms[d])
          << "Dungeon " << d << " modification should persist";
    }
  }
}

// Test 3: Floor and basement data persistence
TEST_F(ScreenEditorSaveTest, FloorBasement_Persistence) {
  // Test dungeon with multiple floors and basements
  int target_dungeon = -1;
  for (size_t d = 0; d < dungeon_maps_.size(); ++d) {
    if (dungeon_maps_[d].nbr_of_floor >= 2 || 
        dungeon_maps_[d].nbr_of_basement >= 1) {
      target_dungeon = static_cast<int>(d);
      break;
    }
  }

  if (target_dungeon < 0) {
    GTEST_SKIP() << "No dungeon with multiple floors/basements found";
  }

  auto& dm = dungeon_maps_[target_dungeon];
  const int total_levels = dm.nbr_of_floor + dm.nbr_of_basement;

  // Modify a room on each level
  std::vector<uint8_t> original_rooms(total_levels);
  std::vector<uint8_t> modified_rooms(total_levels);

  for (int level = 0; level < total_levels; ++level) {
    original_rooms[level] = dm.floor_rooms[level][0];
    modified_rooms[level] = (original_rooms[level] + level + 5) % 0xFF;
    dm.floor_rooms[level][0] = modified_rooms[level];
  }

  // Save and reload
  ASSERT_OK(zelda3::SaveDungeonMaps(*rom_, dungeon_maps_));
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  zelda3::DungeonMapLabels reloaded_labels;
  auto reloaded_maps = zelda3::LoadDungeonMaps(*reloaded_rom, reloaded_labels);
  ASSERT_TRUE(reloaded_maps.ok());

  for (int level = 0; level < total_levels; ++level) {
    EXPECT_EQ((*reloaded_maps)[target_dungeon].floor_rooms[level][0], 
              modified_rooms[level])
        << "Level " << level << " modification should persist";
  }
}

// Test 4: GFX data persistence
TEST_F(ScreenEditorSaveTest, GfxData_Persistence) {
  if (dungeon_maps_.empty()) {
    GTEST_SKIP() << "No dungeon maps loaded";
  }

  const int dungeon_id = 0;
  const int floor = 0;
  const int room = 0;

  // Record and modify GFX data
  uint8_t original_gfx = dungeon_maps_[dungeon_id].floor_gfx[floor][room];
  uint8_t modified_gfx = (original_gfx + 0x10) & 0xFF;
  dungeon_maps_[dungeon_id].floor_gfx[floor][room] = modified_gfx;

  // Save and reload
  ASSERT_OK(zelda3::SaveDungeonMaps(*rom_, dungeon_maps_));
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  zelda3::DungeonMapLabels reloaded_labels;
  auto reloaded_maps = zelda3::LoadDungeonMaps(*reloaded_rom, reloaded_labels);
  ASSERT_TRUE(reloaded_maps.ok());

  EXPECT_EQ((*reloaded_maps)[dungeon_id].floor_gfx[floor][room], modified_gfx)
      << "GFX modification should persist";
}

// Test 5: No cross-dungeon corruption
TEST_F(ScreenEditorSaveTest, NoCrossDungeonCorruption) {
  if (dungeon_maps_.size() < 3) {
    GTEST_SKIP() << "Not enough dungeons for corruption test";
  }

  // Record data from dungeons 0 and 2
  uint8_t dungeon0_room = dungeon_maps_[0].floor_rooms[0][0];
  uint8_t dungeon2_room = dungeon_maps_[2].floor_rooms[0][0];

  // Modify only dungeon 1
  uint8_t original_d1 = dungeon_maps_[1].floor_rooms[0][0];
  dungeon_maps_[1].floor_rooms[0][0] = (original_d1 + 0x55) % 0xFF;

  // Save
  ASSERT_OK(zelda3::SaveDungeonMaps(*rom_, dungeon_maps_));
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload and verify
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  zelda3::DungeonMapLabels reloaded_labels;
  auto reloaded_maps = zelda3::LoadDungeonMaps(*reloaded_rom, reloaded_labels);
  ASSERT_TRUE(reloaded_maps.ok());

  // Verify dungeon 1 was modified
  EXPECT_EQ((*reloaded_maps)[1].floor_rooms[0][0], (original_d1 + 0x55) % 0xFF);

  // Verify dungeons 0 and 2 were NOT corrupted
  EXPECT_EQ((*reloaded_maps)[0].floor_rooms[0][0], dungeon0_room)
      << "Dungeon 0 should not be corrupted";
  EXPECT_EQ((*reloaded_maps)[2].floor_rooms[0][0], dungeon2_room)
      << "Dungeon 2 should not be corrupted";
}

// Test 6: All rooms on a floor
TEST_F(ScreenEditorSaveTest, AllRoomsOnFloor_Persistence) {
  if (dungeon_maps_.empty()) {
    GTEST_SKIP() << "No dungeon maps loaded";
  }

  const int dungeon_id = 0;
  const int floor = 0;

  // Modify all rooms on the floor
  std::vector<uint8_t> original_rooms(zelda3::kNumRooms);
  std::vector<uint8_t> modified_rooms(zelda3::kNumRooms);

  for (int r = 0; r < zelda3::kNumRooms; ++r) {
    original_rooms[r] = dungeon_maps_[dungeon_id].floor_rooms[floor][r];
    modified_rooms[r] = (r * 7) % 0xFF;
    dungeon_maps_[dungeon_id].floor_rooms[floor][r] = modified_rooms[r];
  }

  // Save and reload
  ASSERT_OK(zelda3::SaveDungeonMaps(*rom_, dungeon_maps_));
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  zelda3::DungeonMapLabels reloaded_labels;
  auto reloaded_maps = zelda3::LoadDungeonMaps(*reloaded_rom, reloaded_labels);
  ASSERT_TRUE(reloaded_maps.ok());

  for (int r = 0; r < zelda3::kNumRooms; ++r) {
    EXPECT_EQ((*reloaded_maps)[dungeon_id].floor_rooms[floor][r], modified_rooms[r])
        << "Room " << r << " modification should persist";
  }
}

// Test 7: Round-trip without modification
TEST_F(ScreenEditorSaveTest, RoundTrip_NoModification) {
  // Record original state of first few dungeons
  std::vector<std::vector<std::vector<uint8_t>>> original_data;
  const int test_dungeons = std::min(3, static_cast<int>(dungeon_maps_.size()));

  for (int d = 0; d < test_dungeons; ++d) {
    std::vector<std::vector<uint8_t>> dungeon_data;
    const int levels = dungeon_maps_[d].nbr_of_floor + 
                       dungeon_maps_[d].nbr_of_basement;
    for (int l = 0; l < levels; ++l) {
      std::vector<uint8_t> floor_data(zelda3::kNumRooms);
      for (int r = 0; r < zelda3::kNumRooms; ++r) {
        floor_data[r] = dungeon_maps_[d].floor_rooms[l][r];
      }
      dungeon_data.push_back(floor_data);
    }
    original_data.push_back(dungeon_data);
  }

  // Save without modification
  ASSERT_OK(zelda3::SaveDungeonMaps(*rom_, dungeon_maps_));
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload and verify
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  zelda3::DungeonMapLabels reloaded_labels;
  auto reloaded_maps = zelda3::LoadDungeonMaps(*reloaded_rom, reloaded_labels);
  ASSERT_TRUE(reloaded_maps.ok());

  for (int d = 0; d < test_dungeons; ++d) {
    const int levels = dungeon_maps_[d].nbr_of_floor + 
                       dungeon_maps_[d].nbr_of_basement;
    for (int l = 0; l < levels && l < static_cast<int>(original_data[d].size()); ++l) {
      for (int r = 0; r < zelda3::kNumRooms; ++r) {
        EXPECT_EQ((*reloaded_maps)[d].floor_rooms[l][r], original_data[d][l][r])
            << "Dungeon " << d << " level " << l << " room " << r 
            << " should be preserved";
      }
    }
  }
}

// Test 8: Large batch dungeon modifications
TEST_F(ScreenEditorSaveTest, LargeBatch_DungeonModifications) {
  // Modify all dungeons, all floors, first room
  std::map<std::pair<int, int>, uint8_t> modifications;

  for (size_t d = 0; d < dungeon_maps_.size(); ++d) {
    const int levels = dungeon_maps_[d].nbr_of_floor + 
                       dungeon_maps_[d].nbr_of_basement;
    for (int l = 0; l < levels; ++l) {
      uint8_t original = dungeon_maps_[d].floor_rooms[l][0];
      uint8_t modified = (original + d + l) % 0xFF;
      dungeon_maps_[d].floor_rooms[l][0] = modified;
      modifications[{static_cast<int>(d), l}] = modified;
    }
  }

  // Save
  ASSERT_OK(zelda3::SaveDungeonMaps(*rom_, dungeon_maps_));
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload and verify
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  zelda3::DungeonMapLabels reloaded_labels;
  auto reloaded_maps = zelda3::LoadDungeonMaps(*reloaded_rom, reloaded_labels);
  ASSERT_TRUE(reloaded_maps.ok());

  int verified_count = 0;
  for (const auto& [key, expected] : modifications) {
    auto [d, l] = key;
    if ((*reloaded_maps)[d].floor_rooms[l][0] == expected) {
      verified_count++;
    }
  }

  EXPECT_EQ(verified_count, static_cast<int>(modifications.size()))
      << "All batch dungeon map modifications should persist";
}

}  // namespace test
}  // namespace yaze
