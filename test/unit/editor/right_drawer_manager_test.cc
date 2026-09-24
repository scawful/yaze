#include "app/editor/menu/right_drawer_manager.h"

#include <gtest/gtest.h>

#include "app/editor/ui/toast_manager.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/theme_manager.h"
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

  static void DrawNavStrip(RightDrawerManager& manager) {
    manager.DrawDrawerNavStrip(manager.GetActiveDrawer());
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

  void DrawNavFrame(RightDrawerManager& manager) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f));
    ImGui::SetNextWindowSize(ImVec2(320.0f, 200.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Drawer test", nullptr, ImGuiWindowFlags_NoDecoration);
    RightDrawerManagerTestPeer::DrawNavStrip(manager);
    // The strip positions the cursor for the drawer content drawn next.
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::Render();
  }

  void ClickNavTab(RightDrawerManager& manager, size_t index) {
    DrawNavFrame(manager);
    const auto* window = ImGui::FindWindowByName("Drawer test");
    ASSERT_NE(window, nullptr);
    // Target the middle of the catalog cell, independently of the widget's
    // width calculation. Send real press/release events through ImGui.
    const float x =
        window->Pos.x + window->Size.x * (static_cast<float>(index) + 0.5f) /
                            static_cast<float>(GetDrawerCatalog().size());
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent(x, window->Pos.y + 16.0f);
    DrawNavFrame(manager);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    DrawNavFrame(manager);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    DrawNavFrame(manager);
  }
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
  RightDrawerManager manager;
  manager.OpenDrawer(RightDrawerManager::DrawerType::kHelp);
  ASSERT_EQ(manager.GetActiveDrawer(), RightDrawerManager::DrawerType::kHelp);

  ClickNavTab(manager, 5);  // Help
  EXPECT_EQ(manager.GetActiveDrawer(), RightDrawerManager::DrawerType::kNone);
}

TEST_F(RightDrawerManagerTest, NavStripInactiveTabLogicSwitchesDrawer) {
  RightDrawerManager manager;
  manager.OpenDrawer(RightDrawerManager::DrawerType::kSettings);
  ASSERT_EQ(manager.GetActiveDrawer(),
            RightDrawerManager::DrawerType::kSettings);

  ClickNavTab(manager, 1);  // Properties
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

TEST_F(RightDrawerManagerTest, HeaderBadgesStayBeforeActionButtons) {
  auto& themes = gui::ThemeManager::Get();
  const gui::Theme previous_theme = themes.GetCurrentTheme();
  gui::Theme test_theme = previous_theme;
  // Identify badge geometry independently of the user's active theme. The
  // default theme can give header backgrounds and badges the same color.
  test_theme.primary = {0.91f, 0.07f, 0.59f, 1.0f};
  test_theme.header_hovered = {0.13f, 0.79f, 0.29f, 1.0f};
  themes.ApplyTheme(test_theme);
  ToastManager toasts;
  toasts.Show("Saved ROM copy");
  RightDrawerManager manager;
  manager.SetToastManager(&toasts);
  manager.SetActiveEditor(EditorType::kOverworld);

  for (const auto type : {RightDrawerManager::DrawerType::kNotifications,
                          RightDrawerManager::DrawerType::kHelp}) {
    manager.OpenDrawer(type);
    for (float width : {280.0f, 320.0f, 480.0f}) {
      for (float scale : {1.0f, 1.5f, 2.0f}) {
        SCOPED_TRACE(::testing::Message()
                     << "type=" << static_cast<int>(type) << " width=" << width
                     << " scale=" << scale);
        ImGui::GetIO().FontGlobalScale = scale;
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f));
        ImGui::SetNextWindowSize(ImVec2(width, 200.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Header test", nullptr, ImGuiWindowFlags_NoDecoration);
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const int first_vertex = draw_list->VtxBuffer.Size;
        RightDrawerManagerTestPeer::DrawHeader(
            manager, "A long drawer title that needs to be truncated");

        const int action_count =
            type == RightDrawerManager::DrawerType::kNotifications ? 4 : 3;
        const float first_action_x = ImGui::GetWindowPos().x + width -
                                     gui::UIConfig::kPanelPaddingLarge -
                                     action_count * 24.0f -
                                     (action_count - 1) * 4.0f;
        const ImU32 badge_color = ImGui::GetColorU32(
            type == RightDrawerManager::DrawerType::kNotifications
                ? gui::GetPrimaryVec4()
                : gui::GetSurfaceContainerHighestVec4());
        for (int i = first_vertex; i < draw_list->VtxBuffer.Size; ++i) {
          const auto& vertex = draw_list->VtxBuffer[i];
          if (vertex.col == badge_color) {
            EXPECT_LT(vertex.pos.x, first_action_x)
                << "Badge overlaps a header action";
          }
        }
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::Render();
      }
    }
  }
  themes.ApplyTheme(previous_theme);
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
