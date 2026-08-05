#include "app/service/imgui_test_harness_internal.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
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

class ReentrantHarnessListener : public HarnessListener {
 public:
  explicit ReentrantHarnessListener(TestManager* manager) : manager_(manager) {}

  void OnHarnessTestUpdated(const HarnessTestExecution& execution) override {
    ++update_count;
    auto current = manager_->GetHarnessTestExecution(execution.test_id);
    history_query_succeeded = current.ok();
  }

  void OnHarnessPlanSummary(const std::string&) override {}

  int update_count = 0;
  bool history_query_succeeded = false;

 private:
  TestManager* manager_;
};

class HarnessCallbackReset {
 public:
  explicit HarnessCallbackReset(TestManager* manager) : manager_(manager) {}
  ~HarnessCallbackReset() {
    manager_->SetFailureScreenshotRequester({});
    manager_->SetHarnessListener(nullptr);
  }

 private:
  TestManager* manager_;
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

TEST(ImGuiTestHarnessLogicTest, ShortcutTargetPreservesCommandName) {
  auto resolved = internal::ResolveWidgetSelector("shortcut:Save ROM", "");

  ASSERT_TRUE(resolved.ok()) << resolved.status();
  EXPECT_EQ(resolved->target.type, "shortcut");
  EXPECT_EQ(resolved->target.label, "Save ROM");
  EXPECT_EQ(resolved->imgui_id, 0u);
}

TEST(ImGuiTestHarnessLogicTest, WidgetRegistryQueriesReturnStableSnapshots) {
  WidgetRegistryReset reset;
  auto& registry = gui::WidgetIdRegistry::Instance();
  registry.RegisterWidget("Dungeon/Workbench/button:mode", "button", 0x101,
                          "mode");

  const auto widget = registry.GetWidgetInfo("Dungeon/Workbench/button:mode");
  const auto all_widgets = registry.GetAllWidgets();
  ASSERT_TRUE(widget.has_value());
  ASSERT_EQ(all_widgets.count("Dungeon/Workbench/button:mode"), 1);

  registry.RegisterWidget("Dungeon/Workbench/button:mode", "button", 0x202,
                          "updated mode");

  EXPECT_EQ(widget->imgui_id, 0x101u);
  EXPECT_EQ(all_widgets.at("Dungeon/Workbench/button:mode").imgui_id, 0x101u);
  EXPECT_EQ(registry.GetWidgetId("Dungeon/Workbench/button:mode"), 0x202u);
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

TEST(ImGuiTestHarnessLogicTest,
     FailedCompletionReturnsAndPersistsUnavailableContext) {
  auto& manager = TestManager::Get();
  HarnessCallbackReset reset(&manager);
  manager.SetHarnessListener(nullptr);
  manager.SetFailureScreenshotRequester({});
  const std::string test_id =
      manager.RegisterHarnessTest("failure-deadlock-regression", "grpc");

  manager.MarkHarnessTestRunning(test_id);
  manager.MarkHarnessTestCompleted(test_id, HarnessTestStatus::kFailed,
                                   "expected failure");

  auto execution = manager.GetHarnessTestExecution(test_id);
  ASSERT_TRUE(execution.ok()) << execution.status();
  EXPECT_EQ(execution->status, HarnessTestStatus::kFailed);
  EXPECT_EQ(execution->failure_context,
            "Harness failure context capture unavailable");

  const std::string followup_id =
      manager.RegisterHarnessTest("failure-deadlock-followup", "grpc");
  EXPECT_FALSE(followup_id.empty());
}

TEST(ImGuiTestHarnessLogicTest, FailureContextUpdatesAggregateLatestExecution) {
  auto& manager = TestManager::Get();
  HarnessCallbackReset reset(&manager);
  manager.SetHarnessListener(nullptr);
  manager.SetFailureScreenshotRequester({});
  constexpr char kName[] = "failure-context-aggregate-regression";
  const std::string test_id = manager.RegisterHarnessTest(kName, "grpc");

  manager.MarkHarnessTestRunning(test_id);
  manager.MarkHarnessTestCompleted(test_id, HarnessTestStatus::kTimeout,
                                   "expected timeout");

  auto execution = manager.GetHarnessTestExecution(test_id);
  ASSERT_TRUE(execution.ok()) << execution.status();
  auto summaries = manager.ListHarnessTestSummaries("grpc");
  auto summary = std::find_if(summaries.begin(), summaries.end(),
                              [&](const HarnessTestSummary& entry) {
                                return entry.latest_execution.name == kName;
                              });
  ASSERT_NE(summary, summaries.end());
  EXPECT_EQ(summary->latest_execution.status, execution->status);
  EXPECT_EQ(summary->latest_execution.failure_context,
            execution->failure_context);
  EXPECT_EQ(summary->latest_execution.screenshot_path,
            execution->screenshot_path);
  EXPECT_EQ(summary->fail_count, 1);
}

TEST(ImGuiTestHarnessLogicTest,
     DelayedFailureContextDoesNotReplaceNewerAggregateExecution) {
  auto& manager = TestManager::Get();
  HarnessCallbackReset reset(&manager);
  manager.SetHarnessListener(nullptr);
  TestManager::FailureScreenshotCallback delayed_callback;
  manager.SetFailureScreenshotRequester(
      [&](const std::string&, TestManager::FailureScreenshotCallback callback) {
        delayed_callback = std::move(callback);
      });
  constexpr char kName[] = "failure-context-stale-aggregate-regression";
  const std::string failed_id = manager.RegisterHarnessTest(kName, "grpc");
  manager.MarkHarnessTestRunning(failed_id);
  manager.MarkHarnessTestCompleted(failed_id, HarnessTestStatus::kFailed,
                                   "expected failure");
  ASSERT_TRUE(static_cast<bool>(delayed_callback));

  const std::string passed_id = manager.RegisterHarnessTest(kName, "grpc");
  manager.MarkHarnessTestRunning(passed_id);
  manager.MarkHarnessTestCompleted(passed_id, HarnessTestStatus::kPassed);

  delayed_callback(absl::FailedPreconditionError("capture unavailable"));
  manager.SetFailureScreenshotRequester({});

  auto summaries = manager.ListHarnessTestSummaries("grpc");
  auto summary = std::find_if(summaries.begin(), summaries.end(),
                              [&](const HarnessTestSummary& entry) {
                                return entry.latest_execution.name == kName;
                              });
  ASSERT_NE(summary, summaries.end());
  EXPECT_EQ(summary->latest_execution.test_id, passed_id);
  EXPECT_EQ(summary->latest_execution.status, HarnessTestStatus::kPassed);
  auto failed_execution = manager.GetHarnessTestExecution(failed_id);
  ASSERT_TRUE(failed_execution.ok()) << failed_execution.status();
  EXPECT_EQ(failed_execution->failure_context,
            "Harness failure context capture unavailable");
}

TEST(ImGuiTestHarnessLogicTest,
     SuccessfulFailureCaptureEnrichesHistoryWithoutDuplicateTelemetry) {
  auto& manager = TestManager::Get();
  HarnessCallbackReset reset(&manager);
  auto listener = std::make_shared<ReentrantHarnessListener>(&manager);
  manager.SetHarnessListener(listener);
  TestManager::FailureScreenshotCallback delayed_callback;
  manager.SetFailureScreenshotRequester(
      [&](const std::string&, TestManager::FailureScreenshotCallback callback) {
        delayed_callback = std::move(callback);
      });
  constexpr char kName[] = "failure-context-success-regression";
  const std::string test_id = manager.RegisterHarnessTest(kName, "grpc");
  manager.MarkHarnessTestRunning(test_id);
  manager.MarkHarnessTestCompleted(test_id, HarnessTestStatus::kFailed,
                                   "expected failure");
  ASSERT_TRUE(static_cast<bool>(delayed_callback));
  EXPECT_EQ(listener->update_count, 1);

  ScreenshotArtifact artifact;
  artifact.file_path = "/tmp/harness-failure.bmp";
  artifact.file_size_bytes = 1234;
  delayed_callback(artifact);

  auto execution = manager.GetHarnessTestExecution(test_id);
  ASSERT_TRUE(execution.ok()) << execution.status();
  EXPECT_EQ(execution->failure_context,
            "Harness failure context captured successfully");
  EXPECT_EQ(execution->screenshot_path, artifact.file_path);
  EXPECT_EQ(execution->screenshot_size_bytes, artifact.file_size_bytes);
  EXPECT_EQ(listener->update_count, 1);

  manager.SetFailureScreenshotRequester({});
  manager.ClearHarnessListener(listener.get());
}

TEST(ImGuiTestHarnessLogicTest, ListenerCanReenterHistoryQuery) {
  auto& manager = TestManager::Get();
  HarnessCallbackReset reset(&manager);
  auto listener = std::make_shared<ReentrantHarnessListener>(&manager);
  manager.SetHarnessListener(listener);
  const std::string test_id =
      manager.RegisterHarnessTest("listener-reentry-regression", "grpc");

  manager.MarkHarnessTestRunning(test_id);
  manager.MarkHarnessTestCompleted(test_id, HarnessTestStatus::kPassed);

  EXPECT_EQ(listener->update_count, 1);
  EXPECT_TRUE(listener->history_query_succeeded);
  manager.ClearHarnessListener(listener.get());
}

TEST(ImGuiTestHarnessLogicTest, ListenerLifetimeSurvivesCallerRelease) {
  auto& manager = TestManager::Get();
  HarnessCallbackReset reset(&manager);
  auto listener = std::make_shared<ReentrantHarnessListener>(&manager);
  std::weak_ptr<ReentrantHarnessListener> weak_listener = listener;
  manager.SetHarnessListener(listener);
  listener.reset();
  ASSERT_FALSE(weak_listener.expired());

  const std::string test_id =
      manager.RegisterHarnessTest("listener-lifetime-regression", "grpc");
  manager.MarkHarnessTestRunning(test_id);
  manager.MarkHarnessTestCompleted(test_id, HarnessTestStatus::kPassed);

  auto retained_listener = weak_listener.lock();
  ASSERT_NE(retained_listener, nullptr);
  EXPECT_EQ(retained_listener->update_count, 1);
  manager.ClearHarnessListener(retained_listener.get());
  retained_listener.reset();
  EXPECT_TRUE(weak_listener.expired());
}

}  // namespace
}  // namespace yaze::test
