#include "app/editor/shell/coordinator/welcome_screen.h"

#include <functional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/theme_manager.h"
#include "core/project.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze::editor {

class WelcomeScreenTestPeer {
 public:
  static void SetEntryTime(WelcomeScreen* screen, float entry_time) {
    screen->entry_time_ = entry_time;
    screen->entry_animations_started_ = true;
  }

  static void DrawProjectPanel(WelcomeScreen* screen,
                               const RecentProject& project, int index,
                               const ImVec2& card_size) {
    screen->DrawProjectPanel(project, index, card_size);
  }

  static void DrawQuickActions(WelcomeScreen* screen) {
    screen->DrawQuickActions();
  }

  static bool ShouldUseStackedLayout(float content_width, float content_height,
                                     float layout_scale) {
    return WelcomeScreen::ShouldUseStackedLayout(content_width, content_height,
                                                 layout_scale);
  }

  static int CalculateVisibleRecentCount(int entry_count,
                                         float available_height,
                                         float row_height, float row_gap,
                                         float more_line_height) {
    return WelcomeScreen::CalculateVisibleRecentCount(
        entry_count, available_height, row_height, row_gap, more_line_height);
  }

  static const RecentProject* FindResumeProject(
      const std::vector<RecentProject>& entries) {
    return WelcomeScreen::FindResumeProject(entries);
  }

  static ImGuiID ProjectPanelId(int index) {
    ImGui::PushID(index);
    const ImGuiID id = ImGui::GetID("##ProjectPanel");
    ImGui::PopID();
    return id;
  }
};

namespace {

class WelcomeScreenTest : public ::testing::Test {
 protected:
  void SetUp() override {
    IMGUI_CHECKVERSION();
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigNavCursorVisibleAlways = true;
    io.DisplaySize = ImVec2(800.0f, 600.0f);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefault();

    unsigned char* pixels = nullptr;
    int atlas_width = 0;
    int atlas_height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &atlas_width, &atlas_height);

    auto& recents = project::RecentFilesManager::GetInstance();
    saved_recents_ = recents.GetRecentFiles();
    recents.Clear();
  }

  void TearDown() override {
    auto& recents = project::RecentFilesManager::GetInstance();
    recents.Clear();
    for (auto it = saved_recents_.rbegin(); it != saved_recents_.rend(); ++it) {
      recents.AddFile(*it);
    }
    ImGui::DestroyContext(context_);
    context_ = nullptr;
  }

  void DrawCardFrame(WelcomeScreen* screen, const RecentProject& project,
                     bool request_keyboard_focus,
                     const std::function<void(ImGuiIO&)>& inject_events = {}) {
    ImGuiIO& io = ImGui::GetIO();
    if (inject_events) {
      inject_events(io);
    }

    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(420.0f, 220.0f), ImGuiCond_Always);
    ImGui::SetNextWindowFocus();
    ImGui::Begin(
        "WelcomeCardHost", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    if (request_keyboard_focus) {
      ImGui::SetKeyboardFocusHere();
    }
    expected_card_id_ = WelcomeScreenTestPeer::ProjectPanelId(0);
    WelcomeScreenTestPeer::DrawProjectPanel(screen, project, 0,
                                            ImVec2(360.0f, 130.0f));
    ImGui::End();
    ImGui::EndFrame();
  }

  ImGuiContext* context_ = nullptr;
  ImGuiID expected_card_id_ = 0;
  std::vector<std::string> saved_recents_;
};

// Measures the real start-button geometry at each Display Density preset.
// Reads the nav rect rather than a recomputed formula, so it fails if the
// production sizing stops responding to density for any reason.
class WelcomeScreenDensityTest : public WelcomeScreenTest {
 protected:
  void SetUp() override {
    WelcomeScreenTest::SetUp();
    auto& themes = gui::ThemeManager::Get();
    saved_theme_ = themes.GetCurrentThemeName();
    ImGui::GetIO().DisplaySize = ImVec2(1400.0f, 1050.0f);
  }

  void TearDown() override {
    gui::ThemeManager::Get().ApplyTheme(saved_theme_);
    WelcomeScreenTest::TearDown();
  }

  void DrawFrame(WelcomeScreen* screen, ImGuiWindow* focus = nullptr) {
    ImGui::NewFrame();
    if (focus) {
      ImGui::FocusWindow(focus);
      ImGui::NavInitWindow(focus, true);
    }
    bool open = true;
    screen->Show(&open);
    ImGui::EndFrame();
  }

  ImGuiWindow* FindChild(const char* name) {
    for (ImGuiWindow* window : context_->Windows) {
      if (window->Active &&
          std::string(window->Name).find(name) != std::string::npos) {
        return window;
      }
    }
    return nullptr;
  }

  // Height of the "Open ROM / Project" button under `preset`.
  float MeasurePrimaryButtonHeight(gui::DensityPreset preset) {
    auto& themes = gui::ThemeManager::Get();
    themes.ApplyClassicYazeTheme();
    gui::Theme theme = themes.GetCurrentTheme();
    theme.ApplyDensityPreset(preset);
    // ApplyTheme, not ReapplyTheme: that helper lands with the Classic YAZE
    // theme-compat work on a separate branch. Either routes density sizing
    // into ImGui, which is what this measurement needs.
    themes.ApplyTheme(theme);
    EXPECT_EQ(themes.GetCurrentTheme().density_preset, preset);

    WelcomeScreen screen;
    screen.RefreshRecentProjects();
    WelcomeScreenTestPeer::SetEntryTime(&screen, 1.0f);
    screen.SetOpenRomCallback([]() {});
    screen.SetNewProjectCallback([]() {});
    for (int frame = 0; frame < 3; ++frame) {
      DrawFrame(&screen);
    }
    ImGuiWindow* rail = FindChild("/LeftPanel_");
    EXPECT_NE(rail, nullptr) << "expected the split layout at 1400x1050";
    if (rail == nullptr) {
      return 0.0f;
    }
    DrawFrame(&screen, rail);
    DrawFrame(&screen);
    const ImRect rect =
        ImGui::WindowRectRelToAbs(rail, rail->NavRectRel[ImGuiNavLayer_Main]);
    return rect.GetHeight();
  }

  std::string saved_theme_;
};

struct WelcomeLayoutCase {
  const char* name;
  ImVec2 viewport;
  float font_size;
  bool populated;
  bool stacked;
  const char* theme = "Classic YAZE";
};

class WelcomeScreenFullLayoutTest
    : public WelcomeScreenTest,
      public ::testing::WithParamInterface<WelcomeLayoutCase> {
 protected:
  void SetUp() override {
    WelcomeScreenTest::SetUp();
    auto& themes = gui::ThemeManager::Get();
    saved_theme_ = themes.GetCurrentThemeName();
    themes.ApplyTheme(GetParam().theme);
    ASSERT_EQ(themes.GetCurrentThemeName(), GetParam().theme);
  }

  void TearDown() override {
    gui::ThemeManager::Get().ApplyTheme(saved_theme_);
    WelcomeScreenTest::TearDown();
  }

  void DrawFrame(WelcomeScreen* screen, ImGuiWindow* focus = nullptr) {
    ImGui::NewFrame();
    if (focus) {
      ImGui::FocusWindow(focus);
      ImGui::NavInitWindow(focus, true);
    }
    bool open = true;
    screen->Show(&open);
    EXPECT_TRUE(open);
    ImGui::EndFrame();
  }

  ImGuiWindow* FindChild(const char* name) {
    for (ImGuiWindow* window : context_->Windows) {
      if (window->Active &&
          std::string(window->Name).find(name) != std::string::npos) {
        return window;
      }
    }
    return nullptr;
  }

  std::string saved_theme_;
};

