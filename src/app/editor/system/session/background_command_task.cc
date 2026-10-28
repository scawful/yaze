#include "app/editor/system/session/background_command_task.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <string>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/str_split.h"

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <TargetConditionals.h>
#if !TARGET_OS_IPHONE
#include <crt_externs.h>
#endif
#else
extern char** environ;
#endif
#endif

namespace yaze::editor {
namespace {

constexpr size_t kTailLineLimit = 4;
constexpr size_t kFailureDetailLimit = 512;
constexpr int kPollIntervalMs = 50;
constexpr auto kCancelEscalationDelay = std::chrono::seconds(2);

std::vector<std::string> ChildEnvironment(
    const std::map<std::string, std::string>& overrides) {
  std::vector<std::string> environment;
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__) && \
    !(defined(__APPLE__) && TARGET_OS_IPHONE)
#if defined(__APPLE__)
  char** inherited = *_NSGetEnviron();
#else
  char** inherited = environ;
#endif
  for (char** entry = inherited; entry && *entry; ++entry) {
    const std::string value(*entry);
    const auto separator = value.find('=');
    if (!overrides.contains(value.substr(0, separator))) {
      environment.push_back(value);
    }
  }
#endif
  for (const auto& [name, value] : overrides) {
    environment.push_back(name + "=" + value);
  }
  return environment;
}

bool SupportsNativeBackgroundCommands() {
#if defined(_WIN32) || defined(__EMSCRIPTEN__) || \
    (defined(__APPLE__) && TARGET_OS_IPHONE)
  return false;
#else
  return true;
#endif
}

}  // namespace

BackgroundCommandTask::~BackgroundCommandTask() {
  Cancel();
  JoinIfNeeded();
}

absl::Status BackgroundCommandTask::Start(
    const std::string& command, const std::string& directory,
    const std::map<std::string, std::string>& environment) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (started_ && !finished_) {
    return absl::FailedPreconditionError("Task is already running");
  }

  if (!SupportsNativeBackgroundCommands()) {
    return absl::UnimplementedError(
        "Background commands are not supported on this platform");
  }

  for (const auto& [name, value] : environment) {
    if (name.empty() || name.find('=') != std::string::npos ||
        name.find('\0') != std::string::npos ||
        value.find('\0') != std::string::npos) {
      return absl::InvalidArgumentError("Invalid child environment override");
    }
  }
  auto child_environment = ChildEnvironment(environment);

  JoinIfNeeded();
  started_ = true;
  running_ = true;
  finished_ = false;
  cancel_requested_ = false;
  exit_code_ = -1;
  child_pid_ = -1;
  command_ = command;
  directory_ = directory;
  output_.clear();
  output_tail_.clear();
  status_ = absl::UnknownError("Task is running");
  worker_ = std::thread(&BackgroundCommandTask::WorkerMain, this, command,
                        directory, std::move(child_environment));
  return absl::OkStatus();
}

void BackgroundCommandTask::Cancel() {
  int pid = -1;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    cancel_requested_ = true;
    pid = child_pid_;
  }

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__) && \
    !(defined(__APPLE__) && TARGET_OS_IPHONE)
  if (pid > 0) {
    kill(-pid, SIGTERM);
    kill(pid, SIGTERM);
  }
#endif
}

BackgroundCommandTask::Snapshot BackgroundCommandTask::GetSnapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return {.started = started_,
          .running = running_,
          .finished = finished_,
          .cancel_requested = cancel_requested_,
          .can_cancel = running_,
          .exit_code = exit_code_,
          .command = command_,
          .directory = directory_,
          .output = output_,
          .output_tail = output_tail_,
          .status = status_};
}

absl::Status BackgroundCommandTask::Wait() {
  JoinIfNeeded();
  std::lock_guard<std::mutex> lock(mutex_);
  return status_;
}

void BackgroundCommandTask::WorkerMain(std::string command,
                                       std::string directory,
                                       std::vector<std::string> environment) {
#if defined(_WIN32) || defined(__EMSCRIPTEN__) || \
    (defined(__APPLE__) && TARGET_OS_IPHONE)
  Finalize(absl::UnimplementedError(
               "Background commands are not supported on this platform"),
           -1);
  return;
#else
  // All storage and pointers are owned before fork; the child only performs
  // process setup and exec, without mutating the parent's environment.
  std::vector<char*> environment_pointers;
  environment_pointers.reserve(environment.size() + 1);
  for (auto& entry : environment) {
    environment_pointers.push_back(entry.data());
  }
  environment_pointers.push_back(nullptr);

  int pipe_fds[2];
  if (pipe(pipe_fds) != 0) {
    Finalize(absl::InternalError("Failed to create process pipe"), -1);
    return;
  }

  const pid_t pid = fork();
  if (pid < 0) {
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    Finalize(absl::InternalError("Failed to fork background process"), -1);
    return;
  }

  if (pid == 0) {
    setpgid(0, 0);
    dup2(pipe_fds[1], STDOUT_FILENO);
    dup2(pipe_fds[1], STDERR_FILENO);
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    if (!directory.empty()) {
      if (chdir(directory.c_str()) != 0) {
        constexpr char kPrefix[] = "Cannot enter command working directory: ";
        constexpr char kNewline[] = "\n";
        (void)write(STDERR_FILENO, kPrefix, sizeof(kPrefix) - 1);
        (void)write(STDERR_FILENO, directory.data(), directory.size());
        (void)write(STDERR_FILENO, kNewline, sizeof(kNewline) - 1);
        _exit(127);
      }
    }
    execle("/bin/sh", "sh", "-lc", command.c_str(), static_cast<char*>(nullptr),
           environment_pointers.data());
    _exit(127);
  }

  setpgid(pid, pid);
  close(pipe_fds[1]);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    child_pid_ = pid;
  }

  std::array<char, 512> buffer{};
  bool sent_sigterm = false;
  bool sent_sigkill = false;
  auto cancel_requested_at = std::chrono::steady_clock::time_point{};
  bool pipe_closed = false;
  int wait_status = 0;
  bool child_reaped = false;

  while (!pipe_closed || !child_reaped) {
    struct pollfd pfd{pipe_fds[0], POLLIN | POLLHUP, 0};
    const int poll_result = poll(&pfd, 1, kPollIntervalMs);
    if (poll_result > 0 && (pfd.revents & (POLLIN | POLLHUP))) {
      const ssize_t bytes_read =
          read(pipe_fds[0], buffer.data(), buffer.size());
      if (bytes_read > 0) {
        AppendOutput(buffer.data(), static_cast<size_t>(bytes_read));
      } else if (bytes_read == 0) {
        pipe_closed = true;
      } else if (errno != EINTR) {
        pipe_closed = true;
      }
    }

    const bool cancel_requested = [this]() {
      std::lock_guard<std::mutex> lock(mutex_);
      return cancel_requested_;
    }();
    if (cancel_requested) {
      if (cancel_requested_at == std::chrono::steady_clock::time_point{}) {
        cancel_requested_at = std::chrono::steady_clock::now();
      }
      if (!sent_sigterm) {
        kill(-pid, SIGTERM);
        kill(pid, SIGTERM);
        sent_sigterm = true;
      } else if (!sent_sigkill &&
                 std::chrono::steady_clock::now() - cancel_requested_at >=
                     kCancelEscalationDelay) {
        kill(-pid, SIGKILL);
        kill(pid, SIGKILL);
        sent_sigkill = true;
      }
    }

    const pid_t wait_result = waitpid(pid, &wait_status, WNOHANG);
    if (wait_result == pid) {
      child_reaped = true;
    }
  }

  close(pipe_fds[0]);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    child_pid_ = -1;
  }

  const bool cancel_requested = [this]() {
    std::lock_guard<std::mutex> lock(mutex_);
    return cancel_requested_;
  }();
  if (cancel_requested) {
    Finalize(absl::CancelledError("Background command cancelled"), -1);
    return;
  }

  if (!WIFEXITED(wait_status) || WEXITSTATUS(wait_status) != 0) {
    const int exit_code =
        WIFEXITED(wait_status) ? WEXITSTATUS(wait_status) : -1;
    std::string message = "Background command failed";
    if (WIFEXITED(wait_status)) {
      message += " (exit " + std::to_string(exit_code) + ")";
    } else if (WIFSIGNALED(wait_status)) {
      message += " (signal " + std::to_string(WTERMSIG(wait_status)) + ")";
    }
    std::string detail;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      detail = output_tail_;
    }
    const auto last_newline = detail.rfind('\n');
    if (last_newline != std::string::npos) {
      detail.erase(0, last_newline + 1);
    }
    if (detail.size() > kFailureDetailLimit) {
      size_t end = kFailureDetailLimit;
      // Do not cut a UTF-8 diagnostic in the middle of a code point.
      while (end > 0 &&
             (static_cast<unsigned char>(detail[end]) & 0xC0) == 0x80) {
        --end;
      }
      detail.resize(end);
      detail += "...";
    }
    if (!detail.empty()) {
      message += ": " + detail;
    }
    Finalize(absl::InternalError(message), exit_code);
    return;
  }

  Finalize(absl::OkStatus(), WEXITSTATUS(wait_status));
#endif
}

void BackgroundCommandTask::AppendOutput(const char* data, size_t size) {
  std::lock_guard<std::mutex> lock(mutex_);
  output_.append(data, size);
  output_tail_ = ComputeOutputTail(output_);
}

void BackgroundCommandTask::Finalize(absl::Status status, int exit_code) {
  std::lock_guard<std::mutex> lock(mutex_);
  running_ = false;
  finished_ = true;
  exit_code_ = exit_code;
  status_ = std::move(status);
  output_tail_ = ComputeOutputTail(output_);
}

void BackgroundCommandTask::JoinIfNeeded() {
  if (worker_.joinable()) {
    worker_.join();
  }
}

std::string BackgroundCommandTask::ComputeOutputTail(
    const std::string& output) {
  if (output.empty()) {
    return "";
  }

  std::vector<std::string> lines;
  for (absl::string_view line_view : absl::StrSplit(output, '\n')) {
    std::string line(line_view);
    absl::StripAsciiWhitespace(&line);
    if (!line.empty()) {
      lines.push_back(std::move(line));
    }
  }
  if (lines.empty()) {
    return "";
  }
  const size_t start =
      lines.size() > kTailLineLimit ? lines.size() - kTailLineLimit : 0;
  std::string tail;
  for (size_t i = start; i < lines.size(); ++i) {
    if (!tail.empty()) {
      tail += "\n";
    }
    tail += lines[i];
  }
  return tail;
}

}  // namespace yaze::editor
