#ifndef YAZE_APP_CORE_SERVICE_UNIFIED_GRPC_SERVER_H_
#define YAZE_APP_CORE_SERVICE_UNIFIED_GRPC_SERVER_H_

#ifdef YAZE_WITH_GRPC

#include <functional>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "app/net/rom_version_manager.h"
#include "util/grpc_win_compat.h"

// gRPC types used by the server interface.
#include <grpcpp/impl/service_type.h>
#include <grpcpp/server.h>

#include "app/service/visual_service_impl.h"
#include "app/emu/i_emulator.h"

namespace yaze {

// Forward declarations
class CanvasAutomationServiceImpl;
class Rom;

namespace emu {
// class Emulator; // Forward decl handled by i_emulator.h if needed, or unnecessary now
}

namespace editor {
class EditorManager;
}

namespace net {
class ProposalApprovalManager;
class RomServiceImpl;
class EmulatorServiceImpl;
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
  using RomLoader = std::function<bool(const std::string& path)>;

  struct Config {
    int port = 50052;  // Unified server port
    bool enable_test_harness = true;
    bool enable_rom_service = true;
    bool enable_emulator_service = true;
    bool enable_canvas_automation = true;
    bool require_approval_for_rom_writes = true;
  };

  YazeGRPCServer();
  ~YazeGRPCServer();

  /**
   * @brief Initialize the server with all required services
   * @param port Port to listen on
   * @param emulator Pointer to the emulator instance
   * @param rom_getter Function to retrieve active ROM
   * @param rom_loader Function to load a ROM
   * @param test_manager TestManager for GUI automation
   * @param version_mgr Version manager for ROM snapshots
   * @param approval_mgr Approval manager for proposals
   * @param canvas_service Canvas automation service implementation
   * @return OK status if initialized successfully
   */
  absl::Status Initialize(
      int port,
      emu::IEmulator* emulator = nullptr,
      RomGetter rom_getter = nullptr,
      RomLoader rom_loader = nullptr,
      test::TestManager* test_manager = nullptr,
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

  // Services
  std::unique_ptr<test::ImGuiTestHarnessServiceImpl> test_harness_service_;
  std::unique_ptr<net::RomServiceImpl> rom_service_;
  std::unique_ptr<net::EmulatorServiceImpl> emulator_service_;
  CanvasAutomationServiceImpl* canvas_service_ = nullptr;

  // GRPC Wrappers
  std::unique_ptr<grpc::Service> canvas_grpc_service_;
  std::unique_ptr<grpc::Service> test_harness_grpc_wrapper_;
  std::vector<std::unique_ptr<grpc::Service>> extra_services_;

  bool is_running_;

  absl::Status BuildServer();
};

}  // namespace yaze

#endif  // YAZE_WITH_GRPC
#endif  // YAZE_APP_CORE_SERVICE_UNIFIED_GRPC_SERVER_H_
