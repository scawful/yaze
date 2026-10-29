#include "app/editor/system/session/background_command_task.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cwchar>
#include <limits>
#include <string>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif !defined(__EMSCRIPTEN__)
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

#ifdef _WIN32
absl::Status WindowsError(const std::string& action, DWORD error) {
  return absl::InternalError(
      absl::StrCat(action, " (Windows error ", error, ")"));
}

absl::StatusOr<std::wstring> ToWide(const std::string& text) {
  if (text.empty())
    return std::wstring();
  if (text.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return absl::InvalidArgumentError("Background command text is too large");
  }
  const int text_size = static_cast<int>(text.size());
  const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                         text.data(), text_size, nullptr, 0);
  if (length == 0)
    return WindowsError("Cannot decode UTF-8 command text", GetLastError());
  std::wstring result(length, L'\0');
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), text_size,
                          result.data(), length) == 0) {
    return WindowsError("Cannot decode UTF-8 command text", GetLastError());
  }
  return result;
}

absl::StatusOr<std::string> ToUtf8(const std::wstring& text) {
  if (text.empty())
    return std::string();
  if (text.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return absl::InvalidArgumentError(
        "Background environment text is too large");
  }
  const int text_size = static_cast<int>(text.size());
  const int length =
      WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), text_size,
                          nullptr, 0, nullptr, nullptr);
  if (length == 0)
    return WindowsError("Cannot encode inherited environment", GetLastError());
  std::string result(length, '\0');
  if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), text_size,
                          result.data(), length, nullptr, nullptr) == 0) {
    return WindowsError("Cannot encode inherited environment", GetLastError());
  }
  return result;
}

struct EnvironmentNameLess {
  bool operator()(const std::wstring& left, const std::wstring& right) const {
    return CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) ==
           CSTR_LESS_THAN;
  }
};

class ScopedWindowsHandle {
 public:
  explicit ScopedWindowsHandle(HANDLE handle = nullptr) : handle_(handle) {}
  ~ScopedWindowsHandle() { Reset(); }
  ScopedWindowsHandle(const ScopedWindowsHandle&) = delete;
  ScopedWindowsHandle& operator=(const ScopedWindowsHandle&) = delete;
  HANDLE get() const { return handle_; }
  bool valid() const { return handle_ && handle_ != INVALID_HANDLE_VALUE; }
  void Reset(HANDLE handle = nullptr) {
    if (valid())
      CloseHandle(handle_);
    handle_ = handle;
  }

 private:
  HANDLE handle_;
};
#endif

absl::StatusOr<std::vector<std::string>> ChildEnvironment(
    const std::map<std::string, std::string>& overrides) {
  std::vector<std::string> environment;
#ifdef _WIN32
  std::map<std::wstring, std::wstring, EnvironmentNameLess> values;
  const auto block = GetEnvironmentStringsW();
  if (!block)
    return WindowsError("Cannot read inherited environment", GetLastError());
  absl::Cleanup free_environment = [block] {
    FreeEnvironmentStringsW(block);
  };
  for (const wchar_t* entry = block; *entry; entry += std::wcslen(entry) + 1) {
    const std::wstring text(entry);
    const auto separator = text.find(L'=', text.front() == L'=' ? 1 : 0);
    if (separator != std::wstring::npos) {
      values[text.substr(0, separator)] = text.substr(separator + 1);
    }
  }
  std::map<std::wstring, bool, EnvironmentNameLess> seen;
  for (const auto& [name, value] : overrides) {
    auto wide_name = ToWide(name);
    if (!wide_name.ok())
      return wide_name.status();
    auto wide_value = ToWide(value);
    if (!wide_value.ok())
      return wide_value.status();
    if (!seen.emplace(*wide_name, true).second) {
      return absl::InvalidArgumentError(
          "Windows child environment overrides contain duplicate names");
    }
    values[*wide_name] = *wide_value;
  }
  for (const auto& [name, value] : values) {
    auto entry = ToUtf8(name + L"=" + value);
    if (!entry.ok())
      return entry.status();
    environment.push_back(std::move(*entry));
  }
#elif !defined(__EMSCRIPTEN__) && !(defined(__APPLE__) && TARGET_OS_IPHONE)
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
  for (const auto& [name, value] : overrides) {
    environment.push_back(name + "=" + value);
  }
#endif
  return environment;
}

bool SupportsNativeBackgroundCommands() {
#if defined(__EMSCRIPTEN__) || (defined(__APPLE__) && TARGET_OS_IPHONE)
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
#ifdef _WIN32
  if (command.find('\0') != std::string::npos ||
      directory.find('\0') != std::string::npos) {
    return absl::InvalidArgumentError(
        "Invalid background command or directory");
  }
#endif
  auto child_environment = ChildEnvironment(environment);
  if (!child_environment.ok())
    return child_environment.status();

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
                        directory, std::move(*child_environment));
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
#if defined(_WIN32)
  WorkerMainWindows(std::move(command), std::move(directory),
                    std::move(environment));
  return;
#elif defined(__EMSCRIPTEN__) || (defined(__APPLE__) && TARGET_OS_IPHONE)
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

#ifdef _WIN32
void BackgroundCommandTask::WorkerMainWindows(
    std::string command, std::string directory,
    std::vector<std::string> environment) {
  const auto cancelled = [this] {
    std::lock_guard<std::mutex> lock(mutex_);
    return cancel_requested_;
  };
  const auto fail = [this](const std::string& action, DWORD error) {
    Finalize(WindowsError(action, error), -1);
  };
  auto wide_command = ToWide(command);
  if (!wide_command.ok()) {
    Finalize(wide_command.status(), -1);
    return;
  }
  auto wide_directory = ToWide(directory);
  if (!wide_directory.ok()) {
    Finalize(wide_directory.status(), -1);
    return;
  }
  if (!wide_directory->empty()) {
    const DWORD required =
        GetFullPathNameW(wide_directory->c_str(), 0, nullptr, nullptr);
    if (!required) {
      fail("Cannot resolve command working directory", GetLastError());
      return;
    }
    std::vector<wchar_t> absolute_directory(required);
    const DWORD copied = GetFullPathNameW(wide_directory->c_str(), required,
                                          absolute_directory.data(), nullptr);
    if (!copied || copied >= required) {
      fail("Cannot resolve command working directory", GetLastError());
      return;
    }
    *wide_directory = std::wstring(absolute_directory.data(), copied);
    const DWORD attributes = GetFileAttributesW(wide_directory->c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
      const DWORD error = attributes == INVALID_FILE_ATTRIBUTES
                              ? GetLastError()
                              : ERROR_DIRECTORY;
      fail("Cannot enter command working directory: " + directory, error);
      return;
    }
  }
  std::vector<wchar_t> environment_block;
  for (const auto& entry : environment) {
    auto wide_entry = ToWide(entry);
    if (!wide_entry.ok()) {
      Finalize(wide_entry.status(), -1);
      return;
    }
    environment_block.insert(environment_block.end(), wide_entry->begin(),
                             wide_entry->end());
    environment_block.push_back(L'\0');
  }
  if (environment_block.empty())
    environment_block.push_back(L'\0');
  environment_block.push_back(L'\0');

  std::vector<wchar_t> system_directory(MAX_PATH);
  DWORD system_length =
      GetSystemDirectoryW(system_directory.data(), system_directory.size());
  if (system_length >= system_directory.size()) {
    system_directory.resize(system_length + 1);
    system_length =
        GetSystemDirectoryW(system_directory.data(), system_directory.size());
  }
  if (!system_length || system_length >= system_directory.size()) {
    fail("Cannot resolve Windows command interpreter", GetLastError());
    return;
  }
  const std::wstring shell =
      std::wstring(system_directory.data(), system_length) + L"\\cmd.exe";
  std::wstring command_line =
      L"\"" + shell + L"\" /d /s /c \"" + *wide_command + L"\"";
  if (command_line.size() >= 32767) {
    Finalize(absl::InvalidArgumentError(
                 "Windows command line exceeds 32766 characters"),
             -1);
    return;
  }

  SECURITY_ATTRIBUTES security{};
  security.nLength = sizeof(security);
  security.bInheritHandle = TRUE;
  HANDLE raw_read = nullptr;
  HANDLE raw_write = nullptr;
  if (!CreatePipe(&raw_read, &raw_write, &security, 0)) {
    fail("Failed to create process pipe", GetLastError());
    return;
  }
  ScopedWindowsHandle pipe_read(raw_read);
  ScopedWindowsHandle pipe_write(raw_write);
  if (!SetHandleInformation(pipe_read.get(), HANDLE_FLAG_INHERIT, 0)) {
    fail("Cannot protect parent process pipe", GetLastError());
    return;
  }
  ScopedWindowsHandle input(
      CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                  &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
  if (!input.valid()) {
    fail("Cannot open background command input", GetLastError());
    return;
  }
  ScopedWindowsHandle job(CreateJobObjectW(nullptr, nullptr));
  if (!job.valid()) {
    fail("Cannot create background command job", GetLastError());
    return;
  }
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
  limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if (!SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation,
                               &limits, sizeof(limits))) {
    fail("Cannot contain background command job", GetLastError());
    return;
  }

  SIZE_T attribute_bytes = 0;
  InitializeProcThreadAttributeList(nullptr, 1, 0, &attribute_bytes);
  if (!attribute_bytes) {
    fail("Cannot size background command handle list", GetLastError());
    return;
  }
  std::vector<unsigned char> attribute_storage(attribute_bytes);
  auto* attributes =
      reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attribute_storage.data());
  if (!InitializeProcThreadAttributeList(attributes, 1, 0, &attribute_bytes)) {
    fail("Cannot initialize background command handle list", GetLastError());
    return;
  }
  absl::Cleanup delete_attributes = [attributes] {
    DeleteProcThreadAttributeList(attributes);
  };
  HANDLE inherited[] = {pipe_write.get(), input.get()};
  if (!UpdateProcThreadAttribute(attributes, 0,
                                 PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited,
                                 sizeof(inherited), nullptr, nullptr)) {
    fail("Cannot restrict background command handles", GetLastError());
    return;
  }
  STARTUPINFOEXW startup{};
  startup.StartupInfo.cb = sizeof(startup);
  startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
  startup.StartupInfo.hStdInput = input.get();
  startup.StartupInfo.hStdOutput = pipe_write.get();
  startup.StartupInfo.hStdError = pipe_write.get();
  startup.lpAttributeList = attributes;
  PROCESS_INFORMATION process_info{};
  if (cancelled()) {
    Finalize(absl::CancelledError("Background command cancelled"), -1);
    return;
  }
  if (!CreateProcessW(
          shell.c_str(), command_line.data(), nullptr, nullptr, TRUE,
          CREATE_SUSPENDED | CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT |
              EXTENDED_STARTUPINFO_PRESENT,
          environment_block.data(),
          wide_directory->empty() ? nullptr : wide_directory->c_str(),
          &startup.StartupInfo, &process_info)) {
    fail("Cannot start background command", GetLastError());
    return;
  }
  ScopedWindowsHandle process(process_info.hProcess);
  ScopedWindowsHandle primary_thread(process_info.hThread);
  pipe_write.Reset();
  input.Reset();
  if (!AssignProcessToJobObject(job.get(), process.get())) {
    const DWORD assignment_error = GetLastError();
    if (!TerminateProcess(process.get(), 1)) {
      fail(absl::StrCat("Cannot stop uncontained suspended command PID ",
                        process_info.dwProcessId),
           GetLastError());
      return;
    }
    WaitForSingleObject(process.get(), INFINITE);
    fail("Cannot assign background command to its job", assignment_error);
    return;
  }
  if (cancelled()) {
    if (!TerminateJobObject(job.get(), 1))
      job.Reset();
    WaitForSingleObject(process.get(), INFINITE);
    Finalize(absl::CancelledError("Background command cancelled"), -1);
    return;
  }
  if (ResumeThread(primary_thread.get()) == static_cast<DWORD>(-1)) {
    const DWORD resume_error = GetLastError();
    if (!TerminateJobObject(job.get(), 1))
      job.Reset();
    WaitForSingleObject(process.get(), INFINITE);
    fail("Cannot resume background command", resume_error);
    return;
  }
  primary_thread.Reset();

  std::array<char, 4096> buffer{};
  bool process_exited = false;
  bool pipe_closed = false;
  bool job_terminated = false;
  bool job_empty = false;
  DWORD exit_code = 0;
  absl::Status transport_status = absl::OkStatus();
  while (!process_exited || !pipe_closed || !job_empty) {
    if (cancelled() && !job_terminated) {
      if (!TerminateJobObject(job.get(), 1)) {
        transport_status = WindowsError("Cannot cancel background command job",
                                        GetLastError());
        job.Reset();
        WaitForSingleObject(process.get(), INFINITE);
        break;
      }
      job_terminated = true;
    }
    bool read_output = false;
    if (!pipe_closed) {
      DWORD available = 0;
      if (PeekNamedPipe(pipe_read.get(), nullptr, 0, nullptr, &available,
                        nullptr)) {
        if (available) {
          DWORD bytes_read = 0;
          if (ReadFile(pipe_read.get(), buffer.data(),
                       std::min<DWORD>(available, buffer.size()), &bytes_read,
                       nullptr)) {
            if (bytes_read) {
              AppendOutput(buffer.data(), bytes_read);
              read_output = true;
            }
          } else if (GetLastError() == ERROR_BROKEN_PIPE) {
            pipe_closed = true;
          } else {
            transport_status = WindowsError(
                "Cannot read background command output", GetLastError());
            pipe_closed = true;
          }
        }
      } else if (GetLastError() == ERROR_BROKEN_PIPE) {
        pipe_closed = true;
      } else {
        transport_status = WindowsError(
            "Cannot inspect background command output", GetLastError());
        pipe_closed = true;
      }
    }
    if (!process_exited) {
      const DWORD wait =
          WaitForSingleObject(process.get(), read_output ? 0 : kPollIntervalMs);
      if (wait == WAIT_OBJECT_0) {
        if (!GetExitCodeProcess(process.get(), &exit_code)) {
          transport_status = WindowsError(
              "Cannot read background command exit code", GetLastError());
        }
        process_exited = true;
      } else if (wait == WAIT_FAILED) {
        transport_status =
            WindowsError("Cannot wait for background command", GetLastError());
        process_exited = true;
      }
    } else if (!read_output) {
      std::this_thread::sleep_for(std::chrono::milliseconds(kPollIntervalMs));
    }
    // A completed shell must not leave descendants or inherited pipe writers.
    if ((process_exited || !transport_status.ok()) && !job_terminated) {
      if (!TerminateJobObject(job.get(), exit_code)) {
        transport_status = WindowsError("Cannot finish background command job",
                                        GetLastError());
        job.Reset();
        WaitForSingleObject(process.get(), INFINITE);
        break;
      }
      job_terminated = true;
    }
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
    if (!QueryInformationJobObject(job.get(),
                                   JobObjectBasicAccountingInformation,
                                   &accounting, sizeof(accounting), nullptr)) {
      transport_status = WindowsError(
          "Cannot verify background command job cleanup", GetLastError());
      // Closing the sole kill-on-close handle still terminates the owned tree.
      job.Reset();
      WaitForSingleObject(process.get(), INFINITE);
      break;
    }
    job_empty = accounting.ActiveProcesses == 0;
  }
  if (cancelled()) {
    Finalize(absl::CancelledError("Background command cancelled"), -1);
    return;
  }
  if (!transport_status.ok()) {
    Finalize(transport_status, -1);
    return;
  }
  if (exit_code != 0) {
    std::string message =
        absl::StrCat("Background command failed (exit ", exit_code, ")");
    std::string detail = GetSnapshot().output_tail;
    const auto last_newline = detail.rfind('\n');
    if (last_newline != std::string::npos)
      detail.erase(0, last_newline + 1);
    if (detail.size() > kFailureDetailLimit) {
      size_t end = kFailureDetailLimit;
      while (end > 0 &&
             (static_cast<unsigned char>(detail[end]) & 0xC0) == 0x80)
        --end;
      detail.resize(end);
      detail += "...";
    }
    if (!detail.empty())
      message += ": " + detail;
    Finalize(absl::InternalError(message), static_cast<int>(exit_code));
    return;
  }
  Finalize(absl::OkStatus(), 0);
}
#endif

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
