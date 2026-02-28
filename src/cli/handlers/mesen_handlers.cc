#include "cli/handlers/mesen_handlers.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <unordered_map>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/emu/mesen/mesen_client_registry.h"
#include "nlohmann/json.hpp"
#include "util/rom_hash.h"

ABSL_DECLARE_FLAG(std::string, mesen_socket);

namespace yaze {
namespace cli {
namespace handlers {

namespace {

struct EmulatorAgentSessionState {
  bool connected = false;
  bool running = false;
  bool paused = false;
  uint64_t frame = 0;
  uint32_t pc = 0;
  int last_breakpoint_id = -1;
  std::string last_action = "init";
  std::unordered_map<int, uint32_t> breakpoints_by_id;
};

EmulatorAgentSessionState& SessionState() {
  static EmulatorAgentSessionState state;
  return state;
}

void ResetSessionState() {
  SessionState() = EmulatorAgentSessionState{};
}

void UpdateSessionFromRuntime(
    const std::shared_ptr<emu::mesen::MesenSocketClient>& client) {
  auto& session = SessionState();
  session.connected = client && client->IsConnected();
  if (!session.connected) {
    return;
  }
  if (auto state_or = client->GetState(); state_or.ok()) {
    session.running = state_or->running;
    session.paused = state_or->paused;
    session.frame = state_or->frame;
  }
  if (auto cpu_or = client->GetCpuState(); cpu_or.ok()) {
    session.pc =
        (static_cast<uint32_t>(cpu_or->K) << 16) | (cpu_or->PC & 0xFFFF);
  }
}

int ClampTimeoutMs(int timeout_ms) {
  return std::max(1, timeout_ms);
}

int ClampPollMs(int poll_ms) {
  return std::clamp(poll_ms, 1, 1000);
}

std::string DefaultMetaPathForState(const std::string& state_path) {
  return state_path + ".meta.json";
}

::absl::Status EnsureFileExists(const std::string& path,
                                const char* label_for_errors) {
  std::error_code ec;
  if (!std::filesystem::exists(path, ec) || ec) {
    return ::absl::NotFoundError(
        ::absl::StrFormat("%s not found: %s", label_for_errors, path));
  }
  if (std::filesystem::is_directory(path, ec) || ec) {
    return ::absl::FailedPreconditionError(
        ::absl::StrFormat("%s is a directory: %s", label_for_errors, path));
  }
  return ::absl::OkStatus();
}

::absl::StatusOr<nlohmann::json> LoadJsonFile(const std::string& path) {
  std::ifstream in(path);
  if (!in.is_open()) {
    return ::absl::NotFoundError(::absl::StrFormat("File not found: %s", path));
  }
  nlohmann::json j;
  try {
    in >> j;
  } catch (const std::exception& e) {
    return ::absl::InvalidArgumentError(
        ::absl::StrFormat("Invalid JSON at %s: %s", path, e.what()));
  }
  return j;
}

::absl::Status WriteJsonFile(const std::string& path, const nlohmann::json& j) {
  std::ofstream out(path);
  if (!out.is_open()) {
    return ::absl::PermissionDeniedError(
        ::absl::StrFormat("Failed to open output file: %s", path));
  }
  out << j.dump(2);
  if (!out.good()) {
    return ::absl::InternalError(
        ::absl::StrFormat("Failed to write output file: %s", path));
  }
  return ::absl::OkStatus();
}

std::string OptionalScenario(const resources::ArgumentParser& parser) {
  return parser.GetString("scenario").value_or("");
}

struct SavestateFreshnessResult {
  bool fresh = false;
  std::string state_path;
  std::string rom_path;
  std::string meta_path;
  std::string expected_scenario;
  std::string recorded_scenario;
  std::string current_rom_sha1;
  std::string current_state_sha1;
  std::string recorded_rom_sha1;
  std::string recorded_state_sha1;
  std::vector<std::string> stale_reasons;
};

::absl::StatusOr<SavestateFreshnessResult> ComputeSavestateFreshness(
    const std::string& state_path, const std::string& rom_path,
    const std::string& meta_path, const std::string& expected_scenario) {
  auto state_status = EnsureFileExists(state_path, "state file");
  if (!state_status.ok()) {
    return state_status;
  }
  auto rom_status = EnsureFileExists(rom_path, "ROM file");
  if (!rom_status.ok()) {
    return rom_status;
  }
  auto meta_status = EnsureFileExists(meta_path, "metadata file");
  if (!meta_status.ok()) {
    return meta_status;
  }

  auto meta_or = LoadJsonFile(meta_path);
  if (!meta_or.ok()) {
    return meta_or.status();
  }
  const auto& meta = *meta_or;

  SavestateFreshnessResult result;
  result.state_path = state_path;
  result.rom_path = rom_path;
  result.meta_path = meta_path;
  result.expected_scenario = expected_scenario;
  result.current_rom_sha1 = util::ComputeFileSha1Hex(rom_path);
  result.current_state_sha1 = util::ComputeFileSha1Hex(state_path);
  if (result.current_rom_sha1.empty()) {
    return ::absl::InternalError("Failed to hash ROM file");
  }
  if (result.current_state_sha1.empty()) {
    return ::absl::InternalError("Failed to hash state file");
  }

  result.recorded_rom_sha1 = meta.value("rom_sha1", "");
  result.recorded_state_sha1 = meta.value("state_sha1", "");
  result.recorded_scenario = meta.value("scenario", "");

  const bool rom_match = !result.recorded_rom_sha1.empty() &&
                         result.recorded_rom_sha1 == result.current_rom_sha1;
  const bool state_match =
      !result.recorded_state_sha1.empty() &&
      result.recorded_state_sha1 == result.current_state_sha1;
  const bool scenario_match =
      result.expected_scenario.empty() ||
      result.expected_scenario == result.recorded_scenario;
  if (!rom_match) {
    result.stale_reasons.push_back("rom_sha1_mismatch");
  }
  if (!state_match) {
    result.stale_reasons.push_back("state_sha1_mismatch");
  }
  if (!scenario_match) {
    result.stale_reasons.push_back("scenario_mismatch");
  }
  result.fresh = result.stale_reasons.empty();
  return result;
}

void AddSavestateFreshnessFields(resources::OutputFormatter& formatter,
                                 const SavestateFreshnessResult& result) {
  formatter.AddField("status", result.fresh ? "pass" : "fail");
  formatter.AddField("ok", result.fresh);
  formatter.AddField("state", result.state_path);
  formatter.AddField("rom", result.rom_path);
  formatter.AddField("meta", result.meta_path);
  formatter.AddField("fresh", result.fresh);
  formatter.AddField("recorded_rom_sha1", result.recorded_rom_sha1);
  formatter.AddField("current_rom_sha1", result.current_rom_sha1);
  formatter.AddField("recorded_state_sha1", result.recorded_state_sha1);
  formatter.AddField("current_state_sha1", result.current_state_sha1);
  if (!result.recorded_scenario.empty()) {
    formatter.AddField("recorded_scenario", result.recorded_scenario);
  }
  if (!result.expected_scenario.empty()) {
    formatter.AddField("expected_scenario", result.expected_scenario);
  }
  if (!result.stale_reasons.empty()) {
    formatter.AddField("reasons", absl::StrJoin(result.stale_reasons, ","));
  }
}

::absl::StatusOr<nlohmann::json> BuildSavestateMetadata(
    const std::string& state_path, const std::string& rom_path,
    const std::string& scenario, const std::string& generator_name) {
  auto state_status = EnsureFileExists(state_path, "state file");
  if (!state_status.ok()) {
    return state_status;
  }
  auto rom_status = EnsureFileExists(rom_path, "ROM file");
  if (!rom_status.ok()) {
    return rom_status;
  }

  const std::string rom_sha1 = util::ComputeFileSha1Hex(rom_path);
  const std::string state_sha1 = util::ComputeFileSha1Hex(state_path);
  if (rom_sha1.empty()) {
    return ::absl::InternalError("Failed to hash ROM file");
  }
  if (state_sha1.empty()) {
    return ::absl::InternalError("Failed to hash state file");
  }

  nlohmann::json meta;
  meta["schema"] = "mesen_state_meta/v1";
  meta["state_path"] = state_path;
  meta["state_sha1"] = state_sha1;
  meta["rom_path"] = rom_path;
  meta["rom_sha1"] = rom_sha1;
  meta["generated_at"] = absl::FormatTime(absl::Now(), absl::UTCTimeZone());
  meta["generator"] = generator_name;
  if (!scenario.empty()) {
    meta["scenario"] = scenario;
  }

  // Best-effort runtime capture when Mesen2 is connected.
  auto client = emu::mesen::MesenClientRegistry::GetOrCreate();
  if (client && client->IsConnected()) {
    nlohmann::json runtime;
    if (auto state_or = client->GetState(); state_or.ok()) {
      runtime["frame"] = state_or->frame;
      runtime["running"] = state_or->running;
      runtime["paused"] = state_or->paused;
    }
    if (auto cpu_or = client->GetCpuState(); cpu_or.ok()) {
      const uint32_t pc =
          (static_cast<uint32_t>(cpu_or->K) << 16) | (cpu_or->PC & 0xFFFF);
      runtime["pc"] = absl::StrFormat("0x%06X", pc);
    }
    if (auto game_or = client->GetGameState(); game_or.ok()) {
      runtime["indoors"] = game_or->game.indoors;
      runtime["room_id"] = absl::StrFormat("0x%04X", game_or->game.room_id);
      runtime["link_x"] = game_or->link.x;
      runtime["link_y"] = game_or->link.y;
    }
    if (!runtime.empty()) {
      meta["runtime"] = runtime;
    }
  }
  return meta;
}

::absl::StatusOr<std::filesystem::path> FindLatestStateFileInDir(
    const std::filesystem::path& states_dir) {
  std::error_code ec;
  if (!std::filesystem::exists(states_dir, ec) || ec ||
      !std::filesystem::is_directory(states_dir, ec) || ec) {
    return ::absl::NotFoundError(::absl::StrFormat(
        "states directory not found: %s", states_dir.string()));
  }

  bool found = false;
  std::filesystem::path latest_path;
  std::filesystem::file_time_type latest_time;
  for (const auto& entry :
       std::filesystem::directory_iterator(states_dir, ec)) {
    if (ec) {
      break;
    }
    if (!entry.is_regular_file()) {
      continue;
    }
    const auto ext = entry.path().extension().string();
    if (ext != ".state" && ext != ".mss") {
      continue;
    }
    const auto file_time = entry.last_write_time(ec);
    if (ec) {
      continue;
    }
    if (!found || file_time > latest_time) {
      found = true;
      latest_time = file_time;
      latest_path = entry.path();
    }
  }
  if (!found) {
    return ::absl::NotFoundError(::absl::StrFormat(
        "no state files (.state/.mss) found in %s", states_dir.string()));
  }
  return latest_path;
}

::absl::Status ExportSessionStateToFile(const std::string& file_path) {
  nlohmann::json j;
  const auto& s = SessionState();
  j["connected"] = s.connected;
  j["running"] = s.running;
  j["paused"] = s.paused;
  j["frame"] = s.frame;
  j["pc"] = s.pc;
  j["last_breakpoint_id"] = s.last_breakpoint_id;
  j["last_action"] = s.last_action;
  j["breakpoints"] = nlohmann::json::array();
  for (const auto& [id, addr] : s.breakpoints_by_id) {
    j["breakpoints"].push_back({{"id", id}, {"address", addr}});
  }

  std::ofstream out(file_path);
  if (!out.is_open()) {
    return ::absl::PermissionDeniedError("Failed to open session export path");
  }
  out << j.dump(2);
  if (!out.good()) {
    return ::absl::InternalError("Failed to write session export file");
  }
  return ::absl::OkStatus();
}

::absl::Status ImportSessionStateFromFile(const std::string& file_path) {
  std::ifstream in(file_path);
  if (!in.is_open()) {
    return ::absl::NotFoundError("Session import file not found");
  }
  nlohmann::json j;
  try {
    in >> j;
  } catch (const std::exception& e) {
    return ::absl::InvalidArgumentError(
        ::absl::StrFormat("Invalid session JSON: %s", e.what()));
  }

  EmulatorAgentSessionState loaded;
  loaded.connected = j.value("connected", false);
  loaded.running = j.value("running", false);
  loaded.paused = j.value("paused", false);
  loaded.frame = j.value("frame", 0ULL);
  loaded.pc = j.value("pc", 0U);
  loaded.last_breakpoint_id = j.value("last_breakpoint_id", -1);
  loaded.last_action = j.value("last_action", "import");
  if (j.contains("breakpoints") && j["breakpoints"].is_array()) {
    for (const auto& bp : j["breakpoints"]) {
      const int id = bp.value("id", -1);
      const uint32_t addr = bp.value("address", 0U);
      if (id >= 0) {
        loaded.breakpoints_by_id[id] = addr;
      }
    }
  }

  SessionState() = std::move(loaded);
  return ::absl::OkStatus();
}

::absl::Status EnsureConnected() {
  auto client = emu::mesen::MesenClientRegistry::GetOrCreate();
  if (!client->IsConnected()) {
    std::string socket_path = ::absl::GetFlag(FLAGS_mesen_socket);
    if (socket_path.empty()) {
      const char* env_path = std::getenv("MESEN2_SOCKET_PATH");
      if (env_path && env_path[0] != '\0') {
        socket_path = env_path;
      }
    }
    auto status =
        socket_path.empty() ? client->Connect() : client->Connect(socket_path);
    if (!status.ok()) {
      return ::absl::UnavailableError(
          "Not connected to Mesen2. Is Mesen2-OoS running?");
    }
    auto& session = SessionState();
    session.connected = true;
    session.last_action = "connect";
  }
  UpdateSessionFromRuntime(client);
  return ::absl::OkStatus();
}

::absl::StatusOr<int> ParseOptionalInt(const resources::ArgumentParser& parser,
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
    auto res = std::from_chars(hex.data() + i, hex.data() + i + 2, byte, 16);
    if (res.ec == std::errc()) {
      data.push_back(static_cast<uint8_t>(byte));
    }
  }
  return data;
}

::absl::StatusOr<emu::mesen::CpuState> PollCpuStateWithDeadline(
    const std::shared_ptr<emu::mesen::MesenSocketClient>& client,
    ::absl::Time deadline) {
  while (::absl::Now() < deadline) {
    auto cpu_or = client->GetCpuState();
    if (cpu_or.ok()) {
      return cpu_or;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return ::absl::DeadlineExceededError(
      "Timed out while polling Mesen2 CPU state");
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
  if (!status.ok())
    return status;

  auto client = emu::mesen::MesenClientRegistry::GetOrCreate();
  auto result = client->GetGameState();
  if (!result.ok())
    return result.status();

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
  if (!status.ok())
    return status;

  bool show_all = parser.HasFlag("all");
  auto client = emu::mesen::MesenClientRegistry::GetOrCreate();
  auto result = client->GetSprites(show_all);
  if (!result.ok())
    return result.status();

  formatter.AddField("count", static_cast<int>(result->size()));
  formatter.BeginArray("sprites");
  for (const auto& sprite : *result) {
    formatter.AddArrayItem(::absl::StrFormat(
        "[#%d] type=0x%02X state=%d @(%d,%d) hp=%d", sprite.slot, sprite.type,
        sprite.state, sprite.x, sprite.y, sprite.health));
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
  if (!status.ok())
    return status;

  auto client = emu::mesen::MesenClientRegistry::GetOrCreate();
  auto result = client->GetCpuState();
  if (!result.ok())
    return result.status();

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
  if (!status.ok())
    return status;

  auto addr_or = parser.GetHex("address");
  if (!addr_or.ok())
    return addr_or.status();
  uint32_t addr = static_cast<uint32_t>(*addr_or);

  auto length_or = ParseOptionalInt(parser, "length", 16);
  if (!length_or.ok())
    return length_or.status();
  int length = *length_or;

  auto client = emu::mesen::MesenClientRegistry::GetOrCreate();
  auto result = client->ReadBlock(addr, length);
  if (!result.ok())
    return result.status();

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
  if (!status.ok())
    return status;

  auto addr_or = parser.GetHex("address");
  if (!addr_or.ok())
    return addr_or.status();
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
  if (!write_status.ok())
    return write_status;

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
  if (!status.ok())
    return status;

  auto addr_or = parser.GetHex("address");
  if (!addr_or.ok())
    return addr_or.status();
  uint32_t addr = static_cast<uint32_t>(*addr_or);

  auto count_or = ParseOptionalInt(parser, "count", 10);
  if (!count_or.ok())
    return count_or.status();
  int count = *count_or;

  auto client = emu::mesen::MesenClientRegistry::GetOrCreate();
  auto result = client->Disassemble(addr, count);
  if (!result.ok())
    return result.status();

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
  if (!status.ok())
    return status;

  auto count_or = ParseOptionalInt(parser, "count", 20);
  if (!count_or.ok())
    return count_or.status();
  int count = *count_or;

  auto client = emu::mesen::MesenClientRegistry::GetOrCreate();
  auto result = client->GetTrace(count);
  if (!result.ok())
    return result.status();

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
  if (!status.ok())
    return status;

  auto action = parser.GetString("action");
  if (!action.has_value()) {
    return ::absl::InvalidArgumentError("--action is required");
  }

  if (*action == "add") {
    auto addr_or = parser.GetHex("address");
    if (!addr_or.ok())
      return addr_or.status();
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
    if (!result.ok())
      return result.status();
    formatter.AddField("breakpoint_id", *result);
    formatter.AddHexField("address", addr, 6);
    formatter.AddField("status", "added");
  } else if (*action == "remove") {
    auto id_or = parser.GetInt("id");
    if (!id_or.ok())
      return id_or.status();
    int id = *id_or;

    auto client = emu::mesen::MesenClientRegistry::GetOrCreate();
    auto remove_status = client->RemoveBreakpoint(id);
    if (!remove_status.ok())
      return remove_status;
    formatter.AddField("breakpoint_id", id);
    formatter.AddField("status", "removed");
  } else if (*action == "clear") {
    auto client = emu::mesen::MesenClientRegistry::GetOrCreate();
    auto clear_status = client->ClearBreakpoints();
    if (!clear_status.ok())
      return clear_status;
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
  if (!status.ok())
    return status;

  auto action = parser.GetString("action");
  if (!action.has_value()) {
    return ::absl::InvalidArgumentError("--action required");
  }

  auto client = emu::mesen::MesenClientRegistry::GetOrCreate();
  if (*action == "pause") {
    auto result = client->Pause();
    if (!result.ok())
      return result;
  } else if (*action == "resume") {
    auto result = client->Resume();
    if (!result.ok())
      return result;
  } else if (*action == "step") {
    auto result = client->Step(1);
    if (!result.ok())
      return result;
  } else if (*action == "frame") {
    auto result = client->Frame();
    if (!result.ok())
      return result;
  } else if (*action == "reset") {
    auto result = client->Reset();
    if (!result.ok())
      return result;
  } else {
    return ::absl::InvalidArgumentError("Unknown action: " + *action);
  }

  auto& session = SessionState();
  session.last_action = *action;
  UpdateSessionFromRuntime(client);
  formatter.AddField("status", "ok");
  formatter.AddField("action", *action);
  return ::absl::OkStatus();
}

// MesenSessionCommandHandler
::absl::Status MesenSessionCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  if (!parser.GetString("action").has_value()) {
    return ::absl::OkStatus();
  }
  const auto action = parser.GetString("action").value_or("show");
  if (action == "show" || action == "reset") {
    return ::absl::OkStatus();
  }
  if (action == "export" || action == "import") {
    return parser.RequireArgs({"file"});
  }
  return ::absl::InvalidArgumentError(
      "--action must be show|reset|export|import");
}

::absl::Status AddSessionFields(resources::OutputFormatter& formatter) {
  const auto& session = SessionState();
  formatter.AddField("connected", session.connected);
  formatter.AddField("running", session.running);
  formatter.AddField("paused", session.paused);
  formatter.AddField("frame", static_cast<uint64_t>(session.frame));
  formatter.AddHexField("pc", session.pc, 6);
  formatter.AddField("breakpoint_count",
                     static_cast<int>(session.breakpoints_by_id.size()));
  formatter.AddField("last_breakpoint_id", session.last_breakpoint_id);
  formatter.AddField("last_action", session.last_action);
  return ::absl::OkStatus();
}

::absl::Status MesenSessionCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  (void)rom;
  const auto action = parser.GetString("action").value_or("show");
  if (action == "reset") {
    ResetSessionState();
    auto& session = SessionState();
    session.last_action = "reset";
    formatter.AddField("status", "ok");
    formatter.AddField("action", "reset");
    return AddSessionFields(formatter);
  }

  if (action == "export") {
    auto file = parser.GetString("file");
    if (!file.has_value()) {
      return ::absl::InvalidArgumentError("--file required for export");
    }
    auto export_status = ExportSessionStateToFile(*file);
    if (!export_status.ok()) {
      return export_status;
    }
    auto& session = SessionState();
    session.last_action = "export";
    formatter.AddField("status", "ok");
    formatter.AddField("action", "export");
    formatter.AddField("file", *file);
    return AddSessionFields(formatter);
  }

  if (action == "import") {
    auto file = parser.GetString("file");
    if (!file.has_value()) {
      return ::absl::InvalidArgumentError("--file required for import");
    }
    auto import_status = ImportSessionStateFromFile(*file);
    if (!import_status.ok()) {
      return import_status;
    }
    auto& session = SessionState();
    session.last_action = "import";
    formatter.AddField("status", "ok");
    formatter.AddField("action", "import");
    formatter.AddField("file", *file);
    return AddSessionFields(formatter);
  }

  auto status = EnsureConnected();
  if (!status.ok()) {
    return status;
  }
  auto client = emu::mesen::MesenClientRegistry::GetOrCreate();
  UpdateSessionFromRuntime(client);
  return AddSessionFields(formatter);
}

// MesenAwaitCommandHandler
::absl::Status MesenAwaitCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  auto status = parser.RequireArgs({"type"});
  if (!status.ok()) {
    return status;
  }
  const auto type = parser.GetString("type").value_or("");
  if (type == "frame") {
    return parser.RequireArgs({"count"});
  }
  if (type == "pc") {
    return parser.RequireArgs({"address"});
  }
  if (type == "breakpoint") {
    return parser.RequireArgs({"id"});
  }
  return ::absl::InvalidArgumentError(
      "--type must be one of frame|pc|breakpoint");
}

::absl::Status MesenAwaitCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  (void)rom;
  auto status = EnsureConnected();
  if (!status.ok()) {
    return status;
  }
  auto client = emu::mesen::MesenClientRegistry::GetOrCreate();
  auto& session = SessionState();

  auto timeout_or = ParseOptionalInt(parser, "timeout-ms", 2000);
  if (!timeout_or.ok()) {
    return timeout_or.status();
  }
  auto poll_or = ParseOptionalInt(parser, "poll-ms", 25);
  if (!poll_or.ok()) {
    return poll_or.status();
  }
  const int timeout_ms = ClampTimeoutMs(*timeout_or);
  const int poll_ms = ClampPollMs(*poll_or);
  const auto start = ::absl::Now();
  const auto deadline = start + ::absl::Milliseconds(timeout_ms);

  const auto type = parser.GetString("type").value_or("");
  if (type == "frame") {
    auto count_or = parser.GetInt("count");
    if (!count_or.ok()) {
      return count_or.status();
    }
    UpdateSessionFromRuntime(client);
    const uint64_t target =
        session.frame + static_cast<uint64_t>(std::max(0, *count_or));
    while (::absl::Now() < deadline) {
      if (auto state_or = client->GetState(); state_or.ok()) {
        session.frame = state_or->frame;
        session.running = state_or->running;
        session.paused = state_or->paused;
        if (session.frame >= target) {
          session.last_action = "await-frame";
          formatter.AddField("status", "ok");
          formatter.AddField("type", "frame");
          formatter.AddField("target_frame", static_cast<uint64_t>(target));
          formatter.AddField("frame", static_cast<uint64_t>(session.frame));
          formatter.AddField("elapsed_ms",
                             static_cast<uint64_t>(::absl::ToInt64Milliseconds(
                                 ::absl::Now() - start)));
          return ::absl::OkStatus();
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms));
    }
    return ::absl::DeadlineExceededError("Timed out waiting for target frame");
  }

  if (type == "pc") {
    auto addr_or = parser.GetHex("address");
    if (!addr_or.ok()) {
      return addr_or.status();
    }
    const uint32_t target_pc = static_cast<uint32_t>(*addr_or);
    while (::absl::Now() < deadline) {
      auto cpu_or = PollCpuStateWithDeadline(client, deadline);
      if (!cpu_or.ok()) {
        return cpu_or.status();
      }
      const uint32_t pc =
          (static_cast<uint32_t>(cpu_or->K) << 16) | (cpu_or->PC & 0xFFFF);
      session.pc = pc;
      if (pc == target_pc) {
        session.last_action = "await-pc";
        formatter.AddField("status", "ok");
        formatter.AddField("type", "pc");
        formatter.AddHexField("target_pc", target_pc, 6);
        formatter.AddHexField("pc", pc, 6);
        formatter.AddField("elapsed_ms",
                           static_cast<uint64_t>(::absl::ToInt64Milliseconds(
                               ::absl::Now() - start)));
        return ::absl::OkStatus();
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms));
    }
    return ::absl::DeadlineExceededError("Timed out waiting for target pc");
  }

