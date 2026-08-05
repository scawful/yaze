#include "app/service/imgui_test_harness_internal.h"

#include <gtest/gtest.h>

#include <string>

#include "app/gui/automation/widget_id_registry.h"
#include "app/testing/test_manager.h"

namespace yaze::test {
namespace {

class WidgetRegistryReset {
 public:
  WidgetRegistryReset() { gui::WidgetIdRegistry::Instance().Clear(); }
  ~WidgetRegistryReset() { gui::WidgetIdRegistry::Instance().Clear(); }
};

TEST(ImGuiTestHarnessLogicTest,
     WidgetKeyPreservesExactIdsWhenLabelsAreDuplicated) {
  WidgetRegistryReset reset;
  auto& registry = gui::WidgetIdRegistry::Instance();

  gui::WidgetIdRegistry::WidgetMetadata metadata;
  metadata.label = "Save anyway";
  registry.RegisterWidget("first/button:Save anyway", "button", 0xA11CE001,
                          "first duplicate", metadata);
  registry.RegisterWidget("second/button:Save anyway", "button", 0xA11CE002,
                          "second duplicate", metadata);

  auto first = internal::ResolveWidgetSelector("", "first/button:Save anyway");
  auto second =
      internal::ResolveWidgetSelector("", "second/button:Save anyway");

  ASSERT_TRUE(first.ok()) << first.status();
  ASSERT_TRUE(second.ok()) << second.status();
  EXPECT_EQ(first->target.label, second->target.label);
  EXPECT_EQ(first->imgui_id, 0xA11CE001u);
  EXPECT_EQ(second->imgui_id, 0xA11CE002u);
  EXPECT_NE(first->imgui_id, second->imgui_id);
}

TEST(ImGuiTestHarnessLogicTest, WidgetKeyWithoutImGuiIdFailsClosed) {
  WidgetRegistryReset reset;
  gui::WidgetIdRegistry::Instance().RegisterWidget(
      "broken/button:Save anyway", "button", 0, "invalid test widget");

  auto resolved =
      internal::ResolveWidgetSelector("", "broken/button:Save anyway");

  ASSERT_FALSE(resolved.ok());
  EXPECT_NE(std::string(resolved.status().message()).find("valid ImGui ID"),
            std::string::npos);
}

TEST(ImGuiTestHarnessLogicTest,
     ReplayTerminalFailureOverridesQueuedRpcSuccess) {
  HarnessTestExecution execution;
  execution.status = HarnessTestStatus::kFailed;
  execution.error_message = "exact widget action failed";
  bool step_success = true;
  std::string step_message = "Queued click";

  internal::ApplyTerminalHarnessExecution(execution, &step_success,
                                          &step_message);

  EXPECT_FALSE(step_success);
  EXPECT_EQ(step_message, "exact widget action failed");
}

TEST(ImGuiTestHarnessLogicTest, ReplayTerminalPassOverridesQueuedFailure) {
  HarnessTestExecution execution;
  execution.status = HarnessTestStatus::kPassed;
  bool step_success = false;
  std::string step_message = "Queued assertion";

  internal::ApplyTerminalHarnessExecution(execution, &step_success,
                                          &step_message);

  EXPECT_TRUE(step_success);
  EXPECT_EQ(step_message, "Queued assertion");
}

}  // namespace
}  // namespace yaze::test
