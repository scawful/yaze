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

// ============================================================================
// Phase 8 review (2026-04-24): ApplyDockTree opens panel visibility +
// marks editor-type layouts initialized so the lazy-init path doesn't
// clobber the just-applied custom layout.
// ============================================================================

TEST_F(LayoutManagerDockTreeTest, ApplyDockTreeOpensReferencedPanels) {
  // Register two extra windows with private visibility flags (so the
  // shared `always_true_` from the fixture doesn't mask the result).
  bool visible_x = false;
  bool visible_y = false;
  WindowDescriptor desc_x{};
  desc_x.card_id = "x";
  desc_x.display_name = "X Display";
  desc_x.icon = "ICON_MD_ACCOUNT_TREE";
  desc_x.category = "Test";
  desc_x.priority = 1;
  desc_x.visibility_flag = &visible_x;
  window_manager_.RegisterWindow(0, desc_x);

  WindowDescriptor desc_y = desc_x;
  desc_y.card_id = "y";
  desc_y.display_name = "Y Display";
  desc_y.visibility_flag = &visible_y;
  window_manager_.RegisterWindow(0, desc_y);

  DockTree tree;
  tree.root = DockNode::MakeSplit(SplitDirection::kLeft, 0.5f,
                                  DockNode::MakeLeaf({MakePanel("x")}),
                                  DockNode::MakeLeaf({MakePanel("y")}));

  ASSERT_TRUE(layout_manager_.ApplyDockTree(tree, kDockspaceId).ok());
  EXPECT_TRUE(visible_x)
      << "ApplyDockTree must open every panel referenced in the tree so "
         "users see what they docked rather than empty slots.";
  EXPECT_TRUE(visible_y);
}

TEST_F(LayoutManagerDockTreeTest,
       ApplyDockTreeMarksOnlyCurrentEditorTypeInitialized) {
  // Phase 8.2 review (2026-04-25): the previous "mark every type"
  // approach blocked legitimate first-run preset init for OTHER
  // editors. The narrow contract: when a custom layout is applied
  // while the user is in editor X, only editor X's lazy-init is
  // suppressed. Other editors (Assembly, Emulator, etc.) keep their
  // first-run preset behavior.
  layout_manager_.set_current_editor_type_for_test(EditorType::kDungeon);
  for (size_t i = 0; i < kEditorTypeCount; ++i) {
    ASSERT_FALSE(
        layout_manager_.IsLayoutInitialized(static_cast<EditorType>(i)));
  }

  DockTree tree;
  tree.root = DockNode::MakeLeaf({MakePanel("a")});
  ASSERT_TRUE(layout_manager_.ApplyDockTree(tree, kDockspaceId).ok());

  EXPECT_TRUE(layout_manager_.IsLayoutInitialized(EditorType::kDungeon))
      << "Apply must mark the current editor type initialized so the "
         "lazy preset init path doesn't clobber the custom layout.";
  EXPECT_FALSE(layout_manager_.IsLayoutInitialized(EditorType::kAssembly))
      << "Apply must NOT mark unrelated editor types — Assembly should "
         "still get its first-time preset on first activation.";
  EXPECT_FALSE(layout_manager_.IsLayoutInitialized(EditorType::kEmulator));
  EXPECT_FALSE(layout_manager_.IsLayoutInitialized(EditorType::kOverworld));
}

TEST_F(LayoutManagerDockTreeTest,
       ApplyDockTreeWithUnknownCurrentEditorMarksNothing) {
  // Default current_editor_type_ is kUnknown (no editor activated yet).
  // Applying in this state should mark nothing — the startup-reapply
  // path uses a separate one-shot flag for protection.
  DockTree tree;
  tree.root = DockNode::MakeLeaf({MakePanel("a")});
  ASSERT_TRUE(layout_manager_.ApplyDockTree(tree, kDockspaceId).ok());

  for (size_t i = 0; i < kEditorTypeCount; ++i) {
    EXPECT_FALSE(
        layout_manager_.IsLayoutInitialized(static_cast<EditorType>(i)))
        << "Apply with current_editor_type_ == kUnknown must not mark "
           "any editor type initialized; the startup-reapply one-shot "
           "covers the pre-activation case. Type index = "
        << i;
  }
}

