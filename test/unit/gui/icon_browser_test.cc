#include "app/gui/widgets/icon_browser.h"

#include <cstring>

#include <gtest/gtest.h>

#include "app/gui/core/common_icons.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {
namespace {

using icon_browser_internal::MatchesQuery;

TEST(CommonIconsCatalogTest, HasCuratedMinimumCount) {
  // Catalog is intentionally ~90-100 icons; guard the floor.
  EXPECT_GE(kCommonIconCount, 80);
  EXPECT_LE(kCommonIconCount, 200);
}

TEST(CommonIconsCatalogTest, SentinelTerminates) {
  EXPECT_EQ(kCommonIcons[kCommonIconCount].glyph, nullptr);
  EXPECT_EQ(kCommonIcons[kCommonIconCount].macro_name, nullptr);
}

TEST(CommonIconsCatalogTest, EntriesHaveNonEmptyFields) {
  for (int i = 0; i < kCommonIconCount; ++i) {
    const CommonIcon& ic = kCommonIcons[i];
    ASSERT_NE(ic.macro_name, nullptr) << "entry " << i;
    ASSERT_NE(ic.glyph, nullptr) << "entry " << i;
    ASSERT_NE(ic.category, nullptr) << "entry " << i;
    ASSERT_NE(ic.search_key, nullptr) << "entry " << i;
    EXPECT_GT(std::strlen(ic.macro_name), 0u) << "entry " << i;
    EXPECT_GT(std::strlen(ic.glyph), 0u) << "entry " << i;
    EXPECT_GT(std::strlen(ic.category), 0u) << "entry " << i;
    EXPECT_GT(std::strlen(ic.search_key), 0u) << "entry " << i;
  }
}

TEST(CommonIconsCatalogTest, CategoriesUseExpectedLabels) {
  const char* expected[] = {"actions", "navigation", "files",
                            "editing", "status",     "layout"};
  bool seen[6] = {false, false, false, false, false, false};
  for (int i = 0; i < kCommonIconCount; ++i) {
    for (int c = 0; c < 6; ++c) {
      if (std::strcmp(kCommonIcons[i].category, expected[c]) == 0) {
        seen[c] = true;
        break;
      }
    }
  }
  for (int c = 0; c < 6; ++c) {
    EXPECT_TRUE(seen[c]) << "category missing: " << expected[c];
  }
}

TEST(IconBrowserMatchesQueryTest, EmptyQueryMatchesAll) {
  EXPECT_TRUE(MatchesQuery("save disk", ""));
  EXPECT_TRUE(MatchesQuery("anything", nullptr));
}

TEST(IconBrowserMatchesQueryTest, CaseInsensitiveSubstring) {
  EXPECT_TRUE(MatchesQuery("save disk", "SAVE"));
  EXPECT_TRUE(MatchesQuery("save disk", "sAve"));
  EXPECT_TRUE(MatchesQuery("save disk", "disk"));
}

TEST(IconBrowserMatchesQueryTest, AllTermsMustMatch) {
  EXPECT_TRUE(MatchesQuery("save disk write", "save write"));
  EXPECT_FALSE(MatchesQuery("save disk write", "save fly"));
}

TEST(IconBrowserMatchesQueryTest, NonMatchReturnsFalse) {
  EXPECT_FALSE(MatchesQuery("save disk", "xyz"));
  EXPECT_FALSE(MatchesQuery(nullptr, "xyz"));
}

TEST(IconBrowserMatchesQueryTest, WhitespaceOnlyQueryMatchesAll) {
  EXPECT_TRUE(MatchesQuery("save disk", "   "));
  EXPECT_TRUE(MatchesQuery("save disk", "\t\n"));
}

}  // namespace
}  // namespace gui
}  // namespace yaze