TEST_P(WelcomeScreenFullLayoutTest, StartActionsStayReachableWithoutScrolling) {
  const auto& layout = GetParam();
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = layout.viewport;
  io.FontGlobalScale = layout.font_size / 13.0f;
  if (layout.populated) {
    for (int i = 0; i < 6; ++i) {
      // Missing-file entries exercise the real model without ROM fixtures or
      // background metadata scans. Do not save the process-local recent list.
      project::RecentFilesManager::GetInstance().AddFile(
          "/missing/welcome-layout-" + std::to_string(i) + ".sfc");
    }
  }

  WelcomeScreen screen;
  screen.RefreshRecentProjects();
  ASSERT_EQ(screen.recent_projects().entries().size(),
            layout.populated ? 6u : 0u);
  WelcomeScreenTestPeer::SetEntryTime(&screen, 1.0f);
  int open_count = 0;
  int new_count = 0;
  screen.SetOpenRomCallback([&]() { ++open_count; });
  screen.SetNewProjectCallback([&]() { ++new_count; });
  screen.SetOpenPrototypeResearchCallback([]() {});
  screen.SetOpenAssemblyEditorNoRomCallback([]() {});

  // ImGui computes scroll ranges from the preceding frame's content size.
  for (int frame = 0; frame < 3; ++frame) {
    DrawFrame(&screen);
  }
  ImGuiWindow* root = ImGui::FindWindowByName("##WelcomeScreen");
  ImGuiWindow* content = FindChild("/WelcomeContent_");
  ASSERT_NE(root, nullptr);
  ASSERT_NE(content, nullptr);
  EXPECT_GE(root->Pos.x, 0.0f);
  EXPECT_GE(root->Pos.y, 0.0f);
  EXPECT_LE(root->Pos.x + root->Size.x, io.DisplaySize.x);
  EXPECT_LE(root->Pos.y + root->Size.y, io.DisplaySize.y);
  EXPECT_FALSE(root->ScrollbarY);
  EXPECT_FALSE(content->ScrollbarY);
  EXPECT_FLOAT_EQ(content->ScrollMax.y, 0.0f);
  EXPECT_LE(root->ContentSize.x, root->InnerRect.GetWidth());
  const ImRect root_bounds = root->Rect();
  const ImRect content_bounds = content->Rect();
  ImGuiWindow* actions = FindChild("/LeftPanel_");
  EXPECT_EQ(actions == nullptr, layout.stacked);
  if (!layout.stacked) {
    ASSERT_NE(actions, nullptr);
    EXPECT_FALSE(actions->ScrollbarY);
    EXPECT_LE(actions->ContentSize.y, actions->InnerRect.GetHeight() + 1.0f);
    EXPECT_FLOAT_EQ(actions->ScrollMax.y, 0.0f);
    ImGuiWindow* right = FindChild("/RightPanel_");
    ASSERT_NE(right, nullptr);
    EXPECT_FALSE(right->ScrollbarY);
    EXPECT_FLOAT_EQ(right->ScrollMax.y, 0.0f);
  } else {
    actions = content;
  }

  // Walk real keyboard navigation rather than activating IDs directly. Each
  // primary action must be visible after focus.
  DrawFrame(&screen, actions);
  DrawFrame(&screen);
#ifdef __EMSCRIPTEN__
  const ImGuiID open_id = actions->GetID(ICON_MD_FOLDER_OPEN " Open ROM");
#else
  const ImGuiID open_id =
      actions->GetID(ICON_MD_FOLDER_OPEN " Open ROM / Project");
#endif
  const ImGuiID new_id = actions->GetID(ICON_MD_ADD_CIRCLE " New Project");
  bool saw_open = false;
  bool saw_new = false;
  for (int step = 0; step < 12 && !(saw_open && saw_new); ++step) {
    if (context_->NavId == open_id || context_->NavId == new_id) {
      const ImRect rect = ImGui::WindowRectRelToAbs(
          actions, actions->NavRectRel[ImGuiNavLayer_Main]);
      EXPECT_GE(rect.Min.y, actions->ClipRect.Min.y);
      EXPECT_LE(rect.Max.y, actions->ClipRect.Max.y);
      EXPECT_GE(rect.Min.x, actions->ClipRect.Min.x);
      EXPECT_LE(rect.Max.x, actions->ClipRect.Max.x);
      saw_open |= context_->NavId == open_id;
      saw_new |= context_->NavId == new_id;
      io.AddKeyEvent(ImGuiKey_Enter, true);
      DrawFrame(&screen);
      io.AddKeyEvent(ImGuiKey_Enter, false);
      DrawFrame(&screen);
    }
    io.AddKeyEvent(ImGuiKey_Tab, true);
    DrawFrame(&screen);
    io.AddKeyEvent(ImGuiKey_Tab, false);
    DrawFrame(&screen);
  }
  EXPECT_TRUE(saw_open);
  EXPECT_TRUE(saw_new);
  EXPECT_EQ(open_count, 1);
  EXPECT_EQ(new_count, 1);

  EXPECT_FALSE(root->ScrollbarY);
  EXPECT_LE(root->ContentSize.x, root->InnerRect.GetWidth());
  EXPECT_FLOAT_EQ(root->Pos.x, root_bounds.Min.x);
  EXPECT_FLOAT_EQ(root->Pos.y, root_bounds.Min.y);
  EXPECT_FLOAT_EQ(root->Size.x, root_bounds.GetWidth());
  EXPECT_FLOAT_EQ(root->Size.y, root_bounds.GetHeight());
  EXPECT_FLOAT_EQ(content->Pos.x, content_bounds.Min.x);
  EXPECT_FLOAT_EQ(content->Pos.y, content_bounds.Min.y);
  EXPECT_FLOAT_EQ(content->Size.x, content_bounds.GetWidth());
  EXPECT_FLOAT_EQ(content->Size.y, content_bounds.GetHeight());
}