TEST_F(LayoutManagerDockTreeTest,
       StartupReapplySetsOneShotProtectionFlagOnSuccess) {
  layout_manager_.SetMainDockspaceId(kDockspaceId);
  UserSettings settings;
  DockTree tree;
  tree.root = DockNode::MakeLeaf({MakePanel("a")});
  settings.prefs().last_applied_layout_name = "boot";
  settings.prefs().named_layouts["boot"] =
      layout_designer::DockTreeToJson(tree).dump();

  ASSERT_FALSE(layout_manager_.startup_reapply_pending_protection_for_test());
  ASSERT_TRUE(layout_manager_.MaybeReapplyStartupLayout(&settings).ok());
  EXPECT_TRUE(layout_manager_.startup_reapply_pending_protection_for_test())
      << "Successful startup-reapply must arm the one-shot protection so "
         "the next InitializeEditorLayout call doesn't clobber the just-"
         "installed custom layout.";
}

TEST_F(LayoutManagerDockTreeTest,
       InitializeEditorLayoutConsumesStartupProtectionWithoutRebuild) {
  layout_manager_.SetMainDockspaceId(kDockspaceId);
  UserSettings settings;
  DockTree tree;
  tree.root = DockNode::MakeLeaf({MakePanel("a")});
  settings.prefs().last_applied_layout_name = "boot";
  settings.prefs().named_layouts["boot"] =
      layout_designer::DockTreeToJson(tree).dump();
  ASSERT_TRUE(layout_manager_.MaybeReapplyStartupLayout(&settings).ok());
  ASSERT_TRUE(layout_manager_.startup_reapply_pending_protection_for_test());

  ImGuiDockNode* before = ImGui::DockBuilderGetNode(kDockspaceId);
  ASSERT_NE(before, nullptr);
  ASSERT_TRUE(before->IsLeafNode());

  // First lazy init after startup reapply must consume the flag, mark
  // the type initialized, and NOT rebuild the dockspace.
  layout_manager_.InitializeEditorLayout(EditorType::kDungeon, kDockspaceId);
  EXPECT_FALSE(layout_manager_.startup_reapply_pending_protection_for_test());
  EXPECT_TRUE(layout_manager_.IsLayoutInitialized(EditorType::kDungeon));

  ImGuiDockNode* after = ImGui::DockBuilderGetNode(kDockspaceId);
  ASSERT_NE(after, nullptr);
  EXPECT_TRUE(after->IsLeafNode())
      << "Custom layout shape must survive the protected first lazy "
         "init.";
}

TEST_F(LayoutManagerDockTreeTest,
       ApplyDockTreeArmsProtectionWhenCurrentEditorIsUnknown) {
  // Phase 8.2 review 3 (2026-04-25): manual apply from a no-editor
  // context (e.g. dashboard / settings shell pre-activation) was
  // unprotected — current_editor_type_ defaults to kUnknown, so
  // ApplyDockTree had nothing to mark, and the startup-protection
  // one-shot was only set by MaybeReapplyStartupLayout. The fix:
  // arm the flag in ApplyDockTree itself when current is kUnknown.
  ASSERT_FALSE(layout_manager_.startup_reapply_pending_protection_for_test());
  // Default current_editor_type_ is kUnknown — no setter call here.

  DockTree tree;
  tree.root = DockNode::MakeLeaf({MakePanel("a")});
  ASSERT_TRUE(layout_manager_.ApplyDockTree(tree, kDockspaceId).ok());
  EXPECT_TRUE(layout_manager_.startup_reapply_pending_protection_for_test())
      << "Apply with current_editor_type_ == kUnknown must arm the "
         "one-shot flag — otherwise the next InitializeEditorLayout "
         "would clobber the just-installed custom layout (the same bug "
         "the original Phase 8.2 fix tried to close).";
}

