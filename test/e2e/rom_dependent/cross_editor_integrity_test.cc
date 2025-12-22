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
#include "zelda3/overworld/overworld.h"
#include "zelda3/screen/dungeon_map.h"

namespace yaze {
namespace test {

/**
 * @brief E2E Test Suite for Cross-Editor Data Integrity
 *
 * Validates that editing with multiple editors simultaneously
 * doesn't cause data corruption:
 * 1. Overworld + Tile16 combined edits
 * 2. Dungeon + Palette combined edits
 * 3. Full editor workflow: Load -> Edit multiple editors -> Save -> Reload
 * 4. Concurrent modification detection
 */
class CrossEditorIntegrityTest : public EditorSaveTestBase {
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
  }

  std::unique_ptr<Rom> rom_;
  std::unique_ptr<zelda3::GameData> game_data_;
};

// Test 1: Overworld + Tile16 combined edits
TEST_F(CrossEditorIntegrityTest, Overworld_Plus_Tile16) {
  // Load overworld
  zelda3::Overworld overworld(rom_.get());
  ASSERT_OK(overworld.Load(rom_.get()));

  // --- Overworld Edit ---
  auto* map0 = overworld.mutable_overworld_map(0);
  uint8_t original_map_gfx = map0->area_graphics();
  map0->set_area_graphics((original_map_gfx + 1) % 256);

  // --- Tile16 Edit ---
  auto* tiles16_ptr = overworld.mutable_tiles16();
  if (tiles16_ptr == nullptr || tiles16_ptr->empty()) {
    GTEST_SKIP() << "No tile16 data to edit";
  }

  gfx::Tile16 original_tile0 = (*tiles16_ptr)[0];
  (*tiles16_ptr)[0].tile0_.id_ = (original_tile0.tile0_.id_ + 1) % 0x200;

  // --- Save Both ---
  ASSERT_OK(overworld.SaveMapProperties());
  ASSERT_OK(overworld.SaveMap16Tiles());
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // --- Reload and Verify Both Edits ---
  std::unique_ptr<Rom> reloaded;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded));

  zelda3::Overworld reloaded_ow(reloaded.get());
  ASSERT_OK(reloaded_ow.Load(reloaded.get()));

  // Verify overworld edit
  EXPECT_EQ(reloaded_ow.overworld_map(0)->area_graphics(),
            (original_map_gfx + 1) % 256)
      << "Overworld map edit should persist";

  // Verify tile16 edit
  const auto reloaded_tiles16 = reloaded_ow.tiles16();
  ASSERT_FALSE(reloaded_tiles16.empty());
  EXPECT_EQ(reloaded_tiles16[0].tile0_.id_, 
            (original_tile0.tile0_.id_ + 1) % 0x200)
      << "Tile16 edit should persist";
}

// Test 2: Overworld + Palette combined edits
TEST_F(CrossEditorIntegrityTest, Overworld_Plus_Palette) {
  // Load overworld
  zelda3::Overworld overworld(rom_.get());
  ASSERT_OK(overworld.Load(rom_.get()));

  // --- Overworld Edit ---
  auto* map5 = overworld.mutable_overworld_map(5);
  uint8_t original_palette_id = map5->main_palette();
  map5->set_main_palette((original_palette_id + 1) % 8);

  // --- Palette Edit ---
  const uint32_t palette_offset = 0xDE6C8;  // Overworld main palette
  auto original_color = rom_->ReadWord(palette_offset);
  ASSERT_TRUE(original_color.ok());

  uint16_t new_color = (*original_color + 0x0421) & 0x7FFF;
  ASSERT_OK(rom_->WriteWord(palette_offset, new_color));

  // --- Save Both ---
  ASSERT_OK(overworld.SaveMapProperties());
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // --- Reload and Verify Both Edits ---
  std::unique_ptr<Rom> reloaded;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded));

  zelda3::Overworld reloaded_ow(reloaded.get());
  ASSERT_OK(reloaded_ow.Load(reloaded.get()));

  // Verify overworld edit
  EXPECT_EQ(reloaded_ow.overworld_map(5)->main_palette(),
            (original_palette_id + 1) % 8)
      << "Overworld palette ID edit should persist";

  // Verify palette color edit
  auto reloaded_color = reloaded->ReadWord(palette_offset);
  ASSERT_TRUE(reloaded_color.ok());
  EXPECT_EQ(*reloaded_color, new_color)
      << "Palette color edit should persist";
}

