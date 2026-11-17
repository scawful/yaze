#include "cli/service/ai/cli_harness_service.h"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "cli/service/ai/ai_http_client.h"
#include "cli/service/ai/provider_ids.h"
#include "cli/service/ai/service_factory.h"
#include "cli/service/ai/subprocess_runner.h"
#include "nlohmann/json.hpp"

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

class ScopedPidCleanup {
 public:
  explicit ScopedPidCleanup(fs::path pid_file)
      : pid_file_(std::move(pid_file)) {}

  ~ScopedPidCleanup() { KillRecordedPid(); }

  pid_t ReadPid() const {
    std::ifstream in(pid_file_);
    std::string contents;
    std::getline(in, contents);
    pid_t pid = -1;
    int parsed = 0;
    if (absl::SimpleAtoi(absl::StripAsciiWhitespace(contents), &parsed) &&
        parsed > 1) {
      pid = static_cast<pid_t>(parsed);
    }
    return pid;
  }

  void KillRecordedPid() const {
    const pid_t pid = ReadPid();
    if (pid > 1) {
      ::kill(pid, SIGKILL);
      int status = 0;
      ::waitpid(pid, &status, WNOHANG);
    }
  }

 private:
  fs::path pid_file_;
};

class ScopedFdPair {
 public:
  ScopedFdPair() = default;
  ~ScopedFdPair() {
    for (int& fd : fds_) {
      if (fd >= 0) {
        ::close(fd);
        fd = -1;
      }
    }
  }

  int* data() { return fds_; }
  int read_fd() const { return fds_[0]; }
  int write_fd() const { return fds_[1]; }

 private:
  int fds_[2] = {-1, -1};
};
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

  // CliHarnessService (gemini-cli) end-to-end check with binary override.
  CliHarnessConfig cfg;
  cfg.provider = kProviderGeminiCli;
  cfg.model = nasty_model;
  cfg.binary_override = script.string();
  CliHarnessService service(cfg);
  auto resp_or = service.GenerateResponse(nasty_prompt);
  ASSERT_TRUE(resp_or.ok()) << resp_or.status().message();
  auto cli_argv = nlohmann::json::parse(resp_or->text_response);
  EXPECT_THAT(
      cli_argv.get<std::vector<std::string>>(),
      testing::ElementsAre("-p", nasty_prompt, "--output-format", "json",
                           "--approval-mode", "plan", "--model", nasty_model));

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

TEST(SubprocessRunnerTest,
     ReturnsPromptlyWhenGrandchildKeepsStdoutAndStderrOpen) {
  TempTestDir temp;
  const fs::path pid_file = temp.path() / "grandchild.pid";
  ScopedPidCleanup grandchild_cleanup(pid_file);

  const fs::path script = WriteExecutablePythonScript(
      temp.path(), "grandchild_holder",
      absl::StrCat(
          "import json, subprocess, sys\n"
          "proc = subprocess.Popen(\n"
          "    [sys.executable, '-c', 'import time; time.sleep(30)'],\n"
          "    stdout=sys.stdout, stderr=sys.stderr, start_new_session=True)\n"
          "with open(",
          nlohmann::json(pid_file.string()).dump(),
          ", 'w') as f:\n"
          "  f.write(str(proc.pid))\n"
          "print(json.dumps({'type':'result','subtype':'success','is_error':"
          "False,"
          "'result':'GRANDCHILD OK'}), flush=True)\n"));

  SubprocessOptions options;
  options.executable = script.string();
  options.timeout = std::chrono::seconds(30);
  options.post_exit_drain_grace = std::chrono::milliseconds(150);

  const auto start = std::chrono::steady_clock::now();
  auto direct_or = RunSubprocess(options);
  const auto elapsed = std::chrono::steady_clock::now() - start;
  grandchild_cleanup.KillRecordedPid();

  ASSERT_TRUE(direct_or.ok()) << direct_or.status().message();
  EXPECT_EQ(direct_or->exit_code, 0);
  EXPECT_FALSE(direct_or->timed_out);
  EXPECT_TRUE(direct_or->post_exit_eof_timed_out);
  EXPECT_THAT(direct_or->stdout_output, testing::HasSubstr("GRANDCHILD OK"));
  EXPECT_LT(elapsed, std::chrono::seconds(3));

  CliHarnessConfig cfg;
  cfg.provider = kProviderClaudeCode;
  cfg.model = "claude-sonnet-5-5";
  cfg.binary_override = script.string();
  cfg.timeout_seconds = 30;
  CliHarnessService service(cfg);

  const auto svc_start = std::chrono::steady_clock::now();
  auto resp_or = service.GenerateResponse("hello");
  const auto svc_elapsed = std::chrono::steady_clock::now() - svc_start;
  grandchild_cleanup.KillRecordedPid();

  ASSERT_TRUE(resp_or.ok()) << resp_or.status().message();
  EXPECT_EQ(resp_or->text_response, "GRANDCHILD OK");
  EXPECT_LT(svc_elapsed, std::chrono::seconds(3));
}

TEST(SubprocessRunnerTest, TimeoutKillsProcessGroupAndPreservesPartialOutput) {
  TempTestDir temp;
  const fs::path group_pid_file = temp.path() / "group_child.pid";
  ScopedPidCleanup group_cleanup(group_pid_file);

  const fs::path script = WriteExecutablePythonScript(
      temp.path(), "group_sleeper",
      absl::StrCat(
          "import subprocess, sys, time\n"
          "proc = subprocess.Popen(\n"
          "    [sys.executable, '-c', 'import time; time.sleep(30)'])\n"
          "with open(",
          nlohmann::json(group_pid_file.string()).dump(),
          ", 'w') as f:\n"
          "  f.write(str(proc.pid))\n"
          "print('partial-stdout-line', flush=True)\n"
          "sys.stderr.write('partial-stderr-line\\n')\n"
          "sys.stderr.flush()\n"
          "time.sleep(30)\n"));

  SubprocessResult partial;
  SubprocessOptions options;
  options.executable = script.string();
  options.timeout = std::chrono::milliseconds(350);
  options.partial_result = &partial;

  auto result_or = RunSubprocess(options);
  EXPECT_FALSE(result_or.ok());
  EXPECT_EQ(result_or.status().code(), absl::StatusCode::kDeadlineExceeded);
  EXPECT_THAT(std::string(result_or.status().message()),
              testing::HasSubstr("partial-stderr-line"));
  EXPECT_TRUE(partial.timed_out);
  EXPECT_THAT(partial.stdout_output, testing::HasSubstr("partial-stdout-line"));
  EXPECT_THAT(partial.stderr_output, testing::HasSubstr("partial-stderr-line"));

  const pid_t child_pid = group_cleanup.ReadPid();
  ASSERT_GT(child_pid, 1);
  bool process_gone = false;
  for (int i = 0; i < 20; ++i) {
    if (::kill(child_pid, 0) != 0 && errno == ESRCH) {
      process_gone = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
  }
  EXPECT_TRUE(process_gone)
      << "In-group child pid " << child_pid << " survived group timeout kill";
}

TEST(SubprocessRunnerTest, ConcurrentSpawnsDoNotInheritPipesOrLeakFds) {
  TempTestDir temp;

#ifdef __APPLE__
  ScopedFdPair leaked_pipe;
  ASSERT_EQ(::pipe(leaked_pipe.data()), 0);
  const fs::path fd_probe =
      WriteExecutablePythonScript(temp.path(), "fd_probe",
                                  "import os, sys\n"
                                  "results = []\n"
                                  "for arg in sys.argv[1:]:\n"
                                  "  try:\n"
                                  "    os.fstat(int(arg))\n"
                                  "    results.append('OPEN')\n"
                                  "  except OSError:\n"
                                  "    results.append('EBADF')\n"
                                  "print(','.join(results))\n");
  SubprocessOptions fd_opts;
  fd_opts.executable = fd_probe.string();
  fd_opts.args = {absl::StrCat(leaked_pipe.read_fd()),
                  absl::StrCat(leaked_pipe.write_fd())};
  auto fd_res = RunSubprocess(fd_opts);
  ASSERT_TRUE(fd_res.ok()) << fd_res.status().message();
  EXPECT_EQ(absl::StripAsciiWhitespace(fd_res->stdout_output), "EBADF,EBADF");
#endif

  const fs::path fast_script =
      WriteExecutablePythonScript(temp.path(), "fast_echo",
                                  "import sys\n"
                                  "print('fast-' + sys.argv[1])\n");
  const fs::path slow_script =
      WriteExecutablePythonScript(temp.path(), "slow_echo",
                                  "import sys, time\n"
                                  "time.sleep(1.2)\n"
                                  "print('slow-' + sys.argv[1])\n");

  constexpr int kSlowCount = 2;
  constexpr int kFastCount = 8;
  constexpr int kTotal = kSlowCount + kFastCount;

  std::mutex mu;
  std::condition_variable cv;
  int ready_count = 0;
  bool go = false;

  std::vector<absl::StatusOr<SubprocessResult>> results(kTotal);
  std::vector<std::chrono::milliseconds> elapsed_ms(kTotal);
  std::vector<std::thread> threads;
  threads.reserve(kTotal);

  for (int i = 0; i < kTotal; ++i) {
    threads.emplace_back([&, i]() {
      {
        std::unique_lock<std::mutex> lock(mu);
        ++ready_count;
        if (ready_count == kTotal) {
          go = true;
          cv.notify_all();
        } else {
          cv.wait(lock, [&]() { return go; });
        }
      }
      SubprocessOptions opts;
      opts.executable =
          (i < kSlowCount) ? slow_script.string() : fast_script.string();
      opts.args = {absl::StrCat(i)};
      opts.timeout = std::chrono::seconds(10);
      opts.post_exit_drain_grace = std::chrono::seconds(5);
      const auto t0 = std::chrono::steady_clock::now();
      results[i] = RunSubprocess(opts);
      elapsed_ms[i] = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - t0);
    });
  }
  for (auto& t : threads) {
    t.join();
  }

  for (int i = 0; i < kTotal; ++i) {
    ASSERT_TRUE(results[i].ok())
        << "worker " << i << ": " << results[i].status().message();
    EXPECT_EQ(results[i]->exit_code, 0);
    EXPECT_FALSE(results[i]->post_exit_eof_timed_out);
    if (i < kSlowCount) {
      EXPECT_EQ(absl::StripAsciiWhitespace(results[i]->stdout_output),
                absl::StrCat("slow-", i));
    } else {
      EXPECT_EQ(absl::StripAsciiWhitespace(results[i]->stdout_output),
                absl::StrCat("fast-", i));
      EXPECT_LT(elapsed_ms[i].count(), 1000)
          << "fast worker " << i << " stalled for " << elapsed_ms[i].count()
          << "ms (likely inherited a slow worker's pipe write end)";
    }
  }
}
#endif  // !_WIN32