  auto id_or = parser.GetInt("id");
  if (!id_or.ok()) {
    return id_or.status();
  }
  const int id = *id_or;
  const auto it = session.breakpoints_by_id.find(id);
  if (it == session.breakpoints_by_id.end()) {
    return ::absl::NotFoundError("Unknown breakpoint id in session state");
  }
  const uint32_t target_pc = it->second;
  while (::absl::Now() < deadline) {
    auto state_or = client->GetState();
    if (!state_or.ok()) {
      return state_or.status();
    }
    session.running = state_or->running;
    session.paused = state_or->paused;
    session.frame = state_or->frame;
    auto cpu_or = client->GetCpuState();
    if (!cpu_or.ok()) {
      return cpu_or.status();
    }
    const uint32_t pc =
        (static_cast<uint32_t>(cpu_or->K) << 16) | (cpu_or->PC & 0xFFFF);
    session.pc = pc;
    if (session.paused && pc == target_pc) {
      session.last_action = "await-breakpoint";
      formatter.AddField("status", "ok");
      formatter.AddField("type", "breakpoint");
      formatter.AddField("breakpoint_id", id);
      formatter.AddHexField("pc", pc, 6);
      formatter.AddField("frame", static_cast<uint64_t>(session.frame));
      formatter.AddField("elapsed_ms",
                         static_cast<uint64_t>(::absl::ToInt64Milliseconds(
                             ::absl::Now() - start)));
      return ::absl::OkStatus();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms));
  }
  return ::absl::DeadlineExceededError("Timed out waiting for breakpoint hit");
}

// MesenGoalCommandHandler
::absl::Status MesenGoalCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  auto status = parser.RequireArgs({"goal"});
  if (!status.ok()) {
    return status;
  }
  const auto goal = parser.GetString("goal").value_or("");
  if (goal == "break-at" || goal == "capture-state-at-pc") {
    return parser.RequireArgs({"address"});
  }
  if (goal == "run-frames") {
    return parser.RequireArgs({"count"});
  }
  return ::absl::InvalidArgumentError(
      "Unknown --goal. Supported: break-at|run-frames|capture-state-at-pc");
}

