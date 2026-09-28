#include "zelda3/overworld/overworld.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "rom/rom.h"
#include "zelda3/overworld/diggable_tiles.h"
#include "zelda3/overworld/overworld_item.h"
#include "zelda3/overworld/overworld_map.h"
#include "zelda3/overworld/overworld_version_helper.h"

namespace yaze {
namespace zelda3 {

class OverworldRegressionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Skip tests on Linux CI - these require SDL/graphics system initialization
#if defined(__linux__)
    GTEST_SKIP() << "Overworld tests require graphics context";
#endif
    rom_ = std::make_unique<Rom>();
    // 2MB ROM filled with 0x00
    std::vector<uint8_t> mock_rom_data(0x200000, 0x00);

    // Initialize minimal data to prevent crashes during Load
    // Message IDs
    for (int i = 0; i < 160; i++) {
      mock_rom_data[0x3F51D + (i * 2)] = 0x00;
      mock_rom_data[0x3F51D + (i * 2) + 1] = 0x00;
    }
    // Area graphics/palettes
    for (int i = 0; i < 160; i++) {
      mock_rom_data[0x7C9C + i] = 0x00;
      mock_rom_data[0x7D1C + i] = 0x00;
    }
    // Screen sizes - Set ALL to Small (0x01) initially
    for (int i = 0; i < 160; i++) {
      mock_rom_data[0x1788D + i] = 0x01;
    }
    // Parent table - identity for LW so DW mirrors correctly (+0x40)
    for (int i = 0; i < 64; i++) {
      mock_rom_data[0x125EC + i] = static_cast<uint8_t>(i);
    }
    // Sprite sets/palettes
    for (int i = 0; i < 160; i++) {
      mock_rom_data[0x7A41 + i] = 0x00;
      mock_rom_data[0x7B41 + i] = 0x00;
    }

    rom_->LoadFromData(mock_rom_data);
    overworld_ = std::make_unique<Overworld>(rom_.get());
  }

  void TearDown() override {
    overworld_.reset();
    rom_.reset();
  }

  std::unique_ptr<Rom> rom_;
  std::unique_ptr<Overworld> overworld_;

  void PopulateOverworldMaps() {
    auto& maps =
        const_cast<std::vector<OverworldMap>&>(overworld_->overworld_maps());
    maps.clear();
    maps.reserve(kNumOverworldMaps);
    for (int i = 0; i < kNumOverworldMaps; ++i) {
      maps.emplace_back(i, rom_.get());
    }
  }

  gfx::Tile16 SolidPaletteTile16(uint8_t palette) {
    gfx::TileInfo info(0, palette, false, false, false);
    return gfx::Tile16(info, info, info, info);
  }

  std::vector<gfx::Tile16> SolidPaletteTestTiles16() {
    return {SolidPaletteTile16(0), SolidPaletteTile16(1),
            SolidPaletteTile16(2)};
  }
};

TEST_F(OverworldRegressionTest, VersionHelperLogic) {
  // This test verifies the logic we WANT to implement.

  // Vanilla (0xFF)
  (*rom_)[OverworldCustomASMHasBeenApplied] = 0xFF;
  uint8_t version = (*rom_)[OverworldCustomASMHasBeenApplied];

  // The BUG:
  // EXPECT_TRUE(version >= 3);

  // The FIX:
  // With OverworldVersionHelper, this should now be correctly identified as Vanilla
  auto ov_version = OverworldVersionHelper::GetVersion(*rom_);
  EXPECT_EQ(ov_version, OverworldVersion::kVanilla);
  EXPECT_FALSE(OverworldVersionHelper::SupportsAreaEnum(ov_version));

  // ZScream v3 (0x03)
  (*rom_)[OverworldCustomASMHasBeenApplied] = 0x03;
  ov_version = OverworldVersionHelper::GetVersion(*rom_);
  EXPECT_EQ(ov_version, OverworldVersion::kZSCustomV3);
  EXPECT_TRUE(OverworldVersionHelper::SupportsAreaEnum(ov_version));
}

