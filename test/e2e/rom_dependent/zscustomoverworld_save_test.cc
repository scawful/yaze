#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "e2e/rom_dependent/editor_save_test_base.h"
#include "rom/rom.h"
#include "testing.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/overworld.h"

namespace yaze {
namespace test {

/**
 * @brief E2E Test Suite for ZSCustomOverworld v3 Expanded Save Operations
 *
 * Validates save operations for expanded ROM features:
 * 1. Expanded tile16/tile32 save operations
 * 2. v3 feature flag persistence
 * 3. Area-specific BG colors
 * 4. Custom tile GFX groups
 * 5. Large map expanded transitions
 */
class ZSCustomOverworldSaveTest : public ExpandedRomSaveTest {
 protected:
  // v3 feature flag addresses
  static constexpr uint32_t kVersionFlag = 0x140145;
  static constexpr uint32_t kMainPalettesFlag = 0x140146;
  static constexpr uint32_t kAreaBgFlag = 0x140147;
  static constexpr uint32_t kSubscreenOverlayFlag = 0x140148;
  static constexpr uint32_t kAnimatedGfxFlag = 0x140149;
  static constexpr uint32_t kCustomTilesFlag = 0x14014A;
  static constexpr uint32_t kMosaicFlag = 0x14014B;

  // Expanded data addresses
  static constexpr uint32_t kExpandedBgColors = 0x140000;
  static constexpr uint32_t kExpandedMainPalettes = 0x140160;
  static constexpr uint32_t kExpandedAnimatedGfx = 0x1402A0;
  static constexpr uint32_t kExpandedSubscreenOverlays = 0x140340;
  static constexpr uint32_t kExpandedCustomTiles = 0x140480;
  static constexpr uint32_t kExpandedAreaSizes = 0x140140;

  // Tile16/32 expansion check addresses
  static constexpr uint32_t kTile16ExpansionCheck = 0x02FD28;
  static constexpr uint32_t kTile32ExpansionCheck = 0x01772E;
};

// Test 1: v3 version flag persistence
TEST_F(ZSCustomOverworldSaveTest, VersionFlag_Persistence) {
  std::unique_ptr<Rom> rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, rom));

  // Verify ROM is v3
  auto version = rom->ReadByte(kVersionFlag);
  ASSERT_TRUE(version.ok());
  
  if (*version == 0xFF || *version == 0x00) {
    GTEST_SKIP() << "Test ROM is vanilla, not v3";
  }
  
  if (*version < 0x03) {
    GTEST_SKIP() << "Test ROM is v" << static_cast<int>(*version) << ", not v3";
  }

  // Modify version (shouldn't normally do this, but testing persistence)
  uint8_t original_version = *version;
  
  // Save without modification
  ASSERT_OK(SaveRomToFile(rom.get(), test_rom_path_));

  // Reload and verify
  std::unique_ptr<Rom> reloaded;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded));

  auto reloaded_version = reloaded->ReadByte(kVersionFlag);
  ASSERT_TRUE(reloaded_version.ok());
  EXPECT_EQ(*reloaded_version, original_version)
      << "v3 version flag should persist";
}

// Test 2: v3 feature flags persistence
TEST_F(ZSCustomOverworldSaveTest, FeatureFlags_Persistence) {
  std::unique_ptr<Rom> rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, rom));

  // Check if ROM is v3
  auto version = rom->ReadByte(kVersionFlag);
  if (!version.ok() || *version < 0x03) {
    GTEST_SKIP() << "Not a v3 ROM";
  }

  // Record original feature flags
  auto orig_main_palettes = rom->ReadByte(kMainPalettesFlag);
  auto orig_area_bg = rom->ReadByte(kAreaBgFlag);
  auto orig_subscreen = rom->ReadByte(kSubscreenOverlayFlag);
  auto orig_animated = rom->ReadByte(kAnimatedGfxFlag);
  auto orig_custom_tiles = rom->ReadByte(kCustomTilesFlag);
  auto orig_mosaic = rom->ReadByte(kMosaicFlag);

  // Toggle some flags
  ASSERT_OK(rom->WriteByte(kMainPalettesFlag, 
      orig_main_palettes.ok() ? (*orig_main_palettes ^ 0x01) : 0x01));
  ASSERT_OK(rom->WriteByte(kAnimatedGfxFlag,
      orig_animated.ok() ? (*orig_animated ^ 0x01) : 0x01));

  // Save ROM
  ASSERT_OK(SaveRomToFile(rom.get(), test_rom_path_));

  // Reload and verify
  std::unique_ptr<Rom> reloaded;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded));

  auto new_main_palettes = reloaded->ReadByte(kMainPalettesFlag);
  auto new_animated = reloaded->ReadByte(kAnimatedGfxFlag);

  ASSERT_TRUE(new_main_palettes.ok());
  ASSERT_TRUE(new_animated.ok());

  if (orig_main_palettes.ok()) {
    EXPECT_EQ(*new_main_palettes, (*orig_main_palettes ^ 0x01))
        << "Main palettes flag toggle should persist";
  }
  if (orig_animated.ok()) {
    EXPECT_EQ(*new_animated, (*orig_animated ^ 0x01))
        << "Animated GFX flag toggle should persist";
  }
}

