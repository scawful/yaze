#include "cli/service/agent/agent_control_server.h"

#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <iostream>

#include "cli/service/agent/emulator_service_impl.h"

namespace yaze::agent {

AgentControlServer::AgentControlServer(yaze::emu::Emulator* emulator)
    : emulator_(emulator) {}

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
  std::string server_address("0.0.0.0:50051");
  yaze::net::EmulatorServiceImpl service(emulator_, nullptr);

  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  server_ = builder.BuildAndStart();
  if (server_) {
    std::cout << "AgentControlServer listening on " << server_address
              << std::endl;
    server_->Wait();
  } else {
    std::cerr << "Failed to start AgentControlServer on " << server_address
              << std::endl;
  }
}

}  // namespace yaze::agent
