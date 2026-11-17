#include "cli/service/ai/subprocess_runner.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
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

#if defined(YAZE_SUBPROCESS_POSIX)
bool SetNonBlockingAndCloExec(int fd) {
  int flags = ::fcntl(fd, F_GETFL, 0);
  if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
    return false;
  }
  int fd_flags = ::fcntl(fd, F_GETFD, 0);
  if (fd_flags < 0 || ::fcntl(fd, F_SETFD, fd_flags | FD_CLOEXEC) != 0) {
    return false;
  }
  return true;
}

void DrainFd(int fd, std::string* out, size_t max_bytes, bool* eof) {
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
  };

  if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0) ||
      !CreatePipe(&stderr_read, &stderr_write, &sa, 0) ||
      !CreatePipe(&stdin_read, &stdin_write, &sa, 0)) {
    close_all();
    return absl::InternalError("CreatePipe failed");
  }
  SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0);

  STARTUPINFOW si{};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdInput = stdin_read;
  si.hStdOutput = stdout_write;
  si.hStdError = stderr_write;

  PROCESS_INFORMATION pi{};
  std::wstring mutable_cmdline = *cmdline_w_or;
  const BOOL ok = CreateProcessW(exe_w_or->c_str(), mutable_cmdline.data(),
                                 nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
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
  auto drain_win_pipe = [&](HANDLE pipe, std::string* dst) {
    std::array<char, 4096> buf{};
    DWORD read_bytes = 0;
    while (ReadFile(pipe, buf.data(), static_cast<DWORD>(buf.size()),
                    &read_bytes, nullptr) &&
           read_bytes > 0) {
      const size_t avail = dst->size() < options.max_output_bytes
                               ? (options.max_output_bytes - dst->size())
                               : 0;
      const size_t copy_len = std::min(avail, static_cast<size_t>(read_bytes));
      if (copy_len > 0) {
        dst->append(buf.data(), copy_len);
      }
    }
  };

  std::thread stdout_thread(
      [&]() { drain_win_pipe(stdout_read, &result.stdout_output); });
  std::thread stderr_thread(
      [&]() { drain_win_pipe(stderr_read, &result.stderr_output); });

  const auto deadline =
      options.timeout.count() > 0
          ? std::optional<std::chrono::steady_clock::time_point>(
                start_time + options.timeout)
          : std::nullopt;

  bool terminated_for_timeout = false;
  bool terminated_for_cancel = false;
  while (true) {
    const DWORD wait_rc = WaitForSingleObject(pi.hProcess, 50);
    if (wait_rc == WAIT_OBJECT_0) {
      break;
    }
    if (options.cancel_requested != nullptr &&
        options.cancel_requested->load(std::memory_order_relaxed)) {
      terminated_for_cancel = true;
      TerminateProcess(pi.hProcess, 1);
      WaitForSingleObject(pi.hProcess, 1000);
      break;
    }
    if (deadline.has_value() && std::chrono::steady_clock::now() >= *deadline) {
      terminated_for_timeout = true;
      TerminateProcess(pi.hProcess, 1);
      WaitForSingleObject(pi.hProcess, 1000);
      break;
    }
  }

  DWORD win_exit_code = 1;
  GetExitCodeProcess(pi.hProcess, &win_exit_code);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  stdout_thread.join();
  stderr_thread.join();
  close_all();

  result.duration_seconds = std::chrono::duration<double>(
                                std::chrono::steady_clock::now() - start_time)
                                .count();
  result.exit_code = static_cast<int>(win_exit_code);
  result.timed_out = terminated_for_timeout;
  result.cancelled = terminated_for_cancel;

  if (terminated_for_cancel) {
    return absl::CancelledError("Subprocess execution cancelled");
  }
  if (terminated_for_timeout) {
    return absl::DeadlineExceededError(absl::StrCat(
        "CLI subprocess timed out after ",
        std::chrono::duration_cast<std::chrono::seconds>(options.timeout)
            .count(),
        "s"));
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

  if (::pipe(stdout_pipe) != 0 || ::pipe(stderr_pipe) != 0 ||
      (send_stdin && ::pipe(stdin_pipe) != 0)) {
    const int err = errno;
    cleanup_pipes();
    return absl::InternalError(
        absl::StrCat("pipe() failed: ", std::strerror(err)));
  }

  if (!SetNonBlockingAndCloExec(stdout_pipe[0]) ||
      !SetNonBlockingAndCloExec(stderr_pipe[0]) ||
      (send_stdin && !SetNonBlockingAndCloExec(stdin_pipe[1]))) {
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

  int setup_rc = ::posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
  if (setup_rc == 0) {
    setup_rc = ::posix_spawnattr_setpgroup(&attr, 0);
  }

  if (setup_rc == 0) {
    if (send_stdin) {
      setup_rc = ::posix_spawn_file_actions_adddup2(&actions, stdin_pipe[0],
                                                    STDIN_FILENO);
      if (setup_rc == 0) {
        setup_rc = ::posix_spawn_file_actions_addclose(&actions, stdin_pipe[0]);
      }
      if (setup_rc == 0) {
        setup_rc = ::posix_spawn_file_actions_addclose(&actions, stdin_pipe[1]);
      }
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

  while (!stdout_eof || !stderr_eof || !child_exited) {
    if (options.cancel_requested != nullptr &&
        options.cancel_requested->load(std::memory_order_relaxed)) {
      result.cancelled = true;
      TerminateProcessGroup(pid);
      DrainFd(stdout_pipe[0], &result.stdout_output, options.max_output_bytes,
              &stdout_eof);
      DrainFd(stderr_pipe[0], &result.stderr_output, options.max_output_bytes,
              &stderr_eof);
      cleanup_pipes();
      result.duration_seconds =
          std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                        start_time)
              .count();
      return absl::CancelledError("Subprocess execution cancelled");
    }

    if (deadline.has_value() && std::chrono::steady_clock::now() >= *deadline) {
      result.timed_out = true;
      TerminateProcessGroup(pid);
      DrainFd(stdout_pipe[0], &result.stdout_output, options.max_output_bytes,
              &stdout_eof);
      DrainFd(stderr_pipe[0], &result.stderr_output, options.max_output_bytes,
              &stderr_eof);
      cleanup_pipes();
      result.duration_seconds =
          std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                        start_time)
              .count();
      return absl::DeadlineExceededError(absl::StrCat(
          "CLI subprocess timed out after ",
          std::chrono::duration_cast<std::chrono::seconds>(options.timeout)
              .count(),
          "s"));
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

    if (nfds > 0) {
      const int poll_rc = ::poll(fds.data(), nfds, 50);
      if (poll_rc < 0 && errno != EINTR) {
        break;
      }
      if (stdout_idx >= 0 && fds[stdout_idx].revents != 0) {
        DrainFd(stdout_pipe[0], &result.stdout_output, options.max_output_bytes,
                &stdout_eof);
      }
      if (stderr_idx >= 0 && fds[stderr_idx].revents != 0) {
        DrainFd(stderr_pipe[0], &result.stderr_output, options.max_output_bytes,
                &stderr_eof);
      }
      if (stdin_idx >= 0 && fds[stdin_idx].revents != 0) {
        if ((fds[stdin_idx].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
          stdin_done = true;
          close_fd(stdin_pipe[1]);
        } else if ((fds[stdin_idx].revents & POLLOUT) != 0) {
          while (stdin_offset < options.stdin_data.size()) {
            const ssize_t written =
                ::write(stdin_pipe[1], options.stdin_data.data() + stdin_offset,
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
    } else if (!child_exited) {
      std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }

    if (!child_exited) {
      const pid_t waited = ::waitpid(pid, &wait_status, WNOHANG);
      if (waited == pid) {
        child_exited = true;
        result.exit_code = WaitStatusToExitCode(wait_status);
      } else if (waited < 0 && errno != EINTR) {
        child_exited = true;
      }
    }
  }

  cleanup_pipes();
  result.duration_seconds = std::chrono::duration<double>(
                                std::chrono::steady_clock::now() - start_time)
                                .count();
  return result;
#else
  (void)options;
  return absl::UnavailableError(
      "CLI subprocess execution is not supported on this platform");
#endif
}

}  // namespace yaze::cli
