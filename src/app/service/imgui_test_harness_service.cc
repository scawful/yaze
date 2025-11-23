#include "app/service/imgui_test_harness_service.h"

#ifdef YAZE_WITH_GRPC

#include "app/platform/sdl_compat.h"

// Undefine Windows macros that conflict with protobuf generated code
// SDL.h includes Windows.h on Windows, which defines these macros
#ifdef _WIN32
#ifdef DWORD
#undef DWORD
#endif
#ifdef ERROR
#undef ERROR
#endif
#ifdef OVERFLOW
#undef OVERFLOW
#endif
#ifdef IGNORE
#undef IGNORE
#endif
#endif  // _WIN32

#include <algorithm>
#include <chrono>
#include <deque>
#include <fstream>
#include <iostream>
#include <limits>
#include <thread>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/service/screenshot_utils.h"
#include "app/test/test_manager.h"
#include "app/test/test_script_parser.h"
#include "protos/imgui_test_harness.grpc.pb.h"
#include "protos/imgui_test_harness.pb.h"
#include "yaze.h"  // For YAZE_VERSION_STRING

#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"

// Helper to register and run a test dynamically
namespace {
struct DynamicTestData {
  std::function<void(ImGuiTestContext*)> test_func;
};

absl::Mutex g_dynamic_tests_mutex;
std::deque<std::shared_ptr<DynamicTestData>> g_dynamic_tests
    ABSL_GUARDED_BY(g_dynamic_tests_mutex);

void KeepDynamicTestData(const std::shared_ptr<DynamicTestData>& data) {
  absl::MutexLock lock(&g_dynamic_tests_mutex);
  constexpr size_t kMaxKeepAlive = 64;
  g_dynamic_tests.push_back(data);
  while (g_dynamic_tests.size() > kMaxKeepAlive) {
    g_dynamic_tests.pop_front();
  }
}

void RunDynamicTest(ImGuiTestContext* ctx) {
  auto* data = (DynamicTestData*)ctx->Test->UserData;
  if (data && data->test_func) {
    data->test_func(ctx);
  }
}

// Helper to check if a test has completed (not queued or running)
bool IsTestCompleted(ImGuiTest* test) {
  return test->Output.Status != ImGuiTestStatus_Queued &&
         test->Output.Status != ImGuiTestStatus_Running;
}

// Thread-safe state for RPC communication
template <typename T>
struct RPCState {
  std::atomic<bool> completed{false};
  std::mutex data_mutex;
  T result;
  std::string message;

  void SetResult(const T& res, const std::string& msg) {
    std::lock_guard<std::mutex> lock(data_mutex);
    result = res;
    message = msg;
    completed.store(true);
  }

  void GetResult(T& res, std::string& msg) {
    std::lock_guard<std::mutex> lock(data_mutex);
    res = result;
    msg = message;
  }
};

}  // namespace
#endif

namespace {

::yaze::test::GetTestStatusResponse_TestStatus ConvertHarnessStatus(
    ::yaze::test::HarnessTestStatus status) {
  switch (status) {
    case ::yaze::test::HarnessTestStatus::kQueued:
      return ::yaze::test::GetTestStatusResponse::TEST_STATUS_QUEUED;
    case ::yaze::test::HarnessTestStatus::kRunning:
      return ::yaze::test::GetTestStatusResponse::TEST_STATUS_RUNNING;
    case ::yaze::test::HarnessTestStatus::kPassed:
      return ::yaze::test::GetTestStatusResponse::TEST_STATUS_PASSED;
    case ::yaze::test::HarnessTestStatus::kFailed:
      return ::yaze::test::GetTestStatusResponse::TEST_STATUS_FAILED;
    case ::yaze::test::HarnessTestStatus::kTimeout:
      return ::yaze::test::GetTestStatusResponse::TEST_STATUS_TIMEOUT;
    case ::yaze::test::HarnessTestStatus::kUnspecified:
    default:
      return ::yaze::test::GetTestStatusResponse::TEST_STATUS_UNSPECIFIED;
  }
}

int64_t ToUnixMillisSafe(absl::Time timestamp) {
  if (timestamp == absl::InfinitePast()) {
    return 0;
  }
  return absl::ToUnixMillis(timestamp);
}

int32_t ClampDurationToInt32(absl::Duration duration) {
  int64_t millis = absl::ToInt64Milliseconds(duration);
  if (millis > std::numeric_limits<int32_t>::max()) {
    return std::numeric_limits<int32_t>::max();
  }
  if (millis < std::numeric_limits<int32_t>::min()) {
    return std::numeric_limits<int32_t>::min();
  }
  return static_cast<int32_t>(millis);
}

}  // namespace

#include <grpcpp/grpcpp.h>
#include <grpcpp/server_builder.h>

namespace yaze {
namespace test {

namespace {

std::string ClickTypeToString(ClickRequest::ClickType type) {
  switch (type) {
    case ClickRequest::CLICK_TYPE_RIGHT:
      return "right";
    case ClickRequest::CLICK_TYPE_MIDDLE:
      return "middle";
    case ClickRequest::CLICK_TYPE_DOUBLE:
      return "double";
    case ClickRequest::CLICK_TYPE_LEFT:
    case ClickRequest::CLICK_TYPE_UNSPECIFIED:
    default:
      return "left";
  }
}

ClickRequest::ClickType ClickTypeFromString(absl::string_view type) {
  const std::string lower = absl::AsciiStrToLower(std::string(type));
  if (lower == "right") {
    return ClickRequest::CLICK_TYPE_RIGHT;
  }
  if (lower == "middle") {
    return ClickRequest::CLICK_TYPE_MIDDLE;
  }
  if (lower == "double" || lower == "double_click" || lower == "dbl") {
    return ClickRequest::CLICK_TYPE_DOUBLE;
  }
  return ClickRequest::CLICK_TYPE_LEFT;
}

HarnessTestStatus HarnessStatusFromString(absl::string_view status) {
  const std::string lower = absl::AsciiStrToLower(std::string(status));
  if (lower == "passed" || lower == "success") {
    return HarnessTestStatus::kPassed;
  }
  if (lower == "failed" || lower == "fail") {
    return HarnessTestStatus::kFailed;
  }
  if (lower == "timeout") {
    return HarnessTestStatus::kTimeout;
  }
  if (lower == "queued") {
    return HarnessTestStatus::kQueued;
  }
  if (lower == "running") {
    return HarnessTestStatus::kRunning;
  }
  return HarnessTestStatus::kUnspecified;
}

const char* HarnessStatusToString(HarnessTestStatus status) {
  switch (status) {
    case HarnessTestStatus::kPassed:
      return "passed";
    case HarnessTestStatus::kFailed:
      return "failed";
    case HarnessTestStatus::kTimeout:
      return "timeout";
    case HarnessTestStatus::kQueued:
      return "queued";
    case HarnessTestStatus::kRunning:
      return "running";
    case HarnessTestStatus::kUnspecified:
    default:
      return "unknown";
  }
}

std::string ApplyOverrides(
    const std::string& value,
    const absl::flat_hash_map<std::string, std::string>& overrides) {
  if (overrides.empty() || value.empty()) {
    return value;
  }
  std::string result = value;
  for (const auto& [key, replacement] : overrides) {
    const std::string placeholder = absl::StrCat("{{", key, "}}");
    result = absl::StrReplaceAll(result, {{placeholder, replacement}});
  }
  return result;
}

void MaybeRecordStep(TestRecorder* recorder, TestRecorder::RecordedStep step) {
  if (!recorder || !recorder->IsRecording()) {
    return;
  }
  if (step.captured_at == absl::InfinitePast()) {
    step.captured_at = absl::Now();
  }
  recorder->RecordStep(step);
}

absl::Status WaitForHarnessTestCompletion(TestManager* manager,
                                          const std::string& test_id,
                                          HarnessTestExecution* execution) {
  if (!manager) {
    return absl::FailedPreconditionError("TestManager unavailable");
  }
  if (test_id.empty()) {
    return absl::InvalidArgumentError("Missing harness test identifier");
  }

  const absl::Time deadline = absl::Now() + absl::Seconds(20);
  while (absl::Now() < deadline) {
    absl::StatusOr<HarnessTestExecution> current =
        manager->GetHarnessTestExecution(test_id);
    if (!current.ok()) {
      absl::SleepFor(absl::Milliseconds(75));
      continue;
    }

    if (execution) {
      *execution = std::move(current.value());
    }

    if (current->status == HarnessTestStatus::kQueued ||
        current->status == HarnessTestStatus::kRunning) {
      absl::SleepFor(absl::Milliseconds(75));
      continue;
    }
    return absl::OkStatus();
  }

  return absl::DeadlineExceededError(absl::StrFormat(
      "Harness test %s did not reach a terminal state", test_id));
}

}  // namespace

// gRPC service wrapper that forwards to our implementation
class ImGuiTestHarnessServiceGrpc final : public ImGuiTestHarness::Service {
 public:
  explicit ImGuiTestHarnessServiceGrpc(ImGuiTestHarnessServiceImpl* impl)
      : impl_(impl) {}

  grpc::Status Ping(grpc::ServerContext* context, const PingRequest* request,
                    PingResponse* response) override {
    return ConvertStatus(impl_->Ping(request, response));
  }

  grpc::Status Click(grpc::ServerContext* context, const ClickRequest* request,
                     ClickResponse* response) override {
    return ConvertStatus(impl_->Click(request, response));
  }

  grpc::Status Type(grpc::ServerContext* context, const TypeRequest* request,
                    TypeResponse* response) override {
    return ConvertStatus(impl_->Type(request, response));
  }

  grpc::Status Wait(grpc::ServerContext* context, const WaitRequest* request,
                    WaitResponse* response) override {
    return ConvertStatus(impl_->Wait(request, response));
  }

  grpc::Status Assert(grpc::ServerContext* context,
                      const AssertRequest* request,
                      AssertResponse* response) override {
    return ConvertStatus(impl_->Assert(request, response));
  }

  grpc::Status Screenshot(grpc::ServerContext* context,
                          const ScreenshotRequest* request,
                          ScreenshotResponse* response) override {
    return ConvertStatus(impl_->Screenshot(request, response));
  }

  grpc::Status GetTestStatus(grpc::ServerContext* context,
                             const GetTestStatusRequest* request,
                             GetTestStatusResponse* response) override {
    return ConvertStatus(impl_->GetTestStatus(request, response));
  }

  grpc::Status ListTests(grpc::ServerContext* context,
                         const ListTestsRequest* request,
                         ListTestsResponse* response) override {
    return ConvertStatus(impl_->ListTests(request, response));
  }

  grpc::Status GetTestResults(grpc::ServerContext* context,
                              const GetTestResultsRequest* request,
                              GetTestResultsResponse* response) override {
    return ConvertStatus(impl_->GetTestResults(request, response));
  }

  grpc::Status DiscoverWidgets(grpc::ServerContext* context,
                               const DiscoverWidgetsRequest* request,
                               DiscoverWidgetsResponse* response) override {
    return ConvertStatus(impl_->DiscoverWidgets(request, response));
  }

  grpc::Status StartRecording(grpc::ServerContext* context,
                              const StartRecordingRequest* request,
                              StartRecordingResponse* response) override {
    return ConvertStatus(impl_->StartRecording(request, response));
  }

  grpc::Status StopRecording(grpc::ServerContext* context,
                             const StopRecordingRequest* request,
                             StopRecordingResponse* response) override {
    return ConvertStatus(impl_->StopRecording(request, response));
  }

  grpc::Status ReplayTest(grpc::ServerContext* context,
                          const ReplayTestRequest* request,
                          ReplayTestResponse* response) override {
    return ConvertStatus(impl_->ReplayTest(request, response));
  }

 private:
  static grpc::Status ConvertStatus(const absl::Status& status) {
    if (status.ok()) {
      return grpc::Status::OK;
    }

    grpc::StatusCode code = grpc::StatusCode::UNKNOWN;
    switch (status.code()) {
      case absl::StatusCode::kCancelled:
        code = grpc::StatusCode::CANCELLED;
        break;
      case absl::StatusCode::kUnknown:
        code = grpc::StatusCode::UNKNOWN;
        break;
      case absl::StatusCode::kInvalidArgument:
        code = grpc::StatusCode::INVALID_ARGUMENT;
        break;
      case absl::StatusCode::kDeadlineExceeded:
        code = grpc::StatusCode::DEADLINE_EXCEEDED;
        break;
      case absl::StatusCode::kNotFound:
        code = grpc::StatusCode::NOT_FOUND;
        break;
      case absl::StatusCode::kAlreadyExists:
        code = grpc::StatusCode::ALREADY_EXISTS;
        break;
      case absl::StatusCode::kPermissionDenied:
        code = grpc::StatusCode::PERMISSION_DENIED;
        break;
      case absl::StatusCode::kResourceExhausted:
        code = grpc::StatusCode::RESOURCE_EXHAUSTED;
        break;
      case absl::StatusCode::kFailedPrecondition:
        code = grpc::StatusCode::FAILED_PRECONDITION;
        break;
      case absl::StatusCode::kAborted:
        code = grpc::StatusCode::ABORTED;
        break;
      case absl::StatusCode::kOutOfRange:
        code = grpc::StatusCode::OUT_OF_RANGE;
        break;
      case absl::StatusCode::kUnimplemented:
        code = grpc::StatusCode::UNIMPLEMENTED;
        break;
      case absl::StatusCode::kInternal:
        code = grpc::StatusCode::INTERNAL;
        break;
      case absl::StatusCode::kUnavailable:
        code = grpc::StatusCode::UNAVAILABLE;
        break;
      case absl::StatusCode::kDataLoss:
        code = grpc::StatusCode::DATA_LOSS;
        break;
      case absl::StatusCode::kUnauthenticated:
        code = grpc::StatusCode::UNAUTHENTICATED;
        break;
      default:
        code = grpc::StatusCode::UNKNOWN;
        break;
    }

    return grpc::Status(code, std::string(status.message().data(), status.message().size()));
  }

  ImGuiTestHarnessServiceImpl* impl_;
};

// ============================================================================
// ImGuiTestHarnessServiceImpl - RPC Handlers
// ============================================================================

absl::Status ImGuiTestHarnessServiceImpl::Ping(const PingRequest* request,
                                               PingResponse* response) {
  // Echo back the message with "Pong: " prefix
  response->set_message(absl::StrFormat("Pong: %s", request->message()));

  // Add current timestamp
  auto now = std::chrono::system_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      now.time_since_epoch());
  response->set_timestamp_ms(ms.count());

  // Add YAZE version
  response->set_yaze_version(YAZE_VERSION_STRING);

  return absl::OkStatus();
}

absl::Status ImGuiTestHarnessServiceImpl::Click(const ClickRequest* request,
                                                ClickResponse* response) {
  auto start = std::chrono::steady_clock::now();

  TestRecorder::RecordedStep recorded_step;
  recorded_step.type = TestRecorder::ActionType::kClick;
  if (request) {
    recorded_step.target = request->target();
    recorded_step.click_type = ClickTypeToString(request->type());
  }

  auto finalize = [&](const absl::Status& status) {
    recorded_step.success = response->success();
    recorded_step.message = response->message();
    recorded_step.execution_time_ms = response->execution_time_ms();
    recorded_step.test_id = response->test_id();
    MaybeRecordStep(&test_recorder_, recorded_step);
    return status;
  };

  if (!test_manager_) {
    response->set_success(false);
    response->set_message("TestManager not available");
    response->set_execution_time_ms(0);
    return finalize(absl::FailedPreconditionError("TestManager not available"));
  }

  const std::string test_id = test_manager_->RegisterHarnessTest(
      absl::StrFormat("Click: %s", request->target()), "grpc");
  response->set_test_id(test_id);
  recorded_step.test_id = test_id;
  test_manager_->AppendHarnessTestLog(
      test_id, absl::StrCat("Queued click request: ", request->target()));

#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE
  ImGuiTestEngine* engine = test_manager_->GetUITestEngine();
  if (!engine) {
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    std::string message = "ImGuiTestEngine not initialized";
    response->set_success(false);
    response->set_message(message);
    response->set_execution_time_ms(elapsed.count());
    test_manager_->MarkHarnessTestCompleted(test_id, HarnessTestStatus::kFailed,
                                            message);
    return finalize(absl::OkStatus());
  }

  std::string target = request->target();
  size_t colon_pos = target.find(':');
  if (colon_pos == std::string::npos) {
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    std::string message =
        "Invalid target format. Use 'type:label' (e.g. 'button:Open ROM')";
    response->set_success(false);
    response->set_message(message);
    response->set_execution_time_ms(elapsed.count());
    test_manager_->MarkHarnessTestCompleted(test_id, HarnessTestStatus::kFailed,
                                            message);
    test_manager_->AppendHarnessTestLog(test_id, message);
    return finalize(absl::OkStatus());
  }

  std::string widget_type = target.substr(0, colon_pos);
  std::string widget_label = target.substr(colon_pos + 1);

  ImGuiMouseButton mouse_button = ImGuiMouseButton_Left;
  switch (request->type()) {
    case ClickRequest::CLICK_TYPE_UNSPECIFIED:
    case ClickRequest::CLICK_TYPE_LEFT:
      mouse_button = ImGuiMouseButton_Left;
      break;
    case ClickRequest::CLICK_TYPE_RIGHT:
      mouse_button = ImGuiMouseButton_Right;
      break;
    case ClickRequest::CLICK_TYPE_MIDDLE:
      mouse_button = ImGuiMouseButton_Middle;
      break;
    case ClickRequest::CLICK_TYPE_DOUBLE:
      // handled below
      break;
  }

  auto test_data = std::make_shared<DynamicTestData>();
  TestManager* manager = test_manager_;
  test_data->test_func = [manager, captured_id = test_id, widget_type,
                          widget_label, click_type = request->type(),
                          mouse_button](ImGuiTestContext* ctx) {
    manager->MarkHarnessTestRunning(captured_id);
    try {
      if (click_type == ClickRequest::CLICK_TYPE_DOUBLE) {
        ctx->ItemDoubleClick(widget_label.c_str());
      } else {
        ctx->ItemClick(widget_label.c_str(), mouse_button);
      }
      ctx->Yield();
      const std::string success_message =
          absl::StrFormat("Clicked %s '%s'", widget_type, widget_label);
      manager->AppendHarnessTestLog(captured_id, success_message);
      manager->MarkHarnessTestCompleted(captured_id, HarnessTestStatus::kPassed,
                                        success_message);
    } catch (const std::exception& e) {
      const std::string error_message =
          absl::StrFormat("Click failed: %s", e.what());
      manager->AppendHarnessTestLog(captured_id, error_message);
      manager->MarkHarnessTestCompleted(captured_id, HarnessTestStatus::kFailed,
                                        error_message);
    }
  };

  std::string test_name = absl::StrFormat(
      "grpc_click_%lld",
      static_cast<long long>(
          std::chrono::system_clock::now().time_since_epoch().count()));

  ImGuiTest* test = IM_REGISTER_TEST(engine, "grpc", test_name.c_str());
  test->TestFunc = RunDynamicTest;
  test->UserData = test_data.get();

  ImGuiTestEngine_QueueTest(engine, test, ImGuiTestRunFlags_RunFromGui);
  KeepDynamicTestData(test_data);

  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);
  std::string message =
      absl::StrFormat("Queued click on %s '%s'", widget_type, widget_label);
  response->set_success(true);
  response->set_message(message);
  response->set_execution_time_ms(elapsed.count());
  test_manager_->AppendHarnessTestLog(test_id, message);

#else
  std::string target = request->target();
  size_t colon_pos = target.find(':');
  if (colon_pos == std::string::npos) {
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    std::string message = "Invalid target format. Use 'type:label'";
    response->set_success(false);
    response->set_message(message);
    response->set_execution_time_ms(elapsed.count());
    test_manager_->MarkHarnessTestCompleted(test_id, HarnessTestStatus::kFailed,
                                            message);
    test_manager_->AppendHarnessTestLog(test_id, message);
    return finalize(absl::OkStatus());
  }

  std::string widget_type = target.substr(0, colon_pos);
  std::string widget_label = target.substr(colon_pos + 1);
  std::string message =
      absl::StrFormat("[STUB] Clicked %s '%s' (ImGuiTestEngine not available)",
                      widget_type, widget_label);

  test_manager_->MarkHarnessTestRunning(test_id);
  test_manager_->MarkHarnessTestCompleted(test_id, HarnessTestStatus::kPassed,
                                          message);
  test_manager_->AppendHarnessTestLog(test_id, message);

  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);
  response->set_success(true);
  response->set_message(message);
  response->set_execution_time_ms(elapsed.count());
#endif

  return finalize(absl::OkStatus());
}

absl::Status ImGuiTestHarnessServiceImpl::Type(const TypeRequest* request,
                                               TypeResponse* response) {
  auto start = std::chrono::steady_clock::now();

  TestRecorder::RecordedStep recorded_step;
  recorded_step.type = TestRecorder::ActionType::kType;
  if (request) {
    recorded_step.target = request->target();
    recorded_step.text = request->text();
    recorded_step.clear_first = request->clear_first();
  }

  auto finalize = [&](const absl::Status& status) {
    recorded_step.success = response->success();
    recorded_step.message = response->message();
    recorded_step.execution_time_ms = response->execution_time_ms();
    recorded_step.test_id = response->test_id();
    MaybeRecordStep(&test_recorder_, recorded_step);
    return status;
  };

  if (!test_manager_) {
    response->set_success(false);
    response->set_message("TestManager not available");
    response->set_execution_time_ms(0);
    return finalize(absl::FailedPreconditionError("TestManager not available"));
  }

  const std::string test_id = test_manager_->RegisterHarnessTest(
      absl::StrFormat("Type: %s", request->target()), "grpc");
  response->set_test_id(test_id);
  recorded_step.test_id = test_id;
  test_manager_->AppendHarnessTestLog(
      test_id, absl::StrFormat("Queued type request: %s", request->target()));

#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE
  ImGuiTestEngine* engine = test_manager_->GetUITestEngine();
  if (!engine) {
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    std::string message = "ImGuiTestEngine not initialized";
    response->set_success(false);
    response->set_message(message);
    response->set_execution_time_ms(elapsed.count());
    test_manager_->MarkHarnessTestCompleted(test_id, HarnessTestStatus::kFailed,
                                            message);
    return finalize(absl::OkStatus());
  }

  std::string target = request->target();
  size_t colon_pos = target.find(':');
  if (colon_pos == std::string::npos) {
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    std::string message =
        "Invalid target format. Use 'type:label' (e.g. 'input:Filename')";
    response->set_success(false);
    response->set_message(message);
    response->set_execution_time_ms(elapsed.count());
    test_manager_->MarkHarnessTestCompleted(test_id, HarnessTestStatus::kFailed,
                                            message);
    test_manager_->AppendHarnessTestLog(test_id, message);
    return finalize(absl::OkStatus());
  }

  std::string widget_type = target.substr(0, colon_pos);
  std::string widget_label = target.substr(colon_pos + 1);
  std::string text = request->text();
  bool clear_first = request->clear_first();

  auto rpc_state = std::make_shared<RPCState<bool>>();
  auto test_data = std::make_shared<DynamicTestData>();
  TestManager* manager = test_manager_;
  test_data->test_func = [manager, captured_id = test_id, widget_type,
                          widget_label, clear_first, text,
                          rpc_state](ImGuiTestContext* ctx) {
    manager->MarkHarnessTestRunning(captured_id);
    try {
      ImGuiTestItemInfo item = ctx->ItemInfo(widget_label.c_str());
      if (item.ID == 0) {
        std::string error_message =
            absl::StrFormat("Input field '%s' not found", widget_label);
        manager->AppendHarnessTestLog(captured_id, error_message);
        manager->MarkHarnessTestCompleted(
            captured_id, HarnessTestStatus::kFailed, error_message);
        rpc_state->SetResult(false, error_message);
        return;
      }

      ctx->ItemClick(widget_label.c_str());
      if (clear_first) {
        ctx->KeyPress(ImGuiMod_Shortcut | ImGuiKey_A);
        ctx->KeyPress(ImGuiKey_Delete);
      }

      ctx->ItemInputValue(widget_label.c_str(), text.c_str());

      std::string success_message =
          absl::StrFormat("Typed '%s' into %s '%s'%s", text, widget_type,
                          widget_label, clear_first ? " (cleared first)" : "");
      manager->AppendHarnessTestLog(captured_id, success_message);
      manager->MarkHarnessTestCompleted(captured_id, HarnessTestStatus::kPassed,
                                        success_message);
      rpc_state->SetResult(true, success_message);
    } catch (const std::exception& e) {
      std::string error_message = absl::StrFormat("Type failed: %s", e.what());
      manager->AppendHarnessTestLog(captured_id, error_message);
      manager->MarkHarnessTestCompleted(captured_id, HarnessTestStatus::kFailed,
                                        error_message);
      rpc_state->SetResult(false, error_message);
    }
  };

  std::string test_name = absl::StrFormat(
      "grpc_type_%lld",
      static_cast<long long>(
          std::chrono::system_clock::now().time_since_epoch().count()));

  ImGuiTest* test = IM_REGISTER_TEST(engine, "grpc", test_name.c_str());
  test->TestFunc = RunDynamicTest;
  test->UserData = test_data.get();

  ImGuiTestEngine_QueueTest(engine, test, ImGuiTestRunFlags_RunFromGui);
  KeepDynamicTestData(test_data);

  auto timeout = std::chrono::seconds(5);
  auto wait_start = std::chrono::steady_clock::now();
  while (!rpc_state->completed.load()) {
    if (std::chrono::steady_clock::now() - wait_start > timeout) {
      std::string error_message =
          "Test timeout - input field not found or unresponsive";
      manager->AppendHarnessTestLog(test_id, error_message);
      manager->MarkHarnessTestCompleted(test_id, HarnessTestStatus::kTimeout,
                                        error_message);
      rpc_state->SetResult(false, error_message);
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  bool success = false;
  std::string message;
  rpc_state->GetResult(success, message);
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);

  response->set_success(success);
  response->set_message(message);
  response->set_execution_time_ms(elapsed.count());
  if (!message.empty()) {
    test_manager_->AppendHarnessTestLog(test_id, message);
  }

#else
  test_manager_->MarkHarnessTestRunning(test_id);
  std::string message = absl::StrFormat(
      "[STUB] Typed '%s' into %s (ImGuiTestEngine not available)",
      request->text(), request->target());
  test_manager_->MarkHarnessTestCompleted(test_id, HarnessTestStatus::kPassed,
                                          message);
  test_manager_->AppendHarnessTestLog(test_id, message);

  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);
  response->set_success(true);
  response->set_message(message);
  response->set_execution_time_ms(elapsed.count());
#endif

  return finalize(absl::OkStatus());
}

absl::Status ImGuiTestHarnessServiceImpl::Wait(const WaitRequest* request,
                                               WaitResponse* response) {
  auto start = std::chrono::steady_clock::now();

  TestRecorder::RecordedStep recorded_step;
  recorded_step.type = TestRecorder::ActionType::kWait;
  if (request) {
    recorded_step.condition = request->condition();
    recorded_step.timeout_ms = request->timeout_ms();
  }

  auto finalize = [&](const absl::Status& status) {
    recorded_step.success = response->success();
    recorded_step.message = response->message();
    recorded_step.execution_time_ms = response->elapsed_ms();
    recorded_step.test_id = response->test_id();
    MaybeRecordStep(&test_recorder_, recorded_step);
    return status;
  };

  if (!test_manager_) {
    response->set_success(false);
    response->set_message("TestManager not available");
    response->set_elapsed_ms(0);
    return finalize(absl::FailedPreconditionError("TestManager not available"));
  }

  const std::string test_id = test_manager_->RegisterHarnessTest(
      absl::StrFormat("Wait: %s", request->condition()), "grpc");
  response->set_test_id(test_id);
  recorded_step.test_id = test_id;
  test_manager_->AppendHarnessTestLog(
      test_id,
      absl::StrFormat("Queued wait condition: %s", request->condition()));

#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE
  ImGuiTestEngine* engine = test_manager_->GetUITestEngine();
  if (!engine) {
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    std::string message = "ImGuiTestEngine not initialized";
    response->set_success(false);
    response->set_message(message);
    response->set_elapsed_ms(elapsed.count());
    test_manager_->MarkHarnessTestCompleted(test_id, HarnessTestStatus::kFailed,
                                            message);
    return finalize(absl::OkStatus());
  }

  std::string condition = request->condition();
  size_t colon_pos = condition.find(':');
  if (colon_pos == std::string::npos) {
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    std::string message =
        "Invalid condition format. Use 'type:target' (e.g. "
        "'window_visible:Overworld Editor')";
    response->set_success(false);
    response->set_message(message);
    response->set_elapsed_ms(elapsed.count());
    test_manager_->MarkHarnessTestCompleted(test_id, HarnessTestStatus::kFailed,
                                            message);
    test_manager_->AppendHarnessTestLog(test_id, message);
    return finalize(absl::OkStatus());
  }

  std::string condition_type = condition.substr(0, colon_pos);
  std::string condition_target = condition.substr(colon_pos + 1);
  int timeout_ms = request->timeout_ms() > 0 ? request->timeout_ms() : 5000;
  int poll_interval_ms =
      request->poll_interval_ms() > 0 ? request->poll_interval_ms() : 100;

  auto test_data = std::make_shared<DynamicTestData>();
  TestManager* manager = test_manager_;
  test_data->test_func = [manager, captured_id = test_id, condition_type,
                          condition_target, timeout_ms,
                          poll_interval_ms](ImGuiTestContext* ctx) {
    manager->MarkHarnessTestRunning(captured_id);
    auto poll_start = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(timeout_ms);

    for (int i = 0; i < 10; ++i) {
      ctx->Yield();
    }

    try {
      while (std::chrono::steady_clock::now() - poll_start < timeout) {
        bool current_state = false;

        if (condition_type == "window_visible") {
          ImGuiTestItemInfo window_info = ctx->WindowInfo(
              condition_target.c_str(), ImGuiTestOpFlags_NoError);
          current_state = (window_info.ID != 0);
        } else if (condition_type == "element_visible") {
          ImGuiTestItemInfo item =
              ctx->ItemInfo(condition_target.c_str(), ImGuiTestOpFlags_NoError);
          current_state = (item.ID != 0 && item.RectClipped.GetWidth() > 0 &&
                           item.RectClipped.GetHeight() > 0);
        } else if (condition_type == "element_enabled") {
          ImGuiTestItemInfo item =
              ctx->ItemInfo(condition_target.c_str(), ImGuiTestOpFlags_NoError);
          current_state =
              (item.ID != 0 && !(item.ItemFlags & ImGuiItemFlags_Disabled));
        } else {
          std::string error_message =
              absl::StrFormat("Unknown condition type: %s", condition_type);
          manager->AppendHarnessTestLog(captured_id, error_message);
          manager->MarkHarnessTestCompleted(
              captured_id, HarnessTestStatus::kFailed, error_message);
          return;
        }

        if (current_state) {
          auto elapsed_ms =
              std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - poll_start);
          std::string success_message = absl::StrFormat(
              "Condition '%s:%s' met after %lld ms", condition_type,
              condition_target, static_cast<long long>(elapsed_ms.count()));
          manager->AppendHarnessTestLog(captured_id, success_message);
          manager->MarkHarnessTestCompleted(
              captured_id, HarnessTestStatus::kPassed, success_message);
          return;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(poll_interval_ms));
        ctx->Yield();
      }

      std::string timeout_message =
          absl::StrFormat("Condition '%s:%s' not met after %d ms timeout",
                          condition_type, condition_target, timeout_ms);
      manager->AppendHarnessTestLog(captured_id, timeout_message);
      manager->MarkHarnessTestCompleted(
          captured_id, HarnessTestStatus::kTimeout, timeout_message);
    } catch (const std::exception& e) {
      std::string error_message = absl::StrFormat("Wait failed: %s", e.what());
      manager->AppendHarnessTestLog(captured_id, error_message);
      manager->MarkHarnessTestCompleted(captured_id, HarnessTestStatus::kFailed,
                                        error_message);
    }
  };

  std::string test_name = absl::StrFormat(
      "grpc_wait_%lld",
      static_cast<long long>(
          std::chrono::system_clock::now().time_since_epoch().count()));

  ImGuiTest* test = IM_REGISTER_TEST(engine, "grpc", test_name.c_str());
  test->TestFunc = RunDynamicTest;
  test->UserData = test_data.get();

  ImGuiTestEngine_QueueTest(engine, test, ImGuiTestRunFlags_RunFromGui);

  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);
  std::string message = absl::StrFormat("Queued wait for '%s:%s'",
                                        condition_type, condition_target);
  response->set_success(true);
  response->set_message(message);
  response->set_elapsed_ms(elapsed.count());
  test_manager_->AppendHarnessTestLog(test_id, message);

#else
  test_manager_->MarkHarnessTestRunning(test_id);
  std::string message = absl::StrFormat(
      "[STUB] Condition '%s' met (ImGuiTestEngine not available)",
      request->condition());
  test_manager_->MarkHarnessTestCompleted(test_id, HarnessTestStatus::kPassed,
                                          message);
  test_manager_->AppendHarnessTestLog(test_id, message);

  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);
  response->set_success(true);
  response->set_message(message);
  response->set_elapsed_ms(elapsed.count());
#endif

  return finalize(absl::OkStatus());
}

absl::Status ImGuiTestHarnessServiceImpl::Assert(const AssertRequest* request,
                                                 AssertResponse* response) {
  TestRecorder::RecordedStep recorded_step;
  recorded_step.type = TestRecorder::ActionType::kAssert;
  if (request) {
    recorded_step.condition = request->condition();
  }

  auto finalize = [&](const absl::Status& status) {
    recorded_step.success = response->success();
    recorded_step.message = response->message();
    recorded_step.expected_value = response->expected_value();
    recorded_step.actual_value = response->actual_value();
    recorded_step.test_id = response->test_id();
    MaybeRecordStep(&test_recorder_, recorded_step);
    return status;
  };

  if (!test_manager_) {
    response->set_success(false);
    response->set_message("TestManager not available");
    response->set_actual_value("N/A");
    response->set_expected_value("N/A");
    return finalize(absl::FailedPreconditionError("TestManager not available"));
  }

  const std::string test_id = test_manager_->RegisterHarnessTest(
      absl::StrFormat("Assert: %s", request->condition()), "grpc");
  response->set_test_id(test_id);
  recorded_step.test_id = test_id;
  test_manager_->AppendHarnessTestLog(
      test_id, absl::StrFormat("Queued assertion: %s", request->condition()));

#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE
  ImGuiTestEngine* engine = test_manager_->GetUITestEngine();
  if (!engine) {
    std::string message = "ImGuiTestEngine not initialized";
    response->set_success(false);
    response->set_message(message);
    response->set_actual_value("N/A");
    response->set_expected_value("N/A");
    test_manager_->MarkHarnessTestCompleted(test_id, HarnessTestStatus::kFailed,
                                            message);
    return finalize(absl::OkStatus());
  }

  std::string condition = request->condition();
  size_t colon_pos = condition.find(':');
  if (colon_pos == std::string::npos) {
    std::string message =
        "Invalid condition format. Use 'type:target' (e.g. 'visible:Main "
        "Window')";
    response->set_success(false);
    response->set_message(message);
    response->set_actual_value("N/A");
    response->set_expected_value("N/A");
    test_manager_->MarkHarnessTestCompleted(test_id, HarnessTestStatus::kFailed,
                                            message);
    test_manager_->AppendHarnessTestLog(test_id, message);
    return finalize(absl::OkStatus());
  }

  std::string assertion_type = condition.substr(0, colon_pos);
  std::string assertion_target = condition.substr(colon_pos + 1);

  auto test_data = std::make_shared<DynamicTestData>();
  TestManager* manager = test_manager_;
  test_data->test_func = [manager, captured_id = test_id, assertion_type,
                          assertion_target](ImGuiTestContext* ctx) {
    manager->MarkHarnessTestRunning(captured_id);

    auto complete_with = [manager, captured_id](bool passed,
                                                const std::string& message,
                                                const std::string& actual,
                                                const std::string& expected,
                                                HarnessTestStatus status) {
      manager->AppendHarnessTestLog(captured_id, message);
      if (!actual.empty() || !expected.empty()) {
        manager->AppendHarnessTestLog(
            captured_id,
            absl::StrFormat("Actual: %s | Expected: %s", actual, expected));
      }
      manager->MarkHarnessTestCompleted(captured_id, status,
                                        passed ? "" : message);
    };

    try {
      bool passed = false;
      std::string actual_value;
      std::string expected_value;
      std::string message;

      if (assertion_type == "visible") {
        ImGuiTestItemInfo window_info =
            ctx->WindowInfo(assertion_target.c_str(), ImGuiTestOpFlags_NoError);
        bool is_visible = (window_info.ID != 0);
        passed = is_visible;
        actual_value = is_visible ? "visible" : "hidden";
        expected_value = "visible";
        message =
            passed ? absl::StrFormat("'%s' is visible", assertion_target)
                   : absl::StrFormat("'%s' is not visible", assertion_target);
      } else if (assertion_type == "enabled") {
        ImGuiTestItemInfo item =
            ctx->ItemInfo(assertion_target.c_str(), ImGuiTestOpFlags_NoError);
        bool is_enabled =
            (item.ID != 0 && !(item.ItemFlags & ImGuiItemFlags_Disabled));
        passed = is_enabled;
        actual_value = is_enabled ? "enabled" : "disabled";
        expected_value = "enabled";
        message =
            passed ? absl::StrFormat("'%s' is enabled", assertion_target)
                   : absl::StrFormat("'%s' is not enabled", assertion_target);
      } else if (assertion_type == "exists") {
        ImGuiTestItemInfo item =
            ctx->ItemInfo(assertion_target.c_str(), ImGuiTestOpFlags_NoError);
        bool exists = (item.ID != 0);
        passed = exists;
        actual_value = exists ? "exists" : "not found";
        expected_value = "exists";
        message = passed ? absl::StrFormat("'%s' exists", assertion_target)
                         : absl::StrFormat("'%s' not found", assertion_target);
      } else if (assertion_type == "text_contains") {
        size_t second_colon = assertion_target.find(':');
        if (second_colon == std::string::npos) {
          std::string error_message =
              "text_contains requires format "
              "'text_contains:target:expected_text'";
          complete_with(false, error_message, "N/A", "N/A",
                        HarnessTestStatus::kFailed);
          return;
        }

        std::string input_target = assertion_target.substr(0, second_colon);
        std::string expected_text = assertion_target.substr(second_colon + 1);

        ImGuiTestItemInfo item = ctx->ItemInfo(input_target.c_str());
        if (item.ID != 0) {
          std::string actual_text = "(text_retrieval_not_fully_implemented)";
          passed = actual_text.find(expected_text) != std::string::npos;
          actual_value = actual_text;
          expected_value = absl::StrFormat("contains '%s'", expected_text);
          message = passed ? absl::StrFormat("'%s' contains '%s'", input_target,
                                             expected_text)
                           : absl::StrFormat(
                                 "'%s' does not contain '%s' (actual: '%s')",
                                 input_target, expected_text, actual_text);
        } else {
          passed = false;
          actual_value = "not found";
          expected_value = expected_text;
          message = absl::StrFormat("Input '%s' not found", input_target);
        }
      } else {
        std::string error_message =
            absl::StrFormat("Unknown assertion type: %s", assertion_type);
        complete_with(false, error_message, "N/A", "N/A",
                      HarnessTestStatus::kFailed);
        return;
      }

      complete_with(
          passed, message, actual_value, expected_value,
          passed ? HarnessTestStatus::kPassed : HarnessTestStatus::kFailed);
    } catch (const std::exception& e) {
      std::string error_message =
          absl::StrFormat("Assertion failed: %s", e.what());
      manager->AppendHarnessTestLog(captured_id, error_message);
      manager->MarkHarnessTestCompleted(captured_id, HarnessTestStatus::kFailed,
                                        error_message);
    }
  };

  std::string test_name = absl::StrFormat(
      "grpc_assert_%lld",
      static_cast<long long>(
          std::chrono::system_clock::now().time_since_epoch().count()));

  ImGuiTest* test = IM_REGISTER_TEST(engine, "grpc", test_name.c_str());
  test->TestFunc = RunDynamicTest;
  test->UserData = test_data.get();

  ImGuiTestEngine_QueueTest(engine, test, ImGuiTestRunFlags_RunFromGui);

  response->set_success(true);
  std::string message = absl::StrFormat("Queued assertion for '%s:%s'",
                                        assertion_type, assertion_target);
  response->set_message(message);
  response->set_actual_value("(async)");
  response->set_expected_value("(async)");
  test_manager_->AppendHarnessTestLog(test_id, message);

#else
  test_manager_->MarkHarnessTestRunning(test_id);
  std::string message = absl::StrFormat(
      "[STUB] Assertion '%s' passed (ImGuiTestEngine not available)",
      request->condition());
  test_manager_->MarkHarnessTestCompleted(test_id, HarnessTestStatus::kPassed,
                                          message);
  test_manager_->AppendHarnessTestLog(test_id, message);

  response->set_success(true);
  response->set_message(message);
  response->set_actual_value("(stub)");
  response->set_expected_value("(stub)");
#endif

  return finalize(absl::OkStatus());
}

absl::Status ImGuiTestHarnessServiceImpl::Screenshot(
    const ScreenshotRequest* request, ScreenshotResponse* response) {
  if (!response) {
    return absl::InvalidArgumentError("response cannot be null");
  }

  const std::string requested_path =
      request ? request->output_path() : std::string();
  absl::StatusOr<ScreenshotArtifact> artifact_or =
      CaptureHarnessScreenshot(requested_path);
  if (!artifact_or.ok()) {
    response->set_success(false);
    response->set_message(std::string(artifact_or.status().message()));
    return artifact_or.status();
  }

  const ScreenshotArtifact& artifact = *artifact_or;
  response->set_success(true);
  response->set_message(absl::StrFormat("Screenshot saved to %s (%dx%d)",
                                        artifact.file_path, artifact.width,
                                        artifact.height));
  response->set_file_path(artifact.file_path);
  response->set_file_size_bytes(artifact.file_size_bytes);

  return absl::OkStatus();
}

absl::Status ImGuiTestHarnessServiceImpl::GetTestStatus(
    const GetTestStatusRequest* request, GetTestStatusResponse* response) {
  if (!test_manager_) {
    return absl::FailedPreconditionError("TestManager not available");
  }

  if (request->test_id().empty()) {
    return absl::InvalidArgumentError("test_id must be provided");
  }

  auto execution_or =
      test_manager_->GetHarnessTestExecution(request->test_id());
  if (!execution_or.ok()) {
    response->set_status(GetTestStatusResponse::TEST_STATUS_UNSPECIFIED);
    response->set_error_message(std::string(execution_or.status().message()));
    return absl::OkStatus();
  }

  const auto& execution = execution_or.value();
  response->set_status(ConvertHarnessStatus(execution.status));
  response->set_queued_at_ms(ToUnixMillisSafe(execution.queued_at));
  response->set_started_at_ms(ToUnixMillisSafe(execution.started_at));
  response->set_completed_at_ms(ToUnixMillisSafe(execution.completed_at));
  response->set_execution_time_ms(ClampDurationToInt32(execution.duration));
  if (!execution.error_message.empty()) {
    response->set_error_message(execution.error_message);
  } else {
    response->clear_error_message();
  }

  response->clear_assertion_failures();
  for (const auto& failure : execution.assertion_failures) {
    response->add_assertion_failures(failure);
  }

  return absl::OkStatus();
}

absl::Status ImGuiTestHarnessServiceImpl::ListTests(
    const ListTestsRequest* request, ListTestsResponse* response) {
  if (!test_manager_) {
    return absl::FailedPreconditionError("TestManager not available");
  }

  if (request->page_size() < 0) {
    return absl::InvalidArgumentError("page_size cannot be negative");
  }

  int page_size = request->page_size() > 0 ? request->page_size() : 100;
  constexpr int kMaxPageSize = 500;
  if (page_size > kMaxPageSize) {
    page_size = kMaxPageSize;
  }

  size_t start_index = 0;
  if (!request->page_token().empty()) {
    int64_t token_value = 0;
    if (!absl::SimpleAtoi(request->page_token(), &token_value) ||
        token_value < 0) {
      return absl::InvalidArgumentError("Invalid page_token");
    }
    start_index = static_cast<size_t>(token_value);
  }

  auto summaries =
      test_manager_->ListHarnessTestSummaries(request->category_filter());

  response->set_total_count(static_cast<int32_t>(summaries.size()));

  if (start_index >= summaries.size()) {
    response->clear_tests();
    response->clear_next_page_token();
    return absl::OkStatus();
  }

  size_t end_index =
      std::min(start_index + static_cast<size_t>(page_size), summaries.size());

  for (size_t i = start_index; i < end_index; ++i) {
    const auto& summary = summaries[i];
    auto* test_info = response->add_tests();
    const auto& exec = summary.latest_execution;

    test_info->set_test_id(exec.test_id);
    test_info->set_name(exec.name);
    test_info->set_category(exec.category);

    int64_t last_run_ms = ToUnixMillisSafe(exec.completed_at);
    if (last_run_ms == 0) {
      last_run_ms = ToUnixMillisSafe(exec.started_at);
    }
    if (last_run_ms == 0) {
      last_run_ms = ToUnixMillisSafe(exec.queued_at);
    }
    test_info->set_last_run_timestamp_ms(last_run_ms);

    test_info->set_total_runs(summary.total_runs);
    test_info->set_pass_count(summary.pass_count);
    test_info->set_fail_count(summary.fail_count);

    int32_t average_duration_ms = 0;
    if (summary.total_runs > 0) {
      absl::Duration average_duration =
          summary.total_duration / summary.total_runs;
      average_duration_ms = ClampDurationToInt32(average_duration);
    }
    test_info->set_average_duration_ms(average_duration_ms);
  }

  if (end_index < summaries.size()) {
    response->set_next_page_token(absl::StrCat(end_index));
  } else {
    response->clear_next_page_token();
  }

  return absl::OkStatus();
}

absl::Status ImGuiTestHarnessServiceImpl::GetTestResults(
    const GetTestResultsRequest* request, GetTestResultsResponse* response) {
  if (!test_manager_) {
    return absl::FailedPreconditionError("TestManager not available");
  }

  if (request->test_id().empty()) {
    return absl::InvalidArgumentError("test_id must be provided");
  }

  auto execution_or =
      test_manager_->GetHarnessTestExecution(request->test_id());
  if (!execution_or.ok()) {
    return execution_or.status();
  }

  const auto& execution = execution_or.value();
  response->set_success(execution.status == HarnessTestStatus::kPassed);
  response->set_test_name(execution.name);
  response->set_category(execution.category);

  int64_t executed_at_ms = ToUnixMillisSafe(execution.completed_at);
  if (executed_at_ms == 0) {
    executed_at_ms = ToUnixMillisSafe(execution.started_at);
  }
  if (executed_at_ms == 0) {
    executed_at_ms = ToUnixMillisSafe(execution.queued_at);
  }
  response->set_executed_at_ms(executed_at_ms);
  response->set_duration_ms(ClampDurationToInt32(execution.duration));

  response->clear_assertions();
  if (!execution.assertion_failures.empty()) {
    for (const auto& failure : execution.assertion_failures) {
      auto* assertion = response->add_assertions();
      assertion->set_description(failure);
      assertion->set_passed(false);
      assertion->set_error_message(failure);
    }
  } else if (!execution.error_message.empty()) {
    auto* assertion = response->add_assertions();
    assertion->set_description("Execution error");
    assertion->set_passed(false);
    assertion->set_error_message(execution.error_message);
  }

  if (request->include_logs()) {
    for (const auto& log_entry : execution.logs) {
      response->add_logs(log_entry);
    }
  }

  auto* metrics_map = response->mutable_metrics();
  for (const auto& [key, value] : execution.metrics) {
    (*metrics_map)[key] = value;
  }

  // IT-08b: Include failure diagnostics if available
  if (!execution.screenshot_path.empty()) {
    response->set_screenshot_path(execution.screenshot_path);
    response->set_screenshot_size_bytes(execution.screenshot_size_bytes);
  }
  if (!execution.failure_context.empty()) {
    response->set_failure_context(execution.failure_context);
  }
  if (!execution.widget_state.empty()) {
    response->set_widget_state(execution.widget_state);
  }

  return absl::OkStatus();
}

absl::Status ImGuiTestHarnessServiceImpl::DiscoverWidgets(
    const DiscoverWidgetsRequest* request, DiscoverWidgetsResponse* response) {
  if (!request) {
    return absl::InvalidArgumentError("request cannot be null");
  }
  if (!response) {
    return absl::InvalidArgumentError("response cannot be null");
  }

  if (!test_manager_) {
    return absl::FailedPreconditionError("TestManager not available");
  }

  widget_discovery_service_.CollectWidgets(/*ctx=*/nullptr, *request, response);
  return absl::OkStatus();
}

absl::Status ImGuiTestHarnessServiceImpl::StartRecording(
    const StartRecordingRequest* request, StartRecordingResponse* response) {
  if (!request) {
    return absl::InvalidArgumentError("request cannot be null");
  }
  if (!response) {
    return absl::InvalidArgumentError("response cannot be null");
  }

  TestRecorder::RecordingOptions options;
  options.output_path = request->output_path();
  options.session_name = request->session_name();
  options.description = request->description();

  if (options.output_path.empty()) {
    response->set_success(false);
    response->set_message("output_path is required to start recording");
    return absl::InvalidArgumentError("output_path cannot be empty");
  }

  absl::StatusOr<std::string> recording_id = test_recorder_.Start(options);
  if (!recording_id.ok()) {
    response->set_success(false);
    response->set_message(std::string(recording_id.status().message()));
    return recording_id.status();
  }

  response->set_success(true);
  response->set_message("Recording started");
  response->set_recording_id(*recording_id);
  response->set_started_at_ms(absl::ToUnixMillis(absl::Now()));
  return absl::OkStatus();
}

absl::Status ImGuiTestHarnessServiceImpl::StopRecording(
    const StopRecordingRequest* request, StopRecordingResponse* response) {
  if (!request) {
    return absl::InvalidArgumentError("request cannot be null");
  }
  if (!response) {
    return absl::InvalidArgumentError("response cannot be null");
  }

  absl::StatusOr<TestRecorder::StopRecordingSummary> summary =
      test_recorder_.Stop(request->recording_id(), request->discard());
  if (!summary.ok()) {
    response->set_success(false);
    response->set_message(std::string(summary.status().message()));
    return summary.status();
  }

  response->set_success(true);
  if (summary->saved) {
    response->set_message("Recording saved");
  } else {
    response->set_message("Recording discarded");
  }
  response->set_output_path(summary->output_path);
  response->set_step_count(summary->step_count);
  response->set_duration_ms(absl::ToInt64Milliseconds(summary->duration));
  return absl::OkStatus();
}

absl::Status ImGuiTestHarnessServiceImpl::ReplayTest(
    const ReplayTestRequest* request, ReplayTestResponse* response) {
  if (!request) {
    return absl::InvalidArgumentError("request cannot be null");
  }
  if (!response) {
    return absl::InvalidArgumentError("response cannot be null");
  }

  response->clear_logs();
  response->clear_assertions();

  if (request->script_path().empty()) {
    response->set_success(false);
    response->set_message("script_path is required");
    return absl::InvalidArgumentError("script_path cannot be empty");
  }

  absl::StatusOr<TestScript> script_or =
      TestScriptParser::ParseFromFile(request->script_path());
  if (!script_or.ok()) {
    response->set_success(false);
    response->set_message(std::string(script_or.status().message()));
    return script_or.status();
  }
  TestScript script = std::move(*script_or);

  absl::flat_hash_map<std::string, std::string> overrides;
  for (const auto& entry : request->parameter_overrides()) {
    overrides[entry.first] = entry.second;
  }

  response->set_replay_session_id(absl::StrFormat(
      "replay_%s",
      absl::FormatTime("%Y%m%dT%H%M%S", absl::Now(), absl::UTCTimeZone())));

  auto suspension = test_recorder_.Suspend();

  std::vector<std::string> logs;
  logs.reserve(script.steps.size() * 2 + 4);

  bool overall_success = true;
  std::string overall_message = "Replay completed successfully";
  int steps_executed = 0;
  absl::Status overall_rpc_status = absl::OkStatus();

  for (const auto& step : script.steps) {
    ++steps_executed;
    std::string action_label =
        absl::StrFormat("Step %d: %s", steps_executed, step.action);
    logs.push_back(action_label);

    absl::Status status = absl::OkStatus();
    bool step_success = false;
    std::string step_message;
    HarnessTestExecution execution;
    bool have_execution = false;

    if (step.action == "click") {
      ClickRequest sub_request;
      sub_request.set_target(ApplyOverrides(step.target, overrides));
      sub_request.set_type(ClickTypeFromString(step.click_type));
      ClickResponse sub_response;
      status = Click(&sub_request, &sub_response);
      step_success = sub_response.success();
      step_message = sub_response.message();
      if (status.ok() && !sub_response.test_id().empty()) {
        absl::Status wait_status = WaitForHarnessTestCompletion(
            test_manager_, sub_response.test_id(), &execution);
        if (wait_status.ok()) {
          have_execution = true;
          if (!execution.error_message.empty()) {
            step_message = execution.error_message;
          }
        } else {
          status = wait_status;
          step_success = false;
          step_message = std::string(wait_status.message());
        }
      }
    } else if (step.action == "type") {
      TypeRequest sub_request;
      sub_request.set_target(ApplyOverrides(step.target, overrides));
      sub_request.set_text(ApplyOverrides(step.text, overrides));
      sub_request.set_clear_first(step.clear_first);
      TypeResponse sub_response;
      status = Type(&sub_request, &sub_response);
      step_success = sub_response.success();
      step_message = sub_response.message();
      if (status.ok() && !sub_response.test_id().empty()) {
        absl::Status wait_status = WaitForHarnessTestCompletion(
            test_manager_, sub_response.test_id(), &execution);
        if (wait_status.ok()) {
          have_execution = true;
          if (!execution.error_message.empty()) {
            step_message = execution.error_message;
          }
        } else {
          status = wait_status;
          step_success = false;
          step_message = std::string(wait_status.message());
        }
      }
    } else if (step.action == "wait") {
      WaitRequest sub_request;
      sub_request.set_condition(ApplyOverrides(step.condition, overrides));
      if (step.timeout_ms > 0) {
        sub_request.set_timeout_ms(step.timeout_ms);
      }
      WaitResponse sub_response;
      status = Wait(&sub_request, &sub_response);
      step_success = sub_response.success();
      step_message = sub_response.message();
      if (status.ok() && !sub_response.test_id().empty()) {
        absl::Status wait_status = WaitForHarnessTestCompletion(
            test_manager_, sub_response.test_id(), &execution);
        if (wait_status.ok()) {
          have_execution = true;
          if (!execution.error_message.empty()) {
            step_message = execution.error_message;
          }
        } else {
          status = wait_status;
          step_success = false;
          step_message = std::string(wait_status.message());
        }
      }
    } else if (step.action == "assert") {
      AssertRequest sub_request;
      sub_request.set_condition(ApplyOverrides(step.condition, overrides));
      AssertResponse sub_response;
      status = Assert(&sub_request, &sub_response);
      step_success = sub_response.success();
      step_message = sub_response.message();
      if (status.ok() && !sub_response.test_id().empty()) {
        absl::Status wait_status = WaitForHarnessTestCompletion(
            test_manager_, sub_response.test_id(), &execution);
        if (wait_status.ok()) {
          have_execution = true;
          if (!execution.error_message.empty()) {
            step_message = execution.error_message;
          }
        } else {
          status = wait_status;
          step_success = false;
          step_message = std::string(wait_status.message());
        }
      }
    } else if (step.action == "screenshot") {
      ScreenshotRequest sub_request;
      sub_request.set_window_title(ApplyOverrides(step.target, overrides));
      if (!step.region.empty()) {
        sub_request.set_output_path(ApplyOverrides(step.region, overrides));
      }
      ScreenshotResponse sub_response;
      status = Screenshot(&sub_request, &sub_response);
      step_success = sub_response.success();
      step_message = sub_response.message();
    } else {
      status = absl::InvalidArgumentError(
          absl::StrFormat("Unsupported action '%s'", step.action));
      step_message = std::string(status.message().data(), status.message().size());
    }

    auto* assertion = response->add_assertions();
    assertion->set_description(
        absl::StrFormat("Step %d (%s)", steps_executed, step.action));

    if (!status.ok()) {
      assertion->set_passed(false);
      assertion->set_error_message(std::string(status.message().data(), status.message().size()));
      overall_success = false;
      overall_message = step_message;
      logs.push_back(absl::StrFormat("  Error: %s", status.message()));
      overall_rpc_status = status;
      break;
    }

    bool expectations_met = (step_success == step.expect_success);
    std::string expectation_error;

    if (!expectations_met) {
      expectation_error =
          absl::StrFormat("Expected success=%s but got %s",
                          step.expect_success ? "true" : "false",
                          step_success ? "true" : "false");
    }

    if (!step.expect_status.empty()) {
      HarnessTestStatus expected_status =
          ::yaze::test::HarnessStatusFromString(step.expect_status);
      if (!have_execution) {
        expectations_met = false;
        if (!expectation_error.empty()) {
          expectation_error.append("; ");
        }
        expectation_error.append("No execution details available");
      } else if (expected_status != HarnessTestStatus::kUnspecified &&
                 execution.status != expected_status) {
        expectations_met = false;
        if (!expectation_error.empty()) {
          expectation_error.append("; ");
        }
        expectation_error.append(absl::StrFormat(
            "Expected status %s but observed %s", step.expect_status,
            ::yaze::test::HarnessStatusToString(execution.status)));
      }
      if (have_execution) {
        assertion->set_actual_value(
            ::yaze::test::HarnessStatusToString(execution.status));
        assertion->set_expected_value(step.expect_status);
      }
    }

    if (!step.expect_message.empty()) {
      std::string actual_message = step_message;
      if (have_execution && !execution.error_message.empty()) {
        actual_message = execution.error_message;
      }
      if (actual_message.find(step.expect_message) == std::string::npos) {
        expectations_met = false;
        if (!expectation_error.empty()) {
          expectation_error.append("; ");
        }
        expectation_error.append(
            absl::StrFormat("Expected message containing '%s' but got '%s'",
                            step.expect_message, actual_message));
      }
    }

    if (!expectations_met) {
      assertion->set_passed(false);
      assertion->set_error_message(expectation_error);
      overall_success = false;
      overall_message = expectation_error;
      logs.push_back(
          absl::StrFormat("  Failed expectations: %s", expectation_error));
      if (request->ci_mode()) {
        break;
      }
    } else {
      assertion->set_passed(true);
      logs.push_back(absl::StrFormat("  Result: %s", step_message));
    }

    if (have_execution && !execution.assertion_failures.empty()) {
      for (const auto& failure : execution.assertion_failures) {
        logs.push_back(absl::StrFormat("  Assertion failure: %s", failure));
      }
    }

    if (!overall_success && request->ci_mode()) {
      break;
    }
  }

  response->set_steps_executed(steps_executed);
  response->set_success(overall_success);
  response->set_message(overall_message);
  for (const auto& log_entry : logs) {
    response->add_logs(log_entry);
  }

  return overall_rpc_status;
}

// ============================================================================
// ImGuiTestHarnessServer - Server Lifecycle
// ============================================================================

ImGuiTestHarnessServer& ImGuiTestHarnessServer::Instance() {
  static ImGuiTestHarnessServer* instance = new ImGuiTestHarnessServer();
  return *instance;
}

ImGuiTestHarnessServer::~ImGuiTestHarnessServer() {
  Shutdown();
}

absl::Status ImGuiTestHarnessServer::Start(int port,
                                           TestManager* test_manager) {
  if (server_) {
    return absl::FailedPreconditionError("Server already running");
  }

  if (!test_manager) {
    return absl::InvalidArgumentError("TestManager cannot be null");
  }

  // Create the service implementation with TestManager reference
  service_ = std::make_unique<ImGuiTestHarnessServiceImpl>(test_manager);

  // Create the gRPC service wrapper (store as member to prevent it from going
  // out of scope)
  grpc_service_ = std::make_unique<ImGuiTestHarnessServiceGrpc>(service_.get());

  std::string server_address = absl::StrFormat("0.0.0.0:%d", port);

  grpc::ServerBuilder builder;

  // Listen on all interfaces (use 0.0.0.0 to avoid IPv6/IPv4 binding conflicts)
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

  // Register service
  builder.RegisterService(grpc_service_.get());

  // Build and start
  server_ = builder.BuildAndStart();

  if (!server_) {
    return absl::InternalError(
        absl::StrFormat("Failed to start gRPC server on %s", server_address));
  }

  port_ = port;

  std::cout << "✓ ImGuiTestHarness gRPC server listening on " << server_address
            << " (with TestManager integration)\n";
  std::cout << "  Use 'grpcurl -plaintext -d '{\"message\":\"test\"}' "
            << server_address << " yaze.test.ImGuiTestHarness/Ping' to test\n";

  return absl::OkStatus();
}

void ImGuiTestHarnessServer::Shutdown() {
  if (server_) {
    std::cout << "⏹ Shutting down ImGuiTestHarness gRPC server...\n";
    server_->Shutdown();
    server_.reset();
    service_.reset();
    port_ = 0;
    std::cout << "✓ ImGuiTestHarness gRPC server stopped\n";
  }
}

}  // namespace test
}  // namespace yaze

#endif  // YAZE_WITH_GRPC
