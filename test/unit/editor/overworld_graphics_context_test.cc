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
  EXPECT_EQ(context->targets.size(), 1u);
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
