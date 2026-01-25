#include "cli/handlers/mesen_handlers.h"

#include <cctype>
#include <sstream>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "app/emu/mesen/mesen_client_registry.h"

ABSL_DECLARE_FLAG(std::string, mesen_socket);

namespace yaze {
namespace cli {
namespace handlers {

namespace {

::absl::Status EnsureConnected() {
  auto client = emu::mesen::MesenClientRegistry::GetOrCreate();
  if (!client->IsConnected()) {
    std::string socket_path = ::absl::GetFlag(FLAGS_mesen_socket);
    auto status = socket_path.empty() ? client->Connect()
                                      : client->Connect(socket_path);
    if (!status.ok()) {
      return ::absl::UnavailableError(
          "Not connected to Mesen2. Is Mesen2-OoS running?");
    }
  }
  return ::absl::OkStatus();
}

::absl::StatusOr<int> ParseOptionalInt(
    const resources::ArgumentParser& parser,
    const std::string& name,
    int default_value) {
  if (!parser.GetString(name).has_value()) {
    return default_value;
  }
  return parser.GetInt(name);
}

std::vector<uint8_t> ParseHexBytes(const std::string& data_str) {
  std::string hex;
  hex.reserve(data_str.size());
  for (char c : data_str) {
    if (std::isxdigit(static_cast<unsigned char>(c))) {
      hex.push_back(c);
    }
  }

  std::vector<uint8_t> data;
  for (size_t i = 0; i + 1 < hex.size(); i += 2) {
    int byte = 0;
    if (::absl::SimpleHexAtoi(hex.substr(i, 2), &byte)) {
      data.push_back(static_cast<uint8_t>(byte));
    }
  }
  return data;
}

}  // namespace

// MesenGamestateCommandHandler
::absl::Status MesenGamestateCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  (void)parser;
  return ::absl::OkStatus();
}

::absl::Status MesenGamestateCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  (void)rom;
  (void)parser;
  auto status = EnsureConnected();
  if (!status.ok()) return status;

  auto client = emu::mesen::MesenClientRegistry::GetOrCreate();
  auto result = client->GetGameState();
  if (!result.ok()) return result.status();

  const auto& state = *result;
  formatter.AddField("link_x", state.link.x);
  formatter.AddField("link_y", state.link.y);
  formatter.AddField("link_layer", state.link.layer);
  formatter.AddField("link_direction", static_cast<int>(state.link.direction));
  formatter.AddField("health", static_cast<int>(state.items.current_health));
  formatter.AddField("max_health", static_cast<int>(state.items.max_health));
  formatter.AddField("magic", static_cast<int>(state.items.magic));
  formatter.AddField("rupees", static_cast<int>(state.items.rupees));
  formatter.AddField("bombs", static_cast<int>(state.items.bombs));
  formatter.AddField("arrows", static_cast<int>(state.items.arrows));
  formatter.AddField("mode", static_cast<int>(state.game.mode));
  formatter.AddField("submode", static_cast<int>(state.game.submode));
  formatter.AddField("indoors", state.game.indoors);
  if (state.game.indoors) {
    formatter.AddHexField("room_id", state.game.room_id, 4);
  } else {
    formatter.AddHexField("overworld_area", state.game.overworld_area, 2);
  }

  return ::absl::OkStatus();
}

// MesenSpritesCommandHandler
::absl::Status MesenSpritesCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  (void)parser;
  return ::absl::OkStatus();
}

::absl::Status MesenSpritesCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  (void)rom;
  auto status = EnsureConnected();
  if (!status.ok()) return status;

  bool show_all = parser.HasFlag("all");
  auto client = emu::mesen::MesenClientRegistry::GetOrCreate();
  auto result = client->GetSprites(show_all);
  if (!result.ok()) return result.status();

  formatter.AddField("count", static_cast<int>(result->size()));
  formatter.BeginArray("sprites");
  for (const auto& sprite : *result) {
    formatter.AddArrayItem(
        ::absl::StrFormat("[#%d] type=0x%02X state=%d @(%d,%d) hp=%d",
                        sprite.slot, sprite.type, sprite.state, sprite.x,
                        sprite.y, sprite.health));
  }
  formatter.EndArray();

  return ::absl::OkStatus();
}

