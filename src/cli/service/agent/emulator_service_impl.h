#pragma once

#include <grpcpp/grpcpp.h>
#include <memory>

#include "app/emu/debug/step_controller.h"
#include "app/emu/debug/symbol_provider.h"
#include "protos/emulator_service.grpc.pb.h"

namespace yaze {
class Rom;
namespace emu {
class Emulator;
}
}

namespace yaze::net {

class EmulatorServiceImpl final : public agent::EmulatorService::Service {
 public:
  using RomGetter = std::function<Rom*()>;
  using RomLoader = std::function<bool(const std::string& path)>;
  explicit EmulatorServiceImpl(yaze::emu::Emulator* emulator,
                               RomGetter rom_getter = nullptr,
                               RomLoader rom_loader = nullptr);

  // --- ROM Loading ---
  grpc::Status LoadRom(grpc::ServerContext* context,
                       const agent::LoadRomRequest* request,
                       agent::LoadRomResponse* response) override;
  grpc::Status GetLoadedRomPath(grpc::ServerContext* context,
                                const agent::Empty* request,
                                agent::LoadedRomPathResponse* response) override;

  // --- Core Lifecycle & Control ---
  grpc::Status ControlEmulator(grpc::ServerContext* context, 
                               const agent::ControlRequest* request,
                               agent::CommandResponse* response) override;
  
  grpc::Status StepEmulator(grpc::ServerContext* context, 
                            const agent::StepControlRequest* request,
                            agent::StepResponse* response) override;
  
  grpc::Status RunToBreakpoint(grpc::ServerContext* context, 
                               const agent::Empty* request,
                               agent::BreakpointHitResponse* response) override;

  // --- Input & State ---
  grpc::Status PressButtons(grpc::ServerContext* context,
                            const agent::ButtonRequest* request,
                            agent::CommandResponse* response) override;
  grpc::Status ReleaseButtons(grpc::ServerContext* context,
                              const agent::ButtonRequest* request,
                              agent::CommandResponse* response) override;
  grpc::Status HoldButtons(grpc::ServerContext* context,
                           const agent::ButtonHoldRequest* request,
                           agent::CommandResponse* response) override;
  grpc::Status GetGameState(grpc::ServerContext* context,
                            const agent::GameStateRequest* request,
                            agent::GameStateResponse* response) override;
  grpc::Status ReadMemory(grpc::ServerContext* context,
                          const agent::MemoryRequest* request,
                          agent::MemoryResponse* response) override;
  grpc::Status WriteMemory(grpc::ServerContext* context,
                           const agent::MemoryWriteRequest* request,
                           agent::CommandResponse* response) override;

  // --- Debugging Management ---
  grpc::Status BreakpointControl(grpc::ServerContext* context,
                                 const agent::BreakpointControlRequest* request,
                                 agent::BreakpointControlResponse* response) override;
  
  grpc::Status WatchpointControl(grpc::ServerContext* context,
                                 const agent::WatchpointControlRequest* request,
                                 agent::WatchpointControlResponse* response) override;

  // --- Analysis & Symbols ---
  grpc::Status GetDisassembly(grpc::ServerContext* context,
                              const agent::DisassemblyRequest* request,
                              agent::DisassemblyResponse* response) override;
  grpc::Status GetExecutionTrace(grpc::ServerContext* context,
                                 const agent::TraceRequest* request,
                                 agent::TraceResponse* response) override;
  grpc::Status ResolveSymbol(grpc::ServerContext* context,
                             const agent::SymbolLookupRequest* request,
                             agent::SymbolLookupResponse* response) override;
  grpc::Status GetSymbolAt(grpc::ServerContext* context,
                           const agent::AddressRequest* request,
                           agent::SymbolLookupResponse* response) override;
  grpc::Status LoadSymbols(grpc::ServerContext* context,
                           const agent::SymbolFileRequest* request,
                           agent::CommandResponse* response) override;

  // --- Session & Experiments ---
  grpc::Status GetDebugStatus(grpc::ServerContext* context,
                              const agent::Empty* request,
                              agent::DebugStatusResponse* response) override;
  grpc::Status TestRun(grpc::ServerContext* context,
                       const agent::TestRunRequest* request,
                       agent::TestRunResponse* response) override;

 private:
  void InitializeStepController();
  void CaptureCPUState(agent::CPUState* state);

  yaze::emu::Emulator*
      emulator_;  // Non-owning pointer to the emulator instance
  RomGetter rom_getter_;
  RomLoader rom_loader_;
  yaze::emu::debug::SymbolProvider symbol_provider_;  // Symbol table for debugging

  yaze::emu::debug::StepController step_controller_;
};

}  // namespace yaze::net
