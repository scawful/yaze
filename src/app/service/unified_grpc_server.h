#ifndef YAZE_APP_CORE_SERVICE_UNIFIED_GRPC_SERVER_H_
#define YAZE_APP_CORE_SERVICE_UNIFIED_GRPC_SERVER_H_

#ifdef YAZE_WITH_GRPC

#include <functional>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "app/net/rom_version_manager.h"

// gRPC types used by the server interface.
#include <grpcpp/impl/service_type.h>
#include <grpcpp/server.h>

namespace yaze {

// Forward declarations
class CanvasAutomationServiceImpl;
class Rom;

namespace editor {
class EditorManager;
}

namespace net {
class ProposalApprovalManager;
class RomServiceImpl;
}  // namespace net

namespace test {
class TestManager;
class ImGuiTestHarnessServiceImpl;
}  // namespace test

/**
 * @class YazeGRPCServer
 * @brief YAZE's unified gRPC server for Zelda3 editor automation
 */
class YazeGRPCServer {
 public:
  using RomGetter = std::function<Rom*()>;

  struct Config {
    int port = 50051;
    bool enable_test_harness = true;
    bool enable_rom_service = true;
    bool enable_canvas_automation = true;
    bool require_approval_for_rom_writes = true;
  };

  YazeGRPCServer();
  ~YazeGRPCServer();

  /**
   * @brief Initialize the server with all required services
   * @param port Port to listen on
   * @param test_manager TestManager for GUI automation
   * @param rom_getter Function to retrieve active ROM
   * @param version_mgr Version manager for ROM snapshots
   * @param approval_mgr Approval manager for proposals
   * @param canvas_service Canvas automation service implementation
   * @return OK status if initialized successfully
   */
  absl::Status Initialize(
      int port, test::TestManager* test_manager = nullptr,
      RomGetter rom_getter = nullptr,
      net::RomVersionManager* version_mgr = nullptr,
      net::ProposalApprovalManager* approval_mgr = nullptr,
      CanvasAutomationServiceImpl* canvas_service = nullptr);

  absl::Status Start();
  absl::Status StartAsync();
  absl::Status AddService(std::unique_ptr<grpc::Service> service);
  void Shutdown();
  bool IsRunning() const;
  int Port() const { return config_.port; }
  void SetConfig(const Config& config) { config_ = config; }

 private:
  Config config_;
  std::unique_ptr<grpc::Server> server_;
  std::unique_ptr<test::ImGuiTestHarnessServiceImpl> test_harness_service_;
  std::unique_ptr<net::RomServiceImpl> rom_service_;
  CanvasAutomationServiceImpl* canvas_service_ = nullptr;
  std::unique_ptr<grpc::Service> canvas_grpc_service_;
  std::unique_ptr<grpc::Service> test_harness_grpc_wrapper_;
  std::vector<std::unique_ptr<grpc::Service>> extra_services_;
  bool is_running_;

  absl::Status BuildServer();
};

}  // namespace yaze

#endif  // YAZE_WITH_GRPC
#endif  // YAZE_APP_CORE_SERVICE_UNIFIED_GRPC_SERVER_H_
