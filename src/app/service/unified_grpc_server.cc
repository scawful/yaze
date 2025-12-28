#include "app/service/unified_grpc_server.h"

#ifdef YAZE_WITH_GRPC

#include <grpcpp/grpcpp.h>

#include <iostream>
#include <thread>

#include "absl/strings/str_format.h"
#include "app/service/rom_service_impl.h"
#include "rom/rom.h"
#include "app/service/canvas_automation_service.h"
#include "app/service/imgui_test_harness_service.h"
#include "protos/canvas_automation.grpc.pb.h"

#include "app/editor/editor_manager.h"

namespace yaze {

YazeGRPCServer::YazeGRPCServer() : is_running_(false) {}

YazeGRPCServer::~YazeGRPCServer() {
  Shutdown();
}

absl::Status YazeGRPCServer::Initialize(
    int port, test::TestManager* test_manager, 
    RomGetter rom_getter,
    net::RomVersionManager* version_mgr,
    net::ProposalApprovalManager* approval_mgr,
    CanvasAutomationServiceImpl* canvas_service) {
  if (is_running_) {
    return absl::FailedPreconditionError("Server is already running");
  }

  config_.port = port;

  // Create ImGuiTestHarness service if test_manager provided
  if (config_.enable_test_harness && test_manager) {
    test_harness_service_ =
        std::make_unique<test::ImGuiTestHarnessServiceImpl>(test_manager);
    std::cout << "✓ ImGuiTestHarness service initialized\n";
  }

  // Create ROM service if rom_getter provided
  if (config_.enable_rom_service && rom_getter) {
    rom_service_ =
        std::make_unique<net::RomServiceImpl>(rom_getter, version_mgr, approval_mgr);

    // Configure ROM service
    net::RomServiceImpl::Config rom_config;
    rom_config.require_approval_for_writes =
        config_.require_approval_for_rom_writes;
    rom_service_->SetConfig(rom_config);

    std::cout << "✓ ROM service initialized\n";
  } else if (config_.enable_rom_service) {
    std::cout << "⚠ ROM service requested but no ROM provided\n";
  }

  // Create Canvas Automation service if canvas_service provided
  if (config_.enable_canvas_automation && canvas_service) {
    // Store the provided service (not owned by us)
    canvas_service_ = canvas_service;
    std::cout << "✓ Canvas Automation service initialized\n";
  } else if (config_.enable_canvas_automation) {
    std::cout << "⚠ Canvas Automation requested but no service provided\n";
  }

  if (!test_harness_service_ && !rom_service_ && !canvas_service_) {
    return absl::InvalidArgumentError(
        "At least one service must be enabled and initialized");
  }

  return absl::OkStatus();
}

absl::Status YazeGRPCServer::Start() {
  auto status = BuildServer();
  if (!status.ok()) {
    return status;
  }

  std::cout << "✓ YAZE gRPC automation server listening on 0.0.0.0:"
            << config_.port << "\n";

  if (test_harness_service_) {
    std::cout << "  ✓ ImGuiTestHarness available\n";
  }
  if (rom_service_) {
    std::cout << "  ✓ ROM service available\n";
  }
  if (canvas_service_) {
    std::cout << "  ✓ Canvas Automation available\n";
  }

  std::cout << "\nServer is ready to accept requests...\n";

  // Block until server is shut down
  server_->Wait();

  return absl::OkStatus();
}

absl::Status YazeGRPCServer::StartAsync() {
  auto status = BuildServer();
  if (!status.ok()) {
    return status;
  }

  std::cout << "✓ Unified gRPC server started on port " << config_.port << "\n";

  // Server runs in background, doesn't block
  return absl::OkStatus();
}

absl::Status YazeGRPCServer::AddService(
    std::unique_ptr<grpc::Service> service) {
  if (!service) {
    return absl::InvalidArgumentError("Service is null");
  }
  if (is_running_) {
    return absl::FailedPreconditionError(
        "Cannot add services after the server has started");
  }
  extra_services_.push_back(std::move(service));
  return absl::OkStatus();
}

void YazeGRPCServer::Shutdown() {
  if (server_ && is_running_) {
    std::cout << "⏹ Shutting down unified gRPC server...\n";
    server_->Shutdown();
    server_.reset();
    is_running_ = false;
    std::cout << "✓ Server stopped\n";
  }
}

bool YazeGRPCServer::IsRunning() const {
  return is_running_;
}

absl::Status YazeGRPCServer::BuildServer() {
  if (is_running_) {
    return absl::FailedPreconditionError("Server already running");
  }

  std::string server_address = absl::StrFormat("0.0.0.0:%d", config_.port);

  grpc::ServerBuilder builder;

  // Listen on all interfaces
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

  // Register services
  if (test_harness_service_) {
    // Create gRPC wrapper for the test harness service
    test_harness_grpc_wrapper_ = 
        test::CreateImGuiTestHarnessServiceGrpc(test_harness_service_.get());
    std::cout << "  Registering ImGuiTestHarness service...\n";
    builder.RegisterService(test_harness_grpc_wrapper_.get());
  }

  if (rom_service_) {
    std::cout << "  Registering ROM service...\n";
    builder.RegisterService(rom_service_.get());
  }

  if (canvas_service_) {
    std::cout << "  Registering Canvas Automation service...\n";
    // Create gRPC wrapper using factory function
    canvas_grpc_service_ =
        CreateCanvasAutomationServiceGrpc(canvas_service_);
    builder.RegisterService(canvas_grpc_service_.get());
  }

  for (auto& service : extra_services_) {
    builder.RegisterService(service.get());
  }

  // Build and start
  server_ = builder.BuildAndStart();

  if (!server_) {
    return absl::InternalError(
        absl::StrFormat("Failed to start server on %s", server_address));
  }

  is_running_ = true;

  return absl::OkStatus();
}

}  // namespace yaze

#endif  // YAZE_WITH_GRPC
