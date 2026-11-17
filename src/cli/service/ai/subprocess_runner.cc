#include "cli/service/ai/subprocess_runner.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_set>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif !defined(__EMSCRIPTEN__) && !defined(YAZE_IOS) && \
    !(defined(__APPLE__) &&                             \
      (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1))
#define YAZE_SUBPROCESS_POSIX 1
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace yaze::cli {

namespace {

namespace fs = std::filesystem;

std::string GetHomeDirectory() {
#ifdef _WIN32
  if (const char* user_profile = std::getenv("USERPROFILE");
      user_profile != nullptr && *user_profile != '\0') {
    return user_profile;
  }
#endif
  if (const char* home = std::getenv("HOME");
      home != nullptr && *home != '\0') {
    return home;
  }
  return "";
}

bool HasPathSeparator(absl::string_view value) {
  return value.find('/') != absl::string_view::npos ||
         value.find('\\') != absl::string_view::npos;
}

bool IsExecutableFile(const fs::path& candidate,
                      std::string* reason = nullptr) {
  std::error_code ec;
  if (!fs::exists(candidate, ec)) {
    if (reason != nullptr) {
      *reason = "does not exist";
    }
    return false;
  }
  if (!fs::is_regular_file(candidate, ec)) {
    if (reason != nullptr) {
      *reason = "is not a regular file";
    }
    return false;
  }
#if defined(_WIN32)
  return true;
#else
  if (::access(candidate.c_str(), X_OK) != 0) {
    if (reason != nullptr) {
      *reason = absl::StrCat("is not executable (", std::strerror(errno), ")");
    }
    return false;
  }
  return true;
#endif
}

std::vector<std::string> GetCandidateFilenames(const fs::path& binary) {
  std::vector<std::string> names;
  names.push_back(binary.string());
#ifdef _WIN32
  if (!binary.has_extension()) {
    names.push_back(binary.string() + ".exe");
    names.push_back(binary.string() + ".com");
  }
#endif
  return names;
}

std::string FormatPartialOutputTail(const SubprocessResult& result) {
  absl::string_view raw = !result.stderr_output.empty()
                              ? absl::string_view(result.stderr_output)
                              : absl::string_view(result.stdout_output);
  raw = absl::StripAsciiWhitespace(raw);
  if (raw.empty()) {
    return "";
  }
  constexpr size_t kMaxSnippetBytes = 240;
  if (raw.size() > kMaxSnippetBytes) {
    raw = raw.substr(raw.size() - kMaxSnippetBytes);
  }
  std::string collapsed;
  collapsed.reserve(raw.size());
  bool prev_space = false;
  for (unsigned char c : raw) {
    if (c == '\r' || c == '\n' || c == '\t') {
      if (!prev_space) {
        collapsed.push_back(' ');
        prev_space = true;
      }
    } else if (c >= 0x20 && c != 0x7f) {
      collapsed.push_back(static_cast<char>(c));
      prev_space = (c == ' ');
    }
  }
  const std::string trimmed =
      std::string(absl::StripAsciiWhitespace(collapsed));
  if (trimmed.empty()) {
    return "";
  }
  return absl::StrCat(" (partial output: ", trimmed, ")");
}

#if defined(YAZE_SUBPROCESS_POSIX)
bool EnsureCloExecAndAboveStdio(int* fd) {
  if (fd == nullptr || *fd < 0) {
    return false;
  }
  if (*fd <= STDERR_FILENO) {
#ifdef F_DUPFD_CLOEXEC
    const int relocated = ::fcntl(*fd, F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
#else
    const int relocated = ::fcntl(*fd, F_DUPFD, STDERR_FILENO + 1);
#endif
    if (relocated < 0) {
      return false;
    }
    ::close(*fd);
    *fd = relocated;
  }
  const int fd_flags = ::fcntl(*fd, F_GETFD, 0);
  if (fd_flags < 0) {
    return false;
  }
  if ((fd_flags & FD_CLOEXEC) == 0 &&
      ::fcntl(*fd, F_SETFD, fd_flags | FD_CLOEXEC) != 0) {
    return false;
  }
  return true;
}

bool CreateCloExecPipe(int pipe_fds[2]) {
#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
    defined(__NetBSD__)
  if (::pipe2(pipe_fds, O_CLOEXEC) != 0) {
    return false;
  }
#else
  if (::pipe(pipe_fds) != 0) {
    return false;
  }
#endif
  if (!EnsureCloExecAndAboveStdio(&pipe_fds[0]) ||
      !EnsureCloExecAndAboveStdio(&pipe_fds[1])) {
    const int saved_errno = errno;
    if (pipe_fds[0] >= 0) {
      ::close(pipe_fds[0]);
      pipe_fds[0] = -1;
    }
    if (pipe_fds[1] >= 0) {
      ::close(pipe_fds[1]);
      pipe_fds[1] = -1;
    }
    errno = saved_errno;
    return false;
  }
  return true;
}

bool SetNonBlocking(int fd) {
  const int flags = ::fcntl(fd, F_GETFL, 0);
  return flags >= 0 && ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

void DrainFd(int fd, std::string* out, size_t max_bytes, bool* eof,
             bool* truncated = nullptr) {
  std::array<char, 4096> buffer{};
  while (true) {
    const ssize_t n = ::read(fd, buffer.data(), buffer.size());
    if (n > 0) {
      const size_t avail =
          out->size() < max_bytes ? (max_bytes - out->size()) : 0;
      const size_t copy_len = std::min(avail, static_cast<size_t>(n));
      if (copy_len > 0) {
        out->append(buffer.data(), copy_len);
      }
      if (copy_len < static_cast<size_t>(n) && truncated != nullptr) {
        *truncated = true;
      }
      continue;
    }
    if (n == 0) {
      *eof = true;
      return;
    }
    if (errno == EINTR) {
      continue;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return;
    }
    *eof = true;
    return;
  }
}

void TerminateProcessGroup(pid_t pid) {
  if (pid <= 0) {
    return;
  }
  if (::kill(-pid, SIGTERM) != 0 && errno != ESRCH) {
    ::kill(pid, SIGTERM);
  }
  const auto grace_deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
  int status = 0;
  while (std::chrono::steady_clock::now() < grace_deadline) {
    const pid_t waited = ::waitpid(pid, &status, WNOHANG);
    if (waited == pid || (waited < 0 && errno != EINTR)) {
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  if (::kill(-pid, SIGKILL) != 0 && errno != ESRCH) {
    ::kill(pid, SIGKILL);
  }
  pid_t waited = -1;
  do {
    waited = ::waitpid(pid, &status, 0);
  } while (waited < 0 && errno == EINTR);
}

int WaitStatusToExitCode(int status) {
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  if (WIFSIGNALED(status)) {
    return 128 + WTERMSIG(status);
  }
  return -1;
}
#endif  // YAZE_SUBPROCESS_POSIX

#ifdef _WIN32
absl::StatusOr<std::wstring> Utf8ToWide(absl::string_view input) {
  if (input.empty()) {
    return std::wstring{};
  }
  const int needed =
      MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(),
                          static_cast<int>(input.size()), nullptr, 0);
  if (needed <= 0) {
    return absl::InvalidArgumentError("Invalid UTF-8 string for subprocess");
  }
  std::wstring wide(static_cast<size_t>(needed), L'\0');
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(),
                          static_cast<int>(input.size()), wide.data(),
                          needed) <= 0) {
    return absl::InternalError("Failed to convert UTF-8 argument to UTF-16");
  }
  return wide;
}
#endif  // _WIN32

}  // namespace

std::string ExpandLeadingTilde(absl::string_view path) {
  if (path == "~") {
    const std::string home = GetHomeDirectory();
    return home.empty() ? std::string(path) : home;
  }
  if (path.size() >= 2 && path[0] == '~' &&
      (path[1] == '/' || path[1] == '\\')) {
    const std::string home = GetHomeDirectory();
    if (!home.empty()) {
      return (fs::path(home) / fs::path(std::string(path.substr(2)))).string();
    }
  }
  return std::string(path);
}

std::vector<std::string> GetStandardCliSearchDirectories() {
  std::vector<std::string> dirs;
  std::unordered_set<std::string> seen;
  auto push_dir = [&](const std::string& raw) {
    const std::string expanded =
        ExpandLeadingTilde(absl::StripAsciiWhitespace(raw));
    if (expanded.empty()) {
      return;
    }
    if (seen.insert(expanded).second) {
      dirs.push_back(expanded);
    }
  };

#ifdef _WIN32
  constexpr char kPathSep = ';';
#else
  constexpr char kPathSep = ':';
#endif
  if (const char* path_env = std::getenv("PATH"); path_env != nullptr) {
    for (absl::string_view entry : absl::StrSplit(path_env, kPathSep)) {
      push_dir(std::string(entry));
    }
  }

#ifndef _WIN32
  const std::string home = GetHomeDirectory();
  if (!home.empty()) {
    push_dir((fs::path(home) / ".local" / "bin").string());
    push_dir(
        (fs::path(home) / ".antigravity" / "antigravity" / "bin").string());
    push_dir((fs::path(home) / ".cursor" / "bin").string());
  }
  push_dir("/opt/homebrew/bin");
  push_dir("/usr/local/bin");
#endif
  return dirs;
}

absl::StatusOr<std::string> ResolveCliBinary(absl::string_view binary_name,
                                             absl::string_view override_path) {
  const std::string trimmed_override =
      ExpandLeadingTilde(absl::StripAsciiWhitespace(override_path));
  if (!trimmed_override.empty()) {
    std::string reason;
    const fs::path candidate(trimmed_override);
    if (IsExecutableFile(candidate, &reason)) {
      std::error_code ec;
      const fs::path abs_path = fs::absolute(candidate, ec);
      return ec ? candidate.string() : abs_path.string();
    }
    return absl::NotFoundError(absl::StrCat("Configured CLI binary override '",
                                            trimmed_override, "' ", reason));
  }

  const std::string trimmed_name =
      ExpandLeadingTilde(absl::StripAsciiWhitespace(binary_name));
  if (trimmed_name.empty()) {
    return absl::InvalidArgumentError("CLI binary name cannot be empty");
  }

  if (HasPathSeparator(trimmed_name)) {
    std::string reason;
    for (const std::string& candidate_name :
         GetCandidateFilenames(trimmed_name)) {
      const fs::path candidate(candidate_name);
      if (IsExecutableFile(candidate, &reason)) {
        std::error_code ec;
        const fs::path abs_path = fs::absolute(candidate, ec);
        return ec ? candidate.string() : abs_path.string();
      }
    }
    return absl::NotFoundError(
        absl::StrCat("CLI binary '", trimmed_name, "' ", reason));
  }

  const std::vector<std::string> search_dirs =
      GetStandardCliSearchDirectories();
  for (const std::string& dir : search_dirs) {
    const fs::path base = fs::path(dir) / trimmed_name;
    for (const std::string& candidate_name : GetCandidateFilenames(base)) {
      const fs::path candidate(candidate_name);
      if (IsExecutableFile(candidate)) {
        return candidate.string();
      }
    }
  }

  return absl::NotFoundError(
      absl::StrCat("CLI binary '", trimmed_name,
                   "' not found in PATH or standard CLI dirs (",
                   absl::StrJoin(search_dirs, ", "), ")"));
}

std::string QuoteWindowsCommandLineArg(absl::string_view arg) {
  const bool needs_quotes =
      arg.empty() || arg.find_first_of(" \t\n\v\"") != absl::string_view::npos;
  if (!needs_quotes) {
    return std::string(arg);
  }

  std::string out = "\"";
  size_t backslashes = 0;
  for (char c : arg) {
    if (c == '\\') {
      ++backslashes;
    } else if (c == '"') {
      out.append(backslashes * 2 + 1, '\\');
      out.push_back('"');
      backslashes = 0;
    } else {
      out.append(backslashes, '\\');
      out.push_back(c);
      backslashes = 0;
    }
  }
  out.append(backslashes * 2, '\\');
  out.push_back('"');
  return out;
}

std::string BuildWindowsCommandLine(absl::string_view executable,
                                    const std::vector<std::string>& args) {
  std::string cmdline = QuoteWindowsCommandLineArg(executable);
  for (const std::string& arg : args) {
    cmdline.push_back(' ');
    cmdline += QuoteWindowsCommandLineArg(arg);
  }
  return cmdline;
}

absl::StatusOr<SubprocessResult> RunSubprocess(
    const SubprocessOptions& options) {
  if (options.executable.empty()) {
    return absl::InvalidArgumentError("Subprocess executable cannot be empty");
  }
  if (options.cancel_requested != nullptr &&
      options.cancel_requested->load(std::memory_order_relaxed)) {
    return absl::CancelledError("Subprocess execution cancelled");
  }
  if (!options.working_directory.empty()) {
    std::error_code ec;
    if (!fs::is_directory(options.working_directory, ec)) {
      return absl::FailedPreconditionError(absl::StrCat(
          "Working directory does not exist: ", options.working_directory));
    }
  }

#if defined(_WIN32)
  const auto start_time = std::chrono::steady_clock::now();
  auto resolved_or = ResolveCliBinary(options.executable);
  if (!resolved_or.ok()) {
    return resolved_or.status();
  }
  const std::string& resolved_exe = *resolved_or;
  const std::string cmdline_utf8 =
      BuildWindowsCommandLine(resolved_exe, options.args);

  auto exe_w_or = Utf8ToWide(resolved_exe);
  if (!exe_w_or.ok()) {
    return exe_w_or.status();
  }
  auto cmdline_w_or = Utf8ToWide(cmdline_utf8);
  if (!cmdline_w_or.ok()) {
    return cmdline_w_or.status();
  }
  std::wstring cwd_w;
  LPCWSTR cwd_ptr = nullptr;
  if (!options.working_directory.empty()) {
    auto cwd_w_or = Utf8ToWide(options.working_directory);
    if (!cwd_w_or.ok()) {
      return cwd_w_or.status();
    }
    cwd_w = std::move(*cwd_w_or);
    cwd_ptr = cwd_w.c_str();
  }

  SECURITY_ATTRIBUTES sa{};
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;

  HANDLE stdout_read = nullptr;
  HANDLE stdout_write = nullptr;
  HANDLE stderr_read = nullptr;
  HANDLE stderr_write = nullptr;
  HANDLE stdin_read = nullptr;
  HANDLE stdin_write = nullptr;
  HANDLE job = nullptr;

  auto close_all = [&]() {
    auto close_h = [](HANDLE& h) {
      if (h != nullptr) {
        CloseHandle(h);
        h = nullptr;
      }
    };
    close_h(stdout_read);
    close_h(stdout_write);
    close_h(stderr_read);
    close_h(stderr_write);
    close_h(stdin_read);
    close_h(stdin_write);
    close_h(job);
  };

  PROCESS_INFORMATION pi{};
  static std::mutex g_win_spawn_mutex;
  {
    std::lock_guard<std::mutex> spawn_lock(g_win_spawn_mutex);

    if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0) ||
        !CreatePipe(&stderr_read, &stderr_write, &sa, 0) ||
        !CreatePipe(&stdin_read, &stdin_write, &sa, 0)) {
      close_all();
      return absl::InternalError("CreatePipe failed");
    }
    SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0);

    job = CreateJobObjectW(nullptr, nullptr);
    if (job != nullptr) {
      JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
      limits.BasicLimitInformation.LimitFlags =
          JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
      SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits,
                              sizeof(limits));
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = stdin_read;
    si.hStdOutput = stdout_write;
    si.hStdError = stderr_write;

    std::wstring mutable_cmdline = *cmdline_w_or;
    const DWORD creation_flags =
        CREATE_NO_WINDOW | (job != nullptr ? CREATE_SUSPENDED : 0);
    const BOOL ok = CreateProcessW(exe_w_or->c_str(), mutable_cmdline.data(),
                                   nullptr, nullptr, TRUE, creation_flags,
                                   nullptr, cwd_ptr, &si, &pi);

    CloseHandle(stdout_write);
    stdout_write = nullptr;
    CloseHandle(stderr_write);
    stderr_write = nullptr;
    CloseHandle(stdin_read);
    stdin_read = nullptr;

    if (!ok) {
      const DWORD err = GetLastError();
      close_all();
      return absl::InternalError(
          absl::StrCat("CreateProcessW failed with Win32 error ", err));
    }

    if (job != nullptr) {
      if (!AssignProcessToJobObject(job, pi.hProcess)) {
        CloseHandle(job);
        job = nullptr;
      }
      ResumeThread(pi.hThread);
    }
  }

  if (!options.stdin_data.empty()) {
    size_t offset = 0;
    while (offset < options.stdin_data.size()) {
      DWORD written = 0;
      const DWORD chunk = static_cast<DWORD>(
          std::min<size_t>(options.stdin_data.size() - offset, 64 * 1024));
      if (!WriteFile(stdin_write, options.stdin_data.data() + offset, chunk,
                     &written, nullptr)) {
        break;
      }
      offset += written;
    }
  }
  CloseHandle(stdin_write);
  stdin_write = nullptr;

  SubprocessResult result;
  auto drain_win_pipe = [&](HANDLE pipe, std::string* dst, bool* eof) -> bool {
    if (*eof || pipe == nullptr) {
      *eof = true;
      return false;
    }
    std::array<char, 4096> buf{};
    bool read_any = false;
    while (true) {
      DWORD available = 0;
      if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr)) {
        *eof = true;
        break;
      }
      if (available == 0) {
        break;
      }
      DWORD read_bytes = 0;
      const DWORD to_read =
          static_cast<DWORD>(std::min<size_t>(available, buf.size()));
      if (!ReadFile(pipe, buf.data(), to_read, &read_bytes, nullptr) ||
          read_bytes == 0) {
        *eof = true;
        break;
      }
      read_any = true;
      const size_t avail = dst->size() < options.max_output_bytes
                               ? (options.max_output_bytes - dst->size())
                               : 0;
      const size_t copy_len = std::min(avail, static_cast<size_t>(read_bytes));
      if (copy_len > 0) {
        dst->append(buf.data(), copy_len);
      }
      if (copy_len < static_cast<size_t>(read_bytes)) {
        result.output_truncated = true;
      }
    }
    return read_any;
  };

  const auto deadline =
      options.timeout.count() > 0
          ? std::optional<std::chrono::steady_clock::time_point>(
                start_time + options.timeout)
          : std::nullopt;

  auto terminate_win_tree = [&]() {
    if (job != nullptr) {
      TerminateJobObject(job, 1);
    } else {
      TerminateProcess(pi.hProcess, 1);
    }
    WaitForSingleObject(pi.hProcess, 1000);
  };

  bool child_exited = false;
  bool stdout_eof = false;
  bool stderr_eof = false;
  bool terminated_for_timeout = false;
  bool terminated_for_cancel = false;
  DWORD win_exit_code = 1;
  std::optional<std::chrono::steady_clock::time_point> post_exit_deadline;

  while (!stdout_eof || !stderr_eof || !child_exited) {
    const bool drained_stdout =
        drain_win_pipe(stdout_read, &result.stdout_output, &stdout_eof);
    const bool drained_stderr =
        drain_win_pipe(stderr_read, &result.stderr_output, &stderr_eof);

    if (options.cancel_requested != nullptr &&
        options.cancel_requested->load(std::memory_order_relaxed)) {
      terminated_for_cancel = true;
      if (!child_exited) {
        terminate_win_tree();
      }
      drain_win_pipe(stdout_read, &result.stdout_output, &stdout_eof);
      drain_win_pipe(stderr_read, &result.stderr_output, &stderr_eof);
      break;
    }

    if (!child_exited) {
      const DWORD wait_rc = WaitForSingleObject(
          pi.hProcess, (drained_stdout || drained_stderr) ? 0 : 25);
      if (wait_rc == WAIT_OBJECT_0 || wait_rc == WAIT_FAILED) {
        if (wait_rc == WAIT_OBJECT_0) {
          GetExitCodeProcess(pi.hProcess, &win_exit_code);
        }
        child_exited = true;
        auto grace_end = std::chrono::steady_clock::now() +
                         std::max(std::chrono::milliseconds(0),
                                  options.post_exit_drain_grace);
        if (deadline.has_value() && grace_end > *deadline) {
          grace_end = *deadline;
        }
        post_exit_deadline = grace_end;
      }
    } else if (!drained_stdout && !drained_stderr) {
      std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }

    const auto now = std::chrono::steady_clock::now();
    if (child_exited && post_exit_deadline.has_value() &&
        now >= *post_exit_deadline) {
      drain_win_pipe(stdout_read, &result.stdout_output, &stdout_eof);
      drain_win_pipe(stderr_read, &result.stderr_output, &stderr_eof);
      if (!stdout_eof || !stderr_eof) {
        result.post_exit_eof_timed_out = true;
      }
      break;
    }

    if (deadline.has_value() && now >= *deadline) {
      if (child_exited) {
        drain_win_pipe(stdout_read, &result.stdout_output, &stdout_eof);
        drain_win_pipe(stderr_read, &result.stderr_output, &stderr_eof);
        if (!stdout_eof || !stderr_eof) {
          result.post_exit_eof_timed_out = true;
        }
        break;
      }
      terminated_for_timeout = true;
      terminate_win_tree();
      drain_win_pipe(stdout_read, &result.stdout_output, &stdout_eof);
      drain_win_pipe(stderr_read, &result.stderr_output, &stderr_eof);
      break;
    }
  }

  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  close_all();

  result.duration_seconds = std::chrono::duration<double>(
                                std::chrono::steady_clock::now() - start_time)
                                .count();
  result.exit_code = static_cast<int>(win_exit_code);
  result.timed_out = terminated_for_timeout;
  result.cancelled = terminated_for_cancel;
  if (options.partial_result != nullptr) {
    *options.partial_result = result;
  }

  if (terminated_for_cancel) {
    return absl::CancelledError("Subprocess execution cancelled");
  }
  if (terminated_for_timeout) {
    return absl::DeadlineExceededError(absl::StrCat(
        "CLI subprocess timed out after ",
        std::chrono::duration_cast<std::chrono::seconds>(options.timeout)
            .count(),
        "s", FormatPartialOutputTail(result)));
  }
  return result;
#elif defined(YAZE_SUBPROCESS_POSIX)
  const auto start_time = std::chrono::steady_clock::now();

  int stdout_pipe[2] = {-1, -1};
  int stderr_pipe[2] = {-1, -1};
  int stdin_pipe[2] = {-1, -1};
  const bool send_stdin = !options.stdin_data.empty();

  auto close_fd = [](int& fd) {
    if (fd >= 0) {
      ::close(fd);
      fd = -1;
    }
  };
  auto cleanup_pipes = [&]() {
    close_fd(stdout_pipe[0]);
    close_fd(stdout_pipe[1]);
    close_fd(stderr_pipe[0]);
    close_fd(stderr_pipe[1]);
    close_fd(stdin_pipe[0]);
    close_fd(stdin_pipe[1]);
  };

  // Serialize pipe creation + FD_CLOEXEC setup + posix_spawn/fork across threads
  // so concurrent spawns never inherit transient pipe fds before CLOEXEC or
  // expose sibling write ends.
  static std::mutex g_posix_spawn_mutex;
  std::unique_lock<std::mutex> spawn_lock(g_posix_spawn_mutex);

  if (!CreateCloExecPipe(stdout_pipe) || !CreateCloExecPipe(stderr_pipe) ||
      (send_stdin && !CreateCloExecPipe(stdin_pipe))) {
    const int err = errno;
    cleanup_pipes();
    return absl::InternalError(
        absl::StrCat("pipe() failed: ", std::strerror(err)));
  }

  if (!SetNonBlocking(stdout_pipe[0]) || !SetNonBlocking(stderr_pipe[0]) ||
      (send_stdin && !SetNonBlocking(stdin_pipe[1]))) {
    cleanup_pipes();
    return absl::InternalError("Failed to configure non-blocking pipe fds");
  }
#ifdef F_SETNOSIGPIPE
  if (send_stdin) {
    ::fcntl(stdin_pipe[1], F_SETNOSIGPIPE, 1);
  }
#endif

  std::vector<char*> argv;
  argv.reserve(options.args.size() + 2);
  argv.push_back(const_cast<char*>(options.executable.c_str()));
  for (const std::string& arg : options.args) {
    argv.push_back(const_cast<char*>(arg.c_str()));
  }
  argv.push_back(nullptr);

  posix_spawn_file_actions_t actions;
  if (::posix_spawn_file_actions_init(&actions) != 0) {
    cleanup_pipes();
    return absl::InternalError("posix_spawn_file_actions_init failed");
  }

  posix_spawnattr_t attr;
  if (::posix_spawnattr_init(&attr) != 0) {
    ::posix_spawn_file_actions_destroy(&actions);
    cleanup_pipes();
    return absl::InternalError("posix_spawnattr_init failed");
  }

  short spawn_flags = POSIX_SPAWN_SETPGROUP;
#ifdef POSIX_SPAWN_CLOEXEC_DEFAULT
  spawn_flags |= POSIX_SPAWN_CLOEXEC_DEFAULT;
#endif
  int setup_rc = ::posix_spawnattr_setflags(&attr, spawn_flags);
  if (setup_rc == 0) {
    setup_rc = ::posix_spawnattr_setpgroup(&attr, 0);
  }

  if (setup_rc == 0) {
    if (send_stdin) {
      setup_rc = ::posix_spawn_file_actions_adddup2(&actions, stdin_pipe[0],
                                                    STDIN_FILENO);
#ifndef POSIX_SPAWN_CLOEXEC_DEFAULT
      if (setup_rc == 0) {
        setup_rc = ::posix_spawn_file_actions_addclose(&actions, stdin_pipe[0]);
      }
      if (setup_rc == 0) {
        setup_rc = ::posix_spawn_file_actions_addclose(&actions, stdin_pipe[1]);
      }
#endif
    } else {
      setup_rc = ::posix_spawn_file_actions_addopen(&actions, STDIN_FILENO,
                                                    "/dev/null", O_RDONLY, 0);
    }
  }
  if (setup_rc == 0) {
    setup_rc = ::posix_spawn_file_actions_adddup2(&actions, stdout_pipe[1],
                                                  STDOUT_FILENO);
  }
  if (setup_rc == 0) {
    setup_rc = ::posix_spawn_file_actions_adddup2(&actions, stderr_pipe[1],
                                                  STDERR_FILENO);
  }
#ifndef POSIX_SPAWN_CLOEXEC_DEFAULT
  if (setup_rc == 0) {
    setup_rc = ::posix_spawn_file_actions_addclose(&actions, stdout_pipe[0]);
  }
  if (setup_rc == 0) {
    setup_rc = ::posix_spawn_file_actions_addclose(&actions, stdout_pipe[1]);
  }
  if (setup_rc == 0) {
    setup_rc = ::posix_spawn_file_actions_addclose(&actions, stderr_pipe[0]);
  }
  if (setup_rc == 0) {
    setup_rc = ::posix_spawn_file_actions_addclose(&actions, stderr_pipe[1]);
  }
#endif

  bool need_fork_chdir = false;
  if (setup_rc == 0 && !options.working_directory.empty()) {
#if defined(__APPLE__) ||  \
    (defined(__GLIBC__) && \
     (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 29)))
    setup_rc = ::posix_spawn_file_actions_addchdir_np(
        &actions, options.working_directory.c_str());
#else
    need_fork_chdir = true;
#endif
  }

  pid_t pid = -1;
  int spawn_rc = setup_rc;
  if (spawn_rc == 0) {
    if (!need_fork_chdir) {
      if (HasPathSeparator(options.executable)) {
        spawn_rc = ::posix_spawn(&pid, options.executable.c_str(), &actions,
                                 &attr, argv.data(), environ);
      } else {
        spawn_rc = ::posix_spawnp(&pid, options.executable.c_str(), &actions,
                                  &attr, argv.data(), environ);
      }
    } else {
      pid = ::fork();
      if (pid < 0) {
        spawn_rc = errno;
      } else if (pid == 0) {
        ::setpgid(0, 0);
        if (send_stdin) {
          ::dup2(stdin_pipe[0], STDIN_FILENO);
          ::close(stdin_pipe[0]);
          ::close(stdin_pipe[1]);
        } else {
          const int devnull = ::open("/dev/null", O_RDONLY);
          if (devnull >= 0) {
            ::dup2(devnull, STDIN_FILENO);
            ::close(devnull);
          }
        }
        ::dup2(stdout_pipe[1], STDOUT_FILENO);
        ::dup2(stderr_pipe[1], STDERR_FILENO);
        ::close(stdout_pipe[0]);
        ::close(stdout_pipe[1]);
        ::close(stderr_pipe[0]);
        ::close(stderr_pipe[1]);
        if (::chdir(options.working_directory.c_str()) != 0) {
          _exit(126);
        }
        if (HasPathSeparator(options.executable)) {
          ::execv(options.executable.c_str(), argv.data());
        } else {
          ::execvp(options.executable.c_str(), argv.data());
        }
        _exit(127);
      }
    }
  }

  ::posix_spawnattr_destroy(&attr);
  ::posix_spawn_file_actions_destroy(&actions);

  close_fd(stdout_pipe[1]);
  close_fd(stderr_pipe[1]);
  close_fd(stdin_pipe[0]);
  spawn_lock.unlock();

  if (spawn_rc != 0 || pid <= 0) {
    cleanup_pipes();
    if (spawn_rc == ENOENT) {
      return absl::NotFoundError(
          absl::StrCat("CLI executable not found: '", options.executable, "'"));
    }
    return absl::InternalError(absl::StrCat("Failed to spawn '",
                                            options.executable,
                                            "': ", std::strerror(spawn_rc)));
  }

  SubprocessResult result;
  bool stdout_eof = false;
  bool stderr_eof = false;
  bool stdin_done = !send_stdin;
  size_t stdin_offset = 0;

  if (!send_stdin) {
    close_fd(stdin_pipe[1]);
  }

  const auto deadline =
      options.timeout.count() > 0
          ? std::optional<std::chrono::steady_clock::time_point>(
                start_time + options.timeout)
          : std::nullopt;

  bool child_exited = false;
  int wait_status = 0;
  std::optional<std::chrono::steady_clock::time_point> post_exit_deadline;

  auto arm_post_exit_deadline = [&]() {
    if (!post_exit_deadline.has_value()) {
      auto grace_end =
          std::chrono::steady_clock::now() +
          std::max(std::chrono::milliseconds(0), options.post_exit_drain_grace);
      if (deadline.has_value() && grace_end > *deadline) {
        grace_end = *deadline;
      }
      post_exit_deadline = grace_end;
    }
  };

  auto check_child_exit = [&]() {
    if (child_exited) {
      return;
    }
    pid_t waited = -1;
    do {
      waited = ::waitpid(pid, &wait_status, WNOHANG);
    } while (waited < 0 && errno == EINTR);
    if (waited == pid) {
      child_exited = true;
      result.exit_code = WaitStatusToExitCode(wait_status);
      arm_post_exit_deadline();
    } else if (waited < 0) {
      child_exited = true;
      arm_post_exit_deadline();
    }
  };

  while (!stdout_eof || !stderr_eof || !child_exited) {
    if (options.cancel_requested != nullptr &&
        options.cancel_requested->load(std::memory_order_relaxed)) {
      result.cancelled = true;
      if (!child_exited) {
        TerminateProcessGroup(pid);
      }
      if (!stdout_eof) {
        DrainFd(stdout_pipe[0], &result.stdout_output, options.max_output_bytes,
                &stdout_eof, &result.output_truncated);
      }
      if (!stderr_eof) {
        DrainFd(stderr_pipe[0], &result.stderr_output, options.max_output_bytes,
                &stderr_eof, &result.output_truncated);
      }
      cleanup_pipes();
      result.duration_seconds =
          std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                        start_time)
              .count();
      if (options.partial_result != nullptr) {
        *options.partial_result = result;
      }
      return absl::CancelledError("Subprocess execution cancelled");
    }

    const auto now = std::chrono::steady_clock::now();
    if (!child_exited) {
      check_child_exit();
    }

    if (child_exited && post_exit_deadline.has_value() &&
        now >= *post_exit_deadline) {
      if (!stdout_eof) {
        DrainFd(stdout_pipe[0], &result.stdout_output, options.max_output_bytes,
                &stdout_eof, &result.output_truncated);
      }
      if (!stderr_eof) {
        DrainFd(stderr_pipe[0], &result.stderr_output, options.max_output_bytes,
                &stderr_eof, &result.output_truncated);
      }
      if (!stdout_eof || !stderr_eof) {
        result.post_exit_eof_timed_out = true;
      }
      break;
    }

    if (deadline.has_value() && now >= *deadline) {
      if (child_exited) {
        if (!stdout_eof) {
          DrainFd(stdout_pipe[0], &result.stdout_output,
                  options.max_output_bytes, &stdout_eof,
                  &result.output_truncated);
        }
        if (!stderr_eof) {
          DrainFd(stderr_pipe[0], &result.stderr_output,
                  options.max_output_bytes, &stderr_eof,
                  &result.output_truncated);
        }
        if (!stdout_eof || !stderr_eof) {
          result.post_exit_eof_timed_out = true;
        }
        break;
      }
      result.timed_out = true;
      TerminateProcessGroup(pid);
      if (!stdout_eof) {
        DrainFd(stdout_pipe[0], &result.stdout_output, options.max_output_bytes,
                &stdout_eof, &result.output_truncated);
      }
      if (!stderr_eof) {
        DrainFd(stderr_pipe[0], &result.stderr_output, options.max_output_bytes,
                &stderr_eof, &result.output_truncated);
      }
      cleanup_pipes();
      result.duration_seconds =
          std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                        start_time)
              .count();
      if (options.partial_result != nullptr) {
        *options.partial_result = result;
      }
      return absl::DeadlineExceededError(absl::StrCat(
          "CLI subprocess timed out after ",
          std::chrono::duration_cast<std::chrono::seconds>(options.timeout)
              .count(),
          "s", FormatPartialOutputTail(result)));
    }

    std::array<pollfd, 3> fds{};
    nfds_t nfds = 0;
    int stdout_idx = -1;
    int stderr_idx = -1;
    int stdin_idx = -1;

    if (!stdout_eof && stdout_pipe[0] >= 0) {
      stdout_idx = static_cast<int>(nfds);
      fds[nfds++] = {.fd = stdout_pipe[0], .events = POLLIN, .revents = 0};
    }
    if (!stderr_eof && stderr_pipe[0] >= 0) {
      stderr_idx = static_cast<int>(nfds);
      fds[nfds++] = {.fd = stderr_pipe[0], .events = POLLIN, .revents = 0};
    }
    if (!stdin_done && stdin_pipe[1] >= 0) {
      stdin_idx = static_cast<int>(nfds);
      fds[nfds++] = {.fd = stdin_pipe[1], .events = POLLOUT, .revents = 0};
    }

    int poll_ms = 50;
    const auto clamp_wait_to =
        [&](std::chrono::steady_clock::time_point limit) {
          const auto rem =
              std::chrono::duration_cast<std::chrono::milliseconds>(
                  limit - std::chrono::steady_clock::now());
          poll_ms = std::min<int>(
              poll_ms, std::max<int>(0, static_cast<int>(rem.count())));
        };
    if (post_exit_deadline.has_value()) {
      clamp_wait_to(*post_exit_deadline);
    }
    if (deadline.has_value()) {
      clamp_wait_to(*deadline);
    }

    if (nfds > 0) {
      const int poll_rc = ::poll(fds.data(), nfds, poll_ms);
      if (poll_rc < 0 && errno != EINTR) {
        const int poll_err = errno;
        if (!child_exited) {
          TerminateProcessGroup(pid);
        }
        cleanup_pipes();
        result.duration_seconds =
            std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                          start_time)
                .count();
        if (options.partial_result != nullptr) {
          *options.partial_result = result;
        }
        return absl::InternalError(
            absl::StrCat("poll() failed: ", std::strerror(poll_err)));
      }
      if (poll_rc > 0) {
        if (stdout_idx >= 0 && fds[stdout_idx].revents != 0) {
          DrainFd(stdout_pipe[0], &result.stdout_output,
                  options.max_output_bytes, &stdout_eof,
                  &result.output_truncated);
        }
        if (stderr_idx >= 0 && fds[stderr_idx].revents != 0) {
          DrainFd(stderr_pipe[0], &result.stderr_output,
                  options.max_output_bytes, &stderr_eof,
                  &result.output_truncated);
        }
        if (stdin_idx >= 0 && fds[stdin_idx].revents != 0) {
          if ((fds[stdin_idx].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            stdin_done = true;
            close_fd(stdin_pipe[1]);
          } else if ((fds[stdin_idx].revents & POLLOUT) != 0) {
            while (stdin_offset < options.stdin_data.size()) {
              const ssize_t written = ::write(
                  stdin_pipe[1], options.stdin_data.data() + stdin_offset,
                  options.stdin_data.size() - stdin_offset);
              if (written > 0) {
                stdin_offset += static_cast<size_t>(written);
              } else if (written < 0 && errno == EINTR) {
                continue;
              } else if (written < 0 &&
                         (errno == EAGAIN || errno == EWOULDBLOCK)) {
                break;
              } else {
                stdin_done = true;
                close_fd(stdin_pipe[1]);
                break;
              }
            }
            if (!stdin_done && stdin_offset >= options.stdin_data.size()) {
              stdin_done = true;
              close_fd(stdin_pipe[1]);
            }
          }
        }
      }
    } else if (!child_exited) {
      std::this_thread::sleep_for(
          std::chrono::milliseconds(std::min(15, std::max(1, poll_ms))));
    }

    if (!child_exited) {
      check_child_exit();
    }
  }

  cleanup_pipes();
  result.duration_seconds = std::chrono::duration<double>(
                                std::chrono::steady_clock::now() - start_time)
                                .count();
  if (options.partial_result != nullptr) {
    *options.partial_result = result;
  }
  return result;
#else
  (void)options;
  return absl::UnavailableError(
      "CLI subprocess execution is not supported on this platform");
#endif
}

}  // namespace yaze::cli
