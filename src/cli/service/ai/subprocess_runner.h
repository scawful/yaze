#ifndef YAZE_SRC_CLI_SERVICE_AI_SUBPROCESS_RUNNER_H_
#define YAZE_SRC_CLI_SERVICE_AI_SUBPROCESS_RUNNER_H_

#include <atomic>
#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace yaze::cli {

struct SubprocessResult {
  int exit_code = -1;
  std::string stdout_output;
  std::string stderr_output;
  double duration_seconds = 0.0;
  bool timed_out = false;
  bool cancelled = false;
  bool post_exit_eof_timed_out = false;
  bool output_truncated = false;
};

struct SubprocessOptions {
  std::string executable;
  std::vector<std::string> args;
  std::string working_directory;
  std::string stdin_data;
  std::chrono::milliseconds timeout = std::chrono::seconds(300);
  // Maximum grace period to continue draining stdout/stderr after the direct
  // child exits if a descendant keeps the pipe write ends open.
  std::chrono::milliseconds post_exit_drain_grace =
      std::chrono::milliseconds(300);
  std::atomic<bool>* cancel_requested = nullptr;
  size_t max_output_bytes = 8 * 1024 * 1024;
  // Optional out-pointer populated with captured stdout/stderr/timing even when
  // RunSubprocess returns DeadlineExceededError or CancelledError.
  SubprocessResult* partial_result = nullptr;
};

// Expands a leading "~/" or exact "~" using HOME (or USERPROFILE on Windows).
std::string ExpandLeadingTilde(absl::string_view path);

// Returns PATH directories followed by standard CLI installation directories
// that GUI launches on macOS/Unix may not inherit in PATH.
std::vector<std::string> GetStandardCliSearchDirectories();

// Resolves a CLI executable cleanly without invoking a shell.
// When override_path is non-empty, only override_path is checked (no fallback to
// PATH if an explicit override is invalid).
absl::StatusOr<std::string> ResolveCliBinary(
    absl::string_view binary_name, absl::string_view override_path = "");

// Quotes one argument per Windows CommandLineToArgvW rules.
std::string QuoteWindowsCommandLineArg(absl::string_view arg);

// Builds a Windows CreateProcessW command line from executable and argv[1..n].
std::string BuildWindowsCommandLine(absl::string_view executable,
                                    const std::vector<std::string>& args);

// Spawns options.executable directly with argv (posix_spawnp on POSIX,
// CreateProcessW on Windows), never invoking a shell.
absl::StatusOr<SubprocessResult> RunSubprocess(
    const SubprocessOptions& options);

}  // namespace yaze::cli

#endif  // YAZE_SRC_CLI_SERVICE_AI_SUBPROCESS_RUNNER_H_