INSTANTIATE_TEST_SUITE_P(
    ViewportAndRecents, WelcomeScreenFullLayoutTest,
    ::testing::Values(
        WelcomeLayoutCase{"WideFirstRun", ImVec2(1400, 1050), 13, false, false},
        WelcomeLayoutCase{"WideRecents", ImVec2(1400, 1050), 13, true, false},
        WelcomeLayoutCase{"NarrowFirstRun", ImVec2(800, 600), 13, false, true},
        WelcomeLayoutCase{"NarrowRecents", ImVec2(800, 600), 13, true, true},
        // Short but wide: split, not stacked. Split lays the action rail and
        // recents side by side and needs max(left, right) of height; stacked
        // runs them in sequence and needs their sum. Stacking a short card
        // asked for more of the axis that just ran out.
        WelcomeLayoutCase{"ShortFirstRun", ImVec2(1400, 500), 13, false, false},
        WelcomeLayoutCase{"ShortRecents", ImVec2(1400, 500), 13, true, false},
        // Severely short: the action rail cannot fit Resume and the two
        // no-ROM buttons. DrawQuickActions must shed them rather than
        // overflow a pane that has no scrollbar, which would silently clip
        // the entire Recent section drawn beside it.
        WelcomeLayoutCase{"TinyFirstRun", ImVec2(1400, 340), 13, false, false},
        WelcomeLayoutCase{"TinyRecents", ImVec2(1400, 340), 13, true, false},
        WelcomeLayoutCase{"LargeFontFirstRun", ImVec2(1400, 1050), 26, false,
                          true},
        WelcomeLayoutCase{"LargeFontRecents", ImVec2(1400, 1050), 26, true,
                          true},
        WelcomeLayoutCase{"ForestLargeFontFirstRun", ImVec2(1400, 1050), 26,
                          false, true, "Forest"},
        WelcomeLayoutCase{"ForestLargeFontRecents", ImVec2(1400, 1050), 26,
                          true, true, "Forest"},
        WelcomeLayoutCase{"ForestLightLargeFontFirstRun", ImVec2(1400, 1050),
                          26, false, true, "Forest Light"},
        WelcomeLayoutCase{"ForestLightLargeFontRecents", ImVec2(1400, 1050), 26,
                          true, true, "Forest Light"}),
    [](const ::testing::TestParamInfo<WelcomeLayoutCase>& info) {
      return info.param.name;
    });

// The other breakpoint tests only bound kWelcomeSplitMinWidth loosely — the
// constant could drift anywhere in (500, 900] undetected. Pin both edges.
TEST(WelcomeScreenLayoutTest, SplitBreakpointSitsAt800TimesScale) {
  EXPECT_TRUE(
      WelcomeScreenTestPeer::ShouldUseStackedLayout(799.0f, 600.0f, 1.0f));
  EXPECT_FALSE(
      WelcomeScreenTestPeer::ShouldUseStackedLayout(801.0f, 600.0f, 1.0f));
  EXPECT_TRUE(
      WelcomeScreenTestPeer::ShouldUseStackedLayout(1599.0f, 1200.0f, 2.0f));
  EXPECT_FALSE(
      WelcomeScreenTestPeer::ShouldUseStackedLayout(1601.0f, 1200.0f, 2.0f));
}

