// gui_automation_client.cc
// Implementation of gRPC client for YAZE GUI automation

#include "cli/service/gui_automation_client.h"

#include "absl/strings/str_format.h"

namespace yaze {
namespace cli {

GuiAutomationClient::GuiAutomationClient(const std::string& server_address)
    : server_address_(server_address) {}

absl::Status GuiAutomationClient::Connect() {
#ifdef YAZE_WITH_GRPC
  auto channel = grpc::CreateChannel(server_address_,
                                     grpc::InsecureChannelCredentials());
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
    return absl::FailedPreconditionError("Not connected. Call Connect() first.");
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
  result.message = absl::StrFormat("Server version: %s (timestamp: %lld)",
                                   response.yaze_version(),
                                   response.timestamp_ms());
  result.execution_time = std::chrono::milliseconds(0);
  return result;
#else
  return absl::UnimplementedError("gRPC not available");
#endif
}

absl::StatusOr<AutomationResult> GuiAutomationClient::Click(
    const std::string& target, ClickType type) {
#ifdef YAZE_WITH_GRPC
  if (!stub_) {
    return absl::FailedPreconditionError("Not connected. Call Connect() first.");
  }
  
  yaze::test::ClickRequest request;
  request.set_target(target);
  
  switch (type) {
    case ClickType::kLeft:
      request.set_type(yaze::test::ClickRequest::LEFT);
      break;
    case ClickType::kRight:
      request.set_type(yaze::test::ClickRequest::RIGHT);
      break;
    case ClickType::kMiddle:
      request.set_type(yaze::test::ClickRequest::MIDDLE);
      break;
    case ClickType::kDouble:
      request.set_type(yaze::test::ClickRequest::DOUBLE);
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
  result.execution_time = std::chrono::milliseconds(
      response.execution_time_ms());
  return result;
#else
  return absl::UnimplementedError("gRPC not available");
#endif
}

absl::StatusOr<AutomationResult> GuiAutomationClient::Type(
    const std::string& target, const std::string& text, bool clear_first) {
#ifdef YAZE_WITH_GRPC
  if (!stub_) {
    return absl::FailedPreconditionError("Not connected. Call Connect() first.");
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
  result.execution_time = std::chrono::milliseconds(
      response.execution_time_ms());
  return result;
#else
  return absl::UnimplementedError("gRPC not available");
#endif
}

absl::StatusOr<AutomationResult> GuiAutomationClient::Wait(
    const std::string& condition, int timeout_ms, int poll_interval_ms) {
#ifdef YAZE_WITH_GRPC
  if (!stub_) {
    return absl::FailedPreconditionError("Not connected. Call Connect() first.");
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
  result.execution_time = std::chrono::milliseconds(
      response.elapsed_ms());
  return result;
#else
  return absl::UnimplementedError("gRPC not available");
#endif
}

absl::StatusOr<AutomationResult> GuiAutomationClient::Assert(
    const std::string& condition) {
#ifdef YAZE_WITH_GRPC
  if (!stub_) {
    return absl::FailedPreconditionError("Not connected. Call Connect() first.");
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
  return result;
#else
  return absl::UnimplementedError("gRPC not available");
#endif
}

absl::StatusOr<AutomationResult> GuiAutomationClient::Screenshot(
    const std::string& region, const std::string& format) {
#ifdef YAZE_WITH_GRPC
  if (!stub_) {
    return absl::FailedPreconditionError("Not connected. Call Connect() first.");
  }
  
  yaze::test::ScreenshotRequest request;
  request.set_window_title("");  // Empty = main window
  request.set_output_path("/tmp/yaze_screenshot.png");  // Default path
  request.set_format(yaze::test::ScreenshotRequest::PNG);  // Always PNG for now
  
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
  return result;
#else
  return absl::UnimplementedError("gRPC not available");
#endif
}

}  // namespace cli
}  // namespace yaze
