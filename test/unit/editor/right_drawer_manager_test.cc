#include "app/editor/menu/right_drawer_manager.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

#include "app/editor/editor_manager.h"
#include "app/editor/menu/menu_builder.h"
#include "app/editor/shell/windows/settings_panel.h"
#include "app/editor/system/session/user_settings.h"
#include "app/editor/system/workspace/proposal_drawer.h"
#include "rom/rom.h"
#ifdef Z3ED_AI
#include "cli/service/rom/rom_sandbox_manager.h"
#endif
#include "app/editor/ui/toast_manager.h"
#include "app/gfx/backend/null_renderer.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/core/ui_config.h"
#include "app/gui/widgets/themed_widgets.h"
#include "editor_test_support.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze::editor {

class RightDrawerManagerTestPeer {
 public:
  static void DrawProposals(RightDrawerManager& manager) {
    manager.DrawProposalsPanel();
  }

  // Draw just the header (used for layout smoke checks).
  static void DrawHeader(RightDrawerManager& manager, const char* title) {
    const auto type = manager.GetActiveDrawer();
    manager.DrawPanelHeader(type, title, GetDrawerTypeIcon(type));
  }

  static void DrawNavStrip(RightDrawerManager& manager) {
    manager.DrawDrawerNavStrip(manager.GetActiveDrawer());
  }

  static float ConfiguredWidth(RightDrawerManager& manager,
                               RightDrawerManager::DrawerType type) {
    return manager.GetConfiguredPanelWidth(type);
  }
};

