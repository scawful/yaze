#ifndef YAZE_APP_CORE_UTILS_FILE_UTIL_H
#define YAZE_APP_CORE_UTILS_FILE_UTIL_H

#include <string>

namespace yaze {
namespace core {

enum class Platform { kUnknown, kMacOS, kiOS, kWindows, kLinux };

std::string GetFileExtension(const std::string &filename);
std::string GetFileName(const std::string &filename);
std::string LoadFile(const std::string &filename);
std::string LoadConfigFile(const std::string &filename);
std::string GetConfigDirectory(Platform platform);

void SaveFile(const std::string &filename, const std::string &data,
              Platform platform);

}  // namespace core
}  // namespace yaze

#endif  // YAZE_APP_CORE_UTILS_FILE_UTIL_H
