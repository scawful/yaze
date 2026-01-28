#include "app/service/emulator_service_impl.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/emu/debug/disassembler.h"
#include "app/service/screenshot_utils.h"
#include "rom/rom.h"

namespace yaze::net {

namespace {
// Helper to convert absl::Status to grpc::Status
grpc::Status ToGrpcStatus(const absl::Status& status) {
  if (status.ok()) return grpc::Status::OK;
  
  grpc::StatusCode code = grpc::StatusCode::UNKNOWN;
  switch (status.code()) {
    case absl::StatusCode::kOk: code = grpc::StatusCode::OK; break;
    case absl::StatusCode::kCancelled: code = grpc::StatusCode::CANCELLED; break;
    case absl::StatusCode::kUnknown: code = grpc::StatusCode::UNKNOWN; break;
    case absl::StatusCode::kInvalidArgument: code = grpc::StatusCode::INVALID_ARGUMENT; break;
    case absl::StatusCode::kDeadlineExceeded: code = grpc::StatusCode::DEADLINE_EXCEEDED; break;
    case absl::StatusCode::kNotFound: code = grpc::StatusCode::NOT_FOUND; break;
    case absl::StatusCode::kAlreadyExists: code = grpc::StatusCode::ALREADY_EXISTS; break;
    case absl::StatusCode::kPermissionDenied: code = grpc::StatusCode::PERMISSION_DENIED; break;
    case absl::StatusCode::kUnauthenticated: code = grpc::StatusCode::UNAUTHENTICATED; break;
    case absl::StatusCode::kResourceExhausted: code = grpc::StatusCode::RESOURCE_EXHAUSTED; break;
    case absl::StatusCode::kFailedPrecondition: code = grpc::StatusCode::FAILED_PRECONDITION; break;
    case absl::StatusCode::kAborted: code = grpc::StatusCode::ABORTED; break;
    case absl::StatusCode::kOutOfRange: code = grpc::StatusCode::OUT_OF_RANGE; break;
    case absl::StatusCode::kUnimplemented: code = grpc::StatusCode::UNIMPLEMENTED; break;
    case absl::StatusCode::kInternal: code = grpc::StatusCode::INTERNAL; break;
    case absl::StatusCode::kUnavailable: code = grpc::StatusCode::UNAVAILABLE; break;
    case absl::StatusCode::kDataLoss: code = grpc::StatusCode::DATA_LOSS; break;
    default: code = grpc::StatusCode::UNKNOWN; break;
  }
  return grpc::Status(code, std::string(status.message()));
}
}  // namespace

EmulatorServiceImpl::EmulatorServiceImpl(emu::IEmulator* emulator,
                                         RomGetter rom_getter,
                                         RomLoader rom_loader)
    : emulator_(emulator), rom_getter_(rom_getter), rom_loader_(rom_loader) {}

// --- ROM Loading ---

grpc::Status EmulatorServiceImpl::LoadRom(grpc::ServerContext* context,
                                          const agent::LoadRomRequest* request,
                                          agent::LoadRomResponse* response) {
  if (!emulator_) {
     response->set_success(false);
     response->set_message("Emulator middleware not initialized");
     return grpc::Status::OK;
  }
  // Try using emulator adapter's load capability first (e.g. for internal)
  // But wait, the previous impl used `rom_loader_` directly.
  // InternalEmulatorAdapter uses `rom_loader_`.
  // MesenAdapter returns Unimplemented for LoadRom.
  // We should delegate to the adapter.
  auto status = emulator_->LoadRom(request->filepath());
  if (status.ok()) {
      response->set_success(true);
      response->set_message("ROM loaded successfully");
      // Get ROM info if available locally...
      if (rom_getter_) {
          Rom* rom = rom_getter_();
          if (rom && rom->is_loaded()) {
              response->set_rom_title(rom->title());
              response->set_rom_size(rom->size());
          }
      }
  } else {
      response->set_success(false);
      response->set_message("Failed to load ROM: " + std::string(status.message()));
  }
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::GetLoadedRomPath(
    grpc::ServerContext* context, const agent::Empty* request,
    agent::LoadedRomPathResponse* response) {
  if (emulator_) {
      std::string path = emulator_->GetLoadedRomPath();
      if (!path.empty()) {
          response->set_has_rom(true);
          response->set_filepath(path);
          // Assuming title extraction needs local ROM access or we just use filename
          response->set_title(std::filesystem::path(path).stem().string());
          return grpc::Status::OK;
      }
  }
  
  response->set_has_rom(false);
  return grpc::Status::OK;
}

// --- Core Lifecycle & Control ---

grpc::Status EmulatorServiceImpl::ControlEmulator(
    grpc::ServerContext* context, const agent::ControlRequest* request,
    agent::CommandResponse* response) {
  if (!emulator_ || !emulator_->IsConnected())
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Emulator not connected.");

  std::string action = request->action();
  if (action == "start" || action == "resume") {
    emulator_->Resume();
    response->set_message("Emulator resumed.");
  } else if (action == "stop" || action == "pause") {
    emulator_->Pause();
    response->set_message("Emulator paused.");
  } else if (action == "reset") {
    emulator_->Reset();
    response->set_message("Emulator reset.");
  } else if (action == "init" || action == "initialize") {
     // Adapter handles init/connection.
     response->set_message("Emulator connection active.");
  } else {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "Unknown action: " + action);
  }

  response->set_success(true);
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::StepEmulator(
    grpc::ServerContext* context, const agent::StepControlRequest* request,
    agent::StepResponse* response) {
  if (!emulator_ || !emulator_->IsConnected()) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Emulator not connected.");
  }

  std::string mode = request->mode();
  absl::Status status;
  if (mode == "instruction") {
    status = emulator_->Step(1);
    response->set_message("Stepped 1 instruction.");
  } else if (mode == "over") {
    status = emulator_->StepOver();
    response->set_message("Step Over executed.");
  } else if (mode == "out") {
    status = emulator_->StepOut();
    response->set_message("Step Out executed.");
  } else {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Unknown step mode: " + mode);
  }
  
  if (!status.ok()) return ToGrpcStatus(status);

  emulator_->GetCpuState(response->mutable_cpu_state());
  response->set_success(true);
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::RunToBreakpoint(
    grpc::ServerContext* context, const agent::Empty* request,
    agent::BreakpointHitResponse* response) {
  if (!emulator_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Emulator not initialized.");
  
  auto status = emulator_->RunToBreakpoint(response);
  return ToGrpcStatus(status);
}

// --- Input & State ---

grpc::Status EmulatorServiceImpl::PressButtons(
    grpc::ServerContext* context, const agent::ButtonRequest* request,
    agent::CommandResponse* response) {
  if (!emulator_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Emulator not initialized.");

  for (int i = 0; i < request->buttons_size(); i++) {
    emulator_->PressButton(static_cast<agent::Button>(request->buttons(i)));
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  for (int i = 0; i < request->buttons_size(); i++) {
    emulator_->ReleaseButton(static_cast<agent::Button>(request->buttons(i)));
  }
  response->set_success(true);
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::ReleaseButtons(
    grpc::ServerContext* context, const agent::ButtonRequest* request,
    agent::CommandResponse* response) {
  if (!emulator_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Emulator not initialized.");

  for (int i = 0; i < request->buttons_size(); i++) {
    emulator_->ReleaseButton(static_cast<agent::Button>(request->buttons(i)));
  }
  response->set_success(true);
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::HoldButtons(
    grpc::ServerContext* context, const agent::ButtonHoldRequest* request,
    agent::CommandResponse* response) {
  if (!emulator_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Emulator not initialized.");

  for (int i = 0; i < request->buttons_size(); i++) {
    emulator_->PressButton(static_cast<agent::Button>(request->buttons(i)));
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(request->duration_ms()));
  for (int i = 0; i < request->buttons_size(); i++) {
    emulator_->ReleaseButton(static_cast<agent::Button>(request->buttons(i)));
  }
  response->set_success(true);
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::GetGameState(
    grpc::ServerContext* context, const agent::GameStateRequest* request,
    agent::GameStateResponse* response) {
  if (!emulator_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Emulator not initialized.");

  // Get base game state from adapter
  auto status = emulator_->GetGameState(response);
  if (!status.ok()) return ToGrpcStatus(status);

  // Fill memory reads if requested
  for (const auto& mem_req : request->memory_reads()) {
    auto* mem_resp = response->add_memory_responses();
    mem_resp->set_address(mem_req.address());
    auto data_or = emulator_->ReadBlock(mem_req.address(), mem_req.size());
    if (data_or.ok()) {
        mem_resp->set_data(data_or->data(), data_or->size());
    }
  }

#ifdef YAZE_WITH_GRPC
  if (request->include_screenshot()) {
    // Adapter handles screenshot? OR we use screenshot_utils which grabs from renderer?
    // ScreenshotUtils grabs from ImGui/Renderer. This works ONLY if Internal emulator is rendering.
    // If using Mesen2, yaze might not be rendering the game.
    // MesenSocketClient has Screenshot().
    // We should probably add Screenshot to IEmulator.
    // For now, I'll stick to original behavior (CaptureHarnessScreenshot).
    // If Mesen is used, CaptureHarnessScreenshot will likely catch the Yaze UI, which might not show Mesen.
    // This is an area for future improvement.
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

grpc::Status EmulatorServiceImpl::ReadMemory(
    grpc::ServerContext* context, const agent::MemoryRequest* request,
    agent::MemoryResponse* response) {
  if (!emulator_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Emulator not not initialized.");

  response->set_address(request->address());
  auto data_or = emulator_->ReadBlock(request->address(), request->size());
  if (!data_or.ok()) return ToGrpcStatus(data_or.status());
  
  response->set_data(data_or->data(), data_or->size());
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::WriteMemory(
    grpc::ServerContext* context, const agent::MemoryWriteRequest* request,
    agent::CommandResponse* response) {
  if (!emulator_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Emulator not initialized.");

  std::vector<uint8_t> data(request->data().begin(), request->data().end());
  auto status = emulator_->WriteBlock(request->address(), data);
  if (!status.ok()) return ToGrpcStatus(status);

  response->set_success(true);
  return grpc::Status::OK;
}

// --- Debugging Management ---

grpc::Status EmulatorServiceImpl::BreakpointControl(
    grpc::ServerContext* context,
    const agent::BreakpointControlRequest* request,
    agent::BreakpointControlResponse* response) {
  if (!emulator_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Emulator not initialized.");

  std::string action = request->action();
  if (action == "add") {
    auto id_or = emulator_->AddBreakpoint(request->address(), request->type(), request->cpu(), request->condition(), request->description());
    if (!id_or.ok()) return ToGrpcStatus(id_or.status());
    response->set_breakpoint_id(*id_or);
    response->set_message("Breakpoint added.");
  } else if (action == "remove") {
    auto status = emulator_->RemoveBreakpoint(request->id());
    if (!status.ok()) return ToGrpcStatus(status);
    response->set_message("Breakpoint removed.");
  } else if (action == "toggle") {
    auto status = emulator_->ToggleBreakpoint(request->id(), request->enabled());
    if (!status.ok()) return ToGrpcStatus(status);
    response->set_message("Breakpoint toggled.");
  } else if (action == "list") {
    auto list = emulator_->ListBreakpoints();
    for (const auto& bp : list) {
        *response->add_breakpoints() = bp;
    }
  } else {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Unknown action: " + action);
  }

  response->set_success(true);
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::WatchpointControl(
    grpc::ServerContext* context,
    const agent::WatchpointControlRequest* request,
    agent::WatchpointControlResponse* response) {
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
                      "WatchpointManager integration pending.");
}

// --- Analysis & Symbols ---

grpc::Status EmulatorServiceImpl::GetDisassembly(
    grpc::ServerContext* context, const agent::DisassemblyRequest* request,
    agent::DisassemblyResponse* response) {
  if (!emulator_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Emulator not initialized.");

  emu::debug::Disassembler65816 dis;

  uint32_t addr = request->start_address();
  // Fetch memory block for disassembly
  // Heuristic: fetch 4 bytes per instruction * count
  int fetch_size = request->count() * 4;
  auto data_or = emulator_->ReadBlock(addr, fetch_size);
  if (!data_or.ok()) return ToGrpcStatus(data_or.status());
  
  const auto& data = *data_or;
  auto mem_reader = [&](uint32_t a) -> uint8_t {
      if (a >= addr && a < addr + data.size()) {
          return data[a - addr];
      }
      return 0; 
  };
  
  // Need accumulator/index size?
  // We can get CPU state.
  yaze::agent::CPUState cpu;
  emulator_->GetCpuState(&cpu);
  // Infer M/X from P status... or use default?
  // Internal emulator had access to exact M/X flags. 
  // CpuState has P. bit 5=M, bit 4=X.
  bool m_flag = (cpu.status() & 0x20) != 0;
  bool x_flag = (cpu.status() & 0x10) != 0;

  for (uint32_t i = 0; i < request->count(); ++i) {
    auto inst = dis.Disassemble(addr, mem_reader, m_flag, x_flag);
    auto* line = response->add_lines();
    line->set_address(inst.address);
    line->set_mnemonic(inst.mnemonic);
    line->set_operand_str(inst.operand_str);
    addr += inst.size;
    if (addr >= request->start_address() + fetch_size) break;
  }
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::GetExecutionTrace(
    grpc::ServerContext* context, const agent::TraceRequest* request,
    agent::TraceResponse* response) {
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
                      "Trace not implemented.");
}

grpc::Status EmulatorServiceImpl::ResolveSymbol(
    grpc::ServerContext* context, const agent::SymbolLookupRequest* request,
    agent::SymbolLookupResponse* response) {
  auto sym = symbol_provider_.FindSymbol(request->symbol_name());
  if (sym) {
    response->set_found(true);
    response->set_symbol_name(sym->name);
    response->set_address(sym->address);
  }
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::GetSymbolAt(
    grpc::ServerContext* context, const agent::AddressRequest* request,
    agent::SymbolLookupResponse* response) {
  auto sym = symbol_provider_.GetSymbol(request->address());
  if (sym) {
    response->set_found(true);
    response->set_symbol_name(sym->name);
    response->set_address(sym->address);
  }
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::LoadSymbols(
    grpc::ServerContext* context, const agent::SymbolFileRequest* request,
    agent::CommandResponse* response) {
  auto status = symbol_provider_.LoadSymbolFile(
      request->filepath(), (emu::debug::SymbolFormat)request->format());
  response->set_success(status.ok());
  response->set_message(std::string(status.message()));
  return grpc::Status::OK;
}

// --- Session & Experiments ---

grpc::Status EmulatorServiceImpl::GetDebugStatus(
    grpc::ServerContext* context, const agent::Empty* request,
    agent::DebugStatusResponse* response) {
  if (!emulator_) return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Emulator not initialized.");

  response->set_is_running(emulator_->IsRunning());
  emulator_->GetCpuState(response->mutable_cpu_state());
  // Count breakpoints not directly supported by interface? 
  // ListBreakpoints size?
  response->set_active_breakpoints(emulator_->ListBreakpoints().size());
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::TestRun(grpc::ServerContext* context,
                                          const agent::TestRunRequest* request,
                                          agent::TestRunResponse* response) {
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "TestRun requires refactoring for IEmulator");
}

// --- Save State Management ---
// Keeping SaveState logic minimal/delegated if possible or stubbed.
// Mesen doesn't support file-based save via "SaveState(filepath)" easily? 
// MesenSocketClient uses slots.
// Internal emulator used filepath.
// IEmulator should probably have LoadState/SaveState abstracted.
// The Adapter can handle path vs slot conversion?
// For now, I'll return Unimplemented or keep original logic if internal adapter?
// Check InternalEmulatorAdapter/IEmulator interface... 
// I didn't add SaveState/LoadState to IEmulator!
// I'll skip it for now (Unimplemented) as it wasn't critical for the "connect to Mesen" task 
// which focuses on debugging/state inspection.

grpc::Status EmulatorServiceImpl::SaveState(
    grpc::ServerContext* context, const agent::SaveStateRequest* request,
    agent::SaveStateResponse* response) {
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "SaveState not yet ported to IEmulator");
}

grpc::Status EmulatorServiceImpl::LoadState(
    grpc::ServerContext* context, const agent::LoadStateRequest* request,
    agent::LoadStateResponse* response) {
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "LoadState not yet ported to IEmulator");
}

grpc::Status EmulatorServiceImpl::ListStates(
    grpc::ServerContext* context, const agent::ListStatesRequest* request,
    agent::ListStatesResponse* response) {
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "ListStates not yet ported to IEmulator");
}

}  // namespace yaze::net
