#include "app/editor/menu/status_bar.h"

#include <gtest/gtest.h>

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze::editor {
namespace {

TEST(StatusBarContextTest,
     ActiveEditorAndDirtyScopeAreIndependentOfContributions) {
  StatusBar bar;

  bar.SetActiveEditor("Dungeon");
  bar.SetDirtyScope("Rooms+ROM");
  bar.SetCustomSegment("Room", "0x001");

  EXPECT_TRUE(bar.has_active_editor_for_test());
  EXPECT_EQ(bar.active_editor_for_test(), "Dungeon");
  EXPECT_TRUE(bar.has_dirty_scope_for_test());
  EXPECT_EQ(bar.dirty_scope_for_test(), "Rooms+ROM");

  // Editor contributions clear mode/custom only — context strip stays.
  bar.ClearEditorContributions();
  EXPECT_TRUE(bar.has_active_editor_for_test());
  EXPECT_TRUE(bar.has_dirty_scope_for_test());

  bar.ClearActiveEditor();
  bar.ClearDirtyScope();
  EXPECT_FALSE(bar.has_active_editor_for_test());
  EXPECT_FALSE(bar.has_dirty_scope_for_test());
}

TEST(StatusBarContextTest, SessionDisplayNameStoredForMultiSession) {
  StatusBar bar;
  bar.SetSessionInfo(1, 3, "Session 1 (oos168)");
  EXPECT_EQ(bar.session_display_name_for_test(), "Session 1 (oos168)");

  bar.SetSessionInfo(0, 1);
  EXPECT_TRUE(bar.session_display_name_for_test().empty());
}

TEST(StatusBarContextTest, EmptyLabelsClearSegments) {
  StatusBar bar;
  bar.SetActiveEditor("Overworld");
  bar.SetDirtyScope("Project");
  bar.SetActiveEditor("");
  bar.SetDirtyScope("");
  EXPECT_FALSE(bar.has_active_editor_for_test());
  EXPECT_FALSE(bar.has_dirty_scope_for_test());
}

class StatusBarContextClickTest : public ::testing::Test {
 protected:
  void SetUp() override {
    previous_context_ = ImGui::GetCurrentContext();
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1000.0f, 600.0f);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.AddMousePosEvent(-1000.0f, -1000.0f);
    bar_.SetEnabled(true);
  }

  void TearDown() override {
    ImGui::DestroyContext(context_);
    ImGui::SetCurrentContext(previous_context_);
  }

  void DrawFrame() {
    ImGui::NewFrame();
    bar_.Draw();
    ImGui::Render();
  }

  ImVec2 LastSegmentClickPoint() {
    const auto* window = ImGui::FindWindowByName("##StatusBar");
    EXPECT_NE(window, nullptr);
    if (!window) {
      return ImVec2(-1000.0f, -1000.0f);
    }
    // These fixtures draw no right-side contributions. ImGui's last laid-out
    // item is therefore the final context chip; use its right edge/line height
    // instead of hard-coding screen coordinates or exposing a production API.
    return ImVec2(
        window->DC.CursorPosPrevLine.x - 1.0f,
        window->DC.CursorPosPrevLine.y + window->DC.PrevLineSize.y * 0.5f);
  }

  void ShowContext(int& dirty_clicks, int& editor_clicks) {
    bar_.SetDirtyScope("ROM", {[&dirty_clicks]() { ++dirty_clicks; },
                               "Save pending ROM work"});
    DrawFrame();
    DrawFrame();
    dirty_point_ = LastSegmentClickPoint();

    bar_.SetActiveEditor(
        "Dungeon", {[&editor_clicks]() { ++editor_clicks; }, "Switch editor"});
    DrawFrame();
    editor_point_ = LastSegmentClickPoint();
  }

  void Click(const ImVec2& point) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(point.x, point.y);
    DrawFrame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    DrawFrame();
    DrawFrame();  // Holding must not repeat the callback.
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    DrawFrame();
  }

  StatusBar bar_;
  ImVec2 dirty_point_;
  ImVec2 editor_point_;

 private:
  ImGuiContext* previous_context_ = nullptr;
  ImGuiContext* context_ = nullptr;
};

TEST_F(StatusBarContextClickTest, ChipsInvokeOnlyTheirCallbackOncePerClick) {
  int dirty_clicks = 0;
  int editor_clicks = 0;
  ShowContext(dirty_clicks, editor_clicks);

  Click(dirty_point_);
  EXPECT_EQ(dirty_clicks, 1);
  EXPECT_EQ(editor_clicks, 0);
  Click(editor_point_);
  EXPECT_EQ(dirty_clicks, 1);
  EXPECT_EQ(editor_clicks, 1);
}

TEST_F(StatusBarContextClickTest, ClearAllContextRemovesClickTargets) {
  int dirty_clicks = 0;
  int editor_clicks = 0;
  ShowContext(dirty_clicks, editor_clicks);

  bar_.ClearAllContext();
  DrawFrame();
  EXPECT_FALSE(bar_.has_dirty_scope_for_test());
  EXPECT_FALSE(bar_.has_active_editor_for_test());
  Click(dirty_point_);
  Click(editor_point_);
  EXPECT_EQ(dirty_clicks, 0);
  EXPECT_EQ(editor_clicks, 0);
}

TEST_F(StatusBarContextClickTest, ReplacingSessionContextUsesNewCallbacks) {
  int first_dirty_clicks = 0;
  int first_editor_clicks = 0;
  ShowContext(first_dirty_clicks, first_editor_clicks);
  bar_.SetSessionInfo(0, 2, "First session");
  DrawFrame();

  // Model the manager replacing both chips for the next active session. This
  // tests callback ownership only, not EditorManager's SaveRom implementation.
  int next_dirty_clicks = 0;
  int next_editor_clicks = 0;
  bar_.SetSessionInfo(1, 2, "Next session");
  bar_.SetDirtyScope("ROM", {[&]() { ++next_dirty_clicks; }, "Next ROM work"});
  bar_.SetActiveEditor("Dungeon",
                       {[&]() { ++next_editor_clicks; }, "Next editor"});
  DrawFrame();
  editor_point_ = LastSegmentClickPoint();
  // Dirty scope precedes the session label, so its original position is stable.
  Click(dirty_point_);
  Click(editor_point_);
  EXPECT_EQ(next_dirty_clicks, 1);
  EXPECT_EQ(next_editor_clicks, 1);
  EXPECT_EQ(first_dirty_clicks, 0);
  EXPECT_EQ(first_editor_clicks, 0);
}

}  // namespace
}  // namespace yaze::editor
