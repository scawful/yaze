#include "util/platform_paths.h"

#include <cstdlib>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"

#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
#else
#include <pwd.h>
#include <unistd.h>
#include <climits>  // For PATH_MAX
#ifdef __APPLE__
#include <mach-o/dyld.h>  // For _NSGetExecutablePath
#endif
#endif

namespace yaze {
namespace util {

std::filesystem::path PlatformPaths::GetHomeDirectory() {
  try {
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
      return std::filesystem::path(std::string(homedrive) +
                                   std::string(homepath));
    }

    // Last resort: use temp directory
    std::error_code ec;
    auto temp = std::filesystem::temp_directory_path(ec);
    if (!ec) {
      return temp;
    }
    return std::filesystem::path(".");
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

    // Last resort: current directory (with error handling)
    std::error_code ec;
    auto cwd = std::filesystem::current_path(ec);
    if (!ec) {
      return cwd;
    }
    return std::filesystem::path(".");
#endif
  } catch (...) {
    // If everything fails, return current directory placeholder
    return std::filesystem::path(".");
  }
}

absl::StatusOr<std::filesystem::path> PlatformPaths::GetAppDataDirectory() {
#ifdef _WIN32
  wchar_t path[MAX_PATH];
  if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path))) {
    std::filesystem::path app_data = std::filesystem::path(path) / "yaze";
    auto status = EnsureDirectoryExists(app_data);
    if (!status.ok()) {
      return status;
    }
    return app_data;
  }
  // Fallback if SHGetFolderPathW fails
  std::filesystem::path home = GetHomeDirectory();
  std::filesystem::path app_data = home / "yaze_data";
  auto status = EnsureDirectoryExists(app_data);
  if (!status.ok()) {
    return status;
  }
  return app_data;
#else
  // Unix/macOS: Use ~/.yaze for simplicity and consistency
  // This is simpler than XDG or Application Support for a dev tool
  std::filesystem::path home = GetHomeDirectory();
  std::filesystem::path app_data = home / ".yaze";
  auto status = EnsureDirectoryExists(app_data);
  if (!status.ok()) {
    return status;
  }
  return app_data;
#endif
}

absl::StatusOr<std::filesystem::path> PlatformPaths::GetConfigDirectory() {
  // For yaze, config and data directories are the same.
  // This provides a semantically clearer API for config access.
  return GetAppDataDirectory();
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

absl::StatusOr<std::filesystem::path> PlatformPaths::FindAsset(
    const std::string& relative_path) {
  std::vector<std::filesystem::path> search_paths;

  try {
    // 1. Check compile-time YAZE_ASSETS_PATH if defined
#ifdef YAZE_ASSETS_PATH
    try {
      search_paths.push_back(std::filesystem::path(YAZE_ASSETS_PATH) /
                             relative_path);
    } catch (...) {
      // Skip if path construction fails
    }
#endif

    // 2. Current working directory + assets/ (cached to avoid repeated calls)
    static std::filesystem::path cached_cwd;
    static bool cwd_cached = false;

    if (!cwd_cached) {
      std::error_code ec;
      cached_cwd = std::filesystem::current_path(ec);
      cwd_cached = true;  // Only try once to avoid repeated slow calls
    }

    if (!cached_cwd.empty()) {
      try {
        search_paths.push_back(cached_cwd / "assets" / relative_path);
      } catch (...) {
        // Skip if path construction fails
      }
    }

    // 3. Executable directory + assets/ (cached to avoid repeated OS calls)
    static std::filesystem::path cached_exe_dir;
    static bool exe_dir_cached = false;

    if (!exe_dir_cached) {
      try {
#ifdef __APPLE__
        char exe_path[PATH_MAX];
        uint32_t size = sizeof(exe_path);
        if (_NSGetExecutablePath(exe_path, &size) == 0) {
          cached_exe_dir = std::filesystem::path(exe_path).parent_path();
        }
#elif defined(__linux__)
        char exe_path[PATH_MAX];
        ssize_t len =
            readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
        if (len != -1) {
          exe_path[len] = '\0';
          cached_exe_dir = std::filesystem::path(exe_path).parent_path();
        }
#elif defined(_WIN32)
        wchar_t exe_path[MAX_PATH];
        if (GetModuleFileNameW(NULL, exe_path, MAX_PATH) != 0) {
          cached_exe_dir = std::filesystem::path(exe_path).parent_path();
        }
#endif
      } catch (...) {
        // Skip if exe path detection fails
      }
      exe_dir_cached = true;  // Only try once
    }

    if (!cached_exe_dir.empty()) {
      try {
        search_paths.push_back(cached_exe_dir / "assets" / relative_path);
        // Also check parent (for build/bin/yaze case)
        search_paths.push_back(cached_exe_dir.parent_path() / "assets" /
                               relative_path);
      } catch (...) {
        // Skip if path construction fails
      }
    }

    // 4. Parent directories (for running from build subdirectories)
    if (!cached_cwd.empty()) {
      try {
        auto parent = cached_cwd.parent_path();
        if (!parent.empty() && parent != cached_cwd) {
          search_paths.push_back(parent / "assets" / relative_path);
          auto grandparent = parent.parent_path();
          if (!grandparent.empty() && grandparent != parent) {
            search_paths.push_back(grandparent / "assets" / relative_path);
          }
        }
      } catch (...) {
        // Skip if path operations fail
      }
    }

    // 5. User directory ~/.yaze/assets/ (cached home directory)
    static std::filesystem::path cached_home;
    static bool home_cached = false;

    if (!home_cached) {
      try {
        cached_home = GetHomeDirectory();
      } catch (...) {
        // Skip if home lookup fails
      }
      home_cached = true;  // Only try once
    }

    if (!cached_home.empty() && cached_home != ".") {
      try {
        search_paths.push_back(cached_home / ".yaze" / "assets" /
                               relative_path);
      } catch (...) {
        // Skip if path construction fails
      }
    }

    // 6. System-wide installation (Unix only)
#ifndef _WIN32
    try {
      search_paths.push_back(
          std::filesystem::path("/usr/local/share/yaze/assets") /
          relative_path);
      search_paths.push_back(std::filesystem::path("/usr/share/yaze/assets") /
                             relative_path);
    } catch (...) {
      // Skip if path construction fails
    }
#endif

    // Search all paths and return the first one that exists
    // Limit search to prevent infinite loops on weird filesystems
    const size_t max_paths_to_check = 20;
    size_t checked = 0;

    for (const auto& candidate : search_paths) {
      if (++checked > max_paths_to_check) {
        break;  // Safety limit
      }

      try {
        // Use std::filesystem::exists with error code to avoid exceptions
        std::error_code exists_ec;
        if (std::filesystem::exists(candidate, exists_ec) && !exists_ec) {
          // Double-check it's a regular file or directory, not a broken symlink
          auto status = std::filesystem::status(candidate, exists_ec);
          if (!exists_ec &&
              status.type() != std::filesystem::file_type::not_found) {
            return candidate;
          }
        }
      } catch (...) {
        // Skip this candidate if checking fails
        continue;
      }
    }

    // If not found, return a simple error
    return absl::NotFoundError(
        absl::StrCat("Asset not found: ", relative_path));

  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrCat("Exception while searching for asset: ", e.what()));
  } catch (...) {
    return absl::InternalError("Unknown exception while searching for asset");
  }
}

}  // namespace util
}  // namespace yaze
