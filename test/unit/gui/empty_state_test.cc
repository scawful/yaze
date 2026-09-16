#include "app/gui/widgets/empty_state.h"

#include <gtest/gtest.h>

#include "app/gui/core/icons.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze {
namespace gui {
namespace {

class EmptyStateTest : public ::testing::Test {
 protected:
  void SetUp() override {
    IMGUI_CHECKVERSION();
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(800.0f, 600.0f);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefault();
    unsigned char* pixels = nullptr;
    int w = 0;
    int h = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
  }

  void TearDown() override {
    ImGui::DestroyContext(context_);
    context_ = nullptr;
  }

  ImGuiContext* context_ = nullptr;
};

TEST_F(EmptyStateTest, PresetsHaveSharedCopy) {
  const auto no_rom = EmptyNoRom();
  EXPECT_STREQ(no_rom.title, "Open a ROM");
  EXPECT_NE(no_rom.detail, nullptr);
  EXPECT_NE(no_rom.icon, nullptr);

  const auto no_sel = EmptyNoSelection(true);
  EXPECT_STREQ(no_sel.title, "Select something");
  EXPECT_TRUE(no_sel.compact);

  const auto canvas = EmptySelectInCanvas();
  EXPECT_STREQ(canvas.title, "Select in the canvas");

  const auto project = EmptyNoProject();
  EXPECT_STREQ(project.title, "Open a project");

  const auto loading = EmptyLoading("tiles");
  EXPECT_STREQ(loading.title, "Loading…");
  EXPECT_STREQ(loading.detail, "tiles");
}

TEST_F(EmptyStateTest, DrawEmptyStateDoesNotInvokeActionWithoutActivation) {
  ImGui::NewFrame();
  ImGui::Begin("##EmptyStateHarness");

  bool fired = false;
  EmptyStateOptions opts;
  opts.icon = ICON_MD_INFO;
  opts.title = "Title";
  opts.detail = "Detail";
  opts.action_label = "Do It";
  opts.on_action = [&]() {
    fired = true;
  };

  // Without a real click, Draw should return false and not fire.
  EXPECT_FALSE(DrawEmptyState(opts));
  EXPECT_FALSE(fired);

  ImGui::End();
  ImGui::EndFrame();
  ImGui::Render();
}

TEST_F(EmptyStateTest, ActionActivationReturnsTrueAndInvokesCallbackOnce) {
  int calls = 0;
  EmptyStateOptions opts;
  opts.title = "Open a ROM";
  opts.action_label = "Open##EmptyStateAction";
  opts.on_action = [&]() {
    ++calls;
  };
  ImGuiID action_id = 0;
  auto draw_frame = [&]() {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 240), ImGuiCond_Always);
    ImGui::Begin("EmptyStateActionHost", nullptr,
                 ImGuiWindowFlags_NoSavedSettings);
    action_id = ImGui::GetID(opts.action_label);
    const bool activated = DrawEmptyState(opts);
    ImGui::End();
    ImGui::Render();
    return activated;
  };

  EXPECT_FALSE(draw_frame());
  EXPECT_FALSE(draw_frame());
  EXPECT_EQ(calls, 0);

  // Exercise the actual ImGui button activation path, not the callback alone.
  ImGui::ActivateItemByID(action_id);
  EXPECT_TRUE(draw_frame());
  EXPECT_EQ(calls, 1);
  EXPECT_FALSE(draw_frame());
  EXPECT_EQ(calls, 1);

  // A caller may use the returned result without supplying a callback.
  opts.on_action = {};
  ImGui::ActivateItemByID(action_id);
  EXPECT_TRUE(draw_frame());
  EXPECT_EQ(calls, 1);
}

TEST_F(EmptyStateTest, DrawEmptyStateNoopsWhenEmpty) {
  ImGui::NewFrame();
  ImGui::Begin("##EmptyStateHarnessEmpty");
  EXPECT_FALSE(DrawEmptyState(EmptyStateOptions{}));
  ImGui::End();
  ImGui::EndFrame();
  ImGui::Render();
}

}  // namespace
}  // namespace gui
}  // namespace yaze
