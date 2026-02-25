#include "cli/handlers/game/oracle_smoke_check_commands.h"

#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "cli/handlers/game/minecart_commands.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
#include "util/macro.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/oracle_rom_safety_preflight.h"

namespace yaze::cli::handlers {

using json = nlohmann::json;

resources::CommandHandler::Descriptor
OracleSmokeCheckCommandHandler::Describe() const {
  Descriptor desc;
  desc.display_name = "Oracle Smoke Check";
  desc.summary =
      "Single-shot Oracle ROM smoke check covering D4 Zora Temple (water "
      "system), D6 Goron Mines (minecart rooms 0xA8/0xB8/0xD8/0xDA), and "
      "D3 Kalyxo Castle (prison room 0x32 collision readiness). Structural "
      "failures always exit non-zero; readiness gaps are informational unless "
      "--strict-readiness is set.";
  desc.todo_reference = "todo#oracle-testing-infra";
  desc.entries = {
      {"--strict-readiness",
       "Also fail when D4 rooms 0x25/0x27 or D3 room 0x32 lack authored "
       "custom collision data",
       ""},
      {"--min-d6-track-rooms",
       "Fail if fewer than N of the 4 D6 rooms have track rail objects "
       "(default 0 = informational only). Treated as structural failure.",
       ""},
      {"--report", "Write full JSON summary to this path in addition to stdout",
       ""},
  };
  return desc;
}

absl::Status OracleSmokeCheckCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  // Validate --min-d6-track-rooms if provided.
  if (parser.GetString("min-d6-track-rooms").has_value()) {
    auto int_or = parser.GetInt("min-d6-track-rooms");
    if (!int_or.ok()) {
      return absl::InvalidArgumentError(
          "--min-d6-track-rooms: value must be a non-negative integer");
    }
    if (*int_or < 0) {
      return absl::InvalidArgumentError(
          "--min-d6-track-rooms: value must be >= 0");
    }
  }

  // Probe --report path before the formatter opens. Failure here emits nothing
  // to stdout (only stderr + non-zero exit), preserving the contract that
  // stdout is valid iff exit code is 0.
  if (auto rp = parser.GetString("report");
      rp.has_value() && !rp->empty()) {
    const std::filesystem::path rp_path(*rp);
    std::error_code ec;
    const bool existed_before = std::filesystem::exists(rp_path, ec);
    std::ofstream probe(*rp, std::ios::out | std::ios::binary | std::ios::app);
    if (!probe.is_open()) {
      return absl::PermissionDeniedError(
          absl::StrFormat("oracle-smoke-check: cannot open report file "
                          "for writing: %s",
                          *rp));
    }
    probe.close();
    if (!existed_before) {
      std::filesystem::remove(rp_path, ec);
    }
  }
  return absl::OkStatus();
}

