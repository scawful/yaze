#include "zelda3/cutscene/cutscene_shot.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <string>

#include "rom/rom.h"
#include "test_utils.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::zelda3 {
namespace {

// The example from the request, with an actor note.
constexpr char kExample[] = R"({"version": 1, "shots": [
  {"name": "tail_pond_open", "area": "0x2D",
   "camera": {"x": 2600, "y": 2700},
   "link": {"x": 2720, "y": 2800, "facing": 2},
   "actors": [{"sprite": "0xF5", "x": 2690, "y": 2770, "facing": 1,
               "note": "Zora A"}]}
]})";

TEST(CutsceneShotTest, ParsesTheRequestExample) {
  auto set = ParseCutsceneShots(kExample);
  ASSERT_TRUE(set.ok()) << set.status();
  ASSERT_EQ(set->shots.size(), 1u);
  const CutsceneShot& shot = set->shots[0];
  EXPECT_EQ(shot.name, "tail_pond_open");
  EXPECT_EQ(shot.area, 0x2D);
  EXPECT_EQ(shot.camera_x, 2600);
  EXPECT_EQ(shot.camera_y, 2700);
  EXPECT_EQ(shot.link, (CutsceneLink{2720, 2800, 2}));
  ASSERT_EQ(shot.actors.size(), 1u);
  EXPECT_EQ(shot.actors[0], (CutsceneActor{0xF5, 2690, 2770, 1, "Zora A"}));
}

TEST(CutsceneShotTest, RoundTripIsStable) {
  auto set = ParseCutsceneShots(kExample);
  ASSERT_TRUE(set.ok());
  auto text = SerializeCutsceneShots(*set);
  ASSERT_TRUE(text.ok()) << text.status();
  auto again = ParseCutsceneShots(*text);
  ASSERT_TRUE(again.ok()) << again.status();
  EXPECT_EQ(*again, *set);
  auto text_again = SerializeCutsceneShots(*again);
  ASSERT_TRUE(text_again.ok());
  EXPECT_EQ(*text_again, *text);
  EXPECT_NE(text->find("\"area\": \"0x2D\""), std::string::npos);
  EXPECT_NE(text->find("\"sprite\": \"0xF5\""), std::string::npos);
  EXPECT_EQ(text->back(), '\n');
}

TEST(CutsceneShotTest, RejectsInvalidDocuments) {
  const char* bad[] = {
      R"({"version": 2, "shots": []})",
      R"({"version": 1})",
      R"({"version": 1, "shots": [{"name": "a b", "area": 1,
          "camera": {"x": 0, "y": 0}, "link": {"x": 0, "y": 0, "facing": 0}}]})",
      R"({"version": 1, "shots": [{"name": "a", "area": 1,
          "camera": {"x": 0, "y": 0}, "link": {"x": 0, "y": 0, "facing": 4}}]})",
      R"({"version": 1, "shots": [{"name": "a", "area": "0x80",
          "camera": {"x": 0, "y": 0}, "link": {"x": 0, "y": 0, "facing": 0}}]})",
      R"({"version": 1, "shots": [{"name": "a", "area": 1,
          "camera": {"x": 0, "y": 0}, "link": {"x": 0, "y": 0, "facing": 0},
          "actors": [{"sprite": "0x100", "x": 0, "y": 0, "facing": 0}]}]})",
      R"({"version": 1, "shots": [
          {"name": "a", "area": 1, "camera": {"x": 0, "y": 0},
           "link": {"x": 0, "y": 0, "facing": 0}},
          {"name": "a", "area": 1, "camera": {"x": 0, "y": 0},
           "link": {"x": 0, "y": 0, "facing": 0}}]})",
      R"(not json)",
  };
  for (const char* json : bad) {
    EXPECT_FALSE(ParseCutsceneShots(json).ok()) << json;
  }
}

// Origins and sizes from OverworldTransitionPositionX/Y in usdasm bank_02.
TEST(CutsceneShotTest, VanillaAreaLayout) {
  auto small = VanillaAreaExtent(0x2D);
  ASSERT_TRUE(small.ok());
  EXPECT_EQ(small->origin_x, 0x0A00);
  EXPECT_EQ(small->origin_y, 0x0A00);
  EXPECT_EQ(small->width, 512);

  auto lost_woods = VanillaAreaExtent(0x00);
  ASSERT_TRUE(lost_woods.ok());
  EXPECT_EQ(lost_woods->width, 1024);

  // 0x0C is the lower-right screen of the large area whose parent is 0x03.
  auto child = VanillaAreaExtent(0x0C);
  ASSERT_TRUE(child.ok());
  EXPECT_EQ(child->origin_x, 0x0600);
  EXPECT_EQ(child->origin_y, 0x0000);
  EXPECT_EQ(child->width, 1024);

  // Dark World areas share the Light World layout.
  auto dark = VanillaAreaExtent(0x6D);
  ASSERT_TRUE(dark.ok());
  EXPECT_EQ(dark->origin_x, small->origin_x);
  EXPECT_EQ(dark->width, 512);

  EXPECT_FALSE(VanillaAreaExtent(0x80).ok());
}

