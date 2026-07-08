#include "app/editor/layout/layout_designer/dock_tree_hit_test.h"

#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "app/editor/layout/layout_designer/dock_tree.h"
#include "app/editor/layout/layout_designer/dock_tree_renderer.h"
#include "app/editor/layout/layout_designer/split_boundary_drag.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze {
namespace editor {
namespace layout_designer {
namespace {

PanelEntry MakePanel(const std::string& id) {
  return {id, id + " Display", "ICON_MD_FAVORITE"};
}

std::unique_ptr<DockNode> MakeLeafFor(const std::string& id) {
  return DockNode::MakeLeaf({MakePanel(id)});
}

// Builds the same 2-level X-axis split used by several tests:
//   root kLeft @ 0.5, child_a = leaf_a (left 0..200),
//                     child_b = leaf_b (right 200..400).
// Viewport 400 × 300 at origin.
struct TwoLeafSplit {
  DockTree tree;
  const DockNode* leaf_a = nullptr;
  const DockNode* leaf_b = nullptr;
  DockTreeLayout layout;
  ImRect viewport = ImRect(0.0f, 0.0f, 400.0f, 300.0f);
};

TwoLeafSplit BuildTwoLeafSplit() {
  TwoLeafSplit s;
  auto a = MakeLeafFor("a");
  auto b = MakeLeafFor("b");
  s.leaf_a = a.get();
  s.leaf_b = b.get();
  s.tree = DockTree("hit-test");
  s.tree.root = DockNode::MakeSplit(SplitDirection::kLeft, 0.5f, std::move(a),
                                    std::move(b));
  s.layout = ComputeLayout(s.tree, s.viewport);
  return s;
}

TEST(DockTreeHitTestTest, HitTestNodeReturnsLeafForPointInsideItsRect) {
  const TwoLeafSplit s = BuildTwoLeafSplit();
  // Center of left half → leaf_a.
  EXPECT_EQ(HitTestNode(s.layout, ImVec2(100.0f, 150.0f)), s.leaf_a);
  // Center of right half → leaf_b.
  EXPECT_EQ(HitTestNode(s.layout, ImVec2(300.0f, 150.0f)), s.leaf_b);
}

TEST(DockTreeHitTestTest, HitTestNodeReturnsNullForPointOutsideViewport) {
  const TwoLeafSplit s = BuildTwoLeafSplit();
  EXPECT_EQ(HitTestNode(s.layout, ImVec2(-1.0f, 150.0f)), nullptr);
  EXPECT_EQ(HitTestNode(s.layout, ImVec2(500.0f, 150.0f)), nullptr);
  EXPECT_EQ(HitTestNode(s.layout, ImVec2(100.0f, -5.0f)), nullptr);
  EXPECT_EQ(HitTestNode(s.layout, ImVec2(100.0f, 301.0f)), nullptr);
}

TEST(DockTreeHitTestTest, HitTestNodeOnCollapsedSplitReturnsSplitNode) {
  // Viewport 100 × 100, kLeft @ 0.1 → child_a would be 10px (below
  // kMinCellSize 20). Renderer treats the split node itself as the drawn
  // cell; the hit-test should match that.
  auto a = MakeLeafFor("a");
  auto b = MakeLeafFor("b");
  DockTree tree("collapse");
  tree.root = DockNode::MakeSplit(SplitDirection::kLeft, 0.1f, std::move(a),
                                  std::move(b));
  const DockNode* root_ptr = tree.root.get();
  const DockTreeLayout layout = ComputeLayout(tree, ImRect(0, 0, 100, 100));

  EXPECT_EQ(HitTestNode(layout, ImVec2(50.0f, 50.0f)), root_ptr);
}

TEST(DockTreeHitTestTest, HitTestSplitBoundaryFiresOnHorizontalGutter) {
  const TwoLeafSplit s = BuildTwoLeafSplit();
  // Gutter sits at x=200. ±4 px band.
  const SplitBoundaryHit hit =
      HitTestSplitBoundary(s.tree, s.layout, ImVec2(200.0f, 150.0f));
  ASSERT_NE(hit.split_node, nullptr);
  EXPECT_EQ(hit.split_node, s.tree.root.get());
  EXPECT_TRUE(hit.horizontal);
}

TEST(DockTreeHitTestTest, HitTestSplitBoundaryMissesAwayFromGutter) {
  const TwoLeafSplit s = BuildTwoLeafSplit();
  // x=100 is 100 px away from the gutter → miss.
  const SplitBoundaryHit hit =
      HitTestSplitBoundary(s.tree, s.layout, ImVec2(100.0f, 150.0f));
  EXPECT_EQ(hit.split_node, nullptr);
}

TEST(DockTreeHitTestTest, HitTestSplitBoundaryFiresOnVerticalGutter) {
  auto a = MakeLeafFor("top");
  auto b = MakeLeafFor("bot");
  DockTree tree("vertical");
  tree.root = DockNode::MakeSplit(SplitDirection::kUp, 0.5f, std::move(a),
                                  std::move(b));
  const DockTreeLayout layout = ComputeLayout(tree, ImRect(0, 0, 400, 300));
  // Gutter at y = 150.
  const SplitBoundaryHit hit =
      HitTestSplitBoundary(tree, layout, ImVec2(200.0f, 150.0f));
  ASSERT_NE(hit.split_node, nullptr);
  EXPECT_EQ(hit.split_node, tree.root.get());
  EXPECT_FALSE(hit.horizontal);
}

TEST(DockTreeHitTestTest, HitTestSplitBoundaryIgnoresCollapsedSplit) {
  // Children not present in layout → split is collapsed → no interactive
  // gutter reported.
  auto a = MakeLeafFor("a");
  auto b = MakeLeafFor("b");
  DockTree tree("collapsed-boundary");
  tree.root = DockNode::MakeSplit(SplitDirection::kLeft, 0.1f, std::move(a),
                                  std::move(b));
  const DockTreeLayout layout = ComputeLayout(tree, ImRect(0, 0, 100, 100));
  const SplitBoundaryHit hit =
      HitTestSplitBoundary(tree, layout, ImVec2(10.0f, 50.0f));
  EXPECT_EQ(hit.split_node, nullptr);
}

TEST(DockTreeHitTestTest, ComputeDraggedSplitRatioProducesRatioShift) {
  // 200 px axis, start ratio 0.5, delta +20 px → 0.5 + 0.1 = 0.6.
  EXPECT_NEAR(ComputeDraggedSplitRatio(0.5f, 20.0f, 200.0f), 0.6f, 1e-5f);
  // Negative delta shrinks left child.
  EXPECT_NEAR(ComputeDraggedSplitRatio(0.5f, -40.0f, 200.0f), 0.3f, 1e-5f);
}

TEST(DockTreeHitTestTest, ComputeDraggedSplitRatioClampsToValidationBounds) {
  // Drag well past the legal range — clamp at both ends.
  EXPECT_FLOAT_EQ(ComputeDraggedSplitRatio(0.5f, 10000.0f, 200.0f),
                  kMaxSplitRatio);
  EXPECT_FLOAT_EQ(ComputeDraggedSplitRatio(0.5f, -10000.0f, 200.0f),
                  kMinSplitRatio);
}

TEST(DockTreeHitTestTest, ComputeDraggedSplitRatioHandlesZeroAxisSize) {
  // Degenerate axis — fall back to start, clamped.
  EXPECT_FLOAT_EQ(ComputeDraggedSplitRatio(0.5f, 100.0f, 0.0f), 0.5f);
  EXPECT_FLOAT_EQ(ComputeDraggedSplitRatio(1.5f, 100.0f, 0.0f), kMaxSplitRatio);
}

TEST(DockTreeHitTestTest, HitTestSplitBoundaryShrinksToleranceForTinyCells) {
  // Viewport 40 × 40 at kLeft @ 0.5 → each child is 20 × 40 (exactly
  // kMinCellSize). Hand-build a layout where both children are 12 px so
  // the per-side tolerance clamp (min_cell * 0.25 = 3 px) kicks in below
  // the configured 4 px default.
  auto a = MakeLeafFor("a");
  auto b = MakeLeafFor("b");
  const DockNode* a_ptr = a.get();
  const DockNode* b_ptr = b.get();
  DockTree tree("tiny");
  tree.root = DockNode::MakeSplit(SplitDirection::kLeft, 0.5f, std::move(a),
                                  std::move(b));
  DockTreeLayout layout;
  layout.node_rects[tree.root.get()] = ImRect(0.0f, 0.0f, 24.0f, 40.0f);
  layout.node_rects[a_ptr] = ImRect(0.0f, 0.0f, 12.0f, 40.0f);
  layout.node_rects[b_ptr] = ImRect(12.0f, 0.0f, 24.0f, 40.0f);

  // With default 4 px tolerance the gutter would nominally claim
  // x = 8..16. The clamp should reduce effective band to x = 9..15.
  EXPECT_EQ(HitTestSplitBoundary(tree, layout, ImVec2(12.0f, 20.0f)).split_node,
            tree.root.get());
  EXPECT_EQ(HitTestSplitBoundary(tree, layout, ImVec2(15.0f, 20.0f)).split_node,
            tree.root.get());
  // 3.5 px into cell a — outside the clamped band, but would hit at
  // raw tolerance 4. Confirms the clamp is active.
  EXPECT_EQ(HitTestSplitBoundary(tree, layout, ImVec2(8.5f, 20.0f)).split_node,
            nullptr);
}

TEST(DockTreeHitTestTest, HitTestSplitBoundaryPrefersDeeperSplit) {
  //              root kLeft @ 0.5 (X-axis)
  //             /                 \
  //     leaf_outer              inner kLeft @ 0.5 (X-axis)
  //                                /         \
  //                             leaf_m      leaf_r
  // Viewport 400 × 300 → root gutter at x=200, inner gutter at x=300.
  auto outer = MakeLeafFor("outer");
  auto m = MakeLeafFor("middle");
  auto r = MakeLeafFor("right");
  auto inner = DockNode::MakeSplit(SplitDirection::kLeft, 0.5f, std::move(m),
                                   std::move(r));
  const DockNode* inner_ptr = inner.get();
  DockTree tree("nested");
  tree.root = DockNode::MakeSplit(SplitDirection::kLeft, 0.5f, std::move(outer),
                                  std::move(inner));
  const DockTreeLayout layout = ComputeLayout(tree, ImRect(0, 0, 400, 300));

  // Mouse at inner gutter (x=300). Only inner's gutter is in range; root's
  // is 100 px away.
  const SplitBoundaryHit hit =
      HitTestSplitBoundary(tree, layout, ImVec2(300.0f, 150.0f));
  ASSERT_NE(hit.split_node, nullptr);
  EXPECT_EQ(hit.split_node, inner_ptr);
}

}  // namespace
}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze
