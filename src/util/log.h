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

static const std::string kLogFileOut = "yaze_log.txt";

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
  static std::ofstream fout(kLogFileOut, std::ios::out | std::ios::app);
  fout << message;
}

}  // namespace util
}  // namespace yaze

#endif  // YAZE_UTIL_LOG_H