::absl::Status MesenGoalCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  (void)rom;
  auto status = EnsureConnected();
  if (!status.ok()) {
    return status;
  }
  auto client = emu::mesen::MesenClientRegistry::GetOrCreate();
  auto& session = SessionState();
  const auto goal = parser.GetString("goal").value_or("");
  if (goal == "run-frames") {
    auto count_or = parser.GetInt("count");
    if (!count_or.ok()) {
      return count_or.status();
    }
    const int frame_count = std::max(1, *count_or);
    auto timeout_or = ParseOptionalInt(parser, "timeout-ms", 2000);
    if (!timeout_or.ok()) {
      return timeout_or.status();
    }
    auto poll_or = ParseOptionalInt(parser, "poll-ms", 25);
    if (!poll_or.ok()) {
      return poll_or.status();
    }
    const int timeout_ms = ClampTimeoutMs(*timeout_or);
    const int poll_ms = ClampPollMs(*poll_or);
    const auto start = ::absl::Now();
    const auto deadline = start + ::absl::Milliseconds(timeout_ms);

    if (auto pause_status = client->Pause(); !pause_status.ok()) {
      return pause_status;
    }
    UpdateSessionFromRuntime(client);
    const uint64_t target_frame =
        session.frame + static_cast<uint64_t>(frame_count);
    if (auto resume_status = client->Resume(); !resume_status.ok()) {
      return resume_status;
    }
    bool reached = false;
    while (::absl::Now() < deadline) {
      auto state_or = client->GetState();
      if (!state_or.ok()) {
        break;
      }
      session.running = state_or->running;
      session.paused = state_or->paused;
      session.frame = state_or->frame;
      if (session.frame >= target_frame) {
        reached = true;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms));
    }
    (void)client->Pause();
    UpdateSessionFromRuntime(client);
    if (!reached) {
      return ::absl::DeadlineExceededError("Goal run-frames timed out");
    }
    session.last_action = "goal-run-frames";
    formatter.AddField("status", "ok");
    formatter.AddField("goal", "run-frames");
    formatter.AddField("requested_frames", frame_count);
    formatter.AddField("frame", static_cast<uint64_t>(session.frame));
    formatter.AddField("elapsed_ms",
                       static_cast<uint64_t>(
                           ::absl::ToInt64Milliseconds(::absl::Now() - start)));
    return ::absl::OkStatus();
  }

  if (goal != "break-at" && goal != "capture-state-at-pc") {
    return ::absl::InvalidArgumentError("Unsupported goal");
  }
  auto addr_or = parser.GetHex("address");
  if (!addr_or.ok()) {
    return addr_or.status();
  }
  const uint32_t target_pc = static_cast<uint32_t>(*addr_or);
  auto timeout_or = ParseOptionalInt(parser, "timeout-ms", 2000);
  if (!timeout_or.ok()) {
    return timeout_or.status();
  }
  auto poll_or = ParseOptionalInt(parser, "poll-ms", 25);
  if (!poll_or.ok()) {
    return poll_or.status();
  }
  const int timeout_ms = ClampTimeoutMs(*timeout_or);
  const int poll_ms = ClampPollMs(*poll_or);
  const auto start = ::absl::Now();
  const auto deadline = start + ::absl::Milliseconds(timeout_ms);

  if (auto pause_status = client->Pause(); !pause_status.ok()) {
    return pause_status;
  }
  auto bp_or =
      client->AddBreakpoint(target_pc, emu::mesen::BreakpointType::kExecute);
  if (!bp_or.ok()) {
    return bp_or.status();
  }
  const int bp_id = *bp_or;
  session.breakpoints_by_id[bp_id] = target_pc;
  session.last_breakpoint_id = bp_id;

  if (auto resume_status = client->Resume(); !resume_status.ok()) {
    (void)client->RemoveBreakpoint(bp_id);
    session.breakpoints_by_id.erase(bp_id);
    return resume_status;
  }

  bool hit = false;
  while (::absl::Now() < deadline) {
    auto state_or = client->GetState();
    if (!state_or.ok()) {
      break;
    }
    session.running = state_or->running;
    session.paused = state_or->paused;
    session.frame = state_or->frame;
    auto cpu_or = client->GetCpuState();
    if (!cpu_or.ok()) {
      break;
    }
    const uint32_t pc =
        (static_cast<uint32_t>(cpu_or->K) << 16) | (cpu_or->PC & 0xFFFF);
    session.pc = pc;
    if (session.paused && pc == target_pc) {
      hit = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms));
  }

  (void)client->Pause();
  (void)client->RemoveBreakpoint(bp_id);
  session.breakpoints_by_id.erase(bp_id);
  UpdateSessionFromRuntime(client);
  if (!hit) {
    return ::absl::DeadlineExceededError("Goal break-at timed out");
  }

  session.last_action = (goal == "capture-state-at-pc")
                            ? "goal-capture-state-at-pc"
                            : "goal-break-at";
  if (goal == "capture-state-at-pc") {
    auto game_state_or = client->GetGameState();
    if (game_state_or.ok()) {
      formatter.AddHexField("room_id", game_state_or->game.room_id, 4);
      formatter.AddField("indoors", game_state_or->game.indoors);
      formatter.AddField("link_x", game_state_or->link.x);
      formatter.AddField("link_y", game_state_or->link.y);
      formatter.AddField("health", game_state_or->items.current_health);
    }
  }
  formatter.AddField("status", "ok");
  formatter.AddField("goal", goal);
  formatter.AddField("breakpoint_id", bp_id);
  formatter.AddHexField("target_pc", target_pc, 6);
  formatter.AddHexField("pc", session.pc, 6);
  formatter.AddField("frame", static_cast<uint64_t>(session.frame));
  formatter.AddField("elapsed_ms",
                     static_cast<uint64_t>(
                         ::absl::ToInt64Milliseconds(::absl::Now() - start)));
  return ::absl::OkStatus();
}

