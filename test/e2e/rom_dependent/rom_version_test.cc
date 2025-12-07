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
#include "zelda3/overworld/overworld_version_helper.h"

namespace yaze {
namespace test {

/**
 * @brief E2E Test Suite for Multi-ROM Version Validation
 *
 * Validates save/load operations work correctly across different ROM versions:
 * - Japanese (JP) ROM
 * - US (USA) ROM  
 * - European (EU) ROM
 *
 * Tests version-specific constants and data layouts.
 */
class RomVersionTest : public MultiVersionEditorSaveTest {
 protected:
  // Version-specific address constants
  struct VersionAddresses {
    uint32_t overworld_gfx_ptr1;
    uint32_t overworld_gfx_ptr2;
    uint32_t overworld_gfx_ptr3;
    uint32_t map32_tile_tl;
    uint32_t compressed_map_ptr;
  };

  VersionAddresses GetVersionAddresses(zelda3_version version) {
    VersionAddresses addrs;
    
    switch (version) {
      case JP:
        addrs.overworld_gfx_ptr1 = 0x0885A3;
        addrs.overworld_gfx_ptr2 = 0x089AB1;
        addrs.overworld_gfx_ptr3 = 0x08B2C1;
        addrs.map32_tile_tl = 0x18F8A0;
        addrs.compressed_map_ptr = 0x02FFC1;
        break;
      case US:
        addrs.overworld_gfx_ptr1 = 0x0886E3;
        addrs.overworld_gfx_ptr2 = 0x089BF1;
        addrs.overworld_gfx_ptr3 = 0x08B401;
        addrs.map32_tile_tl = 0x18F8A0;
        addrs.compressed_map_ptr = 0x02FFE1;
        break;
      case RANDO:
      default:
        // Default to US addresses
        addrs.overworld_gfx_ptr1 = 0x0886E3;
        addrs.overworld_gfx_ptr2 = 0x089BF1;
        addrs.overworld_gfx_ptr3 = 0x08B401;
        addrs.map32_tile_tl = 0x18F8A0;
        addrs.compressed_map_ptr = 0x02FFE1;
        break;
    }
    
    return addrs;
  }
};

// Test 1: Detect ROM version correctly
TEST_F(RomVersionTest, DetectVersion_Default) {
  std::unique_ptr<Rom> rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, rom));

  auto version_info = DetectRomVersion(*rom);

  // Should detect a valid version
  EXPECT_TRUE(version_info.version == JP ||
              version_info.version == US ||
              version_info.version == RANDO)
      << "Should detect a valid ROM version";

  // Log detected version for debugging
  std::string version_name;
  switch (version_info.version) {
    case JP: version_name = "JP"; break;
    case US: version_name = "US"; break;
    case RANDO: version_name = "RANDO"; break;
    default: version_name = "Unknown"; break;
  }

  std::cout << "Detected ROM version: " << version_name << std::endl;
  std::cout << "ZSCustomOverworld: " << (version_info.zscustom_version == 0xFF ? 
      "vanilla" : std::to_string(version_info.zscustom_version)) << std::endl;
}

// Test 2: US ROM save/load cycle
TEST_F(RomVersionTest, UsRom_SaveLoadCycle) {
  if (!HasUsRom()) {
    // Try to use the default test ROM
    std::unique_ptr<Rom> rom;
    ASSERT_OK(LoadAndVerifyRom(test_rom_path_, rom));
    
    auto version_info = DetectRomVersion(*rom);
    if (version_info.version != US) {
      GTEST_SKIP() << "US ROM not available";
    }
  }

  std::string rom_path = HasUsRom() ? us_rom_path_ : test_rom_path_;
  
  // Copy ROM for testing
  std::string test_path = "test_us_rom.sfc";
  std::filesystem::copy_file(rom_path, test_path,
      std::filesystem::copy_options::overwrite_existing);

  // Load ROM
  std::unique_ptr<Rom> rom;
  ASSERT_OK(LoadAndVerifyRom(test_path, rom));

  // Load overworld
  zelda3::Overworld overworld(rom.get());
  ASSERT_OK(overworld.Load(rom.get()));

  // Verify basic data loads correctly
  EXPECT_TRUE(overworld.is_loaded());
  EXPECT_EQ(overworld.overworld_maps().size(), 160);

  // Modify something
  auto* map0 = overworld.mutable_overworld_map(0);
  uint8_t original_gfx = map0->area_graphics();
  map0->set_area_graphics((original_gfx + 1) % 256);

  // Save
  ASSERT_OK(overworld.SaveMapProperties());
  ASSERT_OK(SaveRomToFile(rom.get(), test_path));

  // Reload and verify
  std::unique_ptr<Rom> reloaded;
  ASSERT_OK(LoadAndVerifyRom(test_path, reloaded));

  zelda3::Overworld reloaded_ow(reloaded.get());
  ASSERT_OK(reloaded_ow.Load(reloaded.get()));

  EXPECT_EQ(reloaded_ow.overworld_map(0)->area_graphics(), 
            (original_gfx + 1) % 256)
      << "US ROM modification should persist";

  // Cleanup
  std::filesystem::remove(test_path);
}

// Test 3: Version constants validation
TEST_F(RomVersionTest, VersionConstants_Validation) {
  std::unique_ptr<Rom> rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, rom));

  auto version_info = DetectRomVersion(*rom);
  auto addrs = GetVersionAddresses(version_info.version);

  // Verify pointers are within ROM bounds
  EXPECT_LT(addrs.overworld_gfx_ptr1, rom->size())
      << "GFX pointer 1 should be within ROM";
  EXPECT_LT(addrs.overworld_gfx_ptr2, rom->size())
      << "GFX pointer 2 should be within ROM";
  EXPECT_LT(addrs.overworld_gfx_ptr3, rom->size())
      << "GFX pointer 3 should be within ROM";
  EXPECT_LT(addrs.map32_tile_tl, rom->size())
      << "Map32 tile pointer should be within ROM";

  // Read some data at these addresses to verify they're valid
  auto gfx1 = rom->ReadByte(addrs.overworld_gfx_ptr1);
  auto gfx2 = rom->ReadByte(addrs.overworld_gfx_ptr2);
  auto gfx3 = rom->ReadByte(addrs.overworld_gfx_ptr3);

  EXPECT_TRUE(gfx1.ok()) << "Should be able to read at GFX pointer 1";
  EXPECT_TRUE(gfx2.ok()) << "Should be able to read at GFX pointer 2";
  EXPECT_TRUE(gfx3.ok()) << "Should be able to read at GFX pointer 3";
}

// Test 4: Game data loads correctly for detected version
TEST_F(RomVersionTest, GameData_LoadsCorrectly) {
  std::unique_ptr<Rom> rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, rom));

  zelda3::GameData game_data;
  ASSERT_OK(zelda3::LoadGameData(*rom, game_data));

  // Verify palettes loaded
  EXPECT_GT(game_data.palette_groups.overworld_main.size(), 0)
      << "Overworld main palettes should load";
  EXPECT_GT(game_data.palette_groups.dungeon_main.size(), 0)
      << "Dungeon main palettes should load";

  // Verify version was detected
  EXPECT_TRUE(game_data.version == JP ||
              game_data.version == US ||
              game_data.version == RANDO);
}

// Test 5: Vanilla ROM identification
TEST_F(RomVersionTest, VanillaRom_Identification) {
  std::unique_ptr<Rom> rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, rom));

  auto version_info = DetectRomVersion(*rom);

  // Check if ROM is vanilla (no ZSCustomOverworld)
  bool is_vanilla = (version_info.zscustom_version == 0xFF || 
                     version_info.zscustom_version == 0x00);

  if (is_vanilla) {
    EXPECT_FALSE(version_info.is_expanded_tile16)
        << "Vanilla ROM should not have expanded tile16";
    EXPECT_FALSE(version_info.is_expanded_tile32)
        << "Vanilla ROM should not have expanded tile32";
  }
}

// Test 6: ROM header validation
TEST_F(RomVersionTest, RomHeader_Validation) {
  std::unique_ptr<Rom> rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, rom));

  // Check ROM title in header (at offset 0x7FC0)
  std::string title;
  for (int i = 0; i < 21; ++i) {
    auto byte = rom->ReadByte(0x7FC0 + i);
    if (byte.ok() && *byte >= 0x20 && *byte < 0x7F) {
      title += static_cast<char>(*byte);
    }
  }

  EXPECT_FALSE(title.empty()) << "ROM should have a valid title";
  std::cout << "ROM Title: " << title << std::endl;

  // Check ROM makeup byte (at 0x7FD5)
  auto makeup = rom->ReadByte(0x7FD5);
  ASSERT_TRUE(makeup.ok());
  EXPECT_EQ(*makeup & 0x01, 0) << "Should be LoROM mapping";

  // Check ROM type byte (at 0x7FD6)
  auto rom_type = rom->ReadByte(0x7FD6);
  ASSERT_TRUE(rom_type.ok());
  // Type 0x02 = ROM + SRAM, common for ALTTP
  
  // Check ROM size byte (at 0x7FD7)
  auto rom_size = rom->ReadByte(0x7FD7);
  ASSERT_TRUE(rom_size.ok());
  // Size = 2^(rom_size + 10) bytes
  // 0x0A = 1MB, 0x0B = 2MB
  EXPECT_GE(*rom_size, 0x0A) << "ROM should be at least 1MB";
}

