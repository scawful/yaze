#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "app/editor/layout/layout_designer/dock_tree.h"
#include "app/editor/layout/layout_designer/dock_tree_json.h"
#include "app/editor/layout/layout_manager.h"
#include "app/editor/system/session/user_settings.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "nlohmann/json.hpp"

namespace yaze::editor {
namespace {

using ::yaze::editor::layout_designer::DockNode;
using ::yaze::editor::layout_designer::DockTree;
using ::yaze::editor::layout_designer::PanelEntry;
using ::yaze::editor::layout_designer::SplitDirection;

constexpr ImGuiID kDockspaceId = 0x43421234u;
constexpr float kDockspaceWidth = 1280.0f;
constexpr float kDockspaceHeight = 720.0f;

PanelEntry MakePanel(const std::string& id) {
  return {id, id + " Display", "ICON_MD_ACCOUNT_TREE"};
}

class LayoutManagerDockTreeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    imgui_context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui_context_);

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(kDockspaceWidth, kDockspaceHeight);
    io.DeltaTime = 1.0f / 60.0f;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    // NewFrame is required before DockBuilder* calls so internal contexts
    // (ImGuiDockContext) are valid.
    ImGui::NewFrame();

    window_manager_.RegisterSession(0);
    window_manager_.SetActiveSession(0);

    for (const char* id : {"a", "b", "c", "d", "e"}) {
      WindowDescriptor desc;
      desc.card_id = id;
      desc.display_name = std::string(id) + " Display";
      desc.icon = "ICON_MD_ACCOUNT_TREE";
      desc.category = "Test";
      desc.priority = 1;
      desc.visibility_flag = &always_true_;
      window_manager_.RegisterWindow(0, desc);
    }
    layout_manager_.SetWindowManager(&window_manager_);

    // Seed the root dockspace node so the DockBuilder internals recognise
    // the id before Apply tries to rebuild it.
    ImGui::DockBuilderRemoveNode(kDockspaceId);
    ImGui::DockBuilderAddNode(kDockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(kDockspaceId,
                                  ImVec2(kDockspaceWidth, kDockspaceHeight));
  }

  void TearDown() override {
    ImGui::DockBuilderRemoveNode(kDockspaceId);
    ImGui::EndFrame();
    if (imgui_context_) {
      ImGui::DestroyContext(imgui_context_);
      imgui_context_ = nullptr;
    }
  }

  ImGuiContext* imgui_context_ = nullptr;
  bool always_true_ = true;
  WorkspaceWindowManager window_manager_;
  LayoutManager layout_manager_;
};

TEST_F(LayoutManagerDockTreeTest, ApplyWithoutWindowManagerFails) {
  LayoutManager bare;
  DockTree tree;
  auto status = bare.ApplyDockTree(tree, kDockspaceId);
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
}

