#include "app/editor/overworld/ui/navigation/overworld_sidebar.h"

#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "app/editor/overworld/maps/map_properties.h"
#include "rom/rom.h"
#include "unique_temp_path.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::editor {
namespace {

TEST(OverworldTile16ReferenceSidebarTest,
     HoverThenLeaveCanvasKeepsSelectionForPin) {
  Rom rom;
  zelda3::Overworld overworld(&rom);
  MapPropertiesSystem props(&overworld, &rom);
  OverworldSidebar sidebar(&overworld, &rom, &props);

  sidebar.SetTile16ReferenceHover(0x0D, 13, 5);
  EXPECT_TRUE(sidebar.tile16_reference_has_selection());
  EXPECT_TRUE(sidebar.tile16_reference_live_hover());
  EXPECT_EQ(sidebar.tile16_reference_map_for_test(), 0x0D);
  EXPECT_EQ(sidebar.tile16_reference_x_for_test(), 13);
  EXPECT_EQ(sidebar.tile16_reference_y_for_test(), 5);

  // Moving into the sidebar clears live hover but must keep the selection so
  // Pin can lock the same tile.
  sidebar.ClearTile16ReferenceHover();
  EXPECT_FALSE(sidebar.tile16_reference_live_hover());
  EXPECT_TRUE(sidebar.tile16_reference_has_selection());
  EXPECT_EQ(sidebar.tile16_reference_map_for_test(), 0x0D);
  EXPECT_EQ(sidebar.tile16_reference_x_for_test(), 13);
  EXPECT_EQ(sidebar.tile16_reference_y_for_test(), 5);

  sidebar.SetTile16ReferencePinned(true);
  EXPECT_TRUE(sidebar.tile16_reference_pinned());
  sidebar.SetTile16ReferenceHover(0x0D, 16, 9);
  EXPECT_EQ(sidebar.tile16_reference_x_for_test(), 13);
  EXPECT_EQ(sidebar.tile16_reference_y_for_test(), 5);
}

TEST(OverworldTile16ReferenceSidebarTest,
     WorldContextInvalidationClearsSelection) {
  Rom rom;
  zelda3::Overworld overworld(&rom);
  MapPropertiesSystem props(&overworld, &rom);
  OverworldSidebar sidebar(&overworld, &rom, &props);

  sidebar.SetTile16ReferenceHover(0x0D, 13, 5);
  sidebar.ClearTile16ReferenceHover();
  sidebar.SetTile16ReferencePinned(true);
  sidebar.InvalidateTile16ReferenceContext();
  EXPECT_FALSE(sidebar.tile16_reference_has_selection());
  EXPECT_FALSE(sidebar.tile16_reference_pinned());
  EXPECT_FALSE(sidebar.tile16_reference_live_hover());
}

TEST(OverworldTile16ReferenceSidebarTest, PolicyLoadsOnCommitNotEveryCall) {
  Rom rom;
  zelda3::Overworld overworld(&rom);
  MapPropertiesSystem props(&overworld, &rom);
  OverworldSidebar sidebar(&overworld, &rom, &props);

  const auto path = test::UniqueTempPath("tile16_ref_sidebar_policy", ".json");
  {
    std::ofstream out(path);
    ASSERT_TRUE(out) << path;
    out << R"json({"rules":[{"id":"t","classification":"confirmed_error",
      "map":"0D","before":[[13,5,"0CE"]]}]})json";
  }

  sidebar.SetTile16ReferencePolicyPathForTest(path.string());
  EXPECT_EQ(sidebar.tile16_reference_policy_load_count_for_test(), 1);
  EXPECT_TRUE(sidebar.tile16_reference_policy_loaded_for_test());
  EXPECT_TRUE(sidebar.tile16_reference_policy_error_for_test().empty());

  // Same committed path must not re-read disk.
  sidebar.ReloadTile16ReferencePolicyForTest();
  EXPECT_EQ(sidebar.tile16_reference_policy_load_count_for_test(), 2);

  sidebar.SetTile16ReferencePolicyPathForTest("/no/such/policy.json");
  EXPECT_EQ(sidebar.tile16_reference_policy_load_count_for_test(), 3);
  EXPECT_FALSE(sidebar.tile16_reference_policy_loaded_for_test());
  EXPECT_FALSE(sidebar.tile16_reference_policy_error_for_test().empty());
}

}  // namespace
}  // namespace yaze::editor
