#include "util/log.h"

#include <chrono>
#include <iomanip>
#include <iostream>

namespace yaze {
namespace util {

// Helper function to convert LogLevel enum to a string representation.
static const char* LogLevelToString(LogLevel level) {
  switch (level) {
    case LogLevel::YAZE_DEBUG:
      return "YAZE_DEBUG";
    case LogLevel::INFO:
      return "INFO";
    case LogLevel::WARNING:
      return "WARN";
    case LogLevel::ERROR:
      return "ERROR";
    case LogLevel::FATAL:
      return "FATAL";
  }
  return "UNKN";
}

// --- LogManager Implementation ---

LogManager& LogManager::instance() {
  static LogManager instance;
  return instance;
}

LogManager::LogManager()
    : min_level_(LogLevel::INFO), all_categories_enabled_(true) {}

LogManager::~LogManager() {
  if (log_stream_.is_open()) {
    log_stream_.close();
  }
}

void LogManager::configure(LogLevel level, const std::string& file_path,
                           const std::set<std::string>& categories) {
  min_level_.store(level);

  if (categories.empty()) {
    all_categories_enabled_.store(true);
    enabled_categories_.clear();
  } else {
    all_categories_enabled_.store(false);
    enabled_categories_ = categories;

    // Log which categories are enabled for debugging
    std::string category_list;
    for (const auto& cat : categories) {
      if (!category_list.empty())
        category_list += ", ";
      category_list += cat;
    }
    std::cerr << "Log categories filter enabled: [" << category_list << "]"
              << std::endl;
  }

  // If a file path is provided, close any existing stream and open the new file.
  if (!file_path.empty() && file_path != log_file_path_) {
    if (log_stream_.is_open()) {
      log_stream_.close();
    }
    // Open in append mode to preserve history.
    log_stream_.open(file_path, std::ios::out | std::ios::app);
    log_file_path_ = file_path;
  }
}

void LogManager::log(LogLevel level, absl::string_view category,
                     absl::string_view message) {
  // 1. Filter by log level.
  if (level < min_level_.load()) {
    return;
  }

  // 2. Filter by category.
  if (!all_categories_enabled_.load()) {
    if (enabled_categories_.find(std::string(category)) ==
        enabled_categories_.end()) {
      return;
    }
  }

  // 3. Format the complete log message.
  // [HH:MM:SS.ms] [LEVEL] [category] message
  auto now = std::chrono::system_clock::now();
  auto now_tt = std::chrono::system_clock::to_time_t(now);
  auto now_tm = *std::localtime(&now_tt);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) %
            1000;

  std::string final_message = absl::StrFormat(
      "[%02d:%02d:%02d.%03d] [%-5s] [%s] %s\n", now_tm.tm_hour, now_tm.tm_min,
      now_tm.tm_sec, ms.count(), LogLevelToString(level), category, message);

  // 4. Write to the configured sink (file or stderr).
  if (log_stream_.is_open()) {
    log_stream_ << final_message;
    log_stream_.flush();  // Ensure immediate write for debugging.
  } else {
    std::cerr << final_message;
  }

  // 5. Abort on FATAL error.
  if (level == LogLevel::FATAL) {
    std::abort();
  }
}

}  // namespace util
}  // namespace yaze
