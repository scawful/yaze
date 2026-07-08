#include "app/editor/layout/layout_designer/dock_tree_renderer.h"

#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "app/editor/layout/layout_designer/dock_tree.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze {
namespace editor {
namespace layout_designer {
namespace {

constexpr float kTolerance = 0.5f;  // Sub-pixel rounding tolerance.

PanelEntry MakePanel(const std::string& id) {
  return {id, id + " Display", "ICON_MD_FAVORITE"};
}

std::unique_ptr<DockNode> MakeLeafFor(const std::string& id) {
  return DockNode::MakeLeaf({MakePanel(id)});
}

bool Overlaps(const ImRect& a, const ImRect& b) {
  // Sibling rects share a split boundary exactly; treat coincident edges
  // as non-overlapping (open intervals).
  if (a.Max.x <= b.Min.x + kTolerance)
    return false;
  if (b.Max.x <= a.Min.x + kTolerance)
    return false;
  if (a.Max.y <= b.Min.y + kTolerance)
    return false;
  if (b.Max.y <= a.Min.y + kTolerance)
    return false;
  return true;
}

TEST(DockTreeRendererTest, ComputeLayoutAssignsRectsMatchingSplitRatios) {
  //          root  kLeft @ 0.3            (X-axis, 400 wide)
  //         /                    \
  //      leaf_A                 right   kUp @ 0.6   (Y-axis, 280 wide)
  //                             /            \
  //                          leaf_B         lower  kRight @ 0.5
  //                                         /             \
  //                                      leaf_C         leaf_D
  auto leaf_a = MakeLeafFor("a");
  auto leaf_b = MakeLeafFor("b");
  auto leaf_c = MakeLeafFor("c");
  auto leaf_d = MakeLeafFor("d");

  const DockNode* const leaf_a_ptr = leaf_a.get();
  const DockNode* const leaf_b_ptr = leaf_b.get();
  const DockNode* const leaf_c_ptr = leaf_c.get();
  const DockNode* const leaf_d_ptr = leaf_d.get();

  auto lower = DockNode::MakeSplit(SplitDirection::kRight, 0.5f,
                                   std::move(leaf_c), std::move(leaf_d));
  auto right = DockNode::MakeSplit(SplitDirection::kUp, 0.6f, std::move(leaf_b),
                                   std::move(lower));

  DockTree tree("ratio-test");
  tree.root = DockNode::MakeSplit(SplitDirection::kLeft, 0.3f,
                                  std::move(leaf_a), std::move(right));

  const ImRect viewport(0.0f, 0.0f, 400.0f, 300.0f);
  const DockTreeLayout layout = ComputeLayout(tree, viewport);

  ASSERT_GT(layout.node_rects.count(leaf_a_ptr), 0u);
  ASSERT_GT(layout.node_rects.count(leaf_b_ptr), 0u);
  ASSERT_GT(layout.node_rects.count(leaf_c_ptr), 0u);
  ASSERT_GT(layout.node_rects.count(leaf_d_ptr), 0u);

  EXPECT_NEAR(layout.node_rects.at(leaf_a_ptr).GetWidth(), 120.0f, kTolerance);
  EXPECT_NEAR(layout.node_rects.at(leaf_a_ptr).GetHeight(), 300.0f, kTolerance);
  EXPECT_NEAR(layout.node_rects.at(leaf_b_ptr).GetWidth(), 280.0f, kTolerance);
  EXPECT_NEAR(layout.node_rects.at(leaf_b_ptr).GetHeight(), 180.0f, kTolerance);
  EXPECT_NEAR(layout.node_rects.at(leaf_c_ptr).GetWidth(), 140.0f, kTolerance);
  EXPECT_NEAR(layout.node_rects.at(leaf_c_ptr).GetHeight(), 120.0f, kTolerance);
  EXPECT_NEAR(layout.node_rects.at(leaf_d_ptr).GetWidth(), 140.0f, kTolerance);
  EXPECT_NEAR(layout.node_rects.at(leaf_d_ptr).GetHeight(), 120.0f, kTolerance);
}

TEST(DockTreeRendererTest, ComputeLayoutLeavesDoNotOverlap) {
  auto leaf_a = MakeLeafFor("a");
  auto leaf_b = MakeLeafFor("b");
  auto leaf_c = MakeLeafFor("c");
  auto leaf_d = MakeLeafFor("d");

  std::vector<const DockNode*> leaf_ptrs = {leaf_a.get(), leaf_b.get(),
                                            leaf_c.get(), leaf_d.get()};

  auto right_inner = DockNode::MakeSplit(SplitDirection::kUp, 0.4f,
                                         std::move(leaf_c), std::move(leaf_d));
  auto right = DockNode::MakeSplit(SplitDirection::kLeft, 0.5f,
                                   std::move(leaf_b), std::move(right_inner));

  DockTree tree("overlap-test");
  tree.root = DockNode::MakeSplit(SplitDirection::kUp, 0.25f, std::move(leaf_a),
                                  std::move(right));

  const ImRect viewport(10.0f, 20.0f, 410.0f, 320.0f);
  const DockTreeLayout layout = ComputeLayout(tree, viewport);

  for (size_t i = 0; i < leaf_ptrs.size(); ++i) {
    for (size_t j = i + 1; j < leaf_ptrs.size(); ++j) {
      const ImRect& a = layout.node_rects.at(leaf_ptrs[i]);
      const ImRect& b = layout.node_rects.at(leaf_ptrs[j]);
      EXPECT_FALSE(Overlaps(a, b))
          << "Leaf " << i << " and " << j << " overlap: "
          << "a=[" << a.Min.x << "," << a.Min.y << "," << a.Max.x << ","
          << a.Max.y << "] b=[" << b.Min.x << "," << b.Min.y << "," << b.Max.x
          << "," << b.Max.y << "]";
    }
  }
}

TEST(DockTreeRendererTest, ComputeLayoutCollapsesChildrenBelowMinCellSize) {
  // Viewport 100 wide, split kLeft @ 0.1 would give child_a = 10px which
  // is below kMinCellSize (20px). ComputeLayout must omit child rects.
  auto leaf_small = MakeLeafFor("small");
  auto leaf_big = MakeLeafFor("big");
  const DockNode* const leaf_small_ptr = leaf_small.get();
  const DockNode* const leaf_big_ptr = leaf_big.get();

  DockTree tree("collapse-test");
  tree.root = DockNode::MakeSplit(SplitDirection::kLeft, 0.1f,
                                  std::move(leaf_small), std::move(leaf_big));

  const DockNode* const root_ptr = tree.root.get();
  const ImRect viewport(0.0f, 0.0f, 100.0f, 100.0f);
  const DockTreeLayout layout = ComputeLayout(tree, viewport);

  EXPECT_GT(layout.node_rects.count(root_ptr), 0u)
      << "Root split rect must still be recorded";
  EXPECT_EQ(layout.node_rects.count(leaf_small_ptr), 0u)
      << "Child_a (10px wide) must be collapsed below kMinCellSize";
  EXPECT_EQ(layout.node_rects.count(leaf_big_ptr), 0u)
      << "Child_b is also dropped when its sibling collapses";
}

}  // namespace
}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze
