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

// This suite uses POSIX shell syntax. Windows has a native suite below;
// Emscripten and iOS retain the unsupported-platform contract.
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

#elif defined(_WIN32)

#include <fstream>
#include <iterator>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "absl/strings/escaping.h"

namespace yaze::editor {
namespace {

class ScopedWindowsTestHandle {
 public:
  explicit ScopedWindowsTestHandle(HANDLE handle) : handle_(handle) {}
  ~ScopedWindowsTestHandle() {
    if (handle_ && handle_ != INVALID_HANDLE_VALUE)
      CloseHandle(handle_);
  }
  HANDLE get() const { return handle_; }
  ScopedWindowsTestHandle(const ScopedWindowsTestHandle&) = delete;
  ScopedWindowsTestHandle& operator=(const ScopedWindowsTestHandle&) = delete;

 private:
  HANDLE handle_;
};

class ScopedWindowsTestDirectory {
 public:
  ScopedWindowsTestDirectory() {
    const auto nonce =
        std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            ("yaze_windows_command_" + std::to_string(nonce));
    std::filesystem::create_directories(path_);
  }
  ~ScopedWindowsTestDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
  }
  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

std::optional<std::wstring> WindowsEnvironmentValue(const std::wstring& name) {
  SetLastError(ERROR_SUCCESS);
  const DWORD required = GetEnvironmentVariableW(name.c_str(), nullptr, 0);
  if (required == 0) {
    if (GetLastError() == ERROR_ENVVAR_NOT_FOUND)
      return std::nullopt;
    return std::wstring();
  }
  std::wstring value(required, L'\0');
  const DWORD copied =
      GetEnvironmentVariableW(name.c_str(), value.data(), required);
  value.resize(copied);
  return value;
}

class ScopedWindowsEnvironment {
 public:
  ScopedWindowsEnvironment(std::wstring name, const std::wstring& value)
      : name_(std::move(name)), before_(WindowsEnvironmentValue(name_)) {
    EXPECT_TRUE(SetEnvironmentVariableW(name_.c_str(), value.c_str()));
  }
  ~ScopedWindowsEnvironment() {
    EXPECT_TRUE(SetEnvironmentVariableW(name_.c_str(),
                                        before_ ? before_->c_str() : nullptr));
  }

