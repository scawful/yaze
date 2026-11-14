#include "zelda3/overworld/overworld_grouping_preflight.h"

#include <gtest/gtest.h>

#include "nlohmann/json.hpp"

namespace yaze::zelda3 {
namespace {

GroupingMapSnapshot Snap(int map_id, int parent, AreaSizeEnum size,
                         uint8_t gfx = 1, uint8_t pal = 2, uint8_t main = 3,
                         uint8_t anim = 4, uint16_t overlay = 0x00FF) {
  GroupingMapSnapshot snap;
  snap.map_id = map_id;
  snap.parent = parent;
  snap.size = size;
  snap.area_graphics = gfx;
  snap.area_palette = pal;
  snap.main_palette = main;
  snap.animated_gfx = anim;
  snap.subscreen_overlay = overlay;
  return snap;
}

TEST(OverworldGroupingPreflightTest, TallFootprint90Plus98) {
  auto maps = ProposedGroupingFootprint(0x90, AreaSizeEnum::TallArea);
  ASSERT_TRUE(maps.ok()) << maps.status();
  EXPECT_EQ(*maps, (std::vector<int>{0x90, 0x98}));
}

TEST(OverworldGroupingPreflightTest, WideFootprintA0PlusA1) {
  auto maps = ProposedGroupingFootprint(0xA0, AreaSizeEnum::WideArea);
  ASSERT_TRUE(maps.ok()) << maps.status();
  EXPECT_EQ(*maps, (std::vector<int>{0xA0, 0xA1}));
}

TEST(OverworldGroupingPreflightTest, CompatibleTallPassesWithContactLabels) {
  GroupingPreflightRequest request;
  request.parent = 0x90;
  request.size = AreaSizeEnum::TallArea;
  request.members = {0x90, 0x98};
  request.contacts = {{"beach_99_98", "99↔98 beach"},
                      {"no_99_a1", "no direct 99↔A1 beach"}};
  request.current = {Snap(0x90, 0x90, AreaSizeEnum::SmallArea),
                     Snap(0x98, 0x98, AreaSizeEnum::SmallArea)};

  auto report = BuildGroupingPreflightReport(request);
  ASSERT_TRUE(report.ok()) << report.status();
  EXPECT_TRUE(report->ok);
  EXPECT_TRUE(report->blocking.empty());
  ASSERT_EQ(report->contacts.size(), 2u);
  EXPECT_FALSE(report->warnings.empty());  // contact disclaimer
  EXPECT_NE(report->contact_disclaimer.find("not collision proof"),
            std::string::npos);
}

TEST(OverworldGroupingPreflightTest, WrongMembersFail) {
  GroupingPreflightRequest request;
  request.parent = 0x90;
  request.size = AreaSizeEnum::TallArea;
  request.members = {0x90, 0x91};  // not the tall footprint
  request.current = {Snap(0x90, 0x90, AreaSizeEnum::SmallArea),
                     Snap(0x91, 0x91, AreaSizeEnum::SmallArea),
                     Snap(0x98, 0x98, AreaSizeEnum::SmallArea)};
  auto report = BuildGroupingPreflightReport(request);
  ASSERT_TRUE(report.ok()) << report.status();
  EXPECT_FALSE(report->ok);
  ASSERT_FALSE(report->blocking.empty());
  EXPECT_NE(report->blocking[0].find("do not match"), std::string::npos);
}

TEST(OverworldGroupingPreflightTest, ForeignParentRefused) {
  GroupingPreflightRequest request;
  request.parent = 0x90;
  request.size = AreaSizeEnum::TallArea;
  request.members = {0x90, 0x98};
  // $98 already belongs to a different multi-area parent $88.
  request.current = {Snap(0x90, 0x90, AreaSizeEnum::SmallArea),
                     Snap(0x98, 0x88, AreaSizeEnum::TallArea)};
  auto report = BuildGroupingPreflightReport(request);
  ASSERT_TRUE(report.ok()) << report.status();
  EXPECT_FALSE(report->ok);
  ASSERT_EQ(report->members.size(), 2u);
  EXPECT_TRUE(report->members[1].foreign_parent);
  ASSERT_FALSE(report->blocking.empty());
  EXPECT_NE(report->blocking[0].find("foreign parent"), std::string::npos);
}

TEST(OverworldGroupingPreflightTest, ProfileMismatchIsWarningNotBlocker) {
  GroupingPreflightRequest request;
  request.parent = 0xA0;
  request.size = AreaSizeEnum::WideArea;
  request.members = {0xA0, 0xA1};
  request.current = {Snap(0xA0, 0xA0, AreaSizeEnum::SmallArea, /*gfx=*/0x0A),
                     Snap(0xA1, 0xA1, AreaSizeEnum::SmallArea, /*gfx=*/0x02)};
  auto report = BuildGroupingPreflightReport(request);
  ASSERT_TRUE(report.ok()) << report.status();
  EXPECT_TRUE(report->ok);
  ASSERT_EQ(report->profile_mismatches.size(), 1u);
  EXPECT_EQ(report->profile_mismatches[0].field, "area_graphics");
  EXPECT_FALSE(report->warnings.empty());
}

TEST(OverworldGroupingPreflightTest, JsonMarksContactsNotCollisionProof) {
  GroupingPreflightRequest request;
  request.parent = 0x90;
  request.size = AreaSizeEnum::TallArea;
  request.members = {0x90, 0x98};
  request.contacts = {{"beach_99_98", "99↔98 beach"}};
  request.current = {Snap(0x90, 0x90, AreaSizeEnum::SmallArea),
                     Snap(0x98, 0x98, AreaSizeEnum::SmallArea)};
  auto report = BuildGroupingPreflightReport(request);
  ASSERT_TRUE(report.ok());
  const auto json = nlohmann::json::parse(GroupingPreflightToJson(*report));
  EXPECT_EQ(json["schema"], "yaze.overworld-grouping-preflight.v1");
  EXPECT_TRUE(json["ok"]);
  EXPECT_FALSE(json["contacts"][0]["collision_proof"]);
}

}  // namespace
}  // namespace yaze::zelda3
