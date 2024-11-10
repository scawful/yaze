#include "project.h"

namespace yaze {
namespace app {

absl::Status Project::Open(const std::string &project_path) {
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
  std::getline(in, keybindings_file);

  while (std::getline(in, line)) {
    if (line == kEndOfProjectFile) {
      break;
    }
  }

  in.close();

  return absl::OkStatus();
}

absl::Status Project::Save() {
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
  out << keybindings_file << std::endl;

  out << kEndOfProjectFile << std::endl;

  out.close();

  return absl::OkStatus();
}

}  // namespace app
}  // namespace yaze
