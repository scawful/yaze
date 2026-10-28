#include "app/gui/core/style_guard.h"

#include <gtest/gtest.h>

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze {
namespace gui {
namespace {

// The ImGui stacks each scope guard touches. The ID stack lives on the
// current window in ImGui 1.93; the others live on the context.
struct StackState {
  int id_stack_size = 0;
  ImGuiID id_stack_top = 0;
  int disabled_stack_size = 0;
  int item_flags_stack_size = 0;
  ImGuiItemFlags item_flags = 0;
  float alpha = 0.0f;
};

StackState CaptureStacks() {
  const ImGuiContext& g = *GImGui;
  StackState state;
  state.id_stack_size = g.CurrentWindow->IDStack.Size;
  state.id_stack_top = g.CurrentWindow->IDStack.back();
  state.disabled_stack_size = g.DisabledStackSize;
  state.item_flags_stack_size = g.ItemFlagsStack.Size;
  state.item_flags = g.CurrentItemFlags;
  state.alpha = g.Style.Alpha;
  return state;
}

void ExpectStacksRestored(const StackState& before) {
  const StackState after = CaptureStacks();
  EXPECT_EQ(after.id_stack_size, before.id_stack_size);
  EXPECT_EQ(after.id_stack_top, before.id_stack_top);
  EXPECT_EQ(after.disabled_stack_size, before.disabled_stack_size);
  EXPECT_EQ(after.item_flags_stack_size, before.item_flags_stack_size);
  EXPECT_EQ(after.item_flags, before.item_flags);
  EXPECT_FLOAT_EQ(after.alpha, before.alpha);
}

bool CurrentItemFlagSet(ImGuiItemFlags flag) {
  return (GImGui->CurrentItemFlags & flag) != 0;
}

// Each test body runs inside one window of one frame, so the guards see a
// valid CurrentWindow. TearDown ends the frame; End() and EndFrame() would
// report an unbalanced stack if a guard leaked a push.
class StyleGuardScopesTest : public ::testing::Test {
 protected:
  void SetUp() override {
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(640.0f, 480.0f);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    ImGui::NewFrame();
    ImGui::Begin("StyleGuardScopesHost", nullptr,
                 ImGuiWindowFlags_NoSavedSettings);
  }

  void TearDown() override {
    ImGui::End();
    ImGui::EndFrame();
    ImGui::DestroyContext(context_);
    context_ = nullptr;
  }

  ImGuiContext* context_ = nullptr;
};

TEST_F(StyleGuardScopesTest, IdScopeStringPushesSameIdAsPushId) {
  const StackState before = CaptureStacks();
  const ImGuiID expected = GImGui->CurrentWindow->GetID("row");
  {
    IdScope id("row");
    EXPECT_EQ(GImGui->CurrentWindow->IDStack.Size, before.id_stack_size + 1);
    EXPECT_EQ(GImGui->CurrentWindow->IDStack.back(), expected);
  }
  ExpectStacksRestored(before);
}

TEST_F(StyleGuardScopesTest, IdScopeIntPushesSameIdAsPushId) {
  const StackState before = CaptureStacks();
  const ImGuiID expected = GImGui->CurrentWindow->GetID(7);
  {
    IdScope id(7);
    EXPECT_EQ(GImGui->CurrentWindow->IDStack.Size, before.id_stack_size + 1);
    EXPECT_EQ(GImGui->CurrentWindow->IDStack.back(), expected);
  }
  ExpectStacksRestored(before);
}

TEST_F(StyleGuardScopesTest, IdScopePointerPushesSameIdAsPushId) {
  const int marker = 0;
  const StackState before = CaptureStacks();
  const ImGuiID expected = GImGui->CurrentWindow->GetID(&marker);
  {
    IdScope id(&marker);
    EXPECT_EQ(GImGui->CurrentWindow->IDStack.Size, before.id_stack_size + 1);
    EXPECT_EQ(GImGui->CurrentWindow->IDStack.back(), expected);
  }
  ExpectStacksRestored(before);
}

// The same label must hash to different IDs in two sibling scopes; this is
// what call sites rely on when they wrap a loop body in IdScope.
TEST_F(StyleGuardScopesTest, IdScopeGivesSameLabelDifferentIdsPerScope) {
  ImGuiID first = 0;
  ImGuiID second = 0;
  {
    IdScope id(0);
    first = ImGui::GetID("Delete");
  }
  {
    IdScope id(1);
    second = ImGui::GetID("Delete");
  }
  EXPECT_NE(first, second);
  EXPECT_NE(first, ImGui::GetID("Delete"));
}

TEST_F(StyleGuardScopesTest, DisabledScopeDisablesItemsAndRestores) {
  const StackState before = CaptureStacks();
  ASSERT_FALSE(CurrentItemFlagSet(ImGuiItemFlags_Disabled));
  {
    DisabledScope disabled;
    EXPECT_TRUE(CurrentItemFlagSet(ImGuiItemFlags_Disabled));
    EXPECT_EQ(GImGui->DisabledStackSize, before.disabled_stack_size + 1);
    EXPECT_EQ(GImGui->ItemFlagsStack.Size, before.item_flags_stack_size + 1);
    EXPECT_LT(GImGui->Style.Alpha, before.alpha);
  }
  ExpectStacksRestored(before);
  EXPECT_FALSE(CurrentItemFlagSet(ImGuiItemFlags_Disabled));
}

TEST_F(StyleGuardScopesTest, DisabledScopeFalseStillPushesButLeavesEnabled) {
  const StackState before = CaptureStacks();
  {
    DisabledScope disabled(false);
    EXPECT_FALSE(CurrentItemFlagSet(ImGuiItemFlags_Disabled));
    EXPECT_EQ(GImGui->DisabledStackSize, before.disabled_stack_size + 1);
    EXPECT_FLOAT_EQ(GImGui->Style.Alpha, before.alpha);
  }
  ExpectStacksRestored(before);
}

TEST_F(StyleGuardScopesTest, DisabledScopeFalseInsideDisabledStaysDisabled) {
  const StackState before = CaptureStacks();
  {
    DisabledScope outer(true);
    {
      DisabledScope inner(false);
      EXPECT_TRUE(CurrentItemFlagSet(ImGuiItemFlags_Disabled));
      EXPECT_EQ(GImGui->DisabledStackSize, before.disabled_stack_size + 2);
    }
    EXPECT_TRUE(CurrentItemFlagSet(ImGuiItemFlags_Disabled));
  }
  ExpectStacksRestored(before);
}

TEST_F(StyleGuardScopesTest, ItemFlagScopeSetsFlagAndRestores) {
  const StackState before = CaptureStacks();
  ASSERT_FALSE(CurrentItemFlagSet(ImGuiItemFlags_NoTabStop));
  {
    ItemFlagScope no_tab(ImGuiItemFlags_NoTabStop, true);
    EXPECT_TRUE(CurrentItemFlagSet(ImGuiItemFlags_NoTabStop));
    EXPECT_EQ(GImGui->ItemFlagsStack.Size, before.item_flags_stack_size + 1);
  }
  ExpectStacksRestored(before);
  EXPECT_FALSE(CurrentItemFlagSet(ImGuiItemFlags_NoTabStop));
}

// AutoClosePopups is on by default, so enabled=false is the case that clears
// a flag instead of setting one.
TEST_F(StyleGuardScopesTest, ItemFlagScopeFalseClearsDefaultOnFlag) {
  const StackState before = CaptureStacks();
  ASSERT_TRUE(CurrentItemFlagSet(ImGuiItemFlags_AutoClosePopups));
  {
    ItemFlagScope keep_open(ImGuiItemFlags_AutoClosePopups, false);
    EXPECT_FALSE(CurrentItemFlagSet(ImGuiItemFlags_AutoClosePopups));
    EXPECT_EQ(GImGui->ItemFlagsStack.Size, before.item_flags_stack_size + 1);
  }
  ExpectStacksRestored(before);
  EXPECT_TRUE(CurrentItemFlagSet(ImGuiItemFlags_AutoClosePopups));
}

TEST_F(StyleGuardScopesTest, ReadOnlyScopeSetsReadOnlyFlagAndRestores) {
  const StackState before = CaptureStacks();
  ASSERT_FALSE(CurrentItemFlagSet(ImGuiItemFlags_ReadOnly));
  {
    ReadOnlyScope read_only;
    EXPECT_TRUE(CurrentItemFlagSet(ImGuiItemFlags_ReadOnly));
    EXPECT_FALSE(CurrentItemFlagSet(ImGuiItemFlags_Disabled));
    EXPECT_EQ(GImGui->ItemFlagsStack.Size, before.item_flags_stack_size + 1);
    // Read-only items are not dimmed, unlike disabled ones.
    EXPECT_FLOAT_EQ(GImGui->Style.Alpha, before.alpha);
  }
  ExpectStacksRestored(before);
  EXPECT_FALSE(CurrentItemFlagSet(ImGuiItemFlags_ReadOnly));
}

TEST_F(StyleGuardScopesTest, ReadOnlyScopeFalseStillPushesButLeavesEditable) {
  const StackState before = CaptureStacks();
  {
    ReadOnlyScope read_only(false);
    EXPECT_FALSE(CurrentItemFlagSet(ImGuiItemFlags_ReadOnly));
    EXPECT_EQ(GImGui->ItemFlagsStack.Size, before.item_flags_stack_size + 1);
  }
  ExpectStacksRestored(before);
}

// The reason for the guards: a return from inside nested scopes must still
// pop every stack.
TEST_F(StyleGuardScopesTest, EarlyReturnInsideNestedScopesRestoresAllStacks) {
  const StackState before = CaptureStacks();
  auto draw_row = [](bool stop_early) {
    IdScope id("row");
    DisabledScope disabled(true);
    ItemFlagScope no_tab(ImGuiItemFlags_NoTabStop, true);
    ReadOnlyScope read_only;
    if (stop_early) {
      return;
    }
    ImGui::Text("unreached");
  };
  draw_row(true);
  ExpectStacksRestored(before);
  EXPECT_FALSE(CurrentItemFlagSet(ImGuiItemFlags_Disabled));
  EXPECT_FALSE(CurrentItemFlagSet(ImGuiItemFlags_NoTabStop));
  EXPECT_FALSE(CurrentItemFlagSet(ImGuiItemFlags_ReadOnly));
}

}  // namespace
}  // namespace gui
}  // namespace yaze