// MesenStateVerifyCommandHandler
::absl::Status MesenStateVerifyCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return parser.RequireArgs({"state", "rom-file"});
}

::absl::Status MesenStateVerifyCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  (void)rom;
  const std::string state_path = *parser.GetString("state");
  const std::string rom_path = *parser.GetString("rom-file");
  const std::string meta_path =
      parser.GetString("meta").value_or(DefaultMetaPathForState(state_path));
  const std::string expected_scenario = OptionalScenario(parser);

  auto result_or = ComputeSavestateFreshness(state_path, rom_path, meta_path,
                                             expected_scenario);
  if (!result_or.ok()) {
    return result_or.status();
  }
  AddSavestateFreshnessFields(formatter, *result_or);
  if (!result_or->fresh) {
    return ::absl::FailedPreconditionError(
        "Savestate freshness verification failed");
  }
  return ::absl::OkStatus();
}

// MesenStateRegenCommandHandler
::absl::Status MesenStateRegenCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return parser.RequireArgs({"state", "rom-file"});
}

::absl::Status MesenStateRegenCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  (void)rom;
  const std::string state_path = *parser.GetString("state");
  const std::string rom_path = *parser.GetString("rom-file");
  const std::string meta_path =
      parser.GetString("meta").value_or(DefaultMetaPathForState(state_path));
  const std::string scenario = OptionalScenario(parser);

  auto meta_or = BuildSavestateMetadata(state_path, rom_path, scenario,
                                        "z3ed mesen-state-regen");
  if (!meta_or.ok()) {
    return meta_or.status();
  }

  auto write_status = WriteJsonFile(meta_path, *meta_or);
  if (!write_status.ok())
    return write_status;

  formatter.AddField("status", "ok");
  formatter.AddField("state", state_path);
  formatter.AddField("rom", rom_path);
  formatter.AddField("meta", meta_path);
  formatter.AddField("rom_sha1", meta_or->value("rom_sha1", ""));
  formatter.AddField("state_sha1", meta_or->value("state_sha1", ""));
  formatter.AddField("scenario", scenario);
  return ::absl::OkStatus();
}

