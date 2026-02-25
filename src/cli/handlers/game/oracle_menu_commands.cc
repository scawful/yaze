#include "cli/handlers/game/oracle_menu_commands.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "cli/util/hex_util.h"
#include "core/oracle_menu_registry.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
#include "util/macro.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/oracle_rom_safety_preflight.h"

namespace yaze::cli::handlers {

namespace {

std::filesystem::path ResolveProjectPath(
    const resources::ArgumentParser& parser) {
  if (auto project_opt = parser.GetString("project"); project_opt.has_value()) {
    return std::filesystem::path(*project_opt);
  }
  return std::filesystem::current_path();
}

std::string FormatSize(uintmax_t size_bytes) {
  return absl::StrFormat("%llu", static_cast<unsigned long long>(size_bytes));
}

std::string SeverityToString(core::OracleMenuValidationSeverity severity) {
  return severity == core::OracleMenuValidationSeverity::kError ? "error"
                                                                 : "warning";
}

std::string FormatIssueLine(const core::OracleMenuValidationIssue& issue) {
  if (!issue.asm_path.empty() && issue.line > 0) {
    return absl::StrFormat("[%s] %s: %s (%s:%d)",
                           SeverityToString(issue.severity), issue.code,
                           issue.message, issue.asm_path, issue.line);
  }
  if (!issue.asm_path.empty()) {
    return absl::StrFormat("[%s] %s: %s (%s)",
                           SeverityToString(issue.severity), issue.code,
                           issue.message, issue.asm_path);
  }
  return absl::StrFormat("[%s] %s: %s", SeverityToString(issue.severity),
                         issue.code, issue.message);
}

}  // namespace

absl::Status OracleMenuIndexCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  if (auto table_filter = parser.GetString("table");
      table_filter.has_value() && table_filter->empty()) {
    return absl::InvalidArgumentError("--table cannot be empty");
  }
  return absl::OkStatus();
}

absl::Status OracleMenuIndexCommandHandler::Execute(
    Rom* /*rom*/, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  ASSIGN_OR_RETURN(const auto root,
                   core::ResolveOracleProjectRoot(ResolveProjectPath(parser)));
  ASSIGN_OR_RETURN(auto registry, core::BuildOracleMenuRegistry(root));

  const std::string table_filter = parser.GetString("table").value_or("");
  std::string draw_filter = parser.GetString("draw-filter").value_or("");
  draw_filter = absl::AsciiStrToLower(draw_filter);
  const bool missing_bins_only = parser.HasFlag("missing-bins");

  std::vector<const core::OracleMenuBinEntry*> bins;
  bins.reserve(registry.bins.size());
  for (const auto& entry : registry.bins) {
    if (missing_bins_only && entry.exists) {
      continue;
    }
    bins.push_back(&entry);
  }

  std::vector<const core::OracleMenuDrawRoutine*> draw_routines;
  draw_routines.reserve(registry.draw_routines.size());
  for (const auto& routine : registry.draw_routines) {
    if (!draw_filter.empty()) {
      std::string label_lower = absl::AsciiStrToLower(routine.label);
      if (label_lower.find(draw_filter) == std::string::npos) {
        continue;
      }
    }
    draw_routines.push_back(&routine);
  }

  std::vector<const core::OracleMenuComponent*> components;
  components.reserve(registry.components.size());
  for (const auto& component : registry.components) {
    if (!table_filter.empty() && component.table_label != table_filter) {
      continue;
    }
    components.push_back(&component);
  }

  formatter.AddField("project_root", root.string());
  formatter.AddField("asm_files", static_cast<int>(registry.asm_files.size()));
  formatter.AddField("bin_count", static_cast<int>(bins.size()));
  formatter.AddField("draw_routine_count",
                     static_cast<int>(draw_routines.size()));
  formatter.AddField("component_count", static_cast<int>(components.size()));
  formatter.AddField("warnings", static_cast<int>(registry.warnings.size()));

  formatter.BeginArray("bins");
  for (const auto* entry : bins) {
    formatter.AddArrayItem(absl::StrFormat(
        "%s | %s:%d | %s | %s bytes | %s",
        entry->label.empty() ? "(unlabeled)" : entry->label, entry->asm_path,
        entry->line, entry->resolved_bin_path, FormatSize(entry->size_bytes),
        entry->exists ? "ok" : "missing"));
  }
  formatter.EndArray();

  formatter.BeginArray("draw_routines");
  for (const auto* routine : draw_routines) {
    formatter.AddArrayItem(absl::StrFormat(
        "%s | %s:%d | refs=%d%s", routine->label, routine->asm_path,
        routine->line, routine->references, routine->local ? " | local" : ""));
  }
  formatter.EndArray();

  formatter.BeginArray("components");
  for (const auto* component : components) {
    formatter.AddArrayItem(
        absl::StrFormat("%s[%d] | (%d,%d) | %s:%d%s%s", component->table_label,
                        component->index, component->row, component->col,
                        component->asm_path, component->line,
                        component->note.empty() ? "" : " | ", component->note));
  }
  formatter.EndArray();

  formatter.BeginArray("warnings_list");
  for (const auto& warning : registry.warnings) {
    formatter.AddArrayItem(warning);
  }
  formatter.EndArray();

  return absl::OkStatus();
}

