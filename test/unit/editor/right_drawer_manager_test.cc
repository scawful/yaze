#include "app/editor/menu/right_drawer_manager.h"

#include <gtest/gtest.h>

#include "imgui/imgui.h"

namespace yaze::editor {
namespace {

class RightDrawerManagerTest : public ::testing::Test {
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

TEST_F(RightDrawerManagerTest, CyclePanelNoopWhenNoPanelIsActive) {
  RightDrawerManager manager;
  EXPECT_EQ(manager.GetActiveDrawer(), RightDrawerManager::DrawerType::kNone);

  manager.CycleToNextDrawer();
  EXPECT_EQ(manager.GetActiveDrawer(), RightDrawerManager::DrawerType::kNone);

  manager.CycleToPreviousDrawer();
  EXPECT_EQ(manager.GetActiveDrawer(), RightDrawerManager::DrawerType::kNone);
}

TEST_F(RightDrawerManagerTest, CyclePanelAdvancesAndWrapsUsingHeaderOrder) {
  RightDrawerManager manager;

  manager.OpenDrawer(RightDrawerManager::DrawerType::kProject);
  manager.CycleToNextDrawer();
  EXPECT_EQ(manager.GetActiveDrawer(),
            RightDrawerManager::DrawerType::kProperties);

  manager.CycleToNextDrawer();
  EXPECT_EQ(manager.GetActiveDrawer(),
            RightDrawerManager::DrawerType::kAgentChat);

  manager.CycleToPreviousDrawer();
  EXPECT_EQ(manager.GetActiveDrawer(),
            RightDrawerManager::DrawerType::kProperties);

  manager.OpenDrawer(RightDrawerManager::DrawerType::kSettings);
  manager.CycleToNextDrawer();
  EXPECT_EQ(manager.GetActiveDrawer(),
            RightDrawerManager::DrawerType::kProject);

  manager.CycleToPreviousDrawer();
  EXPECT_EQ(manager.GetActiveDrawer(),
            RightDrawerManager::DrawerType::kSettings);
}

TEST_F(RightDrawerManagerTest, CyclePanelUsesDirectionSign) {
  RightDrawerManager manager;
  manager.OpenDrawer(RightDrawerManager::DrawerType::kProposals);

  manager.CycleDrawer(99);
  EXPECT_EQ(manager.GetActiveDrawer(),
            RightDrawerManager::DrawerType::kNotifications);

  manager.CycleDrawer(-3);
  EXPECT_EQ(manager.GetActiveDrawer(),
            RightDrawerManager::DrawerType::kProposals);
}

TEST_F(RightDrawerManagerTest, StoresToolOutputAndCanOpenToolDrawer) {
  RightDrawerManager manager;

  manager.SetToolOutput("Project Graph Lookup", "project-graph --query=lookup",
                        "{\"address\":\"$008000\"}");
  manager.OpenDrawer(RightDrawerManager::DrawerType::kToolOutput);

  EXPECT_EQ(manager.GetActiveDrawer(),
            RightDrawerManager::DrawerType::kToolOutput);
  EXPECT_EQ(manager.tool_output_title(), "Project Graph Lookup");
  EXPECT_EQ(manager.tool_output_query(), "project-graph --query=lookup");
  EXPECT_EQ(manager.tool_output_content(), "{\"address\":\"$008000\"}");
}

}  // namespace
}  // namespace yaze::editor
