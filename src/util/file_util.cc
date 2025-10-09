#include "util/file_util.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "app/core/features.h"
#include "util/platform_paths.h"

namespace yaze {
namespace util {

namespace fs = std::filesystem;

std::string GetFileExtension(const std::string &filename) {
  return fs::path(filename).extension().string();
}

std::string GetFileName(const std::string &filename) {
  return fs::path(filename).filename().string();
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

std::string LoadFileFromConfigDir(const std::string &filename) {
  auto config_dir = PlatformPaths::GetConfigDirectory();
  if (!config_dir.ok()) {
    return ""; // Or handle error appropriately
  }

  fs::path filepath = *config_dir / filename;
  std::string contents;
  std::ifstream file(filepath);
  if (file.is_open()) {
    std::stringstream buffer;
    buffer << file.rdbuf();
    contents = buffer.str();
    file.close();
  }
  return contents;
}

void SaveFile(const std::string &filename, const std::string &contents) {
  auto config_dir = PlatformPaths::GetConfigDirectory();
  if (!config_dir.ok()) {
    // Or handle error appropriately
    return;
  }
  fs::path filepath = *config_dir / filename;
  std::ofstream file(filepath);
  if (file.is_open()) {
    file << contents;
    file.close();
  }
}

std::string GetResourcePath(const std::string &resource_path) {
#ifdef __APPLE__
#if TARGET_OS_IOS == 1
  const std::string kBundlePath = GetBundleResourcePath();
  return kBundlePath + resource_path;
#else
  return GetBundleResourcePath() + "Contents/Resources/" + resource_path;
#endif
#else
  return resource_path;  // On Linux/Windows, resources are relative to executable
#endif
}

// Note: FileDialogWrapper implementations are in src/app/platform/file_dialog.mm
// (platform-specific implementations to avoid duplicate symbols)

}  // namespace util
}  // namespace yaze