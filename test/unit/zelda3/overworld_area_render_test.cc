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
  // Default DW sheet 7 (ReadAnimatedTable fallback for 0x00/0xFF).
  rom[OverworldCustomDefaultGFXGroups + 8 + 7] = 0x59;
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

TEST(OverworldAreaRenderTest,
     BoundAuxiliaryRowRendersAndUnavailableRowsRefuse) {
  auto bytes = MakeV3Rom();
  bytes.resize(0x240000, 0);
  // Palette set13 selects the new docks row15 (21), aux2 0D and animation06.
  const auto descriptor =
      zelda3_us_pointers.kOverworldMapPaletteGroup + 0x13 * 4;
  bytes[descriptor] = 0x15;
  bytes[descriptor + 1] = 0x0D;
  bytes[descriptor + 2] = 0x06;
  const uint32_t color_pc = 0x22F900 + 42 + 40;
  bytes[color_pc] = 0x34;
  bytes[color_pc + 1] = 0x12;
  auto rom = LoadRom(bytes);
  GameData data(rom.get());
  core::RomAddressOverrides overrides;
  overrides.addresses = {
      {core::RomAddressKey::kOverworldAuxPaletteExtensionPc, 0x22F900},
      {core::RomAddressKey::kOverworldAuxPaletteExtensionCount, 32},
      {core::RomAddressKey::kOverworldAuxPaletteExtensionAbi, 1}};
  auto layout =
      gfx::palette_group_internal::OverworldPaletteLayout::FromOverrides(
          overrides, rom->size());
  ASSERT_TRUE(layout.ok()) << layout.status();
  data.overworld_palette_layout = *layout;
  ASSERT_TRUE(LoadPalettes(*rom, data).ok());
  OverworldMap map(0x30, rom.get(), &data);
  map.set_area_palette(0x13);
  ASSERT_TRUE(map.LoadPalette().ok());
  EXPECT_EQ(map.current_palette()[4 * 16 + 15].snes(), 0x1234);
  // FF inherits the previous map's descriptor field without aliasing row13.
  (*rom)[kOverworldPalettesScreenToSetNew + 0x2F] = 0x12;
  (*rom)[descriptor] = 0xFF;
  (*rom)[zelda3_us_pointers.kOverworldMapPaletteGroup + 0x12 * 4] = 0x15;
  ASSERT_TRUE(map.LoadPalette().ok());
  EXPECT_EQ(map.current_palette()[4 * 16 + 15].snes(), 0x1234);
  (*rom)[descriptor] = 0x34;
  EXPECT_FALSE(map.LoadPalette().ok());
  (*rom)[descriptor] = 0x15;
  map.set_area_palette(0x1F);
  EXPECT_FALSE(map.LoadPalette().ok());
  map.set_area_palette(0x13);
  data.overworld_palette_layout =
      gfx::palette_group_internal::OverworldPaletteLayout::Legacy();
  // No provider declaration means no usable extension, even if old colors linger.
  data.palette_groups.overworld_aux.clear();
  ASSERT_TRUE(gfx::palette_group_internal::LoadOverworldPaletteRecords(
                  gfx::palette_group_internal::OverworldPaletteKind::kAuxiliary,
                  *data.overworld_palette_layout, rom->vector(),
                  data.palette_groups.overworld_aux)
                  .ok());
  EXPECT_FALSE(map.LoadPalette().ok());
}

TEST(OverworldAreaRenderTest, UnavailableSpriteColorsDoNotHideBackground) {
  auto bytes = MakeV3Rom();
  // Oracle's legacy special maps can retain sprite selectors whose rows are
  // not palette data. The background has a separate, valid palette contract.
  bytes[kOverworldSpritePaletteGroup + 0x8F * 2] = 0xB0;
  bytes[kOverworldSpritePaletteGroup + 0x8F * 2 + 1] = 0xC4;
  auto rom = LoadRom(bytes);
  GameData data(rom.get());
  ASSERT_TRUE(LoadPalettes(*rom, data).ok());
  OverworldMap map(0x98, rom.get(), &data);
  map.set_sprite_palette(0, 1);
  ASSERT_TRUE(map.LoadPalette().ok());
  const auto expected = map.current_palette();
  const auto before = rom->vector();
  map.set_sprite_palette(0, 0x8F);
  ASSERT_TRUE(map.LoadPalette().ok());
  for (int i = 0; i < 128; ++i) {
    EXPECT_EQ(map.current_palette()[i].snes(), expected[i].snes());
  }
  EXPECT_EQ(map.sprite_palette(0), 0x8F);
  EXPECT_FALSE(map.sprite_palette_warning().empty());
  EXPECT_EQ(rom->vector(), before);
  map.set_sprite_palette(0, 1);
  ASSERT_TRUE(map.LoadPalette().ok());
  EXPECT_TRUE(map.sprite_palette_warning().empty());
}

