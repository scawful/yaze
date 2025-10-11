#pragma once

#include <memory>
#include <thread>

namespace grpc {
class Server;
}

namespace yaze::emu {
class Emulator;
}

namespace yaze::agent {

class AgentControlServer {
public:
    AgentControlServer(yaze::emu::Emulator* emulator);
    ~AgentControlServer();

    void Start();
    void Stop();

private:
    void Run();

    yaze::emu::Emulator* emulator_; // Non-owning pointer
    std::unique_ptr<grpc::Server> server_;
    std::thread server_thread_;
};

} // namespace yaze::agent