absl::Status OracleMenuSetOffsetCommandHandler::Execute(
    Rom* /*rom*/, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  ASSIGN_OR_RETURN(const int index, parser.GetInt("index"));
  ASSIGN_OR_RETURN(const int row, parser.GetInt("row"));
  ASSIGN_OR_RETURN(const int col, parser.GetInt("col"));

  const std::filesystem::path project_path = ResolveProjectPath(parser);
  ASSIGN_OR_RETURN(const auto root,
                   core::ResolveOracleProjectRoot(project_path));
  const std::string asm_path = parser.GetString("asm").value_or("");
  const std::string table_label = parser.GetString("table").value_or("");
  const bool write_changes = parser.HasFlag("write");

  ASSIGN_OR_RETURN(auto result, core::SetOracleMenuComponentOffset(
                                    project_path, asm_path, table_label, index,
                                    row, col, write_changes));

  formatter.AddField("project_root", root.string());
  formatter.AddField("asm", result.asm_path);
  formatter.AddField("line", result.line);
  formatter.AddField("table", result.table_label);
  formatter.AddField("index", result.index);
  formatter.AddField("old_row", result.old_row);
  formatter.AddField("old_col", result.old_col);
  formatter.AddField("new_row", result.new_row);
  formatter.AddField("new_col", result.new_col);
  formatter.AddField("changed", result.changed);
  formatter.AddField("write_applied", result.write_applied);
  formatter.AddField("mode", write_changes ? "write" : "dry-run");
  formatter.AddField("old_line", result.old_line);
  formatter.AddField("new_line", result.new_line);

  return absl::OkStatus();
}

absl::Status OracleMenuValidateCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  if (auto max_row_str = parser.GetString("max-row"); max_row_str.has_value()) {
    ASSIGN_OR_RETURN(const int max_row, parser.GetInt("max-row"));
    if (max_row < 0) {
      return absl::InvalidArgumentError("--max-row must be >= 0");
    }
  }
  if (auto max_col_str = parser.GetString("max-col"); max_col_str.has_value()) {
    ASSIGN_OR_RETURN(const int max_col, parser.GetInt("max-col"));
    if (max_col < 0) {
      return absl::InvalidArgumentError("--max-col must be >= 0");
    }
  }
  return absl::OkStatus();
}

absl::Status OracleMenuValidateCommandHandler::Execute(
    Rom* /*rom*/, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  const std::filesystem::path project_path = ResolveProjectPath(parser);
  ASSIGN_OR_RETURN(const auto root,
                   core::ResolveOracleProjectRoot(project_path));
  ASSIGN_OR_RETURN(auto registry, core::BuildOracleMenuRegistry(root));

  int max_row = 31;
  int max_col = 31;
  if (parser.GetString("max-row").has_value()) {
    ASSIGN_OR_RETURN(max_row, parser.GetInt("max-row"));
  }
  if (parser.GetString("max-col").has_value()) {
    ASSIGN_OR_RETURN(max_col, parser.GetInt("max-col"));
  }
  const bool strict = parser.HasFlag("strict");

  core::OracleMenuValidationReport report =
      core::ValidateOracleMenuRegistry(registry, max_row, max_col);
  const bool failed = report.errors > 0 || (strict && report.warnings > 0);

  formatter.AddField("project_root", root.string());
  formatter.AddField("asm_files", static_cast<int>(registry.asm_files.size()));
  formatter.AddField("bin_count", static_cast<int>(registry.bins.size()));
  formatter.AddField("draw_routine_count",
                     static_cast<int>(registry.draw_routines.size()));
  formatter.AddField("component_count", static_cast<int>(registry.components.size()));
  formatter.AddField("max_row", max_row);
  formatter.AddField("max_col", max_col);
  formatter.AddField("strict", strict);
  formatter.AddField("errors", report.errors);
  formatter.AddField("warnings", report.warnings);
  formatter.AddField("status", failed ? "fail" : "pass");

  formatter.BeginArray("issues");
  for (const auto& issue : report.issues) {
    formatter.AddArrayItem(FormatIssueLine(issue));
  }
  formatter.EndArray();

  if (failed) {
    formatter.AddField("failure_reason", "Oracle menu validation failed");
    return absl::FailedPreconditionError("Oracle menu validation failed");
  }

  return absl::OkStatus();
}