// The game's limits: right edge +$0100/$0300, bottom edge +$011E/$031E.
TEST(CutsceneShotTest, ClampsToSmallAreaBounds) {
  const AreaExtent area{0x0A00, 0x0A00, 512, 512};
  const CameraBounds bounds = CameraBoundsForArea(area);
  EXPECT_EQ(bounds.max_x, 0x0A00 + 0x0100);
  EXPECT_EQ(bounds.max_y, 0x0A00 + 0x011E);

  ClampedCamera inside = ClampCamera(area, 0x0A40, 0x0A40);
  EXPECT_FALSE(inside.clamped());
  EXPECT_EQ(inside.x, 0x0A40);

  ClampedCamera low = ClampCamera(area, 0, 0);
  EXPECT_TRUE(low.clamped());
  EXPECT_EQ(low.x, 0x0A00);
  EXPECT_EQ(low.y, 0x0A00);
  EXPECT_EQ(low.requested_x, 0);

  ClampedCamera high = ClampCamera(area, 0x2000, 0x2000);
  EXPECT_EQ(high.x, 0x0B00);
  EXPECT_EQ(high.y, 0x0B1E);
}

TEST(CutsceneShotTest, ClampsToLargeAreaBounds) {
  const AreaExtent area{0x0600, 0x0000, 1024, 1024};
  ClampedCamera high = ClampCamera(area, 0x2000, 0x2000);
  EXPECT_EQ(high.x, 0x0600 + 0x0300);
  EXPECT_EQ(high.y, 0x0000 + 0x031E);
  ClampedCamera inside = ClampCamera(area, 0x0800, 0x0200);
  EXPECT_FALSE(inside.clamped());
}

TEST(CutsceneShotTest, ViewportAndMarkersLandOnMapPixels) {
  const AreaExtent area{0x0A00, 0x0A00, 512, 512};
  const MapRect viewport = ViewportOnMap(area, 0x0A40, 0x0A20, 0.5f);
  EXPECT_FLOAT_EQ(viewport.x, 32.0f);
  EXPECT_FLOAT_EQ(viewport.y, 16.0f);
  EXPECT_FLOAT_EQ(viewport.width, 128.0f);
  EXPECT_FLOAT_EQ(viewport.height, 112.0f);

  const MapRect marker = PointOnMap(area, 0x0A80, 0x0A60, 0.5f);
  EXPECT_FLOAT_EQ(marker.x, 64.0f);
  EXPECT_FLOAT_EQ(marker.y, 48.0f);

  EXPECT_TRUE(PointInViewport(0x0A40, 0x0A20, 0x0A80, 0x0A60));
  EXPECT_FALSE(PointInViewport(0x0A40, 0x0A20, 0x0A40 + 256, 0x0A60));
  EXPECT_FALSE(PointInViewport(0x0A40, 0x0A20, 0x0A80, 0x0A20 + 224));
}

TEST(CutsceneShotTest, AreaExtentFromZsCustomOverworldSizes) {
  const AreaExtent small = AreaExtentFromParent(0x2D, AreaSizeEnum::SmallArea);
  EXPECT_EQ(small.origin_x, 0x0A00);
  EXPECT_EQ(small.origin_y, 0x0A00);
  EXPECT_EQ(small.width, 512);
  EXPECT_EQ(small.height, 512);
  const AreaExtent large = AreaExtentFromParent(0x03, AreaSizeEnum::LargeArea);
  EXPECT_EQ(large.origin_x, 0x0600);
  EXPECT_EQ(large.width, 1024);
  EXPECT_EQ(large.height, 1024);
  const AreaExtent wide = AreaExtentFromParent(0x10, AreaSizeEnum::WideArea);
  EXPECT_EQ(wide.width, 1024);
  EXPECT_EQ(wide.height, 512);
  const AreaExtent tall = AreaExtentFromParent(0x10, AreaSizeEnum::TallArea);
  EXPECT_EQ(tall.width, 512);
  EXPECT_EQ(tall.height, 1024);
  // Dark World areas use the same grid as the Light World.
  const AreaExtent dark = AreaExtentFromParent(0x6D, AreaSizeEnum::SmallArea);
  EXPECT_EQ(dark.origin_x, small.origin_x);
  EXPECT_EQ(dark.origin_y, small.origin_y);
}

