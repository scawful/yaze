// gui_automation_client.cc
// Implementation of gRPC client for YAZE GUI automation

#include "cli/service/gui/gui_automation_client.h"

#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"

namespace yaze {
namespace cli {

namespace {

#ifdef YAZE_WITH_GRPC
std::optional<absl::Time> OptionalTimeFromMillis(int64_t millis) {
  if (millis <= 0) {
    return std::nullopt;
  }
  return absl::FromUnixMillis(millis);
}

yaze::test::WidgetType ConvertWidgetTypeFilterToProto(WidgetTypeFilter filter) {
  using ProtoType = yaze::test::WidgetType;
  switch (filter) {
    case WidgetTypeFilter::kAll:
      return ProtoType::WIDGET_TYPE_ALL;
    case WidgetTypeFilter::kButton:
      return ProtoType::WIDGET_TYPE_BUTTON;
    case WidgetTypeFilter::kInput:
      return ProtoType::WIDGET_TYPE_INPUT;
    case WidgetTypeFilter::kMenu:
      return ProtoType::WIDGET_TYPE_MENU;
    case WidgetTypeFilter::kTab:
      return ProtoType::WIDGET_TYPE_TAB;
    case WidgetTypeFilter::kCheckbox:
      return ProtoType::WIDGET_TYPE_CHECKBOX;
    case WidgetTypeFilter::kSlider:
      return ProtoType::WIDGET_TYPE_SLIDER;
    case WidgetTypeFilter::kCanvas:
      return ProtoType::WIDGET_TYPE_CANVAS;
    case WidgetTypeFilter::kSelectable:
      return ProtoType::WIDGET_TYPE_SELECTABLE;
    case WidgetTypeFilter::kOther:
      return ProtoType::WIDGET_TYPE_OTHER;
    case WidgetTypeFilter::kUnspecified:
    default:
      return ProtoType::WIDGET_TYPE_UNSPECIFIED;
  }
}

TestRunStatus ConvertStatusProto(
    yaze::test::GetTestStatusResponse::TestStatus status) {
  using ProtoStatus = yaze::test::GetTestStatusResponse::TestStatus;
  switch (status) {
    case ProtoStatus::GetTestStatusResponse_TestStatus_TEST_STATUS_QUEUED:
      return TestRunStatus::kQueued;
    case ProtoStatus::GetTestStatusResponse_TestStatus_TEST_STATUS_RUNNING:
      return TestRunStatus::kRunning;
    case ProtoStatus::GetTestStatusResponse_TestStatus_TEST_STATUS_PASSED:
      return TestRunStatus::kPassed;
    case ProtoStatus::GetTestStatusResponse_TestStatus_TEST_STATUS_FAILED:
      return TestRunStatus::kFailed;
    case ProtoStatus::GetTestStatusResponse_TestStatus_TEST_STATUS_TIMEOUT:
      return TestRunStatus::kTimeout;
    case ProtoStatus::GetTestStatusResponse_TestStatus_TEST_STATUS_UNSPECIFIED:
    default:
      return TestRunStatus::kUnknown;
  }
}
#endif  // YAZE_WITH_GRPC

}  // namespace

GuiAutomationClient::GuiAutomationClient(const std::string& server_address)
    : server_address_(server_address) {}

absl::Status GuiAutomationClient::Connect() {
#ifdef YAZE_WITH_GRPC
  auto channel =
      grpc::CreateChannel(server_address_, grpc::InsecureChannelCredentials());
  if (!channel) {
    return absl::InternalError("Failed to create gRPC channel");
  }

  stub_ = yaze::test::ImGuiTestHarness::NewStub(channel);
  if (!stub_) {
    return absl::InternalError("Failed to create gRPC stub");
  }

  // Test connection with a ping
  auto result = Ping("connection_test");
  if (!result.ok()) {
    return absl::UnavailableError(
        absl::StrFormat("Failed to connect to test harness at %s: %s",
                        server_address_, result.status().message()));
  }

  connected_ = true;
  return absl::OkStatus();
#else
  return absl::UnimplementedError(
      "GUI automation requires YAZE_WITH_GRPC=ON at build time");
#endif
}

absl::StatusOr<AutomationResult> GuiAutomationClient::Ping(
    const std::string& message) {
#ifdef YAZE_WITH_GRPC
  if (!stub_) {
    return absl::FailedPreconditionError(
        "Not connected. Call Connect() first.");
  }

  yaze::test::PingRequest request;
  request.set_message(message);

  yaze::test::PingResponse response;
  grpc::ClientContext context;

  grpc::Status status = stub_->Ping(&context, request, &response);

  if (!status.ok()) {
    return absl::InternalError(
        absl::StrFormat("Ping RPC failed: %s", status.error_message()));
  }

  AutomationResult result;
  result.success = true;
  result.message =
      absl::StrFormat("Server version: %s (timestamp: %lld)",
                      response.yaze_version(), response.timestamp_ms());
  result.execution_time = std::chrono::milliseconds(0);
  result.test_id.clear();
  return result;
#else
  return absl::UnimplementedError("gRPC not available");
#endif
}

absl::StatusOr<ReplayTestResult> GuiAutomationClient::ReplayTest(
    const std::string& script_path, bool ci_mode,
    const std::map<std::string, std::string>& parameter_overrides) {
#ifdef YAZE_WITH_GRPC
  if (!stub_) {
    return absl::FailedPreconditionError(
        "Not connected. Call Connect() first.");
  }

  yaze::test::ReplayTestRequest request;
  request.set_script_path(script_path);
  request.set_ci_mode(ci_mode);
  for (const auto& [key, value] : parameter_overrides) {
    (*request.mutable_parameter_overrides())[key] = value;
  }

  yaze::test::ReplayTestResponse response;
  grpc::ClientContext context;

  grpc::Status status = stub_->ReplayTest(&context, request, &response);
  if (!status.ok()) {
    return absl::InternalError(
        absl::StrCat("ReplayTest RPC failed: ", status.error_message()));
  }

  ReplayTestResult result;
  result.success = response.success();
  result.message = response.message();
  result.replay_session_id = response.replay_session_id();
  result.steps_executed = response.steps_executed();
  result.logs.assign(response.logs().begin(), response.logs().end());
  result.assertions.reserve(response.assertions_size());
  for (const auto& assertion_proto : response.assertions()) {
    AssertionOutcome assertion;
    assertion.description = assertion_proto.description();
    assertion.passed = assertion_proto.passed();
    assertion.expected_value = assertion_proto.expected_value();
    assertion.actual_value = assertion_proto.actual_value();
    assertion.error_message = assertion_proto.error_message();
    result.assertions.push_back(std::move(assertion));
  }

  return result;
#else
  return absl::UnimplementedError("gRPC not available");
#endif
}

absl::StatusOr<StartRecordingResult> GuiAutomationClient::StartRecording(
    const std::string& output_path, const std::string& session_name,
    const std::string& description) {
#ifdef YAZE_WITH_GRPC
  if (!stub_) {
    return absl::FailedPreconditionError(
        "Not connected. Call Connect() first.");
  }

  yaze::test::StartRecordingRequest request;
  request.set_output_path(output_path);
  request.set_session_name(session_name);
  request.set_description(description);

  yaze::test::StartRecordingResponse response;
  grpc::ClientContext context;
  grpc::Status status = stub_->StartRecording(&context, request, &response);

  if (!status.ok()) {
    return absl::InternalError(
        absl::StrCat("StartRecording RPC failed: ", status.error_message()));
  }

  StartRecordingResult result;
  result.success = response.success();
  result.message = response.message();
  result.recording_id = response.recording_id();
  result.started_at = OptionalTimeFromMillis(response.started_at_ms());
  return result;
#else
  return absl::UnimplementedError("gRPC not available");
#endif
}

absl::StatusOr<StopRecordingResult> GuiAutomationClient::StopRecording(
    const std::string& recording_id, bool discard) {
#ifdef YAZE_WITH_GRPC
  if (!stub_) {
    return absl::FailedPreconditionError(
        "Not connected. Call Connect() first.");
  }
  if (recording_id.empty()) {
    return absl::InvalidArgumentError("recording_id must not be empty");
  }

  yaze::test::StopRecordingRequest request;
  request.set_recording_id(recording_id);
  request.set_discard(discard);

  yaze::test::StopRecordingResponse response;
  grpc::ClientContext context;
  grpc::Status status = stub_->StopRecording(&context, request, &response);

  if (!status.ok()) {
    return absl::InternalError(
        absl::StrCat("StopRecording RPC failed: ", status.error_message()));
  }

  StopRecordingResult result;
  result.success = response.success();
  result.message = response.message();
  result.output_path = response.output_path();
  result.step_count = response.step_count();
  result.duration = std::chrono::milliseconds(response.duration_ms());
  return result;
#else
  return absl::UnimplementedError("gRPC not available");
#endif
}

absl::StatusOr<AutomationResult> GuiAutomationClient::Click(
    const std::string& target, ClickType type) {
#ifdef YAZE_WITH_GRPC
  if (!stub_) {
    return absl::FailedPreconditionError(
        "Not connected. Call Connect() first.");
  }

  yaze::test::ClickRequest request;
  request.set_target(target);

  switch (type) {
    case ClickType::kLeft:
      request.set_type(yaze::test::ClickRequest::CLICK_TYPE_LEFT);
      break;
    case ClickType::kRight:
      request.set_type(yaze::test::ClickRequest::CLICK_TYPE_RIGHT);
      break;
    case ClickType::kMiddle:
      request.set_type(yaze::test::ClickRequest::CLICK_TYPE_MIDDLE);
      break;
    case ClickType::kDouble:
      request.set_type(yaze::test::ClickRequest::CLICK_TYPE_DOUBLE);
      break;
  }

  yaze::test::ClickResponse response;
  grpc::ClientContext context;

  grpc::Status status = stub_->Click(&context, request, &response);

  if (!status.ok()) {
    return absl::InternalError(
        absl::StrFormat("Click RPC failed: %s", status.error_message()));
  }

  AutomationResult result;
  result.success = response.success();
  result.message = response.message();
  result.execution_time =
      std::chrono::milliseconds(response.execution_time_ms());
  result.test_id = response.test_id();
  return result;
#else
  return absl::UnimplementedError("gRPC not available");
#endif
}

absl::StatusOr<AutomationResult> GuiAutomationClient::Type(
    const std::string& target, const std::string& text, bool clear_first) {
#ifdef YAZE_WITH_GRPC
  if (!stub_) {
    return absl::FailedPreconditionError(
        "Not connected. Call Connect() first.");
  }

  yaze::test::TypeRequest request;
  request.set_target(target);
  request.set_text(text);
  request.set_clear_first(clear_first);

  yaze::test::TypeResponse response;
  grpc::ClientContext context;

  grpc::Status status = stub_->Type(&context, request, &response);

  if (!status.ok()) {
    return absl::InternalError(
        absl::StrFormat("Type RPC failed: %s", status.error_message()));
  }

  AutomationResult result;
  result.success = response.success();
  result.message = response.message();
  result.execution_time =
      std::chrono::milliseconds(response.execution_time_ms());
  result.test_id = response.test_id();
  return result;
#else
  return absl::UnimplementedError("gRPC not available");
#endif
}

absl::StatusOr<AutomationResult> GuiAutomationClient::Wait(
    const std::string& condition, int timeout_ms, int poll_interval_ms) {
#ifdef YAZE_WITH_GRPC
  if (!stub_) {
    return absl::FailedPreconditionError(
        "Not connected. Call Connect() first.");
  }

  yaze::test::WaitRequest request;
  request.set_condition(condition);
  request.set_timeout_ms(timeout_ms);
  request.set_poll_interval_ms(poll_interval_ms);

  yaze::test::WaitResponse response;
  grpc::ClientContext context;

  grpc::Status status = stub_->Wait(&context, request, &response);

  if (!status.ok()) {
    return absl::InternalError(
        absl::StrFormat("Wait RPC failed: %s", status.error_message()));
  }

  AutomationResult result;
  result.success = response.success();
  result.message = response.message();
  result.execution_time = std::chrono::milliseconds(response.elapsed_ms());
  result.test_id = response.test_id();
  return result;
#else
  return absl::UnimplementedError("gRPC not available");
#endif
}

absl::StatusOr<AutomationResult> GuiAutomationClient::Assert(
    const std::string& condition) {
#ifdef YAZE_WITH_GRPC
  if (!stub_) {
    return absl::FailedPreconditionError(
        "Not connected. Call Connect() first.");
  }

  yaze::test::AssertRequest request;
  request.set_condition(condition);

  yaze::test::AssertResponse response;
  grpc::ClientContext context;

  grpc::Status status = stub_->Assert(&context, request, &response);

  if (!status.ok()) {
    return absl::InternalError(
        absl::StrFormat("Assert RPC failed: %s", status.error_message()));
  }

  AutomationResult result;
  result.success = response.success();
  result.message = response.message();
  result.actual_value = response.actual_value();
  result.expected_value = response.expected_value();
  result.execution_time = std::chrono::milliseconds(0);
  result.test_id = response.test_id();
  return result;
#else
  return absl::UnimplementedError("gRPC not available");
#endif
}

absl::StatusOr<AutomationResult> GuiAutomationClient::Screenshot(
    const std::string& region, const std::string& format) {
#ifdef YAZE_WITH_GRPC
  if (!stub_) {
    return absl::FailedPreconditionError(
        "Not connected. Call Connect() first.");
  }

  yaze::test::ScreenshotRequest request;
  request.set_window_title("");                         // Empty = main window
  // No hardcoded path here - let server decide default unless provided
  request.set_format(
      yaze::test::ScreenshotRequest::IMAGE_FORMAT_BMP);  // Match SDL_SaveBMP

  yaze::test::ScreenshotResponse response;
  grpc::ClientContext context;

  grpc::Status status = stub_->Screenshot(&context, request, &response);

  if (!status.ok()) {
    return absl::InternalError(
        absl::StrFormat("Screenshot RPC failed: %s", status.error_message()));
  }

  AutomationResult result;
  result.success = response.success();
  result.message = response.message();
  result.execution_time = std::chrono::milliseconds(0);
  result.test_id.clear();
  return result;
#else
  return absl::UnimplementedError("gRPC not available");
#endif
}

absl::StatusOr<TestStatusDetails> GuiAutomationClient::GetTestStatus(
    const std::string& test_id) {
#ifdef YAZE_WITH_GRPC
  if (!stub_) {
    return absl::FailedPreconditionError(
        "Not connected. Call Connect() first.");
  }

  yaze::test::GetTestStatusRequest request;
  request.set_test_id(test_id);

  yaze::test::GetTestStatusResponse response;
  grpc::ClientContext context;

  grpc::Status status = stub_->GetTestStatus(&context, request, &response);

  if (!status.ok()) {
    return absl::InternalError(absl::StrFormat("GetTestStatus RPC failed: %s",
                                               status.error_message()));
  }

  TestStatusDetails details;
  details.test_id = test_id;
  details.status = ConvertStatusProto(response.status());
  details.queued_at = OptionalTimeFromMillis(response.queued_at_ms());
  details.started_at = OptionalTimeFromMillis(response.started_at_ms());
  details.completed_at = OptionalTimeFromMillis(response.completed_at_ms());
  details.execution_time_ms = response.execution_time_ms();
  details.error_message = response.error_message();
  details.assertion_failures.assign(response.assertion_failures().begin(),
                                    response.assertion_failures().end());
  return details;
#else
  return absl::UnimplementedError("gRPC not available");
#endif
}

absl::StatusOr<ListTestsResult> GuiAutomationClient::ListTests(
    const std::string& category_filter, int page_size,
    const std::string& page_token) {
#ifdef YAZE_WITH_GRPC
  if (!stub_) {
    return absl::FailedPreconditionError(
        "Not connected. Call Connect() first.");
  }

  yaze::test::ListTestsRequest request;
  if (!category_filter.empty()) {
    request.set_category_filter(category_filter);
  }
  if (page_size > 0) {
    request.set_page_size(page_size);
  }
  if (!page_token.empty()) {
    request.set_page_token(page_token);
  }

  yaze::test::ListTestsResponse response;
  grpc::ClientContext context;

  grpc::Status status = stub_->ListTests(&context, request, &response);

  if (!status.ok()) {
    return absl::InternalError(
        absl::StrFormat("ListTests RPC failed: %s", status.error_message()));
  }

  ListTestsResult result;
  result.total_count = response.total_count();
  result.next_page_token = response.next_page_token();
  result.tests.reserve(response.tests_size());

  for (const auto& test_info : response.tests()) {
    HarnessTestSummary summary;
    summary.test_id = test_info.test_id();
    summary.name = test_info.name();
    summary.category = test_info.category();
    summary.last_run_at =
        OptionalTimeFromMillis(test_info.last_run_timestamp_ms());
    summary.total_runs = test_info.total_runs();
    summary.pass_count = test_info.pass_count();
    summary.fail_count = test_info.fail_count();
    summary.average_duration_ms = test_info.average_duration_ms();
    result.tests.push_back(std::move(summary));
  }

  return result;
#else
  return absl::UnimplementedError("gRPC not available");
#endif
}

absl::StatusOr<TestResultDetails> GuiAutomationClient::GetTestResults(
    const std::string& test_id, bool include_logs) {
#ifdef YAZE_WITH_GRPC
  if (!stub_) {
    return absl::FailedPreconditionError(
        "Not connected. Call Connect() first.");
  }

  yaze::test::GetTestResultsRequest request;
  request.set_test_id(test_id);
  request.set_include_logs(include_logs);

  yaze::test::GetTestResultsResponse response;
  grpc::ClientContext context;

  grpc::Status status = stub_->GetTestResults(&context, request, &response);

  if (!status.ok()) {
    return absl::InternalError(absl::StrFormat("GetTestResults RPC failed: %s",
                                               status.error_message()));
  }

  TestResultDetails result;
  result.test_id = test_id;
  result.success = response.success();
  result.test_name = response.test_name();
  result.category = response.category();
  result.executed_at = OptionalTimeFromMillis(response.executed_at_ms());
  result.duration_ms = response.duration_ms();

  result.assertions.reserve(response.assertions_size());
  for (const auto& assertion : response.assertions()) {
    AssertionOutcome outcome;
    outcome.description = assertion.description();
    outcome.passed = assertion.passed();
    outcome.expected_value = assertion.expected_value();
    outcome.actual_value = assertion.actual_value();
    outcome.error_message = assertion.error_message();
    result.assertions.push_back(std::move(outcome));
  }

  if (include_logs) {
    result.logs.assign(response.logs().begin(), response.logs().end());
  }

  for (const auto& metric : response.metrics()) {
    result.metrics.emplace(metric.first, metric.second);
  }

  result.screenshot_path = response.screenshot_path();
  result.screenshot_size_bytes = response.screenshot_size_bytes();
  result.failure_context = response.failure_context();
  result.widget_state = response.widget_state();

  return result;
#else
  return absl::UnimplementedError("gRPC not available");
#endif
}

absl::StatusOr<DiscoverWidgetsResult> GuiAutomationClient::DiscoverWidgets(
    const DiscoverWidgetsQuery& query) {
#ifdef YAZE_WITH_GRPC
  if (!stub_) {
    return absl::FailedPreconditionError(
        "Not connected. Call Connect() first.");
  }

  yaze::test::DiscoverWidgetsRequest request;
  if (!query.window_filter.empty()) {
    request.set_window_filter(query.window_filter);
  }
  request.set_type_filter(ConvertWidgetTypeFilterToProto(query.type_filter));
  if (!query.path_prefix.empty()) {
    request.set_path_prefix(query.path_prefix);
  }
  request.set_include_invisible(query.include_invisible);
  request.set_include_disabled(query.include_disabled);

  yaze::test::DiscoverWidgetsResponse response;
  grpc::ClientContext context;
  grpc::Status status = stub_->DiscoverWidgets(&context, request, &response);

  if (!status.ok()) {
    return absl::InternalError(absl::StrFormat("DiscoverWidgets RPC failed: %s",
                                               status.error_message()));
  }

  DiscoverWidgetsResult result;
  result.total_widgets = response.total_widgets();
  if (response.generated_at_ms() > 0) {
    result.generated_at = OptionalTimeFromMillis(response.generated_at_ms());
  }

  result.windows.reserve(response.windows_size());
  for (const auto& window_proto : response.windows()) {
    DiscoveredWindowInfo window_info;
    window_info.name = window_proto.name();
    window_info.visible = window_proto.visible();
    window_info.widgets.reserve(window_proto.widgets_size());

    for (const auto& widget_proto : window_proto.widgets()) {
      WidgetDescriptor widget;
      widget.path = widget_proto.path();
      widget.label = widget_proto.label();
      widget.type = widget_proto.type();
      widget.description = widget_proto.description();
      widget.suggested_action = widget_proto.suggested_action();
      widget.visible = widget_proto.visible();
      widget.enabled = widget_proto.enabled();
      widget.has_bounds = widget_proto.has_bounds();
      if (widget.has_bounds) {
        widget.bounds.min_x = widget_proto.bounds().min_x();
        widget.bounds.min_y = widget_proto.bounds().min_y();
        widget.bounds.max_x = widget_proto.bounds().max_x();
        widget.bounds.max_y = widget_proto.bounds().max_y();
      } else {
        widget.bounds = WidgetBoundingBox();
      }
      widget.widget_id = widget_proto.widget_id();
      widget.last_seen_frame = widget_proto.last_seen_frame();
      widget.last_seen_at =
          OptionalTimeFromMillis(widget_proto.last_seen_at_ms());
      widget.stale = widget_proto.stale();
      window_info.widgets.push_back(std::move(widget));
    }

    result.windows.push_back(std::move(window_info));
  }

  return result;
#else
  (void)query;
  return absl::UnimplementedError("gRPC not available");
#endif
}

}  // namespace cli
}  // namespace yaze