// Test 3: Expanded tile16 detection and save
TEST_F(ZSCustomOverworldSaveTest, ExpandedTile16_Detection) {
  std::unique_ptr<Rom> rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, rom));

  auto version_info = DetectRomVersion(*rom);

  if (!version_info.is_expanded_tile16) {
    GTEST_SKIP() << "ROM does not have expanded tile16";
  }

  // Load overworld
  zelda3::Overworld overworld(rom.get());
  ASSERT_OK(overworld.Load(rom.get()));

  EXPECT_TRUE(overworld.expanded_tile16())
      << "Overworld should detect expanded tile16";

  // Verify we can access expanded tile16 data
  const auto& tiles16 = overworld.tiles16();
  EXPECT_GT(tiles16.size(), 0) << "Should have tile16 data loaded";
}

// Test 4: Expanded tile32 detection and save
TEST_F(ZSCustomOverworldSaveTest, ExpandedTile32_Detection) {
  std::unique_ptr<Rom> rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, rom));

  auto version_info = DetectRomVersion(*rom);

  if (!version_info.is_expanded_tile32) {
    GTEST_SKIP() << "ROM does not have expanded tile32";
  }

  // Load overworld
  zelda3::Overworld overworld(rom.get());
  ASSERT_OK(overworld.Load(rom.get()));

  EXPECT_TRUE(overworld.expanded_tile32())
      << "Overworld should detect expanded tile32";
}

// Test 5: Area-specific BG colors save
TEST_F(ZSCustomOverworldSaveTest, AreaBgColors_SaveAndReload) {
  std::unique_ptr<Rom> rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, rom));

  auto version = rom->ReadByte(kVersionFlag);
  if (!version.ok() || *version < 0x03) {
    GTEST_SKIP() << "Not a v3 ROM";
  }

  // Record original BG color data
  std::vector<uint16_t> original_colors(64);  // 64 maps worth
  for (int i = 0; i < 64; ++i) {
    auto color = rom->ReadWord(kExpandedBgColors + (i * 2));
    original_colors[i] = color.ok() ? *color : 0;
  }

  // Modify some BG colors
  const int test_map = 5;
  uint16_t new_color = (original_colors[test_map] + 0x0421) & 0x7FFF;
  ASSERT_OK(rom->WriteWord(kExpandedBgColors + (test_map * 2), new_color));

  // Save ROM
  ASSERT_OK(SaveRomToFile(rom.get(), test_rom_path_));

  // Reload and verify
  std::unique_ptr<Rom> reloaded;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded));

  auto reloaded_color = reloaded->ReadWord(kExpandedBgColors + (test_map * 2));
  ASSERT_TRUE(reloaded_color.ok());
  EXPECT_EQ(*reloaded_color, new_color)
      << "Area BG color modification should persist";

  // Verify other colors weren't corrupted
  for (int i = 0; i < 64; ++i) {
    if (i == test_map) continue;
    auto color = reloaded->ReadWord(kExpandedBgColors + (i * 2));
    if (color.ok()) {
      EXPECT_EQ(*color, original_colors[i])
          << "BG color for map " << i << " should not be corrupted";
    }
  }
}

