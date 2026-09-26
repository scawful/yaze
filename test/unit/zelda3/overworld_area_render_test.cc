// Area-level render settings for multi-screen overworld areas: child screens
// render with the parent's main palette / animated GFX / tile GFX groups /
// subscreen overlay (the game reads those tables with $8A = parent), and
// graphics slot 7 is the animated sheet (top) over the area's sheet 7
// (bottom).

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "rom/rom.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze::zelda3 {
namespace {

constexpr int kParent = 0x40;
constexpr int kChildren[] = {0x41, 0x48, 0x49};

// Mock ZSCustomOverworld v3 ROM with DW large area 0x40 (children 0x41,
// 0x48, 0x49) whose child table entries differ from the parent's, like the
// Oracle of Secrets ROM.
std::vector<uint8_t> MakeV3Rom() {
  std::vector<uint8_t> rom(0x200000, 0x00);
  rom[OverworldCustomASMHasBeenApplied] = 0x03;
  rom[OverworldCustomMainPaletteEnabled] = 0xFF;
  rom[OverworldCustomAnimatedGFXEnabled] = 0xFF;
  rom[OverworldCustomSubscreenOverlayEnabled] = 0xFF;
  rom[OverworldCustomTileGFXGroupEnabled] = 0xFF;
  for (int i = 0; i < kNumOverworldMaps; ++i) {
    rom[kOverworldMapParentIdExpanded + i] = static_cast<uint8_t>(i);
    rom[kOverworldScreenSize + i] = 0x00;  // small
    rom[OverworldCustomSubscreenOverlayArray + i * 2] = 0xFF;
  }
  const int area[] = {kParent, 0x41, 0x48, 0x49};
  for (int id : area) {
    rom[kOverworldMapParentIdExpanded + id] = kParent;
    rom[kOverworldScreenSize + id] = 0x01;  // large
    rom[OverworldCustomMainPaletteArray + id] = 0x01;
    rom[OverworldCustomAnimatedGFXArray + id] = 0x59;
    rom[OverworldCustomSubscreenOverlayArray + id * 2] = 0xFF;
    for (int s = 0; s < 8; ++s) {
      rom[OverworldCustomTileGFXGroupArray + id * 8 + s] = 0xFF;
    }
  }
  // Parent entries: what the game uses for the whole area.
  rom[OverworldCustomMainPaletteArray + kParent] = 0x02;
  rom[OverworldCustomAnimatedGFXArray + kParent] = 0x5B;
  rom[OverworldCustomSubscreenOverlayArray + kParent * 2] = 0x9F;
  rom[OverworldCustomSubscreenOverlayArray + kParent * 2 + 1] = 0x00;
  rom[OverworldCustomTileGFXGroupArray + kParent * 8 + 7] = 0x59;
  return rom;
}

std::unique_ptr<Rom> LoadRom(const std::vector<uint8_t>& bytes) {
  auto rom = std::make_unique<Rom>();
  EXPECT_TRUE(rom->LoadFromData(bytes).ok());
  return rom;
}

TEST(OverworldAreaRenderTest, ChildScreensRenderWithParentAreaSettings) {
  auto rom = LoadRom(MakeV3Rom());
  for (int child : kChildren) {
    SCOPED_TRACE(child);
    OverworldMap map(child, rom.get());
    ASSERT_EQ(map.parent(), kParent);
    EXPECT_EQ(map.render_main_palette(), 0x02);
    EXPECT_EQ(map.render_subscreen_overlay(), 0x009F);
    EXPECT_EQ(map.area_render_properties().animated_gfx, 0x5B);
    EXPECT_EQ(map.area_render_properties().custom_gfx_ids[7], 0x59);
    // Raw per-screen values are kept for byte-identical saves.
    EXPECT_EQ(map.main_palette(), 0x01);
    EXPECT_EQ(map.subscreen_overlay(), 0x00FF);
    EXPECT_EQ(map.animated_gfx(), 0x59);
  }
  OverworldMap parent(kParent, rom.get());
  EXPECT_FALSE(parent.has_inherited_area_properties());
  EXPECT_EQ(parent.render_main_palette(), 0x02);
  EXPECT_EQ(parent.render_subscreen_overlay(), 0x009F);
}

TEST(OverworldAreaRenderTest, OverworldSyncPropagatesUnsavedParentEdits) {
  auto rom = LoadRom(MakeV3Rom());
  Overworld overworld(rom.get());
  auto& maps =
      const_cast<std::vector<OverworldMap>&>(overworld.overworld_maps());
  maps.clear();
  for (int i = 0; i < kNumOverworldMaps; ++i) {
    maps.emplace_back(i, rom.get());
  }
  maps[kParent].set_main_palette(0x05);
  maps[kParent].set_subscreen_overlay(0x0095);
  for (int child : kChildren) {
    overworld.SyncAreaProperties(child);
    EXPECT_EQ(maps[child].render_main_palette(), 0x05);
    EXPECT_EQ(maps[child].render_subscreen_overlay(), 0x0095);
    EXPECT_EQ(maps[child].main_palette(), 0x01);
  }
  // Splitting the area makes the screen its own area again.
  maps[0x49].SetAsSmallMap();
  overworld.SyncAreaProperties(0x49);
  EXPECT_FALSE(maps[0x49].has_inherited_area_properties());
  EXPECT_EQ(maps[0x49].render_main_palette(), 0x01);
}

}  // namespace
}  // namespace yaze::zelda3