// MesenCpuCommandHandler
::absl::Status MesenCpuCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  (void)parser;
  return ::absl::OkStatus();
}

::absl::Status MesenCpuCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  (void)rom;
  (void)parser;
  auto status = EnsureConnected();
  if (!status.ok()) return status;

  auto client = emu::mesen::MesenClientRegistry::GetOrCreate();
  auto result = client->GetCpuState();
  if (!result.ok()) return result.status();

  const auto& cpu = *result;
  uint32_t pc = (static_cast<uint32_t>(cpu.K) << 16) | (cpu.PC & 0xFFFF);
  formatter.AddHexField("pc", pc, 6);
  formatter.AddHexField("a", cpu.A, 4);
  formatter.AddHexField("x", cpu.X, 4);
  formatter.AddHexField("y", cpu.Y, 4);
  formatter.AddHexField("sp", cpu.SP, 4);
  formatter.AddHexField("d", cpu.D, 4);
  formatter.AddHexField("dbr", cpu.DBR, 2);
  formatter.AddHexField("p", cpu.P, 2);
  formatter.AddField("emulation_mode", cpu.emulation_mode);

  return ::absl::OkStatus();
}

// MesenMemoryReadCommandHandler
::absl::Status MesenMemoryReadCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return parser.RequireArgs({"address"});
}

::absl::Status MesenMemoryReadCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  (void)rom;
  auto status = EnsureConnected();
  if (!status.ok()) return status;

  auto addr_or = parser.GetHex("address");
  if (!addr_or.ok()) return addr_or.status();
  uint32_t addr = static_cast<uint32_t>(*addr_or);

  auto length_or = ParseOptionalInt(parser, "length", 16);
  if (!length_or.ok()) return length_or.status();
  int length = *length_or;

  auto client = emu::mesen::MesenClientRegistry::GetOrCreate();
  auto result = client->ReadBlock(addr, length);
  if (!result.ok()) return result.status();

  formatter.AddHexField("address", addr, 6);
  formatter.AddField("length", static_cast<int>(result->size()));
  formatter.BeginArray("bytes");
  for (uint8_t byte : *result) {
    formatter.AddArrayItem(::absl::StrFormat("%02X", byte));
  }
  formatter.EndArray();

  return ::absl::OkStatus();
}

// MesenMemoryWriteCommandHandler
::absl::Status MesenMemoryWriteCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return parser.RequireArgs({"address", "data"});
}

::absl::Status MesenMemoryWriteCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  (void)rom;
  auto status = EnsureConnected();
  if (!status.ok()) return status;

  auto addr_or = parser.GetHex("address");
  if (!addr_or.ok()) return addr_or.status();
  uint32_t addr = static_cast<uint32_t>(*addr_or);

  auto data_str = parser.GetString("data");
  if (!data_str.has_value()) {
    return ::absl::InvalidArgumentError("--data is required");
  }

  auto data = ParseHexBytes(*data_str);
  if (data.empty()) {
    return ::absl::InvalidArgumentError("Invalid --data hex string");
  }

  auto client = emu::mesen::MesenClientRegistry::GetOrCreate();
  auto write_status = client->WriteBlock(addr, data);
  if (!write_status.ok()) return write_status;

  formatter.AddHexField("address", addr, 6);
  formatter.AddField("bytes_written", static_cast<int>(data.size()));
  formatter.AddField("status", "ok");
  return ::absl::OkStatus();
}

// MesenDisasmCommandHandler
::absl::Status MesenDisasmCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return parser.RequireArgs({"address"});
}

::absl::Status MesenDisasmCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  (void)rom;
  auto status = EnsureConnected();
  if (!status.ok()) return status;

  auto addr_or = parser.GetHex("address");
  if (!addr_or.ok()) return addr_or.status();
  uint32_t addr = static_cast<uint32_t>(*addr_or);

  auto count_or = ParseOptionalInt(parser, "count", 10);
  if (!count_or.ok()) return count_or.status();
  int count = *count_or;

  auto client = emu::mesen::MesenClientRegistry::GetOrCreate();
  auto result = client->Disassemble(addr, count);
  if (!result.ok()) return result.status();

  formatter.AddHexField("address", addr, 6);
  formatter.AddField("disassembly", *result);
  return ::absl::OkStatus();
}

