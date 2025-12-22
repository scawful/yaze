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

namespace yaze {
namespace test {

/**
 * @brief E2E Test Suite for Tile16Editor Save Operations
 *
 * Validates the complete tile16 editing workflow:
 * 1. Load ROM and tile16 data
 * 2. Modify tile16 compositions
 * 3. Save changes to ROM
 * 4. Reload ROM and verify edits persisted
 * 5. Verify no data corruption occurred
 */
class Tile16EditorSaveTest : public EditorSaveTestBase {
 protected:
  void SetUp() override {
    EditorSaveTestBase::SetUp();

    // Load the test ROM
    rom_ = std::make_unique<Rom>();
    auto load_result = rom_->LoadFromFile(test_rom_path_);
    if (!load_result.ok()) {
      GTEST_SKIP() << "Failed to load test ROM: " << load_result.message();
    }

    // Load overworld data (which includes tile16 data)
    overworld_ = std::make_unique<zelda3::Overworld>(rom_.get());
    auto ow_load = overworld_->Load(rom_.get());
    if (!ow_load.ok()) {
      GTEST_SKIP() << "Failed to load overworld: " << ow_load.message();
    }
  }

  // Helper to read tile16 data from ROM (4 tile8 entries = 8 bytes per tile16)
  std::vector<uint16_t> ReadTile16FromRom(Rom& rom, int tile16_id) {
    std::vector<uint16_t> tiles(4);
    int addr = zelda3::kMap16Tiles + (tile16_id * 8);

    for (int i = 0; i < 4; ++i) {
      auto word = rom.ReadWord(addr + (i * 2));
      tiles[i] = word.ok() ? *word : 0;
    }
    return tiles;
  }

  // Helper to write tile16 data to ROM
  absl::Status WriteTile16ToRom(Rom& rom, int tile16_id,
                                const std::vector<uint16_t>& tiles) {
    if (tiles.size() != 4) {
      return absl::InvalidArgumentError("Tile16 requires exactly 4 tile8 entries");
    }

    int addr = zelda3::kMap16Tiles + (tile16_id * 8);
    for (int i = 0; i < 4; ++i) {
      RETURN_IF_ERROR(rom.WriteWord(addr + (i * 2), tiles[i]));
    }
    return absl::OkStatus();
  }

  std::unique_ptr<Rom> rom_;
  std::unique_ptr<zelda3::Overworld> overworld_;
};

// Test 1: Single tile16 edit, save, and reload verification
TEST_F(Tile16EditorSaveTest, SingleTile16Edit_SaveAndReload) {
  // Record original tile16 data for tile 0
  const int test_tile_id = 0;
  std::vector<uint16_t> original_tiles = ReadTile16FromRom(*rom_, test_tile_id);

  // Modify the tile16 (change first tile8 entry)
  std::vector<uint16_t> modified_tiles = original_tiles;
  modified_tiles[0] = (original_tiles[0] + 1) % 0x400;  // Cycle tile index

  // Write modification to ROM
  ASSERT_OK(WriteTile16ToRom(*rom_, test_tile_id, modified_tiles));

  // Save ROM to disk
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload ROM
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  // Verify modification persisted
  std::vector<uint16_t> reloaded_tiles = ReadTile16FromRom(*reloaded_rom, test_tile_id);
  EXPECT_EQ(reloaded_tiles[0], modified_tiles[0])
      << "Tile16 modification should persist after save/reload";
  EXPECT_EQ(reloaded_tiles[1], modified_tiles[1]);
  EXPECT_EQ(reloaded_tiles[2], modified_tiles[2]);
  EXPECT_EQ(reloaded_tiles[3], modified_tiles[3]);
}

// Test 2: Multiple tile16 edits save atomically
TEST_F(Tile16EditorSaveTest, MultipleTile16Edits_Atomicity) {
  // Test editing multiple tile16 entries
  const std::vector<int> test_tile_ids = {10, 50, 100, 200};
  std::map<int, std::vector<uint16_t>> original_data;
  std::map<int, std::vector<uint16_t>> modified_data;

  // Record original data and prepare modifications
  for (int tile_id : test_tile_ids) {
    original_data[tile_id] = ReadTile16FromRom(*rom_, tile_id);
    modified_data[tile_id] = original_data[tile_id];
    // Modify each tile differently
    modified_data[tile_id][0] = (original_data[tile_id][0] + tile_id) % 0x400;
  }

  // Apply all modifications
  for (int tile_id : test_tile_ids) {
    ASSERT_OK(WriteTile16ToRom(*rom_, tile_id, modified_data[tile_id]));
  }

  // Save ROM
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload and verify ALL changes persisted
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  for (int tile_id : test_tile_ids) {
    std::vector<uint16_t> reloaded_tiles = ReadTile16FromRom(*reloaded_rom, tile_id);
    EXPECT_EQ(reloaded_tiles[0], modified_data[tile_id][0])
        << "Tile16 " << tile_id << " modification should persist";
  }
}

// Test 3: Verify adjacent tile16 entries are not corrupted
TEST_F(Tile16EditorSaveTest, NoAdjacentCorruption) {
  // Test that modifying tile16 #50 doesn't corrupt #49 or #51
  const int target_tile = 50;
  const int prev_tile = 49;
  const int next_tile = 51;

  // Record adjacent tile data
  std::vector<uint16_t> prev_original = ReadTile16FromRom(*rom_, prev_tile);
  std::vector<uint16_t> next_original = ReadTile16FromRom(*rom_, next_tile);

  // Modify target tile
  std::vector<uint16_t> target_tiles = ReadTile16FromRom(*rom_, target_tile);
  target_tiles[0] = 0x1234;  // Arbitrary modification
  target_tiles[1] = 0x5678;
  ASSERT_OK(WriteTile16ToRom(*rom_, target_tile, target_tiles));

  // Save ROM
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload and verify
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  // Verify adjacent tiles were NOT corrupted
  std::vector<uint16_t> prev_reloaded = ReadTile16FromRom(*reloaded_rom, prev_tile);
  std::vector<uint16_t> next_reloaded = ReadTile16FromRom(*reloaded_rom, next_tile);

  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(prev_reloaded[i], prev_original[i])
        << "Tile16 " << prev_tile << " should not be corrupted";
    EXPECT_EQ(next_reloaded[i], next_original[i])
        << "Tile16 " << next_tile << " should not be corrupted";
  }

