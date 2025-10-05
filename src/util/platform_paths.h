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
   * @brief Get application data directory for YAZE
   * 
   * Creates the directory if it doesn't exist.
   * 
   * - Windows: %USERPROFILE%\.yaze
   * - Unix/macOS: $HOME/.yaze
   * 
   * @return StatusOr with path to data directory
   */
  static absl::StatusOr<std::filesystem::path> GetAppDataDirectory();
  
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
};

}  // namespace util
}  // namespace yaze

#endif  // YAZE_UTIL_PLATFORM_PATHS_H_
