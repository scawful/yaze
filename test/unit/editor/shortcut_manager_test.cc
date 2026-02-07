#include <gtest/gtest.h>

#include "app/editor/system/shortcut_manager.h"
#include "imgui/imgui.h"

namespace yaze::editor {
namespace {

class ShortcutManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    imgui_context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui_context_);

    // Ensure fonts are built to avoid asserts in ImGui helpers.
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  }

  void TearDown() override {
    if (imgui_context_) {
      ImGui::DestroyContext(imgui_context_);
      imgui_context_ = nullptr;
    }
  }

  void RunFrame(std::function<void(ImGuiIO&)> inject_events,
                std::function<void()> frame_body) {
    ImGuiIO& io = ImGui::GetIO();
    if (inject_events) {
      inject_events(io);  // Must happen before NewFrame() to be processed.
    }
    io.DisplaySize = ImVec2(1280, 720);
    io.DeltaTime = 1.0f / 60.0f;

    ImGui::NewFrame();
    // Ensure there is a focused host window so ImGui key routing reports
    // IsKeyPressed/IsKeyDown as expected.
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(200, 200));
    ImGui::SetNextWindowFocus();
    ImGui::Begin("##ShortcutHost", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
    if (frame_body) {
      frame_body();
    }
    ImGui::End();
    ImGui::EndFrame();
    ImGui::Render();
  }

 private:
  ImGuiContext* imgui_context_ = nullptr;
};

TEST_F(ShortcutManagerTest, PrefersMoreSpecificShortcut) {
  ShortcutManager shortcuts;
  int save_called = 0;
  int save_as_called = 0;

  shortcuts.RegisterShortcut(
      "Save", {ImGuiMod_Ctrl, ImGuiKey_S},
      [&]() { ++save_called; }, Shortcut::Scope::kGlobal);
  shortcuts.RegisterShortcut(
      "Save As", {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_S},
      [&]() { ++save_as_called; }, Shortcut::Scope::kGlobal);

  RunFrame(nullptr, [&]() { ExecuteShortcuts(shortcuts); });
  RunFrame(
      [](ImGuiIO& io) {
        const ImGuiKey primary =
            io.ConfigMacOSXBehaviors ? ImGuiMod_Super : ImGuiMod_Ctrl;
        io.AddKeyEvent(primary, true);
        io.AddKeyEvent(ImGuiMod_Shift, true);
        io.AddKeyEvent(ImGuiKey_S, true);
      },
      [&]() { ExecuteShortcuts(shortcuts); });

  EXPECT_EQ(save_called, 0);
  EXPECT_EQ(save_as_called, 1);
}

TEST_F(ShortcutManagerTest, DoesNotGateOnWantCaptureKeyboard) {
  ShortcutManager shortcuts;
  int palette_called = 0;

  shortcuts.RegisterShortcut(
      "Command Palette", {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_P},
      [&]() { ++palette_called; }, Shortcut::Scope::kGlobal);

  RunFrame(nullptr, [&]() { ExecuteShortcuts(shortcuts); });
  RunFrame(
      [](ImGuiIO& io) {
        const ImGuiKey primary =
            io.ConfigMacOSXBehaviors ? ImGuiMod_Super : ImGuiMod_Ctrl;
        io.AddKeyEvent(primary, true);
        io.AddKeyEvent(ImGuiMod_Shift, true);
        io.AddKeyEvent(ImGuiKey_P, true);
      },
      [&]() {
        ImGui::GetIO().WantCaptureKeyboard = true;
        ExecuteShortcuts(shortcuts);
      });

  EXPECT_EQ(palette_called, 1);
}

TEST_F(ShortcutManagerTest, WantTextInputBlocksPlainKeysButAllowsModifiedChords) {
  ShortcutManager shortcuts;
  int space_called = 0;
  int palette_called = 0;

  shortcuts.RegisterShortcut(
      "Play/Pause", {ImGuiKey_Space},
      [&]() { ++space_called; }, Shortcut::Scope::kEditor);
  shortcuts.RegisterShortcut(
      "Command Palette", {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_P},
      [&]() { ++palette_called; }, Shortcut::Scope::kGlobal);

  RunFrame(nullptr, [&]() { ExecuteShortcuts(shortcuts); });

  // Plain keys are ignored while typing.
  RunFrame(
      [](ImGuiIO& io) {
        io.AddKeyEvent(ImGuiKey_Space, true);
      },
      [&]() {
        ImGui::GetIO().WantTextInput = true;
        ExecuteShortcuts(shortcuts);
      });

  // Modified chords still work while typing.
  RunFrame(
      [](ImGuiIO& io) {
        const ImGuiKey primary =
            io.ConfigMacOSXBehaviors ? ImGuiMod_Super : ImGuiMod_Ctrl;
        io.AddKeyEvent(primary, true);
        io.AddKeyEvent(ImGuiMod_Shift, true);
        io.AddKeyEvent(ImGuiKey_P, true);
      },
      [&]() {
        ImGui::GetIO().WantTextInput = true;
        ExecuteShortcuts(shortcuts);
      });

  EXPECT_EQ(space_called, 0);
  EXPECT_EQ(palette_called, 1);
}

}  // namespace
}  // namespace yaze::editor
