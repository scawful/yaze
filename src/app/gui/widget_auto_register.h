#ifndef YAZE_APP_GUI_WIDGET_AUTO_REGISTER_H_
#define YAZE_APP_GUI_WIDGET_AUTO_REGISTER_H_

#include <string>

#include "imgui/imgui.h"
#include "app/gui/widget_id_registry.h"
#include "absl/strings/str_cat.h"

/**
 * @file widget_auto_register.h
 * @brief Automatic widget registration helpers for ImGui Test Engine integration
 *
 * This file provides inline wrappers and RAII helpers that automatically
 * register ImGui widgets with the WidgetIdRegistry for test automation.
 *
 * Usage:
 *   {
 *     gui::AutoWidgetScope scope("Dungeon/Canvas");
 *     if (gui::AutoButton("Save##DungeonSave")) {
 *       // Button clicked
 *     }
 *     gui::AutoInputText("RoomName", buffer, sizeof(buffer));
 *   }
 *
 * All widgets created within this scope will be automatically registered
 * with their full hierarchical paths for test harness discovery.
 */

namespace yaze {
namespace gui {

/**
 * @class AutoWidgetScope
 * @brief RAII scope that enables automatic widget registration
 *
 * Creates a widget ID scope and enables auto-registration for all widgets
 * created within this scope. Combines WidgetIdScope with automatic metadata
 * capture.
 */
class AutoWidgetScope {
 public:
  explicit AutoWidgetScope(const std::string& name);
    ~AutoWidgetScope();

  // Get current scope path
  std::string GetPath() const { return scope_.GetFullPath(); }