// MesenStateCaptureCommandHandler
::absl::Status MesenStateCaptureCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return parser.RequireArgs({"state", "rom-file"});
}

::absl::Status MesenStateCaptureCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  (void)rom;
  const std::string requested_state_path = *parser.GetString("state");
  const std::string rom_path = *parser.GetString("rom-file");
  const std::string scenario = OptionalScenario(parser);
  const std::string meta_path = parser.GetString("meta").value_or(
      DefaultMetaPathForState(requested_state_path));
  const int wait_ms = parser.GetInt("wait-ms").value_or(400);
  const int slot = parser.GetInt("slot").value_or(-1);
  const std::string states_dir = parser.GetString("states-dir").value_or("");

  auto rom_status = EnsureFileExists(rom_path, "ROM file");
  if (!rom_status.ok()) {
    return rom_status;
  }

  std::string resolved_state_path = requested_state_path;
  bool captured_from_mesen_slot = false;
  if (slot >= 0) {
    auto status = EnsureConnected();
    if (!status.ok()) {
      return status;
    }
    auto client = emu::mesen::MesenClientRegistry::GetOrCreate();
    auto save_status = client->SaveState(slot);
    if (!save_status.ok()) {
      return save_status;
    }
    captured_from_mesen_slot = true;
    std::this_thread::sleep_for(
        std::chrono::milliseconds(std::max(1, wait_ms)));
  }

  std::error_code ec;
  if (!std::filesystem::exists(resolved_state_path, ec) || ec) {
    if (states_dir.empty()) {
      return ::absl::NotFoundError(
          ::absl::StrFormat("state file not found after capture: %s (use "
                            "--states-dir to auto-locate latest state file)",
                            resolved_state_path));
    }
    auto latest_or = FindLatestStateFileInDir(states_dir);
    if (!latest_or.ok()) {
      return latest_or.status();
    }
    const auto source_path = *latest_or;
    auto parent = std::filesystem::path(resolved_state_path).parent_path();
    if (!parent.empty()) {
      std::filesystem::create_directories(parent, ec);
      if (ec) {
        return ::absl::PermissionDeniedError(::absl::StrFormat(
            "failed to create directory: %s", parent.string()));
      }
    }
    std::filesystem::copy_file(
        source_path, resolved_state_path,
        std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
      return ::absl::InternalError(::absl::StrFormat(
          "failed to copy state from %s to %s: %s", source_path.string(),
          resolved_state_path, ec.message()));
    }
  }

  auto meta_or = BuildSavestateMetadata(resolved_state_path, rom_path, scenario,
                                        "z3ed mesen-state-capture");
  if (!meta_or.ok()) {
    return meta_or.status();
  }
  auto write_status = WriteJsonFile(meta_path, *meta_or);
  if (!write_status.ok()) {
    return write_status;
  }

  auto verify_or = ComputeSavestateFreshness(resolved_state_path, rom_path,
                                             meta_path, scenario);
  if (!verify_or.ok()) {
    return verify_or.status();
  }
  formatter.AddField("status", "ok");
  formatter.AddField("state", resolved_state_path);
  formatter.AddField("rom", rom_path);
  formatter.AddField("meta", meta_path);
  formatter.AddField("scenario", scenario);
  formatter.AddField("captured_from_mesen_slot", captured_from_mesen_slot);
  formatter.AddField("slot", slot);
  formatter.AddField("fresh", verify_or->fresh);
  formatter.AddField("rom_sha1", meta_or->value("rom_sha1", ""));
  formatter.AddField("state_sha1", meta_or->value("state_sha1", ""));
  return ::absl::OkStatus();
}

// MesenStateHookCommandHandler
::absl::Status MesenStateHookCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return parser.RequireArgs({"state", "rom-file"});
}

::absl::Status MesenStateHookCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  (void)rom;
  const std::string state_path = *parser.GetString("state");
  const std::string rom_path = *parser.GetString("rom-file");
  const std::string meta_path =
      parser.GetString("meta").value_or(DefaultMetaPathForState(state_path));
  const std::string expected_scenario = OptionalScenario(parser);

  auto result_or = ComputeSavestateFreshness(state_path, rom_path, meta_path,
                                             expected_scenario);
  if (!result_or.ok()) {
    return result_or.status();
  }

  AddSavestateFreshnessFields(formatter, *result_or);
  formatter.AddField(
      "hook_summary",
      absl::StrFormat(
          "fresh=%s scenario=%s rom_sha1=%s state_sha1=%s",
          result_or->fresh ? "true" : "false",
          result_or->recorded_scenario.empty() ? "(none)"
                                               : result_or->recorded_scenario,
          result_or->current_rom_sha1, result_or->current_state_sha1));
  if (!result_or->fresh) {
    return ::absl::FailedPreconditionError("mesen preflight hook failed");
  }
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
  handlers.push_back(std::make_unique<MesenSessionCommandHandler>());
  handlers.push_back(std::make_unique<MesenAwaitCommandHandler>());
  handlers.push_back(std::make_unique<MesenGoalCommandHandler>());
  handlers.push_back(std::make_unique<MesenStateVerifyCommandHandler>());
  handlers.push_back(std::make_unique<MesenStateRegenCommandHandler>());
  handlers.push_back(std::make_unique<MesenStateCaptureCommandHandler>());
  handlers.push_back(std::make_unique<MesenStateHookCommandHandler>());
  return handlers;
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