// ---------------------------------------------------------------------------
// DungeonOraclePreflightCommandHandler::Execute
// ---------------------------------------------------------------------------

absl::Status DungeonOraclePreflightCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  // Probe --report path writability here, before CommandHandler::Run() calls
  // formatter.BeginObject(). This is the only hook guaranteed to run before
  // any formatter output, so a failure produces true zero-stdout semantics:
  // only stderr + non-zero exit, never "{}".
  if (auto report_path_opt = parser.GetString("report");
      report_path_opt.has_value() && !report_path_opt->empty()) {
    const std::filesystem::path report_path(*report_path_opt);
    std::error_code exists_ec;
    const bool existed_before = std::filesystem::exists(report_path, exists_ec);

    // Never use truncation in the probe path: validation must not clobber
    // existing files if later argument parsing fails in Execute().
    std::ofstream probe(*report_path_opt,
                        std::ios::out | std::ios::binary | std::ios::app);
    if (!probe.is_open()) {
      return absl::PermissionDeniedError(
          absl::StrFormat("dungeon-oracle-preflight: cannot open report file "
                          "for writing: %s",
                          *report_path_opt));
    }
    probe.close();

    // If the probe created a new file, remove it so ValidateArgs remains
    // side-effect free on later parse failures.
    if (!existed_before) {
      std::error_code remove_ec;
      std::filesystem::remove(report_path, remove_ec);
    }
  }
  return absl::OkStatus();
}

