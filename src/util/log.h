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
#include <utility>

#include "absl/strings/str_format.h"
#include "absl/strings/str_cat.h"
#include "app/core/features.h"
#include "absl/strings/string_view.h"

namespace yaze {
namespace util {

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

template <typename... Args>
inline void logf(const absl::FormatSpec<Args...>& format, Args&&... args) {
  LogManager::instance().log(LogLevel::INFO, "General",
                             absl::StrFormat(format, std::forward<Args>(args)...));
}

inline void logf(absl::string_view message) {
  LogManager::instance().log(LogLevel::INFO, "General", message);
}

}  // namespace util
}  // namespace yaze

#endif  // YAZE_UTIL_LOG_H