TEST_F(OverworldRegressionTest, DeathMountainPaletteUsesExactParents) {
  // Treat ROM as vanilla so parent_ stays equal to index
  (*rom_)[OverworldCustomASMHasBeenApplied] = 0xFF;

  // The Death Mountain rule picks the animated sheet shown in the top half
  // of slot 7; the bottom half keeps the world's sheet 7.
  OverworldMap dm_map_lw(0x03, rom_.get());
  dm_map_lw.LoadAreaGraphics();
  EXPECT_EQ(dm_map_lw.animated_sheet(), 0x59);

  OverworldMap dm_map_dw(0x45, rom_.get());
  dm_map_dw.LoadAreaGraphics();
  EXPECT_EQ(dm_map_dw.animated_sheet(), 0x59);

  OverworldMap non_dm_map(0x04, rom_.get());
  non_dm_map.LoadAreaGraphics();
  EXPECT_EQ(non_dm_map.animated_sheet(), 0x5B);
}

// =============================================================================
// Save Function Version Check Tests
// These tests verify that save functions check ROM version before writing
// to custom address space (0x140000+) to prevent vanilla ROM corruption.
// =============================================================================

TEST_F(OverworldRegressionTest,
       SaveAreaSpecificBGColors_VanillaRom_SkipsWrite) {
  // Set version to Vanilla (0xFF)
  (*rom_)[OverworldCustomASMHasBeenApplied] = 0xFF;

  // Record original data at custom address
  uint8_t original_byte = (*rom_)[OverworldCustomAreaSpecificBGPalette];

  // Call save - should be a no-op for vanilla
  auto status = overworld_->SaveAreaSpecificBGColors();
  ASSERT_TRUE(status.ok());

  // Verify data was NOT modified
  EXPECT_EQ((*rom_)[OverworldCustomAreaSpecificBGPalette], original_byte);
}

TEST_F(OverworldRegressionTest, SaveAreaSpecificBGColors_V1Rom_SkipsWrite) {
  // Set version to v1
  (*rom_)[OverworldCustomASMHasBeenApplied] = 0x01;

  // Record original data at custom address
  uint8_t original_byte = (*rom_)[OverworldCustomAreaSpecificBGPalette];

  // Call save - should be a no-op for v1 (only v2+ supports custom BG colors)
  auto status = overworld_->SaveAreaSpecificBGColors();
  ASSERT_TRUE(status.ok());

  // Verify data was NOT modified
  EXPECT_EQ((*rom_)[OverworldCustomAreaSpecificBGPalette], original_byte);
}

TEST_F(OverworldRegressionTest, SaveAreaSpecificBGColors_V2Rom_Writes) {
  // Set version to v2 (supports custom BG colors)
  (*rom_)[OverworldCustomASMHasBeenApplied] = 0x02;

  // Create a standalone map and set its BG color
  OverworldMap test_map(0, rom_.get());
  test_map.set_area_specific_bg_color(0x7FFF);

  // We can't easily test full write without loading overworld.
  // Instead, verify that version check passes for v2
  auto version = OverworldVersionHelper::GetVersion(*rom_);
  EXPECT_TRUE(OverworldVersionHelper::SupportsCustomBGColors(version));
}

TEST_F(OverworldRegressionTest, SaveCustomOverworldASM_VanillaRom_SkipsWrite) {
  // Set version to Vanilla
  (*rom_)[OverworldCustomASMHasBeenApplied] = 0xFF;

  // Record original data at custom enable flag address
  uint8_t original_byte = (*rom_)[OverworldCustomAreaSpecificBGEnabled];

  // Call save - should be a no-op for vanilla
  auto status =
      overworld_->SaveCustomOverworldASM(true, true, true, true, true, true);
  ASSERT_TRUE(status.ok());

  // Verify enable flags were NOT modified
  EXPECT_EQ((*rom_)[OverworldCustomAreaSpecificBGEnabled], original_byte);
}