TEST_F(LayoutManagerDockTreeTest,
       ApplyDockTreeDoesNotArmProtectionWhenCurrentEditorIsKnown) {
  // The flip-side: when an editor is active, the per-editor mark is
  // sufficient and the one-shot flag stays clear. Arming it would
  // mean the next OTHER-editor activation gets suppressed, which is
  // exactly what review round 2 narrowed away from.
  layout_manager_.set_current_editor_type_for_test(EditorType::kDungeon);

  DockTree tree;
  tree.root = DockNode::MakeLeaf({MakePanel("a")});
  ASSERT_TRUE(layout_manager_.ApplyDockTree(tree, kDockspaceId).ok());
  EXPECT_FALSE(layout_manager_.startup_reapply_pending_protection_for_test())
      << "Apply with a known current editor must NOT arm the one-shot "
         "flag; the per-editor mark is the right scope for that case.";
}

TEST_F(LayoutManagerDockTreeTest,
       ApplyDockTreeSkipsClosingAlreadyClosedNonTreePanels) {
  // Phase 8.2 review 3 (2026-04-25): the close pass blindly called
  // CloseWindow on every non-tree, non-pinned panel, which fires
  // on_hide + WindowVisibilityChanged even on already-closed panels.
  // This test pins the no-spurious-events behavior by counting
  // on_hide invocations.
  bool visible_in_tree = false;
  bool visible_already_hidden = false;  // already closed
  bool visible_already_open = true;     // already open, will be closed
  int already_hidden_hide_calls = 0;
  int already_open_hide_calls = 0;

  WindowDescriptor d_in{};
  d_in.card_id = "in_tree";
  d_in.display_name = "InTree";
  d_in.icon = "ICON_MD_ACCOUNT_TREE";
  d_in.category = "Test";
  d_in.priority = 1;
  d_in.visibility_flag = &visible_in_tree;
  window_manager_.RegisterWindow(0, d_in);

  WindowDescriptor d_hidden = d_in;
  d_hidden.card_id = "already_hidden";
  d_hidden.display_name = "Hidden";
  d_hidden.visibility_flag = &visible_already_hidden;
  d_hidden.on_hide = [&]() {
    ++already_hidden_hide_calls;
  };
  window_manager_.RegisterWindow(0, d_hidden);

  WindowDescriptor d_open = d_in;
  d_open.card_id = "already_open";
  d_open.display_name = "Open";
  d_open.visibility_flag = &visible_already_open;
  d_open.on_hide = [&]() {
    ++already_open_hide_calls;
  };
  window_manager_.RegisterWindow(0, d_open);

  DockTree tree;
  tree.root = DockNode::MakeLeaf({MakePanel("in_tree")});
  ASSERT_TRUE(layout_manager_.ApplyDockTree(tree, kDockspaceId).ok());

  EXPECT_EQ(already_hidden_hide_calls, 0)
      << "Apply must not fire on_hide on a panel that was already "
         "closed — those are spurious events.";
  EXPECT_EQ(already_open_hide_calls, 1)
      << "Apply must close panels that were open and aren't in the "
         "tree (and aren't pinned).";
  EXPECT_FALSE(visible_already_open)
      << "already-open non-tree panel should be closed";
}

