#include "app/editor/shell/feedback/popup_manager.h"

#include <gtest/gtest.h>

#include "imgui/imgui.h"

namespace yaze::editor {
namespace {

struct ScopedImGuiContext {
  ImGuiContext* ctx = nullptr;
  ScopedImGuiContext() {
    ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(ctx);
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1280, 720);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  }
  ~ScopedImGuiContext() { ImGui::DestroyContext(ctx); }
};

void RunFrames(PopupManager& popups, int frames) {
  for (int i = 0; i < frames; ++i) {
    ImGui::NewFrame();
    popups.DrawPopups();
    ImGui::Render();
  }
}

// Save As starts a save that needs confirmation, and the confirmation popup is
// shown from inside the Save As popup. Both stay visible in the manager. If
// each frame reopens both modals, they close each other, stay hidden while
// auto-sizing, and the open modal blocks all input: the app looks frozen.
TEST(PopupManagerTest, PopupShownFromAnotherPopupStaysOpen) {
  ScopedImGuiContext imgui;
  PopupManager popups(nullptr);
  int confirm_appearances = 0;
  int save_as_appearances = 0;
  popups.RegisterPopup("Save As", PopupType::kFileOperation, [&] {
    if (ImGui::IsWindowAppearing()) {
      ++save_as_appearances;
    }
    if (!popups.IsVisible("Confirm")) {
      popups.Show("Confirm");
    }
  });
  popups.RegisterPopup("Confirm", PopupType::kConfirmation, [&] {
    if (ImGui::IsWindowAppearing()) {
      ++confirm_appearances;
    }
  });

  popups.Show("Save As");
  RunFrames(popups, 12);
  EXPECT_EQ(confirm_appearances, 1)
      << "the confirmation popup is reopened every frame";
  EXPECT_EQ(save_as_appearances, 1);
  EXPECT_TRUE(popups.IsVisible("Save As"));
  EXPECT_TRUE(popups.IsVisible("Confirm"));
}

// Closing the top popup returns to the one underneath.
TEST(PopupManagerTest, HidingTopPopupRedrawsTheOneBelow) {
  ScopedImGuiContext imgui;
  PopupManager popups(nullptr);
  int lower_draws = 0;
  popups.RegisterPopup("Lower", PopupType::kFileOperation,
                       [&] { ++lower_draws; });
  popups.RegisterPopup("Upper", PopupType::kConfirmation, [] {});

  popups.Show("Lower");
  RunFrames(popups, 3);
  popups.Show("Upper");
  RunFrames(popups, 3);
  popups.Hide("Upper");
  const int lower_before = lower_draws;
  RunFrames(popups, 4);
  EXPECT_GT(lower_draws, lower_before);
}

}  // namespace
}  // namespace yaze::editor