// MesenTraceCommandHandler
::absl::Status MesenTraceCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  (void)parser;
  return ::absl::OkStatus();
}

::absl::Status MesenTraceCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  (void)rom;
  auto status = EnsureConnected();
  if (!status.ok()) return status;

  auto count_or = ParseOptionalInt(parser, "count", 20);
  if (!count_or.ok()) return count_or.status();
  int count = *count_or;

  auto client = emu::mesen::MesenClientRegistry::GetOrCreate();
  auto result = client->GetTrace(count);
  if (!result.ok()) return result.status();

  formatter.AddField("count", count);
  formatter.AddField("trace", *result);
  return ::absl::OkStatus();
}

// MesenBreakpointCommandHandler
::absl::Status MesenBreakpointCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return parser.RequireArgs({"action"});
}

::absl::Status MesenBreakpointCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  (void)rom;
  auto status = EnsureConnected();
  if (!status.ok()) return status;

  auto action = parser.GetString("action");
  if (!action.has_value()) {
    return ::absl::InvalidArgumentError("--action is required");
  }

  if (*action == "add") {
    auto addr_or = parser.GetHex("address");
    if (!addr_or.ok()) return addr_or.status();
    uint32_t addr = static_cast<uint32_t>(*addr_or);

    std::string type_str = parser.GetString("type").value_or("exec");
    emu::mesen::BreakpointType type = emu::mesen::BreakpointType::kExecute;
    if (type_str == "read")
      type = emu::mesen::BreakpointType::kRead;
    else if (type_str == "write")
      type = emu::mesen::BreakpointType::kWrite;
    else if (type_str == "rw")
      type = emu::mesen::BreakpointType::kReadWrite;

    auto client = emu::mesen::MesenClientRegistry::GetOrCreate();
    auto result = client->AddBreakpoint(addr, type);
    if (!result.ok()) return result.status();
    formatter.AddField("breakpoint_id", *result);
    formatter.AddHexField("address", addr, 6);
    formatter.AddField("status", "added");
  } else if (*action == "remove") {
    auto id_or = parser.GetInt("id");
    if (!id_or.ok()) return id_or.status();
    int id = *id_or;

    auto client = emu::mesen::MesenClientRegistry::GetOrCreate();
    auto remove_status = client->RemoveBreakpoint(id);
    if (!remove_status.ok()) return remove_status;
    formatter.AddField("breakpoint_id", id);
    formatter.AddField("status", "removed");
  } else if (*action == "clear") {
    auto client = emu::mesen::MesenClientRegistry::GetOrCreate();
    auto clear_status = client->ClearBreakpoints();
    if (!clear_status.ok()) return clear_status;
    formatter.AddField("status", "cleared");
  } else if (*action == "list") {
    formatter.AddField("status", "not_implemented");
  } else {
    return ::absl::InvalidArgumentError("Unknown action: " + *action);
  }

  return ::absl::OkStatus();
}

// MesenControlCommandHandler
::absl::Status MesenControlCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return parser.RequireArgs({"action"});
}

::absl::Status MesenControlCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  (void)rom;
  auto status = EnsureConnected();
  if (!status.ok()) return status;

  auto action = parser.GetString("action");
  if (!action.has_value()) {
    return ::absl::InvalidArgumentError("--action required");
  }

  auto client = emu::mesen::MesenClientRegistry::GetOrCreate();
  if (*action == "pause") {
    auto result = client->Pause();
    if (!result.ok()) return result;
  } else if (*action == "resume") {
    auto result = client->Resume();
    if (!result.ok()) return result;
  } else if (*action == "step") {
    auto result = client->Step(1);
    if (!result.ok()) return result;
  } else if (*action == "frame") {
    auto result = client->Frame();
    if (!result.ok()) return result;
  } else if (*action == "reset") {
    auto result = client->Reset();
    if (!result.ok()) return result;
  } else {
    return ::absl::InvalidArgumentError("Unknown action: " + *action);
  }

  formatter.AddField("status", "ok");
  formatter.AddField("action", *action);
  return ::absl::OkStatus();
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