TEST(CliHarnessServiceTest, BuildsReadOnlyArgvPerProfileWithoutSkipFlags) {
  auto assert_no_forbidden_flags = [](const std::vector<std::string>& args) {
    for (const std::string& arg : args) {
      EXPECT_THAT(arg, testing::Not(testing::HasSubstr("dangerously")));
      EXPECT_NE(arg, "danger-full-access");
      EXPECT_NE(arg, "--yolo");
      EXPECT_NE(arg, "-y");
      EXPECT_NE(arg, "--force");
      EXPECT_NE(arg, "bypassPermissions");
    }
  };

  auto claude_args = BuildCliHarnessArgv(kProviderClaudeCode, "Inspect room",
                                         "claude-sonnet-5-5", 120);
  ASSERT_TRUE(claude_args.ok());
  EXPECT_THAT(*claude_args,
              testing::ElementsAre(
                  "-p", "Inspect room", "--output-format", "json",
                  "--permission-mode", "plan", "--setting-sources", "user",
                  "--strict-mcp-config", "--model", "claude-sonnet-5-5"));
  assert_no_forbidden_flags(*claude_args);

  auto codex_args =
      BuildCliHarnessArgv(kProviderCodex, "Inspect room", "gpt-5.2", 120);
  ASSERT_TRUE(codex_args.ok());
  EXPECT_THAT(*codex_args,
              testing::ElementsAre("exec", "--json", "--sandbox", "read-only",
                                   "--skip-git-repo-check", "-m", "gpt-5.2",
                                   "--", "Inspect room"));
  assert_no_forbidden_flags(*codex_args);

  // Leading-dash prompts must not be misparsed as flags on any provider.
  for (const char* provider : {kProviderClaudeCode, kProviderCursorAgent,
                               kProviderAgy, kProviderGeminiCli}) {
    auto dash_args =
        BuildCliHarnessArgv(provider, "--help inspect room", "", 120);
    ASSERT_TRUE(dash_args.ok()) << provider;
    ASSERT_GE(dash_args->size(), 2u) << provider;
    EXPECT_EQ((*dash_args)[0], "-p");
    EXPECT_EQ((*dash_args)[1], " --help inspect room");
    EXPECT_NE((*dash_args)[1].front(), '-');
  }
  auto codex_dash_args =
      BuildCliHarnessArgv(kProviderCodex, "--help inspect room", "", 120);
  ASSERT_TRUE(codex_dash_args.ok());
  EXPECT_THAT(*codex_dash_args,
              testing::ElementsAre("exec", "--json", "--sandbox", "read-only",
                                   "--skip-git-repo-check", "--",
                                   "--help inspect room"));

  auto cursor_args =
      BuildCliHarnessArgv(kProviderCursorAgent, "Inspect room", "auto", 120);
  ASSERT_TRUE(cursor_args.ok());
  EXPECT_THAT(*cursor_args, testing::ElementsAre("-p", "Inspect room", "--mode",
                                                 "plan", "--output-format",
                                                 "json", "--model", "auto"));
  assert_no_forbidden_flags(*cursor_args);

  auto agy_args = BuildCliHarnessArgv(kProviderAgy, "Inspect room",
                                      "gemini-4-argon-low", 90);
  ASSERT_TRUE(agy_args.ok());
  EXPECT_THAT(*agy_args,
              testing::ElementsAre("-p", "Inspect room", "--mode", "plan",
                                   "--output-format", "json", "--print-timeout",
                                   "90s", "--model", "gemini-4-argon-low"));
  assert_no_forbidden_flags(*agy_args);

  auto gemini_args = BuildCliHarnessArgv(kProviderGeminiCli, "Inspect room",
                                         "gemini-2.5-flash", 120);
  ASSERT_TRUE(gemini_args.ok());
  EXPECT_THAT(*gemini_args,
              testing::ElementsAre("-p", "Inspect room", "--output-format",
                                   "json", "--approval-mode", "plan", "--model",
                                   "gemini-2.5-flash"));
  assert_no_forbidden_flags(*gemini_args);

  EXPECT_EQ(NormalizeCliHarnessProviderId("claude-cli"), kProviderClaudeCode);
  EXPECT_EQ(NormalizeCliHarnessProviderId("codex-cli"), kProviderCodex);
  EXPECT_EQ(NormalizeCliHarnessProviderId("cursor"), kProviderCursorAgent);
  EXPECT_EQ(NormalizeCliHarnessProviderId("antigravity"), kProviderAgy);
  EXPECT_EQ(NormalizeCliHarnessProviderId("local-gemini"), kProviderGeminiCli);
}

TEST(CliHarnessServiceTest, ParsesRecordedOutputsAcrossProfiles) {
  // 1. agy JSON envelope (captured structure from live agy -p --output-format json)
  const std::string agy_json = R"({
    "conversation_id": "conv-123",
    "status": "SUCCESS",
    "response": "PROBE OK\n",
    "duration_seconds": 5.91,
    "num_turns": 1,
    "usage": {"input_tokens": 120, "output_tokens": 4, "total_tokens": 124}
  })";
  auto agy_or =
      ParseCliHarnessOutput(kProviderAgy, "gemini-4-argon-low", agy_json);
  ASSERT_TRUE(agy_or.ok()) << agy_or.status().message();
  EXPECT_EQ(agy_or->text_response, "PROBE OK\n");
  EXPECT_DOUBLE_EQ(agy_or->latency_seconds, 5.91);
  EXPECT_EQ(agy_or->parameters.at("input_tokens"), "120");
  EXPECT_EQ(agy_or->parameters.at("output_tokens"), "4");

  // 2. claude-code JSON output
  const std::string claude_json = R"({
    "type": "result",
    "subtype": "success",
    "is_error": false,
    "duration_ms": 1450,
    "result": "PROBE OK",
    "usage": {"input_tokens": 10, "output_tokens": 2}
  })";
  auto claude_or = ParseCliHarnessOutput(kProviderClaudeCode,
                                         "claude-sonnet-5-5", claude_json);
  ASSERT_TRUE(claude_or.ok()) << claude_or.status().message();
  EXPECT_EQ(claude_or->text_response, "PROBE OK");
  EXPECT_NEAR(claude_or->latency_seconds, 1.45, 0.01);
  EXPECT_EQ(claude_or->parameters.at("output_tokens"), "2");

  // 3. codex JSONL output
  const std::string codex_jsonl =
      "{\"type\":\"thread.started\",\"thread_id\":\"t1\"}\n"
      "{\"type\":\"item.completed\",\"item\":{\"id\":\"i1\",\"type\":"
      "\"reasoning\",\"text\":\"thinking\"}}\n"
      "{\"type\":\"item.completed\",\"item\":{\"id\":\"i2\",\"type\":\"agent_"
      "message\",\"text\":\"PROBE OK\"}}\n"
      "{\"type\":\"turn.completed\",\"usage\":{\"input_tokens\":55,\"cached_"
      "input_tokens\":10,\"output_tokens\":3}}\n";
  auto codex_or = ParseCliHarnessOutput(kProviderCodex, "gpt-5.2", codex_jsonl);
  ASSERT_TRUE(codex_or.ok()) << codex_or.status().message();
  EXPECT_EQ(codex_or->text_response, "PROBE OK");
  EXPECT_EQ(codex_or->parameters.at("input_tokens"), "55");
  EXPECT_EQ(codex_or->parameters.at("cached_input_tokens"), "10");
  EXPECT_EQ(codex_or->parameters.at("output_tokens"), "3");

  // 4. cursor-agent + gemini-cli with preamble & inner commands
  const std::string gemini_stdout =
      "Loaded extensions...\n"
      "{\"response\":\"```json\\n{\\\"text_response\\\":\\\"Inspecting "
      "room\\\",\\\"commands\\\":[\\\"z3ed dungeon list\\\"]}\\n```\"}";
  auto gemini_or = ParseCliHarnessOutput(kProviderGeminiCli, "gemini-2.5-flash",
                                         gemini_stdout);
  ASSERT_TRUE(gemini_or.ok()) << gemini_or.status().message();
  EXPECT_EQ(gemini_or->text_response, "Inspecting room");
  EXPECT_THAT(gemini_or->commands, testing::ElementsAre("dungeon list"));

  const std::string cursor_json =
      "{\"type\":\"result\",\"result\":\"PROBE OK\"}";
  auto cursor_or =
      ParseCliHarnessOutput(kProviderCursorAgent, "auto", cursor_json);
  ASSERT_TRUE(cursor_or.ok()) << cursor_or.status().message();
  EXPECT_EQ(cursor_or->text_response, "PROBE OK");
}

