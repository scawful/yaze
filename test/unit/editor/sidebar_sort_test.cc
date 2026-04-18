#include "app/editor/menu/activity_bar.h"

#include <string>
#include <unordered_set>
#include <vector>

#include "gtest/gtest.h"

namespace yaze::editor {
namespace {

// Alias for brevity.
using Vec = std::vector<std::string>;
using Set = std::unordered_set<std::string>;

TEST(SidebarSortTest, IdentityWhenNoPrefs) {
  Vec input = {"Overworld", "Dungeon", "Graphics"};
  auto out = ActivityBar::SortCategories(input, /*order=*/{}, /*pinned=*/{},
                                         /*hidden=*/{});
  EXPECT_EQ(out, input);
}

TEST(SidebarSortTest, HiddenCategoriesAreFilteredOut) {
  Vec input = {"Overworld", "Dungeon", "Graphics"};
  Set hidden = {"Dungeon"};
  auto out = ActivityBar::SortCategories(input, /*order=*/{}, /*pinned=*/{},
                                         hidden);
  EXPECT_EQ(out, Vec({"Overworld", "Graphics"}));
}

TEST(SidebarSortTest, PinnedRendersFirstInInputOrder) {
  Vec input = {"Overworld", "Dungeon", "Graphics", "Palette"};
  Set pinned = {"Graphics", "Overworld"};
  auto out = ActivityBar::SortCategories(input, /*order=*/{}, pinned,
                                         /*hidden=*/{});
  // Pinned preserves input order (Overworld before Graphics because Overworld
  // appears first in the input), not the pinned-set's iteration order.
  ASSERT_EQ(out.size(), 4u);
  EXPECT_EQ(out[0], "Overworld");
  EXPECT_EQ(out[1], "Graphics");
}

TEST(SidebarSortTest, OrderedSubsetRespectsUserOrder) {
  Vec input = {"Overworld", "Dungeon", "Graphics", "Palette"};
  Vec order = {"Palette", "Dungeon", "Graphics", "Overworld"};
  auto out = ActivityBar::SortCategories(input, order, /*pinned=*/{},
                                         /*hidden=*/{});
  EXPECT_EQ(out, Vec({"Palette", "Dungeon", "Graphics", "Overworld"}));
}

TEST(SidebarSortTest, NewcomersSortAlphabeticallyAfterOrdered) {
  // "Sprite" and "Music" weren't in the saved order — they arrived after a
  // build upgrade added new categories.
  Vec input = {"Overworld", "Dungeon", "Sprite", "Music"};
  Vec order = {"Dungeon", "Overworld"};  // Only covers the old two.
  auto out = ActivityBar::SortCategories(input, order, /*pinned=*/{},
                                         /*hidden=*/{});
  EXPECT_EQ(out, Vec({"Dungeon", "Overworld", "Music", "Sprite"}));
}

TEST(SidebarSortTest, PinnedAndOrderedDontDoubleCount) {
  // Entries that appear in both `pinned` and `order` render as pinned,
  // never again later in the output.
  Vec input = {"Overworld", "Dungeon", "Graphics"};
  Vec order = {"Dungeon", "Graphics", "Overworld"};
  Set pinned = {"Graphics"};
  auto out = ActivityBar::SortCategories(input, order, pinned, /*hidden=*/{});
  // Pinned: Graphics. Ordered (pinned-excluded): Dungeon, Overworld.
  EXPECT_EQ(out, Vec({"Graphics", "Dungeon", "Overworld"}));
}

TEST(SidebarSortTest, HiddenWinsOverPinned) {
  // If the user both pinned and hid a category, hidden wins — pinning does
  // not re-show a hidden entry. This matches the menu's precedence.
  Vec input = {"Overworld", "Dungeon"};
  Set pinned = {"Dungeon"};
  Set hidden = {"Dungeon"};
  auto out = ActivityBar::SortCategories(input, /*order=*/{}, pinned, hidden);
  EXPECT_EQ(out, Vec({"Overworld"}));
}

TEST(SidebarSortTest, EmptyInputYieldsEmptyOutput) {
  Vec input;
  Set pinned = {"Graphics"};  // Pinned only matters if present in input.
  auto out = ActivityBar::SortCategories(input, /*order=*/{}, pinned,
                                         /*hidden=*/{});
  EXPECT_TRUE(out.empty());
}

TEST(SidebarSortTest, OrderEntriesNotInInputAreIgnored) {
  // Stale category names in `order` (e.g., removed in a later build) must
  // not appear in the output — `input` is authoritative.
  Vec input = {"Overworld", "Dungeon"};
  Vec order = {"DeletedCategory", "Dungeon", "Overworld"};
  auto out = ActivityBar::SortCategories(input, order, /*pinned=*/{},
                                         /*hidden=*/{});
  EXPECT_EQ(out, Vec({"Dungeon", "Overworld"}));
}

}  // namespace
}  // namespace yaze::editor
