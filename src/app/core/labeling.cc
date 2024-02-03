#include "app/core/labeling.h"

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "app/core/common.h"
#include "app/core/constants.h"

namespace yaze {
namespace app {
namespace core {

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
    char* new_label = labels_[type][key].data();
    if (ImGui::InputText("##Label", new_label, labels_[type][key].size() + 1,
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
      labels_[type][key] = new_label;
    }
    ImGui::EndPopup();
  }
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

}  // namespace core
}  // namespace app
}  // namespace yaze
