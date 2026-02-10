#include "core/story_event_graph.h"

#include "gtest/gtest.h"

namespace yaze::core {

TEST(StoryEventGraphTest, EvaluatesCompletedWhenPredicates) {
  constexpr const char* kJson = R"json(
{
  "events": [
    {
      "id": "A",
      "name": "Start",
      "completed_when": [
        { "register": "GameState", "op": ">=", "value": 1 }
      ],
      "dependencies": [],
      "unlocks": ["B"]
    },
    {
      "id": "B",
      "name": "Bit Event",
      "completed_when": [
        { "register": "OOSPROG", "op": "bit_set", "bit": 0 }
      ],
      "dependencies": ["A"]
    }
  ],
  "edges": [
    { "from": "A", "to": "B", "type": "dependency" }
  ]
}
)json";

  StoryEventGraph graph;
  ASSERT_TRUE(graph.LoadFromString(kJson).ok());
  graph.AutoLayout();

  OracleProgressionState state;

  state.game_state = 0;
  state.oosprog = 0;
  graph.UpdateStatus(state);
  ASSERT_NE(graph.GetNode("A"), nullptr);
  ASSERT_NE(graph.GetNode("B"), nullptr);
  EXPECT_EQ(graph.GetNode("A")->status, StoryNodeStatus::kAvailable);
  EXPECT_EQ(graph.GetNode("B")->status, StoryNodeStatus::kLocked);

  state.game_state = 1;
  state.oosprog = 0;
  graph.UpdateStatus(state);
  EXPECT_EQ(graph.GetNode("A")->status, StoryNodeStatus::kCompleted);
  EXPECT_EQ(graph.GetNode("B")->status, StoryNodeStatus::kAvailable);

  state.game_state = 1;
  state.oosprog = 0x01;  // bit0 set
  graph.UpdateStatus(state);
  EXPECT_EQ(graph.GetNode("B")->status, StoryNodeStatus::kCompleted);
}

TEST(StoryEventGraphTest, EvaluatesMaskPredicates) {
  constexpr const char* kJson = R"json(
{
  "events": [
    {
      "id": "C",
      "name": "Crystal D4",
      "completed_when": [
        { "register": "crystals", "op": "mask_any", "mask": "0x20" }
      ]
    }
  ],
  "edges": []
}
)json";

  StoryEventGraph graph;
  ASSERT_TRUE(graph.LoadFromString(kJson).ok());

  OracleProgressionState state;
  state.crystal_bitfield = 0x00;
  graph.UpdateStatus(state);
  ASSERT_NE(graph.GetNode("C"), nullptr);
  EXPECT_EQ(graph.GetNode("C")->status, StoryNodeStatus::kAvailable);

  state.crystal_bitfield = 0x20;
  graph.UpdateStatus(state);
  EXPECT_EQ(graph.GetNode("C")->status, StoryNodeStatus::kCompleted);
}

TEST(StoryEventGraphTest, ParsesScriptRefsFromObjects) {
  constexpr const char* kJson = R"json(
{
  "events": [
    {
      "id": "A",
      "name": "Scripts",
      "scripts": [
        "followers.asm:123",
        { "file": "followers.asm", "symbol": "ZoraBaby_PostSwitch" },
        { "symbol": "GlobalHook" },
        { "file": "followers.asm", "line": 456 }
      ]
    }
  ],
  "edges": []
}
)json";

  StoryEventGraph graph;
  ASSERT_TRUE(graph.LoadFromString(kJson).ok());
  const auto* node = graph.GetNode("A");
  ASSERT_NE(node, nullptr);
  ASSERT_EQ(node->scripts.size(), 4u);
  EXPECT_EQ(node->scripts[0], "followers.asm:123");
  EXPECT_EQ(node->scripts[1], "followers.asm:ZoraBaby_PostSwitch");
  EXPECT_EQ(node->scripts[2], "GlobalHook");
  EXPECT_EQ(node->scripts[3], "followers.asm:456");
}

}  // namespace yaze::core
