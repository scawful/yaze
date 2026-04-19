#include "app/editor/hack/oracle/ui/oracle_validation_view_model.h"

#include <chrono>
#include <ctime>

#include "nlohmann/json.hpp"

namespace yaze::editor::oracle_validation {
namespace {
using json = nlohmann::json;
}  // namespace

std::string CurrentTimestamp() {
  const auto now = std::chrono::system_clock::now();
  const auto t = std::chrono::system_clock::to_time_t(now);
  char buf[32];
  struct tm tm_utc{};
#ifdef _WIN32
  gmtime_s(&tm_utc, &t);
#else
  gmtime_r(&t, &tm_utc);
#endif
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
  return buf;
}

std::vector<std::string> BuildSmokeArgs(const SmokeOptions& opts) {
  std::vector<std::string> args;
  if (!opts.rom_path.empty()) {
    args.push_back("--rom=" + opts.rom_path);
  }
  if (opts.strict_readiness) {
    args.push_back("--strict-readiness");
  }
  if (opts.min_d6_track_rooms != 0) {
    args.push_back("--min-d6-track-rooms=" +
                   std::to_string(opts.min_d6_track_rooms));
  }
  args.push_back("--format=json");
  if (!opts.report_path.empty()) {
    args.push_back("--report=" + opts.report_path);
  }
  return args;
}

std::vector<std::string> BuildPreflightArgs(const PreflightOptions& opts) {
  std::vector<std::string> args;
  if (!opts.rom_path.empty()) {
    args.push_back("--rom=" + opts.rom_path);
  }
  if (!opts.required_collision_rooms.empty()) {
    args.push_back("--required-collision-rooms=" +
                   opts.required_collision_rooms);
  }
  if (opts.require_write_support) {
    args.push_back("--require-write-support");
  }
  if (opts.skip_collision_maps) {
    args.push_back("--skip-collision-maps");
  }
  args.push_back("--format=json");
  if (!opts.report_path.empty()) {
    args.push_back("--report=" + opts.report_path);
  }
  return args;
}

std::string BuildCliCommand(const std::string& command_name,
                            const std::vector<std::string>& args) {
  std::string cmd = "z3ed " + command_name;
  for (const auto& arg : args) {
    cmd += " " + arg;
  }
  return cmd;
}

absl::StatusOr<SmokeResult> ParseSmokeCheckOutput(const std::string& json_str) {
  const auto doc = json::parse(json_str, nullptr, false);
  if (doc.is_discarded()) {
    return absl::InvalidArgumentError(
        "ParseSmokeCheckOutput: JSON parse failed");
  }

  if (!doc.contains("Oracle Smoke Check")) {
    return absl::InvalidArgumentError(
        "ParseSmokeCheckOutput: missing 'Oracle Smoke Check' key");
  }
  const auto& root = doc.at("Oracle Smoke Check");
  const auto& checks = root.value("checks", json::object());

  SmokeResult r;
  r.ok = root.value("ok", false);
  r.status = root.value("status", "");
  r.strict_readiness = root.value("strict_readiness", false);

  const auto& d4 = checks.value("d4_zora_temple", json::object());
  r.d4.structural_ok = d4.value("structural_ok", false);
  r.d4.required_rooms_check = d4.value("required_rooms_check", "skipped");
  if (d4.contains("required_rooms_ok")) {
    r.d4.required_rooms_ok = d4.at("required_rooms_ok").get<bool>();
  }

  const auto& d6 = checks.value("d6_goron_mines", json::object());
  r.d6.ok = d6.value("ok", false);
  r.d6.track_rooms_found = d6.value("track_rooms_found", 0);
  r.d6.min_track_rooms = d6.value("min_track_rooms", 0);
  r.d6.meets_min_track_rooms = d6.value("meets_min_track_rooms", false);

  const auto& d3 = checks.value("d3_kalyxo_castle", json::object());
  r.d3.readiness_check = d3.value("readiness_check", "skipped");
  if (d3.contains("ok")) {
    r.d3.ok = d3.at("ok").get<bool>();
  }

  return r;
}

absl::StatusOr<PreflightResult> ParsePreflightOutput(
    const std::string& json_str) {
  const auto doc = json::parse(json_str, nullptr, false);
  if (doc.is_discarded()) {
    return absl::InvalidArgumentError(
        "ParsePreflightOutput: JSON parse failed");
  }
  if (!doc.contains("Dungeon Oracle Preflight")) {
    return absl::InvalidArgumentError(
        "ParsePreflightOutput: missing 'Dungeon Oracle Preflight' key");
  }
  const auto& root = doc.at("Dungeon Oracle Preflight");

  PreflightResult r;
  r.ok = root.value("ok", false);
  r.error_count = root.value("error_count", 0);
  r.water_fill_region_ok = root.value("water_fill_region_ok", false);
  r.water_fill_table_ok = root.value("water_fill_table_ok", false);
  r.custom_collision_maps_ok = root.value("custom_collision_maps_ok", false);
  r.required_rooms_check = root.value("required_rooms_check", "skipped");
  if (root.contains("required_rooms_ok")) {
    r.required_rooms_ok = root.at("required_rooms_ok").get<bool>();
  }
  r.status = root.value("status", "");

  for (const auto& e : root.value("errors", json::array())) {
    PreflightError err;
    err.code = e.value("code", "");
    err.message = e.value("message", "");
    err.status_code = e.value("status", "");
    if (e.contains("room_id")) {
      err.room_id = e.at("room_id").get<std::string>();
    }
    r.errors.push_back(std::move(err));
  }

  return r;
}

}  // namespace yaze::editor::oracle_validation
