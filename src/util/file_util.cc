#include "util/file_util.h"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "core/features.h"
#include "util/platform_paths.h"

namespace yaze {
namespace util {

namespace fs = std::filesystem;

namespace {
std::mutex save_dialog_test_mutex;
ScopedSaveFileDialogResultForTesting* save_dialog_test_scope = nullptr;
}  // namespace

ScopedSaveFileDialogResultForTesting::ScopedSaveFileDialogResultForTesting(
    std::string result)
    : result_(std::move(result)) {
  std::lock_guard<std::mutex> lock(save_dialog_test_mutex);
  if (save_dialog_test_scope != nullptr) {
    throw std::logic_error("Save dialog test scope is already active");
  }
  save_dialog_test_scope = this;
}

ScopedSaveFileDialogResultForTesting::~ScopedSaveFileDialogResultForTesting() {
  std::lock_guard<std::mutex> lock(save_dialog_test_mutex);
  save_dialog_test_scope = nullptr;
}

bool ScopedSaveFileDialogResultForTesting::consumed() const {
  std::lock_guard<std::mutex> lock(save_dialog_test_mutex);
  return consumed_;
}

std::optional<std::string>
FileDialogWrapper::ConsumeSaveFileDialogResultForTesting() {
  std::lock_guard<std::mutex> lock(save_dialog_test_mutex);
  if (!save_dialog_test_scope || !save_dialog_test_scope->result_) {
    return std::nullopt;
  }
  auto result = std::move(save_dialog_test_scope->result_);
  save_dialog_test_scope->result_.reset();
  save_dialog_test_scope->consumed_ = true;
  return result;
}

std::string GetFileExtension(const std::string& filename) {
  return fs::path(filename).extension().string();
}

std::string GetFileName(const std::string& filename) {
  return fs::path(filename).filename().string();
}

std::string LoadFile(const std::string& filename) {
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

std::string LoadFileFromConfigDir(const std::string& filename) {
  auto config_dir = PlatformPaths::GetConfigDirectory();
  if (!config_dir.ok()) {
    return "";  // Or handle error appropriately
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

void SaveFile(const std::string& filename, const std::string& contents) {
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

std::string GetResourcePath(const std::string& resource_path) {
#ifdef __APPLE__
#if TARGET_OS_IOS == 1
  const std::string kBundlePath = GetBundleResourcePath();
  if (resource_path.rfind("assets/", 0) == 0) {
    return kBundlePath + resource_path;
  }
  return kBundlePath + "assets/" + resource_path;
#else
  return GetBundleResourcePath() + "Contents/Resources/" + resource_path;
#endif
#else
  return resource_path;  // On Linux/Windows, resources are relative to
                         // executable
#endif
}

FileDialogOptions MakeRomFileDialogOptions(bool include_all_files) {
  FileDialogOptions options;
  options.filters.push_back(
      {"ROM/Project Files", "sfc,smc,yaze,yazeproj,zsproj"});
  options.filters.push_back({"ROM Files", "sfc,smc"});
  options.filters.push_back({"Project Files", "yaze,yazeproj,zsproj"});
  if (include_all_files) {
    options.filters.push_back({"All Files", "*"});
  }
  return options;
}

// Note: FileDialogWrapper implementations are in
// src/app/platform/file_dialog.mm (platform-specific implementations to avoid
// duplicate symbols)

}  // namespace util
}  // namespace yaze