TEST_F(LayoutManagerDockTreeTest,
       ApplyDockTreeSkipsOpeningAlreadyOpenInTreePanels) {
  // Phase 9 review (2026-04-25): the open pass blindly called
  // OpenWindow on every in-tree panel, which fires on_show +
  // WindowVisibilityChanged even on already-open panels — think: a
  // no-op Re-apply of the active layout from the Settings combo. This
  // test pins the no-spurious-events behavior by counting on_show
  // invocations across an already-open panel and an already-hidden
  // panel that both end up in the tree.
  bool visible_already_open = true;
  bool visible_already_hidden = false;
  int already_open_show_calls = 0;
  int already_hidden_show_calls = 0;

  WindowDescriptor d_open{};
  d_open.card_id = "already_open";
  d_open.display_name = "Open";
  d_open.icon = "ICON_MD_ACCOUNT_TREE";
  d_open.category = "Test";
  d_open.priority = 1;
  d_open.visibility_flag = &visible_already_open;
  d_open.on_show = [&]() {
    ++already_open_show_calls;
  };
  window_manager_.RegisterWindow(0, d_open);

  WindowDescriptor d_hidden = d_open;
  d_hidden.card_id = "already_hidden";
  d_hidden.display_name = "Hidden";
  d_hidden.visibility_flag = &visible_already_hidden;
  d_hidden.on_show = [&]() {
    ++already_hidden_show_calls;
  };
  window_manager_.RegisterWindow(0, d_hidden);

  DockTree tree;
  tree.root =
      DockNode::MakeSplit(SplitDirection::kLeft, 0.5f,
                          DockNode::MakeLeaf({MakePanel("already_open")}),
                          DockNode::MakeLeaf({MakePanel("already_hidden")}));
  ASSERT_TRUE(layout_manager_.ApplyDockTree(tree, kDockspaceId).ok());

  EXPECT_EQ(already_open_show_calls, 0)
      << "Apply must not fire on_show on a panel that was already open — "
         "those are spurious events that dirty settings on a Re-apply.";
  EXPECT_EQ(already_hidden_show_calls, 1)
      << "Apply must open in-tree panels that were hidden.";
  EXPECT_TRUE(visible_already_hidden)
      << "previously-hidden in-tree panel should be opened";
  EXPECT_TRUE(visible_already_open)
      << "already-open in-tree panel should remain open";
}

TEST_F(LayoutManagerDockTreeTest, ApplyDockTreeClosesNonPinnedNonTreePanels) {
  // Phase 8.2 review (2026-04-25): apply must hide panels that are NOT
  // in the tree, otherwise the saved layout doesn't faithfully restore
  // when it's a subset of the visible workspace. Pinned panels are
  // exempt — they're force-pinned for cross-editor reachability and
  // closing them silently would hide functionality the user didn't
  // ask to lose.
  bool visible_in_tree = false;
  bool visible_extra = true;   // already open, not in tree
  bool visible_pinned = true;  // already open, not in tree, but pinned
  WindowDescriptor d_in{};
  d_in.card_id = "in_tree";
  d_in.display_name = "InTree";
  d_in.icon = "ICON_MD_ACCOUNT_TREE";
  d_in.category = "Test";
  d_in.priority = 1;
  d_in.visibility_flag = &visible_in_tree;
  window_manager_.RegisterWindow(0, d_in);

  WindowDescriptor d_extra = d_in;
  d_extra.card_id = "extra";
  d_extra.display_name = "Extra";
  d_extra.visibility_flag = &visible_extra;
  window_manager_.RegisterWindow(0, d_extra);

  WindowDescriptor d_pinned = d_in;
  d_pinned.card_id = "pinned";
  d_pinned.display_name = "Pinned";
  d_pinned.visibility_flag = &visible_pinned;
  window_manager_.RegisterWindow(0, d_pinned);
  window_manager_.SetWindowPinned(0, "pinned", true);

  DockTree tree;
  tree.root = DockNode::MakeLeaf({MakePanel("in_tree")});
  ASSERT_TRUE(layout_manager_.ApplyDockTree(tree, kDockspaceId).ok());

  EXPECT_TRUE(visible_in_tree)
      << "panel referenced in the tree must be open after apply";
  EXPECT_FALSE(visible_extra)
      << "panel that is neither in the tree nor pinned must be closed "
         "after apply, so a subset layout doesn't leave ghosts";
  EXPECT_TRUE(visible_pinned)
      << "pinned panel survives apply — force-pinning is a deliberate "
         "user/admin choice for cross-editor reachability";
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
