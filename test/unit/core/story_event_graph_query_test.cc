#include "core/story_event_graph_query.h"

#include "gtest/gtest.h"

namespace yaze::core {

TEST(StoryEventGraphQueryTest, MatchesAcrossFieldsCaseInsensitiveAndTokenized) {
  StoryEventNode node;
  node.id = "EV-001";
  node.name = "Zora Baby Switch";
  node.status = StoryNodeStatus::kAvailable;

  StoryFlag flag;
  flag.name = "GameState";
  flag.value = "2";
  flag.reg = "GAMESTATE";
  node.flags.push_back(flag);

  StoryLocation loc;
  loc.name = "D4 Room";
  loc.room_id = "0x25";
  node.locations.push_back(loc);

  node.scripts.push_back("followers.asm:465");
  node.text_ids.push_back("0x1F");

  EXPECT_TRUE(StoryEventNodeMatchesQuery(node, "zora"));
  EXPECT_TRUE(StoryEventNodeMatchesQuery(node, "ZORA"));
  EXPECT_TRUE(StoryEventNodeMatchesQuery(node, "0x25"));
  EXPECT_TRUE(StoryEventNodeMatchesQuery(node, "followers.asm"));
  EXPECT_TRUE(StoryEventNodeMatchesQuery(node, "ev-001 switch"));

  EXPECT_FALSE(StoryEventNodeMatchesQuery(node, "0x27"));
  EXPECT_FALSE(StoryEventNodeMatchesQuery(node, "zora 0x27"));
}

TEST(StoryEventGraphQueryTest, RespectsStatusToggles) {
  StoryEventNode node;
  node.id = "EV-100";
  node.name = "Completed Event";
  node.status = StoryNodeStatus::kCompleted;

  StoryEventNodeFilter filter;
  filter.query = "";
  filter.include_completed = true;
  filter.include_available = false;
  filter.include_locked = false;
  filter.include_blocked = false;

  EXPECT_TRUE(StoryEventNodeMatchesFilter(node, filter));

  filter.include_completed = false;
  EXPECT_FALSE(StoryEventNodeMatchesFilter(node, filter));
}

}  // namespace yaze::core

