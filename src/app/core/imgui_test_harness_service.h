#ifndef YAZE_APP_CORE_IMGUI_TEST_HARNESS_SERVICE_H_
#define YAZE_APP_CORE_IMGUI_TEST_HARNESS_SERVICE_H_

#ifdef YAZE_WITH_GRPC

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/core/widget_discovery_service.h"

// Include grpcpp headers for unique_ptr<Server> in member variable
#include <grpcpp/server.h>

// Forward declarations to avoid including gRPC headers in public interface
namespace grpc {
class ServerContext;
}  // namespace grpc

namespace yaze {
namespace test {

// Forward declare TestManager
class TestManager;

// Forward declare proto types
class PingRequest;
class PingResponse;
class ClickRequest;
class ClickResponse;
class TypeRequest;
class TypeResponse;
class WaitRequest;
class WaitResponse;
class AssertRequest;
class AssertResponse;
class ScreenshotRequest;
class ScreenshotResponse;
class GetTestStatusRequest;
class GetTestStatusResponse;
class ListTestsRequest;
class ListTestsResponse;
class GetTestResultsRequest;
class GetTestResultsResponse;
class DiscoverWidgetsRequest;
class DiscoverWidgetsResponse;

// Implementation of ImGuiTestHarness gRPC service
// This class provides the actual RPC handlers for automated GUI testing
class ImGuiTestHarnessServiceImpl {
 public:
  // Constructor now takes TestManager reference for ImGuiTestEngine access
  explicit ImGuiTestHarnessServiceImpl(TestManager* test_manager)
      : test_manager_(test_manager) {}
  ~ImGuiTestHarnessServiceImpl() = default;

  // Disable copy and move
  ImGuiTestHarnessServiceImpl(const ImGuiTestHarnessServiceImpl&) = delete;
  ImGuiTestHarnessServiceImpl& operator=(const ImGuiTestHarnessServiceImpl&) =
      delete;

  // RPC Handlers - implemented in imgui_test_harness_service.cc

  // Health check - verifies the service is running
  absl::Status Ping(const PingRequest* request, PingResponse* response);

  // Click a button or interactive element
  absl::Status Click(const ClickRequest* request, ClickResponse* response);

  // Type text into an input field
  absl::Status Type(const TypeRequest* request, TypeResponse* response);

  // Wait for a condition to be true
  absl::Status Wait(const WaitRequest* request, WaitResponse* response);

  // Assert that a condition is true
  absl::Status Assert(const AssertRequest* request, AssertResponse* response);

  // Capture a screenshot
  absl::Status Screenshot(const ScreenshotRequest* request,
                          ScreenshotResponse* response);

  // Test introspection APIs
  absl::Status GetTestStatus(const GetTestStatusRequest* request,
                             GetTestStatusResponse* response);
  absl::Status ListTests(const ListTestsRequest* request,
                         ListTestsResponse* response);
  absl::Status GetTestResults(const GetTestResultsRequest* request,
                              GetTestResultsResponse* response);
  absl::Status DiscoverWidgets(const DiscoverWidgetsRequest* request,
                               DiscoverWidgetsResponse* response);

 private:
  TestManager* test_manager_;  // Non-owning pointer to access ImGuiTestEngine
  WidgetDiscoveryService widget_discovery_service_;
};

// Forward declaration of the gRPC service wrapper
class ImGuiTestHarnessServiceGrpc;

// Singleton server managing the gRPC service
// This class manages the lifecycle of the gRPC server
class ImGuiTestHarnessServer {
 public:
  // Get the singleton instance
  static ImGuiTestHarnessServer& Instance();

  // Start the gRPC server on the specified port
  // @param port The port to listen on (default 50051)
  // @param test_manager Pointer to TestManager for ImGuiTestEngine access
  // @return OK status if server started successfully, error otherwise
  absl::Status Start(int port, TestManager* test_manager);

  // Shutdown the server gracefully
  void Shutdown();

  // Check if the server is currently running
  bool IsRunning() const { return server_ != nullptr; }

  // Get the port the server is listening on (0 if not running)
  int Port() const { return port_; }

 private:
  ImGuiTestHarnessServer() = default;
  ~ImGuiTestHarnessServer();  // Defined in .cc file to allow incomplete type deletion

  // Disable copy and move
  ImGuiTestHarnessServer(const ImGuiTestHarnessServer&) = delete;
  ImGuiTestHarnessServer& operator=(const ImGuiTestHarnessServer&) = delete;

  std::unique_ptr<grpc::Server> server_;
  std::unique_ptr<ImGuiTestHarnessServiceImpl> service_;
  std::unique_ptr<ImGuiTestHarnessServiceGrpc> grpc_service_;
  int port_ = 0;
};

}  // namespace test
}  // namespace yaze

#endif  // YAZE_WITH_GRPC
#endif  // YAZE_APP_CORE_IMGUI_TEST_HARNESS_SERVICE_H_
