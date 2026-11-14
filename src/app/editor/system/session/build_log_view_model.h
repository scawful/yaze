#ifndef YAZE_APP_EDITOR_SYSTEM_SESSION_BUILD_LOG_VIEW_MODEL_H_
#define YAZE_APP_EDITOR_SYSTEM_SESSION_BUILD_LOG_VIEW_MODEL_H_

#include <string>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"

namespace yaze::editor {

// Severity of one build-log line or step. Ordered so that a larger value is
// the more important outcome when a step summarizes its lines.
enum class BuildLogSeverity { kInfo, kSuccess, kSkipped, kWarning, kError };

struct BuildLogLine {
  BuildLogSeverity severity = BuildLogSeverity::kInfo;
  std::string text;
  // "file:line" for assembler-style diagnostics, otherwise empty.
  std::string location;
};

struct BuildLogStep {
  std::string title;
  BuildLogSeverity severity = BuildLogSeverity::kInfo;
  std::vector<BuildLogLine> lines;
};

struct BuildLogSummary {
  std::vector<BuildLogStep> steps;
  int passed = 0;
  int warnings = 0;
  int errors = 0;
  int skipped = 0;
  // Path printed by a "Build receipt: <path>" line, if any.
  std::string receipt_path;
};

// Groups command output into steps. "[*] text" starts a step and
// "---  Name  ---" starts a section; "[+]", "[-]" and "[!]" mark passed,
// skipped and failed results; "file:line: warning|error:" and leading
// "warning:"/"error:" are diagnostics. Other lines are step detail.
BuildLogSummary ParseBuildLog(std::string_view log);

struct BuildReceiptCheck {
  std::string name;
  std::string status;  // passed, failed, unavailable, skipped, ...
  bool required = false;
  std::string detail;
};

struct BuildReceiptSummary {
  std::string state;
  int exit_code = 0;
  std::vector<std::string> required_gaps;
  std::vector<std::string> optional_gaps;
  std::vector<BuildReceiptCheck> checks;
};

// Reads the checks of an Oracle-style build receipt JSON document.
absl::StatusOr<BuildReceiptSummary> ParseBuildReceipt(std::string_view json);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_SYSTEM_SESSION_BUILD_LOG_VIEW_MODEL_H_
