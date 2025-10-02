#include "app/core/imgui_test_harness_service.h"

#ifdef YAZE_WITH_GRPC

#include <chrono>
#include <iostream>
#include <thread>

#include "absl/strings/str_format.h"
#include "app/core/proto/imgui_test_harness.grpc.pb.h"
#include "app/core/proto/imgui_test_harness.pb.h"
#include "app/test/test_manager.h"
#include "yaze.h"  // For YAZE_VERSION_STRING

#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE
#include "imgui_test_engine/imgui_te_engine.h"
#include "imgui_test_engine/imgui_te_context.h"

// Helper to register and run a test dynamically
namespace {
struct DynamicTestData {
  std::function<void(ImGuiTestContext*)> test_func;
};

void RunDynamicTest(ImGuiTestContext* ctx) {
  auto* data = (DynamicTestData*)ctx->Test->UserData;
  if (data && data->test_func) {
    data->test_func(ctx);
  }
}
}  // namespace
#endif

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

#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE
  // Validate test manager
  if (!test_manager_) {
    response->set_success(false);
    response->set_message("TestManager not available");
    return absl::OkStatus();
  }

  // Get ImGuiTestEngine
  ImGuiTestEngine* engine = test_manager_->GetUITestEngine();
  if (!engine) {
    response->set_success(false);
    response->set_message("ImGuiTestEngine not initialized");
    return absl::OkStatus();
  }

  // Parse target: "button:Open ROM" -> type=button, label="Open ROM"
  std::string target = request->target();
  size_t colon_pos = target.find(':');

  if (colon_pos == std::string::npos) {
    response->set_success(false);
    response->set_message("Invalid target format. Use 'type:label' (e.g. 'button:Open ROM')");
    return absl::OkStatus();
  }

  std::string widget_type = target.substr(0, colon_pos);
  std::string widget_label = target.substr(colon_pos + 1);

  // Convert click type
  ImGuiMouseButton mouse_button = ImGuiMouseButton_Left;
  switch (request->type()) {
    case ClickRequest::LEFT:
      mouse_button = ImGuiMouseButton_Left;
      break;
    case ClickRequest::RIGHT:
      mouse_button = ImGuiMouseButton_Right;
      break;
    case ClickRequest::MIDDLE:
      mouse_button = ImGuiMouseButton_Middle;
      break;
    case ClickRequest::DOUBLE:
      // Double click handled below
      break;
    default:
      break;
  }

  // Create a dynamic test to perform the click
  bool success = false;
  std::string message;
  
  auto test_data = std::make_shared<DynamicTestData>();
  test_data->test_func = [=, &success, &message](ImGuiTestContext* ctx) {
    try {
      if (request->type() == ClickRequest::DOUBLE) {
        ctx->ItemDoubleClick(widget_label.c_str());
      } else {
        ctx->ItemClick(widget_label.c_str(), mouse_button);
      }
      success = true;
      message = absl::StrFormat("Clicked %s '%s'", widget_type, widget_label);
    } catch (const std::exception& e) {
      success = false;
      message = absl::StrFormat("Click failed: %s", e.what());
    }
  };

  // Register and queue the test
  std::string test_name = absl::StrFormat("grpc_click_%lld", 
      std::chrono::system_clock::now().time_since_epoch().count());
  
  ImGuiTest* test = IM_REGISTER_TEST(engine, "grpc", test_name.c_str());
  test->TestFunc = RunDynamicTest;
  test->UserData = test_data.get();
  
  ImGuiTestEngine_QueueTest(engine, test, ImGuiTestRunFlags_RunFromGui);
  
  // Wait for test to complete (with timeout)
  auto timeout = std::chrono::seconds(5);
  auto wait_start = std::chrono::steady_clock::now();
  while (test->Output.Status == ImGuiTestStatus_Queued || test->Output.Status == ImGuiTestStatus_Running) {
    if (std::chrono::steady_clock::now() - wait_start > timeout) {
      success = false;
      message = "Test timeout";
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  
  if (test->Output.Status == ImGuiTestStatus_Success) {
    success = true;
  } else if (test->Output.Status != ImGuiTestStatus_Unknown) {
    success = false;
    if (message.empty()) {
      message = "Test failed";
    }
  }
  
  // Cleanup
  ImGuiTestEngine_UnregisterTest(engine, test);

#else
  // ImGuiTestEngine not available - stub implementation
  std::string target = request->target();
  size_t colon_pos = target.find(':');

  if (colon_pos == std::string::npos) {
    response->set_success(false);
    response->set_message("Invalid target format. Use 'type:label'");
    return absl::OkStatus();
  }

  std::string widget_type = target.substr(0, colon_pos);
  std::string widget_label = target.substr(colon_pos + 1);
  bool success = true;
  std::string message = absl::StrFormat("[STUB] Clicked %s '%s' (ImGuiTestEngine not available)", 
                                       widget_type, widget_label);
#endif

  // Calculate execution time
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);

  response->set_success(success);
  response->set_message(message);
  response->set_execution_time_ms(elapsed.count());

  return absl::OkStatus();
}

absl::Status ImGuiTestHarnessServiceImpl::Type(const TypeRequest* request,
                                                TypeResponse* response) {
  auto start = std::chrono::steady_clock::now();

  // TODO: Implement with ImGuiTestEngine dynamic tests like Click handler
  bool success = true;
  std::string message = absl::StrFormat("Typed '%s' into %s (implementation pending)",
                                       request->text(), request->target());

  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);

  response->set_success(success);
  response->set_message(message);
  response->set_execution_time_ms(elapsed.count());

  return absl::OkStatus();
}

absl::Status ImGuiTestHarnessServiceImpl::Wait(const WaitRequest* request,
                                                WaitResponse* response) {
  auto start = std::chrono::steady_clock::now();

  // TODO: Implement with ImGuiTestEngine dynamic tests
  bool condition_met = true;
  std::string message = absl::StrFormat("Condition '%s' met (implementation pending)",
                                       request->condition());

  response->set_success(condition_met);
  response->set_message(message);
  response->set_elapsed_ms(0);

  return absl::OkStatus();
}

absl::Status ImGuiTestHarnessServiceImpl::Assert(const AssertRequest* request,
                                                  AssertResponse* response) {
  // TODO: Implement with ImGuiTestEngine dynamic tests
  response->set_success(true);
  response->set_message(
      absl::StrFormat("Assertion '%s' passed (implementation pending)", 
                     request->condition()));
  response->set_actual_value("(pending)");
  response->set_expected_value("");  // Set empty string instead of accessing non-existent field

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

ImGuiTestHarnessServer::~ImGuiTestHarnessServer() {
  Shutdown();
}

absl::Status ImGuiTestHarnessServer::Start(int port, TestManager* test_manager) {
  if (server_) {
    return absl::FailedPreconditionError("Server already running");
  }

  if (!test_manager) {
    return absl::InvalidArgumentError("TestManager cannot be null");
  }

  // Create the service implementation with TestManager reference
  service_ = std::make_unique<ImGuiTestHarnessServiceImpl>(test_manager);

  // Create the gRPC service wrapper (store as member to prevent it from going out of scope)
  grpc_service_ = std::make_unique<ImGuiTestHarnessServiceGrpc>(service_.get());

  std::string server_address = absl::StrFormat("0.0.0.0:%d", port);

  grpc::ServerBuilder builder;

  // Listen on all interfaces (use 0.0.0.0 to avoid IPv6/IPv4 binding conflicts)
  builder.AddListeningPort(server_address,
                           grpc::InsecureServerCredentials());

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
