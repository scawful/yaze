#include "app/editor/layout/layout_designer/drop_zone_suggester.h"

#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "app/editor/layout/layout_designer/dock_tree.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze {
namespace editor {
namespace layout_designer {
namespace {

PanelEntry MakePanel(const std::string& id) {
  return {id, id + " Display", "ICON_MD_FAVORITE"};
}

TEST(DropZoneSuggesterTest, CenterMouseSuggestsTab) {
  const ImRect rect(0.0f, 0.0f, 400.0f, 300.0f);
  const DropSuggestion s = SuggestDrop(rect, ImVec2(200.0f, 150.0f));
  EXPECT_EQ(s.kind, DropSuggestion::Kind::kTab);
}

TEST(DropZoneSuggesterTest, LeftEdgeMouseSuggestsSplitLeft) {
  const ImRect rect(0.0f, 0.0f, 400.0f, 300.0f);
  // x = 20 → u = 0.05 (well below kDropEdgeFraction 0.3), y = 150 middle.
  const DropSuggestion s = SuggestDrop(rect, ImVec2(20.0f, 150.0f));
  EXPECT_EQ(s.kind, DropSuggestion::Kind::kSplitLeft);
}

TEST(DropZoneSuggesterTest, RightEdgeMouseSuggestsSplitRight) {
  const ImRect rect(0.0f, 0.0f, 400.0f, 300.0f);
  // x = 380 → u = 0.95.
  const DropSuggestion s = SuggestDrop(rect, ImVec2(380.0f, 150.0f));
  EXPECT_EQ(s.kind, DropSuggestion::Kind::kSplitRight);
}

TEST(DropZoneSuggesterTest, TopEdgeMouseSuggestsSplitTop) {
  const ImRect rect(0.0f, 0.0f, 400.0f, 300.0f);
  // y = 20 → v ≈ 0.067.
  const DropSuggestion s = SuggestDrop(rect, ImVec2(200.0f, 20.0f));
  EXPECT_EQ(s.kind, DropSuggestion::Kind::kSplitTop);
}

TEST(DropZoneSuggesterTest, BottomEdgeMouseSuggestsSplitBottom) {
  const ImRect rect(0.0f, 0.0f, 400.0f, 300.0f);
  const DropSuggestion s = SuggestDrop(rect, ImVec2(200.0f, 280.0f));
  EXPECT_EQ(s.kind, DropSuggestion::Kind::kSplitBottom);
}

TEST(DropZoneSuggesterTest, MouseOutsideReturnsKNone) {
  const ImRect rect(0.0f, 0.0f, 400.0f, 300.0f);
  EXPECT_EQ(SuggestDrop(rect, ImVec2(-5.0f, 10.0f)).kind,
            DropSuggestion::Kind::kNone);
  EXPECT_EQ(SuggestDrop(rect, ImVec2(500.0f, 150.0f)).kind,
            DropSuggestion::Kind::kNone);
}

TEST(DropZoneSuggesterTest, ComputeDropPreviewRectMatchesSplitRatio) {
  const ImRect rect(100.0f, 50.0f, 500.0f, 350.0f);  // 400 x 300
  DropSuggestion left;
  left.kind = DropSuggestion::Kind::kSplitLeft;
  const ImRect preview_left = ComputeDropPreviewRect(rect, left);
  EXPECT_FLOAT_EQ(preview_left.Min.x, 100.0f);
  EXPECT_FLOAT_EQ(preview_left.Max.x, 100.0f + 400.0f * kDropSplitRatio);
  EXPECT_FLOAT_EQ(preview_left.Min.y, 50.0f);
  EXPECT_FLOAT_EQ(preview_left.Max.y, 350.0f);

  DropSuggestion bottom;
  bottom.kind = DropSuggestion::Kind::kSplitBottom;
  const ImRect preview_bottom = ComputeDropPreviewRect(rect, bottom);
  EXPECT_FLOAT_EQ(preview_bottom.Min.y, 350.0f - 300.0f * kDropSplitRatio);
  EXPECT_FLOAT_EQ(preview_bottom.Max.y, 350.0f);
}

TEST(DropZoneSuggesterTest, ComputeDropPreviewRectForTabReturnsFullRect) {
  const ImRect rect(0.0f, 0.0f, 100.0f, 100.0f);
  DropSuggestion tab;
  tab.kind = DropSuggestion::Kind::kTab;
  const ImRect preview = ComputeDropPreviewRect(rect, tab);
  EXPECT_FLOAT_EQ(preview.Min.x, rect.Min.x);
  EXPECT_FLOAT_EQ(preview.Max.x, rect.Max.x);
}

TEST(DropZoneSuggesterTest, ApplyKTabAppendsAndSelectsPanel) {
  DockTree tree("drop-test");
  tree.root = DockNode::MakeLeaf({MakePanel("existing")});
  DockNode* leaf = tree.root.get();

  DropSuggestion s;
  s.kind = DropSuggestion::Kind::kTab;
  ASSERT_TRUE(ApplyDropSuggestion(&tree, leaf, s, MakePanel("dropped")));

  ASSERT_EQ(leaf->panels.size(), 2u);
  EXPECT_EQ(leaf->panels[1].panel_id, "dropped");
  EXPECT_EQ(leaf->active_tab_index, 1);
}

TEST(DropZoneSuggesterTest, ApplyKSplitLeftSplitsLeafWithCorrectShape) {
  DockTree tree("drop-split");
  tree.root = DockNode::MakeLeaf({MakePanel("existing")});
  DockNode* leaf = tree.root.get();

  DropSuggestion s;
  s.kind = DropSuggestion::Kind::kSplitLeft;
  ASSERT_TRUE(ApplyDropSuggestion(&tree, leaf, s, MakePanel("dropped")));

  EXPECT_EQ(leaf->type, DockNode::Type::kSplit);
  EXPECT_EQ(leaf->split_direction, SplitDirection::kLeft);
  EXPECT_FLOAT_EQ(leaf->split_ratio, kDropSplitRatio);
  ASSERT_TRUE(leaf->child_a);
  ASSERT_TRUE(leaf->child_b);
  // child_a is the new panel (new_child_first == true for kSplitLeft).
  EXPECT_EQ(leaf->child_a->type, DockNode::Type::kLeaf);
  ASSERT_EQ(leaf->child_a->panels.size(), 1u);
  EXPECT_EQ(leaf->child_a->panels[0].panel_id, "dropped");
  // child_b retains the original panel.
  EXPECT_EQ(leaf->child_b->type, DockNode::Type::kLeaf);
  ASSERT_EQ(leaf->child_b->panels.size(), 1u);
  EXPECT_EQ(leaf->child_b->panels[0].panel_id, "existing");
}

TEST(DropZoneSuggesterTest, ApplyKSplitBottomPlacesDroppedPanelAtBottom) {
  DockTree tree("drop-bottom");
  tree.root = DockNode::MakeLeaf({MakePanel("existing")});
  DockNode* leaf = tree.root.get();

  DropSuggestion s;
  s.kind = DropSuggestion::Kind::kSplitBottom;
  ASSERT_TRUE(ApplyDropSuggestion(&tree, leaf, s, MakePanel("bottom")));

  EXPECT_EQ(leaf->type, DockNode::Type::kSplit);
  EXPECT_EQ(leaf->split_direction, SplitDirection::kDown);
  // For kSplitBottom, new_child_first == false, and split_ratio gives
  // the existing panel the top share (1 - kDropSplitRatio).
  EXPECT_FLOAT_EQ(leaf->split_ratio, 1.0f - kDropSplitRatio);
  ASSERT_TRUE(leaf->child_a);
  ASSERT_TRUE(leaf->child_b);
  EXPECT_EQ(leaf->child_a->panels[0].panel_id, "existing");
  EXPECT_EQ(leaf->child_b->panels[0].panel_id, "bottom");
}

TEST(DropZoneSuggesterTest, ApplyRejectsDuplicatePanelId) {
  DockTree tree("dup");
  tree.root = DockNode::MakeLeaf({MakePanel("already.here")});
  DockNode* leaf = tree.root.get();

  DropSuggestion s;
  s.kind = DropSuggestion::Kind::kTab;
  EXPECT_FALSE(ApplyDropSuggestion(&tree, leaf, s, MakePanel("already.here")));
  // Tree state unchanged.
  EXPECT_EQ(leaf->panels.size(), 1u);
}

TEST(DropZoneSuggesterTest, ApplyRejectsNonLeafTarget) {
  DockTree tree("split");
  auto leaf_a = DockNode::MakeLeaf({MakePanel("a")});
  auto leaf_b = DockNode::MakeLeaf({MakePanel("b")});
  tree.root = DockNode::MakeSplit(SplitDirection::kLeft, 0.5f,
                                  std::move(leaf_a), std::move(leaf_b));

  DropSuggestion s;
  s.kind = DropSuggestion::Kind::kTab;
  // Dropping on the root (a split, not a leaf) is refused.
  EXPECT_FALSE(
      ApplyDropSuggestion(&tree, tree.root.get(), s, MakePanel("new")));
}

TEST(DropZoneSuggesterTest, ApplyPreservesValidateInvariants) {
  DockTree tree("validate");
  tree.root = DockNode::MakeLeaf({MakePanel("first")});
  DockNode* leaf = tree.root.get();

  DropSuggestion s;
  s.kind = DropSuggestion::Kind::kSplitRight;
  ASSERT_TRUE(ApplyDropSuggestion(&tree, leaf, s, MakePanel("second")));

  std::string err;
  EXPECT_TRUE(tree.Validate(&err)) << "Validate error: " << err;
}

}  // namespace
}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze
