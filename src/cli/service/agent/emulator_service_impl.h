#pragma once

#include <grpcpp/grpcpp.h>

#include "protos/emulator_service.grpc.pb.h"

// Forward declaration to avoid circular dependencies
namespace yaze::emu {
class Emulator;
}

namespace yaze::agent {

class EmulatorServiceImpl final : public EmulatorService::Service {
 public:
  explicit EmulatorServiceImpl(yaze::emu::Emulator* emulator);

  // --- Lifecycle ---
  grpc::Status Start(grpc::ServerContext* context, const Empty* request,
                     CommandResponse* response) override;
  grpc::Status Stop(grpc::ServerContext* context, const Empty* request,
                    CommandResponse* response) override;
  grpc::Status Pause(grpc::ServerContext* context, const Empty* request,
                     CommandResponse* response) override;
  grpc::Status Resume(grpc::ServerContext* context, const Empty* request,
                      CommandResponse* response) override;
  grpc::Status Reset(grpc::ServerContext* context, const Empty* request,
                     CommandResponse* response) override;

  // --- Input Control ---
  grpc::Status PressButtons(grpc::ServerContext* context,
                            const ButtonRequest* request,
                            CommandResponse* response) override;
  grpc::Status ReleaseButtons(grpc::ServerContext* context,
                              const ButtonRequest* request,
                              CommandResponse* response) override;
  grpc::Status HoldButtons(grpc::ServerContext* context,
                           const ButtonHoldRequest* request,
                           CommandResponse* response) override;

  // --- State Inspection ---
  grpc::Status GetGameState(grpc::ServerContext* context,
                            const GameStateRequest* request,
                            GameStateResponse* response) override;
  grpc::Status ReadMemory(grpc::ServerContext* context,
                          const MemoryRequest* request,
                          MemoryResponse* response) override;
  grpc::Status WriteMemory(grpc::ServerContext* context,
                           const MemoryWriteRequest* request,
                           CommandResponse* response) override;

  // --- Advanced Debugging ---
  // Breakpoints
  grpc::Status AddBreakpoint(grpc::ServerContext* context,
                             const BreakpointRequest* request,
                             BreakpointResponse* response) override;
  grpc::Status RemoveBreakpoint(grpc::ServerContext* context,
                                const BreakpointIdRequest* request,
                                CommandResponse* response) override;
  grpc::Status ListBreakpoints(grpc::ServerContext* context,
                               const Empty* request,
                               BreakpointListResponse* response) override;
  grpc::Status SetBreakpointEnabled(grpc::ServerContext* context,
                                    const BreakpointStateRequest* request,
                                    CommandResponse* response) override;

  // Watchpoints (memory access tracking)
  grpc::Status AddWatchpoint(grpc::ServerContext* context,
                             const WatchpointRequest* request,
                             WatchpointResponse* response) override;
  grpc::Status RemoveWatchpoint(grpc::ServerContext* context,
                                const WatchpointIdRequest* request,
                                CommandResponse* response) override;
  grpc::Status ListWatchpoints(grpc::ServerContext* context,
                               const Empty* request,
                               WatchpointListResponse* response) override;
  grpc::Status GetWatchpointHistory(
      grpc::ServerContext* context, const WatchpointHistoryRequest* request,
      WatchpointHistoryResponse* response) override;

  // Execution Control
  grpc::Status StepInstruction(grpc::ServerContext* context,
                               const Empty* request,
                               StepResponse* response) override;
  grpc::Status RunToBreakpoint(grpc::ServerContext* context,
                               const Empty* request,
                               BreakpointHitResponse* response) override;
  grpc::Status StepOver(grpc::ServerContext* context, const Empty* request,
                        StepResponse* response) override;
  grpc::Status StepOut(grpc::ServerContext* context, const Empty* request,
                       StepResponse* response) override;

  // Disassembly & Code Analysis
  grpc::Status GetDisassembly(grpc::ServerContext* context,
                              const DisassemblyRequest* request,
                              DisassemblyResponse* response) override;
  grpc::Status GetExecutionTrace(grpc::ServerContext* context,
                                 const TraceRequest* request,
                                 TraceResponse* response) override;

  // Symbol Management
  grpc::Status LoadSymbols(grpc::ServerContext* context,
                           const SymbolFileRequest* request,
                           CommandResponse* response) override;
  grpc::Status ResolveSymbol(grpc::ServerContext* context,
                             const SymbolLookupRequest* request,
                             SymbolLookupResponse* response) override;
  grpc::Status GetSymbolAt(grpc::ServerContext* context,
                           const AddressRequest* request,
                           SymbolLookupResponse* response) override;

  // Debugging Session
  grpc::Status CreateDebugSession(grpc::ServerContext* context,
                                  const DebugSessionRequest* request,
                                  DebugSessionResponse* response) override;
  grpc::Status GetDebugStatus(grpc::ServerContext* context,
                              const Empty* request,
                              DebugStatusResponse* response) override;

 private:
  yaze::emu::Emulator*
      emulator_;  // Non-owning pointer to the emulator instance
};

}  // namespace yaze::agent