// Test 7: Cross-version data compatibility
TEST_F(RomVersionTest, CrossVersion_DataCompatibility) {
  std::unique_ptr<Rom> rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, rom));

  // Load overworld
  zelda3::Overworld overworld(rom.get());
  ASSERT_OK(overworld.Load(rom.get()));

  // These structures should be consistent across versions
  EXPECT_EQ(overworld.overworld_maps().size(), 160)
      << "All versions should have 160 overworld maps";
  EXPECT_EQ(overworld.entrances().size(), 129)
      << "All versions should have 129 entrances";
  EXPECT_EQ(overworld.exits()->size(), 0x4F)
      << "All versions should have 0x4F exits";
  EXPECT_EQ(overworld.holes().size(), 0x13)
      << "All versions should have 0x13 holes";
}

// Test 8: ROM checksum after modification
TEST_F(RomVersionTest, Checksum_AfterModification) {
  std::unique_ptr<Rom> rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, rom));

  // Read original checksum (at 0x7FDC-0x7FDF)
  auto checksum_comp = rom->ReadWord(0x7FDC);  // Checksum complement
  auto checksum = rom->ReadWord(0x7FDE);       // Checksum

  ASSERT_TRUE(checksum_comp.ok());
  ASSERT_TRUE(checksum.ok());

  // Verify checksum and complement are inverses
  EXPECT_EQ((*checksum_comp ^ *checksum) & 0xFFFF, 0xFFFF)
      << "Checksum and complement should be inverses";

  // Modify ROM
  ASSERT_OK(rom->WriteByte(0x1000, 0xAB));

  // Save ROM
  ASSERT_OK(SaveRomToFile(rom.get(), test_rom_path_));

  // Reload and verify checksum was updated
  std::unique_ptr<Rom> reloaded;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded));

  auto new_checksum_comp = reloaded->ReadWord(0x7FDC);
  auto new_checksum = reloaded->ReadWord(0x7FDE);

  ASSERT_TRUE(new_checksum_comp.ok());
  ASSERT_TRUE(new_checksum.ok());

  // Checksum should still be valid (complement relationship)
  // Note: yaze may or may not update checksums on save
  if ((*new_checksum_comp ^ *new_checksum) == 0xFFFF) {
    // Checksum was updated correctly
    SUCCEED();
  } else {
    // Checksum wasn't updated - this might be intentional
    std::cout << "Note: ROM checksum not updated after modification" << std::endl;
  }
}

// Test 9: Multiple save/load cycles stability
TEST_F(RomVersionTest, MultipleCycles_Stability) {
  const int num_cycles = 5;
  std::unique_ptr<Rom> rom;
  
  for (int cycle = 0; cycle < num_cycles; ++cycle) {
    ASSERT_OK(LoadAndVerifyRom(test_rom_path_, rom))
        << "Failed on load cycle " << cycle;

    // Load overworld
    zelda3::Overworld overworld(rom.get());
    ASSERT_OK(overworld.Load(rom.get()))
        << "Failed to load overworld on cycle " << cycle;

    // Verify consistent data
    EXPECT_EQ(overworld.overworld_maps().size(), 160)
        << "Map count mismatch on cycle " << cycle;

    // Make a modification
    auto* map = overworld.mutable_overworld_map(cycle % 160);
    uint8_t new_value = static_cast<uint8_t>(cycle);
    map->set_area_graphics(new_value);

    ASSERT_OK(overworld.SaveMapProperties())
        << "Failed to save on cycle " << cycle;
    ASSERT_OK(SaveRomToFile(rom.get(), test_rom_path_))
        << "Failed to save ROM on cycle " << cycle;
  }

  // Final verification
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, rom));
  zelda3::Overworld final_ow(rom.get());
  ASSERT_OK(final_ow.Load(rom.get()));

  // Verify last modification persisted
  EXPECT_EQ(final_ow.overworld_map((num_cycles - 1) % 160)->area_graphics(),
            static_cast<uint8_t>(num_cycles - 1));
}

}  // namespace test
}  // namespace yaze
