#ifndef YAZE_APP_CORE_PLATFORM_ASSET_LOADER_H_
#define YAZE_APP_CORE_PLATFORM_ASSET_LOADER_H_

#include <filesystem>
#include <string>
#include <vector>

#include "absl/status/statusor.h"

namespace yaze {
namespace core {

/**
 * @class AssetLoader
 * @brief Cross-platform asset file loading utility
 * 
 * Handles platform-specific paths for loading assets from:
 * - macOS bundle resources
 * - Windows relative paths
 * - Linux relative paths
 * - Development build directories
 */
class AssetLoader {
 public:
  /**
   * Load a text file from the assets directory
   * @param relative_path Path relative to assets/ (e.g., "agent/system_prompt.txt")
   * @return File contents or error
   */
  static absl::StatusOr<std::string> LoadTextFile(const std::string& relative_path);
  
  /**
   * Find an asset file by trying multiple platform-specific paths
   * @param relative_path Path relative to assets/
   * @return Full path to file or error
   */
  static absl::StatusOr<std::filesystem::path> FindAssetFile(const std::string& relative_path);
  
  /**
   * Get list of search paths for a given asset
   * @param relative_path Path relative to assets/
   * @return Vector of paths to try in order
   */
  static std::vector<std::filesystem::path> GetSearchPaths(const std::string& relative_path);
  
  /**
   * Check if an asset file exists
   * @param relative_path Path relative to assets/
   * @return true if file exists in any search path
   */
  static bool AssetExists(const std::string& relative_path);
};

}  // namespace core
}  // namespace yaze

#endif  // YAZE_APP_CORE_PLATFORM_ASSET_LOADER_H_