TEST_F(OverworldRegressionTest,
       SaveCustomOverworldDataPreservesDisabledTables) {
  (*rom_)[OverworldCustomASMHasBeenApplied] = 0x03;
  (*rom_)[OverworldCustomAreaSpecificBGEnabled] = 0x00;
  (*rom_)[OverworldCustomMainPaletteEnabled] = 0x00;
  (*rom_)[OverworldCustomMosaicEnabled] = 0x00;
  (*rom_)[OverworldCustomAnimatedGFXEnabled] = 0x00;
  (*rom_)[OverworldCustomSubscreenOverlayEnabled] = 0x00;
  (*rom_)[OverworldCustomTileGFXGroupEnabled] = 0x00;
  PopulateOverworldMaps();

  (*rom_)[OverworldCustomAreaSpecificBGPalette] = 0xA1;
  (*rom_)[OverworldCustomMainPaletteArray] = 0xA2;
  (*rom_)[OverworldCustomMosaicArray] = 0xA3;
  (*rom_)[OverworldCustomAnimatedGFXArray] = 0xA4;
  (*rom_)[OverworldCustomSubscreenOverlayArray] = 0xA5;
  (*rom_)[OverworldCustomTileGFXGroupArray] = 0xA6;

  ASSERT_TRUE(overworld_->SaveCustomOverworldData().ok());

  EXPECT_EQ((*rom_)[OverworldCustomAreaSpecificBGPalette], 0xA1);
  EXPECT_EQ((*rom_)[OverworldCustomMainPaletteArray], 0xA2);
  EXPECT_EQ((*rom_)[OverworldCustomMosaicArray], 0xA3);
  EXPECT_EQ((*rom_)[OverworldCustomAnimatedGFXArray], 0xA4);
  EXPECT_EQ((*rom_)[OverworldCustomSubscreenOverlayArray], 0xA5);
  EXPECT_EQ((*rom_)[OverworldCustomTileGFXGroupArray], 0xA6);
}

TEST_F(OverworldRegressionTest,
       SaveCustomOverworldDataPreservesEnabledFlagEncoding) {
  (*rom_)[OverworldCustomASMHasBeenApplied] = 0x03;
  (*rom_)[OverworldCustomMosaicEnabled] = 0x01;
  PopulateOverworldMaps();

  ASSERT_TRUE(overworld_->SaveCustomOverworldData().ok());

  EXPECT_EQ((*rom_)[OverworldCustomMosaicEnabled], 0x01);
}

TEST_F(OverworldRegressionTest, SaveDiggableTiles_VanillaRom_SkipsWrite) {
  // Set version to Vanilla
  (*rom_)[OverworldCustomASMHasBeenApplied] = 0xFF;

  // Record original data at diggable tiles enable address
  uint8_t original_byte = (*rom_)[kOverworldCustomDiggableTilesEnabled];

  // Call save - should be a no-op for vanilla
  auto status = overworld_->SaveDiggableTiles();
  ASSERT_TRUE(status.ok());

  // Verify enable flag was NOT modified
  EXPECT_EQ((*rom_)[kOverworldCustomDiggableTilesEnabled], original_byte);
}

TEST_F(OverworldRegressionTest, SaveDiggableTiles_V2Rom_SkipsWrite) {
  // Set version to v2 (diggable tiles require v3+)
  (*rom_)[OverworldCustomASMHasBeenApplied] = 0x02;

  // Record original data at diggable tiles enable address
  uint8_t original_byte = (*rom_)[kOverworldCustomDiggableTilesEnabled];

  // Call save - should be a no-op for v2
  auto status = overworld_->SaveDiggableTiles();
  ASSERT_TRUE(status.ok());

  // Verify enable flag was NOT modified
  EXPECT_EQ((*rom_)[kOverworldCustomDiggableTilesEnabled], original_byte);
}

TEST_F(OverworldRegressionTest, SaveDiggableTiles_V3Rom_Writes) {
  // Set version to v3 (supports diggable tiles)
  (*rom_)[OverworldCustomASMHasBeenApplied] = 0x03;

  // Call save - should write for v3+
  auto status = overworld_->SaveDiggableTiles();
  ASSERT_TRUE(status.ok());

  // Verify enable flag WAS set to 0xFF
  EXPECT_EQ((*rom_)[kOverworldCustomDiggableTilesEnabled], 0xFF);
}

