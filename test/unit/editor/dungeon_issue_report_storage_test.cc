#include "app/editor/dungeon/ui/reporting/dungeon_issue_report_storage.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

namespace yaze::editor {
namespace {

class ScopedEnvVar {
 public:
  ScopedEnvVar(const char* name, const std::string& value) : name_(name) {
    const char* existing = std::getenv(name_.c_str());
    if (existing) {
      had_original_ = true;
      original_value_ = existing;
    }
    SetEnv(name_.c_str(), value.c_str());
  }

  ~ScopedEnvVar() {
    if (had_original_) {
      SetEnv(name_.c_str(), original_value_.c_str());
    } else {
#ifdef _WIN32
      _putenv_s(name_.c_str(), "");
#else
      unsetenv(name_.c_str());
#endif
    }
  }

 private:
  // setenv/unsetenv are POSIX-only; use _putenv_s on Windows (clang-cl/MSVC).
  static void SetEnv(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
  }

  std::string name_;
  std::string original_value_;
  bool had_original_ = false;
};

std::filesystem::path MakeTempHomeRoot() {
  const auto unique_suffix = std::to_string(
      std::chrono::steady_clock::now().time_since_epoch().count());
  return std::filesystem::temp_directory_path() /
         ("yaze_dungeon_issue_report_storage_" + unique_suffix);
}

std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream in(path);
  return std::string((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
}

TEST(DungeonIssueReportStorageTest,
     ResolvePathsCreatesExpectedDungeonIssueDirectories) {
  const std::filesystem::path temp_home = MakeTempHomeRoot();
  ASSERT_TRUE(std::filesystem::create_directories(temp_home));
  ScopedEnvVar scoped_app_data("YAZE_APP_DATA_DIR",
                               (temp_home / ".yaze").string());

  const auto paths_or = ResolveDungeonIssueReportPaths();
  ASSERT_TRUE(paths_or.ok()) << paths_or.status();

  const std::filesystem::path expected_root =
      temp_home / ".yaze" / "issue_reports" / "dungeon";
  EXPECT_EQ(paths_or->reports_dir, expected_root);
  EXPECT_EQ(paths_or->screenshots_dir, expected_root / "screenshots");
  EXPECT_EQ(paths_or->log_path, expected_root / "dungeon_issues.md");
  EXPECT_TRUE(std::filesystem::exists(paths_or->reports_dir));
  EXPECT_TRUE(std::filesystem::exists(paths_or->screenshots_dir));

  std::filesystem::remove_all(temp_home);
}

TEST(DungeonIssueReportStorageTest,
     BuildScreenshotPathUsesDungeonReportDirectoryAndSanitizedCategory) {
  const std::filesystem::path temp_home = MakeTempHomeRoot();
  ASSERT_TRUE(std::filesystem::create_directories(temp_home));
  ScopedEnvVar scoped_app_data("YAZE_APP_DATA_DIR",
                               (temp_home / ".yaze").string());

  const auto path_or = BuildDungeonIssueScreenshotPath(
      0x25, "General room render mismatch", "20260420-120000");
  ASSERT_TRUE(path_or.ok()) << path_or.status();

  EXPECT_EQ(*path_or,
            temp_home / ".yaze" / "issue_reports" / "dungeon" / "screenshots" /
                "room_025_general_room_render_mismatch_20260420-120000.bmp");

  std::filesystem::remove_all(temp_home);
}

TEST(DungeonIssueReportStorageTest,
     AppendIssueLogEntryCreatesExpectedMarkdownLogFile) {
  const std::filesystem::path temp_home = MakeTempHomeRoot();
  ASSERT_TRUE(std::filesystem::create_directories(temp_home));
  ScopedEnvVar scoped_app_data("YAZE_APP_DATA_DIR",
                               (temp_home / ".yaze").string());

  DungeonIssueLogEntry entry;
  entry.timestamp_display = "2026-04-20 12:34:56 UTC";
  entry.heading = "Room 0x025 render mismatch";
  entry.category_label = "General room render mismatch";
  entry.scope = "Dungeon Render Issue Report";
  entry.room_id = 0x25;
  entry.screenshot_path =
      "/tmp/yaze/.yaze/issue_reports/dungeon/screenshots/room_025.bmp";
  entry.observed_issue = "Save path does not look obvious in the popup.";
  entry.diagnostics = "Room 0x025\nBG1 on\nBG2 off";

  const auto log_path_or = AppendDungeonIssueLogEntry(entry);
  ASSERT_TRUE(log_path_or.ok()) << log_path_or.status();
  ASSERT_TRUE(std::filesystem::exists(*log_path_or));

  const std::string content = ReadFile(*log_path_or);
  EXPECT_NE(
      content.find("## 2026-04-20 12:34:56 UTC - Room 0x025 render mismatch"),
      std::string::npos);
  EXPECT_NE(content.find("- Category: General room render mismatch"),
            std::string::npos);
  EXPECT_NE(content.find("- Scope: Dungeon Render Issue Report"),
            std::string::npos);
  EXPECT_NE(content.find("- Room: 0x025"), std::string::npos);
  EXPECT_NE(
      content.find(
          "- Screenshot: "
          "/tmp/yaze/.yaze/issue_reports/dungeon/screenshots/room_025.bmp"),
      std::string::npos);
  EXPECT_NE(content.find("### Observed Issue"), std::string::npos);
  EXPECT_NE(content.find("Save path does not look obvious in the popup."),
            std::string::npos);
  EXPECT_NE(content.find("### Diagnostics"), std::string::npos);
  EXPECT_NE(content.find("Room 0x025\nBG1 on\nBG2 off"), std::string::npos);

  std::filesystem::remove_all(temp_home);
}

TEST(DungeonIssueReportStorageTest,
     AppendIssueLogEntryAppendsWithoutOverwritingExistingReports) {
  const std::filesystem::path temp_home = MakeTempHomeRoot();
  ASSERT_TRUE(std::filesystem::create_directories(temp_home));
  ScopedEnvVar scoped_app_data("YAZE_APP_DATA_DIR",
                               (temp_home / ".yaze").string());

  DungeonIssueLogEntry first;
  first.timestamp_display = "2026-04-21 01:00:00 UTC";
  first.heading = "First report";
  first.category_label = "Palette mismatch";
  first.scope = "Dungeon Draw Issue Report";
  first.room_id = 0x01;
  first.diagnostics = "first diagnostics";

  DungeonIssueLogEntry second;
  second.timestamp_display = "2026-04-21 01:05:00 UTC";
  second.heading = "Second report";
  second.category_label = "Object draw mismatch";
  second.scope = "Dungeon Selection Issue Report";
  second.room_id = 0x77;
  second.diagnostics = "second diagnostics";

  const auto first_log_path_or = AppendDungeonIssueLogEntry(first);
  ASSERT_TRUE(first_log_path_or.ok()) << first_log_path_or.status();
  const auto second_log_path_or = AppendDungeonIssueLogEntry(second);
  ASSERT_TRUE(second_log_path_or.ok()) << second_log_path_or.status();
  EXPECT_EQ(*first_log_path_or, *second_log_path_or);

  const std::string content = ReadFile(*second_log_path_or);
  EXPECT_NE(content.find("## 2026-04-21 01:00:00 UTC - First report"),
            std::string::npos);
  EXPECT_NE(content.find("## 2026-04-21 01:05:00 UTC - Second report"),
            std::string::npos);
  EXPECT_LT(content.find("## 2026-04-21 01:00:00 UTC - First report"),
            content.find("## 2026-04-21 01:05:00 UTC - Second report"));

  std::filesystem::remove_all(temp_home);
}

}  // namespace
}  // namespace yaze::editor
