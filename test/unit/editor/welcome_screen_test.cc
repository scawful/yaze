#include "app/editor/shell/coordinator/welcome_screen.h"

#include <functional>
#include <string>

#include <gtest/gtest.h>

#include "app/gui/core/input.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze::editor {

class WelcomeScreenTestPeer {
 public:
  static void SetEntryTime(WelcomeScreen* screen, float entry_time) {
    screen->entry_time_ = entry_time;
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

  static ImGuiID ProjectPanelId(int index) {
    ImGui::PushID(index);
    const ImGuiID id = ImGui::GetID("ProjectPanel");
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
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigNavCursorVisibleAlways = true;
    io.DisplaySize = ImVec2(800.0f, 600.0f);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefault();

    unsigned char* pixels = nullptr;
    int atlas_width = 0;
    int atlas_height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &atlas_width, &atlas_height);
  }

  void TearDown() override {
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
};

TEST(WelcomeScreenLayoutTest, ScalesSplitBreakpointWithFontSize) {
  EXPECT_FALSE(
      WelcomeScreenTestPeer::ShouldUseStackedLayout(920.0f, 600.0f, 1.0f));
  EXPECT_TRUE(
      WelcomeScreenTestPeer::ShouldUseStackedLayout(920.0f, 1200.0f, 2.0f));
  EXPECT_FALSE(
      WelcomeScreenTestPeer::ShouldUseStackedLayout(1820.0f, 1200.0f, 2.0f));
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

TEST_F(WelcomeScreenTest, MoreWaysPopupKeepsImGuiStacksBalanced) {
  WelcomeScreen screen;
  WelcomeScreenTestPeer::SetEntryTime(&screen, 1.0f);
  screen.SetOpenPrototypeResearchCallback([]() {});
  screen.SetOpenAssemblyEditorNoRomCallback([]() {});

  ImGui::NewFrame();
  ImGui::SetNextWindowSize(ImVec2(520.0f, 360.0f), ImGuiCond_Always);
  ImGui::Begin(
      "WelcomeActionsHost", nullptr,
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
  ImGui::OpenPopup("WelcomeMoreStartWays");
  ASSERT_TRUE(ImGui::IsPopupOpen("WelcomeMoreStartWays"));

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
