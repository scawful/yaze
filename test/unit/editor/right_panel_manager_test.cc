#include "app/editor/menu/right_panel_manager.h"

#include <gtest/gtest.h>

#include "imgui/imgui.h"

namespace yaze::editor {
namespace {

class RightPanelManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    imgui_context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui_context_);
    ImGuiIO& io = ImGui::GetIO();
    io.DeltaTime = 1.0f / 60.0f;
  }

  void TearDown() override {
    if (imgui_context_ != nullptr) {
      ImGui::DestroyContext(imgui_context_);
      imgui_context_ = nullptr;
    }
  }

  ImGuiContext* imgui_context_ = nullptr;
};

TEST_F(RightPanelManagerTest, CyclePanelNoopWhenNoPanelIsActive) {
  RightPanelManager manager;
  EXPECT_EQ(manager.GetActivePanel(), RightPanelManager::PanelType::kNone);

  manager.CycleToNextPanel();
  EXPECT_EQ(manager.GetActivePanel(), RightPanelManager::PanelType::kNone);

  manager.CycleToPreviousPanel();
  EXPECT_EQ(manager.GetActivePanel(), RightPanelManager::PanelType::kNone);
}

TEST_F(RightPanelManagerTest, CyclePanelAdvancesAndWrapsUsingHeaderOrder) {
  RightPanelManager manager;

  manager.OpenPanel(RightPanelManager::PanelType::kProject);
  manager.CycleToNextPanel();
  EXPECT_EQ(manager.GetActivePanel(),
            RightPanelManager::PanelType::kProperties);

  manager.CycleToNextPanel();
  EXPECT_EQ(manager.GetActivePanel(), RightPanelManager::PanelType::kAgentChat);

  manager.CycleToPreviousPanel();
  EXPECT_EQ(manager.GetActivePanel(),
            RightPanelManager::PanelType::kProperties);

  manager.OpenPanel(RightPanelManager::PanelType::kSettings);
  manager.CycleToNextPanel();
  EXPECT_EQ(manager.GetActivePanel(), RightPanelManager::PanelType::kProject);

  manager.CycleToPreviousPanel();
  EXPECT_EQ(manager.GetActivePanel(), RightPanelManager::PanelType::kSettings);
}

TEST_F(RightPanelManagerTest, CyclePanelUsesDirectionSign) {
  RightPanelManager manager;
  manager.OpenPanel(RightPanelManager::PanelType::kProposals);

  manager.CyclePanel(99);
  EXPECT_EQ(manager.GetActivePanel(),
            RightPanelManager::PanelType::kNotifications);

  manager.CyclePanel(-3);
  EXPECT_EQ(manager.GetActivePanel(), RightPanelManager::PanelType::kProposals);
}

}  // namespace
}  // namespace yaze::editor
