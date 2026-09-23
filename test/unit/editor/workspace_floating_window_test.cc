#include "app/editor/system/workspace/workspace_window_manager.h"

#include "app/gui/animation/animator.h"
#include "app/gui/app/editor_layout.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze::editor {
namespace {
class FloatingTestContent final : public WindowContent {
 public:
  std::string GetId() const override { return "test.floating"; }
  std::string GetDisplayName() const override { return "Floating test"; }
  std::string GetIcon() const override { return ""; }
  std::string GetEditorCategory() const override { return "Test"; }
  bool PrefersFloating() const override { return true; }
  float GetPreferredWidth() const override { return 700; }
  float GetPreferredHeight() const override { return 500; }
  void Draw(bool*) override {
    dock_id = ImGui::GetWindowDockID();
    size = ImGui::GetWindowSize();
    ++draws;
  }
  ImGuiID dock_id = 0;
  ImVec2 size;
  int draws = 0;
};

class WorkspaceFloatingWindowTest : public testing::Test {
 protected:
  void SetUp() override {
    context_ = ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1600, 1000);
    io.DeltaTime = 1.0f / 60;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    unsigned char* pixels;
    int w, h;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    was_reduced_ = gui::GetAnimator().reduced_motion();
    gui::GetAnimator().SetMotionPreferences(
        true, gui::GetAnimator().motion_profile());
    manager_ = std::make_unique<WorkspaceWindowManager>();
    manager_->SetActiveCategory("Test", false);
  }
  void TearDown() override {
    manager_.reset();
    gui::GetAnimator().SetMotionPreferences(
        was_reduced_, gui::GetAnimator().motion_profile());
    ImGui::DestroyContext(context_);
  }
  FloatingTestContent* Register(size_t session) {
    manager_->RegisterSession(session);
    manager_->SetActiveSession(session);
    auto panel = std::make_unique<FloatingTestContent>();
    auto* result = panel.get();
    manager_->RegisterWindowContent(std::move(panel));
    EXPECT_TRUE(manager_->OpenWindow(session, "test.floating"));
    return result;
  }
  void Frame(size_t session, bool dock = false) {
    manager_->SetActiveSession(session);
    ImGui::NewFrame();
    gui::PanelWindow::ResetFrameTracking();
    const ImGuiID node = 0x4400 + session;
    if (dock) {
      ImGui::DockBuilderAddNode(node, ImGuiDockNodeFlags_DockSpace);
      ImGui::DockBuilderSetNodeSize(node, ImVec2(300, 600));
      const auto name =
          "Floating test##" +
          manager_->GetWindowDescriptor(session, "test.floating")->card_id;
      ImGui::DockBuilderDockWindow(name.c_str(), node);
      ImGui::DockBuilderFinish(node);
    }
    ImGui::DockSpace(node, ImVec2(0, 0), ImGuiDockNodeFlags_KeepAliveOnly);
    manager_->DrawAllVisiblePanels();
    ImGui::EndFrame();
    ImGui::Render();
  }
  ImGuiContext* context_ = nullptr;
  std::unique_ptr<WorkspaceWindowManager> manager_;
  bool was_reduced_ = false;
};

TEST_F(WorkspaceFloatingWindowTest, ExplicitOpenUndocksOnceAndRestoresSize) {
  auto* panel = Register(0);
  Frame(0, true);
  ASSERT_NE(panel->dock_id, 0u);
  ASSERT_TRUE(manager_->OpenWindowFloating(0, "test.floating"));
  Frame(0);
  EXPECT_EQ(panel->dock_id, 0u);
  EXPECT_FLOAT_EQ(panel->size.x, 700);
  EXPECT_FLOAT_EQ(panel->size.y, 500);
  Frame(0, true);
  EXPECT_NE(panel->dock_id, 0u);  // User can dock after the one-time request.
}

TEST_F(WorkspaceFloatingWindowTest, RequestDoesNotLeakToAnotherRomSession) {
  auto* first = Register(0);
  Frame(0, true);
  auto* second = Register(1);
  Frame(1, true);
  ASSERT_TRUE(manager_->OpenWindowFloating(0, "test.floating"));
  Frame(1);
  EXPECT_NE(second->dock_id, 0u);
  Frame(0);
  EXPECT_EQ(first->dock_id, 0u);
}

TEST_F(WorkspaceFloatingWindowTest, FailedOpenLeavesNoPendingRequest) {
  manager_->RegisterSession(0);
  EXPECT_FALSE(manager_->OpenWindowFloating(0, "test.floating"));
  auto* panel = Register(0);
  Frame(0, true);
  EXPECT_NE(panel->dock_id, 0u);
}

TEST_F(WorkspaceFloatingWindowTest, ClosingBeforeDrawingCancelsRequest) {
  auto* panel = Register(0);
  Frame(0, true);
  ASSERT_TRUE(manager_->OpenWindowFloating(0, "test.floating"));
  ASSERT_TRUE(manager_->CloseWindow(0, "test.floating"));
  ASSERT_TRUE(manager_->OpenWindow(0, "test.floating"));
  Frame(0);
  EXPECT_NE(panel->dock_id, 0u);
}

TEST_F(WorkspaceFloatingWindowTest, SessionRetirementCancelsRequest) {
  Register(0);
  ASSERT_TRUE(manager_->OpenWindowFloating(0, "test.floating"));
  manager_->UnregisterSession(0);
  auto* panel = Register(0);
  Frame(0, true);
  EXPECT_NE(panel->dock_id, 0u);
}

TEST_F(WorkspaceFloatingWindowTest, BatchHideCancelsRequest) {
  auto* panel = Register(0);
  Frame(0, true);
  ASSERT_TRUE(manager_->OpenWindowFloating(0, "test.floating"));
  manager_->HideAllWindowsInSession(0);
  ASSERT_TRUE(manager_->OpenWindow(0, "test.floating"));
  Frame(0);
  EXPECT_NE(panel->dock_id, 0u);
}
}  // namespace
}  // namespace yaze::editor
