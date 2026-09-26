#include "app/editor/menu/recent_files_menu_model.h"

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <system_error>
#include <vector>

namespace yaze {
namespace editor {

namespace {

bool DefaultExists(const std::string& path) {
  std::error_code ec;
  return std::filesystem::exists(std::filesystem::path(path), ec) && !ec;
}

std::string FileNameOf(const std::string& path) {
  const std::string name = std::filesystem::path(path).filename().string();
  return name.empty() ? path : name;
}

std::string ParentNameOf(const std::string& path) {
  return std::filesystem::path(path).parent_path().filename().string();
}

}  // namespace

std::vector<RecentFileMenuEntry> BuildRecentFileMenuEntries(
    const std::vector<std::string>& recent_paths, size_t max_entries,
    const RecentFileExistsFn& exists) {
  std::vector<RecentFileMenuEntry> entries;
  std::set<std::string> seen_paths;
  for (const std::string& path : recent_paths) {
    if (entries.size() >= max_entries) {
      break;
    }
    if (path.empty() || !seen_paths.insert(path).second) {
      continue;
    }
    RecentFileMenuEntry entry;
    entry.path = path;
    entry.label = FileNameOf(path);
    entry.exists = exists ? exists(path) : DefaultExists(path);
    entries.push_back(std::move(entry));
  }

  // Disambiguate duplicate file names with the parent directory name.
  std::map<std::string, int> name_counts;
  for (const auto& entry : entries) {
    ++name_counts[entry.label];
  }
  for (auto& entry : entries) {
    if (name_counts[entry.label] > 1) {
      const std::string parent = ParentNameOf(entry.path);
      if (!parent.empty()) {
        entry.label = entry.label + " (" + parent + ")";
      }
    }
  }

  // Any label that still collides falls back to the full (unique) path.
  std::map<std::string, int> label_counts;
  for (const auto& entry : entries) {
    ++label_counts[entry.label];
  }
  for (auto& entry : entries) {
    if (label_counts[entry.label] > 1) {
      entry.label = entry.path;
    }
  }
  return entries;
}

}  // namespace editor
}  // namespace yaze
