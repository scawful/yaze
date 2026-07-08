#include "app/editor/dungeon/ui/reporting/dungeon_issue_report_storage.h"

#include <cctype>
#include <cstdio>
#include <fstream>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "util/platform_paths.h"

namespace yaze::editor {
namespace {

constexpr char kIssueReportsSubdir[] = "issue_reports";
constexpr char kDungeonReportsSubdir[] = "dungeon";
constexpr char kDungeonScreenshotsSubdir[] = "screenshots";
constexpr char kDungeonIssueLogFilename[] = "dungeon_issues.md";
constexpr char kDefaultIssueHeading[] = "Dungeon issue report";
constexpr char kUnknownTimestampLabel[] = "Unknown time";
constexpr char kObservedIssueHeading[] = "### Observed Issue\n";
constexpr char kDiagnosticsHeading[] = "### Diagnostics\n";

void WriteDetailLine(std::ofstream& out, const char* label,
                     const std::string& value) {
  if (!value.empty()) {
    out << "- " << label << ": " << value << "\n";
  }
}

void WriteRoomLine(std::ofstream& out, int room_id) {
  if (room_id >= 0) {
    out << "- Room: 0x" << absl::StrFormat("%03X", room_id) << "\n";
  }
}

std::string SanitizeFilenameComponent(std::string value) {
  for (char& ch : value) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    if ((uch >= 'a' && uch <= 'z') || (uch >= 'A' && uch <= 'Z') ||
        (uch >= '0' && uch <= '9')) {
      ch = static_cast<char>(std::tolower(uch));
    } else {
      ch = '_';
    }
  }

  std::string sanitized;
  sanitized.reserve(value.size());
  bool last_was_underscore = false;
  for (char ch : value) {
    if (ch == '_') {
      if (!last_was_underscore) {
        sanitized.push_back(ch);
      }
      last_was_underscore = true;
      continue;
    }
    sanitized.push_back(ch);
    last_was_underscore = false;
  }

  while (!sanitized.empty() && sanitized.front() == '_') {
    sanitized.erase(sanitized.begin());
  }
  while (!sanitized.empty() && sanitized.back() == '_') {
    sanitized.pop_back();
  }

  if (sanitized.empty()) {
    sanitized = "issue";
  }
  return sanitized;
}

std::string BuildRoomFileStem(int room_id) {
  return room_id >= 0 ? absl::StrFormat("room_%03x", room_id) : "room_unknown";
}

}  // namespace

absl::StatusOr<DungeonIssueReportPaths> ResolveDungeonIssueReportPaths() {
  auto base_dir_or =
      util::PlatformPaths::GetAppDataSubdirectory(kIssueReportsSubdir);
  if (!base_dir_or.ok()) {
    return base_dir_or.status();
  }

  DungeonIssueReportPaths paths;
  paths.reports_dir = *base_dir_or / kDungeonReportsSubdir;
  auto status = util::PlatformPaths::EnsureDirectoryExists(paths.reports_dir);
  if (!status.ok()) {
    return status;
  }

  paths.screenshots_dir = paths.reports_dir / kDungeonScreenshotsSubdir;
  status = util::PlatformPaths::EnsureDirectoryExists(paths.screenshots_dir);
  if (!status.ok()) {
    return status;
  }

  paths.log_path = paths.reports_dir / kDungeonIssueLogFilename;
  return paths;
}

absl::StatusOr<std::filesystem::path> BuildDungeonIssueScreenshotPath(
    int room_id, const std::string& category_label,
    const std::string& timestamp_slug) {
  auto paths_or = ResolveDungeonIssueReportPaths();
  if (!paths_or.ok()) {
    return paths_or.status();
  }

  return paths_or->screenshots_dir /
         absl::StrFormat("%s_%s_%s.bmp", BuildRoomFileStem(room_id),
                         SanitizeFilenameComponent(category_label),
                         timestamp_slug);
}

absl::StatusOr<std::filesystem::path> AppendDungeonIssueLogEntry(
    const DungeonIssueLogEntry& entry) {
  auto paths_or = ResolveDungeonIssueReportPaths();
  if (!paths_or.ok()) {
    return paths_or.status();
  }

  const std::filesystem::path& log_path = paths_or->log_path;
  std::ofstream out(log_path, std::ios::app);
  if (!out) {
    return absl::InternalError(
        absl::StrFormat("Failed to open issue log: %s", log_path.string()));
  }

  const std::string heading =
      entry.heading.empty() ? std::string(kDefaultIssueHeading) : entry.heading;
  const std::string timestamp = entry.timestamp_display.empty()
                                    ? std::string(kUnknownTimestampLabel)
                                    : entry.timestamp_display;

  out << "## " << timestamp << " - " << heading << "\n";
  WriteDetailLine(out, "Category", entry.category_label);
  WriteDetailLine(out, "Scope", entry.scope);
  WriteRoomLine(out, entry.room_id);
  WriteDetailLine(out, "Screenshot", entry.screenshot_path);
  out << "\n";

  if (!entry.observed_issue.empty()) {
    out << kObservedIssueHeading;
    out << entry.observed_issue << "\n\n";
  }

  out << kDiagnosticsHeading;
  out << "```text\n" << entry.diagnostics << "\n```\n\n";
  out.flush();
  if (!out) {
    return absl::InternalError(absl::StrFormat(
        "Failed while writing issue log: %s", log_path.string()));
  }

  return log_path;
}

}  // namespace yaze::editor
