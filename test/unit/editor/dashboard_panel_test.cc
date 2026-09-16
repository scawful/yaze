// Pins the dashboard's recent-editors parsing rules.
//
// The dashboard (the editor chooser reached on ROM load and via Ctrl+E) had no
// tests at all, and its recents parser carried two sentinel/bound bugs plus a
// failure mode where one malformed line truncated the rest of the list. These
// rules are cheap to get wrong again, so they are pinned here rather than left
// to a visual check — the parser is deliberately pure and static so none of
// this touches the user's real config directory.

#include <string>
#include <vector>

#include "app/editor/editor.h"
#include "app/editor/shell/windows/dashboard_panel.h"
#include "gtest/gtest.h"

namespace yaze::editor {
namespace {

constexpr size_t kMax = 5;

std::vector<EditorType> Parse(const std::string& body, size_t max = kMax) {
  return DashboardPanel::ParseRecentEditors(body, max);
}

std::string Line(EditorType type) {
  return std::to_string(static_cast<int>(type)) + "\n";
}

TEST(DashboardRecentEditorsTest, RoundTripsInFileOrder) {
  const auto parsed =
      Parse(Line(EditorType::kOverworld) + Line(EditorType::kDungeon) +
            Line(EditorType::kGraphics));
  ASSERT_EQ(parsed.size(), 3u);
  EXPECT_EQ(parsed[0], EditorType::kOverworld);
  EXPECT_EQ(parsed[1], EditorType::kDungeon);
  EXPECT_EQ(parsed[2], EditorType::kGraphics);
}

// kUnknown is the sentinel the layout system keys on, not a selectable editor.
// The old lower bound was `type_int >= 0`, which admitted it.
TEST(DashboardRecentEditorsTest, RejectsTheUnknownSentinel) {
  const auto parsed =
      Parse(Line(EditorType::kUnknown) + Line(EditorType::kDungeon));
  ASSERT_EQ(parsed.size(), 1u);
  EXPECT_EQ(parsed[0], EditorType::kDungeon);
}

// kSettings is a real editor and the last enumerator. The old upper bound was
// `type_int < kSettings`, which excluded it: Settings could never become a
// recent no matter how often it was opened.
TEST(DashboardRecentEditorsTest, AcceptsTheLastEditorType) {
  const auto parsed = Parse(Line(EditorType::kSettings));
  ASSERT_EQ(parsed.size(), 1u);
  EXPECT_EQ(parsed[0], EditorType::kSettings);
}

TEST(DashboardRecentEditorsTest, RejectsOutOfRangeValues) {
  const std::string body =
      "-1\n999\n" +
      std::to_string(static_cast<int>(EditorType::kSettings) + 1) + "\n" +
      Line(EditorType::kMusic);
  const auto parsed = Parse(body);
  ASSERT_EQ(parsed.size(), 1u);
  EXPECT_EQ(parsed[0], EditorType::kMusic);
}

// One bad line used to abandon every entry after it: std::stoi threw and a
// single try/catch around the whole loop swallowed the rest of the file.
TEST(DashboardRecentEditorsTest, AMalformedLineDropsOnlyThatLine) {
  const auto parsed = Parse(Line(EditorType::kOverworld) + "not-a-number\n" +
                            "\n" + Line(EditorType::kSprite));
  ASSERT_EQ(parsed.size(), 2u);
  EXPECT_EQ(parsed[0], EditorType::kOverworld);
  EXPECT_EQ(parsed[1], EditorType::kSprite);
}

// Recents are a set. A duplicated line must not consume two of the five slots.
TEST(DashboardRecentEditorsTest, DeduplicatesRepeatedEntries) {
  const auto parsed =
      Parse(Line(EditorType::kDungeon) + Line(EditorType::kDungeon) +
            Line(EditorType::kPalette));
  ASSERT_EQ(parsed.size(), 2u);
  EXPECT_EQ(parsed[0], EditorType::kDungeon);
  EXPECT_EQ(parsed[1], EditorType::kPalette);
}

TEST(DashboardRecentEditorsTest, StopsAtTheEntryCap) {
  const std::string body =
      Line(EditorType::kAssembly) + Line(EditorType::kDungeon) +
      Line(EditorType::kEmulator) + Line(EditorType::kGraphics) +
      Line(EditorType::kMusic) + Line(EditorType::kOverworld);
  const auto parsed = Parse(body, /*max=*/kMax);
  EXPECT_EQ(parsed.size(), kMax);
  EXPECT_EQ(parsed.back(), EditorType::kMusic);
}

TEST(DashboardRecentEditorsTest, EmptyBodyYieldsNoRecents) {
  EXPECT_TRUE(Parse("").empty());
  EXPECT_TRUE(Parse("\n\n\n").empty());
}

}  // namespace
}  // namespace yaze::editor
