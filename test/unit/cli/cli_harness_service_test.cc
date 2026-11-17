#include "cli/service/ai/subprocess_runner.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "nlohmann/json.hpp"

#ifdef YAZE_AI_RUNTIME_AVAILABLE
#include "cli/service/ai/local_gemini_cli_service.h"
#endif

namespace yaze::cli {
namespace {

namespace fs = std::filesystem;

class TempTestDir {
 public:
  TempTestDir() {
    static std::atomic<int> counter{0};
    path_ = fs::temp_directory_path() /
            absl::StrCat(
                "yaze_subprocess_test_", counter.fetch_add(1), "_",
                std::chrono::steady_clock::now().time_since_epoch().count());
    fs::create_directories(path_);
  }

  ~TempTestDir() {
    std::error_code ec;
    fs::remove_all(path_, ec);
  }

  const fs::path& path() const { return path_; }

 private:
  fs::path path_;
};

class ScopedEnvVar {
 public:
  ScopedEnvVar(const char* name, const std::string& value) : name_(name) {
    if (const char* prev = std::getenv(name)) {
      had_prev_ = true;
      prev_value_ = prev;
    }
#ifdef _WIN32
    _putenv_s(name_.c_str(), value.c_str());
#else
    setenv(name_.c_str(), value.c_str(), 1);
#endif
  }

  ~ScopedEnvVar() {
#ifdef _WIN32
    _putenv_s(name_.c_str(), had_prev_ ? prev_value_.c_str() : "");
#else
    if (had_prev_) {
      setenv(name_.c_str(), prev_value_.c_str(), 1);
    } else {
      unsetenv(name_.c_str());
    }
#endif
  }