absl::Status OracleSmokeCheckCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  const bool strict_readiness = parser.HasFlag("strict-readiness");

  // Parse --min-d6-track-rooms (default 0 = informational only).
  int min_d6_track_rooms = 0;
  if (auto int_or = parser.GetInt("min-d6-track-rooms"); int_or.ok()) {
    min_d6_track_rooms = *int_or;
  }

  // ------------------------------------------------------------------
  // D4 Zora Temple — structural preflight
  // ------------------------------------------------------------------
  // Validates water-fill reserved region and table. Does NOT require specific
  // room collision data (that's a readiness check, below).
  zelda3::OracleRomSafetyPreflightOptions structural_opts;
  structural_opts.require_water_fill_reserved_region = true;
  structural_opts.require_custom_collision_write_support = false;
  structural_opts.validate_water_fill_table = true;
  structural_opts.validate_custom_collision_maps = false;
  const auto structural =
      zelda3::RunOracleRomSafetyPreflight(rom, structural_opts);
  const bool d4_structural_ok = structural.ok();

  // Whether the ROM's custom-collision write region exists determines if
  // required-room checks can actually run. Mirror the preflight library's
  // gate (HasCustomCollisionWriteSupport) so we report "ran" vs "skipped"
  // rather than silently claiming readiness=true on non-expanded ROMs.
  const bool collision_write_support =
      zelda3::HasCustomCollisionWriteSupport(rom->vector().size());

  // ------------------------------------------------------------------
  // D4 Zora Temple — room collision readiness (informational by default)
  // ------------------------------------------------------------------
  bool d4_required_ok = false;
  const char* d4_readiness_state = "skipped";
  if (collision_write_support) {
    zelda3::OracleRomSafetyPreflightOptions d4_opts;
    d4_opts.require_water_fill_reserved_region = false;
    d4_opts.validate_water_fill_table = false;
    d4_opts.validate_custom_collision_maps = false;
    d4_opts.room_ids_requiring_custom_collision = {0x25, 0x27};
    const auto d4_required =
        zelda3::RunOracleRomSafetyPreflight(rom, d4_opts);
    d4_required_ok = d4_required.ok();
    d4_readiness_state = "ran";
  }

  // ------------------------------------------------------------------
  // D6 Goron Mines — minecart audit on fixed room set
  // ------------------------------------------------------------------
  // The audit always exits ok (issues are informational). d6_ok can only be
  // false if the ROM itself is unreadable (which would have been caught by the
  // structural check above).
  DungeonMinecartAuditCommandHandler minecart_handler;
  std::string d6_out;
  const bool d6_ok =
      minecart_handler
          .Run({"--rooms=0xA8,0xB8,0xD8,0xDA", "--include-track-objects",
                "--format=json"},
               rom, &d6_out)
          .ok();

  // Count D6 rooms with non-empty track_object_subtypes from audit output.
  int track_rooms_found = 0;
  {
    const auto d6_doc = json::parse(d6_out, nullptr, false);
    if (!d6_doc.is_discarded() &&
        d6_doc.contains("Dungeon Minecart Audit")) {
      for (const auto& room :
           d6_doc["Dungeon Minecart Audit"].value("rooms", json::array())) {
        if (!room.value("track_object_subtypes", json::array()).empty()) {
          ++track_rooms_found;
        }
      }
    }
  }
  const bool meets_min_track_rooms =
      (min_d6_track_rooms == 0 || track_rooms_found >= min_d6_track_rooms);

  // ------------------------------------------------------------------
  // D3 Kalyxo Castle — prison room collision readiness
  // ------------------------------------------------------------------
  bool d3_ok = false;
  const char* d3_readiness_state = "skipped";
  if (collision_write_support) {
    zelda3::OracleRomSafetyPreflightOptions d3_opts;
    d3_opts.require_water_fill_reserved_region = false;
    d3_opts.validate_water_fill_table = false;
    d3_opts.validate_custom_collision_maps = false;
    d3_opts.room_ids_requiring_custom_collision = {0x32};
    const auto d3_required =
        zelda3::RunOracleRomSafetyPreflight(rom, d3_opts);
    d3_ok = d3_required.ok();
    d3_readiness_state = "ran";
  }

  // ------------------------------------------------------------------
  // Aggregate
  // ------------------------------------------------------------------
  // meets_min_track_rooms is structural (not readiness) so it's always gated.
  bool overall_ok = d4_structural_ok && d6_ok && meets_min_track_rooms;
  if (strict_readiness) {
    // Only apply readiness gates if the checks actually ran. A "skipped" check
    // on a non-expanded ROM is not a readiness failure — it's a precondition
    // failure, already surfaced by d4_structural_ok = false.
    if (collision_write_support) {
      overall_ok = overall_ok && d4_required_ok && d3_ok;
    }
  }

  // ------------------------------------------------------------------
  // Build parallel JSON report (for --report file)
  // ------------------------------------------------------------------
  json report;
  report["ok"] = overall_ok;
  report["status"] = overall_ok ? "pass" : "fail";
  report["strict_readiness"] = strict_readiness;

  json d4_json = json::object();
  d4_json["structural_ok"] = d4_structural_ok;
  d4_json["required_rooms_check"] = d4_readiness_state;
  if (collision_write_support) {
    d4_json["required_rooms_ok"] = d4_required_ok;
  }
  d4_json["required_rooms"] = json::array({"0x25", "0x27"});
  json structural_errors = json::array();
  for (const auto& err : structural.errors) {
    structural_errors.push_back({{"code", err.code}, {"message", err.message}});
  }
  d4_json["structural_errors"] = std::move(structural_errors);

  json d6_json = json::object();
  d6_json["ok"] = d6_ok;
  d6_json["rooms"] = json::array({"0xA8", "0xB8", "0xD8", "0xDA"});
  d6_json["track_rooms_found"] = track_rooms_found;
  d6_json["min_track_rooms"] = min_d6_track_rooms;
  d6_json["meets_min_track_rooms"] = meets_min_track_rooms;

  json d3_json = json::object();
  d3_json["readiness_check"] = d3_readiness_state;
  if (collision_write_support) {
    d3_json["ok"] = d3_ok;
  }
  d3_json["required_rooms"] = json::array({"0x32"});

  report["checks"] = json::object();
  report["checks"]["d4_zora_temple"] = std::move(d4_json);
  report["checks"]["d6_goron_mines"] = std::move(d6_json);
  report["checks"]["d3_kalyxo_castle"] = std::move(d3_json);

  // ------------------------------------------------------------------
  // Emit to formatter
  // ------------------------------------------------------------------
  formatter.BeginObject("Oracle Smoke Check");
  formatter.AddField("ok", overall_ok);
  formatter.AddField("status",
                     overall_ok ? std::string("pass") : std::string("fail"));
  formatter.AddField("strict_readiness", strict_readiness);

  formatter.BeginObject("checks");

  formatter.BeginObject("d4_zora_temple");
  formatter.AddField("structural_ok", d4_structural_ok);
  formatter.AddField("required_rooms_check",
                     std::string(d4_readiness_state));
  if (collision_write_support) {
    formatter.AddField("required_rooms_ok", d4_required_ok);
  }
  formatter.EndObject();

  formatter.BeginObject("d6_goron_mines");
  formatter.AddField("ok", d6_ok);
  formatter.AddField("track_rooms_found", track_rooms_found);
  formatter.AddField("min_track_rooms", min_d6_track_rooms);
  formatter.AddField("meets_min_track_rooms", meets_min_track_rooms);
  formatter.EndObject();

  formatter.BeginObject("d3_kalyxo_castle");
  formatter.AddField("readiness_check", std::string(d3_readiness_state));
  if (collision_write_support) {
    formatter.AddField("ok", d3_ok);
  }
  formatter.EndObject();

  formatter.EndObject();  // close "checks"
  formatter.EndObject();  // close "Oracle Smoke Check"

  // ------------------------------------------------------------------
  // Write report file (always written, pass or fail)
  // ------------------------------------------------------------------
  if (auto rp = parser.GetString("report");
      rp.has_value() && !rp->empty()) {
    std::ofstream report_file(
        *rp, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!report_file.is_open()) {
      return absl::PermissionDeniedError(absl::StrFormat(
          "oracle-smoke-check: cannot open report file: %s", *rp));
    }
    report_file << report.dump(2) << "\n";
    if (!report_file.good()) {
      return absl::InternalError(absl::StrFormat(
          "oracle-smoke-check: failed writing report: %s", *rp));
    }
  }

  if (!overall_ok) {
    // Attribute the failure accurately: structural (d4/d6/threshold always
    // evaluated) vs readiness-only (only possible in strict mode after
    // structural passed).
    const bool structural_failed =
        !d4_structural_ok || !d6_ok || !meets_min_track_rooms;
    return absl::FailedPreconditionError(absl::StrFormat(
        "oracle-smoke-check failed %s",
        structural_failed ? "(structural)" : "(strict-readiness)"));
  }
  return absl::OkStatus();
}

}  // namespace yaze::cli::handlers