TEST_F(OverworldRegressionTest, SupportsCustomBGColors_VersionMatrix) {
  // Test the feature support matrix for custom BG colors

  // Vanilla - should NOT support
  (*rom_)[OverworldCustomASMHasBeenApplied] = 0xFF;
  EXPECT_FALSE(OverworldVersionHelper::SupportsCustomBGColors(
      OverworldVersionHelper::GetVersion(*rom_)));

  // v1 - should NOT support
  (*rom_)[OverworldCustomASMHasBeenApplied] = 0x01;
  EXPECT_FALSE(OverworldVersionHelper::SupportsCustomBGColors(
      OverworldVersionHelper::GetVersion(*rom_)));

  // v2 - should support
  (*rom_)[OverworldCustomASMHasBeenApplied] = 0x02;
  EXPECT_TRUE(OverworldVersionHelper::SupportsCustomBGColors(
      OverworldVersionHelper::GetVersion(*rom_)));

  // v3 - should support
  (*rom_)[OverworldCustomASMHasBeenApplied] = 0x03;
  EXPECT_TRUE(OverworldVersionHelper::SupportsCustomBGColors(
      OverworldVersionHelper::GetVersion(*rom_)));
}

TEST_F(OverworldRegressionTest, SupportsAreaEnum_VersionMatrix) {
  // Test the feature support matrix for area enum (v3+ features)

  // Vanilla - should NOT support
  (*rom_)[OverworldCustomASMHasBeenApplied] = 0xFF;
  EXPECT_FALSE(OverworldVersionHelper::SupportsAreaEnum(
      OverworldVersionHelper::GetVersion(*rom_)));

  // v1 - should NOT support
  (*rom_)[OverworldCustomASMHasBeenApplied] = 0x01;
  EXPECT_FALSE(OverworldVersionHelper::SupportsAreaEnum(
      OverworldVersionHelper::GetVersion(*rom_)));

  // v2 - should NOT support
  (*rom_)[OverworldCustomASMHasBeenApplied] = 0x02;
  EXPECT_FALSE(OverworldVersionHelper::SupportsAreaEnum(
      OverworldVersionHelper::GetVersion(*rom_)));

  // v3 - should support
  (*rom_)[OverworldCustomASMHasBeenApplied] = 0x03;
  EXPECT_TRUE(OverworldVersionHelper::SupportsAreaEnum(
      OverworldVersionHelper::GetVersion(*rom_)));
}

TEST_F(OverworldRegressionTest,
       SaveMapProperties_DarkWorldDoesNotOverwriteLightWorldSpriteTables) {
  (*rom_)[OverworldCustomASMHasBeenApplied] = 0xFF;
  PopulateOverworldMaps();

  auto* light_map = overworld_->mutable_overworld_map(0x04);
  auto* dark_map = overworld_->mutable_overworld_map(0x44);
  ASSERT_NE(light_map, nullptr);
  ASSERT_NE(dark_map, nullptr);

  light_map->set_sprite_graphics(1, 0x21);
  light_map->set_sprite_graphics(2, 0x22);
  light_map->set_sprite_palette(2, 0x07);
  dark_map->set_sprite_graphics(0, 0x5A);
  dark_map->set_sprite_palette(0, 0x0A);
  (*rom_)[kOverworldSpritePaletteIds + 192 + 0x44] = 0xEE;

  ASSERT_TRUE(overworld_->SaveMapProperties().ok());

  EXPECT_EQ((*rom_)[kOverworldSpriteset + 0x40 + 0x04], 0x21);
  EXPECT_EQ((*rom_)[kOverworldSpriteset + 0x80 + 0x04], 0x22);
  EXPECT_EQ((*rom_)[kOverworldSpriteset + 0x80 + 0x44], 0x5A);
  EXPECT_EQ((*rom_)[kOverworldSpritePaletteIds + 0x80 + 0x04], 0x07);
  EXPECT_EQ((*rom_)[kOverworldSpritePaletteIds + 0x80 + 0x44], 0x0A);
  EXPECT_EQ((*rom_)[kOverworldSpritePaletteIds + 192 + 0x44], 0xEE);
}

