#ifndef YAZE_APP_EDITOR_PLANNING_PLANNING_WIDGETS_H_
#define YAZE_APP_EDITOR_PLANNING_PLANNING_WIDGETS_H_

#include <cstddef>
#include <string>

#include "app/gui/core/ui_helpers.h"
#include "imgui/imgui.h"

namespace yaze::editor::planning {

// Combo over fixed status names; a value outside the list is shown as is.
template <size_t N>
bool StatusCombo(const char* label, std::string* value,
                 const char* const (&options)[N]) {
  bool changed = false;
  if (ImGui::BeginCombo(label, value->empty() ? "-" : value->c_str())) {
    for (const char* option : options) {
      const bool selected = *value == option;
      if (ImGui::Selectable(option, selected)) {
        *value = option;
        changed = true;
      }
    }
    ImGui::EndCombo();
  }
  return changed;
}

inline ImVec4 StatusColor(const std::string& status) {
  if (status == "implemented" || status == "done" || status == "decided") {
    return gui::GetSuccessColor();
  }
  if (status == "prototype" || status == "active") {
    return gui::GetInfoColor();
  }
  if (status == "open") {
    return gui::GetWarningColor();
  }
  return gui::GetDisabledColor();
}

}  // namespace yaze::editor::planning

#endif  // YAZE_APP_EDITOR_PLANNING_PLANNING_WIDGETS_H_
