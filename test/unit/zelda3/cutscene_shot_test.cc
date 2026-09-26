#include "zelda3/cutscene/cutscene_shot.h"

#include <gtest/gtest.h>

#include <string>

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

}  // namespace
}  // namespace yaze::zelda3
