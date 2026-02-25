#include "app/service/emulator_service_impl.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/emu/debug/disassembler.h"
#include "app/emu/proto_converter.h"
#include "app/service/screenshot_utils.h"
#include "rom/rom.h"

namespace yaze::net {

namespace {
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
  auto status = emulator_->LoadRom(request->filepath());
  if (status.ok()) {
      response->set_success(true);
      response->set_message("ROM loaded successfully");
      if (rom_getter_) {
          Rom* rom = rom_getter_();
          if (rom && rom->is_loaded()) {
              response->set_rom_title(rom->title());
              response->set_rom_size(rom->size());
          }
      }
  } else {
      response->set_success(false);
      response->set_message("Failed to load ROM: " +
                            std::string(status.message()));
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
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Emulator not connected.");

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
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Emulator not connected.");
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
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "Unknown step mode: " + mode);
  }

  if (!status.ok()) return ToGrpcStatus(status);

  emu::CpuStateSnapshot cpu_snap;
  auto cpu_status = emulator_->GetCpuState(&cpu_snap);
  if (!cpu_status.ok()) return ToGrpcStatus(cpu_status);
  emu::ToProtoCpuState(cpu_snap, response->mutable_cpu_state());
  response->set_success(true);
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::RunToBreakpoint(
    grpc::ServerContext* context, const agent::Empty* request,
    agent::BreakpointHitResponse* response) {
  if (!emulator_)
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Emulator not initialized.");

  emu::BreakpointHitResult result;
  auto status = emulator_->RunToBreakpoint(&result);
  if (!status.ok()) return ToGrpcStatus(status);
  emu::ToProtoBreakpointHitResponse(result, response);
  return grpc::Status::OK;
}

// --- Input & State ---

grpc::Status EmulatorServiceImpl::PressButtons(
    grpc::ServerContext* context, const agent::ButtonRequest* request,
    agent::CommandResponse* response) {
  if (!emulator_)
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Emulator not initialized.");

  std::vector<emu::InputButton> pressed_buttons;
  pressed_buttons.reserve(request->buttons_size());
  for (int i = 0; i < request->buttons_size(); i++) {
    auto btn = emu::FromProtoButton(
        static_cast<agent::Button>(request->buttons(i)));
    if (btn == emu::InputButton::kUnspecified) {
      return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                          absl::StrFormat("Invalid button at index %d: %d", i,
                                          request->buttons(i)));
    }
    auto press_status = emulator_->PressButton(btn);
    if (!press_status.ok()) {
      for (auto it = pressed_buttons.rbegin(); it != pressed_buttons.rend();
           ++it) {
        (void)emulator_->ReleaseButton(*it);
      }
      return ToGrpcStatus(press_status);
    }
    pressed_buttons.push_back(btn);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  for (auto it = pressed_buttons.rbegin(); it != pressed_buttons.rend(); ++it) {
    auto release_status = emulator_->ReleaseButton(*it);
    if (!release_status.ok()) return ToGrpcStatus(release_status);
  }
  response->set_success(true);
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::ReleaseButtons(
    grpc::ServerContext* context, const agent::ButtonRequest* request,
    agent::CommandResponse* response) {
  if (!emulator_)
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Emulator not initialized.");

  for (int i = 0; i < request->buttons_size(); i++) {
    auto btn = emu::FromProtoButton(
        static_cast<agent::Button>(request->buttons(i)));
    if (btn == emu::InputButton::kUnspecified) {
      return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                          absl::StrFormat("Invalid button at index %d: %d", i,
                                          request->buttons(i)));
    }
    auto status = emulator_->ReleaseButton(btn);
    if (!status.ok()) return ToGrpcStatus(status);
  }
  response->set_success(true);
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::HoldButtons(
    grpc::ServerContext* context, const agent::ButtonHoldRequest* request,
    agent::CommandResponse* response) {
  if (!emulator_)
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Emulator not initialized.");

  std::vector<emu::InputButton> held_buttons;
  held_buttons.reserve(request->buttons_size());
  for (int i = 0; i < request->buttons_size(); i++) {
    auto btn = emu::FromProtoButton(
        static_cast<agent::Button>(request->buttons(i)));
    if (btn == emu::InputButton::kUnspecified) {
      return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                          absl::StrFormat("Invalid button at index %d: %d", i,
                                          request->buttons(i)));
    }
    auto press_status = emulator_->PressButton(btn);
    if (!press_status.ok()) {
      for (auto it = held_buttons.rbegin(); it != held_buttons.rend(); ++it) {
        (void)emulator_->ReleaseButton(*it);
      }
      return ToGrpcStatus(press_status);
    }
    held_buttons.push_back(btn);
  }
  std::this_thread::sleep_for(
      std::chrono::milliseconds(request->duration_ms()));
  for (auto it = held_buttons.rbegin(); it != held_buttons.rend(); ++it) {
    auto release_status = emulator_->ReleaseButton(*it);
    if (!release_status.ok()) return ToGrpcStatus(release_status);
  }
  response->set_success(true);
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::GetGameState(
    grpc::ServerContext* context, const agent::GameStateRequest* request,
    agent::GameStateResponse* response) {
  if (!emulator_)
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Emulator not initialized.");

  emu::GameSnapshot snapshot;
  auto status = emulator_->GetGameState(&snapshot);
  if (!status.ok()) return ToGrpcStatus(status);
  emu::ToProtoGameState(snapshot, response);

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
  if (!emulator_)
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Emulator not initialized.");

  response->set_address(request->address());
  auto data_or = emulator_->ReadBlock(request->address(), request->size());
  if (!data_or.ok()) return ToGrpcStatus(data_or.status());

  response->set_data(data_or->data(), data_or->size());
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::WriteMemory(
    grpc::ServerContext* context, const agent::MemoryWriteRequest* request,
    agent::CommandResponse* response) {
  if (!emulator_)
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Emulator not initialized.");

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
  if (!emulator_)
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Emulator not initialized.");

  std::string action = request->action();
  if (action == "add") {
    auto id_or = emulator_->AddBreakpoint(
        request->address(),
        emu::FromProtoBreakpointType(request->type()),
        emu::FromProtoCpuType(request->cpu()),
        request->condition(), request->description());
    if (!id_or.ok()) return ToGrpcStatus(id_or.status());
    response->set_breakpoint_id(*id_or);
    response->set_message("Breakpoint added.");
  } else if (action == "remove") {
    auto status = emulator_->RemoveBreakpoint(request->id());
    if (!status.ok()) return ToGrpcStatus(status);
    response->set_message("Breakpoint removed.");
  } else if (action == "toggle") {
    auto status =
        emulator_->ToggleBreakpoint(request->id(), request->enabled());
    if (!status.ok()) return ToGrpcStatus(status);
    response->set_message("Breakpoint toggled.");
  } else if (action == "list") {
    auto list = emulator_->ListBreakpoints();
    for (const auto& bp_snap : list) {
        emu::ToProtoBreakpointInfo(bp_snap, response->add_breakpoints());
    }
  } else {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "Unknown action: " + action);
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
  if (!emulator_)
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Emulator not initialized.");

  emu::debug::Disassembler65816 dis;

  uint32_t addr = request->start_address();
  int fetch_size = request->count() * 4;
  auto data_or = emulator_->ReadBlock(addr, fetch_size);
  if (!data_or.ok()) return ToGrpcStatus(data_or.status());

  const auto& data = *data_or;
  auto mem_reader = [&](uint32_t read_addr) -> uint8_t {
      if (read_addr >= addr && read_addr < addr + data.size()) {
          return data[read_addr - addr];
      }
      return 0;
  };

  emu::CpuStateSnapshot cpu_snap;
  auto cpu_status = emulator_->GetCpuState(&cpu_snap);
  if (!cpu_status.ok()) return ToGrpcStatus(cpu_status);
  bool m_flag = (cpu_snap.status & 0x20) != 0;
  bool x_flag = (cpu_snap.status & 0x10) != 0;

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
      request->filepath(),
      static_cast<emu::debug::SymbolFormat>(request->format()));
  response->set_success(status.ok());
  response->set_message(std::string(status.message()));
  return grpc::Status::OK;
}

// --- Session & Experiments ---

grpc::Status EmulatorServiceImpl::GetDebugStatus(
    grpc::ServerContext* context, const agent::Empty* request,
    agent::DebugStatusResponse* response) {
  if (!emulator_)
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Emulator not initialized.");

  response->set_is_running(emulator_->IsRunning());

  emu::CpuStateSnapshot cpu_snap;
  auto cpu_status = emulator_->GetCpuState(&cpu_snap);
  if (!cpu_status.ok()) return ToGrpcStatus(cpu_status);
  emu::ToProtoCpuState(cpu_snap, response->mutable_cpu_state());

  response->set_active_breakpoints(emulator_->ListBreakpoints().size());
  return grpc::Status::OK;
}

grpc::Status EmulatorServiceImpl::TestRun(
    grpc::ServerContext* context, const agent::TestRunRequest* request,
    agent::TestRunResponse* response) {
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
                      "TestRun requires refactoring for IEmulator");
}

// --- Save State Management ---

grpc::Status EmulatorServiceImpl::SaveState(
    grpc::ServerContext* context, const agent::SaveStateRequest* request,
    agent::SaveStateResponse* response) {
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
                        "SaveState not yet ported to IEmulator");
}

grpc::Status EmulatorServiceImpl::LoadState(
    grpc::ServerContext* context, const agent::LoadStateRequest* request,
    agent::LoadStateResponse* response) {
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
                        "LoadState not yet ported to IEmulator");
}

grpc::Status EmulatorServiceImpl::ListStates(
    grpc::ServerContext* context, const agent::ListStatesRequest* request,
    agent::ListStatesResponse* response) {
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
                        "ListStates not yet ported to IEmulator");
}

}  // namespace yaze::net
