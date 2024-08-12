#ifndef YAZE_APP_CORE_UTILS_FILE_UTIL_H
#define YAZE_APP_CORE_UTILS_FILE_UTIL_H

#include <string>

namespace yaze {
namespace app {
namespace core {

std::string GetFileExtension(const std::string& filename) {
  size_t dot = filename.find_last_of(".");
  if (dot == std::string::npos) {
    return "";
  }
  return filename.substr(dot + 1);
}

std::string GetFileName(const std::string& filename) {
  size_t slash = filename.find_last_of("/");
  if (slash == std::string::npos) {
    return filename;
  }
  return filename.substr(slash + 1);
}

std::string LoadFile(const std::string& filename);

}  // namespace core
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_CORE_UTILS_FILE_UTIL_H