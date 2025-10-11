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
    grpc::Status Start(grpc::ServerContext* context, const Empty* request, CommandResponse* response) override;
    grpc::Status Stop(grpc::ServerContext* context, const Empty* request, CommandResponse* response) override;
    grpc::Status Pause(grpc::ServerContext* context, const Empty* request, CommandResponse* response) override;
    grpc::Status Resume(grpc::ServerContext* context, const Empty* request, CommandResponse* response) override;
    grpc::Status Reset(grpc::ServerContext* context, const Empty* request, CommandResponse* response) override;

    // --- Input Control ---
    grpc::Status PressButtons(grpc::ServerContext* context, const ButtonRequest* request, CommandResponse* response) override;
    grpc::Status ReleaseButtons(grpc::ServerContext* context, const ButtonRequest* request, CommandResponse* response) override;
    grpc::Status HoldButtons(grpc::ServerContext* context, const ButtonHoldRequest* request, CommandResponse* response) override;

    // --- State Inspection ---
    grpc::Status GetGameState(grpc::ServerContext* context, const GameStateRequest* request, GameStateResponse* response) override;
    grpc::Status ReadMemory(grpc::ServerContext* context, const MemoryRequest* request, MemoryResponse* response) override;
    grpc::Status WriteMemory(grpc::ServerContext* context, const MemoryWriteRequest* request, CommandResponse* response) override;

private:
    yaze::emu::Emulator* emulator_; // Non-owning pointer to the emulator instance
};

} // namespace yaze::agent