absl::Status DungeonOraclePreflightCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  // --report path was already probed in ValidateArgs() (before the formatter
  // was created), so a write failure here cannot produce contradictory output.

  // Parse optional required-collision-rooms list.
  std::vector<int> required_rooms;
  if (auto rooms_opt = parser.GetString("required-collision-rooms");
      rooms_opt.has_value()) {
    for (absl::string_view token :
         absl::StrSplit(rooms_opt.value(), ',', absl::SkipEmpty())) {
      std::string trimmed =
          std::string(absl::StripAsciiWhitespace(token));
      int room_id = 0;
      if (!util::ParseHexString(trimmed, &room_id)) {
        return absl::InvalidArgumentError(absl::StrFormat(
            "Invalid room ID in --required-collision-rooms: '%s'", trimmed));
      }
      required_rooms.push_back(room_id);
    }
  }

  // Build preflight options from flags.
  zelda3::OracleRomSafetyPreflightOptions options;
  options.require_water_fill_reserved_region = true;
  options.require_custom_collision_write_support =
      parser.HasFlag("require-write-support");
  options.validate_water_fill_table = true;
  options.validate_custom_collision_maps =
      !parser.HasFlag("skip-collision-maps");
  options.room_ids_requiring_custom_collision = required_rooms;

  // Run the preflight.
  const auto preflight = zelda3::RunOracleRomSafetyPreflight(rom, options);

  // Count per-check errors so we can report granular booleans.
  bool water_fill_region_ok = true;
  bool water_fill_table_ok = true;
  bool custom_collision_maps_ok = true;
  bool required_rooms_ok = true;
  for (const auto& err : preflight.errors) {
    if (err.code == "ORACLE_WATER_FILL_REGION_MISSING" ||
        err.code == "ORACLE_COLLISION_WRITE_REGION_MISSING") {
      water_fill_region_ok = false;
    } else if (err.code == "ORACLE_WATER_FILL_HEADER_CORRUPT" ||
               err.code == "ORACLE_WATER_FILL_TABLE_INVALID") {
      water_fill_table_ok = false;
    } else if (err.code == "ORACLE_COLLISION_POINTER_INVALID" ||
               err.code == "ORACLE_COLLISION_POINTER_INVALID_TRUNCATED") {
      custom_collision_maps_ok = false;
    } else if (err.code == "ORACLE_REQUIRED_ROOM_MISSING_COLLISION" ||
               err.code == "ORACLE_REQUIRED_ROOM_OUT_OF_RANGE") {
      required_rooms_ok = false;
    }
  }

  const bool failed = !preflight.ok();

  // Determine whether the required-room check actually ran. It is gated on
  // HasCustomCollisionWriteSupport in the preflight library, so on a small ROM
  // the check is silently skipped. Reflect that honestly in the output.
  const bool required_check_ran =
      !required_rooms.empty() &&
      zelda3::HasCustomCollisionWriteSupport(rom->vector().size());

  // Build a machine-readable JSON report in parallel with the formatter so
  // that --report writes the full structured output, not a reduced stub.
  using json = nlohmann::json;
  json report;
  report["ok"] = !failed;
  report["error_count"] = static_cast<int>(preflight.errors.size());
  report["water_fill_region_ok"] = water_fill_region_ok;
  report["water_fill_table_ok"] = water_fill_table_ok;
  report["custom_collision_maps_ok"] = custom_collision_maps_ok;

  if (!required_rooms.empty()) {
    report["required_rooms_checked"] = static_cast<int>(required_rooms.size());
    report["required_rooms_check"] = required_check_ran ? "ran" : "skipped";
    if (required_check_ran) {
      report["required_rooms_ok"] = required_rooms_ok;
    }
  }

  json errors_arr = json::array();
  for (const auto& err : preflight.errors) {
    json entry;
    entry["code"] = err.code;
    entry["message"] = err.message;
    entry["status"] = std::string(absl::StatusCodeToString(err.status_code));
    if (err.room_id >= 0) {
      entry["room_id"] = absl::StrFormat("0x%02X", err.room_id);
    }
    errors_arr.push_back(std::move(entry));
  }
  report["errors"] = std::move(errors_arr);
  report["status"] = failed ? "fail" : "pass";

  // Emit structured output to the formatter.
  formatter.BeginObject("Dungeon Oracle Preflight");
  formatter.AddField("ok", !failed);
  formatter.AddField("error_count", static_cast<int>(preflight.errors.size()));
  formatter.AddField("water_fill_region_ok", water_fill_region_ok);
  formatter.AddField("water_fill_table_ok", water_fill_table_ok);
  formatter.AddField("custom_collision_maps_ok", custom_collision_maps_ok);
  if (!required_rooms.empty()) {
    formatter.AddField("required_rooms_checked",
                       static_cast<int>(required_rooms.size()));
    formatter.AddField("required_rooms_check",
                       required_check_ran ? std::string("ran")
                                          : std::string("skipped"));
    if (required_check_ran) {
      formatter.AddField("required_rooms_ok", required_rooms_ok);
    }
  }
  formatter.BeginArray("errors");
  for (const auto& err : preflight.errors) {
    formatter.BeginObject();
    formatter.AddField("code", err.code);
    formatter.AddField("message", err.message);
    formatter.AddField("status",
                       std::string(absl::StatusCodeToString(err.status_code)));
    if (err.room_id >= 0) {
      formatter.AddHexField("room_id", err.room_id, 2);
    }
    formatter.EndObject();
  }
  formatter.EndArray();
  formatter.AddField("status", failed ? "fail" : "pass");
  formatter.EndObject();

  // Write the full JSON report to a file if --report was given.
  // Fail loudly on write errors so the caller knows the report is missing.
  if (auto report_path = parser.GetString("report");
      report_path.has_value() && !report_path->empty()) {
    const std::string report_content = report.dump(2) + "\n";
    std::ofstream report_file(*report_path,
                              std::ios::out | std::ios::binary | std::ios::trunc);
    if (!report_file.is_open()) {
      return absl::PermissionDeniedError(
          absl::StrFormat("dungeon-oracle-preflight: cannot open report file "
                          "for writing: %s",
                          *report_path));
    }
    report_file << report_content;
    if (!report_file.good()) {
      return absl::InternalError(
          absl::StrFormat("dungeon-oracle-preflight: failed while writing "
                          "report file: %s",
                          *report_path));
    }
  }

  if (failed) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Oracle ROM preflight failed (%d error(s))",
                        static_cast<int>(preflight.errors.size())));
  }
  return absl::OkStatus();
}

}  // namespace yaze::cli::handlers
