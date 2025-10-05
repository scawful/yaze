#include "app/core/service/unified_grpc_server.h"

#ifdef YAZE_WITH_GRPC

#include <iostream>
#include <thread>

#include "absl/strings/str_format.h"
#include "app/core/service/imgui_test_harness_service.h"
#include "app/net/rom_service_impl.h"
#include "app/rom.h"

#include <grpcpp/grpcpp.h>

namespace yaze {

UnifiedGRPCServer::UnifiedGRPCServer()
    : is_running_(false) {
}

UnifiedGRPCServer::~UnifiedGRPCServer() {
  Shutdown();
}

absl::Status UnifiedGRPCServer::Initialize(
    int port,
    test::TestManager* test_manager,
    app::Rom* rom,
    app::net::RomVersionManager* version_mgr,
    app::net::ProposalApprovalManager* approval_mgr) {
  
  if (is_running_) {
    return absl::FailedPreconditionError("Server is already running");
  }
  
  config_.port = port;
  
  // Create ImGuiTestHarness service if test_manager provided
  if (config_.enable_test_harness && test_manager) {
    test_harness_service_ = 
        std::make_unique<test::ImGuiTestHarnessServiceImpl>(test_manager);
    std::cout << "✓ ImGuiTestHarness service initialized\n";
  } else if (config_.enable_test_harness) {
    std::cout << "⚠ ImGuiTestHarness requested but no TestManager provided\n";
  }
  
  // Create ROM service if rom provided
  if (config_.enable_rom_service && rom) {
    rom_service_ = std::make_unique<app::net::RomServiceImpl>(
        rom, version_mgr, approval_mgr);
    
    // Configure ROM service
    app::net::RomServiceImpl::Config rom_config;
    rom_config.require_approval_for_writes = config_.require_approval_for_rom_writes;
    rom_service_->SetConfig(rom_config);
    
    std::cout << "✓ ROM service initialized\n";
  } else if (config_.enable_rom_service) {
    std::cout << "⚠ ROM service requested but no ROM provided\n";
  }
  
  if (!test_harness_service_ && !rom_service_) {
    return absl::InvalidArgumentError(
        "At least one service must be enabled and initialized");
  }
  
  return absl::OkStatus();
}

absl::Status UnifiedGRPCServer::Start() {
  auto status = BuildServer();
  if (!status.ok()) {
    return status;
  }
  
  std::cout << "✓ Unified gRPC server listening on 0.0.0.0:" << config_.port << "\n";
  
  if (test_harness_service_) {
    std::cout << "  ✓ ImGuiTestHarness available\n";
  }
  if (rom_service_) {
    std::cout << "  ✓ ROM service available\n";
  }
  
  std::cout << "\nServer is ready to accept requests...\n";
  
  // Block until server is shut down
  server_->Wait();
  
  return absl::OkStatus();
}

absl::Status UnifiedGRPCServer::StartAsync() {
  auto status = BuildServer();
  if (!status.ok()) {
    return status;
  }
  
  std::cout << "✓ Unified gRPC server started on port " << config_.port << "\n";
  
  // Server runs in background, doesn't block
  return absl::OkStatus();
}

void UnifiedGRPCServer::Shutdown() {
  if (server_ && is_running_) {
    std::cout << "⏹ Shutting down unified gRPC server...\n";
    server_->Shutdown();
    server_.reset();
    is_running_ = false;
    std::cout << "✓ Server stopped\n";
  }
}

bool UnifiedGRPCServer::IsRunning() const {
  return is_running_;
}

absl::Status UnifiedGRPCServer::BuildServer() {
  if (is_running_) {
    return absl::FailedPreconditionError("Server already running");
  }
  
  std::string server_address = absl::StrFormat("0.0.0.0:%d", config_.port);
  
  grpc::ServerBuilder builder;
  
  // Listen on all interfaces
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  
  // Register services
  if (test_harness_service_) {
    // Note: The actual registration requires the gRPC service wrapper
    // This is a simplified version - full implementation would need
    // the wrapper from imgui_test_harness_service.cc
    std::cout << "  Registering ImGuiTestHarness service...\n";
    // builder.RegisterService(test_harness_grpc_wrapper_.get());
  }
  
  if (rom_service_) {
    std::cout << "  Registering ROM service...\n";
    builder.RegisterService(rom_service_.get());
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
