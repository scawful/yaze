#include "cli/service/agent/emulator_service_impl.h"

#include <filesystem>
#include <fstream>
#include <thread>
#include <unordered_set>

#include "absl/strings/escaping.h"
#include "absl/strings/str_format.h"
#include "app/emu/debug/breakpoint_manager.h"
#include "app/emu/debug/disassembler.h"
#include "app/emu/debug/disassembly_viewer.h"
#include "app/emu/debug/watchpoint_manager.h"
#include "app/emu/emulator.h"
#include "app/emu/input/input_backend.h"  // Required for SnesButton enum
#include "app/service/screenshot_utils.h"

namespace yaze::agent {

namespace {
// Helper to convert our gRPC Button enum to the emulator's SnesButton enum
emu::input::SnesButton ToSnesButton(Button button) {
  using emu::input::SnesButton;
  switch (button) {
    case A:
      return SnesButton::A;
    case B:
      return SnesButton::B;
    case X:
      return SnesButton::X;
    case Y:
      return SnesButton::Y;
    case L:
      return SnesButton::L;
    case R:
      return SnesButton::R;
    case SELECT:
      return SnesButton::SELECT;
    case START:
      return SnesButton::START;
    case UP:
      return SnesButton::UP;
    case DOWN:
      return SnesButton::DOWN;
    case LEFT:
      return SnesButton::LEFT;
    case RIGHT:
      return SnesButton::RIGHT;
    default:
      return SnesButton::B;  // Default fallback
  }
}
}  // namespace

EmulatorServiceImpl::EmulatorServiceImpl(yaze::emu::Emulator* emulator)
    : emulator_(emulator) {}

// --- Lifecycle ---

grpc::Status EmulatorServiceImpl::Start(grpc::ServerContext* context,
                                        const Empty* request,
                                        CommandResponse* response) {
  if (!emulator_)
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Emulator not initialized.");
  emulator_->set_running(true);
  response->set_success(true);
  response->set_message("Emulator started.");
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::Stop(grpc::ServerContext* context,
                                       const Empty* request,
                                       CommandResponse* response) {
  if (!emulator_)
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Emulator not initialized.");
  emulator_->set_running(false);
  response->set_success(true);
  response->set_message("Emulator stopped.");
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::Pause(grpc::ServerContext* context,
                                        const Empty* request,
                                        CommandResponse* response) {
  if (!emulator_)
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Emulator not initialized.");
  emulator_->set_running(false);
  response->set_success(true);
  response->set_message("Emulator paused.");
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::Resume(grpc::ServerContext* context,
                                         const Empty* request,
                                         CommandResponse* response) {
  if (!emulator_)
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Emulator not initialized.");
  emulator_->set_running(true);
  response->set_success(true);
  response->set_message("Emulator resumed.");
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::Reset(grpc::ServerContext* context,
                                        const Empty* request,
                                        CommandResponse* response) {
  if (!emulator_)
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Emulator not initialized.");
  emulator_->snes().Reset(true);  // Hard reset
  response->set_success(true);
  response->set_message("Emulator reset.");
  return grpc::Status::OK;
}

// --- Input Control ---

grpc::Status EmulatorServiceImpl::PressButtons(grpc::ServerContext* context,
                                               const ButtonRequest* request,
                                               CommandResponse* response) {
  if (!emulator_)
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Emulator not initialized.");
  auto& input_manager = emulator_->input_manager();
  for (int i = 0; i < request->buttons_size(); i++) {
    input_manager.PressButton(
        ToSnesButton(static_cast<Button>(request->buttons(i))));
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  for (int i = 0; i < request->buttons_size(); i++) {
    input_manager.ReleaseButton(
        ToSnesButton(static_cast<Button>(request->buttons(i))));
  }
  response->set_success(true);
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::ReleaseButtons(grpc::ServerContext* context,
                                                 const ButtonRequest* request,
                                                 CommandResponse* response) {
  if (!emulator_)
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Emulator not initialized.");
  auto& input_manager = emulator_->input_manager();
  for (int i = 0; i < request->buttons_size(); i++) {
    input_manager.ReleaseButton(
        ToSnesButton(static_cast<Button>(request->buttons(i))));
  }
  response->set_success(true);
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::HoldButtons(grpc::ServerContext* context,
                                              const ButtonHoldRequest* request,
                                              CommandResponse* response) {
  if (!emulator_)
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Emulator not initialized.");
  auto& input_manager = emulator_->input_manager();
  for (int i = 0; i < request->buttons_size(); i++) {
    input_manager.PressButton(
        ToSnesButton(static_cast<Button>(request->buttons(i))));
  }
  std::this_thread::sleep_for(
      std::chrono::milliseconds(request->duration_ms()));
  for (int i = 0; i < request->buttons_size(); i++) {
    input_manager.ReleaseButton(
        ToSnesButton(static_cast<Button>(request->buttons(i))));
  }
  response->set_success(true);
  return grpc::Status::OK;
}

// --- State Inspection ---

grpc::Status EmulatorServiceImpl::GetGameState(grpc::ServerContext* context,
                                               const GameStateRequest* request,
                                               GameStateResponse* response) {
  if (!emulator_ || !emulator_->is_snes_initialized()) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "SNES is not initialized.");
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

grpc::Status EmulatorServiceImpl::ReadMemory(grpc::ServerContext* context,
                                             const MemoryRequest* request,
                                             MemoryResponse* response) {
  if (!emulator_ || !emulator_->is_snes_initialized()) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "SNES is not initialized.");
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

grpc::Status EmulatorServiceImpl::WriteMemory(grpc::ServerContext* context,
                                              const MemoryWriteRequest* request,
                                              CommandResponse* response) {
  if (!emulator_ || !emulator_->is_snes_initialized()) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "SNES is not initialized.");
  }
  auto& memory = emulator_->snes().memory();
  const std::string& data = request->data();
  for (uint32_t i = 0; i < data.size(); ++i) {
    memory.WriteByte(request->address() + i, static_cast<uint8_t>(data[i]));
  }
  response->set_success(true);
  response->set_message(absl::StrFormat("Wrote %d bytes to 0x%X.", data.size(),
                                        request->address()));
  return grpc::Status::OK;
}

// --- Advanced Debugging ---

// Helper to convert proto breakpoint type to manager type
emu::BreakpointManager::Type ToBreakpointType(BreakpointType proto_type) {
  using emu::BreakpointManager;
  switch (proto_type) {
    case EXECUTE:
      return BreakpointManager::Type::EXECUTE;
    case READ:
      return BreakpointManager::Type::READ;
    case WRITE:
      return BreakpointManager::Type::WRITE;
    case ACCESS:
      return BreakpointManager::Type::ACCESS;
    case CONDITIONAL:
      return BreakpointManager::Type::CONDITIONAL;
    default:
      return BreakpointManager::Type::EXECUTE;
  }
}

// Helper to convert proto CPU type to manager CPU type
emu::BreakpointManager::CpuType ToCpuType(CpuType proto_cpu) {
  using emu::BreakpointManager;
  switch (proto_cpu) {
    case CPU_65816:
      return BreakpointManager::CpuType::CPU_65816;
    case SPC700:
      return BreakpointManager::CpuType::SPC700;
    default:
      return BreakpointManager::CpuType::CPU_65816;
  }
}

// Helper to convert manager type back to proto
BreakpointType ToProtoBreakpointType(emu::BreakpointManager::Type type) {
  using emu::BreakpointManager;
  switch (type) {
    case BreakpointManager::Type::EXECUTE:
      return EXECUTE;
    case BreakpointManager::Type::READ:
      return READ;
    case BreakpointManager::Type::WRITE:
      return WRITE;
    case BreakpointManager::Type::ACCESS:
      return ACCESS;
    case BreakpointManager::Type::CONDITIONAL:
      return CONDITIONAL;
    default:
      return EXECUTE;
  }
}

// Helper to convert manager CPU type back to proto
CpuType ToProtoCpuType(emu::BreakpointManager::CpuType cpu) {
  using emu::BreakpointManager;
  switch (cpu) {
    case BreakpointManager::CpuType::CPU_65816:
      return CPU_65816;
    case BreakpointManager::CpuType::SPC700:
      return SPC700;
    default:
      return CPU_65816;
  }
}

grpc::Status EmulatorServiceImpl::AddBreakpoint(
    grpc::ServerContext* context, const BreakpointRequest* request,
    BreakpointResponse* response) {
  if (!emulator_) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Emulator not initialized.");
  }

  auto& bp_manager = emulator_->breakpoint_manager();
  uint32_t id = bp_manager.AddBreakpoint(
      request->address(), ToBreakpointType(request->type()),
      ToCpuType(request->cpu()), request->condition(), request->description());

  response->set_success(true);
  response->set_breakpoint_id(id);
  response->set_message(
      absl::StrFormat("Breakpoint %d added at 0x%06X", id, request->address()));
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::RemoveBreakpoint(
    grpc::ServerContext* context, const BreakpointIdRequest* request,
    CommandResponse* response) {
  if (!emulator_) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Emulator not initialized.");
  }

  emulator_->breakpoint_manager().RemoveBreakpoint(request->breakpoint_id());
  response->set_success(true);
  response->set_message(
      absl::StrFormat("Breakpoint %d removed", request->breakpoint_id()));
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::ListBreakpoints(
    grpc::ServerContext* context, const Empty* request,
    BreakpointListResponse* response) {
  if (!emulator_) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Emulator not initialized.");
  }

  auto breakpoints = emulator_->breakpoint_manager().GetAllBreakpoints();
  for (const auto& bp : breakpoints) {
    auto* info = response->add_breakpoints();
    info->set_id(bp.id);
    info->set_address(bp.address);
    info->set_type(ToProtoBreakpointType(bp.type));
    info->set_cpu(ToProtoCpuType(bp.cpu));
    info->set_enabled(bp.enabled);
    info->set_condition(bp.condition);
    info->set_description(bp.description);
    info->set_hit_count(bp.hit_count);
  }

  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::SetBreakpointEnabled(
    grpc::ServerContext* context, const BreakpointStateRequest* request,
    CommandResponse* response) {
  if (!emulator_) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Emulator not initialized.");
  }

  emulator_->breakpoint_manager().SetEnabled(request->breakpoint_id(),
                                             request->enabled());
  response->set_success(true);
  response->set_message(
      absl::StrFormat("Breakpoint %d %s", request->breakpoint_id(),
                      request->enabled() ? "enabled" : "disabled"));
  return grpc::Status::OK;
}

// Watchpoints - Note: Emulator needs WatchpointManager integration first
grpc::Status EmulatorServiceImpl::AddWatchpoint(
    grpc::ServerContext* context, const WatchpointRequest* request,
    WatchpointResponse* response) {
  // TODO: Integrate WatchpointManager into Emulator class
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
                      "Watchpoints require WatchpointManager integration");
}

grpc::Status EmulatorServiceImpl::RemoveWatchpoint(
    grpc::ServerContext* context, const WatchpointIdRequest* request,
    CommandResponse* response) {
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
                      "Watchpoints require WatchpointManager integration");
}

grpc::Status EmulatorServiceImpl::ListWatchpoints(
    grpc::ServerContext* context, const Empty* request,
    WatchpointListResponse* response) {
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
                      "Watchpoints require WatchpointManager integration");
}

grpc::Status EmulatorServiceImpl::GetWatchpointHistory(
    grpc::ServerContext* context, const WatchpointHistoryRequest* request,
    WatchpointHistoryResponse* response) {
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
                      "Watchpoints require WatchpointManager integration");
}

// Execution Control
grpc::Status EmulatorServiceImpl::StepInstruction(grpc::ServerContext* context,
                                                  const Empty* request,
                                                  StepResponse* response) {
  if (!emulator_ || !emulator_->is_snes_initialized()) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "SNES is not initialized.");
  }

  // Capture state before step
  auto& cpu = emulator_->snes().cpu();
  uint32_t pc_before = (cpu.PB << 16) | cpu.PC;

  // Execute one instruction
  emulator_->StepSingleInstruction();

  // Fill response with new CPU state
  auto* cpu_state = response->mutable_cpu_state();
  cpu_state->set_a(cpu.A);
  cpu_state->set_x(cpu.X);
  cpu_state->set_y(cpu.Y);
  cpu_state->set_sp(cpu.SP());
  cpu_state->set_pc(cpu.PC);
  cpu_state->set_db(cpu.DB);
  cpu_state->set_pb(cpu.PB);
  cpu_state->set_d(cpu.D);
  cpu_state->set_status(cpu.status);
  cpu_state->set_flag_n(cpu.GetNegativeFlag());
  cpu_state->set_flag_v(cpu.GetOverflowFlag());
  cpu_state->set_flag_d(cpu.GetDecimalFlag());
  cpu_state->set_flag_i(cpu.GetInterruptFlag());
  cpu_state->set_flag_z(cpu.GetZeroFlag());
  cpu_state->set_flag_c(cpu.GetCarryFlag());
  cpu_state->set_cycles(emulator_->GetCurrentCycle());

  response->set_success(true);
  response->set_message(absl::StrFormat("Stepped from 0x%06X to 0x%06X",
                                        pc_before, (cpu.PB << 16) | cpu.PC));
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::RunToBreakpoint(
    grpc::ServerContext* context, const Empty* request,
    BreakpointHitResponse* response) {
  if (!emulator_ || !emulator_->is_snes_initialized()) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "SNES is not initialized.");
  }

  // Run emulator until breakpoint is hit (max 1 million instructions to prevent
  // hangs)
  const int kMaxInstructions = 1000000;
  int instruction_count = 0;

  auto& bp_manager = emulator_->breakpoint_manager();
  auto& cpu = emulator_->snes().cpu();

  while (instruction_count++ < kMaxInstructions) {
    uint32_t pc = (cpu.PB << 16) | cpu.PC;

    // Check for execute breakpoint
    if (bp_manager.ShouldBreakOnExecute(
            pc, emu::BreakpointManager::CpuType::CPU_65816)) {
      response->set_hit(true);
      response->set_message(
          absl::StrFormat("Breakpoint hit at 0x%06X after %d instructions", pc,
                          instruction_count));

      // Fill breakpoint info
      auto* last_hit = bp_manager.GetLastHit();
      if (last_hit) {
        auto* bp_info = response->mutable_breakpoint();
        bp_info->set_id(last_hit->id);
        bp_info->set_address(last_hit->address);
        bp_info->set_type(ToProtoBreakpointType(last_hit->type));
        bp_info->set_cpu(ToProtoCpuType(last_hit->cpu));
        bp_info->set_enabled(last_hit->enabled);
        bp_info->set_condition(last_hit->condition);
        bp_info->set_description(last_hit->description);
        bp_info->set_hit_count(last_hit->hit_count);
      }

      // Fill CPU state
      auto* cpu_state = response->mutable_cpu_state();
      cpu_state->set_pc(cpu.PC);
      cpu_state->set_pb(cpu.PB);
      cpu_state->set_a(cpu.A);
      cpu_state->set_x(cpu.X);
      cpu_state->set_y(cpu.Y);
      cpu_state->set_sp(cpu.SP());
      cpu_state->set_db(cpu.DB);
      cpu_state->set_d(cpu.D);

      return grpc::Status::OK;
    }

    // Execute instruction
    emulator_->StepSingleInstruction();
  }

  // No breakpoint hit
  response->set_hit(false);
  response->set_message(absl::StrFormat(
      "No breakpoint hit after %d instructions (timeout)", kMaxInstructions));
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::StepOver(grpc::ServerContext* context,
                                           const Empty* request,
                                           StepResponse* response) {
  if (!emulator_ || !emulator_->is_snes_initialized()) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "SNES is not initialized.");
  }

  // Initialize step controller with emulator callbacks
  InitializeStepController();

  // Execute step over
  auto result = step_controller_.StepOver();

  // Fill response
  auto& cpu = emulator_->snes().cpu();
  auto* cpu_state = response->mutable_cpu_state();
  cpu_state->set_a(cpu.A);
  cpu_state->set_x(cpu.X);
  cpu_state->set_y(cpu.Y);
  cpu_state->set_sp(cpu.SP());
  cpu_state->set_pc(cpu.PC);
  cpu_state->set_db(cpu.DB);
  cpu_state->set_pb(cpu.PB);
  cpu_state->set_d(cpu.D);
  cpu_state->set_status(cpu.status);
  cpu_state->set_flag_n(cpu.GetNegativeFlag());
  cpu_state->set_flag_v(cpu.GetOverflowFlag());
  cpu_state->set_flag_d(cpu.GetDecimalFlag());
  cpu_state->set_flag_i(cpu.GetInterruptFlag());
  cpu_state->set_flag_z(cpu.GetZeroFlag());
  cpu_state->set_flag_c(cpu.GetCarryFlag());
  cpu_state->set_cycles(emulator_->GetCurrentCycle());

  response->set_success(result.success);
  response->set_message(result.message);
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::StepOut(grpc::ServerContext* context,
                                          const Empty* request,
                                          StepResponse* response) {
  if (!emulator_ || !emulator_->is_snes_initialized()) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "SNES is not initialized.");
  }

  // Initialize step controller with emulator callbacks
  InitializeStepController();

  // Check if we have a call stack to step out of
  if (step_controller_.GetCallDepth() == 0) {
    response->set_success(false);
    response->set_message("Cannot step out - call stack is empty");
    return grpc::Status::OK;
  }

  // Execute step out
  auto result = step_controller_.StepOut();

  // Fill response
  auto& cpu = emulator_->snes().cpu();
  auto* cpu_state = response->mutable_cpu_state();
  cpu_state->set_a(cpu.A);
  cpu_state->set_x(cpu.X);
  cpu_state->set_y(cpu.Y);
  cpu_state->set_sp(cpu.SP());
  cpu_state->set_pc(cpu.PC);
  cpu_state->set_db(cpu.DB);
  cpu_state->set_pb(cpu.PB);
  cpu_state->set_d(cpu.D);
  cpu_state->set_status(cpu.status);
  cpu_state->set_flag_n(cpu.GetNegativeFlag());
  cpu_state->set_flag_v(cpu.GetOverflowFlag());
  cpu_state->set_flag_d(cpu.GetDecimalFlag());
  cpu_state->set_flag_i(cpu.GetInterruptFlag());
  cpu_state->set_flag_z(cpu.GetZeroFlag());
  cpu_state->set_flag_c(cpu.GetCarryFlag());
  cpu_state->set_cycles(emulator_->GetCurrentCycle());

  response->set_success(result.success);
  response->set_message(result.message);
  return grpc::Status::OK;
}