TEST(CliHarnessServiceTest, FallsBackToRawTextAndDetectsEnvelopeErrors) {
  auto raw_or = ParseCliHarnessOutput(kProviderAgy, "gemini-4-argon-low",
                                      "  plain response line\n");
  ASSERT_TRUE(raw_or.ok()) << raw_or.status().message();
  EXPECT_EQ(raw_or->text_response, "plain response line");
  EXPECT_EQ(raw_or->parameters.at("raw_output"), "true");

  auto claude_err = ParseCliHarnessOutput(
      kProviderClaudeCode, "claude-sonnet-5-5",
      R"({"is_error":true,"result":"Authentication required"})");
  EXPECT_FALSE(claude_err.ok());
  EXPECT_EQ(claude_err.status().code(), absl::StatusCode::kInternal);

  auto codex_err = ParseCliHarnessOutput(
      kProviderCodex, "gpt-5.2",
      "{\"type\":\"error\",\"message\":\"Sandbox denied\"}\n");
  EXPECT_FALSE(codex_err.ok());
  EXPECT_EQ(codex_err.status().code(), absl::StatusCode::kInternal);
}

TEST(CliHarnessServiceTest, DisablesHttpRedirectFollowWhenAuthHeadersPresent) {
  EXPECT_TRUE(ShouldFollowAiHttpRedirects({}));
  EXPECT_TRUE(
      ShouldFollowAiHttpRedirects({{"Content-Type", "application/json"}}));
  EXPECT_TRUE(ShouldFollowAiHttpRedirects({{"Authorization", ""}}));

  EXPECT_FALSE(ShouldFollowAiHttpRedirects({{"x-api-key", "secret"}}));
  EXPECT_FALSE(ShouldFollowAiHttpRedirects({{"X-Api-Key", "secret"}}));
  EXPECT_FALSE(ShouldFollowAiHttpRedirects({{"x-goog-api-key", "secret"}}));
  EXPECT_FALSE(
      ShouldFollowAiHttpRedirects({{"Authorization", "Bearer token"}}));
  EXPECT_FALSE(
      ShouldFollowAiHttpRedirects({{"Proxy-Authorization", "Basic abc"}}));
}