TEST_F(LayoutManagerDockTreeTest, ApplyInvalidTreeReturnsInvalidArgument) {
  DockTree tree;
  // Null root violates Validate().
  tree.root = nullptr;
  auto status = layout_manager_.ApplyDockTree(tree, kDockspaceId);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(LayoutManagerDockTreeTest, ApplyEmptyLeafSucceeds) {
  DockTree tree;
  auto status = layout_manager_.ApplyDockTree(tree, kDockspaceId);
  ASSERT_TRUE(status.ok()) << status;

  ImGuiDockNode* node = ImGui::DockBuilderGetNode(kDockspaceId);
  ASSERT_NE(node, nullptr);
  EXPECT_TRUE(node->IsLeafNode());
}

TEST_F(LayoutManagerDockTreeTest, ApplyLeftSplitUsesHorizontalAxis) {
  DockTree tree;
  tree.root = DockNode::MakeSplit(SplitDirection::kLeft, 0.3f,
                                  DockNode::MakeLeaf({MakePanel("a")}),
                                  DockNode::MakeLeaf({MakePanel("b")}));
  auto status = layout_manager_.ApplyDockTree(tree, kDockspaceId);
  ASSERT_TRUE(status.ok()) << status;

  ImGuiDockNode* root = ImGui::DockBuilderGetNode(kDockspaceId);
  ASSERT_NE(root, nullptr);
  ASSERT_TRUE(root->IsSplitNode());
  EXPECT_EQ(root->SplitAxis, ImGuiAxis_X);
}

TEST_F(LayoutManagerDockTreeTest, ApplyUpSplitUsesVerticalAxis) {
  DockTree tree;
  tree.root = DockNode::MakeSplit(SplitDirection::kUp, 0.4f,
                                  DockNode::MakeLeaf({MakePanel("a")}),
                                  DockNode::MakeLeaf({MakePanel("b")}));
  auto status = layout_manager_.ApplyDockTree(tree, kDockspaceId);
  ASSERT_TRUE(status.ok()) << status;

  ImGuiDockNode* root = ImGui::DockBuilderGetNode(kDockspaceId);
  ASSERT_NE(root, nullptr);
  ASSERT_TRUE(root->IsSplitNode());
  EXPECT_EQ(root->SplitAxis, ImGuiAxis_Y);
}

TEST_F(LayoutManagerDockTreeTest, CaptureRoundTripPreservesLeftSplitRatio) {
  DockTree original;
  original.root = DockNode::MakeSplit(SplitDirection::kLeft, 0.35f,
                                      DockNode::MakeLeaf({MakePanel("a")}),
                                      DockNode::MakeLeaf({MakePanel("b")}));
  ASSERT_TRUE(layout_manager_.ApplyDockTree(original, kDockspaceId).ok());

  auto captured = layout_manager_.CaptureDockTree(kDockspaceId);
  ASSERT_TRUE(captured.ok()) << captured.status();
  ASSERT_NE(captured->root, nullptr);
  ASSERT_EQ(captured->root->type, DockNode::Type::kSplit);
  EXPECT_EQ(captured->root->split_direction, SplitDirection::kLeft);
  EXPECT_NEAR(captured->root->split_ratio, 0.35f, 0.02f);
  ASSERT_NE(captured->root->child_a, nullptr);
  ASSERT_NE(captured->root->child_b, nullptr);
  EXPECT_EQ(captured->root->child_a->type, DockNode::Type::kLeaf);
  EXPECT_EQ(captured->root->child_b->type, DockNode::Type::kLeaf);
}

TEST_F(LayoutManagerDockTreeTest, CaptureRoundTripPreservesNestedStructure) {
  DockTree original;
  original.root = DockNode::MakeSplit(
      SplitDirection::kLeft, 0.25f, DockNode::MakeLeaf({MakePanel("a")}),
      DockNode::MakeSplit(SplitDirection::kUp, 0.6f,
                          DockNode::MakeLeaf({MakePanel("b")}),
                          DockNode::MakeLeaf({MakePanel("c")})));
  ASSERT_TRUE(layout_manager_.ApplyDockTree(original, kDockspaceId).ok());

  auto captured = layout_manager_.CaptureDockTree(kDockspaceId);
  ASSERT_TRUE(captured.ok()) << captured.status();

  // Top-level split on the X axis, captured as kLeft.
  ASSERT_EQ(captured->root->type, DockNode::Type::kSplit);
  EXPECT_EQ(captured->root->split_direction, SplitDirection::kLeft);
  EXPECT_NEAR(captured->root->split_ratio, 0.25f, 0.02f);

  // Right-hand half is itself a Y-axis split, captured as kUp.
  ASSERT_EQ(captured->root->child_b->type, DockNode::Type::kSplit);
  EXPECT_EQ(captured->root->child_b->split_direction, SplitDirection::kUp);
  EXPECT_NEAR(captured->root->child_b->split_ratio, 0.6f, 0.02f);
}

TEST_F(LayoutManagerDockTreeTest, CaptureWithoutWindowManagerFails) {
  LayoutManager bare;
  auto status = bare.CaptureDockTree(kDockspaceId);
  EXPECT_EQ(status.status().code(), absl::StatusCode::kFailedPrecondition);
}

TEST_F(LayoutManagerDockTreeTest, CaptureMissingDockspaceFails) {
  auto status = layout_manager_.CaptureDockTree(0xDEADBEEFu);
  EXPECT_EQ(status.status().code(), absl::StatusCode::kNotFound);
}

TEST_F(LayoutManagerDockTreeTest, CaptureEmptyDockspaceReturnsLeaf) {
  auto captured = layout_manager_.CaptureDockTree(kDockspaceId);
  ASSERT_TRUE(captured.ok()) << captured.status();
  ASSERT_NE(captured->root, nullptr);
  EXPECT_EQ(captured->root->type, DockNode::Type::kLeaf);
  EXPECT_TRUE(captured->root->panels.empty());
}

TEST_F(LayoutManagerDockTreeTest, ApplyDropsInvalidTreeWithMissingChild) {
  DockTree tree;
  tree.root = DockNode::MakeSplit(SplitDirection::kLeft, 0.5f,
                                  DockNode::MakeLeaf({MakePanel("a")}),
                                  DockNode::MakeLeaf({MakePanel("b")}));
  // Corrupt after construction.
  tree.root->child_b.reset();

  auto status = layout_manager_.ApplyDockTree(tree, kDockspaceId);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

// ============================================================================
// Phase 8.2: MaybeReapplyStartupLayout
// ============================================================================
//
// Drives `last_applied_layout_name` + `named_layouts` from UserSettings on
// the first frame the main dockspace is bound. Idempotent — every subsequent
// call (per-frame from the controller) is a no-op.

TEST_F(LayoutManagerDockTreeTest, StartupReapplyNoopWhenSettingsNull) {
  layout_manager_.SetMainDockspaceId(kDockspaceId);
  EXPECT_FALSE(layout_manager_.startup_layout_consumed_for_test());
  auto status = layout_manager_.MaybeReapplyStartupLayout(nullptr);
  EXPECT_TRUE(status.ok()) << status;
  EXPECT_TRUE(layout_manager_.startup_layout_consumed_for_test());
}

TEST_F(LayoutManagerDockTreeTest, StartupReapplyNoopWhenLastAppliedEmpty) {
  layout_manager_.SetMainDockspaceId(kDockspaceId);
  UserSettings settings;
  // last_applied_layout_name is empty by default.
  auto status = layout_manager_.MaybeReapplyStartupLayout(&settings);
  EXPECT_TRUE(status.ok()) << status;
  EXPECT_TRUE(layout_manager_.startup_layout_consumed_for_test());
}

TEST_F(LayoutManagerDockTreeTest, StartupReapplyDeferredWhenDockspaceNotBound) {
  // main_dockspace_id_ defaults to 0; the hook should leave the consumed
  // flag clear so the next frame can retry once the controller binds it.
  UserSettings settings;
  settings.prefs().last_applied_layout_name = "some_layout";
  auto status = layout_manager_.MaybeReapplyStartupLayout(&settings);
  EXPECT_TRUE(status.ok()) << status;
  EXPECT_FALSE(layout_manager_.startup_layout_consumed_for_test())
      << "MaybeReapplyStartupLayout must not consume itself before the "
         "main dockspace id is bound — the controller calls it per-frame.";
}

TEST_F(LayoutManagerDockTreeTest, StartupReapplyNotFoundWhenNameMissing) {
  layout_manager_.SetMainDockspaceId(kDockspaceId);
  UserSettings settings;
  settings.prefs().last_applied_layout_name = "no_such_layout";
  // named_layouts is empty.
  auto status = layout_manager_.MaybeReapplyStartupLayout(&settings);
  EXPECT_EQ(status.code(), absl::StatusCode::kNotFound);
  EXPECT_TRUE(layout_manager_.startup_layout_consumed_for_test());
}

TEST_F(LayoutManagerDockTreeTest, StartupReapplyParseErrorOnMalformedJson) {
  layout_manager_.SetMainDockspaceId(kDockspaceId);
  UserSettings settings;
  settings.prefs().last_applied_layout_name = "bad";
  settings.prefs().named_layouts["bad"] = "{not valid json";
  auto status = layout_manager_.MaybeReapplyStartupLayout(&settings);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_TRUE(layout_manager_.startup_layout_consumed_for_test());
}

TEST_F(LayoutManagerDockTreeTest, StartupReapplyValidationErrorOnDuplicateIds) {
  layout_manager_.SetMainDockspaceId(kDockspaceId);
  UserSettings settings;
  // Build a tree with a duplicate id forcibly stamped, serialize, and
  // hand it back to the manager. Without the Validate gate this would
  // silently install — the gate must reject it.
  DockTree malformed;
  malformed.root = DockNode::MakeSplit(SplitDirection::kLeft, 0.5f,
                                       DockNode::MakeLeaf({MakePanel("a")}),
                                       DockNode::MakeLeaf({MakePanel("b")}));
  malformed.root->child_b->id = malformed.root->child_a->id;  // collide

  settings.prefs().last_applied_layout_name = "dup";
  settings.prefs().named_layouts["dup"] =
      layout_designer::DockTreeToJson(malformed).dump();
  auto status = layout_manager_.MaybeReapplyStartupLayout(&settings);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_TRUE(layout_manager_.startup_layout_consumed_for_test());
}

TEST_F(LayoutManagerDockTreeTest, StartupReapplyAppliesAndConsumes) {
  layout_manager_.SetMainDockspaceId(kDockspaceId);
  UserSettings settings;
  // Build a real, validating left-split tree and persist it as JSON.
  DockTree tree;
  tree.root = DockNode::MakeSplit(SplitDirection::kLeft, 0.3f,
                                  DockNode::MakeLeaf({MakePanel("a")}),
                                  DockNode::MakeLeaf({MakePanel("b")}));
  settings.prefs().last_applied_layout_name = "my_layout";
  settings.prefs().named_layouts["my_layout"] =
      layout_designer::DockTreeToJson(tree).dump();

  auto status = layout_manager_.MaybeReapplyStartupLayout(&settings);
  ASSERT_TRUE(status.ok()) << status;
  EXPECT_TRUE(layout_manager_.startup_layout_consumed_for_test());

  // Side-effect check: the dockspace was actually rebuilt as a horizontal
  // split, matching the tree.
  ImGuiDockNode* root = ImGui::DockBuilderGetNode(kDockspaceId);
  ASSERT_NE(root, nullptr);
  ASSERT_TRUE(root->IsSplitNode());
  EXPECT_EQ(root->SplitAxis, ImGuiAxis_X);
}

TEST_F(LayoutManagerDockTreeTest, StartupReapplyIsIdempotent) {
  layout_manager_.SetMainDockspaceId(kDockspaceId);
  UserSettings settings;
  DockTree tree;
  tree.root = DockNode::MakeLeaf({MakePanel("a")});
  settings.prefs().last_applied_layout_name = "once";
  settings.prefs().named_layouts["once"] =
      layout_designer::DockTreeToJson(tree).dump();

  ASSERT_TRUE(layout_manager_.MaybeReapplyStartupLayout(&settings).ok());
  ASSERT_TRUE(layout_manager_.startup_layout_consumed_for_test());

  // Mutate the named layout AFTER first apply. A second call must
  // ignore the mutation entirely — the consumed flag short-circuits.
  settings.prefs().named_layouts["once"] = "{not valid json";
  auto status = layout_manager_.MaybeReapplyStartupLayout(&settings);
  EXPECT_TRUE(status.ok())
      << "Idempotent guard must short-circuit before any JSON parse, even "
         "when the persisted layout has been corrupted post-apply.";
}

}  // namespace
}  // namespace yaze::editor
