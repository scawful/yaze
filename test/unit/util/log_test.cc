#include "util/log.h"

#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <system_error>

#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"

namespace yaze {
namespace util {
namespace {

// LogManager is a process-wide singleton, so every test configures it
// explicitly and the fixture restores the default (INFO, stderr, no category
// filtering) afterwards.
class LogManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    log_path_ =
        std::filesystem::temp_directory_path() /
        absl::StrCat(
            "yaze_log_test_",
            ::testing::UnitTest::GetInstance()->current_test_info()->name(),
            ".log");
    std::error_code ec;
    std::filesystem::remove(log_path_, ec);
  }

  void TearDown() override {
    LogManager::instance().configure(LogLevel::INFO, "", {});
    std::error_code ec;
    std::filesystem::remove(log_path_, ec);
  }

  void ConfigureToFile(LogLevel level,
                       const std::set<std::string>& categories = {}) {
    LogManager::instance().configure(level, log_path_.string(), categories);
  }

  std::string ReadLog() {
    std::ifstream in(log_path_);
    if (!in)
      return {};
    std::ostringstream contents;
    contents << in.rdbuf();
    return contents.str();
  }

  std::filesystem::path log_path_;
};

// The macros skip formatting when the message will not be emitted, so any
// side effect in an argument is skipped too. LOG_DEBUG is filtered at the
// default INFO level, which is how yaze runs unless --log-level says otherwise.
TEST_F(LogManagerTest, FilteredMacroDoesNotEvaluateArguments) {
  ConfigureToFile(LogLevel::INFO);

  int evaluations = 0;
  LOG_DEBUG("Test", "debug %d", ++evaluations);
  EXPECT_EQ(evaluations, 0);

  LOG_INFO("Test", "info %d", ++evaluations);
  EXPECT_EQ(evaluations, 1);
}

// The guard and the log call must not evaluate the caller's category
// expression twice.
TEST_F(LogManagerTest, MacroEvaluatesCategoryExpressionOnce) {
  ConfigureToFile(LogLevel::YAZE_DEBUG);

  int category_evaluations = 0;
  auto category = [&category_evaluations]() -> const char* {
    ++category_evaluations;
    return "Counted";
  };

  LOG_INFO(category(), "message");
  EXPECT_EQ(category_evaluations, 1);
}

// A category built with absl::StrCat is a temporary: it must stay alive for
// the whole macro body, not just the binding statement.
TEST_F(LogManagerTest, CategoryTemporaryOutlivesTheGuard) {
  ConfigureToFile(LogLevel::INFO);

  LOG_INFO(absl::StrCat("Temp", "Category"), "message %d", 7);

  EXPECT_NE(ReadLog().find("[TempCategory] message 7"), std::string::npos)
      << "log contents: " << ReadLog();
}

TEST_F(LogManagerTest, LevelFilteringMatchesConfiguredMinimum) {
  ConfigureToFile(LogLevel::WARNING);

  EXPECT_FALSE(LogManager::instance().ShouldLog(LogLevel::YAZE_DEBUG));
  EXPECT_FALSE(LogManager::instance().ShouldLog(LogLevel::INFO));
  EXPECT_TRUE(LogManager::instance().ShouldLog(LogLevel::WARNING));
  EXPECT_TRUE(LogManager::instance().ShouldLog(LogLevel::ERROR));
}

// An allowlist entry turns off every category it does not name; a "-" entry
// blocks one category while the rest stay enabled.
TEST_F(LogManagerTest, CategoryAllowlistAndBlocklist) {
  auto& log = LogManager::instance();

  ConfigureToFile(LogLevel::YAZE_DEBUG, {"Alpha", "-Beta"});
  EXPECT_TRUE(log.ShouldLog(LogLevel::YAZE_DEBUG, "Alpha"));
  EXPECT_FALSE(log.ShouldLog(LogLevel::YAZE_DEBUG, "Beta"));
  EXPECT_FALSE(log.ShouldLog(LogLevel::YAZE_DEBUG, "Gamma"));

  ConfigureToFile(LogLevel::YAZE_DEBUG, {"-Beta"});
  EXPECT_TRUE(log.ShouldLog(LogLevel::YAZE_DEBUG, "Alpha"));
  EXPECT_FALSE(log.ShouldLog(LogLevel::YAZE_DEBUG, "Beta"));
  EXPECT_TRUE(log.ShouldLog(LogLevel::YAZE_DEBUG, "Gamma"));

  // The level check still applies inside an enabled category.
  ConfigureToFile(LogLevel::ERROR, {"Alpha"});
  EXPECT_FALSE(log.ShouldLog(LogLevel::INFO, "Alpha"));
  EXPECT_TRUE(log.ShouldLog(LogLevel::ERROR, "Alpha"));
}

// configure() documents an empty path as "log to stderr", so it must release
// the file it was writing to. It previously kept the stream open, which also
// held a Windows file lock.
TEST_F(LogManagerTest, ReconfiguringToStderrReleasesTheLogFile) {
  ConfigureToFile(LogLevel::INFO);
  LOG_INFO("Test", "before %d", 1);
  ASSERT_NE(ReadLog().find("before 1"), std::string::npos);

  LogManager::instance().configure(LogLevel::INFO, "", {});
  LOG_INFO("Test", "after %d", 2);

  const std::string contents = ReadLog();
  EXPECT_EQ(contents.find("after 2"), std::string::npos)
      << "log contents: " << contents;

  // On Windows an open file cannot be removed, so this also proves the handle
  // was released.
  std::error_code ec;
  std::filesystem::remove(log_path_, ec);
  EXPECT_FALSE(ec) << ec.message();
}

// A blocked category must not reach the sink even though the level passes.
TEST_F(LogManagerTest, BlockedCategoryIsNotWritten) {
  ConfigureToFile(LogLevel::YAZE_DEBUG, {"-Blocked"});

  LOG_ERROR("Blocked", "suppressed %d", 1);
  LOG_ERROR("Allowed", "emitted %d", 2);

  const std::string contents = ReadLog();
  EXPECT_EQ(contents.find("suppressed"), std::string::npos)
      << "log contents: " << contents;
  EXPECT_NE(contents.find("emitted 2"), std::string::npos)
      << "log contents: " << contents;
}

}  // namespace
}  // namespace util
}  // namespace yaze