 private:
  std::wstring name_;
  std::optional<std::wstring> before_;
};

std::string WindowsPathUtf8(const std::filesystem::path& path) {
  const auto text = path.u8string();
  return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

std::wstring WindowsTestWide(const std::string& text) {
  const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                       text.data(), text.size(), nullptr, 0);
  EXPECT_GT(size, 0);
  std::wstring wide(size, L'\0');
  EXPECT_EQ(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                text.size(), wide.data(), size),
            size);
  return wide;
}

std::string EncodedPowerShell(const std::string& script) {
  const auto wide = WindowsTestWide(script);
  std::string bytes;
  for (wchar_t character : wide) {
    bytes.push_back(static_cast<char>(character & 255));
    bytes.push_back(static_cast<char>((character >> 8) & 255));
  }
  return absl::Base64Escape(bytes);
}

std::filesystem::path WindowsPowerShellPath() {
  std::array<wchar_t, MAX_PATH> directory{};
  const DWORD size = GetSystemDirectoryW(directory.data(), directory.size());
  EXPECT_GT(size, 0u);
  EXPECT_LT(size, directory.size());
  return std::filesystem::path(directory.data()) / "WindowsPowerShell" /
         "v1.0" / "powershell.exe";
}

std::string PowerShellCommand(const std::string& script) {
  return "\"" + WindowsPathUtf8(WindowsPowerShellPath()) +
         "\" -NoLogo -NoProfile -NonInteractive -EncodedCommand " +
         EncodedPowerShell(
             "[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false); " +
             script);
}

bool WaitForWindowsOutput(BackgroundCommandTask& task,
                          const std::string& marker) {
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (std::chrono::steady_clock::now() < deadline) {
    if (task.GetSnapshot().output.find(marker) != std::string::npos)
      return true;
    if (task.GetSnapshot().finished)
      return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
  }
  return false;
}

std::string QuotePowerShell(const std::string& text) {
  std::string escaped = "'";
  for (char character : text) {
    escaped += character;
    if (character == '\'')
      escaped += '\'';
  }
  return escaped + "'";
}

std::string WindowsTreeCommand(
    const std::filesystem::path& pid_file,
    const std::filesystem::path& release_parent = {}) {
  const std::string child_arguments =
      "-NoLogo -NoProfile -NonInteractive -EncodedCommand " +
      EncodedPowerShell("Start-Sleep -Seconds 30");
  return PowerShellCommand(
      "$start = [Diagnostics.ProcessStartInfo]::new(); "
      "$start.FileName = " +
      QuotePowerShell(WindowsPathUtf8(WindowsPowerShellPath())) +
      "; $start.Arguments = " + QuotePowerShell(child_arguments) +
      "; $start.UseShellExecute = $false; $start.CreateNoWindow = $true; "
      "$child = [Diagnostics.Process]::Start($start); "
      "[IO.File]::WriteAllText(" +
      QuotePowerShell(WindowsPathUtf8(pid_file)) +
      ", [string]$child.Id); [Console]::WriteLine('yaze-win-tree-ready'); " +
      (release_parent.empty()
           ? "Start-Sleep -Seconds 30"
           : "while (-not [IO.File]::Exists(" +
                 QuotePowerShell(WindowsPathUtf8(release_parent)) +
                 ")) { Start-Sleep -Milliseconds 25 }; exit 0"));
}

DWORD ReadWindowsChildPid(const std::filesystem::path& path) {
  std::ifstream input(path);
  DWORD pid = 0;
  input >> pid;
  EXPECT_TRUE(input.good() || input.eof());
  EXPECT_NE(pid, 0u);
  return pid;
}

TEST(BackgroundCommandTaskTest, WindowsStreamsStdoutAndStderrBeforeCompletion) {
  ScopedWindowsTestDirectory root;
  const auto release = root.path() / "release-stream";
  BackgroundCommandTask task;
  ASSERT_TRUE(task.Start(PowerShellCommand(
                             "[Console]::WriteLine('yaze-win-first'); "
                             "[Console]::Error.WriteLine('yaze-win-stderr'); "
                             "while (-not [IO.File]::Exists(" +
                             QuotePowerShell(WindowsPathUtf8(release)) +
                             ")) { Start-Sleep -Milliseconds 25 }; "
                             "[Console]::WriteLine('yaze-win-last')"),
                         ".")
                  .ok());
  ASSERT_TRUE(WaitForWindowsOutput(task, "yaze-win-first"));
  EXPECT_TRUE(task.GetSnapshot().running);
  {
    std::ofstream release_file(release, std::ios::binary);
    ASSERT_TRUE(release_file.is_open());
  }
  ASSERT_TRUE(task.Wait().ok()) << task.GetSnapshot().status;
  const auto snapshot = task.GetSnapshot();
  EXPECT_NE(snapshot.output.find("yaze-win-stderr"), std::string::npos);
  EXPECT_NE(snapshot.output.find("yaze-win-last"), std::string::npos);
  EXPECT_EQ(snapshot.exit_code, 0);
  EXPECT_TRUE(snapshot.finished);
  EXPECT_FALSE(snapshot.running);
}

TEST(BackgroundCommandTaskTest, WindowsKeepsChildEnvironmentIsolated) {
  ScopedWindowsEnvironment inherited(L"YAZE_BG_WIN_INHERITED",
                                     L"parent inherited");
  ScopedWindowsEnvironment replaced(L"YAZE_BG_WIN_OVERRIDE",
                                    L"parent override");
  ScopedWindowsEnvironment emptied(L"YAZE_BG_WIN_EMPTY", L"parent nonempty");
  const std::string literal =
      "é 中文 'quotes' \"double\"; $(not-evaluated) `literal` & ! %PATH%";
  BackgroundCommandTask task;
  ASSERT_TRUE(task.Start(PowerShellCommand("[Console]::WriteLine('inherited=' "
                                           "+ $env:YAZE_BG_WIN_INHERITED); "
                                           "[Console]::WriteLine('override=' + "
                                           "$env:YAZE_BG_WIN_OVERRIDE); "
                                           "[Console]::WriteLine('empty=<' + "
                                           "$env:YAZE_BG_WIN_EMPTY + '>'); "
                                           "[Console]::WriteLine('literal=' + "
                                           "$env:YAZE_BG_WIN_LITERAL)"),
                         ".",
                         {{"yaze_bg_win_override", "child override"},
                          {"YAZE_BG_WIN_EMPTY", ""},
                          {"YAZE_BG_WIN_LITERAL", literal}})
                  .ok());
  ASSERT_TRUE(task.Wait().ok()) << task.GetSnapshot().status;
  const auto output = task.GetSnapshot().output;
  EXPECT_NE(output.find("inherited=parent inherited"), std::string::npos);
  EXPECT_NE(output.find("override=child override"), std::string::npos);
  EXPECT_NE(output.find("empty=<>"), std::string::npos);
  EXPECT_NE(output.find("literal=" + literal), std::string::npos);
  EXPECT_EQ(WindowsEnvironmentValue(L"YAZE_BG_WIN_INHERITED"),
            L"parent inherited");
  EXPECT_EQ(WindowsEnvironmentValue(L"YAZE_BG_WIN_OVERRIDE"),
            L"parent override");
  EXPECT_EQ(WindowsEnvironmentValue(L"YAZE_BG_WIN_EMPTY"), L"parent nonempty");
}

TEST(BackgroundCommandTaskTest,
     WindowsRejectsInvalidAndDuplicateEnvironmentNames) {
  const std::vector<std::map<std::string, std::string>> invalid = {
      {{"", "bad"}},
      {{"BAD=NAME", "bad"}},
      {{std::string("BAD\0NAME", 8), "bad"}},
      {{"YAZE_BG_WIN_VALUE", std::string("bad\0value", 9)}},
      {{"YAZE_BG_WIN_NAME", "one"}, {"yaze_bg_win_name", "two"}}};
  for (const auto& environment : invalid) {
    BackgroundCommandTask task;
    const auto status = task.Start("echo must-not-run", ".", environment);
    EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
    EXPECT_FALSE(task.GetSnapshot().started);
    EXPECT_TRUE(task.GetSnapshot().output.empty());
  }
}

TEST(BackgroundCommandTaskTest, WindowsUsesUnicodeWorkingDirectory) {
  ScopedWindowsTestDirectory root;
  const auto directory =
      root.path() / std::filesystem::path(u8"cwd with spaces é 中文");
  std::filesystem::create_directories(directory);
  BackgroundCommandTask task;
  ASSERT_TRUE(
      task.Start("echo yaze-cwd>marker.txt", WindowsPathUtf8(directory)).ok());
  ASSERT_TRUE(task.Wait().ok()) << task.GetSnapshot().status;
  EXPECT_TRUE(std::filesystem::is_regular_file(directory / "marker.txt"));
  EXPECT_FALSE(std::filesystem::exists(root.path() / "marker.txt"));
}

TEST(BackgroundCommandTaskTest,
     WindowsMissingWorkingDirectoryDoesNotRunCommand) {
  ScopedWindowsTestDirectory root;
  const auto missing = root.path() / "missing";
  const auto marker = root.path() / "marker.txt";
  BackgroundCommandTask task;
  ASSERT_TRUE(task.Start("echo unexpected>\"" + WindowsPathUtf8(marker) + "\"",
                         WindowsPathUtf8(missing))
                  .ok());
  const auto status = task.Wait();
  EXPECT_FALSE(status.ok());
  EXPECT_NE(status.message().find("Cannot enter command working directory"),
            absl::string_view::npos);
  EXPECT_FALSE(std::filesystem::exists(marker));
  EXPECT_FALSE(std::filesystem::exists(missing));
}

TEST(BackgroundCommandTaskTest, WindowsFailurePreservesExitAndDiagnostic) {
  BackgroundCommandTask task;
  ASSERT_TRUE(task.Start("echo Build setup & echo Expanded map writer is "
                         "unavailable. 1>&2 & exit /b 2",
                         ".")
                  .ok());
  const auto status = task.Wait();
  EXPECT_EQ(status.code(), absl::StatusCode::kInternal);
  EXPECT_EQ(status.message(),
            "Background command failed (exit 2): Expanded "
            "map writer is unavailable.");
  EXPECT_EQ(task.GetSnapshot().exit_code, 2);
  EXPECT_NE(task.GetSnapshot().output.find("Build setup"), std::string::npos);
  BackgroundCommandTask silent;
  ASSERT_TRUE(silent.Start("exit /b 7", ".").ok());
  EXPECT_EQ(silent.Wait().message(), "Background command failed (exit 7)");
  EXPECT_EQ(silent.GetSnapshot().exit_code, 7);
}

TEST(BackgroundCommandTaskTest, WindowsCancelTerminatesOwnedChildTree) {
  ScopedWindowsTestDirectory root;
  const auto pid_file = root.path() / "child.pid";
  BackgroundCommandTask task;
  ASSERT_TRUE(task.Start(WindowsTreeCommand(pid_file), ".").ok());
  ASSERT_TRUE(WaitForWindowsOutput(task, "yaze-win-tree-ready"));
  ScopedWindowsTestHandle child(
      OpenProcess(SYNCHRONIZE, FALSE, ReadWindowsChildPid(pid_file)));
  ASSERT_NE(child.get(), nullptr);
  EXPECT_EQ(WaitForSingleObject(child.get(), 0), WAIT_TIMEOUT);
  task.Cancel();
  EXPECT_EQ(task.Wait().code(), absl::StatusCode::kCancelled);
  EXPECT_EQ(WaitForSingleObject(child.get(), 5000), WAIT_OBJECT_0);
  EXPECT_TRUE(task.GetSnapshot().finished);
  EXPECT_FALSE(task.GetSnapshot().running);
}

TEST(BackgroundCommandTaskTest, WindowsShellCompletionLeavesNoOwnedChild) {
  ScopedWindowsTestDirectory root;
  const auto pid_file = root.path() / "child.pid";
  const auto release_parent = root.path() / "release-parent";
  BackgroundCommandTask task;
  ASSERT_TRUE(
      task.Start(WindowsTreeCommand(pid_file, release_parent), ".").ok());
  ASSERT_TRUE(WaitForWindowsOutput(task, "yaze-win-tree-ready"));
  ScopedWindowsTestHandle child(
      OpenProcess(SYNCHRONIZE, FALSE, ReadWindowsChildPid(pid_file)));
  ASSERT_NE(child.get(), nullptr);
  EXPECT_EQ(WaitForSingleObject(child.get(), 0), WAIT_TIMEOUT);
  {
    std::ofstream release_file(release_parent, std::ios::binary);
    ASSERT_TRUE(release_file.is_open());
  }
  ASSERT_TRUE(task.Wait().ok()) << task.GetSnapshot().status;
  EXPECT_EQ(WaitForSingleObject(child.get(), 5000), WAIT_OBJECT_0);
}

TEST(BackgroundCommandTaskTest, WindowsDestructorCancelsOwnedChildTree) {
  ScopedWindowsTestDirectory root;
  const auto pid_file = root.path() / "child.pid";
  HANDLE child_handle = nullptr;
  {
    BackgroundCommandTask task;
    ASSERT_TRUE(task.Start(WindowsTreeCommand(pid_file), ".").ok());
    ASSERT_TRUE(WaitForWindowsOutput(task, "yaze-win-tree-ready"));
    child_handle =
        OpenProcess(SYNCHRONIZE, FALSE, ReadWindowsChildPid(pid_file));
    ASSERT_NE(child_handle, nullptr);
  }
  ScopedWindowsTestHandle child(child_handle);
  EXPECT_EQ(WaitForSingleObject(child.get(), 5000), WAIT_OBJECT_0);
}

TEST(BackgroundCommandTaskTest, WindowsImmediateCancelAndReuseResetTaskState) {
  BackgroundCommandTask task;
  ASSERT_TRUE(
      task.Start(PowerShellCommand("Start-Sleep -Seconds 30"), ".").ok());
  task.Cancel();
  EXPECT_EQ(task.Wait().code(), absl::StatusCode::kCancelled);
  ASSERT_TRUE(task.Start("echo yaze-win-reused", ".").ok());
  ASSERT_TRUE(task.Wait().ok()) << task.GetSnapshot().status;
  EXPECT_FALSE(task.GetSnapshot().cancel_requested);
  EXPECT_EQ(task.GetSnapshot().exit_code, 0);
  EXPECT_NE(task.GetSnapshot().output.find("yaze-win-reused"),
            std::string::npos);
}

}  // namespace
}  // namespace yaze::editor

#else  // !YAZE_BACKGROUND_COMMANDS_SUPPORTED && !defined(_WIN32)

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