 private:
  WidgetIdScope scope_;
  std::string name_;
};

/**
 * @brief Automatically register the last ImGui item
 *
 * Call this after any ImGui widget creation to automatically register it.
 * Captures widget type, bounds, visibility, and enabled state.
 *
 * @param widget_type Type of widget ("button", "input", "checkbox", etc.)
 * @param explicit_label Optional explicit label (uses ImGui::GetItemLabel() if empty)
 * @param description Optional description for the test harness
 */
void AutoRegisterLastItem(const std::string& widget_type,
                          const std::string& explicit_label = "",
                          const std::string& description = "");

// ============================================================================
// Automatic Registration Wrappers for Common ImGui Widgets
// ============================================================================
// These wrappers call the standard ImGui functions and automatically register
// the widget with the WidgetIdRegistry for test automation.
//
// They preserve the exact same API and return values as ImGui, so they can be
// drop-in replacements in existing code.

inline bool AutoButton(const char* label, const ImVec2& size = ImVec2(0, 0)) {
  bool clicked = ImGui::Button(label, size);
  AutoRegisterLastItem("button", label);
  return clicked;
}

inline bool AutoSmallButton(const char* label) {
  bool clicked = ImGui::SmallButton(label);
  AutoRegisterLastItem("button", label, "Small button");
  return clicked;
}

inline bool AutoCheckbox(const char* label, bool* v) {
  bool changed = ImGui::Checkbox(label, v);
  AutoRegisterLastItem("checkbox", label);
  return changed;
}

inline bool AutoRadioButton(const char* label, bool active) {
  bool clicked = ImGui::RadioButton(label, active);
  AutoRegisterLastItem("radio", label);
  return clicked;
}

inline bool AutoRadioButton(const char* label, int* v, int v_button) {
  bool clicked = ImGui::RadioButton(label, v, v_button);
  AutoRegisterLastItem("radio", label);
  return clicked;
}

inline bool AutoInputText(const char* label, char* buf, size_t buf_size,
                          ImGuiInputTextFlags flags = 0,
                          ImGuiInputTextCallback callback = nullptr,
                          void* user_data = nullptr) {
  bool changed = ImGui::InputText(label, buf, buf_size, flags, callback, user_data);
  AutoRegisterLastItem("input", label);
  return changed;
}

inline bool AutoInputTextMultiline(const char* label, char* buf, size_t buf_size,
                                   const ImVec2& size = ImVec2(0, 0),
                                   ImGuiInputTextFlags flags = 0,
                                   ImGuiInputTextCallback callback = nullptr,
                                   void* user_data = nullptr) {
  bool changed = ImGui::InputTextMultiline(label, buf, buf_size, size, flags, callback, user_data);
  AutoRegisterLastItem("textarea", label);
  return changed;
}

inline bool AutoInputInt(const char* label, int* v, int step = 1, int step_fast = 100,
                         ImGuiInputTextFlags flags = 0) {
  bool changed = ImGui::InputInt(label, v, step, step_fast, flags);
  AutoRegisterLastItem("input_int", label);
  return changed;
}

inline bool AutoInputFloat(const char* label, float* v, float step = 0.0f,
                           float step_fast = 0.0f, const char* format = "%.3f",
                           ImGuiInputTextFlags flags = 0) {
  bool changed = ImGui::InputFloat(label, v, step, step_fast, format, flags);
  AutoRegisterLastItem("input_float", label);
  return changed;
}

inline bool AutoSliderInt(const char* label, int* v, int v_min, int v_max,
                          const char* format = "%d", ImGuiSliderFlags flags = 0) {
  bool changed = ImGui::SliderInt(label, v, v_min, v_max, format, flags);
  AutoRegisterLastItem("slider", label);
  return changed;
}

inline bool AutoSliderFloat(const char* label, float* v, float v_min, float v_max,
                            const char* format = "%.3f", ImGuiSliderFlags flags = 0) {
  bool changed = ImGui::SliderFloat(label, v, v_min, v_max, format, flags);
  AutoRegisterLastItem("slider", label);
  return changed;
}

inline bool AutoCombo(const char* label, int* current_item, const char* const items[],
                      int items_count, int popup_max_height_in_items = -1) {
  bool changed = ImGui::Combo(label, current_item, items, items_count, popup_max_height_in_items);
  AutoRegisterLastItem("combo", label);
  return changed;
}

inline bool AutoSelectable(const char* label, bool selected = false,
                           ImGuiSelectableFlags flags = 0,
                           const ImVec2& size = ImVec2(0, 0)) {
  bool clicked = ImGui::Selectable(label, selected, flags, size);
  AutoRegisterLastItem("selectable", label);
  return clicked;
}

inline bool AutoSelectable(const char* label, bool* p_selected,
                           ImGuiSelectableFlags flags = 0,
                           const ImVec2& size = ImVec2(0, 0)) {
  bool clicked = ImGui::Selectable(label, p_selected, flags, size);
  AutoRegisterLastItem("selectable", label);
  return clicked;
}

inline bool AutoMenuItem(const char* label, const char* shortcut = nullptr,
                         bool selected = false, bool enabled = true) {
  bool activated = ImGui::MenuItem(label, shortcut, selected, enabled);
  AutoRegisterLastItem("menuitem", label);
  return activated;
}

inline bool AutoMenuItem(const char* label, const char* shortcut, bool* p_selected,
                         bool enabled = true) {
  bool activated = ImGui::MenuItem(label, shortcut, p_selected, enabled);
  AutoRegisterLastItem("menuitem", label);
  return activated;
}

inline bool AutoBeginMenu(const char* label, bool enabled = true) {
  bool opened = ImGui::BeginMenu(label, enabled);
  if (opened) {
    AutoRegisterLastItem("menu", label, "Submenu");
  }
  return opened;
}

inline bool AutoBeginTabItem(const char* label, bool* p_open = nullptr,
                              ImGuiTabItemFlags flags = 0) {
  bool selected = ImGui::BeginTabItem(label, p_open, flags);
  if (selected) {
    AutoRegisterLastItem("tab", label);
  }
  return selected;
}

inline bool AutoTreeNode(const char* label) {
  bool opened = ImGui::TreeNode(label);
  if (opened) {
    AutoRegisterLastItem("treenode", label);
  }
  return opened;
}

inline bool AutoTreeNodeEx(const char* label, ImGuiTreeNodeFlags flags = 0) {
  bool opened = ImGui::TreeNodeEx(label, flags);
  if (opened) {
    AutoRegisterLastItem("treenode", label);
  }
  return opened;
}

inline bool AutoCollapsingHeader(const char* label, ImGuiTreeNodeFlags flags = 0) {
  bool opened = ImGui::CollapsingHeader(label, flags);
  if (opened) {
    AutoRegisterLastItem("collapsing", label);
  }
  return opened;
}

// ============================================================================
// Canvas-specific registration helpers
// ============================================================================

/**
 * @brief Register a canvas widget after BeginChild or similar
 *
 * Canvases typically use BeginChild which doesn't have a return value,
 * so we provide a separate registration helper.
 *
 * @param canvas_name Name of the canvas (should match BeginChild name)
 * @param description Optional description of the canvas purpose
 */
inline void RegisterCanvas(const char* canvas_name, const std::string& description = "") {
  AutoRegisterLastItem("canvas", canvas_name, description);
}

/**
 * @brief Register a table after BeginTable
 *
 * @param table_name Name of the table (should match BeginTable name)
 * @param description Optional description
 */
inline void RegisterTable(const char* table_name, const std::string& description = "") {
  AutoRegisterLastItem("table", table_name, description);
}

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_WIDGET_AUTO_REGISTER_H_

