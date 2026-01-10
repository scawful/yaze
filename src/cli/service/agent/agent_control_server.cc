#include "cli/service/agent/agent_control_server.h"

#include "util/grpc_win_compat.h"

#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <iostream>

#include "cli/service/agent/emulator_service_impl.h"

namespace yaze::agent {

AgentControlServer::AgentControlServer(yaze::emu::Emulator* emulator,
                                       RomGetter rom_getter,
                                       RomLoader rom_loader)
    : emulator_(emulator), rom_getter_(rom_getter), rom_loader_(rom_loader) {}

AgentControlServer::~AgentControlServer() {
  Stop();
}

void AgentControlServer::Start() {
  server_thread_ = std::thread(&AgentControlServer::Run, this);
}

void AgentControlServer::Stop() {
  if (server_) {
    server_->Shutdown();
  }
  if (server_thread_.joinable()) {
    server_thread_.join();
  }
}

void AgentControlServer::Run() {
  // Port 50053 for agent/MCP debugging (50051 often occupied on macOS)
  std::string server_address("0.0.0.0:50053");
  yaze::net::EmulatorServiceImpl service(emulator_, rom_getter_, rom_loader_);

  grpc::ServerBuilder builder;

  // Track selected port for debugging
  int selected_port = 0;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials(),
                           &selected_port);
  builder.RegisterService(&service);

  server_ = builder.BuildAndStart();
  if (server_ && selected_port > 0) {
    std::cout << "AgentControlServer listening on " << server_address
              << " (selected_port: " << selected_port << ")" << std::endl;
    server_->Wait();
  } else {
    std::cerr << "Failed to start AgentControlServer on " << server_address
              << " (selected_port: " << selected_port << ")" << std::endl;
  }
}

}  // namespace yaze::agent
