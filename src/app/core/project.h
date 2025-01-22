#ifndef YAZE_APP_CORE_PROJECT_H
#define YAZE_APP_CORE_PROJECT_H

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/core/platform/file_dialog.h"

namespace yaze {

const std::string kRecentFilesFilename = "recent_files.txt";
constexpr char kEndOfProjectFile[] = "EndOfProjectFile";

/**
 * @struct Project
 * @brief Represents a project in the application.
 *
 * A project is a collection of files and resources that are used in the
 * creation of a Zelda3 hack that can be saved and loaded. This makes it so the
 * user can have different rom file names for a single project and keep track of
 * backups.
 */
struct Project {
  absl::Status Create(const std::string& project_name) {
    name = project_name;
    project_opened_ = true;
    return absl::OkStatus();
  }
  absl::Status CheckForEmptyFields() {
    if (name.empty() || filepath.empty() || rom_filename_.empty() ||
        code_folder_.empty() || labels_filename_.empty()) {
      return absl::InvalidArgumentError(
          "Project fields cannot be empty. Please load a rom file, set your "
          "code folder, and set your labels file. See HELP for more details.");
    }

    return absl::OkStatus();
  }
  absl::Status Open(const std::string &project_path);
  absl::Status Save();

  bool project_opened_ = false;
  std::string name;
  std::string flags = "";
  std::string filepath;
  std::string rom_filename_ = "";
  std::string code_folder_ = "";
  std::string labels_filename_ = "";
  std::string keybindings_file = "";
};

// Default types
static constexpr absl::string_view kDefaultTypes[] = {
    "Dungeon Names", "Dungeon Room Names", "Overworld Map Names"};

struct ResourceLabelManager {
  bool LoadLabels(const std::string& filename);
  bool SaveLabels();
  void DisplayLabels(bool* p_open);
  void EditLabel(const std::string& type, const std::string& key,
                 const std::string& newValue);
  void SelectableLabelWithNameEdit(bool selected, const std::string& type,
                                   const std::string& key,
                                   const std::string& defaultValue);
  std::string GetLabel(const std::string& type, const std::string& key);
  std::string CreateOrGetLabel(const std::string& type, const std::string& key,
                               const std::string& defaultValue);

  bool labels_loaded_ = false;
  std::string filename_;
  struct ResourceType {
    std::string key_name;
    std::string display_description;
  };

  std::unordered_map<std::string, std::unordered_map<std::string, std::string>>
      labels_;
};

class RecentFilesManager {
 public:
  RecentFilesManager() : RecentFilesManager(kRecentFilesFilename) {}
  RecentFilesManager(const std::string& filename) : filename_(filename) {}

  void AddFile(const std::string& file_path) {
    // Add a file to the list, avoiding duplicates
    auto it = std::find(recent_files_.begin(), recent_files_.end(), file_path);
    if (it == recent_files_.end()) {
      recent_files_.push_back(file_path);
    }
  }

  void Save() {
    std::ofstream file(filename_);
    if (!file.is_open()) {
      return;  // Handle the error appropriately
    }

    for (const auto& file_path : recent_files_) {
      file << file_path << std::endl;
    }
  }

  void Load() {
    std::ifstream file(filename_);
    if (!file.is_open()) {
      return;
    }

    recent_files_.clear();
    std::string line;
    while (std::getline(file, line)) {
      if (!line.empty()) {
        recent_files_.push_back(line);
      }
    }
  }

  const std::vector<std::string>& GetRecentFiles() const {
    return recent_files_;
  }

 private:
  std::string filename_;
  std::vector<std::string> recent_files_;
};


}  // namespace yaze

#endif  // YAZE_APP_CORE_PROJECT_H