  // Verify target tile has the modification
  std::vector<uint16_t> target_reloaded = ReadTile16FromRom(*reloaded_rom, target_tile);
  EXPECT_EQ(target_reloaded[0], 0x1234);
  EXPECT_EQ(target_reloaded[1], 0x5678);
}

// Test 4: Round-trip without modification preserves data
TEST_F(Tile16EditorSaveTest, RoundTrip_NoModification) {
  // Record sample tile16 data
  const std::vector<int> sample_tiles = {0, 25, 50, 75, 100, 150, 200, 250};
  std::map<int, std::vector<uint16_t>> original_data;

  for (int tile_id : sample_tiles) {
    original_data[tile_id] = ReadTile16FromRom(*rom_, tile_id);
  }

  // Save ROM without any modifications
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload and verify data is preserved
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  for (int tile_id : sample_tiles) {
    std::vector<uint16_t> reloaded = ReadTile16FromRom(*reloaded_rom, tile_id);
    for (int i = 0; i < 4; ++i) {
      EXPECT_EQ(reloaded[i], original_data[tile_id][i])
          << "Tile16 " << tile_id << " entry " << i << " should be preserved";
    }
  }
}

// Test 5: Tile16 flip attributes persistence
TEST_F(Tile16EditorSaveTest, FlipAttributes_Persistence) {
  const int test_tile_id = 100;
  std::vector<uint16_t> tiles = ReadTile16FromRom(*rom_, test_tile_id);

  // Modify with flip flags (bits 14-15 in SNES tile format)
  // Bit 14 = horizontal flip, Bit 15 = vertical flip
  tiles[0] = (tiles[0] & 0x03FF) | 0x4000;  // Set H-flip
  tiles[1] = (tiles[1] & 0x03FF) | 0x8000;  // Set V-flip
  tiles[2] = (tiles[2] & 0x03FF) | 0xC000;  // Set both flips
  tiles[3] = (tiles[3] & 0x03FF);           // No flips

  ASSERT_OK(WriteTile16ToRom(*rom_, test_tile_id, tiles));
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload and verify flip attributes
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  std::vector<uint16_t> reloaded = ReadTile16FromRom(*reloaded_rom, test_tile_id);
  EXPECT_EQ(reloaded[0] & 0xC000, 0x4000) << "H-flip should persist";
  EXPECT_EQ(reloaded[1] & 0xC000, 0x8000) << "V-flip should persist";
  EXPECT_EQ(reloaded[2] & 0xC000, 0xC000) << "Both flips should persist";
  EXPECT_EQ(reloaded[3] & 0xC000, 0x0000) << "No flips should persist";
}

// Test 6: Palette attribute persistence
TEST_F(Tile16EditorSaveTest, PaletteAttributes_Persistence) {
  const int test_tile_id = 150;
  std::vector<uint16_t> tiles = ReadTile16FromRom(*rom_, test_tile_id);

  // Modify palette bits (bits 10-12 in SNES tile format)
  tiles[0] = (tiles[0] & 0xE3FF) | (0 << 10);   // Palette 0
  tiles[1] = (tiles[1] & 0xE3FF) | (3 << 10);   // Palette 3
  tiles[2] = (tiles[2] & 0xE3FF) | (5 << 10);   // Palette 5
  tiles[3] = (tiles[3] & 0xE3FF) | (7 << 10);   // Palette 7

  ASSERT_OK(WriteTile16ToRom(*rom_, test_tile_id, tiles));
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload and verify palette attributes
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  std::vector<uint16_t> reloaded = ReadTile16FromRom(*reloaded_rom, test_tile_id);
  EXPECT_EQ((reloaded[0] >> 10) & 0x07, 0) << "Palette 0 should persist";
  EXPECT_EQ((reloaded[1] >> 10) & 0x07, 3) << "Palette 3 should persist";
  EXPECT_EQ((reloaded[2] >> 10) & 0x07, 5) << "Palette 5 should persist";
  EXPECT_EQ((reloaded[3] >> 10) & 0x07, 7) << "Palette 7 should persist";
}

// Test 7: Large batch tile16 modifications
TEST_F(Tile16EditorSaveTest, LargeBatch_Modifications) {
  // Test modifying a large number of tile16 entries
  const int batch_size = 100;
  std::map<int, std::vector<uint16_t>> original_data;
  std::map<int, std::vector<uint16_t>> modified_data;

  // Prepare batch modifications
  for (int i = 0; i < batch_size; ++i) {
    int tile_id = i * 3;  // Spread across tile16 space
    original_data[tile_id] = ReadTile16FromRom(*rom_, tile_id);
    modified_data[tile_id] = original_data[tile_id];
    // Apply unique modification pattern
    modified_data[tile_id][0] = static_cast<uint16_t>((i * 7) % 0x400);
  }

  // Apply all modifications
  for (const auto& [tile_id, tiles] : modified_data) {
    ASSERT_OK(WriteTile16ToRom(*rom_, tile_id, tiles));
  }

  // Save ROM
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload and verify all changes
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  int verified_count = 0;
  for (const auto& [tile_id, expected_tiles] : modified_data) {
    std::vector<uint16_t> reloaded = ReadTile16FromRom(*reloaded_rom, tile_id);
    if (reloaded[0] == expected_tiles[0]) {
      verified_count++;
    }
  }

  EXPECT_EQ(verified_count, batch_size)
      << "All batch modifications should persist";
}

// Test 8: Overworld integration - SaveMap16Tiles via Overworld class
TEST_F(Tile16EditorSaveTest, OverworldIntegration_SaveMap16Tiles) {
  // Modify tiles16_ directly through Overworld class
  auto* tiles16_ptr = overworld_->mutable_tiles16();
  
  if (tiles16_ptr == nullptr || tiles16_ptr->empty()) {
    GTEST_SKIP() << "No tile16 data loaded";
  }

  // Record original first tile
  auto original_tile0 = (*tiles16_ptr)[0];

  // Modify the first tile16
  gfx::Tile16 modified_tile = original_tile0;
  modified_tile.tile0_.id_ = (original_tile0.tile0_.id_ + 1) % 0x200;

  (*tiles16_ptr)[0] = modified_tile;

  // Save via Overworld's SaveMap16Tiles
  ASSERT_OK(overworld_->SaveMap16Tiles());

  // Save ROM to disk
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload and verify through Overworld class
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  zelda3::Overworld reloaded_ow(reloaded_rom.get());
  ASSERT_OK(reloaded_ow.Load(reloaded_rom.get()));

  const auto reloaded_tiles16 = reloaded_ow.tiles16();
  ASSERT_FALSE(reloaded_tiles16.empty());

  EXPECT_EQ(reloaded_tiles16[0].tile0_.id_, modified_tile.tile0_.id_)
      << "Tile16 modification via Overworld should persist";
}

}  // namespace test
}  // namespace yaze