TEST(CliHarnessServiceTest, ParsesAgyAndCursorModelListingsCleanly) {
  const std::string agy_out =
      "Fetching available models...\n"
      "gemini-4-argon-low\tGemini 4 Argon (Low)\n"
      "gemini-4-carbon-medium\tGemini 4 Carbon (Medium)\n"
      "claude-sonnet-5-5-medium\tClaude Sonnet 5.5 (Medium)\n";
  auto agy_models = ParseAgyModelsOutput(agy_out);
  ASSERT_EQ(agy_models.size(), 3u);
  EXPECT_EQ(agy_models[0].name, "gemini-4-argon-low");
  EXPECT_EQ(agy_models[0].display_name, "Gemini 4 Argon (Low)");
  EXPECT_EQ(agy_models[0].provider, kProviderAgy);

  // Include UTF-8 zero-width spaces (\xE2\x80\x8B) and ANSI CSI codes as seen
  // in real cursor-agent --list-models output.
  const std::string cursor_out =
      "\x1b[1mAvailable models\x1b[0m\n\n"
      "auto - Auto (default)\n"
      "composer-2.5 - Composer 2.5 (current)\n"
      "grok-4.7-low-fast - Grok 4.7  Low Fast\xE2\x80\x8B\xE2\x80\x8B\n"
      "\nTip: Use --model <id>\n";
  auto cursor_models = ParseCursorAgentModelsOutput(cursor_out);
  ASSERT_EQ(cursor_models.size(), 3u);
  EXPECT_EQ(cursor_models[0].name, "auto");
  EXPECT_EQ(cursor_models[1].name, "composer-2.5");
  EXPECT_EQ(cursor_models[2].name, "grok-4.7-low-fast");
  EXPECT_EQ(cursor_models[2].display_name, "Grok 4.7  Low Fast");
  EXPECT_EQ(cursor_models[2].provider, kProviderCursorAgent);
}

