#include "app/service/imgui_test_harness_internal.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include "app/editor/system/workspace/panel_host.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "app/gui/automation/widget_id_registry.h"
#include "app/service/imgui_test_harness_service.h"
#include "app/testing/test_manager.h"
#include "app/testing/test_script_parser.h"
#include "imgui/imgui_internal.h"
#include "protos/imgui_test_harness.pb.h"
#include "util/file_util.h"

namespace yaze::test {
namespace {

class HarnessWindowFocusTest : public ::testing::Test {
 protected:
  void SetUp() override {
    previous_ = ImGui::GetCurrentContext();
    context_ = ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = ImVec2(800, 600);
    io.DeltaTime = 1.0f / 60.0f;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    windows_.RegisterSession(0);
    windows_.RegisterSession(1);
    windows_.SetActiveSession(0);
    windows_.RegisterWindow(0, "test.focus", "Focus Target", "", "Test");
    windows_.RegisterWindow(0, "test.other", "Other Target", "", "Test");
    windows_.RegisterWindow(1, "test.foreign", "Foreign Target", "", "Test");
    target_ = host_.GetWorkspaceWindowName(0, "test.focus");
    other_ = host_.GetWorkspaceWindowName(0, "test.other");
  }

  void TearDown() override {
    ImGui::DestroyContext(context_);
    ImGui::SetCurrentContext(previous_);
  }

  void Frame(bool dock = false) {
    ImGui::NewFrame();
    if (dock) {
      const ImGuiID dock_id = ImHashStr("HarnessWindowFocusDock");
      ImGui::SetNextWindowPos(ImVec2(0, 0));
      ImGui::SetNextWindowSize(ImVec2(800, 600));
      ImGui::Begin("Focus Dock Host", nullptr, ImGuiWindowFlags_NoDocking);
      if (!docked_) {
        ImGui::DockBuilderRemoveNode(dock_id);
        ImGui::DockBuilderAddNode(dock_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dock_id, ImVec2(800, 600));
        ImGui::DockBuilderDockWindow(target_.c_str(), dock_id);
        ImGui::DockBuilderDockWindow(other_.c_str(), dock_id);
        ImGui::DockBuilderFinish(dock_id);
        docked_ = true;
      }
      ImGui::DockSpace(dock_id);
      ImGui::End();
    }
    for (const auto* name : {&target_, &other_}) {
      ImGui::Begin(name->c_str());
      ImGui::TextUnformatted("Window focus regression");
      ImGui::End();
    }
    ImGui::Render();
  }

