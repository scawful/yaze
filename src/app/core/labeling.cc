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
    return false;
  }

  std::string line;
  while (std::getline(file, line)) {
    std::istringstream iss(line);
    std::string type, key, value;
    if (std::getline(iss, type, ',') && std::getline(iss, key, ',') &&
        std::getline(iss, value)) {
      labels_[type][key] = value;
    }
  }
  return true;
}

void ResourceLabelManager::DisplayLabels() {
  if (ImGui::Begin("Resource Labels")) {
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
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
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

  // ImGui::PopID();
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