#ifndef _WIN32
TEST(CliHarnessServiceTest,
     ExecutesFakeHarnessWithWorkingDirectoryAndOverride) {
  TempTestDir temp;
  const fs::path project_dir = temp.path() / "my_project";
  fs::create_directories(project_dir);

  const fs::path script = WriteExecutablePythonScript(
      temp.path(), "fake-codex",
      "import json, os, sys\n"
      "print(json.dumps({'type':'item.completed','item':{'id':'1','type':'"
      "agent_message','text':os.getcwd()}}))\n"
      "print(json.dumps({'type':'turn.completed','usage':{'input_tokens':3,'"
      "output_tokens':2}}))\n");

  CliHarnessConfig cfg;
  cfg.provider = kProviderCodex;
  cfg.model = "gpt-5.2";
  cfg.binary_override = script.string();
  cfg.working_directory = project_dir.string();
  CliHarnessService service(cfg);

  ASSERT_TRUE(service.CheckAvailability().ok());
  auto resp_or = service.GenerateResponse("hello");
  ASSERT_TRUE(resp_or.ok()) << resp_or.status().message();
  EXPECT_EQ(fs::canonical(absl::StripAsciiWhitespace(resp_or->text_response)),
            fs::canonical(project_dir));
  EXPECT_EQ(resp_or->parameters.at("input_tokens"), "3");
}
#endif  // !_WIN32

#ifdef YAZE_AI_RUNTIME_AVAILABLE
TEST(CliHarnessServiceTest, OptionalLiveProbeRoundTrip) {
  if (std::getenv("YAZE_RUN_LIVE_AI_PROBES") == nullptr) {
    GTEST_SKIP()
        << "Set YAZE_RUN_LIVE_AI_PROBES=1 to run live agy/local probes";
  }

  AIServiceConfig agy_cfg;
  agy_cfg.provider = kProviderAgy;
  agy_cfg.model = "gemini-4-argon-low";
  agy_cfg.cli_timeout_seconds = 45;
  auto agy_service_or = CreateAIServiceStrict(agy_cfg);
  ASSERT_TRUE(agy_service_or.ok()) << agy_service_or.status().message();
  auto agy_resp = (*agy_service_or)->GenerateResponse("Reply PROBE OK");
  ASSERT_TRUE(agy_resp.ok()) << agy_resp.status().message();
  EXPECT_FALSE(agy_resp->text_response.empty());
  std::cout << "[LIVE_PROBE] agy (" << agy_cfg.model
            << ") latency=" << agy_resp->latency_seconds
            << "s text=" << absl::StripAsciiWhitespace(agy_resp->text_response)
            << std::endl;

  AIServiceConfig lms_cfg;
  lms_cfg.provider = kProviderLmStudio;
  auto lms_service_or = CreateAIServiceStrict(lms_cfg);
  ASSERT_TRUE(lms_service_or.ok()) << lms_service_or.status().message();
  auto lms_models_or = (*lms_service_or)->ListAvailableModels();
  if (lms_models_or.ok() && !lms_models_or->empty()) {
    std::string chat_model;
    for (const auto& m : *lms_models_or) {
      if (m.name.find("embedding") == std::string::npos) {
        chat_model = m.name;
        break;
      }
    }
    if (!chat_model.empty()) {
      lms_cfg.model = chat_model;
      auto lms_chat_or = CreateAIServiceStrict(lms_cfg);
      ASSERT_TRUE(lms_chat_or.ok()) << lms_chat_or.status().message();
      auto lms_resp = (*lms_chat_or)->GenerateResponse("Reply PROBE OK");
      ASSERT_TRUE(lms_resp.ok()) << lms_resp.status().message();
      EXPECT_FALSE(lms_resp->text_response.empty());
      std::cout << "[LIVE_PROBE] lmstudio (" << chat_model
                << ") latency=" << lms_resp->latency_seconds << "s text="
                << absl::StripAsciiWhitespace(lms_resp->text_response)
                << std::endl;
    }
  }
}
#endif

}  // namespace
}  // namespace yaze::cli
