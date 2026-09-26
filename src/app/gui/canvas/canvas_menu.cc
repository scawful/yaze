#include "canvas_menu.h"

#include "app/gui/core/ui_helpers.h"
#include "imgui/imgui_internal.h"

namespace yaze {
namespace gui {

namespace {

constexpr ImGuiStyleVar kPopupSpacingVars[] = {
    ImGuiStyleVar_WindowPadding,
    ImGuiStyleVar_FramePadding,
    ImGuiStyleVar_ItemSpacing,
    ImGuiStyleVar_ItemInnerSpacing,
};

}  // namespace

ImVec2 BaseStyleVarVec2(ImGuiStyleVar idx) {
  ImGuiContext* g = ImGui::GetCurrentContext();
  if (g == nullptr) {
    return ImVec2(0, 0);
  }
  // The oldest backup of `idx` holds the value before any push of it.
  for (const ImGuiStyleMod& mod : g->StyleVarStack) {
    if (mod.VarIdx == idx) {
      return ImVec2(mod.BackupFloat[0], mod.BackupFloat[1]);
    }
  }
  const ImGuiStyleVarInfo* info = ImGui::GetStyleVarInfo(idx);
  return *static_cast<const ImVec2*>(info->GetVarPtr(&g->Style));
}

PopupStyleScope::PopupStyleScope() {
  if (ImGui::GetCurrentContext() == nullptr) {
    return;
  }
  for (const ImGuiStyleVar idx : kPopupSpacingVars) {
    ImGui::PushStyleVar(idx, BaseStyleVarVec2(idx));
    ++pushed_;
  }
}

PopupStyleScope::~PopupStyleScope() {
  if (pushed_ > 0) {
    ImGui::PopStyleVar(pushed_);
  }
}

void MenuConfirmState::NoteRendered(ImGuiID id, int frame) {
  if (armed_id_ != id) {
    return;
  }
  if (frame > last_frame_ + 1) {
    // The item was not drawn on the previous frame: its menu was closed.
    Reset();
    return;
  }
  last_frame_ = frame;
}

bool MenuConfirmState::IsArmed(ImGuiID id, int frame) const {
  return id != 0 && armed_id_ == id && frame <= last_frame_ + 1;
}

bool MenuConfirmState::Click(ImGuiID id, int frame) {
  if (IsArmed(id, frame)) {
    Reset();
    return true;
  }
  armed_id_ = id;
  last_frame_ = frame;
  return false;
}

MenuConfirmState& GetMenuConfirmState() {
  static MenuConfirmState state;
  return state;
}

std::string ConfirmLabel(const std::string& label) {
  std::string base = label;
  static const std::string kUtf8Ellipsis = "\xE2\x80\xA6";
  if (base.size() >= 3 && base.compare(base.size() - 3, 3, "...") == 0) {
    base.resize(base.size() - 3);
  } else if (base.size() >= kUtf8Ellipsis.size() &&
             base.compare(base.size() - kUtf8Ellipsis.size(),
                          kUtf8Ellipsis.size(), kUtf8Ellipsis) == 0) {
    base.resize(base.size() - kUtf8Ellipsis.size());
  }
  return "Confirm " + base + "?";
}

void RenderMenuItem(
    const CanvasMenuItem& item,
    std::function<void(const std::string&, std::function<void()>)>
        popup_opened_callback) {
  // Check visibility
  if (!item.visible_condition()) {
    return;
  }

  const bool enabled = item.enabled_condition();
  const bool checked =
      item.checked_condition ? item.checked_condition() : false;

  // Apply disabled state if needed
  if (!enabled) {
    ImGui::BeginDisabled();
  }

  // Build label with icon if present
  std::string display_label = item.label;
  if (!item.icon.empty()) {
    display_label = item.icon + " " + item.label;
  }

  // Render menu item based on type
  if (item.subitems.empty()) {
    const bool has_custom_color = item.color.x != 1.0f ||
                                  item.color.y != 1.0f ||
                                  item.color.z != 1.0f || item.color.w != 1.0f;
    const bool push_color = item.destructive || has_custom_color;

    // Confirm-gated items keep a stable ImGui ID while their label changes.
    bool armed = false;
    ImGuiID confirm_id = 0;
    const int frame = ImGui::GetFrameCount();
    if (item.requires_confirmation) {
      confirm_id = ImGui::GetID(item.label.c_str());
      auto& confirm = GetMenuConfirmState();
      confirm.NoteRendered(confirm_id, frame);
      armed = confirm.IsArmed(confirm_id, frame);
      std::string text = armed ? ConfirmLabel(item.label) : item.label;
      if (!item.icon.empty()) {
        text = item.icon + " " + text;
      }
      display_label = text + "###" + item.label;
    }

    const char* shortcut =
        armed ? "click again"
              : (item.shortcut.empty() ? nullptr : item.shortcut.c_str());

    if (push_color) {
      ImGui::PushStyleColor(ImGuiCol_Text,
                            item.destructive ? GetErrorColor() : item.color);
    }
    // The arming click must not close the popup; the confirming click does.
    if (item.requires_confirmation) {
      ImGui::PushItemFlag(ImGuiItemFlags_AutoClosePopups, armed);
    }
    bool selected =
        item.checked_condition
            ? ImGui::MenuItem(display_label.c_str(), shortcut, checked)
            : ImGui::MenuItem(display_label.c_str(), shortcut);
    if (item.requires_confirmation) {
      ImGui::PopItemFlag();
    }
    if (push_color) {
      ImGui::PopStyleColor();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      if (armed && item.tooltip.empty()) {
        ImGui::SetTooltip("This cannot be undone.");
      } else if (!item.tooltip.empty()) {
        ImGui::SetTooltip("%s", item.tooltip.c_str());
      }
    }

    if (selected && item.requires_confirmation) {
      selected = GetMenuConfirmState().Click(confirm_id, frame);
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
  if (!enabled) {
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
