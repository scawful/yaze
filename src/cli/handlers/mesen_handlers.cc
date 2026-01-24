#include "cli/handlers/mesen_handlers.h"

#include <sstream>

#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"

namespace yaze {
namespace cli {
namespace handlers {

namespace {
std::shared_ptr<emu::mesen::MesenSocketClient> g_mesen_client;

absl::Status EnsureConnected() {
  if (!g_mesen_client) {
    g_mesen_client = std::make_shared<emu::mesen::MesenSocketClient>();
  }
  if (!g_mesen_client->IsConnected()) {
    auto status = g_mesen_client->Connect();
    if (!status.ok()) {
      return absl::UnavailableError(
          "Not connected to Mesen2. Is Mesen2-OoS running?");
    }
  }
  return absl::OkStatus();
}

std::string GetArgValue(const std::vector<std::string>& args,
                        const std::string& key,
                        const std::string& default_value = "") {
  std::string prefix = "--" + key + "=";
  for (const auto& arg : args) {
    if (arg.find(prefix) == 0) {
      return arg.substr(prefix.length());
    }
  }
  return default_value;
}

uint32_t ParseHexArg(const std::string& value) {
  uint32_t result = 0;
  std::string hex = value;
  if (hex.length() > 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) {
    hex = hex.substr(2);
  }
  absl::SimpleHexAtoi(hex, &result);
  return result;
}

}  // namespace

// Static client accessor
std::shared_ptr<emu::mesen::MesenSocketClient>& MesenClientHolder::GetClient() {
  return g_mesen_client;
}

void MesenClientHolder::SetClient(
    std::shared_ptr<emu::mesen::MesenSocketClient> client) {
  g_mesen_client = std::move(client);
}

// MesenGamestateCommandHandler
absl::Status MesenGamestateCommandHandler::Execute(
    const std::vector<std::string>& args, std::string* output) {
  (void)args;
  auto status = EnsureConnected();
  if (!status.ok()) return status;

  auto result = g_mesen_client->GetGameState();
  if (!result.ok()) return result.status();

  const auto& state = *result;
  std::stringstream ss;
  ss << "=== ALTTP Game State ===\n";
  ss << absl::StrFormat("Link: X=%d, Y=%d, Layer=%d, Direction=%d\n",
                        state.link.x, state.link.y, state.link.layer,
                        state.link.direction);
  ss << absl::StrFormat("Health: %d/%d, Magic: %d\n", state.items.current_health,
                        state.items.max_health, state.items.magic);
  ss << absl::StrFormat("Rupees: %d, Bombs: %d, Arrows: %d\n",
                        state.items.rupees, state.items.bombs,
                        state.items.arrows);
  ss << absl::StrFormat("Mode: %d, Submode: %d, Indoors: %s\n", state.game.mode,
                        state.game.submode, state.game.indoors ? "yes" : "no");
  if (state.game.indoors) {
    ss << absl::StrFormat("Room: 0x%04X\n", state.game.room_id);
  } else {
    ss << absl::StrFormat("Overworld Area: 0x%02X\n", state.game.overworld_area);
  }
  *output = ss.str();
  return absl::OkStatus();
}

// MesenSpritesCommandHandler
absl::Status MesenSpritesCommandHandler::Execute(
    const std::vector<std::string>& args, std::string* output) {
  auto status = EnsureConnected();
  if (!status.ok()) return status;

  bool show_all = GetArgValue(args, "all", "false") == "true";
  auto result = g_mesen_client->GetSprites(show_all);
  if (!result.ok()) return result.status();

  std::stringstream ss;
  ss << "=== Active Sprites ===\n";
  for (const auto& sprite : *result) {
    ss << absl::StrFormat("[%d] Type=0x%02X State=%d @ (%d,%d) HP=%d\n",
                          sprite.slot, sprite.type, sprite.state, sprite.x,
                          sprite.y, sprite.health);
  }
  if (result->empty()) {
    ss << "(No active sprites)\n";
  }
  *output = ss.str();
  return absl::OkStatus();
}

// MesenCpuCommandHandler
absl::Status MesenCpuCommandHandler::Execute(
    const std::vector<std::string>& args, std::string* output) {
  (void)args;
  auto status = EnsureConnected();
  if (!status.ok()) return status;

  auto result = g_mesen_client->GetCpuState();
  if (!result.ok()) return result.status();

  const auto& cpu = *result;
  std::stringstream ss;
  ss << "=== CPU State ===\n";
  ss << absl::StrFormat("PC=$%02X:%04X  A=$%04X  X=$%04X  Y=$%04X\n", cpu.K,
                        cpu.PC & 0xFFFF, cpu.A, cpu.X, cpu.Y);
  ss << absl::StrFormat("SP=$%04X  D=$%04X  DBR=$%02X  P=$%02X\n", cpu.SP, cpu.D,
                        cpu.DBR, cpu.P);
  ss << absl::StrFormat("Emulation: %s\n", cpu.emulation_mode ? "yes" : "no");
  *output = ss.str();
  return absl::OkStatus();
}

// MesenMemoryReadCommandHandler
absl::Status MesenMemoryReadCommandHandler::Execute(
    const std::vector<std::string>& args, std::string* output) {
  auto status = EnsureConnected();
  if (!status.ok()) return status;

  std::string addr_str = GetArgValue(args, "address");
  if (addr_str.empty()) {
    return absl::InvalidArgumentError("--address is required");
  }

  uint32_t addr = ParseHexArg(addr_str);
  int length = 16;
  std::string len_str = GetArgValue(args, "length", "16");
  absl::SimpleAtoi(len_str, &length);

  auto result = g_mesen_client->ReadBlock(addr, length);
  if (!result.ok()) return result.status();

  std::stringstream ss;
  ss << absl::StrFormat("=== Memory @ $%06X ===\n", addr);
  for (size_t i = 0; i < result->size(); i += 16) {
    ss << absl::StrFormat("$%06X: ", addr + i);
    for (size_t j = 0; j < 16 && i + j < result->size(); ++j) {
      ss << absl::StrFormat("%02X ", (*result)[i + j]);
    }
    ss << "\n";
  }
  *output = ss.str();
  return absl::OkStatus();
}

// MesenMemoryWriteCommandHandler
absl::Status MesenMemoryWriteCommandHandler::Execute(
    const std::vector<std::string>& args, std::string* output) {
  auto status = EnsureConnected();
  if (!status.ok()) return status;

  std::string addr_str = GetArgValue(args, "address");
  std::string data_str = GetArgValue(args, "data");
  if (addr_str.empty() || data_str.empty()) {
    return absl::InvalidArgumentError("--address and --data are required");
  }

  uint32_t addr = ParseHexArg(addr_str);

  // Parse hex bytes
  std::vector<uint8_t> data;
  for (size_t i = 0; i + 1 < data_str.length(); i += 2) {
    int byte;
    if (absl::SimpleHexAtoi(data_str.substr(i, 2), &byte)) {
      data.push_back(static_cast<uint8_t>(byte));
    }
  }

  auto write_status = g_mesen_client->WriteBlock(addr, data);
  if (!write_status.ok()) return write_status;

  *output = absl::StrFormat("Wrote %zu bytes to $%06X\n", data.size(), addr);
  return absl::OkStatus();
}

// MesenDisasmCommandHandler
absl::Status MesenDisasmCommandHandler::Execute(
    const std::vector<std::string>& args, std::string* output) {
  auto status = EnsureConnected();
  if (!status.ok()) return status;

  std::string addr_str = GetArgValue(args, "address");
  if (addr_str.empty()) {
    return absl::InvalidArgumentError("--address is required");
  }

  uint32_t addr = ParseHexArg(addr_str);
  int count = 10;
  std::string count_str = GetArgValue(args, "count", "10");
  absl::SimpleAtoi(count_str, &count);

  auto result = g_mesen_client->Disassemble(addr, count);
  if (!result.ok()) return result.status();

  *output = absl::StrFormat("=== Disassembly @ $%06X ===\n%s", addr, *result);
  return absl::OkStatus();
}

// MesenTraceCommandHandler
absl::Status MesenTraceCommandHandler::Execute(
    const std::vector<std::string>& args, std::string* output) {
  auto status = EnsureConnected();
  if (!status.ok()) return status;

  int count = 20;
  std::string count_str = GetArgValue(args, "count", "20");
  absl::SimpleAtoi(count_str, &count);

  auto result = g_mesen_client->GetTrace(count);
  if (!result.ok()) return result.status();

  *output = absl::StrFormat("=== Execution Trace (last %d) ===\n%s", count,
                            *result);
  return absl::OkStatus();
}

// MesenBreakpointCommandHandler
absl::Status MesenBreakpointCommandHandler::Execute(
    const std::vector<std::string>& args, std::string* output) {
  auto status = EnsureConnected();
  if (!status.ok()) return status;

  std::string action = GetArgValue(args, "action");
  if (action.empty()) {
    return absl::InvalidArgumentError(
        "--action is required (add, remove, clear, list)");
  }

  if (action == "add") {
    std::string addr_str = GetArgValue(args, "address");
    if (addr_str.empty()) {
      return absl::InvalidArgumentError("--address required for add");
    }
    uint32_t addr = ParseHexArg(addr_str);

    std::string type_str = GetArgValue(args, "type", "exec");
    emu::mesen::BreakpointType type = emu::mesen::BreakpointType::kExecute;
    if (type_str == "read") type = emu::mesen::BreakpointType::kRead;
    else if (type_str == "write") type = emu::mesen::BreakpointType::kWrite;
    else if (type_str == "rw") type = emu::mesen::BreakpointType::kReadWrite;

    auto result = g_mesen_client->AddBreakpoint(addr, type);
    if (!result.ok()) return result.status();
    *output = absl::StrFormat("Added breakpoint #%d at $%06X\n", *result, addr);

  } else if (action == "remove") {
    std::string id_str = GetArgValue(args, "id");
    if (id_str.empty()) {
      return absl::InvalidArgumentError("--id required for remove");
    }
    int id;
    absl::SimpleAtoi(id_str, &id);
    auto remove_status = g_mesen_client->RemoveBreakpoint(id);
    if (!remove_status.ok()) return remove_status;
    *output = absl::StrFormat("Removed breakpoint #%d\n", id);

  } else if (action == "clear") {
    auto clear_status = g_mesen_client->ClearBreakpoints();
    if (!clear_status.ok()) return clear_status;
    *output = "Cleared all breakpoints\n";

  } else if (action == "list") {
    *output = "Breakpoint listing not yet implemented\n";
  } else {
    return absl::InvalidArgumentError("Unknown action: " + action);
  }

  return absl::OkStatus();
}

// MesenControlCommandHandler
absl::Status MesenControlCommandHandler::Execute(
    const std::vector<std::string>& args, std::string* output) {
  auto status = EnsureConnected();
  if (!status.ok()) return status;

  std::string action = GetArgValue(args, "action");
  if (action.empty()) {
    return absl::InvalidArgumentError(
        "--action required (pause, resume, step, frame, reset)");
  }

  if (action == "pause") {
    auto result = g_mesen_client->Pause();
    if (!result.ok()) return result;
    *output = "Paused\n";
  } else if (action == "resume") {
    auto result = g_mesen_client->Resume();
    if (!result.ok()) return result;
    *output = "Resumed\n";
  } else if (action == "step") {
    auto result = g_mesen_client->Step(1);
    if (!result.ok()) return result;
    *output = "Stepped 1 instruction\n";
  } else if (action == "frame") {
    auto result = g_mesen_client->Frame();
    if (!result.ok()) return result;
    *output = "Advanced 1 frame\n";
  } else if (action == "reset") {
    auto result = g_mesen_client->Reset();
    if (!result.ok()) return result;
    *output = "Reset\n";
  } else {
    return absl::InvalidArgumentError("Unknown action: " + action);
  }

  return absl::OkStatus();
}

// Factory function
std::vector<std::unique_ptr<resources::CommandHandler>>
CreateMesenCommandHandlers() {
  std::vector<std::unique_ptr<resources::CommandHandler>> handlers;
  handlers.push_back(std::make_unique<MesenGamestateCommandHandler>());
  handlers.push_back(std::make_unique<MesenSpritesCommandHandler>());
  handlers.push_back(std::make_unique<MesenCpuCommandHandler>());
  handlers.push_back(std::make_unique<MesenMemoryReadCommandHandler>());
  handlers.push_back(std::make_unique<MesenMemoryWriteCommandHandler>());
  handlers.push_back(std::make_unique<MesenDisasmCommandHandler>());
  handlers.push_back(std::make_unique<MesenTraceCommandHandler>());
  handlers.push_back(std::make_unique<MesenBreakpointCommandHandler>());
  handlers.push_back(std::make_unique<MesenControlCommandHandler>());
  return handlers;
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