// Test 3: Dungeon + Palette combined edits
TEST_F(CrossEditorIntegrityTest, Dungeon_Plus_Palette) {
  // --- Dungeon Edit ---
  const int room_id = 0;
  uint32_t room_header_addr = 0xF8000 + (room_id * 14);
  
  auto original_header = rom_->ReadByte(room_header_addr);
  ASSERT_TRUE(original_header.ok());
  
  uint8_t modified_header = (*original_header + 0x10) & 0xFF;
  ASSERT_OK(rom_->WriteByte(room_header_addr, modified_header));

  // --- Palette Edit ---
  const uint32_t dungeon_palette_offset = 0xDD734;
  auto original_color = rom_->ReadWord(dungeon_palette_offset);
  ASSERT_TRUE(original_color.ok());

  uint16_t new_color = (*original_color + 0x0842) & 0x7FFF;
  ASSERT_OK(rom_->WriteWord(dungeon_palette_offset, new_color));

  // --- Save ---
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // --- Reload and Verify Both Edits ---
  std::unique_ptr<Rom> reloaded;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded));

  // Verify dungeon edit
  auto reloaded_header = reloaded->ReadByte(room_header_addr);
  ASSERT_TRUE(reloaded_header.ok());
  EXPECT_EQ(*reloaded_header, modified_header)
      << "Dungeon header edit should persist";

  // Verify palette edit
  auto reloaded_color = reloaded->ReadWord(dungeon_palette_offset);
  ASSERT_TRUE(reloaded_color.ok());
  EXPECT_EQ(*reloaded_color, new_color)
      << "Dungeon palette color edit should persist";
}

// Test 4: Dungeon Map + Overworld combined edits
TEST_F(CrossEditorIntegrityTest, DungeonMap_Plus_Overworld) {
  // --- Load Dungeon Maps ---
  zelda3::DungeonMapLabels labels;
  auto maps_result = zelda3::LoadDungeonMaps(*rom_, labels);
  if (!maps_result.ok()) {
    GTEST_SKIP() << "Failed to load dungeon maps";
  }
  auto dungeon_maps = std::move(*maps_result);

  // --- Dungeon Map Edit ---
  if (dungeon_maps.empty()) {
    GTEST_SKIP() << "No dungeon maps available";
  }

  uint8_t original_dm_room = dungeon_maps[0].floor_rooms[0][0];
  dungeon_maps[0].floor_rooms[0][0] = (original_dm_room + 5) % 0xFF;

  // --- Overworld Edit ---
  zelda3::Overworld overworld(rom_.get());
  ASSERT_OK(overworld.Load(rom_.get()));

  auto* map10 = overworld.mutable_overworld_map(10);
  uint8_t original_ow_gfx = map10->area_graphics();
  map10->set_area_graphics((original_ow_gfx + 3) % 256);

  // --- Save Both ---
  ASSERT_OK(zelda3::SaveDungeonMaps(*rom_, dungeon_maps));
  ASSERT_OK(overworld.SaveMapProperties());
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // --- Reload and Verify Both Edits ---
  std::unique_ptr<Rom> reloaded;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded));

  // Verify dungeon map edit
  zelda3::DungeonMapLabels reloaded_labels;
  auto reloaded_maps = zelda3::LoadDungeonMaps(*reloaded, reloaded_labels);
  ASSERT_TRUE(reloaded_maps.ok());
  EXPECT_EQ((*reloaded_maps)[0].floor_rooms[0][0], 
            (original_dm_room + 5) % 0xFF)
      << "Dungeon map edit should persist";

  // Verify overworld edit
  zelda3::Overworld reloaded_ow(reloaded.get());
  ASSERT_OK(reloaded_ow.Load(reloaded.get()));
  EXPECT_EQ(reloaded_ow.overworld_map(10)->area_graphics(),
            (original_ow_gfx + 3) % 256)
      << "Overworld edit should persist";
}