// The vanilla table matches what yaze loads from a vanilla ROM, area by area.
TEST(CutsceneShotRomTest, VanillaTableMatchesLoadedOverworld) {
  YAZE_SKIP_IF_ROM_MISSING(
      ::yaze::test::RomRole::kVanilla,
      "CutsceneShotRomTest.VanillaTableMatchesLoadedOverworld");
  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(::yaze::test::TestRomManager::GetRomPath(
                                   ::yaze::test::RomRole::kVanilla))
                  .ok());
  GameData game_data;
  ASSERT_TRUE(LoadGameData(rom, game_data).ok());
  Overworld overworld(&rom, &game_data);
  const auto load = overworld.Load(&rom);
  ASSERT_TRUE(load.ok()) << load;
  for (int area = 0; area < 0x80; ++area) {
    const auto* map = overworld.overworld_map(area);
    ASSERT_NE(map, nullptr);
    const AreaExtent loaded =
        AreaExtentFromParent(map->parent(), map->area_size());
    auto table = VanillaAreaExtent(area);
    ASSERT_TRUE(table.ok());
    EXPECT_EQ(loaded.origin_x, table->origin_x) << "area " << area;
    EXPECT_EQ(loaded.origin_y, table->origin_y) << "area " << area;
    EXPECT_EQ(loaded.width, table->width) << "area " << area;
    EXPECT_EQ(loaded.height, table->height) << "area " << area;
  }
}

// Oracle of Secrets areas from its ZSCustomOverworld tables, on a copy:
//   YAZE_ORACLE_ROOT=~/src/hobby/oracle-of-secrets
TEST(CutsceneShotRomTest, OracleAreasClampToTheirOwnSizes) {
  const char* oracle_env = std::getenv("YAZE_ORACLE_ROOT");
  if (oracle_env == nullptr) {
    GTEST_SKIP() << "Set YAZE_ORACLE_ROOT to run against an Oracle copy";
  }
  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(
                     (std::filesystem::path(oracle_env) / "Roms" / "oos168.sfc")
                         .string())
                  .ok());
  GameData game_data;
  ASSERT_TRUE(LoadGameData(rom, game_data).ok());
  Overworld overworld(&rom, &game_data);
  const auto load = overworld.Load(&rom);
  ASSERT_TRUE(load.ok()) << load;

  // $2D Tail Pond (the experiment scene): a small area.
  const auto* tail_pond = overworld.overworld_map(0x2D);
  ASSERT_NE(tail_pond, nullptr);
  EXPECT_EQ(tail_pond->area_size(), AreaSizeEnum::SmallArea);
  const AreaExtent pond =
      AreaExtentFromParent(tail_pond->parent(), tail_pond->area_size());
  const ClampedCamera pond_low = ClampCamera(pond, 0, 0);
  const ClampedCamera pond_high = ClampCamera(pond, 0xFFFF, 0xFFFF);
  EXPECT_EQ(pond_low.x, pond.origin_x);
  EXPECT_EQ(pond_high.x, pond.origin_x + 0x0100);
  EXPECT_EQ(pond_high.y, pond.origin_y + 0x011E);

  // $23 Wayward Village: a large area.
  const auto* village = overworld.overworld_map(0x23);
  ASSERT_NE(village, nullptr);
  EXPECT_EQ(village->area_size(), AreaSizeEnum::LargeArea);
  const AreaExtent town =
      AreaExtentFromParent(village->parent(), village->area_size());
  EXPECT_EQ(town.width, 1024);
  const ClampedCamera town_high = ClampCamera(town, 0xFFFF, 0xFFFF);
  EXPECT_EQ(town_high.x, town.origin_x + 0x0300);
  EXPECT_EQ(town_high.y, town.origin_y + 0x031E);
  printf("CUTSCENE $2D parent 0x%02X origin %d,%d camera x %d-%d y %d-%d\n",
         tail_pond->parent(), pond.origin_x, pond.origin_y, pond_low.x,
         pond_high.x, pond_low.y, pond_high.y);
  printf("CUTSCENE $23 parent 0x%02X origin %d,%d camera x %d-%d y %d-%d\n",
         village->parent(), town.origin_x, town.origin_y, town.origin_x,
         town_high.x, town.origin_y, town_high.y);
}

}  // namespace
}  // namespace yaze::zelda3
