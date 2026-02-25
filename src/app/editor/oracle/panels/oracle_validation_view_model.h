// Oracle Validation View Model
//
// Pure C++ structs + helpers for:
//   - Parsing oracle-smoke-check and dungeon-oracle-preflight JSON output
//   - Building command argument lists
//   - Reconstructing CLI commands for copy-paste
//
// Separated from the panel to allow independent unit testing.

#ifndef YAZE_APP_EDITOR_ORACLE_PANELS_ORACLE_VALIDATION_VIEW_MODEL_H_
#define YAZE_APP_EDITOR_ORACLE_PANELS_ORACLE_VALIDATION_VIEW_MODEL_H_

#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze::editor::oracle_validation {

struct SmokeD4 {
  bool structural_ok = false;
  std::string required_rooms_check;      // "ran" | "skipped"
  std::optional<bool> required_rooms_ok; // absent when skipped
};

struct SmokeD6 {
  bool ok = false;
  int track_rooms_found = 0;
  int min_track_rooms = 0;
  bool meets_min_track_rooms = false;
};

struct SmokeD3 {
  std::string readiness_check; // "ran" | "skipped"
  std::optional<bool> ok;      // absent when skipped
};

struct SmokeResult {
  bool ok = false;
  std::string status; // "pass" | "fail"
  bool strict_readiness = false;
  SmokeD4 d4;
  SmokeD6 d6;
  SmokeD3 d3;
};

struct PreflightError {
  std::string code;
  std::string message;
  std::string status_code;
  std::optional<std::string> room_id;
};

struct PreflightResult {
  bool ok = false;
  int error_count = 0;
  bool water_fill_region_ok = false;
  bool water_fill_table_ok = false;
  bool custom_collision_maps_ok = false;
  std::string required_rooms_check; // "ran" | "skipped"
  std::optional<bool> required_rooms_ok;
  std::vector<PreflightError> errors;
  std::string status; // "pass" | "fail"
};

enum class RunMode { kStructural, kStrictReadiness, kPreflight };

struct OracleRunResult {
  RunMode mode = RunMode::kStructural;

  bool command_ok = false;
  absl::StatusCode status_code = absl::StatusCode::kOk;
  std::string error_message;
  std::string raw_output;
  std::string cli_command;
  std::string timestamp;

  std::optional<SmokeResult> smoke;
  std::optional<PreflightResult> preflight;

  bool json_parse_failed = false;
};

absl::StatusOr<SmokeResult> ParseSmokeCheckOutput(const std::string& json_str);
absl::StatusOr<PreflightResult> ParsePreflightOutput(
    const std::string& json_str);

struct SmokeOptions {
  std::string rom_path;
  int min_d6_track_rooms = 4;
  bool strict_readiness = false;
  std::string report_path;
};

struct PreflightOptions {
  std::string rom_path;
  std::string required_collision_rooms;
  bool require_write_support = false;
  bool skip_collision_maps = false;
  std::string report_path;
};

std::vector<std::string> BuildSmokeArgs(const SmokeOptions& opts);
std::vector<std::string> BuildPreflightArgs(const PreflightOptions& opts);

std::string BuildCliCommand(const std::string& command_name,
                            const std::vector<std::string>& args);

std::string CurrentTimestamp();

}  // namespace yaze::editor::oracle_validation

#endif  // YAZE_APP_EDITOR_ORACLE_PANELS_ORACLE_VALIDATION_VIEW_MODEL_H_
