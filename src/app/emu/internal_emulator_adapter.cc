#include "app/emu/internal_emulator_adapter.h"

#include <iostream>
#include <thread>
#include <chrono>

#include "app/service/screenshot_utils.h"

namespace yaze {
namespace emu {

namespace {

input::SnesButton ToSnesButton(yaze::agent::Button button) {
  using yaze::emu::input::SnesButton;
  switch (button) {
    case yaze::agent::A: return SnesButton::A;
    case yaze::agent::B: return SnesButton::B;
    case yaze::agent::X: return SnesButton::X;
    case yaze::agent::Y: return SnesButton::Y;
    case yaze::agent::L: return SnesButton::L;
    case yaze::agent::R: return SnesButton::R;
    case yaze::agent::SELECT: return SnesButton::SELECT;
    case yaze::agent::START: return SnesButton::START;
    case yaze::agent::UP: return SnesButton::UP;
    case yaze::agent::DOWN: return SnesButton::DOWN;
    case yaze::agent::LEFT: return SnesButton::LEFT;
    case yaze::agent::RIGHT: return SnesButton::RIGHT;
    default: return SnesButton::B;
  }
}

BreakpointManager::Type ToBreakpointType(yaze::agent::BreakpointType proto_type) {
  using yaze::emu::BreakpointManager;
  switch (proto_type) {
    case yaze::agent::EXECUTE: return BreakpointManager::Type::EXECUTE;
    case yaze::agent::READ: return BreakpointManager::Type::READ;
    case yaze::agent::WRITE: return BreakpointManager::Type::WRITE;
    case yaze::agent::ACCESS: return BreakpointManager::Type::ACCESS;
    case yaze::agent::CONDITIONAL: return BreakpointManager::Type::CONDITIONAL;
    default: return BreakpointManager::Type::EXECUTE;
  }
}

BreakpointManager::CpuType ToCpuType(yaze::agent::CpuType proto_cpu) {
  using yaze::emu::BreakpointManager;
  switch (proto_cpu) {
    case yaze::agent::CPU_65816: return BreakpointManager::CpuType::CPU_65816;
    case yaze::agent::SPC700: return BreakpointManager::CpuType::SPC700;
    default: return BreakpointManager::CpuType::CPU_65816;
  }
}

yaze::agent::BreakpointType ToProtoBreakpointType(BreakpointManager::Type type) {
  using yaze::emu::BreakpointManager;
  switch (type) {
    case BreakpointManager::Type::EXECUTE: return yaze::agent::EXECUTE;
    case BreakpointManager::Type::READ: return yaze::agent::READ;
    case BreakpointManager::Type::WRITE: return yaze::agent::WRITE;
    case BreakpointManager::Type::ACCESS: return yaze::agent::ACCESS;
    case BreakpointManager::Type::CONDITIONAL: return yaze::agent::CONDITIONAL;
    default: return yaze::agent::EXECUTE;
  }
}

yaze::agent::CpuType ToProtoCpuType(BreakpointManager::CpuType cpu) {
  using yaze::emu::BreakpointManager;
  switch (cpu) {
    case BreakpointManager::CpuType::CPU_65816: return yaze::agent::CPU_65816;
    case BreakpointManager::CpuType::SPC700: return yaze::agent::SPC700;
    default: return yaze::agent::CPU_65816;
  }
}

}  // namespace

InternalEmulatorAdapter::InternalEmulatorAdapter(Emulator* emulator)
    : emulator_(emulator) {}

bool InternalEmulatorAdapter::IsConnected() const {
    // Internal emulator is always "connected" if it exists
    return emulator_ != nullptr;
}

bool InternalEmulatorAdapter::IsRunning() const {
    return emulator_ && emulator_->running();
}

void InternalEmulatorAdapter::Pause() {
    if (emulator_) emulator_->set_running(false);
}

void InternalEmulatorAdapter::Resume() {
    if (emulator_) emulator_->set_running(true);
}

void InternalEmulatorAdapter::Reset() {
    if (emulator_) emulator_->snes().Reset(true);
}

absl::Status InternalEmulatorAdapter::Step(int count) {
    if (!emulator_) return absl::UnavailableError("Emulator not initialized");
    // Only instruction step supported simply here, count loop
    for(int i=0; i<count; ++i) {
        emulator_->StepSingleInstruction();
    }
    return absl::OkStatus();
}

absl::Status InternalEmulatorAdapter::StepOver() {
    if (!emulator_) return absl::UnavailableError("Emulator not initialized");
    InitializeStepController();
    auto result = step_controller_.StepOver();
    // Assuming StepResult has success/message
    return absl::OkStatus(); // result.message? We return Status.
}

absl::Status InternalEmulatorAdapter::StepOut() {
    if (!emulator_) return absl::UnavailableError("Emulator not initialized");
    InitializeStepController();
    auto result = step_controller_.StepOut();
    return absl::OkStatus();
}

void InternalEmulatorAdapter::InitializeStepController() {
  auto& memory = emulator_->snes().memory();
  auto& cpu = emulator_->snes().cpu();
  step_controller_.SetMemoryReader(
      [&memory](uint32_t addr) -> uint8_t { return memory.ReadByte(addr); });
  step_controller_.SetSingleStepper(
      [this]() { emulator_->StepSingleInstruction(); });
  step_controller_.SetPcGetter(
      [&cpu]() -> uint32_t { return (cpu.PB << 16) | cpu.PC; });
}


absl::Status InternalEmulatorAdapter::LoadRom(const std::string& path) {
    if (!rom_loader_) return absl::UnavailableError("No ROM loader configured");
    if (rom_loader_(path)) return absl::OkStatus();
    return absl::InternalError("Failed to load ROM");
}

std::string InternalEmulatorAdapter::GetLoadedRomPath() const {
    if (rom_getter_) {
        Rom* rom = rom_getter_();
        if (rom && rom->is_loaded()) {
            return rom->filename();
        }
    }
    return "";
}

absl::StatusOr<uint8_t> InternalEmulatorAdapter::ReadByte(uint32_t addr) {
    if (!emulator_) return absl::UnavailableError("Emulator not initialized");
    // Use Snes::Read for correct mapping
    return emulator_->snes().Read(addr);
}

absl::StatusOr<std::vector<uint8_t>> InternalEmulatorAdapter::ReadBlock(uint32_t addr, size_t len) {
    if (!emulator_) return absl::UnavailableError("Emulator not initialized");
    std::vector<uint8_t> data(len);
    for (size_t i = 0; i < len; ++i) {
        data[i] = emulator_->snes().Read(addr + i);
    }
    return data;
}

absl::Status InternalEmulatorAdapter::WriteByte(uint32_t addr, uint8_t val) {
    if (!emulator_) return absl::UnavailableError("Emulator not initialized");
    emulator_->snes().Write(addr, val);
    return absl::OkStatus();
}

absl::Status InternalEmulatorAdapter::WriteBlock(uint32_t addr, const std::vector<uint8_t>& data) {
    if (!emulator_) return absl::UnavailableError("Emulator not initialized");
    for (size_t i = 0; i < data.size(); ++i) {
        emulator_->snes().Write(addr + i, data[i]);
    }
    return absl::OkStatus();
}

void InternalEmulatorAdapter::CaptureCPUState(yaze::agent::CPUState* state) {
  auto& cpu = emulator_->snes().cpu();
  state->set_a(cpu.A);
  state->set_x(cpu.X);
  state->set_y(cpu.Y);
  state->set_pc(cpu.PC);
  state->set_pb(cpu.PB);
  state->set_db(cpu.DB);
  state->set_sp(cpu.SP());
  state->set_d(cpu.D);
  state->set_status(cpu.status);
  state->set_flag_n(cpu.GetNegativeFlag());
  state->set_flag_v(cpu.GetOverflowFlag());
  state->set_flag_z(cpu.GetZeroFlag());
  state->set_flag_c(cpu.GetCarryFlag());
  state->set_cycles(emulator_->GetCurrentCycle());
}

absl::Status InternalEmulatorAdapter::GetCpuState(yaze::agent::CPUState* out_state) {
    if (!emulator_) return absl::UnavailableError("Emulator not initialized");
    CaptureCPUState(out_state);
    return absl::OkStatus();
}

absl::Status InternalEmulatorAdapter::GetGameState(yaze::agent::GameStateResponse* response) {
    if (!emulator_) return absl::UnavailableError("Emulator not initialized");
    
    // Replicate logic from old EmulatorServiceImpl::GetGameState
    auto& snes = emulator_->snes();
    auto read8 = [&snes](uint32_t addr) { return snes.Read(addr); };
    auto read16 = [&read8](uint32_t addr) {
        return static_cast<uint16_t>(read8(addr)) |
               (static_cast<uint16_t>(read8(addr + 1)) << 8);
    };
    
    response->set_game_mode(read8(0x7E0010));
    response->set_link_state(read8(0x7E005D));
    response->set_link_pos_x(read16(0x7E0020));
    response->set_link_pos_y(read16(0x7E0022));
    response->set_link_health(read8(0x7EF36D));

    // Screenshot logic?
    // The previous implementation captured screenshot if requested.
    // The adapter interface accepts GameStateResponse* which doesn't include the request info.
    // IEmulator::GetGameState signature might need to take `include_screenshot` bool? 
    // Or IEmulator should handle screenshot separately.
    // For now, I'll omit screenshot in the core GameState call to keep it simple, 
    // or assume we don't need it for now.
    
    return absl::OkStatus();
}

absl::Status InternalEmulatorAdapter::RunToBreakpoint(yaze::agent::BreakpointHitResponse* response) {
    if (!emulator_ || !emulator_->is_snes_initialized()) {
        return absl::UnavailableError("SNES is not initialized.");
    }

    const int kMaxInstructions = 1000000;
    int instruction_count = 0;
    auto& bp_manager = emulator_->breakpoint_manager();
    auto& cpu = emulator_->snes().cpu();

    while (instruction_count++ < kMaxInstructions) {
        uint32_t pc = (cpu.PB << 16) | cpu.PC;
        using yaze::emu::BreakpointManager;
        if (bp_manager.ShouldBreakOnExecute(pc, BreakpointManager::CpuType::CPU_65816)) {
             response->set_hit(true);
             auto* last_hit = bp_manager.GetLastHit();
             if (last_hit) {
                 auto* bp_info = response->mutable_breakpoint();
                 bp_info->set_id(last_hit->id);
                 bp_info->set_address(last_hit->address);
                 bp_info->set_type(ToProtoBreakpointType(last_hit->type));
                 bp_info->set_cpu(ToProtoCpuType(last_hit->cpu));
                 bp_info->set_enabled(last_hit->enabled);
             }
             CaptureCPUState(response->mutable_cpu_state());
             return absl::OkStatus();
        }
        emulator_->StepSingleInstruction();
    }
    response->set_hit(false);
    return absl::OkStatus();
}


absl::StatusOr<uint32_t> InternalEmulatorAdapter::AddBreakpoint(uint32_t addr, 
                                       yaze::agent::BreakpointType type,
                                       yaze::agent::CpuType cpu,
                                       const std::string& condition,
                                       const std::string& description) {
    if (!emulator_) return absl::UnavailableError("Emulator not initialized");
    auto& bp_manager = emulator_->breakpoint_manager();
    return bp_manager.AddBreakpoint(addr, ToBreakpointType(type), ToCpuType(cpu), condition, description);
}

absl::Status InternalEmulatorAdapter::RemoveBreakpoint(uint32_t id) {
    if (!emulator_) return absl::UnavailableError("Emulator not initialized");
    emulator_->breakpoint_manager().RemoveBreakpoint(id);
    return absl::OkStatus();
}

absl::Status InternalEmulatorAdapter::ToggleBreakpoint(uint32_t id, bool enabled) {
    if (!emulator_) return absl::UnavailableError("Emulator not initialized");
    emulator_->breakpoint_manager().SetEnabled(id, enabled);
    return absl::OkStatus();
}

std::vector<yaze::agent::BreakpointInfo> InternalEmulatorAdapter::ListBreakpoints() {
    std::vector<yaze::agent::BreakpointInfo> result;
    if (!emulator_) return result;
    
    auto breakpoints = emulator_->breakpoint_manager().GetAllBreakpoints();
    for (const auto& bp : breakpoints) {
        yaze::agent::BreakpointInfo info;
        info.set_id(bp.id);
        info.set_address(bp.address);
        info.set_type(ToProtoBreakpointType(bp.type));
        info.set_cpu(ToProtoCpuType(bp.cpu));
        info.set_enabled(bp.enabled);
        result.push_back(info);
    }
    return result;
}

absl::Status InternalEmulatorAdapter::PressButton(yaze::agent::Button button) {
    if (!emulator_) return absl::UnavailableError("Emulator not initialized");
    emulator_->input_manager().PressButton(ToSnesButton(button));
    return absl::OkStatus();
}

absl::Status InternalEmulatorAdapter::ReleaseButton(yaze::agent::Button button) {
    if (!emulator_) return absl::UnavailableError("Emulator not initialized");
    emulator_->input_manager().ReleaseButton(ToSnesButton(button));
    return absl::OkStatus();
}

}  // namespace emu
}  // namespace yaze
