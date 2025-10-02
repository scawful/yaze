// gui_automation_client.h
// gRPC client for automating YAZE GUI through ImGuiTestHarness service

#ifndef YAZE_CLI_SERVICE_GUI_AUTOMATION_CLIENT_H
#define YAZE_CLI_SERVICE_GUI_AUTOMATION_CLIENT_H

#include "absl/status/status.h"
#include "absl/status/statusor.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#ifdef YAZE_WITH_GRPC
#include <grpcpp/grpcpp.h>
#include "app/core/proto/imgui_test_harness.grpc.pb.h"
#endif

namespace yaze {
namespace cli {

/**
 * @brief Type of click action to perform
 */
enum class ClickType {
  kLeft,
  kRight,
  kMiddle,
  kDouble
};

/**
 * @brief Result of a GUI automation action
 */
struct AutomationResult {
  bool success;
  std::string message;
  std::chrono::milliseconds execution_time;
  std::string actual_value;    // For assertions
  std::string expected_value;  // For assertions
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
   * @param server_address Address of the test harness server (e.g., "localhost:50052")
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
  absl::StatusOr<AutomationResult> Screenshot(const std::string& region = "full",
                                               const std::string& format = "PNG");

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