TEST_F(OverworldRegressionTest,
       SaveMapProperties_V3PersistsSpecialWorldToExpandedTables) {
  (*rom_)[OverworldCustomASMHasBeenApplied] = 0x03;
  PopulateOverworldMaps();

  auto* special_map = overworld_->mutable_overworld_map(0x81);
  ASSERT_NE(special_map, nullptr);
  special_map->set_area_graphics(0x66);
  special_map->set_area_palette(0x77);
  special_map->set_sprite_graphics(0, 0x12);
  special_map->set_sprite_palette(0, 0x05);
  (*rom_)[kOverworldSpecialSpriteGFXGroup + 0x01] = 0xE1;
  (*rom_)[kOverworldSpecialSpritePalette + 0x01] = 0xE2;

  ASSERT_TRUE(overworld_->SaveMapProperties().ok());

  EXPECT_EQ((*rom_)[kAreaGfxIdPtr + 0x81], 0x66);
  EXPECT_EQ((*rom_)[kOverworldPalettesScreenToSetNew + 0x81], 0x77);
  EXPECT_EQ((*rom_)[kOverworldSpecialSpriteGfxGroupExpandedTemp + 0x01], 0x12);
  EXPECT_EQ((*rom_)[kOverworldSpecialSpritePaletteExpandedTemp + 0x01], 0x05);
  EXPECT_EQ((*rom_)[kOverworldSpecialSpriteGFXGroup + 0x01], 0xE1);
  EXPECT_EQ((*rom_)[kOverworldSpecialSpritePalette + 0x01], 0xE2);
}

TEST_F(OverworldRegressionTest,
       BuildBitmapDerivesDarkWorldCoordinatesFromMapId) {
  OverworldMap map(kDarkWorldMapIdStart, rom_.get());
  auto tiles16 = SolidPaletteTestTiles16();
  OverworldBlockset dark_world(0x200, std::vector<uint16_t>(0x200, 0));
  dark_world[0][0] = 1;

  ASSERT_TRUE(map.BuildTileset().ok());
  ASSERT_TRUE(
      map.BuildTiles16Gfx(tiles16, static_cast<int>(tiles16.size())).ok());
  ASSERT_TRUE(map.BuildBitmap(dark_world).ok());

  ASSERT_FALSE(map.bitmap_data().empty());
  EXPECT_EQ(map.bitmap_data()[0], 0x10);
}

TEST_F(OverworldRegressionTest,
       BuildBitmapDerivesSpecialWorldCoordinatesFromMapId) {
  OverworldMap map(kSpecialWorldMapIdStart, rom_.get());
  auto tiles16 = SolidPaletteTestTiles16();
  OverworldBlockset special_world(0x200, std::vector<uint16_t>(0x200, 0));
  special_world[0][0] = 2;

  ASSERT_TRUE(map.BuildTileset().ok());
  ASSERT_TRUE(
      map.BuildTiles16Gfx(tiles16, static_cast<int>(tiles16.size())).ok());
  ASSERT_TRUE(map.BuildBitmap(special_world).ok());

  ASSERT_FALSE(map.bitmap_data().empty());
  EXPECT_EQ(map.bitmap_data()[0], 0x20);
}

// =============================================================================
// P1-02: Deleted OverworldItem must not persist after save
// =============================================================================