TEST(OverworldAreaRenderTest, UnavailableInheritedSpriteColorsKeepBackground) {
  auto bytes = MakeV3Rom();
  // Special map99 inherits OBJ colors. A bad previous selector must not make
  // its otherwise valid background disappear in the integrated layout either.
  bytes[kOverworldSpritePaletteGroup] = 0xFF;
  bytes[kOverworldSpritePaletteGroup + 1] = 0xFF;
  bytes[kOverworldSpritePaletteIds + 0x98] = 0x8F;
  bytes[kOverworldSpritePaletteGroup + 0x8F * 2] = 0xB0;
  bytes[kOverworldSpritePaletteGroup + 0x8F * 2 + 1] = 0xC4;
  auto rom = LoadRom(bytes);
  GameData data(rom.get());
  ASSERT_TRUE(LoadPalettes(*rom, data).ok());
  OverworldMap map(0x99, rom.get(), &data);
  const auto before = rom->vector();
  ASSERT_TRUE(map.LoadPalette().ok());
  EXPECT_EQ(map.current_palette().size(), 256);
  EXPECT_FALSE(map.sprite_palette_warning().empty());
  EXPECT_EQ(rom->vector(), before);
}

TEST(OverworldAreaRenderTest, AllQuadrantsResolveTheSameGraphics) {
  auto rom = LoadRom(MakeV3Rom());
  OverworldMap parent(kParent, rom.get());
  parent.LoadAreaGraphics();
  EXPECT_EQ(parent.animated_sheet(), 0x5B);
  EXPECT_EQ(parent.static_graphics(7), 0x59);
  for (int child : kChildren) {
    SCOPED_TRACE(child);
    OverworldMap map(child, rom.get());
    map.LoadAreaGraphics();
    EXPECT_EQ(map.animated_sheet(), parent.animated_sheet());
    EXPECT_EQ(map.tileset_key(), parent.tileset_key());
  }
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

TEST(OverworldAreaRenderTest, AnimatedFallbackUsesWorldDefaultSheet7) {
  auto bytes = MakeV3Rom();
  bytes[OverworldCustomAnimatedGFXArray + kParent] = 0xFF;
  auto rom = LoadRom(bytes);
  OverworldMap map(0x49, rom.get());
  map.LoadAreaGraphics();
  EXPECT_EQ(map.animated_sheet(), 0x59);
}

TEST(OverworldAreaRenderTest, Slot7TopHalfComesFromAnimatedSheet) {
  auto rom = LoadRom(MakeV3Rom());
  GameData game_data;
  game_data.graphics_buffer.assign(0x100 * 0x1000, 0x00);
  std::fill_n(game_data.graphics_buffer.begin() + 0x5B * 0x1000, 0x1000, 0x0A);
  std::fill_n(game_data.graphics_buffer.begin() + 0x59 * 0x1000, 0x1000, 0x05);

  OverworldMap map(0x48, rom.get(), &game_data);
  map.LoadAreaGraphics();
  ASSERT_TRUE(map.BuildTileset().ok());
  const auto& gfx = map.current_graphics();
  constexpr int kSlot7 = 7 * 0x1000;
  EXPECT_EQ(gfx[kSlot7], 0x0A);  // animated frame 0 (water)
  EXPECT_EQ(gfx[kSlot7 + kAnimatedSheetSlotBytes - 1], 0x0A);
  EXPECT_EQ(gfx[kSlot7 + kAnimatedSheetSlotBytes], 0x05);  // door frames
  EXPECT_EQ(gfx[kSlot7 + 0xFFF], 0x05);
}

TEST(OverworldAreaRenderTest,
     ExplicitSheetZeroLoadsBgAndPreservesAnimationAndEmptySprites) {
  auto bytes = MakeV3Rom();
  for (int slot : {0, 1, 3, 7})
    bytes[OverworldCustomTileGFXGroupArray + kParent * 8 + slot] = 0;
  // The legacy sprite group representation can resolve to its zero sentinel.
  bytes[zelda3_us_pointers.kSpriteBlocksetPointer] = 0x8D;
  auto rom = LoadRom(bytes);
  GameData data;
  data.graphics_buffer.assign(0x100 * 0x1000, 0);
  std::fill_n(data.graphics_buffer.begin(), 0x1000, 3);
  std::fill_n(data.graphics_buffer.begin() + 0x5B * 0x1000, 0x1000, 10);
  OverworldMap map(kParent, rom.get(), &data);
  map.set_sprite_graphics(0, 0);
  map.LoadAreaGraphics();
  ASSERT_TRUE(map.BuildTileset().ok());
  const auto& pixels = map.current_graphics();
  for (int slot : {0, 1, 3, 7}) {
    EXPECT_EQ(map.static_graphics(slot), 0);
    EXPECT_TRUE(map.GraphicsSlotHasSource(slot));
    const size_t begin = slot * 0x1000 + (slot == 7 ? 0x800 : 0);
    const uint8_t expected = slot == 0 || slot == 3 ? 0x8B : 3;
    EXPECT_TRUE(std::all_of(
        pixels.begin() + begin, pixels.begin() + (slot + 1) * 0x1000,
        [expected](uint8_t value) { return value == expected; }));
  }
  EXPECT_TRUE(std::all_of(pixels.begin() + 7 * 0x1000,
                          pixels.begin() + 7 * 0x1000 + 0x800,
                          [](uint8_t value) { return value == 10; }));
  ASSERT_EQ(map.static_graphics(12), 0);
  EXPECT_FALSE(map.GraphicsSlotHasSource(12));
  EXPECT_TRUE(std::all_of(pixels.begin() + 12 * 0x1000,
                          pixels.begin() + 13 * 0x1000,
                          [](uint8_t value) { return value == 0; }));
  EXPECT_FALSE(map.GraphicsSlotHasSource(-1));
  EXPECT_FALSE(map.GraphicsSlotHasSource(16));
}

TEST(OverworldAreaRenderTest, EmptyZeroAndCustomDefaultFallbackStayDistinct) {
  for (bool custom_groups : {false, true}) {
    SCOPED_TRACE(custom_groups);
    auto bytes = MakeV3Rom();
    bytes[OverworldCustomTileGFXGroupEnabled] = custom_groups ? 0xFF : 0;
    bytes[OverworldCustomTileGFXGroupArray + kParent * 8] = 0xFF;
    bytes[OverworldCustomTileGFXGroupArray + kParent * 8 + 1] = 0xFF;
    bytes[OverworldCustomDefaultGFXGroups + 8] = 0;
    bytes[OverworldCustomDefaultGFXGroups + 8 + 1] = 6;
    auto rom = LoadRom(bytes);
    GameData data;
    data.graphics_buffer.assign(0x100 * 0x1000, 0);
    std::fill_n(data.graphics_buffer.begin(), 0x1000, 3);
    std::fill_n(data.graphics_buffer.begin() + 6 * 0x1000, 0x1000, 4);
    OverworldMap map(kParent, rom.get(), &data);
    map.LoadAreaGraphics();
    ASSERT_EQ(map.static_graphics(0), 0);
    EXPECT_FALSE(map.GraphicsSlotHasSource(0));
    ASSERT_TRUE(map.BuildTileset().ok());
    EXPECT_TRUE(std::all_of(map.current_graphics().begin(),
                            map.current_graphics().begin() + 0x1000,
                            [](uint8_t value) { return value == 0; }));
    if (custom_groups) {
      EXPECT_EQ(map.static_graphics(1), 6);
      EXPECT_TRUE(map.GraphicsSlotHasSource(1));
      EXPECT_EQ(map.current_graphics()[0x1000], 4);
    }
  }
}

TEST(OverworldAreaRenderTest,
     ExplicitSheetZeroRevisionAndPresenceChangesInvalidateMapSources) {
  auto bytes = MakeV3Rom();
  bytes[OverworldCustomTileGFXGroupArray + kParent * 8] = 0;
  auto rom = LoadRom(bytes);
  GameData data;
  data.graphics_buffer.assign(0x100 * 0x1000, 0);
  OverworldMap map(kParent, rom.get(), &data);
  map.LoadAreaGraphics();
  ASSERT_TRUE(map.BuildTileset().ok());
  EXPECT_FALSE(map.SourceSheetsChanged());
  // The first tracked write must invalidate even when the loaded revision was0.
  std::vector<uint8_t> sheet(0x1000, 5);
  ASSERT_TRUE(data.sheet_store.WriteSheet(0, sheet).ok());
  EXPECT_TRUE(map.SourceSheetsChanged());
  ASSERT_TRUE(map.BuildTileset().ok());
  EXPECT_FALSE(map.SourceSheetsChanged());
  EXPECT_EQ(map.current_graphics()[0], 0x8D);
  // Same numeric sheet ID, different meaning: FF resolves to an empty default0.
  map.set_custom_tileset(0, 0xFF);
  EXPECT_TRUE(map.SourceSheetsChanged());
  map.LoadAreaGraphics();
  ASSERT_TRUE(map.BuildTileset().ok());
  EXPECT_EQ(map.current_graphics()[0], 0);
  EXPECT_FALSE(map.SourceSheetsChanged());
  std::fill(sheet.begin(), sheet.end(), 6);
  ASSERT_TRUE(data.sheet_store.WriteSheet(0, sheet).ok());
  EXPECT_FALSE(map.SourceSheetsChanged());
}

TEST(OverworldAreaRenderTest,
     SharedCacheSeparatesExplicitZeroAndTracksSheetZeroEdits) {
  auto bytes = MakeV3Rom();
  for (int map : {0, 1})
    for (int slot = 0; slot < 8; ++slot)
      bytes[OverworldCustomTileGFXGroupArray + map * 8 + slot] = 0xFF;
  bytes[OverworldCustomTileGFXGroupArray] = 0;
  auto rom = LoadRom(bytes);
  GameData data(rom.get());
  ASSERT_TRUE(LoadPalettes(*rom, data).ok());
  data.graphics_buffer.assign(0x100 * 0x1000, 0);
  std::fill_n(data.graphics_buffer.begin(), 0x1000, 3);
  Overworld world(rom.get(), &data);
  world.GetMapTiles(0).assign(64, std::vector<uint16_t>(32, 0));
  const auto tile = gfx::WordToTileInfo(0);
  world.mutable_tiles16()->emplace_back(tile, tile, tile, tile);
  auto& maps = const_cast<std::vector<OverworldMap>&>(world.overworld_maps());
  for (int map : {0, 1})
    maps.emplace_back(map, rom.get(), &data);
  const auto built0 = world.EnsureMapBuilt(0);
  ASSERT_TRUE(built0.ok()) << built0;
  const auto built1 = world.EnsureMapBuilt(1);
  ASSERT_TRUE(built1.ok()) << built1;
  EXPECT_EQ(maps[0].tileset_key(), maps[1].tileset_key());
  EXPECT_EQ(maps[0].current_graphics()[0], 0x8B);
  EXPECT_EQ(maps[1].current_graphics()[0], 0);
  EXPECT_EQ(world.tileset_cache_size(), 2u);
  std::vector<uint8_t> sheet(0x1000, 5);
  ASSERT_TRUE(data.sheet_store.WriteSheet(0, sheet).ok());
  maps[0].SetNotBuilt();
  ASSERT_TRUE(world.EnsureMapBuilt(0).ok());
  EXPECT_EQ(maps[0].current_graphics()[0], 0x8D);
  EXPECT_EQ(maps[1].current_graphics()[0], 0);
  EXPECT_EQ(world.tileset_cache_size(), 2u);
  world.InvalidateTilesetCacheForSheet(0);
  EXPECT_EQ(world.tileset_cache_size(), 1u);
  world.InvalidateMapCache(1);
  EXPECT_EQ(world.tileset_cache_size(), 0u);
}

TEST(OverworldAreaRenderTest, VanillaAnimatedSheetAndOverlays) {
  std::vector<uint8_t> bytes(0x200000, 0x00);
  bytes[OverworldCustomASMHasBeenApplied] = 0xFF;
  // Identity parents, all small, vanilla DW group sheet 7 = 0x59.
  for (int i = 0; i < 64; ++i) {
    bytes[kOverworldMapParentId + i] = static_cast<uint8_t>(i);
  }
  for (int i = 0; i < kNumOverworldMaps; ++i) {
    bytes[kOverworldScreenSize + i] = 0x01;  // legacy: 1 = small
  }
  auto rom = LoadRom(bytes);

  OverworldMap dm(0x03, rom.get());
  dm.LoadAreaGraphics();
  EXPECT_EQ(dm.animated_sheet(), 0x59);
  EXPECT_EQ(dm.render_subscreen_overlay(), 0x0095);

  OverworldMap dw(0x50, rom.get());
  dw.LoadAreaGraphics();
  EXPECT_EQ(dw.animated_sheet(), 0x5B);
  EXPECT_EQ(dw.render_subscreen_overlay(), 0x00FF);

  OverworldMap lost_woods(0x00, rom.get());
  EXPECT_EQ(lost_woods.render_subscreen_overlay(), 0x009D);
}

TEST(OverworldAreaRenderTest, SubscreenOverlayClassification) {
  EXPECT_EQ(SubscreenOverlayScreen(0x0095), 0x95);
  EXPECT_EQ(SubscreenOverlayScreen(0x009F), 0x9F);
  EXPECT_EQ(SubscreenOverlayScreen(0x00FF), -1);
  EXPECT_EQ(SubscreenOverlayScreen(0x0000), -1);
  EXPECT_TRUE(IsBackgroundSubscreenOverlay(0x0095));
  EXPECT_TRUE(IsBackgroundSubscreenOverlay(0x0096));
  EXPECT_TRUE(IsBackgroundSubscreenOverlay(0x009C));
  EXPECT_FALSE(IsBackgroundSubscreenOverlay(0x009D));
  EXPECT_FALSE(IsBackgroundSubscreenOverlay(0x009F));
}

}  // namespace
}  // namespace yaze::zelda3