// Test 5: Full editor workflow - all editors
TEST_F(CrossEditorIntegrityTest, FullWorkflow_AllEditors) {
  // --- Load All Data ---
  zelda3::Overworld overworld(rom_.get());
  ASSERT_OK(overworld.Load(rom_.get()));

  zelda3::DungeonMapLabels labels;
  auto dungeon_maps_result = zelda3::LoadDungeonMaps(*rom_, labels);
  if (!dungeon_maps_result.ok()) {
    GTEST_SKIP() << "Failed to load dungeon maps";
  }
  auto dungeon_maps = std::move(*dungeon_maps_result);

  // --- Record Original Values ---
  uint8_t orig_ow_gfx = overworld.overworld_map(0)->area_graphics();
  
  auto* tiles16_ptr = overworld.mutable_tiles16();
  uint16_t orig_tile16_id = (tiles16_ptr && !tiles16_ptr->empty()) 
                            ? (*tiles16_ptr)[0].tile0_.id_ : 0;
  
  uint8_t orig_dm_room = dungeon_maps.empty() ? 0 : 
                         dungeon_maps[0].floor_rooms[0][0];

  const uint32_t palette_offset = 0xDE6C8;
  auto orig_palette = rom_->ReadWord(palette_offset);
  ASSERT_TRUE(orig_palette.ok());

  const uint32_t room_header = 0xF8000;
  auto orig_room_header = rom_->ReadByte(room_header);
  ASSERT_TRUE(orig_room_header.ok());

  // --- Make All Edits ---
  // Overworld map
  overworld.mutable_overworld_map(0)->set_area_graphics((orig_ow_gfx + 1) % 256);

  // Tile16
  if (tiles16_ptr && !tiles16_ptr->empty()) {
    (*tiles16_ptr)[0].tile0_.id_ = (orig_tile16_id + 1) % 0x200;
  }

  // Dungeon map
  if (!dungeon_maps.empty()) {
    dungeon_maps[0].floor_rooms[0][0] = (orig_dm_room + 1) % 0xFF;
  }

  // Palette
  uint16_t new_palette = (*orig_palette + 0x0421) & 0x7FFF;
  ASSERT_OK(rom_->WriteWord(palette_offset, new_palette));

  // Room header
  uint8_t new_room_header = (*orig_room_header + 0x10) & 0xFF;
  ASSERT_OK(rom_->WriteByte(room_header, new_room_header));

  // --- Save All ---
  ASSERT_OK(overworld.SaveMapProperties());
  ASSERT_OK(overworld.SaveMap16Tiles());
  if (!dungeon_maps.empty()) {
    ASSERT_OK(zelda3::SaveDungeonMaps(*rom_, dungeon_maps));
  }
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // --- Reload and Verify All ---
  std::unique_ptr<Rom> reloaded;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded));

  // Verify overworld
  zelda3::Overworld reloaded_ow(reloaded.get());
  ASSERT_OK(reloaded_ow.Load(reloaded.get()));
  EXPECT_EQ(reloaded_ow.overworld_map(0)->area_graphics(),
            (orig_ow_gfx + 1) % 256)
      << "Full workflow: Overworld edit should persist";

  // Verify tile16
  const auto reloaded_tiles16 = reloaded_ow.tiles16();
  if (!reloaded_tiles16.empty()) {
    EXPECT_EQ(reloaded_tiles16[0].tile0_.id_, (orig_tile16_id + 1) % 0x200)
        << "Full workflow: Tile16 edit should persist";
  }

  // Verify dungeon map
  zelda3::DungeonMapLabels reloaded_labels;
  auto reloaded_dm = zelda3::LoadDungeonMaps(*reloaded, reloaded_labels);
  if (reloaded_dm.ok() && !(*reloaded_dm).empty()) {
    EXPECT_EQ((*reloaded_dm)[0].floor_rooms[0][0], (orig_dm_room + 1) % 0xFF)
        << "Full workflow: Dungeon map edit should persist";
  }

  // Verify palette
  auto reloaded_palette = reloaded->ReadWord(palette_offset);
  ASSERT_TRUE(reloaded_palette.ok());
  EXPECT_EQ(*reloaded_palette, new_palette)
      << "Full workflow: Palette edit should persist";

  // Verify room header
  auto reloaded_room = reloaded->ReadByte(room_header);
  ASSERT_TRUE(reloaded_room.ok());
  EXPECT_EQ(*reloaded_room, new_room_header)
      << "Full workflow: Room header edit should persist";
}

