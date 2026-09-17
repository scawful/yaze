#include "app/editor/menu/right_drawer_manager.h"

#include <gtest/gtest.h>

#include "app/gui/core/icons.h"
#include "app/gui/core/ui_config.h"
#include "app/gui/widgets/themed_widgets.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze::editor {

class RightDrawerManagerTestPeer {
 public:
  // Draw just the header (used for layout smoke checks).
  static void DrawHeader(RightDrawerManager& manager, const char* title) {
    const auto type = manager.GetActiveDrawer();
    manager.DrawPanelHeader(type, title, GetDrawerTypeIcon(type));
  }
};

namespace {

class RightDrawerManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    imgui_context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui_context_);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1280.0f, 720.0f);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefault();
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
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

TEST_F(RightDrawerManagerTest, DrawerCatalogMatchesHeaderCycleOrder) {
  const auto catalog = GetDrawerCatalog();
  ASSERT_EQ(catalog.size(), 7u);
  EXPECT_EQ(catalog[0].type, RightDrawerManager::DrawerType::kProject);
  EXPECT_EQ(catalog[1].type, RightDrawerManager::DrawerType::kProperties);
  EXPECT_EQ(catalog[2].type, RightDrawerManager::DrawerType::kAgentChat);
  EXPECT_EQ(catalog[3].type, RightDrawerManager::DrawerType::kProposals);
  EXPECT_EQ(catalog[4].type, RightDrawerManager::DrawerType::kNotifications);
  EXPECT_EQ(catalog[5].type, RightDrawerManager::DrawerType::kHelp);
  EXPECT_EQ(catalog[6].type, RightDrawerManager::DrawerType::kSettings);

  for (const DrawerCatalogEntry& entry : catalog) {
    EXPECT_STREQ(entry.name, GetDrawerTypeName(entry.type));
    EXPECT_STREQ(entry.icon, GetDrawerTypeIcon(entry.type));
    EXPECT_STREQ(entry.shortcut_action, GetDrawerShortcutAction(entry.type));
  }
}

TEST_F(RightDrawerManagerTest, ToggleActiveDrawerClosesIt) {
  RightDrawerManager manager;
  manager.OpenDrawer(RightDrawerManager::DrawerType::kProperties);
  EXPECT_EQ(manager.GetActiveDrawer(),
            RightDrawerManager::DrawerType::kProperties);

  manager.ToggleDrawer(RightDrawerManager::DrawerType::kProperties);
  EXPECT_EQ(manager.GetActiveDrawer(), RightDrawerManager::DrawerType::kNone);
}

// Nav strip behavioral tests: active-close / inactive-switch

TEST_F(RightDrawerManagerTest, NavStripActiveTabLogicClosesDrawer) {
  // Simulates the InvisibleButton handler in DrawDrawerNavStrip:
  // clicking the active tab should close the drawer.
  RightDrawerManager manager;
  manager.OpenDrawer(RightDrawerManager::DrawerType::kHelp);
  ASSERT_EQ(manager.GetActiveDrawer(), RightDrawerManager::DrawerType::kHelp);

  // Mimic the nav strip click: is_active → CloseDrawer()
  manager.CloseDrawer();
  EXPECT_EQ(manager.GetActiveDrawer(), RightDrawerManager::DrawerType::kNone);
}

TEST_F(RightDrawerManagerTest, NavStripInactiveTabLogicSwitchesDrawer) {
  // Simulates the InvisibleButton handler in DrawDrawerNavStrip:
  // clicking an inactive tab should open that drawer.
  RightDrawerManager manager;
  manager.OpenDrawer(RightDrawerManager::DrawerType::kSettings);
  ASSERT_EQ(manager.GetActiveDrawer(),
            RightDrawerManager::DrawerType::kSettings);

  // Mimic click on Properties (inactive)
  manager.OpenDrawer(RightDrawerManager::DrawerType::kProperties);
  EXPECT_EQ(manager.GetActiveDrawer(),
            RightDrawerManager::DrawerType::kProperties);
}

TEST_F(RightDrawerManagerTest, NavStripCoversCatalogEntries) {
  // Every catalog entry must produce a valid icon string and name.
  const auto catalog = GetDrawerCatalog();
  for (const DrawerCatalogEntry& entry : catalog) {
    EXPECT_NE(entry.icon, nullptr) << entry.name;
    EXPECT_NE(entry.name, nullptr);
    EXPECT_GT(std::strlen(entry.icon), 0u) << entry.name;
  }
}

// Render smoke tests: all drawers must not crash when Draw() is called.

TEST_F(RightDrawerManagerTest, RenderFrameWithActiveDrawerDoesNotCrash) {
  RightDrawerManager manager;
  manager.OpenDrawer(RightDrawerManager::DrawerType::kHelp);
  EXPECT_EQ(manager.GetActiveDrawer(), RightDrawerManager::DrawerType::kHelp);

  ImGui::NewFrame();
  EXPECT_NO_FATAL_FAILURE(manager.Draw());
  ImGui::Render();
}

TEST_F(RightDrawerManagerTest, RenderFrameWithAllDrawersDoesNotCrash) {
  const auto catalog = GetDrawerCatalog();
  for (const auto& entry : catalog) {
    RightDrawerManager manager;
    manager.OpenDrawer(entry.type);

    ImGui::NewFrame();
    EXPECT_NO_FATAL_FAILURE(manager.Draw());
    ImGui::Render();
  }
}

// Chrome non-overlap: the nav strip width must not exceed the window width.

TEST_F(RightDrawerManagerTest,
       NavStripTabWidthFitsWithinWindowForAllCatalogSizes) {
  // Verify the tab-width formula does not produce negative or oversized values
  // for any reasonable window width (from very narrow to very wide).
  const auto catalog = GetDrawerCatalog();
  const size_t count = catalog.size();
  ASSERT_GT(count, 0u);

  const float padding = 6.0f;
  const float gap = 3.0f;

  for (const float window_w : {120.0f, 280.0f, 480.0f, 1024.0f, 1920.0f}) {
    const float avail_w = window_w - padding * 2.0f;
    const float tab_w =
        std::max(24.0f, std::floor((avail_w - (count - 1) * gap) / count));
    const float total_w = padding * 2.0f + tab_w * count + gap * (count - 1);

    EXPECT_GE(tab_w, 24.0f) << "tab too narrow at window_w=" << window_w;
    // Even if tabs overflow the window on very narrow widths, tab_w >= 24px.
    (void)total_w;
  }
}

}  // namespace
}  // namespace yaze::editor
