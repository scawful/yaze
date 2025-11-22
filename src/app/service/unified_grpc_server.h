#ifndef YAZE_APP_CORE_SERVICE_UNIFIED_GRPC_SERVER_H_
#define YAZE_APP_CORE_SERVICE_UNIFIED_GRPC_SERVER_H_

#ifdef YAZE_WITH_GRPC

#include <memory>

#include "absl/status/status.h"
#include "app/net/rom_version_manager.h"

// Include grpcpp for grpc::Service forward declaration
#include <grpcpp/impl/service_type.h>

namespace grpc {
class Server;
}

namespace yaze {

// Forward declarations
class CanvasAutomationServiceImpl;

class Rom;
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
 *
 * This server combines multiple automation services for the Zelda editor:
 * 1. ImGuiTestHarness - GUI test automation (widget discovery, screenshots,
 * etc.)
 * 2. RomService - ROM manipulation (read/write, proposals, version management)
 * 3. CanvasAutomation - Canvas operations (tiles, selection, zoom, pan)
 *
 * All services share the same gRPC server instance and port, allowing
 * clients (CLI, AI agents, remote scripts) to interact with GUI, ROM data,
 * and canvas operations simultaneously.
 *
 * Example usage:
 * ```cpp
 * YazeGRPCServer server;
 * server.Initialize(50051, test_manager, rom, version_mgr, approval_mgr,
 * canvas_service); server.Start();
 * // ... do work ...
 * server.Shutdown();
 * ```
 */
class YazeGRPCServer {
 public:
  /**
   * @brief Configuration for the unified server
   */
  struct Config {
    int port = 50051;
    bool enable_test_harness = true;
    bool enable_rom_service = true;
    bool enable_canvas_automation = true;
    bool require_approval_for_rom_writes = true;
  };

  YazeGRPCServer();
  // Destructor must be defined in .cc file to allow deletion of incomplete
  // types
  ~YazeGRPCServer();

  /**
   * @brief Initialize the server with all required services
   * @param port Port to listen on (default 50051)
   * @param test_manager TestManager for GUI automation (optional)
   * @param rom ROM instance for ROM service (optional)
   * @param version_mgr Version manager for ROM snapshots (optional)
   * @param approval_mgr Approval manager for proposals (optional)
   * @param canvas_service Canvas automation service implementation (optional)
   * @return OK status if initialized successfully
   */
  absl::Status Initialize(
      int port, test::TestManager* test_manager = nullptr, Rom* rom = nullptr,
      net::RomVersionManager* version_mgr = nullptr,
      net::ProposalApprovalManager* approval_mgr = nullptr,
      CanvasAutomationServiceImpl* canvas_service = nullptr);

  /**
   * @brief Start the gRPC server (blocking)
   * Starts the server and blocks until Shutdown() is called
   */
  absl::Status Start();

  /**
   * @brief Start the server in a background thread (non-blocking)
   * Returns immediately after starting the server
   */
  absl::Status StartAsync();

  /**
   * @brief Shutdown the server gracefully
   */
  void Shutdown();

  /**
   * @brief Check if server is currently running
   */
  bool IsRunning() const;

  /**
   * @brief Get the port the server is listening on
   */
  int Port() const { return config_.port; }

  /**
   * @brief Update configuration (must be called before Start)
   */
  void SetConfig(const Config& config) { config_ = config; }

 private:
  Config config_;
  std::unique_ptr<grpc::Server> server_;
  std::unique_ptr<test::ImGuiTestHarnessServiceImpl> test_harness_service_;
  std::unique_ptr<net::RomServiceImpl> rom_service_;
  std::unique_ptr<CanvasAutomationServiceImpl> canvas_service_;
  // Store as base grpc::Service* to avoid incomplete type issues
  std::unique_ptr<grpc::Service> canvas_grpc_service_;
  bool is_running_;

  // Build the gRPC server with all services
  absl::Status BuildServer();
};

}  // namespace yaze

#endif  // YAZE_WITH_GRPC
#endif  // YAZE_APP_CORE_SERVICE_UNIFIED_GRPC_SERVER_H_

// Backwards compatibility alias
#ifdef YAZE_WITH_GRPC
namespace yaze {
using UnifiedGRPCServer = YazeGRPCServer;
}
#endif