class ProposalDrawerTestPeer {
 public:
  static absl::Status Accept(ProposalDrawer& drawer, const std::string& id) {
    return drawer.AcceptProposal(id);
  }
  static void Refresh(ProposalDrawer& drawer) { drawer.RefreshProposals(); }
  static void Select(ProposalDrawer& drawer, const std::string& id) {
    drawer.SelectProposal(id);
  }
  static bool SelectionIsCurrent(const ProposalDrawer& drawer,
                                 const std::string& id) {
    for (const auto& proposal : drawer.proposals_) {
      if (&proposal == drawer.selected_proposal_)
        return proposal.id == id;
    }
    return false;
  }
  static bool HasSelection(const ProposalDrawer& drawer) {
    return drawer.selected_proposal_ != nullptr;
  }
  static void Request(ProposalDrawer& drawer, const std::string& action,
                      const std::string& id) {
    drawer.confirm_action_ = action;
    drawer.confirm_proposal_id_ = id;
    drawer.show_confirm_dialog_ = true;
  }
  static bool HasPendingAction(const ProposalDrawer& drawer) {
    return drawer.show_confirm_dialog_ || !drawer.confirm_action_.empty() ||
           !drawer.confirm_proposal_id_.empty();
  }
  static const absl::Status& ActionStatus(const ProposalDrawer& drawer) {
    return drawer.action_status_;
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

TEST_F(RightDrawerManagerTest, MainMenusAndCanvasPopupWorkWithSettingsOpen) {
  RightDrawerManager manager;
  UserSettings settings;
  SettingsPanel panel;
  panel.SetUserSettings(&settings);
  manager.SetSettingsPanel(&panel);
  manager.OpenDrawer(RightDrawerManager::DrawerType::kSettings);
  int activated = 0;
  ImRect trigger, action;
  int menu_index = 0;
  bool canvas = false;
  const char* names[] = {"File", "Edit", "View", "Windows", "Tools", "Help"};
  const auto frame = [&]() {
    ImGui::NewFrame();
    if (ImGui::BeginMainMenuBar()) {
      for (int i = 0; i < 6; ++i) {
        const bool opened = ImGui::BeginMenu(names[i]);
        if (!canvas && i == menu_index)
          trigger = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        if (opened) {
          if (ImGui::MenuItem("Test action"))
            ++activated;
          action = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
          ImGui::EndMenu();
        }
      }
      ImGui::EndMainMenuBar();
    }
    ImGui::SetNextWindowPos(ImVec2(40, 80));
    ImGui::SetNextWindowSize(ImVec2(600, 500));
    ImGui::Begin("Canvas popup host");
    if (ImGui::Button("Canvas options"))
      ImGui::OpenPopup("Options");
    if (canvas)
      trigger = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    if (ImGui::BeginPopup("Options")) {
      if (ImGui::MenuItem("Test action"))
        ++activated;
      action = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
      ImGui::EndPopup();
    }
    ImGui::End();
    manager.Draw();
    ImGui::Render();
  };
  const auto click = [&](ImRect rect) {
    const auto center = rect.GetCenter();
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent(center.x, center.y);
    frame();
    io.AddMouseButtonEvent(0, true);
    frame();
    io.AddMouseButtonEvent(0, false);
    frame();
    frame();
  };
  for (int i = 0; i < 30; ++i)
    frame();
  for (menu_index = 0; menu_index < 6; ++menu_index) {
    frame();
    click(trigger);
    ASSERT_FALSE(GImGui->OpenPopupStack.empty()) << names[menu_index];
    click(action);
    EXPECT_EQ(activated, menu_index + 1);
  }
  canvas = true;
  frame();
  click(trigger);
  ASSERT_FALSE(GImGui->OpenPopupStack.empty());
  click(action);
  EXPECT_EQ(activated, 7);
  EXPECT_EQ(manager.GetActiveDrawer(),
            RightDrawerManager::DrawerType::kSettings);
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

TEST_F(RightDrawerManagerTest, SingleToggleRestoresLastSelectedTab) {
  RightDrawerManager manager;
  EXPECT_EQ(manager.GetLastActiveDrawer(),
            RightDrawerManager::DrawerType::kProject);

  manager.ToggleLastDrawer();
  EXPECT_EQ(manager.GetActiveDrawer(),
            RightDrawerManager::DrawerType::kProject);
  manager.OpenDrawer(RightDrawerManager::DrawerType::kHelp);
  manager.ToggleLastDrawer();
  EXPECT_EQ(manager.GetActiveDrawer(), RightDrawerManager::DrawerType::kNone);
  EXPECT_EQ(manager.GetLastActiveDrawer(),
            RightDrawerManager::DrawerType::kHelp);

  manager.ToggleLastDrawer();
  EXPECT_EQ(manager.GetActiveDrawer(), RightDrawerManager::DrawerType::kHelp);
}

TEST_F(RightDrawerManagerTest, AllDrawerTabsShareOnePersistedWidth) {
  RightDrawerManager manager;
  manager.SetDrawerWidth(RightDrawerManager::DrawerType::kProject, 512.0f);

  for (const DrawerCatalogEntry& entry : GetDrawerCatalog()) {
    EXPECT_FLOAT_EQ(
        RightDrawerManagerTestPeer::ConfiguredWidth(manager, entry.type),
        512.0f);
  }

  const auto serialized = manager.SerializeDrawerWidths();
  ASSERT_EQ(serialized.size(), 1U);
  EXPECT_FLOAT_EQ(serialized.at("right_sidebar.shared"), 512.0f);
}

TEST_F(RightDrawerManagerTest, SharedWidthUsesStrictestDrawerMinimum) {
  RightDrawerManager manager;
  manager.SetDrawerWidth(RightDrawerManager::DrawerType::kSettings, 200.0f);

  for (const DrawerCatalogEntry& entry : GetDrawerCatalog()) {
    EXPECT_FLOAT_EQ(
        RightDrawerManagerTestPeer::ConfiguredWidth(manager, entry.type),
        360.0f);
  }
}

TEST_F(RightDrawerManagerTest, LegacyDrawerWidthsMigrateToWidestValue) {
  RightDrawerManager manager;
  manager.RestoreDrawerWidths({{"project", 360.0f},
                               {"properties", 520.0f},
                               {"settings", 390.0f},
                               {"dungeon.workbench", 700.0f}});

  EXPECT_FLOAT_EQ(RightDrawerManagerTestPeer::ConfiguredWidth(
                      manager, RightDrawerManager::DrawerType::kProject),
                  520.0f);
  EXPECT_FLOAT_EQ(RightDrawerManagerTestPeer::ConfiguredWidth(
                      manager, RightDrawerManager::DrawerType::kSettings),
                  520.0f);
  EXPECT_FLOAT_EQ(manager.SerializeDrawerWidths().at("right_sidebar.shared"),
                  520.0f);
}

TEST_F(RightDrawerManagerTest, ResetSharedWidthUsesWidestDrawerDefault) {
  RightDrawerManager manager;
  manager.SetActiveEditor(EditorType::kDungeon);
  manager.SetDrawerWidth(RightDrawerManager::DrawerType::kProject, 700.0f);
  manager.ResetDrawerWidths();

  EXPECT_FLOAT_EQ(RightDrawerManagerTestPeer::ConfiguredWidth(
                      manager, RightDrawerManager::DrawerType::kSettings),
                  480.0f);
}

// Nav strip behavioral tests: active-stable / inactive-switch

TEST_F(RightDrawerManagerTest, NavStripActiveTabKeepsDrawerOpen) {
  RightDrawerManager manager;
  manager.OpenDrawer(RightDrawerManager::DrawerType::kHelp);
  ASSERT_EQ(manager.GetActiveDrawer(), RightDrawerManager::DrawerType::kHelp);

  ClickNavTab(manager, 5);  // Help
  EXPECT_EQ(manager.GetActiveDrawer(), RightDrawerManager::DrawerType::kHelp);
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
            type == RightDrawerManager::DrawerType::kNotifications ? 3 : 2;
        const float action_size = gui::ScaledSize(24.0f, 0.0f).x;
        const float action_gap = gui::ScaledSize(4.0f, 0.0f).x;
        const float header_padding =
            gui::ScaledSize(gui::UIConfig::kPanelPaddingLarge, 0.0f).x;
        const float first_action_x =
            ImGui::GetWindowPos().x + width - header_padding -
            action_count * action_size - (action_count - 1) * action_gap;
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

TEST_F(RightDrawerManagerTest, CompactNavTabsFitEverySupportedDrawerWidth) {
  const auto catalog = GetDrawerCatalog();
  const size_t count = catalog.size();
  ASSERT_GT(count, 0u);

  const float tab_height =
      std::max(24.0f, gui::LayoutHelpers::GetStandardWidgetHeight());
  const float padding =
      std::clamp(gui::LayoutHelpers::GetStandardSpacing(), 4.0f, 8.0f);
  const float gap = std::max(2.0f, padding * 0.5f);

  for (const float window_w : {280.0f, 320.0f, 480.0f, 1024.0f, 1920.0f}) {
    const float avail_w = window_w - padding * 2.0f;
    const float tab_w =
        std::max(tab_height, std::floor((avail_w - (count - 1) * gap) / count));
    const float total_w = padding * 2.0f + tab_w * count + gap * (count - 1);

    EXPECT_GE(tab_w, tab_height) << "tab too narrow at window_w=" << window_w;
    EXPECT_LE(total_w, window_w + 0.5f)
        << "compact tabs overflow at window_w=" << window_w;
  }
}

#ifdef Z3ED_AI
class ProposalDrawerWorkflowTest : public RightDrawerManagerTest {
 protected:
  void SetUp() override {
    RightDrawerManagerTest::SetUp();
    const auto serial =
        std::chrono::steady_clock::now().time_since_epoch().count();
    root_ = std::filesystem::temp_directory_path() /
            ("yaze-proposal-drawer-" + std::to_string(serial));
    auto& registry = cli::ProposalRegistry::Instance();
    previous_proposal_root_ = registry.RootDirectory();
    registry.SetRootDirectory(root_ / "proposals");
    auto& sandboxes = cli::RomSandboxManager::Instance();
    previous_sandbox_root_ = sandboxes.RootDirectory();
    sandboxes.SetRootDirectory(root_ / "sandboxes");
    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
    rom_.set_filename((root_ / "source.sfc").string());
    drawer_.SetRom(&rom_);
    manager_.SetRom(&rom_);
    manager_.SetProposalDrawer(&drawer_);
  }

  void TearDown() override {
    auto& registry = cli::ProposalRegistry::Instance();
    for (const auto& id : proposal_ids_)
      (void)registry.RemoveProposal(id);
    registry.SetRootDirectory(previous_proposal_root_);
    auto& sandboxes = cli::RomSandboxManager::Instance();
    for (const auto& id : sandbox_ids_)
      (void)sandboxes.RemoveSandbox(id);
    sandboxes.SetRootDirectory(previous_sandbox_root_);
    std::error_code ec;
    std::filesystem::remove_all(root_, ec);
    RightDrawerManagerTest::TearDown();
  }

  std::string CreateProposal(Rom* source_rom = nullptr) {
    auto sandbox = cli::RomSandboxManager::Instance().CreateSandbox(
        source_rom ? *source_rom : rom_, "test");
    EXPECT_TRUE(sandbox.ok()) << sandbox.status();
    if (!sandbox.ok())
      return {};
    sandbox_ids_.push_back(sandbox->id);
    auto proposal = cli::ProposalRegistry::Instance().CreateProposal(
        sandbox->id, sandbox->rom_path, "test proposal", "test changes");
    EXPECT_TRUE(proposal.ok()) << proposal.status();
    if (!proposal.ok())
      return {};
    proposal_ids_.push_back(proposal->id);
    return proposal->id;
  }

  void Frame(RightDrawerManager& manager) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20));
    ImGui::SetNextWindowSize(ImVec2(560, 640));
    ImGui::Begin("Proposal drawer host");
    RightDrawerManagerTestPeer::DrawProposals(manager);
    ImGui::End();
    ImGui::Render();
  }

  void Frame() { Frame(manager_); }

  void Confirm() {
    Frame();
    Frame();  // Let the modal settle its automatic size and position.
    auto* popup = ImGui::FindWindowByName("Confirm Proposal Action");
    ASSERT_NE(popup, nullptr);
    ASSERT_TRUE(ImGui::IsPopupOpen("Confirm Proposal Action",
                                   ImGuiPopupFlags_AnyPopupId));
    const float button_height =
        ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2;
    const ImVec2 yes(popup->DC.CursorStartPos.x + 60,
                     popup->DC.CursorMaxPos.y - button_height / 2);
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent(yes.x, yes.y);
    Frame();
    io.AddMouseButtonEvent(0, true);
    Frame();
    io.AddMouseButtonEvent(0, false);
    Frame();
    Frame();
  }

  Rom rom_;
  ProposalDrawer drawer_;
  RightDrawerManager manager_;
  std::filesystem::path root_, previous_proposal_root_, previous_sandbox_root_;
  std::vector<std::string> proposal_ids_, sandbox_ids_;
};

TEST_F(ProposalDrawerWorkflowTest, ApplyRefusesBeforeRomOrReviewStateMutation) {
  const auto id = CreateProposal();
  ASSERT_FALSE(id.empty());
  const auto sandbox = cli::RomSandboxManager::Instance().ActiveSandbox();
  ASSERT_TRUE(sandbox.ok());
  Rom changed;
  ASSERT_TRUE(changed.LoadFromFile(sandbox->rom_path.string()).ok());
  ASSERT_TRUE(changed.WriteByte(0x100, 0xA5).ok());
  ASSERT_TRUE(changed.SaveToFile({.save_new = false}).ok());
  const auto original = rom_.vector();
  const bool was_dirty = rom_.dirty();

  const auto status = ProposalDrawerTestPeer::Accept(drawer_, id);
  EXPECT_EQ(status.code(), absl::StatusCode::kUnimplemented);
  EXPECT_EQ(rom_.vector(), original);
  EXPECT_EQ(rom_.dirty(), was_dirty);
  const auto proposal = cli::ProposalRegistry::Instance().GetProposal(id);
  ASSERT_TRUE(proposal.ok());
  EXPECT_EQ(proposal->status, cli::ProposalRegistry::ProposalStatus::kPending);
}

TEST_F(ProposalDrawerWorkflowTest, EmbeddedRejectAndDeleteRequireConfirmation) {
  for (const std::string action : {"reject", "delete"}) {
    const auto id = CreateProposal();
    ASSERT_FALSE(id.empty());
    ProposalDrawerTestPeer::Refresh(drawer_);
    ProposalDrawerTestPeer::Select(drawer_, id);
    ProposalDrawerTestPeer::Request(drawer_, action, id);
    const auto pending = cli::ProposalRegistry::Instance().GetProposal(id);
    ASSERT_TRUE(pending.ok());
    EXPECT_EQ(pending->status, cli::ProposalRegistry::ProposalStatus::kPending);
    Confirm();  // Runs RightDrawerManager's embedded DrawContent path.
    EXPECT_TRUE(ProposalDrawerTestPeer::ActionStatus(drawer_).ok());
    EXPECT_FALSE(ProposalDrawerTestPeer::HasPendingAction(drawer_));
    const auto after = cli::ProposalRegistry::Instance().GetProposal(id);
    if (action == "reject") {
      ASSERT_TRUE(after.ok());
      EXPECT_EQ(after->status,
                cli::ProposalRegistry::ProposalStatus::kRejected);
    } else {
      EXPECT_EQ(after.status().code(), absl::StatusCode::kNotFound);
    }
  }
}

TEST_F(ProposalDrawerWorkflowTest,
       RefreshResolvesTheRetainedIdAndClearsRemoval) {
  const auto selected = CreateProposal();
  ASSERT_FALSE(selected.empty());
  ProposalDrawerTestPeer::Refresh(drawer_);
  ProposalDrawerTestPeer::Select(drawer_, selected);
  for (int i = 0; i < 5; ++i) {
    ASSERT_FALSE(CreateProposal().empty());
    ProposalDrawerTestPeer::Refresh(drawer_);
    EXPECT_TRUE(ProposalDrawerTestPeer::SelectionIsCurrent(drawer_, selected));
    Frame();
  }
  ASSERT_TRUE(cli::ProposalRegistry::Instance().RemoveProposal(selected).ok());
  ProposalDrawerTestPeer::Refresh(drawer_);
  EXPECT_FALSE(ProposalDrawerTestPeer::HasSelection(drawer_));
  Frame();
}

TEST_F(ProposalDrawerWorkflowTest, SwitchingRomCancelsAnOpenConfirmation) {
  const auto id = CreateProposal();
  ASSERT_FALSE(id.empty());
  ProposalDrawerTestPeer::Refresh(drawer_);
  ProposalDrawerTestPeer::Select(drawer_, id);
  ProposalDrawerTestPeer::Request(drawer_, "reject", id);
  Frame();
  Frame();
  ASSERT_TRUE(ProposalDrawerTestPeer::HasPendingAction(drawer_));
  ASSERT_TRUE(ProposalDrawerTestPeer::SelectionIsCurrent(drawer_, id));
  auto* popup = ImGui::FindWindowByName("Confirm Proposal Action");
  ASSERT_NE(popup, nullptr);
  const ImGuiID popup_id = popup->PopupId;
  ASSERT_TRUE(ImGui::IsPopupOpen(popup_id, ImGuiPopupFlags_AnyPopupLevel));
  Rom other;
  drawer_.SetRom(&other);
  manager_.SetRom(&other);
  Frame();
  EXPECT_FALSE(ImGui::IsPopupOpen(popup_id, ImGuiPopupFlags_AnyPopupLevel));
  Frame();
  EXPECT_FALSE(popup->Active);
  EXPECT_FALSE(ProposalDrawerTestPeer::HasPendingAction(drawer_));
  EXPECT_FALSE(ProposalDrawerTestPeer::HasSelection(drawer_));
  const auto proposal = cli::ProposalRegistry::Instance().GetProposal(id);
  ASSERT_TRUE(proposal.ok());
  EXPECT_EQ(proposal->status, cli::ProposalRegistry::ProposalStatus::kPending);
}

TEST_F(ProposalDrawerWorkflowTest,
       SwappingProjectRomInPlaceClosesConfirmationWithoutReviewAction) {
  const auto original_path = root_ / "original.sfc";
  const auto replacement_path = root_ / "replacement.sfc";
  const auto project_path = root_ / "minimal.yaze";
  auto write_rom = [](const std::filesystem::path& path,
                      const std::string& title) {
    std::vector<uint8_t> bytes(512 * 1024, 0);
    for (size_t i = 0; i < title.size(); ++i) {
      bytes[0x7FC0 + i] = static_cast<uint8_t>(title[i]);
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(output.is_open());
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    ASSERT_TRUE(output.good());
  };
  write_rom(original_path, "BEFORE PROPOSAL SWAP");
  write_rom(replacement_path, "AFTER PROPOSAL SWAP");
  {
    std::ofstream output(project_path);
    ASSERT_TRUE(output.is_open());
    output << "[project]\nname=Proposal replacement fixture\n\n"
              "[files]\nrom_filename="
           << original_path.string()
           << "\nrom_backup_folder=backups\ncode_folder=\nassets_folder=\n"
              "patches_folder=\nlabels_filename=\nsymbols_filename=\n"
              "output_folder=build\n\n[workspace]\nautosave_enabled=false\n"
              "backup_on_save=false\n";
    ASSERT_TRUE(output.good());
  }

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto editor = std::make_unique<EditorManager>();
  test::InitializeWithIsolatedSettings(*editor, renderer.get());
  editor->SetAssetLoadMode(AssetLoadMode::kLazy);
  const auto opened = editor->OpenRomOrProject(project_path.string());
  ASSERT_TRUE(opened.ok()) << opened;
  Rom* const rom_address = editor->GetCurrentRom();
  ASSERT_NE(rom_address, nullptr);
  const auto original_bytes = rom_address->vector();
  auto* right_drawer = editor->right_drawer_manager();
  ASSERT_NE(right_drawer, nullptr);
  auto* proposal_drawer = right_drawer->proposal_drawer();
  ASSERT_NE(proposal_drawer, nullptr);
  right_drawer->SetRom(rom_address);
  proposal_drawer->SetRom(rom_address);

  const auto id = CreateProposal(rom_address);
  ASSERT_FALSE(id.empty());
  ProposalDrawerTestPeer::Refresh(*proposal_drawer);
  ProposalDrawerTestPeer::Select(*proposal_drawer, id);
  ProposalDrawerTestPeer::Request(*proposal_drawer, "reject", id);
  Frame(*right_drawer);
  Frame(*right_drawer);
  ASSERT_TRUE(ProposalDrawerTestPeer::HasPendingAction(*proposal_drawer));
  ASSERT_TRUE(ProposalDrawerTestPeer::SelectionIsCurrent(*proposal_drawer, id));
  auto* popup = ImGui::FindWindowByName("Confirm Proposal Action");
  ASSERT_NE(popup, nullptr);
  const ImGuiID popup_id = popup->PopupId;
  ASSERT_TRUE(ImGui::IsPopupOpen(popup_id, ImGuiPopupFlags_AnyPopupLevel));

  const auto swapped = editor->SwapProjectRom(replacement_path.string());
  ASSERT_TRUE(swapped.ok()) << swapped;
  ASSERT_EQ(editor->GetCurrentRom(), rom_address);
  EXPECT_NE(rom_address->vector(), original_bytes);
  const auto replacement_bytes = rom_address->vector();
  const bool replacement_dirty = rom_address->dirty();
  Frame(*right_drawer);
  EXPECT_FALSE(ImGui::IsPopupOpen(popup_id, ImGuiPopupFlags_AnyPopupLevel));
  Frame(*right_drawer);
  EXPECT_FALSE(popup->Active);
  EXPECT_FALSE(ProposalDrawerTestPeer::HasPendingAction(*proposal_drawer));
  EXPECT_FALSE(ProposalDrawerTestPeer::HasSelection(*proposal_drawer));
  const auto proposal = cli::ProposalRegistry::Instance().GetProposal(id);
  ASSERT_TRUE(proposal.ok());
  EXPECT_EQ(proposal->status, cli::ProposalRegistry::ProposalStatus::kPending);
  EXPECT_EQ(rom_address->vector(), replacement_bytes);
  EXPECT_EQ(rom_address->dirty(), replacement_dirty);
}
#endif  // Z3ED_AI

}  // namespace
}  // namespace yaze::editor
