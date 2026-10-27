#include "app/editor/system/session/background_command_task.h"

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <thread>

#include "absl/strings/str_cat.h"

// BackgroundCommandTask::Start() returns UnimplementedError on Windows,
// Emscripten, and iOS (see background_command_task.cc:SupportsNative*).
// Gate the whole suite on POSIX-desktop targets so CI on those platforms
// does not spuriously fail.
#if defined(_WIN32) || defined(__EMSCRIPTEN__) || \
    (defined(__APPLE__) && TARGET_OS_IPHONE)
#define YAZE_BACKGROUND_COMMANDS_SUPPORTED 0
#else
#define YAZE_BACKGROUND_COMMANDS_SUPPORTED 1
#endif

#if YAZE_BACKGROUND_COMMANDS_SUPPORTED

namespace yaze::editor {
namespace {

std::optional<std::string> GetEnvironmentValue(const char* name) {
  const char* value = std::getenv(name);
  if (value == nullptr) {
    return std::nullopt;
  }
  return std::string(value);
}

class ScopedEnvVar {
 public:
  ScopedEnvVar(const char* name, const std::string& value)
      : name_(name), original_value_(GetEnvironmentValue(name)) {
    EXPECT_EQ(setenv(name_.c_str(), value.c_str(), 1), 0);
  }

  ~ScopedEnvVar() {
    if (original_value_.has_value()) {
      EXPECT_EQ(setenv(name_.c_str(), original_value_->c_str(), 1), 0);
    } else {
      EXPECT_EQ(unsetenv(name_.c_str()), 0);
    }
  }

  ScopedEnvVar(const ScopedEnvVar&) = delete;
  ScopedEnvVar& operator=(const ScopedEnvVar&) = delete;