void EmulatorServiceImpl::InitializeStepController() {
  auto& memory = emulator_->snes().memory();
  auto& cpu = emulator_->snes().cpu();

  // Set up memory reader
  step_controller_.SetMemoryReader([&memory](uint32_t addr) -> uint8_t {
    return memory.ReadByte(addr);
  });

  // Set up single step function
  step_controller_.SetSingleStepper([this]() {
    emulator_->StepSingleInstruction();
  });

  // Set up PC getter
  step_controller_.SetPcGetter([&cpu]() -> uint32_t {
    return (cpu.PB << 16) | cpu.PC;
  });
}

// Disassembly
grpc::Status EmulatorServiceImpl::GetDisassembly(
    grpc::ServerContext* context, const DisassemblyRequest* request,
    DisassemblyResponse* response) {
  if (!emulator_ || !emulator_->is_snes_initialized()) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "SNES is not initialized.");
  }

  auto& cpu = emulator_->snes().cpu();
  auto& memory = emulator_->snes().memory();
  auto& bp_manager = emulator_->breakpoint_manager();

  // Create disassembler instance
  emu::debug::Disassembler65816 disassembler;

  // Memory reader lambda that captures our memory reference
  auto read_byte = [&memory](uint32_t addr) -> uint8_t {
    return memory.ReadByte(addr);
  };

  // Get CPU flags for proper operand size handling
  // M flag: true = 8-bit accumulator, false = 16-bit
  // X flag: true = 8-bit index registers, false = 16-bit
  bool m_flag = cpu.GetAccumulatorSize();  // true if 8-bit mode
  bool x_flag = cpu.GetIndexSize();        // true if 8-bit mode

  uint32_t current_address = request->start_address();
  uint32_t instructions_added = 0;
  const uint32_t max_instructions = std::min(request->count(), 1000u);

  // Build set of breakpoint addresses for quick lookup
  auto breakpoints =
      bp_manager.GetBreakpoints(emu::BreakpointManager::CpuType::CPU_65816);
  std::unordered_set<uint32_t> bp_addresses;
  for (const auto& bp : breakpoints) {
    if (bp.enabled && bp.type == emu::BreakpointManager::Type::EXECUTE) {
      bp_addresses.insert(bp.address);
    }
  }

  while (instructions_added < max_instructions) {
    // Disassemble the instruction using our new disassembler
    auto instruction =
        disassembler.Disassemble(current_address, read_byte, m_flag, x_flag);

    auto* line = response->add_lines();
    line->set_address(instruction.address);
    line->set_opcode(instruction.opcode);
    line->set_mnemonic(instruction.mnemonic);
    line->set_operand_str(instruction.operand_str);
    line->set_size(instruction.size);
    line->set_execution_count(0);  // Would need DisassemblyViewer integration

    // Add operand bytes
    for (const auto& byte : instruction.operands) {
      line->add_operands(byte);
    }

    // Check if this address has an execute breakpoint
    line->set_is_breakpoint(bp_addresses.count(current_address) > 0);

    current_address += instruction.size;
    instructions_added++;
  }

  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::GetExecutionTrace(
    grpc::ServerContext* context, const TraceRequest* request,
    TraceResponse* response) {
  // TODO: Implement execution trace (requires trace buffer in CPU)
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
                      "Execution trace not yet implemented");
}

// Symbol Management
grpc::Status EmulatorServiceImpl::LoadSymbols(grpc::ServerContext* context,
                                              const SymbolFileRequest* request,
                                              CommandResponse* response) {
  std::string filepath = request->filepath();
  if (filepath.empty()) {
    response->set_success(false);
    response->set_message("No filepath specified");
    return grpc::Status::OK;
  }

  // Convert proto enum to SymbolFormat
  emu::debug::SymbolFormat format = emu::debug::SymbolFormat::kAuto;
  switch (request->format()) {
    case SymbolFormat::ASAR:
      format = emu::debug::SymbolFormat::kAsar;
      break;
    case SymbolFormat::WLA_DX:
      format = emu::debug::SymbolFormat::kWlaDx;
      break;
    case SymbolFormat::MESEN:
      format = emu::debug::SymbolFormat::kMesen;
      break;
    default:
      format = emu::debug::SymbolFormat::kAuto;
      break;
  }

  // Check if it's a directory (for loading multiple ASM files)
  std::filesystem::path path(filepath);
  absl::Status status;

  if (std::filesystem::is_directory(path)) {
    status = symbol_provider_.LoadAsarAsmDirectory(filepath);
  } else {
    status = symbol_provider_.LoadSymbolFile(filepath, format);
  }

  if (status.ok()) {
    response->set_success(true);
    response->set_message(
        absl::StrFormat("Loaded %zu symbols from %s",
                        symbol_provider_.GetSymbolCount(), filepath));
  } else {
    response->set_success(false);
    response->set_message(std::string(status.message().data(), status.message().size()));
  }

  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::ResolveSymbol(
    grpc::ServerContext* context, const SymbolLookupRequest* request,
    SymbolLookupResponse* response) {
  std::string symbol_name = request->symbol_name();

  // First try exact match
  auto symbol = symbol_provider_.FindSymbol(symbol_name);
  if (symbol) {
    response->set_found(true);
    response->set_symbol_name(symbol->name);
    response->set_address(symbol->address);
    response->set_type(symbol->is_local ? "local_label" : "label");
    response->set_description(symbol->comment);
    return grpc::Status::OK;
  }

  // Try wildcard match if the name contains wildcards
  if (symbol_name.find('*') != std::string::npos ||
      symbol_name.find('?') != std::string::npos) {
    auto matches = symbol_provider_.FindSymbolsMatching(symbol_name);
    if (!matches.empty()) {
      // Return the first match
      const auto& first_match = matches[0];
      response->set_found(true);
      response->set_symbol_name(first_match.name);
      response->set_address(first_match.address);
      response->set_type(first_match.is_local ? "local_label" : "label");
      response->set_description(
          absl::StrFormat("First of %zu matches", matches.size()));
      return grpc::Status::OK;
    }
  }

  response->set_found(false);
  response->set_symbol_name(symbol_name);
  response->set_description("Symbol not found");
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::GetSymbolAt(grpc::ServerContext* context,
                                              const AddressRequest* request,
                                              SymbolLookupResponse* response) {
  uint32_t address = request->address();

  // Try exact match first
  auto symbol = symbol_provider_.GetSymbol(address);
  if (symbol) {
    response->set_found(true);
    response->set_symbol_name(symbol->name);
    response->set_address(symbol->address);
    response->set_type(symbol->is_local ? "local_label" : "label");
    response->set_description(symbol->comment);
    return grpc::Status::OK;
  }

  // Try to find nearest symbol
  auto nearest = symbol_provider_.GetNearestSymbol(address);
  if (nearest && (address - nearest->address) <= 0x100) {
    // Within reasonable offset range (256 bytes)
    uint32_t offset = address - nearest->address;
    response->set_found(true);
    response->set_symbol_name(
        absl::StrFormat("%s+$%X", nearest->name, offset));
    response->set_address(address);
    response->set_type("offset");
    response->set_description(
        absl::StrFormat("Offset $%X from %s", offset, nearest->name));
    return grpc::Status::OK;
  }

  response->set_found(false);
  response->set_address(address);
  response->set_description(absl::StrFormat("No symbol at $%06X", address));
  return grpc::Status::OK;
}

// Debug Session
grpc::Status EmulatorServiceImpl::CreateDebugSession(
    grpc::ServerContext* context, const DebugSessionRequest* request,
    DebugSessionResponse* response) {
  if (!emulator_) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Emulator not initialized.");
  }

  // Enable debugging mode
  emulator_->set_debugging(true);

  response->set_success(true);
  response->set_session_id(request->session_name());
  response->set_message(
      absl::StrFormat("Debug session '%s' created", request->session_name()));
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::GetDebugStatus(
    grpc::ServerContext* context, const Empty* request,
    DebugStatusResponse* response) {
  if (!emulator_) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Emulator not initialized.");
  }

  response->set_is_running(emulator_->running());
  response->set_is_paused(!emulator_->running());
  response->set_fps(emulator_->GetCurrentFPS());
  response->set_cycles(emulator_->GetCurrentCycle());

  // Get active counts
  auto breakpoints = emulator_->breakpoint_manager().GetAllBreakpoints();
  uint32_t active_bp_count = 0;
  for (const auto& bp : breakpoints) {
    if (bp.enabled)
      active_bp_count++;
  }
  response->set_active_breakpoints(active_bp_count);
  response->set_active_watchpoints(
      0);  // TODO: When WatchpointManager is integrated

  // Fill CPU state
  auto& cpu = emulator_->snes().cpu();
  auto* cpu_state = response->mutable_cpu_state();
  cpu_state->set_a(cpu.A);
  cpu_state->set_x(cpu.X);
  cpu_state->set_y(cpu.Y);
  cpu_state->set_sp(cpu.SP());
  cpu_state->set_pc(cpu.PC);
  cpu_state->set_db(cpu.DB);
  cpu_state->set_pb(cpu.PB);
  cpu_state->set_d(cpu.D);
  cpu_state->set_status(cpu.status);
  cpu_state->set_flag_n(cpu.GetNegativeFlag());
  cpu_state->set_flag_v(cpu.GetOverflowFlag());
  cpu_state->set_flag_d(cpu.GetDecimalFlag());
  cpu_state->set_flag_i(cpu.GetInterruptFlag());
  cpu_state->set_flag_z(cpu.GetZeroFlag());
  cpu_state->set_flag_c(cpu.GetCarryFlag());
  cpu_state->set_cycles(emulator_->GetCurrentCycle());

  // Last breakpoint hit
  auto* last_hit = emulator_->breakpoint_manager().GetLastHit();
  if (last_hit) {
    auto* bp_info = response->mutable_last_breakpoint_hit();
    bp_info->set_id(last_hit->id);
    bp_info->set_address(last_hit->address);
    bp_info->set_type(ToProtoBreakpointType(last_hit->type));
    bp_info->set_cpu(ToProtoCpuType(last_hit->cpu));
    bp_info->set_enabled(last_hit->enabled);
    bp_info->set_hit_count(last_hit->hit_count);
  }

  return grpc::Status::OK;
}

}  // namespace yaze::agent
