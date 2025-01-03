#include "file_util.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#include <fstream>
#include <sstream>

namespace yaze {
namespace core {

std::string GetFileExtension(const std::string &filename) {
  size_t dot = filename.find_last_of(".");
  if (dot == std::string::npos) {
    return "";
  }
  return filename.substr(dot + 1);
}

std::string GetFileName(const std::string &filename) {
  size_t slash = filename.find_last_of("/");
  if (slash == std::string::npos) {
    return filename;
  }
  return filename.substr(slash + 1);
}

std::string LoadFile(const std::string &filename) {
  std::string contents;
  std::ifstream file(filename);
  if (file.is_open()) {
    std::stringstream buffer;
    buffer << file.rdbuf();
    contents = buffer.str();
    file.close();
  } else {
    // Throw an exception
    throw std::runtime_error("Could not open file: " + filename);
  }
  return contents;
}

std::string LoadConfigFile(const std::string &filename) {
  std::string contents;
  Platform platform;
#if defined(_WIN32)
  platform = Platform::kWindows;
#elif defined(__APPLE__)
  platform = Platform::kMacOS;
#else
  platform = Platform::kLinux;
#endif
  std::string filepath = GetConfigDirectory(platform) + "/" + filename;
  std::ifstream file(filepath);
  if (file.is_open()) {
    std::stringstream buffer;
    buffer << file.rdbuf();
    contents = buffer.str();
    file.close();
  }
  return contents;
}

void SaveFile(const std::string &filename, const std::string &contents,
              Platform platform) {
  std::string filepath = GetConfigDirectory(platform) + "/" + filename;
  std::ofstream file(filepath);
  if (file.is_open()) {
    file << contents;
    file.close();
  }
}

std::string GetConfigDirectory(Platform platform) {
  std::string config_directory = ".yaze";
  switch (platform) {
  case Platform::kWindows:
    config_directory = "~/AppData/Roaming/yaze";
    break;
  case Platform::kMacOS:
  case Platform::kLinux:
    config_directory = "~/.config/yaze";
    break;
  default:
    break;
  }
  return config_directory;
}

} // namespace core
} // namespace yaze