// Test 6: Multiple maps with no cross-corruption
TEST_F(CrossEditorIntegrityTest, MultipleMaps_NoCrossCorruption) {
  zelda3::Overworld overworld(rom_.get());
  ASSERT_OK(overworld.Load(rom_.get()));

  // Record data for maps we won't modify
  std::vector<uint8_t> untouched_gfx;
  const std::vector<int> untouched_maps = {5, 10, 15, 20, 25};
  for (int map_id : untouched_maps) {
    untouched_gfx.push_back(overworld.overworld_map(map_id)->area_graphics());
  }

  // Modify only maps 0-4
  for (int i = 0; i < 5; ++i) {
    auto* map = overworld.mutable_overworld_map(i);
    map->set_area_graphics((map->area_graphics() + i + 1) % 256);
  }

  // Save
  ASSERT_OK(overworld.SaveMapProperties());
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload
  std::unique_ptr<Rom> reloaded;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded));

  zelda3::Overworld reloaded_ow(reloaded.get());
  ASSERT_OK(reloaded_ow.Load(reloaded.get()));

  // Verify untouched maps weren't corrupted
  for (size_t i = 0; i < untouched_maps.size(); ++i) {
    int map_id = untouched_maps[i];
    EXPECT_EQ(reloaded_ow.overworld_map(map_id)->area_graphics(), untouched_gfx[i])
        << "Map " << map_id << " should not be corrupted";
  }
}

// Test 7: Large scale combined edits
TEST_F(CrossEditorIntegrityTest, LargeScale_CombinedEdits) {
  zelda3::Overworld overworld(rom_.get());
  ASSERT_OK(overworld.Load(rom_.get()));

  // Edit many overworld maps
  const int num_map_edits = 50;
  std::map<int, uint8_t> expected_gfx;
  for (int i = 0; i < num_map_edits; ++i) {
    auto* map = overworld.mutable_overworld_map(i);
    expected_gfx[i] = (map->area_graphics() + i) % 256;
    map->set_area_graphics(expected_gfx[i]);
  }

  // Edit many palette colors
  const uint32_t palette_base = 0xDE6C8;
  const int num_palette_edits = 32;
  std::map<uint32_t, uint16_t> expected_colors;
  for (int i = 0; i < num_palette_edits; ++i) {
    uint32_t offset = palette_base + (i * 2);
    auto orig = rom_->ReadWord(offset);
    if (orig.ok()) {
      expected_colors[offset] = (*orig + i) & 0x7FFF;
      ASSERT_OK(rom_->WriteWord(offset, expected_colors[offset]));
    }
  }

  // Save
  ASSERT_OK(overworld.SaveMapProperties());
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload and verify
  std::unique_ptr<Rom> reloaded;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded));

  zelda3::Overworld reloaded_ow(reloaded.get());
  ASSERT_OK(reloaded_ow.Load(reloaded.get()));

  // Verify map edits
  int map_verified = 0;
  for (const auto& [map_id, gfx] : expected_gfx) {
    if (reloaded_ow.overworld_map(map_id)->area_graphics() == gfx) {
      map_verified++;
    }
  }
  EXPECT_EQ(map_verified, num_map_edits)
      << "All map edits should persist";

  // Verify palette edits
  int palette_verified = 0;
  for (const auto& [offset, color] : expected_colors) {
    auto reloaded_color = reloaded->ReadWord(offset);
    if (reloaded_color.ok() && *reloaded_color == color) {
      palette_verified++;
    }
  }
  EXPECT_EQ(palette_verified, static_cast<int>(expected_colors.size()))
      << "All palette edits should persist";
}

