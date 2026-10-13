#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/overworld/overworld_graphics_context.h"
#include "app/editor/overworld/tile16_editor.h"
#include "app/gfx/types/snes_color.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/types/snes_tile.h"
#include "rom/rom.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze::editor {
namespace {

class OverworldGraphicsContextTest : public ::testing::Test {
 protected:
  void SetUp() override {
    data_.graphics_buffer.assign(zelda3::GraphicsSheetStore::kSheetCount * 4096,
                                 0);
    std::vector<gfx::SnesColor> colors;
    colors.reserve(256);
    for (int i = 0; i < 256; ++i) {
      colors.emplace_back(static_cast<uint8_t>((i & 7) * 8),
                          static_cast<uint8_t>(((i >> 3) & 7) * 8),
                          static_cast<uint8_t>(((i >> 6) & 3) * 16));
    }
    *map_.mutable_current_palette() = gfx::SnesPalette(colors);
    map_.SetParent(0x82);  // A special-world child uses its resolved colors.
    for (int slot = 0; slot < 8; ++slot) {
      *map_.mutable_static_graphics(slot) = 0x10 + slot;
    }
  }

  gfx::Tile16 Tile(int tile8, int palette) const {
    const gfx::TileInfo info(tile8, palette, false, false, false);
    return gfx::Tile16(info, info, info, info);
  }