 private:
  std::string name_;
  bool had_prev_ = false;
  std::string prev_value_;
};

#ifndef _WIN32
fs::path WriteExecutablePythonScript(const fs::path& dir,
                                     const std::string& filename,
                                     const std::string& body) {
  const fs::path script_path = dir / filename;
  std::ofstream out(script_path);
  out << "#!/usr/bin/env python3\n" << body;
  out.close();
  fs::permissions(script_path,
                  fs::perms::owner_read | fs::perms::owner_write |
                      fs::perms::owner_exec | fs::perms::group_read |
                      fs::perms::group_exec,
                  fs::perm_options::replace);
  return script_path;
}
#endif

TEST(SubprocessRunnerTest, EmptyExecutableReturnsInvalidArgument) {
  SubprocessOptions options;
  auto result = RunSubprocess(options);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST(SubprocessRunnerTest, PreCancelledReturnsCancelledBeforeSpawn) {
  std::atomic<bool> cancel{true};
  SubprocessOptions options;
  options.executable = "python3";
  options.args = {"--version"};
  options.cancel_requested = &cancel;

  auto result = RunSubprocess(options);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kCancelled);
}

TEST(SubprocessRunnerTest, MissingOverrideDoesNotFallbackToPath) {
  TempTestDir temp;
#ifndef _WIN32
  WriteExecutablePythonScript(temp.path(), "fake-cli",
                              "import sys\nprint('ok')\n");
  const char* orig_path = std::getenv("PATH");
  ScopedEnvVar path_guard("PATH", absl::StrCat(temp.path().string(), ":",
                                               orig_path ? orig_path : ""));

  auto ok_from_path = ResolveCliBinary("fake-cli");
  ASSERT_TRUE(ok_from_path.ok()) << ok_from_path.status().message();

  const fs::path nonexistent_override = temp.path() / "missing-override";
  auto err_from_override =
      ResolveCliBinary("fake-cli", nonexistent_override.string());
  EXPECT_FALSE(err_from_override.ok());
  EXPECT_EQ(err_from_override.status().code(), absl::StatusCode::kNotFound);

  const fs::path valid_override = temp.path() / "fake-cli";
  auto ok_override = ResolveCliBinary("fake-cli", valid_override.string());
  ASSERT_TRUE(ok_override.ok()) << ok_override.status().message();
  EXPECT_EQ(fs::path(*ok_override), valid_override);
#else
  const fs::path nonexistent_override = temp.path() / "missing-override.exe";
  auto err_from_override =
      ResolveCliBinary("cmd", nonexistent_override.string());
  EXPECT_FALSE(err_from_override.ok());
  EXPECT_EQ(err_from_override.status().code(), absl::StatusCode::kNotFound);
#endif
}

TEST(SubprocessRunnerTest, QuotesWindowsCommandLineArgsWithoutShellLoss) {
  EXPECT_EQ(QuoteWindowsCommandLineArg("plain"), "plain");
  EXPECT_EQ(QuoteWindowsCommandLineArg(""), "\"\"");
  EXPECT_EQ(QuoteWindowsCommandLineArg("two words"), "\"two words\"");
  EXPECT_EQ(QuoteWindowsCommandLineArg("a\"b"), "\"a\\\"b\"");
  EXPECT_EQ(QuoteWindowsCommandLineArg("C:\\path with\\"),
            "\"C:\\path with\\\\\"");
  EXPECT_EQ(BuildWindowsCommandLine("tool.exe", {"-p", "hello 'world'"}),
            "tool.exe -p \"hello 'world'\"");
}

#ifndef _WIN32
TEST(SubprocessRunnerTest, PassesShellMetacharactersVerbatimWithoutShell) {
  TempTestDir temp;
  const fs::path sentinel_prompt = temp.path() / "sentinel_prompt";
  const fs::path sentinel_backtick = temp.path() / "sentinel_backtick";
  const fs::path sentinel_semi = temp.path() / "sentinel_semi";
  const fs::path sentinel_model = temp.path() / "sentinel_model";

  const fs::path script = WriteExecutablePythonScript(
      temp.path(), "gemini",
      "import json, sys\n"
      "print(json.dumps({'response': json.dumps(sys.argv[1:])}))\n");

  const std::string nasty_prompt = absl::StrCat(
      "line1 'single' \"double\" $(touch ", sentinel_prompt.string(),
      ") `touch ", sentinel_backtick.string(), "`; touch ",
      sentinel_semi.string(), " | cat $HOME\nline2");
  const std::string nasty_model =
      absl::StrCat("gemini-2.5-flash';touch ", sentinel_model.string(), "#");

  // Direct RunSubprocess check.
  SubprocessOptions options;
  options.executable = script.string();
  options.args = {"-p", nasty_prompt, "--model", nasty_model};
  auto direct_or = RunSubprocess(options);
  ASSERT_TRUE(direct_or.ok()) << direct_or.status().message();
  EXPECT_EQ(direct_or->exit_code, 0);

  auto envelope = nlohmann::json::parse(direct_or->stdout_output);
  auto direct_argv =
      nlohmann::json::parse(envelope.at("response").get<std::string>());
  ASSERT_EQ(direct_argv.size(), 4u);
  EXPECT_EQ(direct_argv[0], "-p");
  EXPECT_EQ(direct_argv[1], nasty_prompt);
  EXPECT_EQ(direct_argv[2], "--model");
  EXPECT_EQ(direct_argv[3], nasty_model);

#ifdef YAZE_AI_RUNTIME_AVAILABLE
  // LocalGeminiCliService end-to-end check (PATH prepended with fake gemini).
  const char* orig_path = std::getenv("PATH");
  ScopedEnvVar path_guard("PATH", absl::StrCat(temp.path().string(), ":",
                                               orig_path ? orig_path : ""));
  LocalGeminiCliService service(nasty_model);
  auto resp_or = service.GenerateResponse(nasty_prompt);
  ASSERT_TRUE(resp_or.ok()) << resp_or.status().message();
  auto cli_argv = nlohmann::json::parse(resp_or->text_response);
  EXPECT_THAT(
      cli_argv.get<std::vector<std::string>>(),
      testing::ElementsAre("-p", nasty_prompt, "--output-format", "json",
                           "--approval-mode", "plan", "--model", nasty_model));
#endif

  EXPECT_FALSE(fs::exists(sentinel_prompt));
  EXPECT_FALSE(fs::exists(sentinel_backtick));
  EXPECT_FALSE(fs::exists(sentinel_semi));
  EXPECT_FALSE(fs::exists(sentinel_model));
}

TEST(SubprocessRunnerTest, WorkingDirectoryAndNonZeroExitArePreserved) {
  TempTestDir temp;
  const fs::path work_subdir = temp.path() / "project_root";
  fs::create_directories(work_subdir);

  const fs::path script =
      WriteExecutablePythonScript(temp.path(), "check_cwd",
                                  "import os, sys\n"
                                  "print(os.getcwd())\n"
                                  "sys.stderr.write('err-line\\n')\n"
                                  "sys.exit(7)\n");

  SubprocessOptions options;
  options.executable = script.string();
  options.working_directory = work_subdir.string();
  auto result_or = RunSubprocess(options);
  ASSERT_TRUE(result_or.ok()) << result_or.status().message();
  EXPECT_EQ(result_or->exit_code, 7);
  EXPECT_EQ(fs::canonical(absl::StripAsciiWhitespace(result_or->stdout_output)),
            fs::canonical(work_subdir));
  EXPECT_THAT(result_or->stderr_output, testing::HasSubstr("err-line"));
}

TEST(SubprocessRunnerTest, EnforcesTimeoutAndReturnsDeadlineExceeded) {
  TempTestDir temp;
  const fs::path script = WriteExecutablePythonScript(temp.path(), "sleeper",
                                                      "import time\n"
                                                      "time.sleep(10)\n");

  SubprocessOptions options;
  options.executable = script.string();
  options.timeout = std::chrono::milliseconds(150);

  const auto start = std::chrono::steady_clock::now();
  auto result_or = RunSubprocess(options);
  const auto elapsed = std::chrono::steady_clock::now() - start;

  EXPECT_FALSE(result_or.ok());
  EXPECT_EQ(result_or.status().code(), absl::StatusCode::kDeadlineExceeded);
  EXPECT_LT(elapsed, std::chrono::seconds(3));
}

TEST(SubprocessRunnerTest, CancelsRunningChildWhenFlagFlipped) {
  TempTestDir temp;
  const fs::path script = WriteExecutablePythonScript(temp.path(), "waiter",
                                                      "import time\n"
                                                      "time.sleep(10)\n");

  std::atomic<bool> cancel{false};
  std::thread canceller([&cancel]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    cancel.store(true, std::memory_order_relaxed);
  });

  SubprocessOptions options;
  options.executable = script.string();
  options.timeout = std::chrono::seconds(10);
  options.cancel_requested = &cancel;

  const auto start = std::chrono::steady_clock::now();
  auto result_or = RunSubprocess(options);
  const auto elapsed = std::chrono::steady_clock::now() - start;
  canceller.join();

  EXPECT_FALSE(result_or.ok());
  EXPECT_EQ(result_or.status().code(), absl::StatusCode::kCancelled);
  EXPECT_LT(elapsed, std::chrono::seconds(3));
}
#endif  // !_WIN32

}  // namespace
}  // namespace yaze::cli
