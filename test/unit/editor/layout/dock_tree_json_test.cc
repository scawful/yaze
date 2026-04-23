#include "app/editor/layout/layout_designer/dock_tree_json.h"

#include <string>

#include <gtest/gtest.h>

#include "app/editor/layout/layout_designer/dock_tree.h"
#include "nlohmann/json.hpp"

namespace yaze {
namespace editor {
namespace layout_designer {
namespace {

using json = nlohmann::json;

PanelEntry MakePanel(const std::string& id) {
  return {id, id + " Display", "ICON_MD_FAVORITE"};
}

TEST(DockTreeJsonTest, EmptyTreeRoundTrip) {
  DockTree original("My Layout");
  original.description = "A description";

  json j = DockTreeToJson(original);
  auto parsed = DockTreeFromJson(j);
  ASSERT_TRUE(parsed.ok()) << parsed.status();

  EXPECT_EQ(parsed->name, "My Layout");
  EXPECT_EQ(parsed->description, "A description");
  EXPECT_EQ(parsed->schema_version, 1u);
  ASSERT_NE(parsed->root, nullptr);
  EXPECT_EQ(parsed->root->type, DockNode::Type::kLeaf);
  EXPECT_TRUE(parsed->root->panels.empty());
}

TEST(DockTreeJsonTest, LeafWithPanelsRoundTrip) {
  DockTree original("Leafy");
  original.root =
      DockNode::MakeLeaf({MakePanel("a"), MakePanel("b"), MakePanel("c")});
  original.root->active_tab_index = 2;

  json j = DockTreeToJson(original);
  auto parsed = DockTreeFromJson(j);
  ASSERT_TRUE(parsed.ok()) << parsed.status();

  ASSERT_EQ(parsed->root->panels.size(), 3u);
  EXPECT_EQ(parsed->root->panels[0].panel_id, "a");
  EXPECT_EQ(parsed->root->panels[0].display_name, "a Display");
  EXPECT_EQ(parsed->root->panels[0].icon, "ICON_MD_FAVORITE");
  EXPECT_EQ(parsed->root->active_tab_index, 2);
}

TEST(DockTreeJsonTest, SplitRoundTrip) {
  DockTree original("Split");
  original.root = DockNode::MakeSplit(
      SplitDirection::kUp, 0.37f, DockNode::MakeLeaf({MakePanel("a")}),
      DockNode::MakeLeaf({MakePanel("b"), MakePanel("c")}));

  json j = DockTreeToJson(original);
  auto parsed = DockTreeFromJson(j);
  ASSERT_TRUE(parsed.ok()) << parsed.status();

  ASSERT_EQ(parsed->root->type, DockNode::Type::kSplit);
  EXPECT_EQ(parsed->root->split_direction, SplitDirection::kUp);
  EXPECT_FLOAT_EQ(parsed->root->split_ratio, 0.37f);
  ASSERT_NE(parsed->root->child_a, nullptr);
  ASSERT_NE(parsed->root->child_b, nullptr);
  EXPECT_EQ(parsed->root->child_a->panels[0].panel_id, "a");
  ASSERT_EQ(parsed->root->child_b->panels.size(), 2u);
  EXPECT_EQ(parsed->root->child_b->panels[1].panel_id, "c");
}

TEST(DockTreeJsonTest, DeeplyNestedRoundTrip) {
  DockTree original("Deep");
  original.root = DockNode::MakeSplit(
      SplitDirection::kLeft, 0.3f,
      DockNode::MakeSplit(SplitDirection::kUp, 0.4f,
                          DockNode::MakeLeaf({MakePanel("a")}),
                          DockNode::MakeLeaf({MakePanel("b")})),
      DockNode::MakeSplit(
          SplitDirection::kDown, 0.5f, DockNode::MakeLeaf({MakePanel("c")}),
          DockNode::MakeLeaf({MakePanel("d"), MakePanel("e")})));

  json j = DockTreeToJson(original);
  auto parsed = DockTreeFromJson(j);
  ASSERT_TRUE(parsed.ok()) << parsed.status();

  std::string err;
  EXPECT_TRUE(parsed->Validate(&err)) << err;
  EXPECT_NE(parsed->root->FindPanel("a"), nullptr);
  EXPECT_NE(parsed->root->FindPanel("e"), nullptr);
}

TEST(DockTreeJsonTest, AllSplitDirectionsSerialize) {
  for (SplitDirection dir : {SplitDirection::kLeft, SplitDirection::kRight,
                             SplitDirection::kUp, SplitDirection::kDown}) {
    DockTree tree;
    tree.root =
        DockNode::MakeSplit(dir, 0.5f, DockNode::MakeLeaf({MakePanel("a")}),
                            DockNode::MakeLeaf({MakePanel("b")}));
    json j = DockTreeToJson(tree);
    auto parsed = DockTreeFromJson(j);
    ASSERT_TRUE(parsed.ok()) << j.dump();
    EXPECT_EQ(parsed->root->split_direction, dir);
  }
}

TEST(DockTreeJsonTest, UnknownNodeTypeRejected) {
  json j = {
      {"schema_version", 1},
      {"name", "bad"},
      {"root", {{"type", "pyramid"}, {"panels", json::array()}}},
  };
  auto parsed = DockTreeFromJson(j);
  EXPECT_FALSE(parsed.ok());
  EXPECT_NE(parsed.status().message().find("pyramid"), std::string::npos);
}

TEST(DockTreeJsonTest, UnknownDirectionRejected) {
  json j = {
      {"root",
       {{"type", "split"},
        {"direction", "sideways"},
        {"ratio", 0.5f},
        {"child_a", {{"type", "leaf"}}},
        {"child_b", {{"type", "leaf"}}}}},
  };
  auto parsed = DockTreeFromJson(j);
  EXPECT_FALSE(parsed.ok());
  EXPECT_NE(parsed.status().message().find("sideways"), std::string::npos);
}

TEST(DockTreeJsonTest, SplitMissingChildRejected) {
  json j = {
      {"root",
       {{"type", "split"},
        {"direction", "left"},
        {"ratio", 0.5f},
        {"child_a", {{"type", "leaf"}}}}},  // child_b missing
  };
  auto parsed = DockTreeFromJson(j);
  EXPECT_FALSE(parsed.ok());
}

TEST(DockTreeJsonTest, PanelMissingIdRejected) {
  json j = {
      {"root",
       {{"type", "leaf"},
        {"panels", json::array({{{"display_name", "no id"}}})}}},
  };
  auto parsed = DockTreeFromJson(j);
  EXPECT_FALSE(parsed.ok());
}

TEST(DockTreeJsonTest, NonObjectInputRejected) {
  auto parsed = DockTreeFromJson(json::array());
  EXPECT_FALSE(parsed.ok());
}

TEST(DockTreeJsonTest, UnknownFieldsIgnored) {
  json j = {
      {"schema_version", 1},
      {"name", "forward"},
      {"description", "x"},
      {"root",
       {{"type", "leaf"},
        {"panels",
         json::array({{{"panel_id", "p1"}, {"unknown_field", 42}}})}}},
      {"mystery_top_level_key", "hello"},
  };
  auto parsed = DockTreeFromJson(j);
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  EXPECT_EQ(parsed->name, "forward");
  ASSERT_EQ(parsed->root->panels.size(), 1u);
  EXPECT_EQ(parsed->root->panels[0].panel_id, "p1");
}

TEST(DockTreeJsonTest, MissingOptionalFieldsDefault) {
  json j = {{"root", {{"type", "leaf"}}}};
  auto parsed = DockTreeFromJson(j);
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  EXPECT_EQ(parsed->name, "");
  EXPECT_EQ(parsed->description, "");
  EXPECT_EQ(parsed->schema_version, 1u);
  ASSERT_NE(parsed->root, nullptr);
  EXPECT_EQ(parsed->root->type, DockNode::Type::kLeaf);
  EXPECT_TRUE(parsed->root->panels.empty());
}

TEST(DockTreeJsonTest, MissingRootUsesDefaultEmptyLeaf) {
  json j = {{"name", "rootless"}};
  auto parsed = DockTreeFromJson(j);
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  ASSERT_NE(parsed->root, nullptr);
  EXPECT_EQ(parsed->root->type, DockNode::Type::kLeaf);
  EXPECT_TRUE(parsed->root->panels.empty());
}

}  // namespace
}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze
