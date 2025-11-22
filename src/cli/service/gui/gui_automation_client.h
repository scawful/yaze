// gui_automation_client.h
// gRPC client for automating YAZE GUI through ImGuiTestHarness service

#ifndef YAZE_CLI_SERVICE_GUI_AUTOMATION_CLIENT_H
#define YAZE_CLI_SERVICE_GUI_AUTOMATION_CLIENT_H

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"

#ifdef YAZE_WITH_GRPC
// Undefine Windows macros that conflict with protobuf generated code
#ifdef _WIN32
#pragma push_macro("DWORD")
#pragma push_macro("ERROR")
#undef DWORD
#undef ERROR
#endif  // _WIN32

#include <grpcpp/grpcpp.h>

#include "protos/imgui_test_harness.grpc.pb.h"

// Restore Windows macros
#ifdef _WIN32
#pragma pop_macro("DWORD")
#pragma pop_macro("ERROR")
#endif
#endif

namespace yaze {
namespace cli {

/**
 * @brief Type of click action to perform
 */
enum class ClickType { kLeft, kRight, kMiddle, kDouble };

/**
 * @brief Result of a GUI automation action
 */
struct AutomationResult {
  bool success;
  std::string message;
  std::chrono::milliseconds execution_time;
  std::string actual_value;    // For assertions
  std::string expected_value;  // For assertions
  std::string test_id;         // Test execution identifier (for introspection)
};

/**
 * @brief Execution status codes returned by the harness
 */
enum class TestRunStatus {
  kUnknown,
  kQueued,
  kRunning,
  kPassed,
  kFailed,
  kTimeout
};

/**
 * @brief Detailed information about an individual test execution
 */
struct TestStatusDetails {
  std::string test_id;
  TestRunStatus status = TestRunStatus::kUnknown;
  std::optional<absl::Time> queued_at;
  std::optional<absl::Time> started_at;
  std::optional<absl::Time> completed_at;
  int execution_time_ms = 0;
  std::string error_message;
  std::vector<std::string> assertion_failures;
};

/**
 * @brief Aggregated metadata about a harness test
 */
struct HarnessTestSummary {
  std::string test_id;
  std::string name;
  std::string category;
  std::optional<absl::Time> last_run_at;
  int total_runs = 0;
  int pass_count = 0;
  int fail_count = 0;
  int average_duration_ms = 0;
};

/**
 * @brief Result container for ListTests RPC
 */
struct ListTestsResult {
  std::vector<HarnessTestSummary> tests;
  std::string next_page_token;
  int total_count = 0;
};

/**
 * @brief Individual assertion outcome within a harness test
 */
struct AssertionOutcome {
  std::string description;
  bool passed = false;
  std::string expected_value;
  std::string actual_value;
  std::string error_message;
};

/**
 * @brief Detailed execution results for a specific harness test
 */
struct TestResultDetails {
  std::string test_id;
  bool success = false;
  std::string test_name;
  std::string category;
  std::optional<absl::Time> executed_at;
  int duration_ms = 0;
  std::vector<AssertionOutcome> assertions;
  std::vector<std::string> logs;
  std::map<std::string, int> metrics;
  std::string screenshot_path;
  int64_t screenshot_size_bytes = 0;
  std::string failure_context;
  std::string widget_state;
};

struct ReplayTestResult {
  bool success = false;
  std::string message;
  std::string replay_session_id;
  int steps_executed = 0;
  std::vector<AssertionOutcome> assertions;
  std::vector<std::string> logs;
};

struct StartRecordingResult {
  bool success = false;
  std::string message;
  std::string recording_id;
  std::optional<absl::Time> started_at;
};

struct StopRecordingResult {
  bool success = false;
  std::string message;
  std::string output_path;
  int step_count = 0;
  std::chrono::milliseconds duration{0};
};

enum class WidgetTypeFilter {
  kUnspecified,
  kAll,
  kButton,
  kInput,
  kMenu,
  kTab,
  kCheckbox,
  kSlider,
  kCanvas,
  kSelectable,
  kOther,
};

struct WidgetBoundingBox {
  float min_x = 0.0f;
  float min_y = 0.0f;
  float max_x = 0.0f;
  float max_y = 0.0f;
};

struct WidgetDescriptor {
  std::string path;
  std::string label;
  std::string type;
  std::string description;
  std::string suggested_action;
  bool visible = true;
  bool enabled = true;
  WidgetBoundingBox bounds;
  uint32_t widget_id = 0;
  bool has_bounds = false;
  int64_t last_seen_frame = -1;
  std::optional<absl::Time> last_seen_at;
  bool stale = false;
};

struct DiscoveredWindowInfo {
  std::string name;
  bool visible = true;
  std::vector<WidgetDescriptor> widgets;
};

struct DiscoverWidgetsQuery {
  std::string window_filter;
  WidgetTypeFilter type_filter = WidgetTypeFilter::kUnspecified;
  std::string path_prefix;
  bool include_invisible = false;
  bool include_disabled = false;
};

struct DiscoverWidgetsResult {
  std::vector<DiscoveredWindowInfo> windows;
  int total_widgets = 0;
  std::optional<absl::Time> generated_at;
};

/**
 * @brief Client for automating YAZE GUI through gRPC
 *
 * This client wraps the ImGuiTestHarness gRPC service and provides
 * a C++ API for CLI commands to drive the YAZE GUI remotely.
 *
 * Example usage:
 * @code
 *   GuiAutomationClient client("localhost:50052");
 *   RETURN_IF_ERROR(client.Connect());
 *
 *   auto result = client.Click("button:Overworld", ClickType::kLeft);
 *   if (!result.ok()) return result.status();
 *
 *   if (!result->success) {
 *     return absl::InternalError(result->message);
 *   }
 * @endcode
 */
class GuiAutomationClient {
 public:
  /**
   * @brief Construct a new GUI automation client
   * @param server_address Address of the test harness server (e.g.,
   * "localhost:50052")
   */
  explicit GuiAutomationClient(const std::string& server_address);

  /**
   * @brief Connect to the test harness server
   * @return Status indicating success or failure
   */
  absl::Status Connect();

  /**
   * @brief Check if the server is reachable and responsive
   * @param message Optional message to send in ping
   * @return Result with server version and timestamp
   */
  absl::StatusOr<AutomationResult> Ping(const std::string& message = "ping");

  /**
   * @brief Click a GUI element
   * @param target Target element (format: "button:Label" or "window:Name")
   * @param type Type of click (left, right, middle, double)
   * @return Result indicating success/failure and execution time
   */
  absl::StatusOr<AutomationResult> Click(const std::string& target,
                                         ClickType type = ClickType::kLeft);

  /**
   * @brief Type text into an input field
   * @param target Target input field (format: "input:Label")
   * @param text Text to type
   * @param clear_first Whether to clear existing text before typing
   * @return Result indicating success/failure and execution time
   */
  absl::StatusOr<AutomationResult> Type(const std::string& target,
                                        const std::string& text,
                                        bool clear_first = false);

  /**
   * @brief Wait for a condition to be met
   * @param condition Condition to wait for (e.g., "window_visible:Editor")
   * @param timeout_ms Maximum time to wait in milliseconds
   * @param poll_interval_ms How often to check the condition
   * @return Result indicating whether condition was met
   */
  absl::StatusOr<AutomationResult> Wait(const std::string& condition,
                                        int timeout_ms = 5000,
                                        int poll_interval_ms = 100);

  /**
   * @brief Assert a GUI state condition
   * @param condition Condition to assert (e.g., "visible:Window Name")
   * @return Result with actual vs expected values
   */
  absl::StatusOr<AutomationResult> Assert(const std::string& condition);

  /**
   * @brief Capture a screenshot
   * @param region Region to capture ("full", "window", "element")
   * @param format Image format ("PNG", "JPEG")
   * @return Result with file path if successful
   */
  absl::StatusOr<AutomationResult> Screenshot(
      const std::string& region = "full", const std::string& format = "PNG");

  /**
   * @brief Fetch the current execution status for a harness test
   */
  absl::StatusOr<TestStatusDetails> GetTestStatus(const std::string& test_id);

  /**
   * @brief Enumerate harness tests with optional filtering
   */
  absl::StatusOr<ListTestsResult> ListTests(
      const std::string& category_filter = "", int page_size = 100,
      const std::string& page_token = "");

  /**
   * @brief Retrieve detailed results for a harness test execution
   */
  absl::StatusOr<TestResultDetails> GetTestResults(const std::string& test_id,
                                                   bool include_logs = false);

  absl::StatusOr<DiscoverWidgetsResult> DiscoverWidgets(
      const DiscoverWidgetsQuery& query);

  absl::StatusOr<ReplayTestResult> ReplayTest(
      const std::string& script_path, bool ci_mode,
      const std::map<std::string, std::string>& parameter_overrides = {});

  absl::StatusOr<StartRecordingResult> StartRecording(
      const std::string& output_path, const std::string& session_name,
      const std::string& description);

  absl::StatusOr<StopRecordingResult> StopRecording(
      const std::string& recording_id, bool discard = false);

  /**
   * @brief Check if client is connected
   */
  bool IsConnected() const { return connected_; }

  /**
   * @brief Get the server address
   */
  const std::string& ServerAddress() const { return server_address_; }

 private:
#ifdef YAZE_WITH_GRPC
  std::unique_ptr<yaze::test::ImGuiTestHarness::Stub> stub_;
#endif
  std::string server_address_;
  bool connected_ = false;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_GUI_AUTOMATION_CLIENT_H
