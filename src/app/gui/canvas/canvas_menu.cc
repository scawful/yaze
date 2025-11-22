#include "canvas_menu.h"

namespace yaze {
namespace gui {

void RenderMenuItem(
    const CanvasMenuItem& item,
    std::function<void(const std::string&, std::function<void()>)>
        popup_opened_callback) {
  // Check visibility
  if (!item.visible_condition()) {
    return;
  }

  // Apply disabled state if needed
  if (!item.enabled_condition()) {
    ImGui::BeginDisabled();
  }

  // Build label with icon if present
  std::string display_label = item.label;
  if (!item.icon.empty()) {
    display_label = item.icon + " " + item.label;
  }

  // Render menu item based on type
  if (item.subitems.empty()) {
    // Simple menu item
    bool selected = false;
    if (item.color.x != 1.0f || item.color.y != 1.0f || item.color.z != 1.0f ||
        item.color.w != 1.0f) {
      // Render with custom color
      ImGui::PushStyleColor(ImGuiCol_Text, item.color);
      selected = ImGui::MenuItem(
          display_label.c_str(),
          item.shortcut.empty() ? nullptr : item.shortcut.c_str());
      ImGui::PopStyleColor();
    } else {
      selected = ImGui::MenuItem(
          display_label.c_str(),
          item.shortcut.empty() ? nullptr : item.shortcut.c_str());
    }

    if (selected) {
      // Invoke callback
      if (item.callback) {
        item.callback();
      }

      // Handle popup if defined
      if (item.popup.has_value() && item.popup->auto_open_on_select &&
          popup_opened_callback) {
        popup_opened_callback(item.popup->popup_id,
                              item.popup->render_callback);
      }
    }
  } else {
    // Submenu
    if (ImGui::BeginMenu(display_label.c_str())) {
      for (const auto& subitem : item.subitems) {
        RenderMenuItem(subitem, popup_opened_callback);
      }
      ImGui::EndMenu();
    }
  }

  // Restore enabled state
  if (!item.enabled_condition()) {
    ImGui::EndDisabled();
  }

  // Render separator if requested
  if (item.separator_after) {
    ImGui::Separator();
  }
}

void RenderMenuSection(
    const CanvasMenuSection& section,
    std::function<void(const std::string&, std::function<void()>)>
        popup_opened_callback) {
  // Skip empty sections
  if (section.items.empty()) {
    return;
  }

  // Render section title if present
  if (!section.title.empty()) {
    ImGui::TextColored(section.title_color, "%s", section.title.c_str());
    ImGui::Separator();
  }

  // Render all items in section
  for (const auto& item : section.items) {
    RenderMenuItem(item, popup_opened_callback);
  }

  // Render separator after section if requested
  if (section.separator_after) {
    ImGui::Separator();
  }
}

void RenderCanvasMenu(
    const CanvasMenuDefinition& menu,
    std::function<void(const std::string&, std::function<void()>)>
        popup_opened_callback) {
  // Skip disabled menus
  if (!menu.enabled) {
    return;
  }

  // Render all sections
  for (const auto& section : menu.sections) {
    RenderMenuSection(section, popup_opened_callback);
  }
}

}  // namespace gui
}  // namespace yaze
