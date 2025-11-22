#include "app/platform/asset_loader.h"

#include <fstream>
#include <sstream>

#include "absl/strings/str_format.h"
#include "util/file_util.h"

namespace yaze {

std::vector<std::filesystem::path> AssetLoader::GetSearchPaths(
    const std::string& relative_path) {
  std::vector<std::filesystem::path> search_paths;

#ifdef __APPLE__
  // macOS bundle resource paths
  std::string bundle_root = yaze::util::GetBundleResourcePath();

  // Try Contents/Resources first (standard bundle location)
  search_paths.push_back(std::filesystem::path(bundle_root) / "Contents" /
                         "Resources" / relative_path);

  // Try without Contents (if app is at root)
  search_paths.push_back(std::filesystem::path(bundle_root) / "Resources" /
                         relative_path);

  // Development paths (when running from build dir)
  search_paths.push_back(std::filesystem::path(bundle_root) / ".." / ".." /
                         ".." / "assets" / relative_path);
  search_paths.push_back(std::filesystem::path(bundle_root) / ".." / ".." /
                         ".." / ".." / "assets" / relative_path);
#endif

  // Standard relative paths (works for all platforms)
  search_paths.push_back(std::filesystem::path("assets") / relative_path);
  search_paths.push_back(std::filesystem::path("../assets") / relative_path);
  search_paths.push_back(std::filesystem::path("../../assets") / relative_path);
  search_paths.push_back(std::filesystem::path("../../../assets") /
                         relative_path);
  search_paths.push_back(std::filesystem::path("../../../../assets") /
                         relative_path);

  // Build directory paths
  search_paths.push_back(std::filesystem::path("build/assets") / relative_path);
  search_paths.push_back(std::filesystem::path("../build/assets") /
                         relative_path);

  return search_paths;
}

absl::StatusOr<std::filesystem::path> AssetLoader::FindAssetFile(
    const std::string& relative_path) {
  auto search_paths = GetSearchPaths(relative_path);

  for (const auto& path : search_paths) {
    if (std::filesystem::exists(path)) {
      return path;
    }
  }

  // Debug: Print searched paths
  std::string searched_paths;
  for (const auto& path : search_paths) {
    searched_paths += "\n  - " + path.string();
  }

  return absl::NotFoundError(
      absl::StrFormat("Asset file not found: %s\nSearched paths:%s",
                      relative_path, searched_paths));
}

absl::StatusOr<std::string> AssetLoader::LoadTextFile(
    const std::string& relative_path) {
  auto path_result = FindAssetFile(relative_path);
  if (!path_result.ok()) {
    return path_result.status();
  }

  const auto& path = *path_result;
  std::ifstream file(path);
  if (!file.is_open()) {
    return absl::InternalError(
        absl::StrFormat("Failed to open file: %s", path.string()));
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string content = buffer.str();

  if (content.empty()) {
    return absl::InternalError(
        absl::StrFormat("File is empty: %s", path.string()));
  }

  return content;
}

bool AssetLoader::AssetExists(const std::string& relative_path) {
  return FindAssetFile(relative_path).ok();
}

}  // namespace yaze
