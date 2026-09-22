#include "app/editor/dungeon/ui/window/object_coverage_panel.h"

#include "gtest/gtest.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze::editor {

class ObjectCoveragePanelTestPeer {
 public:
  static void DrawDetails(ObjectCoveragePanel& panel) { panel.DrawDetails(); }
};

namespace {

class ObjectCoveragePanelTest : public ::testing::Test {
 protected:
  void SetUp() override {
    IMGUI_CHECKVERSION();
    context_ = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(800.0f, 600.0f);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefault();
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  }

  void TearDown() override { ImGui::DestroyContext(context_); }

  void DrawDetailsAndCheckParent(ObjectCoveragePanel& panel) {
    ImGui::NewFrame();
    ImGui::SetNextWindowSize(ImVec2(700.0f, 500.0f), ImGuiCond_Always);
    ImGui::Begin("ObjectCoverageHost");
    ImGuiWindow* parent = ImGui::GetCurrentWindow();
    const int window_depth = context_->CurrentWindowStack.Size;

    ObjectCoveragePanelTestPeer::DrawDetails(panel);

    EXPECT_EQ(ImGui::GetCurrentWindow(), parent);
    EXPECT_EQ(context_->CurrentWindowStack.Size, window_depth);
    ImGui::TextUnformatted("Parent content after object details");
    ImGui::End();
    ImGui::Render();
  }

 private:
  ImGuiContext* context_ = nullptr;
};

TEST_F(ObjectCoveragePanelTest, UnplacedObjectKeepsParentWindowActive) {
  ObjectCoveragePanel panel;
  // An empty usage index models a mapped object absent from this ROM. Users
  // reach this state by disabling "Placed only" and selecting that object.
  panel.FocusObject(0x04C, 0x042);
  DrawDetailsAndCheckParent(panel);
  DrawDetailsAndCheckParent(panel);
}

TEST_F(ObjectCoveragePanelTest, NoSelectionKeepsParentWindowActive) {
  ObjectCoveragePanel panel;
  DrawDetailsAndCheckParent(panel);
}

}  // namespace
}  // namespace yaze::editor
