#include "core/story_event_graph.h"

#include "absl/status/status.h"
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
        { "script_id": "oos:d4:water_gate_opened" },
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
  ASSERT_EQ(node->scripts.size(), 5u);
  EXPECT_EQ(node->scripts[0], "followers.asm:123");
  EXPECT_EQ(node->scripts[1], "oos:d4:water_gate_opened");
  EXPECT_EQ(node->scripts[2], "followers.asm:ZoraBaby_PostSwitch");
  EXPECT_EQ(node->scripts[3], "GlobalHook");
  EXPECT_EQ(node->scripts[4], "followers.asm:456");
}

TEST(StoryEventGraphTest, PrefersStableNodeIdsAndCanonicalizesLegacyRefs) {
  constexpr const char* kJson = R"json(
{
  "events": [
    {
      "id": "EV-001",
      "stable_id": "oos:d4:intro",
      "name": "Intro",
      "unlocks": ["EV-002"]
    },
    {
      "id": "EV-002",
      "stable_id": "oos:d4:gate",
      "name": "Gate",
      "dependencies": ["EV-001"]
    }
  ],
  "edges": [
    { "from": "EV-001", "to": "EV-002", "type": "dependency" }
  ]
}
)json";

  StoryEventGraph graph;
  ASSERT_TRUE(graph.LoadFromString(kJson).ok());

  const auto* intro = graph.GetNode("oos:d4:intro");
  const auto* gate = graph.GetNode("oos:d4:gate");
  ASSERT_NE(intro, nullptr);
  ASSERT_NE(gate, nullptr);
  EXPECT_EQ(graph.GetNode("EV-001"), nullptr);
  EXPECT_EQ(graph.GetNode("EV-002"), nullptr);

  ASSERT_EQ(intro->unlocks.size(), 1u);
  EXPECT_EQ(intro->unlocks[0], "oos:d4:gate");
  ASSERT_EQ(gate->dependencies.size(), 1u);
  EXPECT_EQ(gate->dependencies[0], "oos:d4:intro");

  ASSERT_EQ(graph.edges().size(), 1u);
  EXPECT_EQ(graph.edges()[0].from, "oos:d4:intro");
  EXPECT_EQ(graph.edges()[0].to, "oos:d4:gate");
}

TEST(StoryEventGraphTest, RejectsUnknownDependencies) {
  constexpr const char* kJson = R"json(
{
  "events": [
    {
      "id": "A",
      "name": "Bad deps",
      "dependencies": ["MISSING"]
    }
  ],
  "edges": []
}
)json";

  StoryEventGraph graph;
  auto st = graph.LoadFromString(kJson);
  EXPECT_FALSE(st.ok());
  EXPECT_EQ(st.code(), absl::StatusCode::kInvalidArgument);
}

TEST(StoryEventGraphTest, RejectsDuplicateCanonicalIds) {
  constexpr const char* kJson = R"json(
{
  "events": [
    { "id": "EV-001", "stable_id": "oos:event:a", "name": "A" },
    { "id": "EV-002", "stable_id": "oos:event:a", "name": "B" }
  ],
  "edges": []
}
)json";

  StoryEventGraph graph;
  auto st = graph.LoadFromString(kJson);
  EXPECT_FALSE(st.ok());
  EXPECT_EQ(st.code(), absl::StatusCode::kInvalidArgument);
}

}  // namespace yaze::core
