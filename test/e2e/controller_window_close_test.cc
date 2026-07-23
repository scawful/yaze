#include <gtest/gtest.h>

#include <memory>

#include "app/controller.h"
#include "app/platform/null_window_backend.h"
#include "imgui/imgui.h"

namespace yaze {
namespace {

class QueuedWindowBackend : public platform::NullWindowBackend {
 public:
  explicit QueuedWindowBackend(platform::WindowEventType event_type)
      : event_type_(event_type) {}

  bool PollEvent(platform::WindowEvent& out_event) override {
    if (delivered_) {
      return false;
    }
    delivered_ = true;
    out_event.type = event_type_;
    return true;
  }

 private:
  platform::WindowEventType event_type_;
  bool delivered_ = false;
};

class ControllerWindowCloseTest
    : public ::testing::TestWithParam<platform::WindowEventType> {};

TEST_P(ControllerWindowCloseTest, RoutesNativeCloseThroughEditorQuit) {
  ImGuiContext* imgui = ImGui::CreateContext();
  {
    Controller controller;
    controller.set_active(true);
    controller.SetWindowBackendForTesting(
        std::make_unique<QueuedWindowBackend>(GetParam()));

    controller.OnInput();

    EXPECT_TRUE(controller.active());
    ASSERT_NE(controller.window_backend(), nullptr);
    EXPECT_TRUE(controller.window_backend()->IsActive());
    EXPECT_TRUE(controller.editor_manager()->quit());
  }
  ImGui::DestroyContext(imgui);
}

INSTANTIATE_TEST_SUITE_P(NativeCloseEvents, ControllerWindowCloseTest,
                         ::testing::Values(platform::WindowEventType::Quit,
                                           platform::WindowEventType::Close));

}  // namespace
}  // namespace yaze
