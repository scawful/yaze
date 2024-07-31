#ifndef YAZE_APP_EDITOR_UTILS_RECENT_FILES_H
#define YAZE_APP_EDITOR_UTILS_RECENT_FILES_H

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

namespace yaze {
namespace app {
namespace editor {

class RecentFilesManager {
 public:
  RecentFilesManager(const std::string& filename) : filename_(filename) {}

  void AddFile(const std::string& filePath) {
    // Add a file to the list, avoiding duplicates
    auto it = std::find(recentFiles_.begin(), recentFiles_.end(), filePath);
    if (it == recentFiles_.end()) {
      recentFiles_.push_back(filePath);
    }
  }

  void Save() {
    std::ofstream file(filename_);
    if (!file.is_open()) {
      return;  // Handle the error appropriately
    }

    for (const auto& filePath : recentFiles_) {
      file << filePath << std::endl;
    }
  }

  void Load() {
    std::ifstream file(filename_);
    if (!file.is_open()) {
      return;  // Handle the error appropriately
    }

    recentFiles_.clear();
    std::string line;
    while (std::getline(file, line)) {
      if (!line.empty()) {
        recentFiles_.push_back(line);
      }
    }
  }

  const std::vector<std::string>& GetRecentFiles() const {
    return recentFiles_;
  }

 private:
  std::string filename_;
  std::vector<std::string> recentFiles_;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_UTILS_RECENT_FILES_H