  editor::WorkspaceWindowManager windows_;
  editor::WindowHost host_{&windows_};
  std::string target_, other_;
  ImGuiContext* previous_ = nullptr;
  ImGuiContext* context_ = nullptr;
  bool docked_ = false;
};

TEST_F(HarnessWindowFocusTest, FocusesRegisteredWindowThroughNormalHost) {
  Frame();
  const auto focused = internal::FocusHarnessWindow(&host_, 0, "test.focus");
  ASSERT_TRUE(focused.ok()) << focused.status();
  EXPECT_EQ(*focused, target_);
  Frame();
  EXPECT_TRUE(windows_.IsWindowOpen(0, "test.focus"));
  EXPECT_TRUE(internal::VerifyHarnessWindowFocus(*focused).ok());
}

TEST_F(HarnessWindowFocusTest, RejectsUnknownEmptyAndForeignSessions) {
  Frame();
  EXPECT_TRUE(absl::IsInvalidArgument(
      internal::FocusHarnessWindow(&host_, 0, "").status()));
  EXPECT_TRUE(absl::IsNotFound(
      internal::FocusHarnessWindow(&host_, 0, "unknown").status()));
  EXPECT_TRUE(absl::IsFailedPrecondition(
      internal::FocusHarnessWindow(&host_, 1, "test.focus").status()));
  EXPECT_TRUE(absl::IsFailedPrecondition(
      internal::FocusHarnessWindow(&host_, 99, "test.focus").status()));
  const auto* foreign = windows_.GetWindowDescriptor(1, "test.foreign");
  ASSERT_NE(foreign, nullptr);
  EXPECT_TRUE(absl::IsNotFound(
      internal::FocusHarnessWindow(&host_, 0, foreign->card_id).status()));
  windows_.SetActiveSession(1);
  EXPECT_TRUE(absl::IsNotFound(
      internal::FocusHarnessWindow(&host_, 1, "test.focus").status()));
  EXPECT_FALSE(windows_.IsWindowOpen(0, "test.focus"));
}

TEST_F(HarnessWindowFocusTest, RejectsUnavailableOrUndrawnWindowBeforeOpening) {
  EXPECT_TRUE(absl::IsFailedPrecondition(
      internal::FocusHarnessWindow(nullptr, 0, "test.focus").status()));
  EXPECT_TRUE(absl::IsFailedPrecondition(
      internal::FocusHarnessWindow(&host_, 0, "test.focus").status()));
  EXPECT_FALSE(windows_.IsWindowOpen(0, "test.focus"));
}

TEST_F(HarnessWindowFocusTest, FocusSelectsTheRequestedDockTab) {
  Frame(true);
  Frame(true);
  ASSERT_TRUE(internal::FocusHarnessWindow(&host_, 0, "test.other").ok());
  Frame(true);
  EXPECT_FALSE(internal::VerifyHarnessWindowFocus(target_).ok());
  const auto focused = internal::FocusHarnessWindow(&host_, 0, "test.focus");
  ASSERT_TRUE(focused.ok()) << focused.status();
  Frame(true);
  Frame(true);
  ASSERT_NE(ImGui::FindWindowByName(target_.c_str()), nullptr);
  EXPECT_TRUE(ImGui::FindWindowByName(target_.c_str())->DockTabIsVisible);
  EXPECT_TRUE(internal::VerifyHarnessWindowFocus(target_).ok());
}

TEST(ImGuiTestHarnessLogicTest, WindowTargetKeepsStableIdForUiDispatch) {
  const auto target = internal::ResolveWidgetSelector("window:test.focus", "");
  ASSERT_TRUE(target.ok()) << target.status();
  EXPECT_EQ(target->target.type, "window");
  EXPECT_EQ(target->target.label, "test.focus");
  EXPECT_EQ(target->imgui_id, 0u);
}

TEST(ProjectSaveDialogSeamTest, ValidatesSelectionBeforeKeyboardDispatch) {
  EXPECT_TRUE(internal::ValidateProjectSaveAsDestination("").ok());
  EXPECT_TRUE(absl::IsInvalidArgument(
      internal::ValidateProjectSaveAsDestination(nullptr)));
  EXPECT_TRUE(absl::IsInvalidArgument(
      internal::ValidateProjectSaveAsDestination("relative.yaze")));
}

TEST(ProjectSaveDialogSeamTest,
     RejectsExistingEntryAndReportsFilesystemErrors) {
  namespace fs = std::filesystem;
  const auto root =
      fs::temp_directory_path() /
      ("yaze-save-as-entry-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directory(root);
  struct Cleanup {
    fs::path root;
    ~Cleanup() {
      std::error_code error;
      fs::remove_all(root, error);
    }
  } cleanup{root};
  const auto destination = root / "occupied.yaze";
  EXPECT_TRUE(
      internal::ValidateProjectSaveAsDestination(destination.string().c_str())
          .ok());
  std::ofstream(destination).close();
  EXPECT_TRUE(absl::IsAlreadyExists(internal::ValidateProjectSaveAsDestination(
      destination.string().c_str())));
  const auto invalid_parent = destination / "child.yaze";
  const auto error = internal::ValidateProjectSaveAsDestination(
      invalid_parent.string().c_str());
  EXPECT_TRUE(absl::IsFailedPrecondition(error));
  EXPECT_NE(std::string(error.message()).find("Cannot inspect"),
            std::string::npos);
}

TEST(ProjectSaveDialogSeamTest, SelectionIsConsumedOnceAcrossUiThread) {
  util::ScopedSaveFileDialogResultForTesting picker("/scratch/moved.yaze");
  std::optional<std::string> selected;
  std::thread ui([&]() {
    selected = util::FileDialogWrapper::ConsumeSaveFileDialogResultForTesting();
  });
  ui.join();
  ASSERT_TRUE(selected.has_value());
  EXPECT_EQ(*selected, "/scratch/moved.yaze");
  EXPECT_TRUE(picker.consumed());
  EXPECT_FALSE(
      util::FileDialogWrapper::ConsumeSaveFileDialogResultForTesting());
}

TEST(ProjectSaveDialogSeamTest, CancelIsAnEngagedEmptyResult) {
  util::ScopedSaveFileDialogResultForTesting picker("");
  auto selected =
      util::FileDialogWrapper::ConsumeSaveFileDialogResultForTesting();
  ASSERT_TRUE(selected.has_value());
  EXPECT_TRUE(selected->empty());
  EXPECT_TRUE(picker.consumed());
}

TEST(ProjectSaveDialogSeamTest, UnusedSelectionDoesNotLeakPastScope) {
  {
    util::ScopedSaveFileDialogResultForTesting picker("/scratch/unused.yaze");
    EXPECT_FALSE(picker.consumed());
    EXPECT_THROW(util::ScopedSaveFileDialogResultForTesting nested(""),
                 std::logic_error);
  }
  EXPECT_FALSE(
      util::FileDialogWrapper::ConsumeSaveFileDialogResultForTesting());
}

TEST(ImGuiTestHarnessLogicTest,
     ScreenshotRejectsUnsupportedFormatsBeforeQueue) {
  ImGuiTestHarnessServiceImpl service(nullptr);
  ScreenshotRequest request;
  ScreenshotResponse response;
  request.set_format(ScreenshotRequest::IMAGE_FORMAT_JPEG);
  response.set_success(true);
  response.set_file_path("stale-result.bmp");
  EXPECT_EQ(service.Screenshot(&request, &response).code(),
            absl::StatusCode::kUnimplemented);
  EXPECT_FALSE(response.success());
  EXPECT_TRUE(response.file_path().empty());

  request.set_format(static_cast<ScreenshotRequest::ImageFormat>(99));
  EXPECT_EQ(service.Screenshot(&request, &response).code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(ImGuiTestHarnessLogicTest, ScreenshotRejectsMislabeledOutputBeforeQueue) {
  ImGuiTestHarnessServiceImpl service(nullptr);
  ScreenshotRequest request;
  ScreenshotResponse response;
  request.set_format(ScreenshotRequest::IMAGE_FORMAT_BMP);
  request.set_output_path("must-not-write.png");
  EXPECT_EQ(service.Screenshot(&request, &response).code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_FALSE(response.success());
  EXPECT_TRUE(response.file_path().empty());
}

TEST(ImGuiTestHarnessLogicTest, ScreenshotWithoutControllerFailsImmediately) {
  // The non-GUI test binary has no Controller. A valid request must fail
  // explicitly instead of dereferencing it or waiting for a nonexistent frame.
  ImGuiTestHarnessServiceImpl service(nullptr);
  ScreenshotRequest request;
  ScreenshotResponse response;
  request.set_format(ScreenshotRequest::IMAGE_FORMAT_BMP);
  EXPECT_EQ(service.Screenshot(&request, &response).code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_FALSE(response.success());
  EXPECT_TRUE(response.file_path().empty());
}

TEST(ImGuiTestHarnessLogicTest, ScreenshotRequiresResponse) {
  ImGuiTestHarnessServiceImpl service(nullptr);
  ScreenshotRequest request;
  EXPECT_EQ(service.Screenshot(&request, nullptr).code(),
            absl::StatusCode::kInvalidArgument);
}

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
     CiReplayKeepsTerminalErrorBeforeFirstExpectationFailureStops) {
  auto& manager = TestManager::Get();
  HarnessCallbackReset reset(&manager);
  manager.SetHarnessListener(nullptr);
  manager.SetFailureScreenshotRequester({});
  const auto root =
      std::filesystem::temp_directory_path() /
      ("yaze-replay-failure-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  struct Cleanup {
    std::filesystem::path root;
    ~Cleanup() {
      std::error_code error;
      std::filesystem::remove_all(root, error);
    }
  } cleanup{root};

  TestScript script;
  TestScriptStep step;
  step.action = "assert";
  step.condition = "value_equals:42";
  step.expect_success = true;
  step.expect_status = "passed";
  script.steps = {step, step};
  const auto script_path = (root / "first-failure.json").string();
  ASSERT_TRUE(TestScriptParser::WriteToFile(script, script_path).ok());
  ImGuiTestHarnessServiceImpl service(&manager);
  ReplayTestRequest request;
  request.set_script_path(script_path);
  request.set_ci_mode(true);
  ReplayTestResponse response;

  ASSERT_TRUE(service.ReplayTest(&request, &response).ok());

  EXPECT_FALSE(response.success());
  EXPECT_EQ(response.steps_executed(), 1);
  ASSERT_EQ(response.assertions_size(), 1);
  EXPECT_EQ(response.assertions(0).description(), "Step 1 (assert)");
  EXPECT_FALSE(response.assertions(0).passed());
  EXPECT_EQ(response.assertions(0).expected_value(), "passed");
  EXPECT_EQ(response.assertions(0).actual_value(), "failed");
  EXPECT_NE(response.message().find("Expected success=true but got false"),
            std::string::npos);
  const auto has_log = [&](const std::string& expected) {
    return std::find(response.logs().begin(), response.logs().end(),
                     expected) != response.logs().end();
  };
  EXPECT_TRUE(has_log("Step 1: assert"));
  EXPECT_TRUE(
      has_log("  Result: value_equals requires widget_key so the input "
              "is unambiguous"));
  EXPECT_TRUE(has_log("  Status: failed"));
  EXPECT_FALSE(has_log("Step 2: assert"));
  EXPECT_TRUE(std::any_of(response.logs().begin(), response.logs().end(),
                          [](const std::string& log) {
                            return log.find("  Test ID: ") == 0 &&
                                   log.size() >
                                       std::string("  Test ID: ").size();
                          }));
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