TEST(WelcomeScreenLayoutTest, ScalesSplitBreakpointWithFontSize) {
  EXPECT_FALSE(
      WelcomeScreenTestPeer::ShouldUseStackedLayout(920.0f, 600.0f, 1.0f));
  EXPECT_TRUE(
      WelcomeScreenTestPeer::ShouldUseStackedLayout(920.0f, 1200.0f, 2.0f));
  EXPECT_FALSE(
      WelcomeScreenTestPeer::ShouldUseStackedLayout(1820.0f, 1200.0f, 2.0f));
}

// Split puts the action rail and the recents side by side, so it needs
// max(left, right) of vertical space; stacked runs them in sequence and needs
// their sum. Falling back to stacked on a short card therefore picked the
// layout that needs MORE of the axis that just ran out. Height is not a
// reason to stack — only width is.
// The Display Density preset scales FramePadding, not font size, so a start
// button sized off the font alone ignores it. A previous fix derived the
// heights from GetFrameHeight but kept flat 34/28px floors, which clamped
// Compact and Normal to the same value at the shipped 16px font — the density
// setting still did nothing, and no test noticed. Measure the real buttons.
TEST_F(WelcomeScreenDensityTest, StartButtonHeightsGrowWithDisplayDensity) {
  const float compact =
      MeasurePrimaryButtonHeight(gui::DensityPreset::kCompact);
  const float normal = MeasurePrimaryButtonHeight(gui::DensityPreset::kNormal);
  const float comfortable =
      MeasurePrimaryButtonHeight(gui::DensityPreset::kComfortable);

  EXPECT_LT(compact, normal) << "Compact is not smaller than Normal";
  EXPECT_LT(normal, comfortable) << "Comfortable is not larger than Normal";
}

TEST(WelcomeScreenLayoutTest, ShortCardsKeepTheSplitLayout) {
  // Wide enough for two columns, far too short for the old 420 threshold.
  EXPECT_FALSE(
      WelcomeScreenTestPeer::ShouldUseStackedLayout(1200.0f, 200.0f, 1.0f));
  EXPECT_FALSE(
      WelcomeScreenTestPeer::ShouldUseStackedLayout(900.0f, 50.0f, 1.0f));
  // Narrow still stacks, at any height.
  EXPECT_TRUE(
      WelcomeScreenTestPeer::ShouldUseStackedLayout(500.0f, 2000.0f, 1.0f));
}

TEST(WelcomeScreenLayoutTest, ReservesMoreHintWhenOnlyOneRecentRowFits) {
  constexpr float kRowHeight = 44.0f;
  constexpr float kRowGap = 4.0f;
  constexpr float kMoreLineHeight = 17.0f;

  EXPECT_EQ(WelcomeScreenTestPeer::CalculateVisibleRecentCount(
                6, kRowHeight, kRowHeight, kRowGap, kMoreLineHeight),
            0);
  EXPECT_EQ(WelcomeScreenTestPeer::CalculateVisibleRecentCount(
                6, kRowHeight + kRowGap + kMoreLineHeight, kRowHeight, kRowGap,
                kMoreLineHeight),
            1);
}

