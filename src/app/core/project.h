#ifndef YAZE_APP_CORE_PROJECT_H
#define YAZE_APP_CORE_PROJECT_H

#include <fstream>
#include <string>

#include "absl/status/status.h"
#include "app/core/common.h"
#include "app/core/constants.h"

namespace yaze {
namespace app {

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
struct Project : public core::ExperimentFlags {
  /**
   * @brief Creates a new project.
   *
   * @param project_name The name of the project.
   * @param project_path The path to the project.
   * @return An absl::Status indicating the success or failure of the project
   * creation.
   */
  absl::Status Create(const std::string &project_name) {
    name = project_name;
    project_opened_ = true;
    return absl::OkStatus();
  }

  absl::Status Open(const std::string &project_path);
  absl::Status Save();

  absl::Status CheckForEmptyFields() {
    if (name.empty() || filepath.empty() || rom_filename_.empty() ||
        code_folder_.empty() || labels_filename_.empty()) {
      return absl::InvalidArgumentError(
          "Project fields cannot be empty. Please load a rom file, set your "
          "code folder, and set your labels file. See HELP for more details.");
    }

    return absl::OkStatus();
  }

  bool project_opened_ = false;
  std::string name;
  std::string flags = "";
  std::string filepath;
  std::string rom_filename_ = "";
  std::string code_folder_ = "";
  std::string labels_filename_ = "";
  std::string keybindings_file = "";
};

}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_CORE_PROJECT_H
