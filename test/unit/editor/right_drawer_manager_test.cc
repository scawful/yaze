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
  static void DrawHeader(RightDrawerManager& manager, const char* title) {
    manager.DrawPanelHeader(title,
                            GetDrawerTypeIcon(manager.GetActiveDrawer()));
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

struct HeaderHitSnapshot {
  ImGuiID hovered_id = 0;
  ImGuiID expected_id = 0;
  ImRect chrome_rect;
};

HeaderHitSnapshot ProbeHeader(RightDrawerManager& manager, const char* title,
                              float width, float mouse_x,
                              const char* expected_icon) {
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(mouse_x, gui::UIConfig::kPanelHeaderHeight * 0.5f);
  ImGui::NewFrame();
  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
  ImGui::SetNextWindowSize(ImVec2(width, 120.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::Begin("DrawerHeaderTest", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings);
  RightDrawerManagerTestPeer::DrawHeader(manager, title);
  const HeaderHitSnapshot result{
      .hovered_id = ImGui::GetCurrentContext()->HoveredId,
      .expected_id = ImGui::GetID(expected_icon),
      .chrome_rect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()),
  };
  ImGui::Dummy(ImVec2(0.0f, 0.0f));
  ImGui::End();
  ImGui::PopStyleVar();
  ImGui::EndFrame();
  return result;
}

TEST_F(RightDrawerManagerTest, HeaderUsesOverflowBeforeTabsTouchChrome) {
  for (const auto type : {RightDrawerManager::DrawerType::kProperties,
                          RightDrawerManager::DrawerType::kNotifications,
                          RightDrawerManager::DrawerType::kToolOutput}) {
    RightDrawerManager manager;
    manager.OpenDrawer(type);
    const char* title = type == RightDrawerManager::DrawerType::kToolOutput
                            ? "Project Graph Lookup Results"
                            : GetDrawerTypeName(type);
    SCOPED_TRACE(title);
    ProbeHeader(manager, title, 1000.0f, -100.0f, "");

    const float padding = gui::UIConfig::kPanelPaddingLarge;
    const float tab_size = gui::IconSize::Small().x;
    const float gap = gui::UIConfig::kHeaderButtonGap;
    const float chrome_size = gui::IconSize::Toolbar().x;
    const float title_right =
        padding + ImGui::CalcTextSize(GetDrawerTypeIcon(type)).x +
        ImGui::GetStyle().ItemSpacing.x + ImGui::CalcTextSize(title).x;
    const float first_tab_x = title_right + gui::UIConfig::kHeaderButtonSpacing;
    const float tabs_right = first_tab_x +
                             GetDrawerCatalog().size() * tab_size +
                             (GetDrawerCatalog().size() - 1) * gap;
    const float chrome_width =
        padding + chrome_size +
        (type == RightDrawerManager::DrawerType::kProperties
             ? chrome_size + 4.0f
             : 0.0f);
    const float threshold = tabs_right + gap + chrome_width;

    // Warm the actual ImGui window before probing its hit rectangles.
    ProbeHeader(manager, title, threshold - 1.0f, -100.0f, "");
    auto hit = ProbeHeader(manager, title, threshold - 1.0f,
                           first_tab_x + tab_size * 0.5f, ICON_MD_SWAP_HORIZ);
    EXPECT_EQ(hit.hovered_id, hit.expected_id);
    EXPECT_GE(hit.chrome_rect.Min.x, first_tab_x + tab_size + gap);

    ProbeHeader(manager, title, threshold + 1.0f, -100.0f, "");
    size_t index = 0;
    for (const DrawerCatalogEntry& entry : GetDrawerCatalog()) {
      const float tab_left = first_tab_x + index * (tab_size + gap);
      // Both edges must be inside this tab's real mouse hit rectangle.
      for (const float x : {tab_left + 1.0f, tab_left + tab_size - 1.0f}) {
        hit = ProbeHeader(manager, title, threshold + 1.0f, x, entry.icon);
        EXPECT_EQ(hit.hovered_id, hit.expected_id) << entry.name;
      }
      EXPECT_LE(tab_left + tab_size + gap, hit.chrome_rect.Min.x);
      ++index;
    }

    const char* chrome_icon =
        type == RightDrawerManager::DrawerType::kProperties ? ICON_MD_LOCK_OPEN
                                                            : ICON_MD_CANCEL;
    hit = ProbeHeader(manager, title, threshold + 1.0f,
                      hit.chrome_rect.GetCenter().x, chrome_icon);
    EXPECT_EQ(hit.hovered_id, hit.expected_id);
    hit = ProbeHeader(manager, title, threshold + 1.0f,
                      padding + ImGui::CalcTextSize(GetDrawerTypeIcon(type)).x +
                          ImGui::GetStyle().ItemSpacing.x + 2.0f,
                      "");
    EXPECT_EQ(hit.hovered_id, 0u) << "Tabs must not cover the title";
  }
}

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

}  // namespace
}  // namespace yaze::editor
