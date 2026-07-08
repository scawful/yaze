#include "app/editor/layout/layout_designer/panel_palette.h"

#include <gtest/gtest.h>

namespace yaze {
namespace editor {
namespace layout_designer {
namespace {

PanelPaletteEntry MakeEntry(const std::string& id, const std::string& name,
                            const std::string& category = "System") {
  PanelPaletteEntry e;
  e.panel_id = id;
  e.display_name = name;
  e.icon = "ICON_MD_FAVORITE";
  e.category = category;
  return e;
}

TEST(PanelPaletteTest, MatchesQueryEmptyMatchesAll) {
  EXPECT_TRUE(
      panel_palette_internal::MatchesQuery(MakeEntry("x.y", "Anything"), ""));
}

TEST(PanelPaletteTest, MatchesQueryIsCaseInsensitive) {
  const PanelPaletteEntry e =
      MakeEntry("dungeon.room_selector", "Room Selector", "Dungeon");
  EXPECT_TRUE(panel_palette_internal::MatchesQuery(e, "room"));
  EXPECT_TRUE(panel_palette_internal::MatchesQuery(e, "ROOM"));
  EXPECT_TRUE(panel_palette_internal::MatchesQuery(e, "Selector"));
}

TEST(PanelPaletteTest, MatchesQueryAllTermsMustMatch) {
  const PanelPaletteEntry e =
      MakeEntry("dungeon.room_selector", "Room Selector", "Dungeon");
  // All terms match.
  EXPECT_TRUE(panel_palette_internal::MatchesQuery(e, "room selector"));
  EXPECT_TRUE(panel_palette_internal::MatchesQuery(e, "dungeon room"));
  // One missing term → no match.
  EXPECT_FALSE(panel_palette_internal::MatchesQuery(e, "room overworld"));
}

TEST(PanelPaletteTest, MatchesQuerySearchesIdAndCategory) {
  const PanelPaletteEntry e =
      MakeEntry("overworld.tile16", "Tile 16 Selector", "Overworld");
  // Query matches panel_id fragment.
  EXPECT_TRUE(panel_palette_internal::MatchesQuery(e, "tile16"));
  // Query matches category.
  EXPECT_TRUE(panel_palette_internal::MatchesQuery(e, "overworld"));
}

}  // namespace
}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze
