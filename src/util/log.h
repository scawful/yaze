#ifndef YAZE_UTIL_LOG_H
#define YAZE_UTIL_LOG_H

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "app/core/features.h"

namespace yaze {
namespace util {

static std::string g_log_file_path = "yaze_log.txt";

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

}  // namespace util
}  // namespace yaze

#endif  // YAZE_UTIL_LOG_H