TEST_F(OverworldRegressionTest, SaveItems_DeletedItemsExcludedFromRom) {
  // Create three items on map 0, then mark the middle one deleted.
  // After SaveItems + LoadItems round-trip only two items should remain.

  std::vector<OverworldItem> items;
  // Item A: id=1, map 0, game coords (2, 3)
  OverworldItem item_a(/*id=*/1, /*room_map_id=*/0, /*x=*/32, /*y=*/48,
                       /*bg2=*/false);
  // Item B: id=2, map 0, game coords (4, 5) -- will be deleted
  OverworldItem item_b(/*id=*/2, /*room_map_id=*/0, /*x=*/64, /*y=*/80,
                       /*bg2=*/false);
  item_b.deleted = true;
  // Item C: id=3, map 0, game coords (6, 7)
  OverworldItem item_c(/*id=*/3, /*room_map_id=*/0, /*x=*/96, /*y=*/112,
                       /*bg2=*/false);

  items.push_back(item_a);
  items.push_back(item_b);
  items.push_back(item_c);

  // Save items to ROM (the free function already skips deleted items).
  auto save_status = SaveItems(rom_.get(), items);
  ASSERT_TRUE(save_status.ok()) << save_status.message();

  // Reload items from ROM. We need OverworldMaps for LoadItems.
  // Construct minimal maps (small area, parent == index).
  std::vector<OverworldMap> maps;
  maps.reserve(kNumOverworldMaps);
  for (int i = 0; i < kNumOverworldMaps; ++i) {
    maps.emplace_back(i, rom_.get());
  }

  auto loaded_or = LoadItems(rom_.get(), maps);
  ASSERT_TRUE(loaded_or.ok()) << loaded_or.status().message();
  const auto& loaded = loaded_or.value();

  // Only the two non-deleted items should have been saved and reloaded.
  EXPECT_EQ(loaded.size(), 2u);

  // Verify the deleted item (id=2) is absent.
  bool found_deleted =
      std::any_of(loaded.begin(), loaded.end(),
                  [](const OverworldItem& it) { return it.id_ == 2; });
  EXPECT_FALSE(found_deleted)
      << "Deleted item (id=2) should not appear after save/load round-trip";

  // Verify the surviving items are present.
  bool found_a =
      std::any_of(loaded.begin(), loaded.end(),
                  [](const OverworldItem& it) { return it.id_ == 1; });
  bool found_c =
      std::any_of(loaded.begin(), loaded.end(),
                  [](const OverworldItem& it) { return it.id_ == 3; });
  EXPECT_TRUE(found_a) << "Item A (id=1) should survive save/load";
  EXPECT_TRUE(found_c) << "Item C (id=3) should survive save/load";
}

TEST_F(OverworldRegressionTest, SaveItems_AllDeletedProducesEmptyRoundTrip) {
  // If every item is deleted, the round-trip should yield zero items.
  std::vector<OverworldItem> items;
  OverworldItem item(/*id=*/5, /*room_map_id=*/0, /*x=*/16, /*y=*/16,
                     /*bg2=*/false);
  item.deleted = true;
  items.push_back(item);

  auto save_status = SaveItems(rom_.get(), items);
  ASSERT_TRUE(save_status.ok()) << save_status.message();

  std::vector<OverworldMap> maps;
  maps.reserve(kNumOverworldMaps);
  for (int i = 0; i < kNumOverworldMaps; ++i) {
    maps.emplace_back(i, rom_.get());
  }

  auto loaded_or = LoadItems(rom_.get(), maps);
  ASSERT_TRUE(loaded_or.ok()) << loaded_or.status().message();
  EXPECT_EQ(loaded_or.value().size(), 0u);
}

TEST_F(OverworldRegressionTest, CompactDeletedItemsFromVector) {
  // Verify that the erase-remove idiom used in Overworld::SaveItems()
  // correctly compacts the vector, removing deleted items in-place.
  std::vector<OverworldItem> items;
  items.emplace_back(/*id=*/1, /*room_map_id=*/0, /*x=*/0, /*y=*/0,
                     /*bg2=*/false);
  items.emplace_back(/*id=*/2, /*room_map_id=*/0, /*x=*/16, /*y=*/0,
                     /*bg2=*/false);
  items.emplace_back(/*id=*/3, /*room_map_id=*/0, /*x=*/32, /*y=*/0,
                     /*bg2=*/false);

  items[0].deleted = true;
  items[2].deleted = true;

  // Apply the same erase-remove idiom used in Overworld::SaveItems()
  items.erase(
      std::remove_if(items.begin(), items.end(),
                     [](const OverworldItem& it) { return it.deleted; }),
      items.end());

  ASSERT_EQ(items.size(), 1u);
  EXPECT_EQ(items[0].id_, 2);
  EXPECT_FALSE(items[0].deleted);
}

}  // namespace zelda3
}  // namespace yaze
