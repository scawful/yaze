#include "cli/service/agent/emulator_service_impl.h"

#include "util/grpc_win_compat.h"

#include <grpcpp/grpcpp.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <unordered_set>

#include "emu/emulator.h"
#include "rom/rom.h"
#include "app/emu/debug/disassembler.h"
#include "app/service/screenshot_utils.h"

namespace yaze::net {

namespace {
emu::input::SnesButton ToSnesButton(agent::Button button) {
  using emu::input::SnesButton;
  switch (button) {
    case agent::A: return SnesButton::A;
    case agent::B: return SnesButton::B;
    case agent::X: return SnesButton::X;
    case agent::Y: return SnesButton::Y;
    case agent::L: return SnesButton::L;
    case agent::R: return SnesButton::R;
    case agent::SELECT: return SnesButton::SELECT;
    case agent::START: return SnesButton::START;
    case agent::UP: return SnesButton::UP;
    case agent::DOWN: return SnesButton::DOWN;
    case agent::LEFT: return SnesButton::LEFT;
    case agent::RIGHT: return SnesButton::RIGHT;
    default: return SnesButton::B;
  }
}

emu::BreakpointManager::Type ToBreakpointType(agent::BreakpointType proto_type) {
  using emu::BreakpointManager;
  switch (proto_type) {
    case agent::EXECUTE: return BreakpointManager::Type::EXECUTE;
    case agent::READ: return BreakpointManager::Type::READ;
    case agent::WRITE: return BreakpointManager::Type::WRITE;
    case agent::ACCESS: return BreakpointManager::Type::ACCESS;
    case agent::CONDITIONAL: return BreakpointManager::Type::CONDITIONAL;
    default: return BreakpointManager::Type::EXECUTE;
  }
}

emu::BreakpointManager::CpuType ToCpuType(agent::CpuType proto_cpu) {
  using emu::BreakpointManager;
  switch (proto_cpu) {
    case agent::CPU_65816: return BreakpointManager::CpuType::CPU_65816;
    case agent::SPC700: return BreakpointManager::CpuType::SPC700;
    default: return BreakpointManager::CpuType::CPU_65816;
  }
}

agent::BreakpointType ToProtoBreakpointType(emu::BreakpointManager::Type type) {
  using emu::BreakpointManager;
  switch (type) {
    case BreakpointManager::Type::EXECUTE: return agent::EXECUTE;
    case BreakpointManager::Type::READ: return agent::READ;
    case BreakpointManager::Type::WRITE: return agent::WRITE;
    case BreakpointManager::Type::ACCESS: return agent::ACCESS;
    case BreakpointManager::Type::CONDITIONAL: return agent::CONDITIONAL;
    default: return agent::EXECUTE;
  }
}

agent::CpuType ToProtoCpuType(emu::BreakpointManager::CpuType cpu) {
  using emu::BreakpointManager;
  switch (cpu) {
    case BreakpointManager::CpuType::CPU_65816: return agent::CPU_65816;
    case BreakpointManager::CpuType::SPC700: return agent::SPC700;
    default: return agent::CPU_65816;
  }
}
}  // namespace

EmulatorServiceImpl::EmulatorServiceImpl(yaze::emu::Emulator* emulator,
                                         RomGetter rom_getter,
                                         RomLoader rom_loader)
    : emulator_(emulator), rom_getter_(rom_getter), rom_loader_(rom_loader) {}

// --- ROM Loading ---

grpc::Status EmulatorServiceImpl::LoadRom(
    grpc::ServerContext* context, const agent::LoadRomRequest* request,
    agent::LoadRomResponse* response) {
  if (!rom_loader_) {
    response->set_success(false);
    response->set_message("ROM loading not available (no loader callback set)");
    return grpc::Status::OK;
  }

  const std::string& filepath = request->filepath();
  if (filepath.empty()) {
    response->set_success(false);
    response->set_message("Filepath is required");
    return grpc::Status::OK;
  }

  bool success = rom_loader_(filepath);
  if (success) {
    response->set_success(true);
    response->set_message("ROM loaded successfully");
    // Get ROM info if available
    if (rom_getter_) {
      Rom* rom = rom_getter_();
      if (rom && rom->is_loaded()) {
        response->set_rom_title(rom->title());
        response->set_rom_size(rom->size());
      }
    }
  } else {
    response->set_success(false);
    response->set_message("Failed to load ROM: " + filepath);
  }
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::GetLoadedRomPath(
    grpc::ServerContext* context, const agent::Empty* request,
    agent::LoadedRomPathResponse* response) {
  if (!rom_getter_) {
    response->set_has_rom(false);
    return grpc::Status::OK;
  }

  Rom* rom = rom_getter_();
  if (rom && rom->is_loaded()) {
    response->set_has_rom(true);
    response->set_filepath(rom->filename());
    response->set_title(rom->title());
  } else {
    response->set_has_rom(false);
  }
  return grpc::Status::OK;
}

void EmulatorServiceImpl::CaptureCPUState(agent::CPUState* state) {
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

// --- Core Lifecycle & Control ---

grpc::Status EmulatorServiceImpl::ControlEmulator(grpc::ServerContext* context, 
                               const agent::ControlRequest* request,
                               agent::CommandResponse* response) {
  if (!emulator_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Emulator not initialized.");
  
  std::string action = request->action();
  if (action == "start" || action == "resume") {
    emulator_->set_running(true);
    response->set_message("Emulator started/resumed.");
  } else if (action == "stop" || action == "pause") {
    emulator_->set_running(false);
    response->set_message("Emulator stopped/paused.");
  } else if (action == "reset") {
    emulator_->snes().Reset(true);
    response->set_message("Emulator reset.");
  } else if (action == "init" || action == "initialize") {
    Rom* rom = rom_getter_ ? rom_getter_() : nullptr;
    if (rom && rom->is_loaded()) {
      emulator_->EnsureInitialized(rom);
      response->set_message("Emulator initialized with active ROM.");
    } else {
      return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "ROM not loaded in core.");
    }
  } else {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Unknown action: " + action);
  }
  
  response->set_success(true);
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::StepEmulator(grpc::ServerContext* context, 
                            const agent::StepControlRequest* request,
                            agent::StepResponse* response) {
  if (!emulator_ || !emulator_->is_snes_initialized()) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "SNES is not initialized.");
  }

  std::string mode = request->mode();
  if (mode == "instruction") {
    emulator_->StepSingleInstruction();
    response->set_message("Stepped 1 instruction.");
  } else if (mode == "over") {
    InitializeStepController();
    auto result = step_controller_.StepOver();
    response->set_message(result.message);
  } else if (mode == "out") {
    InitializeStepController();
    auto result = step_controller_.StepOut();
    response->set_message(result.message);
  } else {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Unknown step mode: " + mode);
  }

  CaptureCPUState(response->mutable_cpu_state());
  response->set_success(true);
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::RunToBreakpoint(grpc::ServerContext* context, 
                               const agent::Empty* request,
                               agent::BreakpointHitResponse* response) {
  if (!emulator_ || !emulator_->is_snes_initialized()) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "SNES is not initialized.");
  }

  const int kMaxInstructions = 1000000;
  int instruction_count = 0;
  auto& bp_manager = emulator_->breakpoint_manager();
  auto& cpu = emulator_->snes().cpu();

  while (instruction_count++ < kMaxInstructions) {
    uint32_t pc = (cpu.PB << 16) | cpu.PC;
    if (bp_manager.ShouldBreakOnExecute(pc, emu::BreakpointManager::CpuType::CPU_65816)) {
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
      return grpc::Status::OK;
    }
    emulator_->StepSingleInstruction();
  }
  response->set_hit(false);
  return grpc::Status::OK;
}

// --- Input & State ---

grpc::Status EmulatorServiceImpl::PressButtons(grpc::ServerContext* context,
                                               const agent::ButtonRequest* request,
                                               agent::CommandResponse* response) {
  if (!emulator_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Emulator not initialized.");
  auto& input_manager = emulator_->input_manager();
  for (int i = 0; i < request->buttons_size(); i++) {
    input_manager.PressButton(ToSnesButton(static_cast<agent::Button>(request->buttons(i))));
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  for (int i = 0; i < request->buttons_size(); i++) {
    input_manager.ReleaseButton(ToSnesButton(static_cast<agent::Button>(request->buttons(i))));
  }
  response->set_success(true);
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::ReleaseButtons(grpc::ServerContext* context,
                                                 const agent::ButtonRequest* request,
                                                 agent::CommandResponse* response) {
  if (!emulator_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Emulator not initialized.");
  auto& input_manager = emulator_->input_manager();
  for (int i = 0; i < request->buttons_size(); i++) {
    input_manager.ReleaseButton(ToSnesButton(static_cast<agent::Button>(request->buttons(i))));
  }
  response->set_success(true);
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::HoldButtons(grpc::ServerContext* context,
                                              const agent::ButtonHoldRequest* request,
                                              agent::CommandResponse* response) {
  if (!emulator_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Emulator not initialized.");
  auto& input_manager = emulator_->input_manager();
  for (int i = 0; i < request->buttons_size(); i++) {
    input_manager.PressButton(ToSnesButton(static_cast<agent::Button>(request->buttons(i))));
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(request->duration_ms()));
  for (int i = 0; i < request->buttons_size(); i++) {
    input_manager.ReleaseButton(ToSnesButton(static_cast<agent::Button>(request->buttons(i))));
  }
  response->set_success(true);
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::GetGameState(grpc::ServerContext* context,
                                               const agent::GameStateRequest* request,
                                               agent::GameStateResponse* response) {
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

grpc::Status EmulatorServiceImpl::ReadMemory(grpc::ServerContext* context,
                                             const agent::MemoryRequest* request,
                                             agent::MemoryResponse* response) {
  if (!emulator_ || !emulator_->is_snes_initialized()) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "SNES is not initialized.");
  }
  // Use Snes::Read() for proper memory mapping (WRAM, registers, etc.)
  auto& snes = emulator_->snes();
  response->set_address(request->address());
  std::vector<uint8_t> data(request->size());
  for (uint32_t i = 0; i < request->size(); ++i) {
    data[i] = snes.Read(request->address() + i);
  }
  response->set_data(data.data(), data.size());
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::WriteMemory(grpc::ServerContext* context,
                                              const agent::MemoryWriteRequest* request,
                                              agent::CommandResponse* response) {
  if (!emulator_ || !emulator_->is_snes_initialized()) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "SNES is not initialized.");
  }
  auto& memory = emulator_->snes().memory();
  const std::string& data = request->data();
  for (uint32_t i = 0; i < data.size(); ++i) {
    memory.WriteByte(request->address() + i, static_cast<uint8_t>(data[i]));
  }
  response->set_success(true);
  return grpc::Status::OK;
}

// --- Debugging Management ---

grpc::Status EmulatorServiceImpl::BreakpointControl(grpc::ServerContext* context,
                                 const agent::BreakpointControlRequest* request,
                                 agent::BreakpointControlResponse* response) {
  if (!emulator_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Emulator not initialized.");
  auto& bp_manager = emulator_->breakpoint_manager();
  std::string action = request->action();

  if (action == "add") {
    uint32_t id = bp_manager.AddBreakpoint(request->address(), ToBreakpointType(request->type()), ToCpuType(request->cpu()), request->condition(), request->description());
    response->set_breakpoint_id(id);
    response->set_message("Breakpoint added.");
  } else if (action == "remove") {
    bp_manager.RemoveBreakpoint(request->id());
    response->set_message("Breakpoint removed.");
  } else if (action == "toggle") {
    bp_manager.SetEnabled(request->id(), request->enabled());
    response->set_message("Breakpoint toggled.");
  } else if (action == "list") {
    auto breakpoints = bp_manager.GetAllBreakpoints();
    for (const auto& bp : breakpoints) {
      auto* info = response->add_breakpoints();
      info->set_id(bp.id);
      info->set_address(bp.address);
      info->set_type(ToProtoBreakpointType(bp.type));
      info->set_cpu(ToProtoCpuType(bp.cpu));
      info->set_enabled(bp.enabled);
    }
  } else {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Unknown action: " + action);
  }

  response->set_success(true);
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::WatchpointControl(grpc::ServerContext* context,
                                 const agent::WatchpointControlRequest* request,
                                 agent::WatchpointControlResponse* response) {
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "WatchpointManager integration pending.");
}

// --- Analysis & Symbols ---

grpc::Status EmulatorServiceImpl::GetDisassembly(grpc::ServerContext* context,
                              const agent::DisassemblyRequest* request,
                              agent::DisassemblyResponse* response) {
  if (!emulator_ || !emulator_->is_snes_initialized()) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "SNES not initialized.");
  
  yaze::emu::debug::Disassembler65816 dis;
  auto& memory = emulator_->snes().memory();
  auto& cpu = emulator_->snes().cpu();
  
  uint32_t addr = request->start_address();
  for (uint32_t i = 0; i < request->count(); ++i) {
    auto inst = dis.Disassemble(addr, [&memory](uint32_t a){ return memory.ReadByte(a); }, cpu.GetAccumulatorSize(), cpu.GetIndexSize());
    auto* line = response->add_lines();
    line->set_address(inst.address);
    line->set_mnemonic(inst.mnemonic);
    line->set_operand_str(inst.operand_str);
    addr += inst.size;
  }
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::GetExecutionTrace(grpc::ServerContext* context,
                                 const agent::TraceRequest* request,
                                 agent::TraceResponse* response) {
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Trace not implemented.");
}

grpc::Status EmulatorServiceImpl::ResolveSymbol(grpc::ServerContext* context,
                             const agent::SymbolLookupRequest* request,
                             agent::SymbolLookupResponse* response) {
  auto sym = symbol_provider_.FindSymbol(request->symbol_name());
  if (sym) {
    response->set_found(true);
    response->set_symbol_name(sym->name);
    response->set_address(sym->address);
  }
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::GetSymbolAt(grpc::ServerContext* context,
                           const agent::AddressRequest* request,
                           agent::SymbolLookupResponse* response) {
  auto sym = symbol_provider_.GetSymbol(request->address());
  if (sym) {
    response->set_found(true);
    response->set_symbol_name(sym->name);
    response->set_address(sym->address);
  }
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::LoadSymbols(grpc::ServerContext* context,
                           const agent::SymbolFileRequest* request,
                           agent::CommandResponse* response) {
  auto status = symbol_provider_.LoadSymbolFile(request->filepath(), (yaze::emu::debug::SymbolFormat)request->format());
  response->set_success(status.ok());
  response->set_message(std::string(status.message()));
  return grpc::Status::OK;
}

// --- Session & Experiments ---

grpc::Status EmulatorServiceImpl::GetDebugStatus(grpc::ServerContext* context,
                              const agent::Empty* request,
                              agent::DebugStatusResponse* response) {
  if (!emulator_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Emulator not initialized.");
  response->set_is_running(emulator_->running());
  CaptureCPUState(response->mutable_cpu_state());
  response->set_active_breakpoints(emulator_->breakpoint_manager().GetAllBreakpoints().size());
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::TestRun(grpc::ServerContext* context,
                       const agent::TestRunRequest* request,
                       agent::TestRunResponse* response) {
  if (!emulator_ || !emulator_->is_snes_initialized()) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "SNES not initialized.");
  auto& snes = emulator_->snes();
  auto& cpu = snes.cpu();
  auto& memory = snes.memory();

  uint32_t addr = request->address();
  const std::string& data = request->data();
  for (size_t i = 0; i < data.size(); ++i) { memory.WriteByte(addr + i, (uint8_t)data[i]); }

  bool was_running = emulator_->running();
  emulator_->set_running(false);
  cpu.PB = (addr >> 16) & 0xFF;
  cpu.PC = addr & 0xFFFF;

  uint32_t frames = request->frame_count() > 0 ? request->frame_count() : 60;
  for (uint32_t i = 0; i < frames; ++i) {
    emulator_->RunFrameOnly();
    if (cpu.PC == 0 && cpu.PB == 0) { response->set_crashed(true); break; }
  }

  CaptureCPUState(response->mutable_final_cpu_state());
  emulator_->set_running(was_running);
  response->set_success(!response->crashed());
  return grpc::Status::OK;
}

void EmulatorServiceImpl::InitializeStepController() {
  auto& memory = emulator_->snes().memory();
  auto& cpu = emulator_->snes().cpu();
  step_controller_.SetMemoryReader([&memory](uint32_t addr) -> uint8_t { return memory.ReadByte(addr); });
  step_controller_.SetSingleStepper([this]() { emulator_->StepSingleInstruction(); });
  step_controller_.SetPcGetter([&cpu]() -> uint32_t { return (cpu.PB << 16) | cpu.PC; });
}

}  // namespace yaze::net