 private:
  std::string name_;
  std::optional<std::string> original_value_;
};

constexpr char kInheritedVariable[] = "YAZE_BG_ENV_INHERITED";

TEST(BackgroundCommandTaskTest, StreamsOutputWhileRunning) {
  BackgroundCommandTask task;
  ASSERT_TRUE(
      task.Start("printf 'first\\n'; sleep 0.2; printf 'second\\n'", ".").ok());

  bool saw_first_line = false;
  for (int i = 0; i < 20; ++i) {
    const auto snapshot = task.GetSnapshot();
    if (snapshot.output.find("first") != std::string::npos) {
      saw_first_line = true;
      EXPECT_TRUE(snapshot.running || snapshot.finished);
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  EXPECT_TRUE(saw_first_line);
  EXPECT_TRUE(task.Wait().ok());
  const auto final_snapshot = task.GetSnapshot();
  EXPECT_TRUE(final_snapshot.finished);
  EXPECT_FALSE(final_snapshot.running);
  EXPECT_NE(final_snapshot.output.find("second"), std::string::npos);
}

TEST(BackgroundCommandTaskTest,
     ChildEnvironmentOverridesInheritedValueWithoutChangingParent) {
  const std::string parent_value = "parent inherited value";
  ScopedEnvVar inherited(kInheritedVariable, parent_value);
  ASSERT_EQ(GetEnvironmentValue(kInheritedVariable), parent_value);
  const std::string child_value = "child override value";

  BackgroundCommandTask task;
  ASSERT_TRUE(task.Start("printf '<yaze-bg-override>%s</yaze-bg-override>' "
                         "\"$YAZE_BG_ENV_INHERITED\"",
                         ".", {{kInheritedVariable, child_value}})
                  .ok());
  EXPECT_EQ(GetEnvironmentValue(kInheritedVariable), parent_value);
  ASSERT_TRUE(task.Wait().ok());
  EXPECT_NE(task.GetSnapshot().output.find("<yaze-bg-override>" + child_value +
                                           "</yaze-bg-override>"),
            std::string::npos);
  EXPECT_EQ(GetEnvironmentValue(kInheritedVariable), parent_value);
}

TEST(BackgroundCommandTaskTest,
     EmptyChildEnvironmentValueClearsInheritedValue) {
  const std::string parent_value = "parent inherited value";
  ScopedEnvVar inherited(kInheritedVariable, parent_value);
  ASSERT_EQ(GetEnvironmentValue(kInheritedVariable), parent_value);

  BackgroundCommandTask task;
  ASSERT_TRUE(task.Start("printf '<yaze-bg-empty>%s:%s</yaze-bg-empty>' "
                         "\"${YAZE_BG_ENV_INHERITED+x}\" "
                         "\"$YAZE_BG_ENV_INHERITED\"",
                         ".", {{kInheritedVariable, ""}})
                  .ok());
  EXPECT_EQ(GetEnvironmentValue(kInheritedVariable), parent_value);
  ASSERT_TRUE(task.Wait().ok());
  EXPECT_NE(task.GetSnapshot().output.find("<yaze-bg-empty>x:</yaze-bg-empty>"),
            std::string::npos);
  EXPECT_EQ(GetEnvironmentValue(kInheritedVariable), parent_value);
}

TEST(BackgroundCommandTaskTest,
     ChildEnvironmentValuePreservesShellMetacharacters) {
  constexpr char kVariable[] = "YAZE_BG_ENV_TEST_VALUE";
  const auto parent_value = GetEnvironmentValue(kVariable);
  const std::string child_value =
      "spaces 'single quotes' \"double quotes\"; $(printf expanded) "
      "`printf expanded` \\backslash\nsecond line";

  BackgroundCommandTask task;
  ASSERT_TRUE(task.Start("printf '<yaze-bg-literal>%s</yaze-bg-literal>' "
                         "\"$YAZE_BG_ENV_TEST_VALUE\"",
                         ".", {{kVariable, child_value}})
                  .ok());
  EXPECT_EQ(GetEnvironmentValue(kVariable), parent_value);
  ASSERT_TRUE(task.Wait().ok());
  EXPECT_NE(task.GetSnapshot().output.find("<yaze-bg-literal>" + child_value +
                                           "</yaze-bg-literal>"),
            std::string::npos);
  EXPECT_EQ(GetEnvironmentValue(kVariable), parent_value);
}

TEST(BackgroundCommandTaskTest,
     ChildEnvironmentPreservesUnrelatedInheritedValue) {
  const std::string inherited_value = "parent inherited value";
  ScopedEnvVar inherited(kInheritedVariable, inherited_value);
  ASSERT_EQ(GetEnvironmentValue(kInheritedVariable), inherited_value);
  constexpr char kVariable[] = "YAZE_BG_ENV_TEST_VALUE";
  const auto parent_value = GetEnvironmentValue(kVariable);

  BackgroundCommandTask task;
  ASSERT_TRUE(task.Start("printf '<yaze-bg-inherited>%s</yaze-bg-inherited>' "
                         "\"$YAZE_BG_ENV_INHERITED\"",
                         ".", {{kVariable, "child-only"}})
                  .ok());
  EXPECT_EQ(GetEnvironmentValue(kInheritedVariable), inherited_value);
  EXPECT_EQ(GetEnvironmentValue(kVariable), parent_value);
  ASSERT_TRUE(task.Wait().ok());
  EXPECT_NE(
      task.GetSnapshot().output.find("<yaze-bg-inherited>" + inherited_value +
                                     "</yaze-bg-inherited>"),
      std::string::npos);
  EXPECT_EQ(GetEnvironmentValue(kInheritedVariable), inherited_value);
  EXPECT_EQ(GetEnvironmentValue(kVariable), parent_value);
}

TEST(BackgroundCommandTaskTest, InvalidEnvironmentNamesDoNotStartTask) {
  const std::array<std::string, 3> invalid_names = {
      "", "BAD=NAME", std::string("BAD\0NAME", 8)};
  for (size_t i = 0; i < invalid_names.size(); ++i) {
    SCOPED_TRACE(i);
    BackgroundCommandTask task;
    ASSERT_FALSE(task.GetSnapshot().started);
    const auto status =
        task.Start("printf 'unexpected'", ".", {{invalid_names[i], "value"}});
    EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
    const auto snapshot = task.GetSnapshot();
    EXPECT_FALSE(snapshot.started);
    EXPECT_FALSE(snapshot.running);
    EXPECT_FALSE(snapshot.finished);
    EXPECT_TRUE(snapshot.command.empty());
    EXPECT_TRUE(snapshot.output.empty());
  }
}

TEST(BackgroundCommandTaskTest, EnvironmentValueWithNulDoesNotStartTask) {
  BackgroundCommandTask task;
  ASSERT_FALSE(task.GetSnapshot().started);
  const auto status =
      task.Start("printf 'unexpected'", ".",
                 {{"YAZE_BG_ENV_TEST_VALUE", std::string("bad\0value", 9)}});
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  const auto snapshot = task.GetSnapshot();
  EXPECT_FALSE(snapshot.started);
  EXPECT_FALSE(snapshot.running);
  EXPECT_FALSE(snapshot.finished);
  EXPECT_TRUE(snapshot.command.empty());
  EXPECT_TRUE(snapshot.output.empty());
}

TEST(BackgroundCommandTaskTest, MissingWorkingDirectoryDoesNotRunCommand) {
  const auto nonce =
      std::chrono::steady_clock::now().time_since_epoch().count();
  const auto directory = std::filesystem::temp_directory_path() /
                         ("yaze_bg_missing_cwd_" + std::to_string(nonce));
  const auto marker = directory.parent_path() /
                      ("yaze_bg_unexpected_marker_" + std::to_string(nonce));
  ASSERT_FALSE(std::filesystem::exists(directory));
  ASSERT_FALSE(std::filesystem::exists(marker));

  BackgroundCommandTask task;
  ASSERT_TRUE(task.Start("printf 'unexpected' > '" + marker.string() + "'",
                         directory.string())
                  .ok());
  const auto status = task.Wait();
  EXPECT_FALSE(status.ok());
  EXPECT_NE(task.GetSnapshot().output.find("Cannot enter command working "
                                           "directory"),
            std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(marker));
}

TEST(BackgroundCommandTaskTest, FailureStatusIncludesExitAndFinalDiagnostic) {
  BackgroundCommandTask task;
  ASSERT_TRUE(
      task.Start("printf 'Build setup\\n'; "
                 "printf 'Expanded map writer is unavailable.\\n\\n' >&2; "
                 "exit 2",
                 ".")
          .ok());

  const auto status = task.Wait();
  EXPECT_EQ(status.code(), absl::StatusCode::kInternal);
  EXPECT_EQ(status.message(),
            "Background command failed (exit 2): "
            "Expanded map writer is unavailable.");
  const auto snapshot = task.GetSnapshot();
  EXPECT_EQ(snapshot.exit_code, 2);
  EXPECT_EQ(snapshot.status, status);
  EXPECT_NE(snapshot.output.find("Build setup"), std::string::npos);
}

TEST(BackgroundCommandTaskTest, FailureWithoutOutputStillIncludesExitCode) {
  BackgroundCommandTask task;
  ASSERT_TRUE(task.Start("exec >/dev/null 2>&1; exit 7", ".").ok());
  const auto status = task.Wait();
  EXPECT_EQ(status.code(), absl::StatusCode::kInternal);
  EXPECT_NE(status.message().find("Background command failed (exit 7)"),
            absl::string_view::npos);
  EXPECT_EQ(task.GetSnapshot().exit_code, 7);
}

TEST(BackgroundCommandTaskTest,
     FailureDiagnosticIsBoundedWithoutLosingFullLog) {
  BackgroundCommandTask task;
  ASSERT_TRUE(task.Start("printf 'reason: '; "
                         "i=0; while [ $i -lt 600 ]; do printf x; "
                         "i=$((i + 1)); done; printf '\\n'; exit 2",
                         ".")
                  .ok());
  const auto status = task.Wait();
  const std::string prefix = "Background command failed (exit 2): ";
  EXPECT_EQ(status.message().substr(0, prefix.size()), prefix);
  EXPECT_EQ(status.message().size(), prefix.size() + 512 + 3);
  EXPECT_EQ(status.message().substr(status.message().size() - 3), "...");
  EXPECT_NE(task.GetSnapshot().output.find(std::string(600, 'x')),
            std::string::npos);
}

TEST(BackgroundCommandTaskTest, FailureDiagnosticPreservesUtf8Boundary) {
  BackgroundCommandTask task;
  const std::string detail = std::string(511, 'x') + "é";
  ASSERT_TRUE(task.Start("printf '%s\\n' '" + detail + "'; exit 2", ".").ok());
  const auto status = task.Wait();
  EXPECT_EQ(status.message(), "Background command failed (exit 2): " +
                                  std::string(511, 'x') + "...");
  EXPECT_NE(task.GetSnapshot().output.find(detail), std::string::npos);
}

TEST(BackgroundCommandTaskTest, SignalFailureReportsSignal) {
  BackgroundCommandTask task;
  ASSERT_TRUE(task.Start("kill -TERM $$", ".").ok());
  const auto status = task.Wait();
  EXPECT_EQ(status.code(), absl::StatusCode::kInternal);
  EXPECT_NE(status.message().find("Background command failed (signal 15)"),
            absl::string_view::npos);
  EXPECT_EQ(task.GetSnapshot().exit_code, -1);
}

// The command runs through a login shell ("sh -lc"), so the host's startup
// files run first and may print text. Plain words such as "start" and "done"
// can appear in that text, which would make this test cancel before the command
// runs or fail although the command was stopped. The markers below cannot come
// from startup noise, and a command that kept running after cancel would still
// print the second one.
constexpr char kStartMarker[] = "yaze-bg-cancel-start";
constexpr char kAfterSleepMarker[] = "yaze-bg-cancel-after-sleep";

TEST(BackgroundCommandTaskTest, CancelStopsRunningCommand) {
  BackgroundCommandTask task;
  ASSERT_TRUE(task.Start(absl::StrCat("printf '", kStartMarker,
                                      "\\n'; sleep 5; printf '",
                                      kAfterSleepMarker, "\\n'"),
                         ".")
                  .ok());

  bool saw_start = false;
  for (int i = 0; i < 20; ++i) {
    const auto snapshot = task.GetSnapshot();
    if (snapshot.output.find(kStartMarker) != std::string::npos) {
      saw_start = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  ASSERT_TRUE(saw_start);

  task.Cancel();
  const auto status = task.Wait();
  EXPECT_EQ(status.code(), absl::StatusCode::kCancelled);

  const auto snapshot = task.GetSnapshot();
  EXPECT_TRUE(snapshot.finished);
  EXPECT_FALSE(snapshot.running);
  EXPECT_TRUE(snapshot.cancel_requested);
  EXPECT_EQ(snapshot.output.find(kAfterSleepMarker), std::string::npos)
      << "the command kept running after cancel; output:\n"
      << snapshot.output;
}

}  // namespace
}  // namespace yaze::editor

#else  // !YAZE_BACKGROUND_COMMANDS_SUPPORTED

#include <absl/status/status.h>
#include "app/editor/system/session/background_command_task.h"

namespace yaze::editor {
namespace {

// Sanity check: on unsupported platforms Start() must refuse cleanly so
// higher-level code can fall back. Skipping the full streaming suite.
TEST(BackgroundCommandTaskTest, UnsupportedPlatformReturnsUnimplemented) {
  BackgroundCommandTask task;
  auto status = task.Start("true", ".");
  EXPECT_EQ(status.code(), absl::StatusCode::kUnimplemented);
}

}  // namespace
}  // namespace yaze::editor

#endif  // YAZE_BACKGROUND_COMMANDS_SUPPORTED
