#include "util/platform_paths.h"

#include <cstdlib>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <pwd.h>
#endif

namespace yaze {
namespace util {

std::filesystem::path PlatformPaths::GetHomeDirectory() {
#ifdef _WIN32
  // Windows: Use USERPROFILE environment variable
  const char* userprofile = std::getenv("USERPROFILE");
  if (userprofile && *userprofile) {
    return std::filesystem::path(userprofile);
  }
  
  // Fallback to HOMEDRIVE + HOMEPATH
  const char* homedrive = std::getenv("HOMEDRIVE");
  const char* homepath = std::getenv("HOMEPATH");
  if (homedrive && homepath) {
    return std::filesystem::path(std::string(homedrive) + std::string(homepath));
  }
  
  // Last resort: use temp directory
  return std::filesystem::temp_directory_path();
#else
  // Unix/macOS: Use HOME environment variable
  const char* home = std::getenv("HOME");
  if (home && *home) {
    return std::filesystem::path(home);
  }
  
  // Fallback: try getpwuid
  struct passwd* pw = getpwuid(getuid());
  if (pw && pw->pw_dir) {
    return std::filesystem::path(pw->pw_dir);
  }
  
  // Last resort: current directory
  return std::filesystem::current_path();
#endif
}

absl::StatusOr<std::filesystem::path> PlatformPaths::GetAppDataDirectory() {
  std::filesystem::path home = GetHomeDirectory();
  std::filesystem::path app_data = home / ".yaze";
  
  auto status = EnsureDirectoryExists(app_data);
  if (!status.ok()) {
    return status;
  }
  
  return app_data;
}

absl::StatusOr<std::filesystem::path> PlatformPaths::GetAppDataSubdirectory(
    const std::string& subdir) {
  auto app_data_result = GetAppDataDirectory();
  if (!app_data_result.ok()) {
    return app_data_result.status();
  }
  
  std::filesystem::path subdir_path = *app_data_result / subdir;
  
  auto status = EnsureDirectoryExists(subdir_path);
  if (!status.ok()) {
    return status;
  }
  
  return subdir_path;
}

absl::Status PlatformPaths::EnsureDirectoryExists(
    const std::filesystem::path& path) {
  if (Exists(path)) {
    return absl::OkStatus();
  }
  
  std::error_code ec;
  if (!std::filesystem::create_directories(path, ec)) {
    if (ec) {
      return absl::InternalError(
          absl::StrCat("Failed to create directory: ", path.string(),
                      " - Error: ", ec.message()));
    }
  }
  
  return absl::OkStatus();
}

bool PlatformPaths::Exists(const std::filesystem::path& path) {
  std::error_code ec;
  bool exists = std::filesystem::exists(path, ec);
  return exists && !ec;
}

absl::StatusOr<std::filesystem::path> PlatformPaths::GetTempDirectory() {
  std::error_code ec;
  std::filesystem::path temp_base = std::filesystem::temp_directory_path(ec);
  
  if (ec) {
    return absl::InternalError(
        absl::StrCat("Failed to get temp directory: ", ec.message()));
  }
  
  std::filesystem::path yaze_temp = temp_base / "yaze";
  
  auto status = EnsureDirectoryExists(yaze_temp);
  if (!status.ok()) {
    return status;
  }
  
  return yaze_temp;
}

std::string PlatformPaths::NormalizePathForDisplay(
    const std::filesystem::path& path) {
  // Convert to string and replace backslashes with forward slashes
  // Forward slashes work on all platforms for display purposes
  std::string path_str = path.string();
  return absl::StrReplaceAll(path_str, {{"\\", "/"}});
}

std::string PlatformPaths::ToNativePath(const std::filesystem::path& path) {
  // std::filesystem::path::string() already returns the native format
  return path.string();
}

}  // namespace util
}  // namespace yaze
