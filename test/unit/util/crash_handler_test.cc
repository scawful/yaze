#include "util/crash_handler.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "gtest/gtest.h"
#include "unique_temp_path.h"

namespace yaze::util {
namespace {

std::vector<std::filesystem::path> CrashLogsIn(
    const std::filesystem::path& dir) {
  std::vector<std::filesystem::path> logs;
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    const std::string name = entry.path().filename().string();
    if (name.starts_with("crash_") && entry.path().extension() == ".log") {
      logs.push_back(entry.path());
    }
  }
  return logs;
}

std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream contents;
  contents << in.rdbuf();
  return contents.str();
}

void WriteFile(const std::filesystem::path& path, const std::string& data) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << data;
}

constexpr char kLegacyHeader[] =
    "=== YAZE Crash Report ===\n"
    "Version: 0.9.0\n"
    "Timestamp: 2026-10-28 19:36:20\n"
    "Platform: macOS\n"
    "========================\n\n";

class CrashHandlerTest : public ::testing::Test {
 protected:
  void SetUp() override { dir_ = test::UniqueTempPath("yaze_crash_logs"); }

  void TearDown() override {
    CrashHandler::ResetForTesting();
    std::error_code ec;
    std::filesystem::remove_all(dir_, ec);
  }

  std::filesystem::path dir_;
};

// A normal session (prepare, then clean shutdown with no crash) used to leave a
// header-only crash_*.log that read as a crash.
TEST_F(CrashHandlerTest, CleanSessionLeavesNoCrashLog) {
  CrashHandler::PrepareCrashLogForTesting("test-1.2.3", dir_);
  const auto pending = CrashHandler::PendingCrashLogPathForTesting();
  EXPECT_EQ(pending.parent_path(), dir_);
  EXPECT_FALSE(std::filesystem::exists(pending));

  CrashHandler::ResetForTesting();

  EXPECT_TRUE(CrashLogsIn(dir_).empty());
}

TEST_F(CrashHandlerTest, FirstHandlerWriteCreatesLogWithHeaderAndReport) {
  CrashHandler::PrepareCrashLogForTesting("test-1.2.3", dir_);
  const auto pending = CrashHandler::PendingCrashLogPathForTesting();

  CrashHandler::WriteCrashLogForTesting("crash_handler_test: report line\n");
  CrashHandler::WriteCrashLogForTesting("crash_handler_test: frame line\n");
  CrashHandler::WriteCrashLogForTesting(nullptr);
  CrashHandler::ResetForTesting();

  ASSERT_TRUE(std::filesystem::exists(pending));
  const std::string contents = ReadFile(pending);
  EXPECT_TRUE(contents.starts_with("=== YAZE Crash Report ===\n")) << contents;
  EXPECT_NE(contents.find("Version: test-1.2.3\n"), std::string::npos);
  const auto report_pos = contents.find("crash_handler_test: report line\n");
  const auto frame_pos = contents.find("crash_handler_test: frame line\n");
  ASSERT_NE(report_pos, std::string::npos) << contents;
  ASSERT_NE(frame_pos, std::string::npos) << contents;
  EXPECT_LT(report_pos, frame_pos);
  EXPECT_FALSE(CrashHandler::IsHeaderOnlyCrashLog(pending));
  EXPECT_EQ(CrashLogsIn(dir_).size(), 1U);
}

TEST_F(CrashHandlerTest, RecognizesHeaderOnlyLogsFromOlderBuilds) {
  std::filesystem::create_directories(dir_);
  const auto lf = dir_ / "crash_20261028_193620.log";
  const auto crlf = dir_ / "crash_20261028_193621.log";
  const auto real = dir_ / "crash_20261028_193622.log";
  WriteFile(lf, kLegacyHeader);
  std::string crlf_header;
  for (const char c : std::string(kLegacyHeader)) {
    if (c == '\n') {
      crlf_header += '\r';
    }
    crlf_header += c;
  }
  WriteFile(crlf, crlf_header);
  WriteFile(real, std::string(kLegacyHeader) + "*** SIGABRT received ***\n");

  EXPECT_TRUE(CrashHandler::IsHeaderOnlyCrashLog(lf));
  EXPECT_TRUE(CrashHandler::IsHeaderOnlyCrashLog(crlf));
  EXPECT_FALSE(CrashHandler::IsHeaderOnlyCrashLog(real));
  EXPECT_FALSE(CrashHandler::IsHeaderOnlyCrashLog(dir_ / "missing.log"));
}

// The test runner points YAZE_APP_DATA_DIR at a per-process temp directory, so
// GetCrashLogDirectory() never resolves to the developer's real crash logs.
TEST_F(CrashHandlerTest, CleanupRemovesHeaderOnlyLogsAndKeepsRealCrashes) {
  if (std::getenv("YAZE_APP_DATA_DIR") == nullptr) {
    GTEST_SKIP() << "YAZE_APP_DATA_DIR is not isolated";
  }
  const auto crash_dir = CrashHandler::GetCrashLogDirectory();
  std::filesystem::create_directories(crash_dir);
  const auto header_only = crash_dir / "crash_20000101_000001.log";
  const auto real = crash_dir / "crash_20000101_000002.log";
  WriteFile(header_only, kLegacyHeader);
  WriteFile(real, std::string(kLegacyHeader) + "*** SIGSEGV received ***\n");

  EXPECT_EQ(CrashHandler::GetMostRecentCrashLog(), real);

  CrashHandler::CleanupOldLogs(100);

  EXPECT_FALSE(std::filesystem::exists(header_only));
  EXPECT_TRUE(std::filesystem::exists(real));

  std::error_code ec;
  std::filesystem::remove(real, ec);
}

}  // namespace
}  // namespace yaze::util
