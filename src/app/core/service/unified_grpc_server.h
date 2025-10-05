#ifndef YAZE_APP_CORE_SERVICE_UNIFIED_GRPC_SERVER_H_
#define YAZE_APP_CORE_SERVICE_UNIFIED_GRPC_SERVER_H_

#ifdef YAZE_WITH_GRPC

#include <memory>

#include "absl/status/status.h"
#include "app/net/rom_version_manager.h"

namespace grpc {
class Server;
}

namespace yaze {

// Forward declarations
namespace app {
class Rom;
namespace net {
class ProposalApprovalManager;
class RomServiceImpl;
}
}

namespace test {
class TestManager;
class ImGuiTestHarnessServiceImpl;
}

/**
 * @class UnifiedGRPCServer
 * @brief Unified gRPC server hosting both ImGuiTestHarness and RomService
 * 
 * This server combines:
 * 1. ImGuiTestHarness - GUI test automation (widget discovery, screenshots, etc.)
 * 2. RomService - ROM manipulation (read/write, proposals, version management)
 * 
 * Both services share the same gRPC server instance and port, allowing
 * clients to interact with both the GUI and ROM data simultaneously.
 * 
 * Example usage:
 * ```cpp
 * UnifiedGRPCServer server;
 * server.Initialize(50051, test_manager, rom, version_mgr, approval_mgr);
 * server.Start();
 * // ... do work ...
 * server.Shutdown();
 * ```
 */
class UnifiedGRPCServer {
 public:
  /**
   * @brief Configuration for the unified server
   */
  struct Config {
    int port = 50051;
    bool enable_test_harness = true;
    bool enable_rom_service = true;
    bool require_approval_for_rom_writes = true;
  };
  
  UnifiedGRPCServer();
  ~UnifiedGRPCServer();
  
  /**
   * @brief Initialize the server with all required services
   * @param port Port to listen on (default 50051)
   * @param test_manager TestManager for GUI automation (optional)
   * @param rom ROM instance for ROM service (optional)
   * @param version_mgr Version manager for ROM snapshots (optional)
   * @param approval_mgr Approval manager for proposals (optional)
   * @return OK status if initialized successfully
   */
  absl::Status Initialize(
      int port,
      test::TestManager* test_manager = nullptr,
      app::Rom* rom = nullptr,
      app::net::RomVersionManager* version_mgr = nullptr,
      app::net::ProposalApprovalManager* approval_mgr = nullptr);
  
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
  std::unique_ptr<app::net::RomServiceImpl> rom_service_;
  bool is_running_;
  
  // Build the gRPC server with both services
  absl::Status BuildServer();
};

}  // namespace yaze

#endif  // YAZE_WITH_GRPC
#endif  // YAZE_APP_CORE_SERVICE_UNIFIED_GRPC_SERVER_H_
