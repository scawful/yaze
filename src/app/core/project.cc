#include "project.h"

#include <fstream>
#include <string>

#include "app/core/constants.h"
#include "app/gui/icons.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

namespace yaze {
namespace app {

absl::Status Project::Open(const std::string& project_path) {
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

bool ResourceLabelManager::LoadLabels(const std::string& filename) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    // Create the file if it does not exist
    std::ofstream create_file(filename);
    if (!create_file.is_open()) {
      return false;
    }
    create_file.close();
    file.open(filename);
    if (!file.is_open()) {
      return false;
    }
  }
  filename_ = filename;

  std::string line;
  while (std::getline(file, line)) {
    std::istringstream iss(line);
    std::string type, key, value;
    if (std::getline(iss, type, ',') && std::getline(iss, key, ',') &&
        std::getline(iss, value)) {
      labels_[type][key] = value;
    }
  }
  labels_loaded_ = true;
  return true;
}

bool ResourceLabelManager::SaveLabels() {
  if (!labels_loaded_) {
    return false;
  }
  std::ofstream file(filename_);
  if (!file.is_open()) {
    return false;
  }
  for (const auto& type_pair : labels_) {
    for (const auto& label_pair : type_pair.second) {
      file << type_pair.first << "," << label_pair.first << ","
           << label_pair.second << std::endl;
    }
  }
  file.close();
  return true;
}

void ResourceLabelManager::DisplayLabels(bool* p_open) {
  if (!labels_loaded_) {
    ImGui::Text("No labels loaded.");
    return;
  }

  if (ImGui::Begin("Resource Labels", p_open)) {
    for (const auto& type_pair : labels_) {
      if (ImGui::TreeNode(type_pair.first.c_str())) {
        for (const auto& label_pair : type_pair.second) {
          std::string label_id = type_pair.first + "_" + label_pair.first;
          ImGui::Text("%s: %s", label_pair.first.c_str(),
                      label_pair.second.c_str());
        }
        ImGui::TreePop();
      }
    }

    if (ImGui::Button("Update Labels")) {
      if (SaveLabels()) {
        ImGui::Text("Labels updated successfully!");
      } else {
        ImGui::Text("Failed to update labels.");
      }
    }
  }
  ImGui::End();
}

void ResourceLabelManager::EditLabel(const std::string& type,
                                     const std::string& key,
                                     const std::string& newValue) {
  labels_[type][key] = newValue;
}

void ResourceLabelManager::SelectableLabelWithNameEdit(
    bool selected, const std::string& type, const std::string& key,
    const std::string& defaultValue) {
  std::string label = CreateOrGetLabel(type, key, defaultValue);
  ImGui::Selectable(label.c_str(), selected,
                    ImGuiSelectableFlags_AllowDoubleClick);
  std::string label_id = type + "_" + key;
  if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    ImGui::OpenPopup(label_id.c_str());
  }

  if (ImGui::BeginPopupContextItem(label_id.c_str())) {
    std::string* new_label = &labels_[type][key];
    if (ImGui::InputText("##Label", new_label,
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
      labels_[type][key] = *new_label;
    }
    if (ImGui::Button(ICON_MD_CLOSE)) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

std::string ResourceLabelManager::GetLabel(const std::string& type,
                                           const std::string& key) {
  return labels_[type][key];
}

std::string ResourceLabelManager::CreateOrGetLabel(
    const std::string& type, const std::string& key,
    const std::string& defaultValue) {
  if (labels_.find(type) == labels_.end()) {
    labels_[type] = std::unordered_map<std::string, std::string>();
  }
  if (labels_[type].find(key) == labels_[type].end()) {
    labels_[type][key] = defaultValue;
  }
  return labels_[type][key];
}

}  // namespace app
}  // namespace yaze
