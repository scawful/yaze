#ifndef YAZE_UTIL_LOG_H
#define YAZE_UTIL_LOG_H

#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/strings/str_cat.h"
#include "app/core/features.h"
#include "absl/strings/string_view.h"

namespace yaze {
namespace util {

// Static variables for library state
static std::string g_log_file_path = "";


// Set custom log file path
inline void SetLogFile(const std::string& filepath) {
  g_log_file_path = filepath;
}

template <typename... Args>
static void logf(const absl::FormatSpec<Args...> &format, const Args &...args) {
  std::string message = absl::StrFormat(format, args...);
  auto timestamp = std::chrono::system_clock::now();

  std::time_t now_tt = std::chrono::system_clock::to_time_t(timestamp);
  std::tm tm = *std::localtime(&now_tt);
  message = absl::StrCat("[", tm.tm_hour, ":", tm.tm_min, ":", tm.tm_sec, "] ",
                         message, "\n");

  if (core::FeatureFlags::get().kLogToConsole) {
    std::cout << message;
  }
  
  // Use the configurable log file path
  static std::ofstream fout;
  static std::string last_log_path = "";
  
  // Reopen file if path changed
  if (g_log_file_path != last_log_path) {
    fout.close();
    fout.open(g_log_file_path, std::ios::out | std::ios::trunc);
    last_log_path = g_log_file_path;
  }
  
  fout << message;
  fout.flush(); // Ensure immediate write for debugging
}


/**
 * @enum LogLevel
 * @brief Defines the severity levels for log messages.
 * This allows for filtering messages based on their importance.
 */
enum class LogLevel { YAZE_DEBUG, INFO, WARNING, ERROR, FATAL };

/**
 * @class LogManager
 * @brief A singleton that manages all logging configuration and output.
 *
 * It is designed to be configured once at application startup, typically from
 * command-line arguments. It supports filtering by level and category, and can
 * direct output to stderr (default) or a specified file.
 */
class LogManager {
 public:
  // Singleton access
  static LogManager& instance();

  // Deleted constructors for singleton pattern
  LogManager(const LogManager&) = delete;
  void operator=(const LogManager&) = delete;

  /**
   * @brief Configures the logging system.
   * @param level The minimum log level to record.
   * @param file_path The path to the log file. If empty, logs to stderr.
   * @param categories A set of specific categories to enable. If empty, all
   * categories are enabled.
   */
  void configure(LogLevel level, const std::string& file_path,
                 const std::set<std::string>& categories);

  /**
   * @brief The primary logging function.
   * @param level The severity level of the message.
   * @param category The category of the message (e.g., "Graphics", "Agent").
   * @param message The formatted log message.
   */
  void log(LogLevel level, absl::string_view category,
           absl::string_view message);

 private:
  LogManager();
  ~LogManager();

  // Configuration state
  std::atomic<LogLevel> min_level_;
  std::set<std::string> enabled_categories_;
  std::atomic<bool> all_categories_enabled_;

  // Output sink
  std::ofstream log_stream_;
  std::string log_file_path_;
};

// logf mapping
#define logf LOG_INFO

// --- Public Logging Macros ---
// These macros provide a convenient and efficient API for logging.
// The `do-while(0)` loop ensures they behave like a single statement.
// The level check avoids the cost of string formatting if the message won't be
// logged.

#define LOG(level, category, format, ...)                                  \
  do {                                                                     \
    yaze::util::LogManager::instance().log(                                \
        level, category, absl::StrFormat(format, ##__VA_ARGS__));           \
  } while (0)

#define LOG_DEBUG(category, format, ...)                                   \
  LOG(yaze::util::LogLevel::YAZE_DEBUG, category, format, ##__VA_ARGS__)
#define LOG_INFO(category, format, ...)                                    \
  LOG(yaze::util::LogLevel::INFO, category, format, ##__VA_ARGS__)
#define LOG_WARN(category, format, ...)                                    \
  LOG(yaze::util::LogLevel::WARNING, category, format, ##__VA_ARGS__)
#define LOG_ERROR(category, format, ...)                                   \
  LOG(yaze::util::LogLevel::ERROR, category, format, ##__VA_ARGS__)
#define LOG_FATAL(category, format, ...)                                   \
  LOG(yaze::util::LogLevel::FATAL, category, format, ##__VA_ARGS__)

}  // namespace util
}  // namespace yaze

#endif  // YAZE_UTIL_LOG_H
