#include "util/crash_handler.h"

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string_view>
#include <system_error>
#include <vector>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "util/platform_paths.h"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#define STDERR_FILENO _fileno(stderr)
#define write _write
#define close _close
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace yaze {
namespace util {

namespace {

using PathChar = std::filesystem::path::value_type;

constexpr size_t kMaxCrashLogPathChars = 4096;
constexpr size_t kMaxCrashLogHeaderBytes = 1024;

constexpr std::string_view kHeaderBanner = "=== YAZE Crash Report ===\n";
constexpr std::string_view kHeaderEnd = "========================\n\n";

// Filled by PrepareCrashLog before the handler is installed and only read in
// the signal path, which must not allocate.
PathChar g_crash_log_path[kMaxCrashLogPathChars] = {};
char g_crash_log_header[kMaxCrashLogHeaderBytes] = {};
size_t g_crash_log_header_len = 0;
volatile sig_atomic_t g_crash_log_open_attempted = 0;

}  // namespace

// Static member definitions
std::string CrashHandler::version_;
std::filesystem::path CrashHandler::crash_log_path_;
int CrashHandler::crash_log_fd_ = -1;

void CrashHandler::OpenCrashLogFromSignalHandler() {
  if (crash_log_fd_ >= 0 || g_crash_log_open_attempted ||
      g_crash_log_path[0] == PathChar{0}) {
    return;
  }
  g_crash_log_open_attempted = 1;
#ifdef _WIN32
  crash_log_fd_ =
      _wopen(g_crash_log_path, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY,
             _S_IREAD | _S_IWRITE);
#else
  crash_log_fd_ =
      open(g_crash_log_path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
           S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
#endif
  if (crash_log_fd_ >= 0 && g_crash_log_header_len > 0) {
    write(crash_log_fd_, g_crash_log_header, g_crash_log_header_len);
  }
}

void CrashHandler::CrashLogWriter(const char* data) {
  // Prevent re-entrancy - if we crash while handling a crash, just abort
  static volatile sig_atomic_t in_crash_handler = 0;
  if (in_crash_handler) {
    // Already in crash handler - abort to prevent infinite loop
    _Exit(1);
  }
  in_crash_handler = 1;

  if (data == nullptr) {
    in_crash_handler = 0;
    return;
  }

  // Calculate length manually (strlen might crash)
  size_t len = 0;
  while (len < 4096 && data[len] != '\0') {  // Cap at 4KB to prevent runaway
    ++len;
  }

  OpenCrashLogFromSignalHandler();
  if (crash_log_fd_ >= 0) {
    // Write to crash log file (ignore errors - we're crashing anyway)
    write(crash_log_fd_, data, len);
  }

  // Also write to stderr for immediate visibility
  write(STDERR_FILENO, data, len);

  in_crash_handler = 0;
}

void CrashHandler::PrepareCrashLog(const std::string& version,
                                   const std::filesystem::path& directory) {
  version_ = version;

  auto now = std::chrono::system_clock::now();
  auto time_t_now = std::chrono::system_clock::to_time_t(now);
  std::tm* tm_now = std::localtime(&time_t_now);

  std::ostringstream filename;
  filename << "crash_" << std::put_time(tm_now, "%Y%m%d_%H%M%S") << ".log";
  crash_log_path_ = directory / filename.str();

  std::error_code ec;
  std::filesystem::create_directories(directory, ec);

  const auto& native_path = crash_log_path_.native();
  if (native_path.size() < kMaxCrashLogPathChars) {
    std::copy(native_path.begin(), native_path.end(), g_crash_log_path);
    g_crash_log_path[native_path.size()] = PathChar{0};
  } else {
    // Too long for the static buffer: crashes still reach stderr.
    g_crash_log_path[0] = PathChar{0};
  }

  std::ostringstream header;
  header << kHeaderBanner;
  header << "Version: " << version_ << "\n";
  header << "Session start: " << std::put_time(tm_now, "%Y-%m-%d %H:%M:%S")
         << "\n";
  header << "Platform: ";
#if defined(_WIN32)
  header << "Windows";
#elif defined(__APPLE__)
  header << "macOS";
#elif defined(__linux__)
  header << "Linux";
#else
  header << "Unknown";
#endif
  header << "\n";
  header << kHeaderEnd;

  const std::string header_str = header.str();
  g_crash_log_header_len =
      std::min(header_str.size(), sizeof(g_crash_log_header));
  std::memcpy(g_crash_log_header, header_str.data(), g_crash_log_header_len);
  g_crash_log_open_attempted = 0;
}

void CrashHandler::Initialize(const std::string& version) {
  PrepareCrashLog(version, GetCrashLogDirectory());

  // Configure absl failure signal handler
  absl::FailureSignalHandlerOptions options;
  options.symbolize_stacktrace = true;
  options.use_alternate_stack = true;
  options.call_previous_handler = true;

  // Write to both the crash log (created on first write) and stderr
  options.writerfn = CrashLogWriter;

  absl::InstallFailureSignalHandler(options);
}

void CrashHandler::PrepareCrashLogForTesting(
    const std::string& version, const std::filesystem::path& directory) {
  ResetForTesting();
  PrepareCrashLog(version, directory);
}

void CrashHandler::WriteCrashLogForTesting(const char* data) {
  CrashLogWriter(data);
}

std::filesystem::path CrashHandler::PendingCrashLogPathForTesting() {
  return crash_log_path_;
}

void CrashHandler::ResetForTesting() {
  if (crash_log_fd_ >= 0) {
    close(crash_log_fd_);
    crash_log_fd_ = -1;
  }
  g_crash_log_path[0] = PathChar{0};
  g_crash_log_header_len = 0;
  g_crash_log_open_attempted = 0;
  crash_log_path_.clear();
}

bool CrashHandler::IsHeaderOnlyCrashLog(const std::filesystem::path& path) {
  std::error_code ec;
  const auto size = std::filesystem::file_size(path, ec);
  if (ec || size > kMaxCrashLogHeaderBytes * 2) {
    return false;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  std::string contents((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
  // Older Windows builds opened the log in text mode and wrote CRLF.
  contents.erase(std::remove(contents.begin(), contents.end(), '\r'),
                 contents.end());
  const std::string_view view(contents);
  return view.starts_with(kHeaderBanner) && view.ends_with(kHeaderEnd);
}

std::filesystem::path CrashHandler::GetCrashLogDirectory() {
  auto app_data_result = PlatformPaths::GetAppDataSubdirectory("crash_logs");
  if (app_data_result.ok()) {
    return *app_data_result;
  }

  // Fallback to temp directory
  auto temp_result = PlatformPaths::GetTempDirectory();
  if (temp_result.ok()) {
    return *temp_result / "crash_logs";
  }

  // Last resort: current directory
  return std::filesystem::current_path() / "crash_logs";
}

std::filesystem::path CrashHandler::GetMostRecentCrashLog() {
  auto crash_dir = GetCrashLogDirectory();

  if (!std::filesystem::exists(crash_dir)) {
    return {};
  }

  std::vector<std::filesystem::path> logs;
  for (const auto& entry : std::filesystem::directory_iterator(crash_dir)) {
    if (entry.path().extension() == ".log" &&
        entry.path().filename().string().starts_with("crash_") &&
        !IsHeaderOnlyCrashLog(entry.path())) {
      logs.push_back(entry.path());
    }
  }

  if (logs.empty()) {
    return {};
  }

  // Sort by modification time, newest first
  std::sort(logs.begin(), logs.end(),
            [](const std::filesystem::path& a, const std::filesystem::path& b) {
              return std::filesystem::last_write_time(a) >
                     std::filesystem::last_write_time(b);
            });

  return logs.front();
}

bool CrashHandler::HasUnacknowledgedCrashLog() {
  auto crash_dir = GetCrashLogDirectory();
  auto ack_file = crash_dir / ".acknowledged";

  auto most_recent = GetMostRecentCrashLog();
  if (most_recent.empty()) {
    return false;
  }

  // Check if there's an acknowledgment file newer than the crash log
  if (std::filesystem::exists(ack_file)) {
    auto ack_time = std::filesystem::last_write_time(ack_file);
    auto crash_time = std::filesystem::last_write_time(most_recent);
    return crash_time > ack_time;
  }

  return true;
}

void CrashHandler::AcknowledgeCrashLog() {
  auto crash_dir = GetCrashLogDirectory();
  auto ack_file = crash_dir / ".acknowledged";

  // Create/update the acknowledgment file
  std::ofstream ack(ack_file);
  ack << "acknowledged";
}

void CrashHandler::CleanupOldLogs(int keep_count) {
  auto crash_dir = GetCrashLogDirectory();

  if (!std::filesystem::exists(crash_dir)) {
    return;
  }

  std::vector<std::filesystem::path> logs;
  std::vector<std::filesystem::path> header_only_logs;
  for (const auto& entry : std::filesystem::directory_iterator(crash_dir)) {
    if (entry.path().extension() == ".log" &&
        entry.path().filename().string().starts_with("crash_")) {
      if (IsHeaderOnlyCrashLog(entry.path())) {
        header_only_logs.push_back(entry.path());
      } else {
        logs.push_back(entry.path());
      }
    }
  }

  for (const auto& path : header_only_logs) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
  }

  if (static_cast<int>(logs.size()) <= keep_count) {
    return;
  }

  // Sort by modification time, newest first
  std::sort(logs.begin(), logs.end(),
            [](const std::filesystem::path& a, const std::filesystem::path& b) {
              return std::filesystem::last_write_time(a) >
                     std::filesystem::last_write_time(b);
            });

  // Remove older logs
  for (size_t i = keep_count; i < logs.size(); ++i) {
    std::error_code ec;
    std::filesystem::remove(logs[i], ec);
  }
}

}  // namespace util
}  // namespace yaze