  zelda3::GameData data_;
  zelda3::OverworldMap map_;
};

TEST_F(OverworldGraphicsContextTest, DefaultMapHasNoRomIdentity) {
  zelda3::OverworldMap map;
  const auto context = zelda3::DescribeOverworldGraphicsContext(map);
  EXPECT_EQ(context.surface, "overworld_map");
  EXPECT_FALSE(context.graphics_loaded);
  EXPECT_TRUE(context.rom_sha256.empty());
}

TEST_F(OverworldGraphicsContextTest,
       UsesSelectedMapsLoadedSlotsAndPaletteHalves) {
  for (int slot = 0; slot < 8; ++slot) {
    const int tile_index = slot == 7 ? 32 : 5;
    const auto tile = Tile(slot * 64 + tile_index, 3);
    auto result = ResolveOverworldQuadrantTarget(map_, tile, 0x123, 2, &data_);
    ASSERT_TRUE(result.ok()) << result.status();
    EXPECT_FALSE(result->writable_palette);
    EXPECT_FALSE(result->palette_address_limitation.empty());
    EXPECT_EQ(result->sheet_id, 0x10 + slot);
    EXPECT_EQ(result->tile_index, tile_index);
    EXPECT_NE(result->label.find("quadrant 3"), std::string::npos);
    const int half = (slot == 0 || slot == 3 || slot == 4 || slot == 5) ? 8 : 0;
    for (int pixel = 0; pixel < 8; ++pixel) {
      const auto color = map_.current_palette()[3 * 16 + half + pixel];
      EXPECT_EQ(result->palette[pixel].snes(), color.snes());
    }
  }
  auto context =
      MakeOverworldGraphicsEditContext(map_, Tile(64 + 4, 2), 0x234, 0, &data_);
  ASSERT_TRUE(context.ok()) << context.status();
  EXPECT_EQ(context->owner, &data_);
  EXPECT_EQ(context->targets.size(), 4u);
  EXPECT_NE(context->label.find("area 82"), std::string::npos);

  // An alternate selected-map palette changes display colors without
  // modifying the source sheet or another map's palette.
  (*map_.mutable_current_palette())[2 * 16 + 1] = gfx::SnesColor(248, 0, 0);
  auto alternate =
      ResolveOverworldQuadrantTarget(map_, Tile(64 + 4, 2), 0x234, 0, &data_);
  ASSERT_TRUE(alternate.ok()) << alternate.status();
  EXPECT_EQ(alternate->sheet_id, context->targets.front().sheet_id);
  EXPECT_NE(alternate->palette[1].snes(),
            context->targets.front().palette[1].snes());
}

TEST_F(OverworldGraphicsContextTest, RefusesAnimatedUnmappedAndMissingSources) {
  EXPECT_EQ(
      ResolveOverworldQuadrantTarget(map_, Tile(7 * 64 + 31, 1), 1, 0, &data_)
          .status()
          .code(),
      absl::StatusCode::kUnimplemented);
  EXPECT_EQ(ResolveOverworldQuadrantTarget(map_, Tile(8 * 64, 1), 1, 0, &data_)
                .status()
                .code(),
            absl::StatusCode::kUnimplemented);
  *map_.mutable_static_graphics(1) = 113;  // Compressed 2bpp.
  EXPECT_EQ(ResolveOverworldQuadrantTarget(map_, Tile(64, 1), 1, 0, &data_)
                .status()
                .code(),
            absl::StatusCode::kUnimplemented);
  *map_.mutable_static_graphics(1) = 0x11;
  data_.sheet_store.Clear();
  EXPECT_EQ(ResolveOverworldQuadrantTarget(map_, Tile(64, 1), 1, 0, &data_)
                .status()
                .code(),
            absl::StatusCode::kUnimplemented);
  EXPECT_EQ(ResolveOverworldQuadrantTarget(map_, Tile(64, 1), 1, 4, &data_)
                .status()
                .code(),
            absl::StatusCode::kInvalidArgument);
}

TEST_F(OverworldGraphicsContextTest,
       WholeTileKeepsEachQuadrantsPaletteAndFlipWithRequestedTileFirst) {
  const gfx::Tile16 tile(gfx::TileInfo(64 + 5, 2, false, false, false),
                         gfx::TileInfo(3 * 64 + 6, 3, false, true, false),
                         gfx::TileInfo(64 + 5, 4, true, false, false),
                         gfx::TileInfo(3 * 64 + 6, 5, true, true, false));
  auto context = MakeOverworldGraphicsEditContext(map_, tile, 0x123, 2, &data_);
  ASSERT_TRUE(context.ok()) << context.status();
  ASSERT_EQ(context->targets.size(), 4u);
  const int order[] = {2, 0, 1, 3};
  for (int i = 0; i < 4; ++i) {
    const auto resolved =
        ResolveOverworldQuadrantTarget(map_, tile, 0x123, order[i], &data_);
    ASSERT_TRUE(resolved.ok()) << resolved.status();
    const auto& target = context->targets[i];
    EXPECT_EQ(target.label, resolved->label);
    EXPECT_EQ(target.source_orientation_mask, 1 << order[i]);
    for (int pixel = 0; pixel < 8; ++pixel) {
      EXPECT_EQ(target.palette[pixel].snes(), resolved->palette[pixel].snes());
    }
  }
  // Reused physical tiles remain separate uses because their palettes differ.
  EXPECT_EQ(context->targets[0].sheet_id, context->targets[1].sheet_id);
  EXPECT_EQ(context->targets[0].tile_index, context->targets[1].tile_index);
  EXPECT_NE(context->targets[0].palette[1].snes(),
            context->targets[1].palette[1].snes());
}

TEST_F(OverworldGraphicsContextTest,
       OtherAnimatedQuadrantsAreExplicitlyOmitted) {
  const gfx::TileInfo ordinary(64 + 5, 2, false, false, false);
  const gfx::TileInfo animated(7 * 64 + 1, 2, false, false, false);
  const gfx::Tile16 tile(ordinary, animated, ordinary, ordinary);
  auto context = MakeOverworldGraphicsEditContext(map_, tile, 0x123, 0, &data_);
  ASSERT_TRUE(context.ok()) << context.status();
  EXPECT_EQ(context->targets.size(), 3u);
  EXPECT_NE(context->limitation.find("Quadrant 2 is omitted"),
            std::string::npos);
  EXPECT_EQ(MakeOverworldGraphicsEditContext(map_, tile, 0x123, 1, &data_)
                .status()
                .code(),
            absl::StatusCode::kUnimplemented);
}

TEST(OverworldGraphicsContextUiTest, InspectorActionUsesActiveQuadrant) {
  gfx::Tilemap blockset;
  Tile16Editor editor(nullptr, &blockset);
  int seen_tile = -1;
  int seen_quadrant = -1;
  editor.set_active_quadrant(3);
  editor.SetOpenGraphicsForQuadrant([&](int tile, int quadrant) {
    seen_tile = tile;
    seen_quadrant = quadrant;
    return absl::OkStatus();
  });
  EXPECT_TRUE(editor.OpenGraphicsForActiveQuadrant().ok());
  EXPECT_EQ(seen_tile, editor.current_tile16());
  EXPECT_EQ(seen_quadrant, 3);
}

TEST(OverworldGraphicsContextRomTest,
     WritablePaletteTracksMainAuxAndInheritedSelectors) {
  const char* path = std::getenv("YAZE_TEST_ROM_OOS");
  if (path == nullptr || !std::filesystem::exists(path))
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS to an Oracle ROM copy";
  Rom rom;
  zelda3::GameData data;
  ASSERT_TRUE(rom.LoadFromFile(path).ok());
  ASSERT_TRUE(zelda3::LoadGameData(rom, data).ok());
  const auto version = zelda3::OverworldVersionHelper::GetVersion(rom);
  ASSERT_NE(version, zelda3::OverworldVersion::kVanilla);
  const auto group_table =
      zelda3::kVersionConstantsMap.at(data.version).kOverworldMapPaletteGroup;
  const auto area_table =
      zelda3::OverworldVersionHelper::SupportsAreaEnum(version)
          ? zelda3::kOverworldPalettesScreenToSetNew
          : zelda3::kOverworldMapPaletteIds;
  // Controlled selectors in this private in-memory ROM only. The real loader
  // must resolve FF through the previous area's group, not guess from RGB.
  ASSERT_TRUE(rom.WriteByte(area_table + 0x81, 4).ok());
  for (int column = 0; column < 3; ++column) {
    ASSERT_TRUE(rom.WriteByte(group_table + 3 * 4 + column, 0xFF).ok());
    ASSERT_TRUE(rom.WriteByte(group_table + 4 * 4 + column, 7 + column).ok());
  }
  zelda3::OverworldMap parent(0x82, &rom, &data);
  parent.set_main_palette(1);
  zelda3::OverworldMap map(0x83, &rom, &data);
  map.SetParent(0x82);
  map.set_main_palette(0);
  map.InheritAreaProperties(parent);
  map.set_area_palette(3);
  map.LoadAreaGraphics();
  ASSERT_TRUE(map.LoadPalette().ok());
  // Keep each tested quadrant on a known editable sheet; this fixture tests
  // palette routing, independently of the child's graphics assignment.
  for (int slot = 0; slot < 8; ++slot)
    *map.mutable_static_graphics(slot) = 0x11;
  auto resolve = [&](int row, int slot) {
    const gfx::TileInfo tile(slot * 64 + 32, row, false, false, false);
    return ResolveOverworldQuadrantTarget(
        map, gfx::Tile16(tile, tile, tile, tile), 0, 0, &data);
  };
  for (int row = 0; row < 8; ++row) {
    for (int slot = 0; slot < 8; ++slot) {
      auto result = resolve(row, slot);
      ASSERT_TRUE(result.ok()) << result.status();
      const int half =
          (slot == 0 || slot == 3 || slot == 4 || slot == 5) ? 8 : 0;
      if (row < 2 || (row == 7 && half == 0)) {
        EXPECT_FALSE(result->writable_palette);
        EXPECT_FALSE(result->palette_address_limitation.empty());
        continue;
      }
      ASSERT_TRUE(result->writable_palette)
          << result->palette_address_limitation;
      EXPECT_TRUE(ValidateGraphicsPaletteAddressing(*result, &data).ok());
      const auto& binding = *result->writable_palette;
      EXPECT_EQ(binding.row, row);
      EXPECT_EQ(binding.half_offset, half);
      EXPECT_FALSE(binding.colors[0]);
      for (int raw = 1; raw < 8; ++raw) {
        ASSERT_TRUE(binding.colors[raw]);
        const auto& address = *binding.colors[raw];
        EXPECT_EQ(address.group,
                  half == 0 ? zelda3::PaletteAddressGroup::kOverworldMain
                            : zelda3::PaletteAddressGroup::kOverworldAux);
        EXPECT_EQ(address.palette_index, half == 0 ? 1 : (row < 5 ? 7 : 8));
        EXPECT_EQ(address.color_index,
                  (row - (half == 0 || row < 5 ? 2 : 5)) * 7 + raw - 1);
        auto color = zelda3::ReadPaletteColor(data, address);
        ASSERT_TRUE(color.ok());
        EXPECT_EQ(color->snes(), result->palette[raw].snes());
      }
    }
  }
  parent.set_main_palette(0);
  map.InheritAreaProperties(parent);
  ASSERT_TRUE(map.LoadPalette().ok());
  auto alternate = resolve(2, 1);
  ASSERT_TRUE(alternate.ok());
  ASSERT_TRUE(alternate->writable_palette);
  EXPECT_EQ(alternate->writable_palette->colors[1]->palette_index, 0);
  zelda3::GameData foreign;
  const gfx::TileInfo tile(64 + 32, 2, false, false, false);
  EXPECT_EQ(ResolveOverworldQuadrantTarget(
                map, gfx::Tile16(tile, tile, tile, tile), 0, 0, &foreign)
                .status()
                .code(),
            absl::StatusCode::kFailedPrecondition);
  // Even an override with identical RGB does not establish writable provenance.
  auto* display_override = map.mutable_current_palette();
  ASSERT_FALSE(display_override->empty());
  auto unbound = resolve(2, 1);
  ASSERT_TRUE(unbound.ok());
  EXPECT_FALSE(unbound->writable_palette);
  ASSERT_TRUE(map.LoadPalette().ok());
  EXPECT_TRUE(resolve(2, 1)->writable_palette);
}

TEST(OverworldGraphicsContextRomTest, SlotZeroRawZeroUsesAreaBackdrop) {
  const char* path = std::getenv("YAZE_TEST_ROM_OOS");
  if (path == nullptr || !std::filesystem::exists(path)) {
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS to an Oracle ROM copy";
  }
  Rom rom;
  zelda3::GameData data;
  ASSERT_TRUE(rom.LoadFromFile(path).ok());
  ASSERT_TRUE(zelda3::LoadGameData(rom, data).ok());
  zelda3::OverworldMap map(0, &rom, &data);
  map.LoadAreaGraphics();
  ASSERT_TRUE(map.LoadPalette().ok());
  const gfx::TileInfo info(0, 2, false, false, false);
  const gfx::Tile16 tile(info, info, info, info);
  auto result = ResolveOverworldQuadrantTarget(map, tile, 0, 0, &data);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->sheet_id, map.static_graphics(0));
  EXPECT_EQ(result->palette[0].snes(),
            map.current_palette()[2 * 16 + 8].snes());
  EXPECT_EQ(result->palette[0].snes(), map.current_palette()[2 * 16].snes());
  EXPECT_EQ(result->palette[1].snes(),
            map.current_palette()[2 * 16 + 9].snes());
}

}  // namespace
}  // namespace yaze::editor