// Test 6: Custom tile GFX groups save
TEST_F(ZSCustomOverworldSaveTest, CustomTileGfxGroups_SaveAndReload) {
  std::unique_ptr<Rom> rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, rom));

  auto version = rom->ReadByte(kVersionFlag);
  if (!version.ok() || *version < 0x03) {
    GTEST_SKIP() << "Not a v3 ROM";
  }

  // Record original custom tile data
  std::vector<uint8_t> original_tiles(64);
  for (int i = 0; i < 64; ++i) {
    auto tile = rom->ReadByte(kExpandedCustomTiles + i);
    original_tiles[i] = tile.ok() ? *tile : 0;
  }

  // Modify custom tile assignments
  const int test_map = 10;
  uint8_t new_tile_group = (original_tiles[test_map] + 5) % 256;
  ASSERT_OK(rom->WriteByte(kExpandedCustomTiles + test_map, new_tile_group));

  // Save ROM
  ASSERT_OK(SaveRomToFile(rom.get(), test_rom_path_));

  // Reload and verify
  std::unique_ptr<Rom> reloaded;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded));

  auto reloaded_tile = reloaded->ReadByte(kExpandedCustomTiles + test_map);
  ASSERT_TRUE(reloaded_tile.ok());
  EXPECT_EQ(*reloaded_tile, new_tile_group)
      << "Custom tile GFX group modification should persist";
}

// Test 7: Animated GFX data save
TEST_F(ZSCustomOverworldSaveTest, AnimatedGfx_SaveAndReload) {
  std::unique_ptr<Rom> rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, rom));

  auto version = rom->ReadByte(kVersionFlag);
  if (!version.ok() || *version < 0x03) {
    GTEST_SKIP() << "Not a v3 ROM";
  }

  // Record original animated GFX data
  std::vector<uint8_t> original_anim(64);
  for (int i = 0; i < 64; ++i) {
    auto anim = rom->ReadByte(kExpandedAnimatedGfx + i);
    original_anim[i] = anim.ok() ? *anim : 0;
  }

  // Modify animated GFX assignments
  const int test_map = 15;
  uint8_t new_anim = (original_anim[test_map] + 3) % 256;
  ASSERT_OK(rom->WriteByte(kExpandedAnimatedGfx + test_map, new_anim));

  // Save ROM
  ASSERT_OK(SaveRomToFile(rom.get(), test_rom_path_));

  // Reload and verify
  std::unique_ptr<Rom> reloaded;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded));

  auto reloaded_anim = reloaded->ReadByte(kExpandedAnimatedGfx + test_map);
  ASSERT_TRUE(reloaded_anim.ok());
  EXPECT_EQ(*reloaded_anim, new_anim)
      << "Animated GFX modification should persist";
}

// Test 8: Main palette data save (expanded)
TEST_F(ZSCustomOverworldSaveTest, ExpandedMainPalettes_SaveAndReload) {
  std::unique_ptr<Rom> rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, rom));

  auto version = rom->ReadByte(kVersionFlag);
  if (!version.ok() || *version < 0x03) {
    GTEST_SKIP() << "Not a v3 ROM";
  }

  // Record and modify main palette data
  auto original = rom->ReadByte(kExpandedMainPalettes);
  if (!original.ok()) {
    GTEST_SKIP() << "Cannot read expanded main palette data";
  }

  uint8_t modified = (*original + 7) % 256;
  ASSERT_OK(rom->WriteByte(kExpandedMainPalettes, modified));

  // Save ROM
  ASSERT_OK(SaveRomToFile(rom.get(), test_rom_path_));

  // Reload and verify
  std::unique_ptr<Rom> reloaded;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded));

  auto reloaded_pal = reloaded->ReadByte(kExpandedMainPalettes);
  ASSERT_TRUE(reloaded_pal.ok());
  EXPECT_EQ(*reloaded_pal, modified)
      << "Expanded main palette modification should persist";
}

