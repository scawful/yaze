#ifndef YAZE_UTIL_CRASH_HANDLER_H
#define YAZE_UTIL_CRASH_HANDLER_H

#include <filesystem>
#include <string>

namespace yaze {
namespace util {

/**
 * @class CrashHandler
 * @brief Manages crash logging for release builds.
 *
 * This class sets up signal handlers to capture crashes and write
 * detailed crash reports to a log file in the user's data directory.
 * The crash report includes:
 * - Timestamp
 * - Signal information
 * - Stack trace (if available)
 * - Application version
 *
 * Usage:
 *   CrashHandler::Initialize("0.3.3");
 *
 * Crash logs are written to:
 * - Windows: %APPDATA%/yaze/crash_logs/
 * - macOS: ~/.yaze/crash_logs/
 * - Linux: ~/.yaze/crash_logs/
 *
 * The log file is created only when the signal handler first writes, so a
 * session that exits normally leaves no crash_*.log behind.
 */
class CrashHandler {
 public:
  /**
   * @brief Initialize the crash handler for the application.
   * @param version The application version string.
   *
   * This should be called early in main() after initializing the symbolizer.
   * In debug builds, this may be less aggressive to allow debugging.
   */
  static void Initialize(const std::string& version);

  /**
   * @brief Get the path where crash logs are stored.
   * @return The crash log directory path.
   */
  static std::filesystem::path GetCrashLogDirectory();

  /**
   * @brief Get the path to the most recent crash log, if any.
   * @return Path to the most recent crash log, or empty path if none exists.
   */
  static std::filesystem::path GetMostRecentCrashLog();

  /**
   * @brief Check if there's a crash log from a previous session.
   * @return True if a crash log exists that hasn't been acknowledged.
   */
  static bool HasUnacknowledgedCrashLog();

  /**
   * @brief Mark the current crash log as acknowledged.
   *
   * Call this after showing the user the crash report dialog.
   */
  static void AcknowledgeCrashLog();

  /**
   * @brief Clean up old crash logs, keeping only the most recent N logs.
   *
   * Header-only logs left by builds that created the file at startup are
   * removed regardless of count; they never recorded a crash.
   * @param keep_count Number of recent logs to keep (default: 5).
   */
  static void CleanupOldLogs(int keep_count = 5);

  /**
   * @brief True if the file holds only the report header, with nothing
   * written by the signal handler after it.
   */
  static bool IsHeaderOnlyCrashLog(const std::filesystem::path& path);

  // Testing hooks: prepare the crash log path and header for `directory`
  // without installing signal handlers, drive the signal-path writer, and
  // release the file afterwards.
  static void PrepareCrashLogForTesting(const std::string& version,
                                        const std::filesystem::path& directory);
  static void WriteCrashLogForTesting(const char* data);
  static std::filesystem::path PendingCrashLogPathForTesting();
  static void ResetForTesting();

 private:
  // Builds the log path and header in static buffers so the signal handler
  // can create the file without allocating.
  static void PrepareCrashLog(const std::string& version,
                              const std::filesystem::path& directory);

  // Opens the log and writes the header on the first crash write. Uses only
  // async-signal-safe calls.
  static void OpenCrashLogFromSignalHandler();

  // Custom writer function for absl failure signal handler
  static void CrashLogWriter(const char* data);

  static std::string version_;
  static std::filesystem::path crash_log_path_;
  static int crash_log_fd_;
};

}  // namespace util
}  // namespace yaze

#endif  // YAZE_UTIL_CRASH_HANDLER_H
