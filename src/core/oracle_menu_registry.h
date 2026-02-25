#ifndef YAZE_CORE_ORACLE_MENU_REGISTRY_H_
#define YAZE_CORE_ORACLE_MENU_REGISTRY_H_

#include <filesystem>
#include <string>
#include <vector>

#include "absl/status/statusor.h"

namespace yaze::core {

struct OracleMenuBinEntry {
  std::string label;
  std::string asm_path;
  int line = 0;
  std::string bin_path;
  std::string resolved_bin_path;
  bool exists = false;
  uintmax_t size_bytes = 0;
};

struct OracleMenuDrawRoutine {
  std::string label;
  std::string asm_path;
  int line = 0;
  bool local = false;
  int references = 0;
};

struct OracleMenuComponent {
  std::string table_label;
  int index = 0;
  int row = 0;
  int col = 0;
  std::string note;
  std::string asm_path;
  int line = 0;
};

struct OracleMenuRegistry {
  std::filesystem::path project_root;
  std::vector<std::string> asm_files;
  std::vector<OracleMenuBinEntry> bins;
  std::vector<OracleMenuDrawRoutine> draw_routines;
  std::vector<OracleMenuComponent> components;
  std::vector<std::string> warnings;
};

enum class OracleMenuValidationSeverity {
  kError,
  kWarning,
};

struct OracleMenuValidationIssue {
  OracleMenuValidationSeverity severity = OracleMenuValidationSeverity::kError;
  std::string code;
  std::string message;
  std::string asm_path;
  int line = 0;
};

struct OracleMenuValidationReport {
  int errors = 0;
  int warnings = 0;
  std::vector<OracleMenuValidationIssue> issues;
};

struct OracleMenuComponentEditResult {
  std::string asm_path;
  int line = 0;
  std::string table_label;
  int index = 0;
  int old_row = 0;
  int old_col = 0;
  int new_row = 0;
  int new_col = 0;
  std::string old_line;
  std::string new_line;
  bool changed = false;
  bool write_applied = false;
};

absl::StatusOr<std::filesystem::path> ResolveOracleProjectRoot(
    const std::filesystem::path& start_path);

absl::StatusOr<OracleMenuRegistry> BuildOracleMenuRegistry(
    const std::filesystem::path& project_root);

OracleMenuValidationReport ValidateOracleMenuRegistry(
    const OracleMenuRegistry& registry, int max_row = 31, int max_col = 31);

absl::StatusOr<OracleMenuComponentEditResult> SetOracleMenuComponentOffset(
    const std::filesystem::path& project_root,
    const std::string& asm_relative_path, const std::string& table_label,
    int index, int row, int col, bool write_changes);

}  // namespace yaze::core

#endif  // YAZE_CORE_ORACLE_MENU_REGISTRY_H_
