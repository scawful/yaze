#include "app/emu/internal_emulator_adapter.h"

#include "absl/strings/str_format.h"

#include <iostream>
#include <thread>
#include <chrono>

namespace yaze {
namespace emu {

namespace {

absl::StatusOr<input::SnesButton> ToSnesButton(InputButton button) {
  using yaze::emu::input::SnesButton;
  switch (button) {
    case InputButton::kA:
      return SnesButton::A;
    case InputButton::kB:
      return SnesButton::B;
    case InputButton::kX:
      return SnesButton::X;
    case InputButton::kY:
      return SnesButton::Y;
    case InputButton::kL:
      return SnesButton::L;
    case InputButton::kR:
      return SnesButton::R;
    case InputButton::kSelect:
      return SnesButton::SELECT;
    case InputButton::kStart:
      return SnesButton::START;
    case InputButton::kUp:
      return SnesButton::UP;
    case InputButton::kDown:
      return SnesButton::DOWN;
    case InputButton::kLeft:
      return SnesButton::LEFT;
    case InputButton::kRight:
      return SnesButton::RIGHT;
    default:
      return absl::InvalidArgumentError(absl::StrFormat(
          "Unsupported button value: %d", static_cast<int>(button)));
  }
}

BreakpointManager::Type ToBreakpointType(BreakpointKind kind) {
  using yaze::emu::BreakpointManager;
  switch (kind) {
    case BreakpointKind::kExecute: return BreakpointManager::Type::EXECUTE;
    case BreakpointKind::kRead: return BreakpointManager::Type::READ;
    case BreakpointKind::kWrite: return BreakpointManager::Type::WRITE;
    case BreakpointKind::kAccess: return BreakpointManager::Type::ACCESS;
    case BreakpointKind::kConditional: return BreakpointManager::Type::CONDITIONAL;
    default: return BreakpointManager::Type::EXECUTE;
  }
}

BreakpointManager::CpuType ToCpuType(CpuKind kind) {
  using yaze::emu::BreakpointManager;
  switch (kind) {
    case CpuKind::k65816: return BreakpointManager::CpuType::CPU_65816;
    case CpuKind::kSpc700: return BreakpointManager::CpuType::SPC700;
    default: return BreakpointManager::CpuType::CPU_65816;
  }
}

BreakpointKind FromBreakpointType(BreakpointManager::Type type) {
  using yaze::emu::BreakpointManager;
  switch (type) {
    case BreakpointManager::Type::EXECUTE: return BreakpointKind::kExecute;
    case BreakpointManager::Type::READ: return BreakpointKind::kRead;
    case BreakpointManager::Type::WRITE: return BreakpointKind::kWrite;
    case BreakpointManager::Type::ACCESS: return BreakpointKind::kAccess;
    case BreakpointManager::Type::CONDITIONAL: return BreakpointKind::kConditional;
    default: return BreakpointKind::kExecute;
  }
}

CpuKind FromCpuType(BreakpointManager::CpuType cpu) {
  using yaze::emu::BreakpointManager;
  switch (cpu) {
    case BreakpointManager::CpuType::CPU_65816: return CpuKind::k65816;
    case BreakpointManager::CpuType::SPC700: return CpuKind::kSpc700;
    default: return CpuKind::k65816;
  }
}

}  // namespace

InternalEmulatorAdapter::InternalEmulatorAdapter(Emulator* emulator)
    : emulator_(emulator) {}

bool InternalEmulatorAdapter::IsConnected() const {
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
    for (int i = 0; i < count; ++i) {
        emulator_->StepSingleInstruction();
    }
    return absl::OkStatus();
}

absl::Status InternalEmulatorAdapter::StepOver() {
    if (!emulator_) return absl::UnavailableError("Emulator not initialized");
    InitializeStepController();
    step_controller_.StepOver();
    return absl::OkStatus();
}

absl::Status InternalEmulatorAdapter::StepOut() {
    if (!emulator_) return absl::UnavailableError("Emulator not initialized");
    InitializeStepController();
    step_controller_.StepOut();
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
    return emulator_->snes().Read(addr);
}

absl::StatusOr<std::vector<uint8_t>> InternalEmulatorAdapter::ReadBlock(
    uint32_t addr, size_t len) {
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

absl::Status InternalEmulatorAdapter::WriteBlock(
    uint32_t addr, const std::vector<uint8_t>& data) {
    if (!emulator_) return absl::UnavailableError("Emulator not initialized");
    for (size_t i = 0; i < data.size(); ++i) {
        emulator_->snes().Write(addr + i, data[i]);
    }
    return absl::OkStatus();
}

void InternalEmulatorAdapter::CaptureCPUState(CpuStateSnapshot* state) {
  auto& cpu = emulator_->snes().cpu();
  state->a = cpu.A;
  state->x = cpu.X;
  state->y = cpu.Y;
  state->pc = cpu.PC;
  state->pb = cpu.PB;
  state->db = cpu.DB;
  state->sp = cpu.SP();
  state->d = cpu.D;
  state->status = cpu.status;
  state->flag_n = cpu.GetNegativeFlag();
  state->flag_v = cpu.GetOverflowFlag();
  state->flag_z = cpu.GetZeroFlag();
  state->flag_c = cpu.GetCarryFlag();
  state->cycles = emulator_->GetCurrentCycle();
}

absl::Status InternalEmulatorAdapter::GetCpuState(CpuStateSnapshot* out_state) {
    if (!emulator_) return absl::UnavailableError("Emulator not initialized");
    CaptureCPUState(out_state);
    return absl::OkStatus();
}

absl::Status InternalEmulatorAdapter::GetGameState(GameSnapshot* response) {
    if (!emulator_) return absl::UnavailableError("Emulator not initialized");

    auto& snes = emulator_->snes();
    auto read8 = [&snes](uint32_t addr) { return snes.Read(addr); };
    auto read16 = [&read8](uint32_t addr) {
        return static_cast<uint16_t>(read8(addr)) |
               (static_cast<uint16_t>(read8(addr + 1)) << 8);
    };

    response->game_mode = read8(0x7E0010);
    response->link_state = read8(0x7E005D);
    response->link_pos_x = read16(0x7E0020);
    response->link_pos_y = read16(0x7E0022);
    response->link_health = read8(0x7EF36D);

    return absl::OkStatus();
}

absl::Status InternalEmulatorAdapter::RunToBreakpoint(
    BreakpointHitResult* response) {
    if (!emulator_ || !emulator_->is_snes_initialized()) {
        return absl::UnavailableError("SNES is not initialized.");
    }

    const int kMaxInstructions = 1000000;
    int instruction_count = 0;
    auto& bp_manager = emulator_->breakpoint_manager();
    auto& cpu = emulator_->snes().cpu();

    while (instruction_count++ < kMaxInstructions) {
        uint32_t pc = (cpu.PB << 16) | cpu.PC;
        if (bp_manager.ShouldBreakOnExecute(
                pc, BreakpointManager::CpuType::CPU_65816)) {
             response->hit = true;
             auto* last_hit = bp_manager.GetLastHit();
             if (last_hit) {
                 response->breakpoint.id = last_hit->id;
                 response->breakpoint.address = last_hit->address;
                 response->breakpoint.kind = FromBreakpointType(last_hit->type);
                 response->breakpoint.cpu = FromCpuType(last_hit->cpu);
                 response->breakpoint.enabled = last_hit->enabled;
             }
             CaptureCPUState(&response->cpu_state);
             return absl::OkStatus();
        }
        emulator_->StepSingleInstruction();
    }
    response->hit = false;
    return absl::OkStatus();
}

absl::StatusOr<uint32_t> InternalEmulatorAdapter::AddBreakpoint(
    uint32_t addr, BreakpointKind type, CpuKind cpu,
    const std::string& condition, const std::string& description) {
    if (!emulator_) return absl::UnavailableError("Emulator not initialized");
    auto& bp_manager = emulator_->breakpoint_manager();
    return bp_manager.AddBreakpoint(addr, ToBreakpointType(type),
                                    ToCpuType(cpu), condition, description);
}

absl::Status InternalEmulatorAdapter::RemoveBreakpoint(uint32_t breakpoint_id) {
    if (!emulator_) return absl::UnavailableError("Emulator not initialized");
    emulator_->breakpoint_manager().RemoveBreakpoint(breakpoint_id);
    return absl::OkStatus();
}

absl::Status InternalEmulatorAdapter::ToggleBreakpoint(uint32_t breakpoint_id,
                                                       bool enabled) {
    if (!emulator_) return absl::UnavailableError("Emulator not initialized");
    emulator_->breakpoint_manager().SetEnabled(breakpoint_id, enabled);
    return absl::OkStatus();
}

std::vector<BreakpointSnapshot> InternalEmulatorAdapter::ListBreakpoints() {
    std::vector<BreakpointSnapshot> result;
    if (!emulator_) return result;

    auto breakpoints = emulator_->breakpoint_manager().GetAllBreakpoints();
    for (const auto& bp : breakpoints) {
        BreakpointSnapshot info;
        info.id = bp.id;
        info.address = bp.address;
        info.kind = FromBreakpointType(bp.type);
        info.cpu = FromCpuType(bp.cpu);
        info.enabled = bp.enabled;
        result.push_back(info);
    }
    return result;
}

absl::Status InternalEmulatorAdapter::PressButton(InputButton button) {
    if (!emulator_) return absl::UnavailableError("Emulator not initialized");
    auto snes_button = ToSnesButton(button);
    if (!snes_button.ok()) return snes_button.status();
    emulator_->input_manager().PressButton(*snes_button);
    return absl::OkStatus();
}

absl::Status InternalEmulatorAdapter::ReleaseButton(InputButton button) {
    if (!emulator_) return absl::UnavailableError("Emulator not initialized");
    auto snes_button = ToSnesButton(button);
    if (!snes_button.ok()) return snes_button.status();
    emulator_->input_manager().ReleaseButton(*snes_button);
    return absl::OkStatus();
}

}  // namespace emu
}  // namespace yaze
