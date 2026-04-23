#include "app/editor/layout/layout_designer/dock_tree.h"

#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace yaze {
namespace editor {
namespace layout_designer {
namespace {

PanelEntry MakePanel(const std::string& id) {
  return {id, id + " Display", "ICON_MD_FAVORITE"};
}

TEST(DockNodeTest, DefaultIsEmptyLeaf) {
  DockNode n;
  EXPECT_EQ(n.type, DockNode::Type::kLeaf);
  EXPECT_TRUE(n.panels.empty());
  EXPECT_EQ(n.active_tab_index, 0);
}

TEST(DockNodeTest, MakeLeafCarriesPanels) {
  auto n = DockNode::MakeLeaf({MakePanel("a"), MakePanel("b")});
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(n->type, DockNode::Type::kLeaf);
  ASSERT_EQ(n->panels.size(), 2u);
  EXPECT_EQ(n->panels[0].panel_id, "a");
  EXPECT_EQ(n->panels[1].panel_id, "b");
}

TEST(DockNodeTest, MakeSplitCarriesChildren) {
  auto s = DockNode::MakeSplit(SplitDirection::kLeft, 0.3f,
                               DockNode::MakeLeaf({MakePanel("a")}),
                               DockNode::MakeLeaf({MakePanel("b")}));
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->type, DockNode::Type::kSplit);
  EXPECT_EQ(s->split_direction, SplitDirection::kLeft);
  EXPECT_FLOAT_EQ(s->split_ratio, 0.3f);
  ASSERT_NE(s->child_a, nullptr);
  ASSERT_NE(s->child_b, nullptr);
  EXPECT_EQ(s->child_a->panels[0].panel_id, "a");
  EXPECT_EQ(s->child_b->panels[0].panel_id, "b");
}

TEST(DockNodeTest, CloneIsDeep) {
  auto original = DockNode::MakeSplit(SplitDirection::kUp, 0.4f,
                                      DockNode::MakeLeaf({MakePanel("left")}),
                                      DockNode::MakeLeaf({MakePanel("right")}));
  auto copy = original->Clone();
  original->child_a->panels.clear();
  original->split_ratio = 0.9f;
  // Copy is untouched.
  ASSERT_EQ(copy->child_a->panels.size(), 1u);
  EXPECT_EQ(copy->child_a->panels[0].panel_id, "left");
  EXPECT_FLOAT_EQ(copy->split_ratio, 0.4f);
}

TEST(DockNodeTest, SplitInPlaceMigratesExistingPanels) {
  auto n = DockNode::MakeLeaf({MakePanel("a"), MakePanel("b")});
  n->active_tab_index = 1;
  n->SplitInPlace(SplitDirection::kLeft, 0.3f,
                  DockNode::MakeLeaf({MakePanel("q")}),
                  /*new_child_first=*/true);
  EXPECT_EQ(n->type, DockNode::Type::kSplit);
  EXPECT_EQ(n->split_direction, SplitDirection::kLeft);
  EXPECT_FLOAT_EQ(n->split_ratio, 0.3f);
  ASSERT_NE(n->child_a, nullptr);
  ASSERT_NE(n->child_b, nullptr);
  EXPECT_EQ(n->child_a->panels.size(), 1u);
  EXPECT_EQ(n->child_a->panels[0].panel_id, "q");
  ASSERT_EQ(n->child_b->panels.size(), 2u);
  EXPECT_EQ(n->child_b->panels[0].panel_id, "a");
  EXPECT_EQ(n->child_b->active_tab_index, 1);
  EXPECT_TRUE(n->panels.empty());
  EXPECT_EQ(n->active_tab_index, 0);
}

TEST(DockNodeTest, SplitInPlaceNewChildSecond) {
  auto n = DockNode::MakeLeaf({MakePanel("existing")});
  n->SplitInPlace(SplitDirection::kRight, 0.5f,
                  DockNode::MakeLeaf({MakePanel("new")}),
                  /*new_child_first=*/false);
  EXPECT_EQ(n->child_a->panels[0].panel_id, "existing");
  EXPECT_EQ(n->child_b->panels[0].panel_id, "new");
}

TEST(DockNodeTest, PromoteSingleChildRemovesSplit) {
  auto n = DockNode::MakeSplit(SplitDirection::kLeft, 0.3f,
                               DockNode::MakeLeaf({MakePanel("surviving")}),
                               /*b=*/nullptr);
  EXPECT_TRUE(n->PromoteSingleChild());
  EXPECT_EQ(n->type, DockNode::Type::kLeaf);
  ASSERT_EQ(n->panels.size(), 1u);
  EXPECT_EQ(n->panels[0].panel_id, "surviving");
}

TEST(DockNodeTest, PromoteSingleChildNoopWhenBothChildrenPresent) {
  auto n = DockNode::MakeSplit(SplitDirection::kLeft, 0.3f,
                               DockNode::MakeLeaf({MakePanel("a")}),
                               DockNode::MakeLeaf({MakePanel("b")}));
  EXPECT_FALSE(n->PromoteSingleChild());
  EXPECT_EQ(n->type, DockNode::Type::kSplit);
}

TEST(DockNodeTest, FindPanelWalksTree) {
  auto n = DockNode::MakeSplit(
      SplitDirection::kLeft, 0.3f, DockNode::MakeLeaf({MakePanel("a")}),
      DockNode::MakeSplit(SplitDirection::kUp, 0.5f,
                          DockNode::MakeLeaf({MakePanel("b")}),
                          DockNode::MakeLeaf({MakePanel("c")})));
  EXPECT_NE(n->FindPanel("a"), nullptr);
  EXPECT_NE(n->FindPanel("b"), nullptr);
  EXPECT_NE(n->FindPanel("c"), nullptr);
  EXPECT_EQ(n->FindPanel("missing"), nullptr);
}

TEST(DockTreeTest, DefaultHasEmptyLeafRoot) {
  DockTree t;
  ASSERT_NE(t.root, nullptr);
  EXPECT_EQ(t.root->type, DockNode::Type::kLeaf);
  EXPECT_TRUE(t.root->panels.empty());
  EXPECT_EQ(t.schema_version, 1u);
}

TEST(DockTreeTest, NamedConstructorStoresName) {
  DockTree t("My Layout");
  EXPECT_EQ(t.name, "My Layout");
  ASSERT_NE(t.root, nullptr);
}

TEST(DockTreeTest, CloneIsDeep) {
  DockTree original("Original");
  original.description = "d";
  original.root->panels.push_back(MakePanel("x"));
  auto copy = original.Clone();
  original.root->panels.clear();
  original.name = "mutated";
  EXPECT_EQ(copy.name, "Original");
  EXPECT_EQ(copy.description, "d");
  ASSERT_NE(copy.root, nullptr);
  ASSERT_EQ(copy.root->panels.size(), 1u);
}

TEST(DockTreeValidateTest, EmptyTreeIsValid) {
  DockTree t;
  std::string err;
  EXPECT_TRUE(t.Validate(&err)) << err;
}

TEST(DockTreeValidateTest, NullRootIsInvalid) {
  DockTree t;
  t.root.reset();
  std::string err;
  EXPECT_FALSE(t.Validate(&err));
  EXPECT_FALSE(err.empty());
}

TEST(DockTreeValidateTest, OutOfRangeRatioIsInvalid) {
  DockTree t;
  t.root = DockNode::MakeSplit(SplitDirection::kLeft, 0.99f,
                               DockNode::MakeLeaf({MakePanel("a")}),
                               DockNode::MakeLeaf({MakePanel("b")}));
  std::string err;
  EXPECT_FALSE(t.Validate(&err));
  EXPECT_NE(err.find("split_ratio"), std::string::npos);
}

TEST(DockTreeValidateTest, NullSplitChildIsInvalid) {
  DockTree t;
  t.root = DockNode::MakeSplit(SplitDirection::kLeft, 0.5f,
                               DockNode::MakeLeaf({MakePanel("a")}), nullptr);
  std::string err;
  EXPECT_FALSE(t.Validate(&err));
}

TEST(DockTreeValidateTest, DuplicatePanelIdIsInvalid) {
  DockTree t;
  t.root = DockNode::MakeSplit(
      SplitDirection::kLeft, 0.5f,
      DockNode::MakeLeaf({MakePanel("same"), MakePanel("other")}),
      DockNode::MakeLeaf({MakePanel("same")}));
  std::string err;
  EXPECT_FALSE(t.Validate(&err));
  EXPECT_NE(err.find("same"), std::string::npos);
}

TEST(DockTreeValidateTest, ActiveTabOutOfRangeIsInvalid) {
  DockTree t;
  t.root = DockNode::MakeLeaf({MakePanel("a"), MakePanel("b")});
  t.root->active_tab_index = 5;
  std::string err;
  EXPECT_FALSE(t.Validate(&err));
}

TEST(DockTreeValidateTest, DeeplyNestedTreeIsValid) {
  DockTree t;
  t.root = DockNode::MakeSplit(
      SplitDirection::kLeft, 0.3f,
      DockNode::MakeSplit(SplitDirection::kUp, 0.4f,
                          DockNode::MakeLeaf({MakePanel("a")}),
                          DockNode::MakeLeaf({MakePanel("b")})),
      DockNode::MakeSplit(
          SplitDirection::kDown, 0.5f, DockNode::MakeLeaf({MakePanel("c")}),
          DockNode::MakeLeaf({MakePanel("d"), MakePanel("e")})));
  std::string err;
  EXPECT_TRUE(t.Validate(&err)) << err;
}

}  // namespace
}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze
