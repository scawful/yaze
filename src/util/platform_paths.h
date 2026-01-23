#ifndef YAZE_UTIL_PLATFORM_PATHS_H_
#define YAZE_UTIL_PLATFORM_PATHS_H_

#include <filesystem>
#include <string>

#include "absl/status/statusor.h"

namespace yaze {
namespace util {

/**
 * @brief Cross-platform utilities for file system paths
 *
 * Provides consistent, platform-independent file and directory operations
 * that work correctly on Windows (MSVC), macOS, and Linux.
 */
class PlatformPaths {
 public:
  /**
   * @brief Get the user's home directory in a cross-platform way
   *
   * - Windows: Uses USERPROFILE environment variable
   * - Unix/macOS: Uses HOME environment variable
   * - Fallback: Returns current directory
   *
   * @return Path to user's home directory, or "." if not available
   */
  static std::filesystem::path GetHomeDirectory();

  /**
   * @brief Get the user-specific application data directory for YAZE.
   *
   * This is the standard location for storing user-specific data, settings,
   * and cache files. The directory is created if it doesn't exist.
   *
   * - Windows/macOS/Linux: `~/.yaze` (user home, cross-platform consistency)
   * - Legacy locations (AppData/Library/XDG) are migrated or reused if present.
   *
   * @return StatusOr with path to the application data directory.
   */
  static absl::StatusOr<std::filesystem::path> GetAppDataDirectory();

  /**
   * @brief Get the user-specific configuration directory for YAZE.
   *
   * This is the standard location for storing user configuration files.
   * The directory is created if it doesn't exist.
   *
   * - Windows/macOS/Linux: `~/.yaze` (user home, cross-platform consistency)
   * - Legacy locations (AppData/Library/XDG) are migrated or reused if present.
   *
   * @return StatusOr with path to the configuration directory.
   */
  static absl::StatusOr<std::filesystem::path> GetConfigDirectory();

  /**
   * @brief Get the ImGui ini path for YAZE.
   *
   * This ensures imgui.ini lives in the app config directory rather than the
   * current working directory.
   *
   * @return StatusOr with path to imgui.ini
   */
  static absl::StatusOr<std::filesystem::path> GetImGuiIniPath();

  /**
   * @brief Get the user's Documents directory.
   *
   * This is a visible, user-facing directory for storing projects, logs,
   * and resources the user might want to access or share.
   *
   * - Windows: `My Documents\Yaze`
   * - macOS/Linux: `~/Documents/Yaze`
   *
   * @return StatusOr with path to the Yaze documents directory.
   */
  static absl::StatusOr<std::filesystem::path> GetUserDocumentsDirectory();

  /**
   * @brief Get a subdirectory within the user documents folder.
   *
   * Creates the directory if it doesn't exist.
   *
   * @param subdir Subdirectory name (e.g., "logs", "agent")
   * @return StatusOr with path to subdirectory
   */
  static absl::StatusOr<std::filesystem::path> GetUserDocumentsSubdirectory(
      const std::string& subdir);

  /**
   * @brief Get a subdirectory within the app data folder
   *
   * Creates the directory if it doesn't exist.
   *
   * @param subdir Subdirectory name (e.g., "agent", "cache", "logs")
   * @return StatusOr with path to subdirectory
   */
  static absl::StatusOr<std::filesystem::path> GetAppDataSubdirectory(
      const std::string& subdir);

  /**
   * @brief Ensure a directory exists, creating it if necessary
   *
   * Uses std::filesystem::create_directories which works cross-platform.
   *
   * @param path Directory path to create
   * @return OK if directory exists or was created successfully
   */
  static absl::Status EnsureDirectoryExists(const std::filesystem::path& path);

  /**
   * @brief Check if a file or directory exists
   *
   * @param path Path to check
   * @return true if exists, false otherwise
   */
  static bool Exists(const std::filesystem::path& path);

  /**
   * @brief Get a temporary directory for the application
   *
   * - Windows: %TEMP%\yaze
   * - Unix: /tmp/yaze or $TMPDIR/yaze
   *
   * @return StatusOr with path to temp directory
   */
  static absl::StatusOr<std::filesystem::path> GetTempDirectory();

  /**
   * @brief Normalize path separators for display
   *
   * Converts all path separators to forward slashes for consistent
   * output in logs and UI (forward slashes work on all platforms).
   *
   * @param path Path to normalize
   * @return Normalized path string
   */
  static std::string NormalizePathForDisplay(const std::filesystem::path& path);

  /**
   * @brief Convert path to native format
   *
   * Ensures path uses the correct separator for the current platform.
   *
   * @param path Path to convert
   * @return Native path string
   */
  static std::string ToNativePath(const std::filesystem::path& path);

  /**
   * @brief Find an asset file in multiple standard locations
   *
   * Searches for an asset file in the following order:
   * 1. YAZE_ASSETS_PATH (if defined at compile time) + relative_path
   * 2. Current working directory + assets/ + relative_path
   * 3. Executable directory + assets/ + relative_path
   * 4. Parent directory + assets/ + relative_path
   * 5. ~/.yaze/assets/ + relative_path (user-installed assets)
   * 6. /usr/local/share/yaze/assets/ + relative_path (system-wide on Unix)
   *
   * @param relative_path Path relative to assets directory (e.g.,
   * "agent/prompt_catalogue.yaml")
   * @return StatusOr with absolute path to found asset file
   */
  static absl::StatusOr<std::filesystem::path> FindAsset(
      const std::string& relative_path);
};

}  // namespace util
}  // namespace yaze

#endif  // YAZE_UTIL_PLATFORM_PATHS_H_
