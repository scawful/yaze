#include "app/editor/message/message_list_query.h"

#include "gtest/gtest.h"

namespace yaze::editor {
namespace {

TEST(MessageListQueryTest, EmptyQueryKeepsEveryBank) {
  EXPECT_TRUE(MessageListQueryMatches(1, 0x0E0000, "Hello", false,
                                      MessageBankFilter::kAll, ""));
  EXPECT_TRUE(MessageListQueryMatches(400, 0x100000, "More", true,
                                      MessageBankFilter::kAll, ""));
}

TEST(MessageListQueryTest, BankFilterHidesTheOtherBank) {
  EXPECT_FALSE(MessageListQueryMatches(400, 0x100000, "More", true,
                                       MessageBankFilter::kVanilla, ""));
  EXPECT_TRUE(MessageListQueryMatches(400, 0x100000, "More", true,
                                      MessageBankFilter::kExpanded, ""));
  EXPECT_FALSE(MessageListQueryMatches(1, 0x0E0000, "Hello", false,
                                       MessageBankFilter::kExpanded, ""));
}

TEST(MessageListQueryTest, MatchesTextIdAndAddress) {
  EXPECT_TRUE(MessageListQueryMatches(0x42, 0x0E1234, "Link saved Hyrule",
                                      false, MessageBankFilter::kAll,
                                      "hyrule"));
  EXPECT_TRUE(MessageListQueryMatches(0x42, 0x0E1234, "Link", false,
                                      MessageBankFilter::kAll, "66"));
  EXPECT_TRUE(MessageListQueryMatches(0x42, 0x0E1234, "Link", false,
                                      MessageBankFilter::kAll, "$42"));
  EXPECT_TRUE(MessageListQueryMatches(0x42, 0x0E1234, "Link", false,
                                      MessageBankFilter::kAll, "0xE1234"));
  EXPECT_FALSE(MessageListQueryMatches(0x42, 0x0E1234, "Link", false,
                                       MessageBankFilter::kAll, "ganon"));
}

TEST(MessageListQueryTest, FindNextRespectsCaseAndWholeWord) {
  const std::string_view text = "The cat catalogs a cat";
  auto loose = FindNextTextMatch(text, "cat", 0, false, false);
  ASSERT_TRUE(loose.has_value());
  EXPECT_EQ(*loose, 4u);

  auto next = FindNextTextMatch(text, "cat", *loose + 1, false, false);
  ASSERT_TRUE(next.has_value());
  EXPECT_EQ(*next, 8u);

  auto whole = FindNextTextMatch(text, "cat", 0, false, true);
  ASSERT_TRUE(whole.has_value());
  EXPECT_EQ(*whole, 4u);
  auto whole_next = FindNextTextMatch(text, "cat", *whole + 1, false, true);
  ASSERT_TRUE(whole_next.has_value());
  EXPECT_EQ(*whole_next, 19u);

  EXPECT_FALSE(FindNextTextMatch("Link", "link", 0, true, false).has_value());
  EXPECT_TRUE(FindNextTextMatch("Link", "link", 0, false, false).has_value());
}

TEST(MessageListQueryTest, FindPreviousStopsBeforeCursor) {
  const std::string_view text = "cat and cat";
  auto previous = FindPreviousTextMatch(text, "cat", 8, false, true);
  ASSERT_TRUE(previous.has_value());
  EXPECT_EQ(*previous, 0u);

  auto last =
      FindPreviousTextMatch(text, "cat", std::string::npos, false, true);
  ASSERT_TRUE(last.has_value());
  EXPECT_EQ(*last, 8u);

  EXPECT_FALSE(FindPreviousTextMatch(text, "cat", 0, false, true).has_value());
  EXPECT_FALSE(FindNextTextMatch(text, "", 0, false, false).has_value());
}

TEST(MessageListQueryTest, PreviewKeepsTheFirstLineShort) {
  EXPECT_EQ(MessageListPreview("Hello\nWorld", 96), "Hello");
  EXPECT_EQ(MessageListPreview("abcdef", 4), "abcd...");
}

}  // namespace
}  // namespace yaze::editor