TEST_F(WelcomeScreenTest, MissingRecentDoesNotOfferResumeAction) {
  const std::string missing_path = "/missing/welcome-resume-test.sfc";
  project::RecentFilesManager::GetInstance().AddFile(missing_path);

  WelcomeScreen screen;
  screen.RefreshRecentProjects();
  ASSERT_EQ(screen.recent_projects().entries().size(), 1u);
  ASSERT_TRUE(screen.recent_projects().entries().front().is_missing);
  WelcomeScreenTestPeer::SetEntryTime(&screen, 1.0f);
  screen.SetOpenProjectCallback([](const std::string&) {});

  ImGui::NewFrame();
  ImGui::SetNextWindowSize(ImVec2(520.0f, 360.0f), ImGuiCond_Always);
  ImGui::Begin(
      "WelcomeMissingResumeHost", nullptr,
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
  WelcomeScreenTestPeer::DrawQuickActions(&screen);

  const ImGuiID new_project_id =
      ImGui::GetID(ICON_MD_ADD_CIRCLE " New Project");
  EXPECT_EQ(ImGui::GetItemID(), new_project_id);

  ImGui::End();
  ImGui::EndFrame();
}

TEST(WelcomeScreenSelectionTest, ResumeUsesRecencyInsteadOfPinnedDisplayOrder) {
  RecentProject older_pinned;
  older_pinned.name = "older-pinned.sfc";
  older_pinned.filepath = "/roms/older-pinned.sfc";
  older_pinned.pinned = true;
  older_pinned.recent_index = 1;

  RecentProject newest;
  newest.name = "newest.sfc";
  newest.filepath = "/roms/newest.sfc";
  newest.recent_index = 0;

  const std::vector<RecentProject> display_order = {older_pinned, newest};
  const RecentProject* resume =
      WelcomeScreenTestPeer::FindResumeProject(display_order);

  ASSERT_NE(resume, nullptr);
  EXPECT_EQ(resume->filepath, newest.filepath);
}

TEST_F(WelcomeScreenTest, CompactCardStaysInsideSmallBrowserViewport) {
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(300.0f, 260.0f);

  WelcomeScreen screen;
  WelcomeScreenTestPeer::SetEntryTime(&screen, 1.0f);
  screen.SetOpenRomCallback([]() {});
  screen.SetNewProjectCallback([]() {});
  screen.SetOpenPrototypeResearchCallback([]() {});
  screen.SetOpenAssemblyEditorNoRomCallback([]() {});

  for (int frame = 0; frame < 3; ++frame) {
    ImGui::NewFrame();
    bool open = true;
    screen.Show(&open);
    EXPECT_TRUE(open);
    ImGui::EndFrame();
  }

  ImGuiWindow* root = ImGui::FindWindowByName("##WelcomeScreen");
  ASSERT_NE(root, nullptr);
  EXPECT_GE(root->Pos.x, 0.0f);
  EXPECT_GE(root->Pos.y, 0.0f);
  EXPECT_LE(root->Pos.x + root->Size.x, io.DisplaySize.x);
  EXPECT_LE(root->Pos.y + root->Size.y, io.DisplaySize.y);
}

TEST_F(WelcomeScreenTest, RecentCardActivatesFromKeyboardNavigation) {
  WelcomeScreen screen;
  RecentProject project;
  project.name = "keyboard-test.sfc";
  project.filepath = "/tmp/keyboard-test.sfc";
  project.rom_title = "THE LEGEND OF ZELDA";
  project.metadata_summary = "USA - LoROM";
  project.last_modified = "just now";
  project.item_type = "ROM";

  int open_count = 0;
  std::string opened_path;
  screen.SetOpenProjectCallback([&](const std::string& path) {
    ++open_count;
    opened_path = path;
  });

  DrawCardFrame(&screen, project, /*request_keyboard_focus=*/true);
  DrawCardFrame(&screen, project, /*request_keyboard_focus=*/false);
  ASSERT_NE(expected_card_id_, 0u);
  ASSERT_EQ(context_->NavId, expected_card_id_);

  DrawCardFrame(&screen, project, /*request_keyboard_focus=*/false,
                [](ImGuiIO& io) { io.AddKeyEvent(ImGuiKey_Enter, true); });

  EXPECT_EQ(open_count, 1);
  EXPECT_EQ(opened_path, project.filepath);
}

TEST_F(WelcomeScreenTest, MissingRecentCardDoesNotActivateOpenCallback) {
  WelcomeScreen screen;
  RecentProject project;
  project.name = "missing-test.sfc";
  project.filepath = "/missing/missing-test.sfc";
  project.item_type = "Missing";
  project.is_missing = true;

  int open_count = 0;
  screen.SetOpenProjectCallback([&](const std::string&) { ++open_count; });

  DrawCardFrame(&screen, project, /*request_keyboard_focus=*/true);
  DrawCardFrame(&screen, project, /*request_keyboard_focus=*/false);
  ASSERT_NE(expected_card_id_, 0u);
  ASSERT_EQ(context_->NavId, expected_card_id_);

  DrawCardFrame(&screen, project, /*request_keyboard_focus=*/false,
                [](ImGuiIO& io) { io.AddKeyEvent(ImGuiKey_Enter, true); });

  EXPECT_EQ(open_count, 0);
}

TEST_F(WelcomeScreenTest, UnavailableRecentCardDoesNotActivateOpenCallback) {
  WelcomeScreen screen;
  RecentProject project;
  project.name = "permission-test.sfc";
  project.filepath = "/unavailable/permission-test.sfc";
  project.item_type = "Unavailable";
  project.unavailable = true;

  int open_count = 0;
  screen.SetOpenProjectCallback([&](const std::string&) { ++open_count; });

  DrawCardFrame(&screen, project, /*request_keyboard_focus=*/true);
  DrawCardFrame(&screen, project, /*request_keyboard_focus=*/false);
  ASSERT_NE(expected_card_id_, 0u);
  ASSERT_EQ(context_->NavId, expected_card_id_);

  DrawCardFrame(&screen, project, /*request_keyboard_focus=*/false,
                [](ImGuiIO& io) { io.AddKeyEvent(ImGuiKey_Enter, true); });

  EXPECT_EQ(open_count, 0);
}

TEST_F(WelcomeScreenTest, SecondaryStartActionsKeepImGuiStacksBalanced) {
  WelcomeScreen screen;
  WelcomeScreenTestPeer::SetEntryTime(&screen, 1.0f);
  screen.SetOpenPrototypeResearchCallback([]() {});
  screen.SetOpenAssemblyEditorNoRomCallback([]() {});

  ImGui::NewFrame();
  ImGui::SetNextWindowSize(ImVec2(520.0f, 360.0f), ImGuiCond_Always);
  ImGui::Begin(
      "WelcomeActionsHost", nullptr,
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

  const int style_before = context_->StyleVarStack.Size;
  const int color_before = context_->ColorStack.Size;
  const int window_stack_before = context_->CurrentWindowStack.Size;
  const int begin_popup_before = context_->BeginPopupStack.Size;
  const int open_popup_before = context_->OpenPopupStack.Size;

  WelcomeScreenTestPeer::DrawQuickActions(&screen);

  EXPECT_EQ(context_->StyleVarStack.Size, style_before);
  EXPECT_EQ(context_->ColorStack.Size, color_before);
  EXPECT_EQ(context_->CurrentWindowStack.Size, window_stack_before);
  EXPECT_EQ(context_->BeginPopupStack.Size, begin_popup_before);
  EXPECT_EQ(context_->OpenPopupStack.Size, open_popup_before);

  ImGui::End();
  ImGui::EndFrame();
}

#ifndef __EMSCRIPTEN__
TEST_F(WelcomeScreenTest, ReleaseNotesOpenerPreservesUrlAndPropagatesFailure) {
  struct OpenRequest {
    std::string url;
    int calls = 0;
    bool result = false;
  } request;
  auto& platform_io = ImGui::GetPlatformIO();
  platform_io.Platform_OpenInShellUserData = &request;
  platform_io.Platform_OpenInShellFn = [](ImGuiContext* context,
                                          const char* url) {
    auto* request = static_cast<OpenRequest*>(
        context->PlatformIO.Platform_OpenInShellUserData);
    request->url = url;
    ++request->calls;
    return request->result;
  };

  EXPECT_FALSE(gui::OpenUrl(""));
  EXPECT_EQ(request.calls, 0);
  const std::string url = "https://example.test/notes?q=one two&v=0.8";
  EXPECT_FALSE(gui::OpenUrl(url));
  EXPECT_EQ(request.url, url);
  request.result = true;
  EXPECT_TRUE(gui::OpenUrl(url));
  EXPECT_EQ(request.calls, 2);

  platform_io.Platform_OpenInShellFn = nullptr;
  EXPECT_FALSE(gui::OpenUrl(url));
  ImGui::SetCurrentContext(nullptr);
  EXPECT_FALSE(gui::OpenUrl(url));
  ImGui::SetCurrentContext(context_);
}
#endif

}  // namespace
}  // namespace yaze::editor
