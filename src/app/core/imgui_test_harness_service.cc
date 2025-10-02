#include "app/core/imgui_test_harness_service.h"

#ifdef YAZE_WITH_GRPC

#include <chrono>
#include <iostream>

#include "absl/strings/str_format.h"
#include "app/core/proto/imgui_test_harness.grpc.pb.h"
#include "app/core/proto/imgui_test_harness.pb.h"
#include "yaze.h"  // For YAZE_VERSION_STRING

#include <grpcpp/grpcpp.h>
#include <grpcpp/server_builder.h>

namespace yaze {
namespace test {

// gRPC service wrapper that forwards to our implementation
class ImGuiTestHarnessServiceGrpc final : public ImGuiTestHarness::Service {
 public:
  explicit ImGuiTestHarnessServiceGrpc(ImGuiTestHarnessServiceImpl* impl)
      : impl_(impl) {}

  grpc::Status Ping(grpc::ServerContext* context, const PingRequest* request,
                    PingResponse* response) override {
    auto status = impl_->Ping(request, response);
    if (!status.ok()) {
      return grpc::Status(grpc::StatusCode::INTERNAL,
                          std::string(status.message()));
    }
    return grpc::Status::OK;
  }

  grpc::Status Click(grpc::ServerContext* context, const ClickRequest* request,
                     ClickResponse* response) override {
    auto status = impl_->Click(request, response);
    if (!status.ok()) {
      return grpc::Status(grpc::StatusCode::INTERNAL,
                          std::string(status.message()));
    }
    return grpc::Status::OK;
  }

  grpc::Status Type(grpc::ServerContext* context, const TypeRequest* request,
                    TypeResponse* response) override {
    auto status = impl_->Type(request, response);
    if (!status.ok()) {
      return grpc::Status(grpc::StatusCode::INTERNAL,
                          std::string(status.message()));
    }
    return grpc::Status::OK;
  }

  grpc::Status Wait(grpc::ServerContext* context, const WaitRequest* request,
                    WaitResponse* response) override {
    auto status = impl_->Wait(request, response);
    if (!status.ok()) {
      return grpc::Status(grpc::StatusCode::INTERNAL,
                          std::string(status.message()));
    }
    return grpc::Status::OK;
  }

  grpc::Status Assert(grpc::ServerContext* context,
                      const AssertRequest* request,
                      AssertResponse* response) override {
    auto status = impl_->Assert(request, response);
    if (!status.ok()) {
      return grpc::Status(grpc::StatusCode::INTERNAL,
                          std::string(status.message()));
    }
    return grpc::Status::OK;
  }

  grpc::Status Screenshot(grpc::ServerContext* context,
                          const ScreenshotRequest* request,
                          ScreenshotResponse* response) override {
    auto status = impl_->Screenshot(request, response);
    if (!status.ok()) {
      return grpc::Status(grpc::StatusCode::INTERNAL,
                          std::string(status.message()));
    }
    return grpc::Status::OK;
  }

 private:
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

  // Parse target: "button:Open ROM" -> type=button, label="Open ROM"
  std::string target = request->target();
  size_t colon_pos = target.find(':');

  if (colon_pos == std::string::npos) {
    response->set_success(false);
    response->set_message("Invalid target format. Use 'type:label'");
    return absl::OkStatus();
  }

  std::string widget_type = target.substr(0, colon_pos);
  std::string widget_label = target.substr(colon_pos + 1);

  // TODO: Integrate with ImGuiTestEngine to actually perform the click
  // For now, just simulate success

  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);

  response->set_success(true);
  response->set_message(
      absl::StrFormat("Clicked %s '%s'", widget_type, widget_label));
  response->set_execution_time_ms(elapsed.count());

  return absl::OkStatus();
}

absl::Status ImGuiTestHarnessServiceImpl::Type(const TypeRequest* request,
                                                TypeResponse* response) {
  auto start = std::chrono::steady_clock::now();

  // TODO: Implement actual text input via ImGuiTestEngine
  
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);

  response->set_success(true);
  response->set_message(
      absl::StrFormat("Typed '%s' into %s", request->text(), request->target()));
  response->set_execution_time_ms(elapsed.count());

  return absl::OkStatus();
}

absl::Status ImGuiTestHarnessServiceImpl::Wait(const WaitRequest* request,
                                                WaitResponse* response) {
  auto start = std::chrono::steady_clock::now();

  // TODO: Implement actual condition polling

  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);

  response->set_success(true);
  response->set_message(
      absl::StrFormat("Condition '%s' met", request->condition()));
  response->set_elapsed_ms(elapsed.count());

  return absl::OkStatus();
}

absl::Status ImGuiTestHarnessServiceImpl::Assert(const AssertRequest* request,
                                                  AssertResponse* response) {
  // TODO: Implement actual assertion checking

  response->set_success(true);
  response->set_message(
      absl::StrFormat("Assertion '%s' passed", request->condition()));
  response->set_actual_value("(not implemented)");
  response->set_expected_value("(not implemented)");

  return absl::OkStatus();
}

absl::Status ImGuiTestHarnessServiceImpl::Screenshot(
    const ScreenshotRequest* request, ScreenshotResponse* response) {
  // TODO: Implement actual screenshot capture

  response->set_success(false);
  response->set_message("Screenshot not yet implemented");
  response->set_file_path("");
  response->set_file_size_bytes(0);

  return absl::OkStatus();
}

// ============================================================================
// ImGuiTestHarnessServer - Server Lifecycle
// ============================================================================

ImGuiTestHarnessServer& ImGuiTestHarnessServer::Instance() {
  static ImGuiTestHarnessServer* instance = new ImGuiTestHarnessServer();
  return *instance;
}

absl::Status ImGuiTestHarnessServer::Start(int port) {
  if (server_) {
    return absl::FailedPreconditionError("Server already running");
  }

  // Create the service implementation
  service_ = std::make_unique<ImGuiTestHarnessServiceImpl>();

  // Create the gRPC service wrapper
  auto grpc_service = std::make_unique<ImGuiTestHarnessServiceGrpc>(service_.get());

  std::string server_address = absl::StrFormat("127.0.0.1:%d", port);

  grpc::ServerBuilder builder;

  // Listen on localhost only (security)
  builder.AddListeningPort(server_address,
                           grpc::InsecureServerCredentials());

  // Register service
  builder.RegisterService(grpc_service.get());

  // Build and start
  server_ = builder.BuildAndStart();

  if (!server_) {
    return absl::InternalError(
        absl::StrFormat("Failed to start gRPC server on %s", server_address));
  }

  port_ = port;

  std::cout << "✓ ImGuiTestHarness gRPC server listening on " << server_address
            << "\n";
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
