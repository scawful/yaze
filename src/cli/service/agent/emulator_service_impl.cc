#include "cli/service/agent/emulator_service_impl.h"
#include "app/emu/emulator.h"
#include "app/core/service/screenshot_utils.h"
#include "app/emu/input/input_backend.h" // Required for SnesButton enum
#include "absl/strings/escaping.h"
#include "absl/strings/str_format.h"
#include <fstream>
#include <thread>

namespace yaze::agent {

namespace {
// Helper to convert our gRPC Button enum to the emulator's SnesButton enum
emu::input::SnesButton ToSnesButton(Button button) {
    using emu::input::SnesButton;
    switch (button) {
        case A: return SnesButton::A;
        case B: return SnesButton::B;
        case X: return SnesButton::X;
        case Y: return SnesButton::Y;
        case L: return SnesButton::L;
        case R: return SnesButton::R;
        case SELECT: return SnesButton::SELECT;
        case START: return SnesButton::START;
        case UP: return SnesButton::UP;
        case DOWN: return SnesButton::DOWN;
        case LEFT: return SnesButton::LEFT;
        case RIGHT: return SnesButton::RIGHT;
        default: 
            return SnesButton::B; // Default fallback
    }
}
} // namespace

EmulatorServiceImpl::EmulatorServiceImpl(yaze::emu::Emulator* emulator)
    : emulator_(emulator) {}

// --- Lifecycle ---

grpc::Status EmulatorServiceImpl::Start(grpc::ServerContext* context, const Empty* request, CommandResponse* response) {
    if (!emulator_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Emulator not initialized.");
    emulator_->set_running(true);
    response->set_success(true);
    response->set_message("Emulator started.");
    return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::Stop(grpc::ServerContext* context, const Empty* request, CommandResponse* response) {
    if (!emulator_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Emulator not initialized.");
    emulator_->set_running(false);
    response->set_success(true);
    response->set_message("Emulator stopped.");
    return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::Pause(grpc::ServerContext* context, const Empty* request, CommandResponse* response) {
    if (!emulator_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Emulator not initialized.");
    emulator_->set_running(false);
    response->set_success(true);
    response->set_message("Emulator paused.");
    return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::Resume(grpc::ServerContext* context, const Empty* request, CommandResponse* response) {
    if (!emulator_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Emulator not initialized.");
    emulator_->set_running(true);
    response->set_success(true);
    response->set_message("Emulator resumed.");
    return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::Reset(grpc::ServerContext* context, const Empty* request, CommandResponse* response) {
    if (!emulator_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Emulator not initialized.");
    emulator_->snes().Reset(true); // Hard reset
    response->set_success(true);
    response->set_message("Emulator reset.");
    return grpc::Status::OK;
}

// --- Input Control ---

grpc::Status EmulatorServiceImpl::PressButtons(grpc::ServerContext* context, const ButtonRequest* request, CommandResponse* response) {
    if (!emulator_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Emulator not initialized.");
    auto& input_manager = emulator_->input_manager();
    for (const auto& button : request->buttons()) {
        input_manager.PressButton(ToSnesButton(static_cast<Button>(button)));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
     for (const auto& button : request->buttons()) {
        input_manager.ReleaseButton(ToSnesButton(static_cast<Button>(button)));
    }
    response->set_success(true);
    return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::ReleaseButtons(grpc::ServerContext* context, const ButtonRequest* request, CommandResponse* response) {
    if (!emulator_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Emulator not initialized.");
    auto& input_manager = emulator_->input_manager();
    for (const auto& button : request->buttons()) {
        input_manager.ReleaseButton(ToSnesButton(static_cast<Button>(button)));
    }
    response->set_success(true);
    return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::HoldButtons(grpc::ServerContext* context, const ButtonHoldRequest* request, CommandResponse* response) {
    if (!emulator_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Emulator not initialized.");
    auto& input_manager = emulator_->input_manager();
    for (const auto& button : request->buttons()) {
        input_manager.PressButton(ToSnesButton(static_cast<Button>(button)));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(request->duration_ms()));
    for (const auto& button : request->buttons()) {
        input_manager.ReleaseButton(ToSnesButton(static_cast<Button>(button)));
    }
    response->set_success(true);
    return grpc::Status::OK;
}

// --- State Inspection ---

grpc::Status EmulatorServiceImpl::GetGameState(grpc::ServerContext* context, const GameStateRequest* request, GameStateResponse* response) {
    if (!emulator_ || !emulator_->is_snes_initialized()) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "SNES is not initialized.");
    }
    auto& memory = emulator_->snes().memory();

    response->set_game_mode(memory.ReadByte(0x7E0010));
    response->set_link_state(memory.ReadByte(0x7E005D));
    response->set_link_pos_x(memory.ReadWord(0x7E0020));
    response->set_link_pos_y(memory.ReadWord(0x7E0022));
    response->set_link_health(memory.ReadByte(0x7EF36D));

    for (const auto& mem_req : request->memory_reads()) {
        auto* mem_resp = response->add_memory_responses();
        mem_resp->set_address(mem_req.address());
        std::vector<uint8_t> data(mem_req.size());
        for (uint32_t i = 0; i < mem_req.size(); ++i) {
            data[i] = memory.ReadByte(mem_req.address() + i);
        }
        mem_resp->set_data(data.data(), data.size());
    }

#ifdef YAZE_WITH_GRPC
    if (request->include_screenshot()) {
        auto screenshot = yaze::test::CaptureHarnessScreenshot();
        if (screenshot.ok()) {
            // Read the screenshot file and convert to PNG data
            std::ifstream file(screenshot->file_path, std::ios::binary);
            if (file.good()) {
                std::string png_data((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
                response->set_screenshot_png(png_data);
            }
        }
    }
#endif

    return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::ReadMemory(grpc::ServerContext* context, const MemoryRequest* request, MemoryResponse* response) {
    if (!emulator_ || !emulator_->is_snes_initialized()) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "SNES is not initialized.");
    }
    auto& memory = emulator_->snes().memory();
    response->set_address(request->address());
    std::vector<uint8_t> data(request->size());
    for (uint32_t i = 0; i < request->size(); ++i) {
        data[i] = memory.ReadByte(request->address() + i);
    }
    response->set_data(data.data(), data.size());
    return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::WriteMemory(grpc::ServerContext* context, const MemoryWriteRequest* request, CommandResponse* response) {
    if (!emulator_ || !emulator_->is_snes_initialized()) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "SNES is not initialized.");
    }
    auto& memory = emulator_->snes().memory();
    const std::string& data = request->data();
    for (uint32_t i = 0; i < data.size(); ++i) {
        memory.WriteByte(request->address() + i, static_cast<uint8_t>(data[i]));
    }
    response->set_success(true);
    response->set_message(absl::StrFormat("Wrote %d bytes to 0x%X.", data.size(), request->address()));
    return grpc::Status::OK;
}

} // namespace yaze::agent