// Test 8: Sequential save operations
TEST_F(CrossEditorIntegrityTest, SequentialSaveOperations) {
  zelda3::Overworld overworld(rom_.get());
  ASSERT_OK(overworld.Load(rom_.get()));

  // First save cycle - overworld only
  auto* map0 = overworld.mutable_overworld_map(0);
  uint8_t gfx_v1 = (map0->area_graphics() + 1) % 256;
  map0->set_area_graphics(gfx_v1);
  
  ASSERT_OK(overworld.SaveMapProperties());
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Second save cycle - add palette edit
  const uint32_t palette_offset = 0xDE6C8;
  auto color1 = rom_->ReadWord(palette_offset);
  ASSERT_TRUE(color1.ok());
  uint16_t new_color = (*color1 + 0x0421) & 0x7FFF;
  ASSERT_OK(rom_->WriteWord(palette_offset, new_color));

  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Third save cycle - modify overworld again
  uint8_t gfx_v2 = (gfx_v1 + 1) % 256;
  map0->set_area_graphics(gfx_v2);
  
  ASSERT_OK(overworld.SaveMapProperties());
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Verify final state
  std::unique_ptr<Rom> reloaded;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded));

  zelda3::Overworld reloaded_ow(reloaded.get());
  ASSERT_OK(reloaded_ow.Load(reloaded.get()));

  // Should have the LATEST values
  EXPECT_EQ(reloaded_ow.overworld_map(0)->area_graphics(), gfx_v2)
      << "Latest overworld edit should persist";

  auto final_color = reloaded->ReadWord(palette_offset);
  ASSERT_TRUE(final_color.ok());
  EXPECT_EQ(*final_color, new_color)
      << "Palette edit should persist through subsequent saves";
}

// Test 9: Interleaved load/edit/save across regions
TEST_F(CrossEditorIntegrityTest, InterleavedOperations) {
  // Take snapshots of different ROM regions
  auto overworld_region = TakeSnapshot(*rom_, 0x7C9C, 128);   // Map properties
  auto dungeon_region = TakeSnapshot(*rom_, 0xF8000, 1400);   // Room headers
  auto palette_region = TakeSnapshot(*rom_, 0xDE6C8, 512);    // Palettes

  // Modify only overworld region
  zelda3::Overworld overworld(rom_.get());
  ASSERT_OK(overworld.Load(rom_.get()));
  
  overworld.mutable_overworld_map(0)->set_area_graphics(
      (overworld.overworld_map(0)->area_graphics() + 1) % 256);
  ASSERT_OK(overworld.SaveMapProperties());
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload
  std::unique_ptr<Rom> reloaded;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded));

  // Verify untouched regions are preserved
  // Note: Dungeon and palette regions should be unchanged
  EXPECT_TRUE(VerifyNoCorruption(*reloaded, dungeon_region, "Dungeon Headers"));
  EXPECT_TRUE(VerifyNoCorruption(*reloaded, palette_region, "Palettes"));
}

}  // namespace test
}  // namespace yaze
