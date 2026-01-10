#pragma once

#ifdef YAZE_WITH_GRPC

#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "util/grpc_win_compat.h"

#include <grpcpp/server.h>

namespace yaze {
class Rom;
}

namespace yaze::emu {
class Emulator;
}

namespace yaze::agent {

class AgentControlServer {
 public:
  using RomGetter = std::function<Rom*()>;
  using RomLoader = std::function<bool(const std::string& path)>;

  AgentControlServer(yaze::emu::Emulator* emulator,
                     RomGetter rom_getter = nullptr,
                     RomLoader rom_loader = nullptr);
  ~AgentControlServer();

  void Start();
  void Stop();

 private:
  void Run();

  yaze::emu::Emulator* emulator_;  // Non-owning pointer
  RomGetter rom_getter_;
  RomLoader rom_loader_;
  std::unique_ptr<grpc::Server> server_;
  std::thread server_thread_;
};

}  // namespace yaze::agent

#endif  // YAZE_WITH_GRPC
