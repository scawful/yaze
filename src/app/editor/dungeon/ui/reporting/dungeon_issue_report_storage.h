#ifndef YAZE_APP_EDITOR_DUNGEON_UI_REPORTING_DUNGEON_ISSUE_REPORT_STORAGE_H
#define YAZE_APP_EDITOR_DUNGEON_UI_REPORTING_DUNGEON_ISSUE_REPORT_STORAGE_H

#include <filesystem>
#include <string>

#include "absl/status/statusor.h"

namespace yaze::editor {

struct DungeonIssueReportPaths {
  std::filesystem::path reports_dir;
  std::filesystem::path screenshots_dir;
  std::filesystem::path log_path;
};

struct DungeonIssueLogEntry {
  std::string timestamp_display;
  std::string heading;
  std::string category_label;
  std::string scope;
  int room_id = -1;
  std::string screenshot_path;
  std::string observed_issue;
  std::string diagnostics;
};

absl::StatusOr<DungeonIssueReportPaths> ResolveDungeonIssueReportPaths();

absl::StatusOr<std::filesystem::path> BuildDungeonIssueScreenshotPath(
    int room_id, const std::string& category_label,
    const std::string& timestamp_slug);

absl::StatusOr<std::filesystem::path> AppendDungeonIssueLogEntry(
    const DungeonIssueLogEntry& entry);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_UI_REPORTING_DUNGEON_ISSUE_REPORT_STORAGE_H
