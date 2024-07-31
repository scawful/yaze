#ifndef YAZE_APP_CORE_PROJECT_H
#define YAZE_APP_CORE_PROJECT_H

#include "absl/strings/match.h"

#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "app/core/common.h"

namespace yaze {
namespace app {

constexpr absl::string_view kProjectFileExtension = ".yaze";
constexpr absl::string_view kProjectFileFilter =
    "Yaze Project Files (*.yaze)\0*.yaze\0";
constexpr absl::string_view kPreviousRomFilenameDelimiter =
    "PreviousRomFilename";
constexpr absl::string_view kEndOfProjectFile = "EndOfProjectFile";

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

  absl::Status Open(const std::string &project_path) {
    filepath = project_path;
    name = project_path.substr(project_path.find_last_of("/") + 1);

    std::ifstream in(project_path);

    if (!in.good()) {
      return absl::InternalError("Could not open project file.");
    }

    std::string line;
    std::getline(in, name);
    std::getline(in, filepath);
    std::getline(in, rom_filename_);
    std::getline(in, code_folder_);
    std::getline(in, labels_filename_);

    while (std::getline(in, line)) {
      if (line == kEndOfProjectFile) {
        break;
      }

      if (absl::StrContains(line, kPreviousRomFilenameDelimiter)) {
        previous_rom_filenames_.push_back(
            line.substr(line.find(kPreviousRomFilenameDelimiter) +
                        kPreviousRomFilenameDelimiter.size() + 1));
      }
    }

    in.close();

    return absl::OkStatus();
  }

  absl::Status Save() {
    RETURN_IF_ERROR(CheckForEmptyFields());

    std::ofstream out(filepath + "/" + name + ".yaze");
    if (!out.good()) {
      return absl::InternalError("Could not open project file.");
    }

    out << name << std::endl;
    out << filepath << std::endl;
    out << rom_filename_ << std::endl;
    out << code_folder_ << std::endl;
    out << labels_filename_ << std::endl;

    for (const auto &filename : previous_rom_filenames_) {
      out << kPreviousRomFilenameDelimiter << " " << filename << std::endl;
    }

    out << kEndOfProjectFile << std::endl;

    out.close();

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

  absl::Status SerializeExperimentFlags() {
    auto flags = mutable_flags();
    // TODO: Serialize flags
    return absl::OkStatus();
  }

  bool project_opened_ = false;
  std::string name;
  std::string filepath;
  std::string rom_filename_ = "";
  std::string code_folder_ = "";
  std::string labels_filename_ = "";
  std::vector<std::string> previous_rom_filenames_;
};

}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_CORE_PROJECT_H
