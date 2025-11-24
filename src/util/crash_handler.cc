#include "util/crash_handler.h"

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cstring>
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
#define open _open
#define O_WRONLY _O_WRONLY
#define O_CREAT _O_CREAT
#define O_TRUNC _O_TRUNC
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace yaze {
namespace util {

// Static member definitions
std::string CrashHandler::version_;
std::filesystem::path CrashHandler::crash_log_path_;
int CrashHandler::crash_log_fd_ = -1;

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
  
  if (crash_log_fd_ >= 0) {
    // Write to crash log file (ignore errors - we're crashing anyway)
    write(crash_log_fd_, data, len);
  }
  
  // Also write to stderr for immediate visibility
  write(STDERR_FILENO, data, len);
  
  in_crash_handler = 0;
}

void CrashHandler::Initialize(const std::string& version) {
  version_ = version;

  // Get or create crash log directory
  auto crash_dir = GetCrashLogDirectory();

  // Create timestamped crash log filename
  auto now = std::chrono::system_clock::now();
  auto time_t_now = std::chrono::system_clock::to_time_t(now);
  std::tm* tm_now = std::localtime(&time_t_now);

  std::ostringstream filename;
  filename << "crash_" << std::put_time(tm_now, "%Y%m%d_%H%M%S") << ".log";
  crash_log_path_ = crash_dir / filename.str();

  // Open crash log file (will be written to if a crash occurs)
  // Using low-level I/O because signal handlers can't use iostream
#ifdef _WIN32
  crash_log_fd_ =
      _open(crash_log_path_.string().c_str(), _O_WRONLY | _O_CREAT | _O_TRUNC,
            _S_IREAD | _S_IWRITE);
#else
  crash_log_fd_ = open(crash_log_path_.c_str(), O_WRONLY | O_CREAT | O_TRUNC,
                       S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
#endif

  if (crash_log_fd_ >= 0) {
    // Write header immediately
    std::ostringstream header;
    header << "=== YAZE Crash Report ===\n";
    header << "Version: " << version_ << "\n";
    header << "Timestamp: " << std::put_time(tm_now, "%Y-%m-%d %H:%M:%S") << "\n";
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
    header << "========================\n\n";

    std::string header_str = header.str();
    write(crash_log_fd_, header_str.c_str(), header_str.size());

    // Don't close the file - it stays open for the signal handler to write to
  }

  // Configure absl failure signal handler
  absl::FailureSignalHandlerOptions options;
  options.symbolize_stacktrace = true;
  options.use_alternate_stack = true;
  options.call_previous_handler = true;

  // Set custom writer to write to both file and stderr
  if (crash_log_fd_ >= 0) {
    options.writerfn = CrashLogWriter;
  }

  absl::InstallFailureSignalHandler(options);
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
        entry.path().filename().string().starts_with("crash_")) {
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
  for (const auto& entry : std::filesystem::directory_iterator(crash_dir)) {
    if (entry.path().extension() == ".log" &&
        entry.path().filename().string().starts_with("crash_")) {
      logs.push_back(entry.path());
    }
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