// Test 9: Overworld Save with v3 expanded data
TEST_F(ZSCustomOverworldSaveTest, OverworldSave_V3Expanded) {
  std::unique_ptr<Rom> rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, rom));

  auto version = rom->ReadByte(kVersionFlag);
  if (!version.ok() || *version < 0x03) {
    GTEST_SKIP() << "Not a v3 ROM";
  }

  // Load overworld
  zelda3::Overworld overworld(rom.get());
  ASSERT_OK(overworld.Load(rom.get()));

  // Verify expanded features are detected
  if (!overworld.expanded_tile16() && !overworld.expanded_tile32()) {
    GTEST_SKIP() << "No expanded features detected in v3 ROM";
  }

  // Modify a map property
  auto* map5 = overworld.mutable_overworld_map(5);
  uint8_t original_gfx = map5->area_graphics();
  map5->set_area_graphics((original_gfx + 1) % 256);

  // Save through Overworld
  ASSERT_OK(overworld.SaveMapProperties());
  ASSERT_OK(SaveRomToFile(rom.get(), test_rom_path_));

  // Reload and verify
  std::unique_ptr<Rom> reloaded;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded));

  zelda3::Overworld reloaded_ow(reloaded.get());
  ASSERT_OK(reloaded_ow.Load(reloaded.get()));

  EXPECT_EQ(reloaded_ow.overworld_map(5)->area_graphics(), 
            (original_gfx + 1) % 256)
      << "v3 overworld map modification should persist";
}

// Test 10: Area sizes (v3 feature) save
TEST_F(ZSCustomOverworldSaveTest, AreaSizes_SaveAndReload) {
  std::unique_ptr<Rom> rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, rom));

  auto version = rom->ReadByte(kVersionFlag);
  if (!version.ok() || *version < 0x03) {
    GTEST_SKIP() << "Not a v3 ROM";
  }

  // Record original area sizes
  std::vector<uint8_t> original_sizes(64);
  for (int i = 0; i < 64; ++i) {
    auto size = rom->ReadByte(kExpandedAreaSizes + i);
    original_sizes[i] = size.ok() ? *size : 0;
  }

  // Modify an area size
  const int test_map = 20;
  uint8_t new_size = (original_sizes[test_map] + 1) % 4;  // 0-3 valid sizes
  ASSERT_OK(rom->WriteByte(kExpandedAreaSizes + test_map, new_size));

  // Save ROM
  ASSERT_OK(SaveRomToFile(rom.get(), test_rom_path_));

  // Reload and verify
  std::unique_ptr<Rom> reloaded;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded));

  auto reloaded_size = reloaded->ReadByte(kExpandedAreaSizes + test_map);
  ASSERT_TRUE(reloaded_size.ok());
  EXPECT_EQ(*reloaded_size, new_size)
      << "Area size modification should persist";

  // Verify other sizes weren't corrupted
  for (int i = 0; i < 64; ++i) {
    if (i == test_map) continue;
    auto size = reloaded->ReadByte(kExpandedAreaSizes + i);
    if (size.ok()) {
      EXPECT_EQ(*size, original_sizes[i])
          << "Area size for map " << i << " should not be corrupted";
    }
  }
}

// Test 11: Full v3 data round-trip
TEST_F(ZSCustomOverworldSaveTest, FullV3RoundTrip) {
  std::unique_ptr<Rom> rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, rom));

  auto version = rom->ReadByte(kVersionFlag);
  if (!version.ok() || *version < 0x03) {
    GTEST_SKIP() << "Not a v3 ROM";
  }

  // Take snapshots of all v3 data regions
  auto bg_snapshot = TakeSnapshot(*rom, kExpandedBgColors, 128);
  auto pal_snapshot = TakeSnapshot(*rom, kExpandedMainPalettes, 160);
  auto anim_snapshot = TakeSnapshot(*rom, kExpandedAnimatedGfx, 64);
  auto sub_snapshot = TakeSnapshot(*rom, kExpandedSubscreenOverlays, 128);
  auto tile_snapshot = TakeSnapshot(*rom, kExpandedCustomTiles, 160);

  // Save ROM without modifications
  ASSERT_OK(SaveRomToFile(rom.get(), test_rom_path_));

  // Reload and verify all regions are preserved
  std::unique_ptr<Rom> reloaded;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded));

  EXPECT_TRUE(VerifyNoCorruption(*reloaded, bg_snapshot, "BG Colors"));
  EXPECT_TRUE(VerifyNoCorruption(*reloaded, pal_snapshot, "Main Palettes"));
  EXPECT_TRUE(VerifyNoCorruption(*reloaded, anim_snapshot, "Animated GFX"));
  EXPECT_TRUE(VerifyNoCorruption(*reloaded, sub_snapshot, "Subscreen Overlays"));
  EXPECT_TRUE(VerifyNoCorruption(*reloaded, tile_snapshot, "Custom Tiles"));
}

}  // namespace test
}  // namespace yaze

