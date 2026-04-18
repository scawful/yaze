#include "app/editor/menu/window_sidebar.h"

#include <algorithm>
#include <cctype>
#include <string>

#include "absl/strings/str_format.h"
#include "app/editor/system/workspace_window_manager.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/core/ui_config.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/themed_widgets.h"
#include "core/color.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

namespace {

std::string LowercaseCopy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(::tolower(c)); });
  return value;
}

bool MatchesWindowSearch(const std::string& query,
                         const WindowDescriptor& window) {
  return WindowSidebar::MatchesWindowSearch(query, window.display_name,
                                            window.card_id,
                                            window.shortcut_hint);
}

bool IsDungeonPanelModeWindow(const std::string& window_id) {
  return WindowSidebar::IsDungeonWindowModeTarget(window_id);
}

}  // namespace

WindowSidebar::WindowSidebar(
    WorkspaceWindowManager& window_manager,
    std::function<bool()> is_dungeon_workbench_mode,
    std::function<void(bool)> set_dungeon_workflow_mode)
    : window_manager_(window_manager),
      is_dungeon_workbench_mode_(std::move(is_dungeon_workbench_mode)),
      set_dungeon_workflow_mode_(std::move(set_dungeon_workflow_mode)) {}

bool WindowSidebar::MatchesWindowSearch(const std::string& query,
                                        const std::string& display_name,
                                        const std::string& window_id,
                                        const std::string& shortcut_hint) {
  if (query.empty()) {
    return true;
  }

  const std::string search_str = LowercaseCopy(query);
  return LowercaseCopy(display_name).find(search_str) != std::string::npos ||
         LowercaseCopy(window_id).find(search_str) != std::string::npos ||
         LowercaseCopy(shortcut_hint).find(search_str) != std::string::npos;
}

bool WindowSidebar::IsDungeonWindowModeTarget(const std::string& window_id) {
  const bool is_room_window = window_id.rfind("dungeon.room_", 0) == 0;
  return window_id == "dungeon.room_selector" ||
         window_id == "dungeon.room_matrix" || is_room_window;
}

void WindowSidebar::Draw(size_t session_id, const std::string& category,
                         std::function<bool()> has_rom) {
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float bar_width = WorkspaceWindowManager::GetSidebarWidth();
  const float panel_width =
      window_manager_.GetActiveSidePanelWidth(viewport->WorkSize.x);

  const float top_inset = gui::LayoutHelpers::GetTopInset();
  const auto safe_area = gui::LayoutHelpers::GetSafeAreaInsets();
  const float panel_height =
      std::max(0.0f, viewport->WorkSize.y - top_inset - safe_area.bottom);

  gui::FixedPanel panel(
      "##SidePanel",
      ImVec2(viewport->WorkPos.x + bar_width, viewport->WorkPos.y + top_inset),
      ImVec2(panel_width, panel_height),
      {.bg = gui::ConvertColorToImVec4(theme.surface),
       .padding = {8.0f, 8.0f},
       .border_size = 1.0f,
       .rounding = 0.0f},
      ImGuiWindowFlags_NoFocusOnAppearing);

  if (!panel) {
    return;
  }

  ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
  ImGui::Text("%s", category.c_str());
  ImGui::PopFont();

  float avail_width = ImGui::GetContentRegionAvail().x;
  const ImVec2 chrome_icon_size = gui::IconSize::Small();
  ImGui::SameLine(ImGui::GetCursorPosX() + avail_width - chrome_icon_size.x);
  if (gui::TransparentIconButton(ICON_MD_CHEVRON_LEFT, chrome_icon_size,
                                 "Collapse Sidebar", false, ImVec4(0, 0, 0, 0),
                                 "window_sidebar", "collapse_side_panel")) {
    window_manager_.SetSidebarExpanded(false);
  }

  ImGui::Separator();
  ImGui::Spacing();

  const ImVec2 search_button_size = gui::IconSize::Small();
  const float standard_spacing =
      std::clamp(gui::LayoutHelpers::GetStandardSpacing(), 4.0f, 8.0f);
  const float compact_spacing = std::max(4.0f, standard_spacing * 0.75f);
  const float search_spacing = compact_spacing;
  const float search_width =
      std::max(140.0f, ImGui::GetContentRegionAvail().x -
                           search_button_size.x - search_spacing);
  ImGui::SetNextItemWidth(search_width);
  ImGui::InputTextWithHint("##SidebarSearch", ICON_MD_SEARCH " Filter...",
                           sidebar_search_, sizeof(sidebar_search_));
  ImGui::SameLine(0.0f, search_spacing);
  const bool filter_empty = sidebar_search_[0] == '\0';
  if (filter_empty) {
    ImGui::BeginDisabled();
  }
  if (gui::TransparentIconButton(ICON_MD_CLEAR, search_button_size,
                                 "Clear filter", false,
                                 gui::GetTextSecondaryVec4(), "window_sidebar",
                                 "clear_sidebar_filter")) {
    sidebar_search_[0] = '\0';
  }
  if (filter_empty) {
    ImGui::EndDisabled();
  }

  auto read_dungeon_workbench_mode = [&]() -> bool {
    if (category != "Dungeon") {
      return false;
    }
    if (is_dungeon_workbench_mode_) {
      return is_dungeon_workbench_mode_();
    }
    return window_manager_.IsWindowOpen(session_id, "dungeon.workbench");
  };
  bool dungeon_workbench_mode = read_dungeon_workbench_mode();

  auto switch_to_dungeon_workbench_mode = [&]() -> bool {
    if (category != "Dungeon" || dungeon_workbench_mode) {
      return false;
    }
    if (set_dungeon_workflow_mode_) {
      set_dungeon_workflow_mode_(true);
      dungeon_workbench_mode = true;
      return true;
    }
    window_manager_.OpenWindow(session_id, "dungeon.workbench");
    for (const auto& descriptor :
         window_manager_.GetWindowsInCategory(session_id, "Dungeon")) {
      const std::string& window_id = descriptor.card_id;
      if (window_id == "dungeon.workbench") {
        continue;
      }
      if (window_manager_.IsWindowPinned(session_id, window_id)) {
        continue;
      }
      if (IsDungeonPanelModeWindow(window_id)) {
        window_manager_.CloseWindow(session_id, window_id);
      }
    }
    dungeon_workbench_mode = read_dungeon_workbench_mode();
    return dungeon_workbench_mode;
  };

  auto switch_to_dungeon_window_mode = [&]() -> bool {
    if (category != "Dungeon" || !dungeon_workbench_mode) {
      return false;
    }
    if (set_dungeon_workflow_mode_) {
      set_dungeon_workflow_mode_(false);
      dungeon_workbench_mode = false;
      return true;
    }
    window_manager_.CloseWindow(session_id, "dungeon.workbench");
    window_manager_.OpenWindow(session_id, "dungeon.room_selector");
    window_manager_.OpenWindow(session_id, "dungeon.room_matrix");
    dungeon_workbench_mode = read_dungeon_workbench_mode();
    return !dungeon_workbench_mode;
  };

  auto ensure_dungeon_window_mode_for_window =
      [&](const std::string& window_id) -> bool {
    if (category != "Dungeon" || !dungeon_workbench_mode ||
        !IsDungeonPanelModeWindow(window_id)) {
      return false;
    }
    return switch_to_dungeon_window_mode();
  };

  if (category == "Dungeon") {
    ImGui::Spacing();
    ImGui::TextDisabled(ICON_MD_WORKSPACES " Workflow");
    ImGui::Spacing();

    const float workflow_gap = compact_spacing;
    const float workflow_min_button_width = 96.0f;
    const float workflow_available_width =
        std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const bool stack_workflow_buttons =
        workflow_available_width <=
        (workflow_min_button_width * 2.0f + workflow_gap);
    const float workflow_button_width =
        stack_workflow_buttons
            ? workflow_available_width
            : std::max(workflow_min_button_width,
                       (workflow_available_width - workflow_gap) * 0.5f);
    const float workflow_button_height =
        std::max(24.0f, gui::LayoutHelpers::GetStandardWidgetHeight());
    auto draw_workflow_button = [&](const char* id, const char* icon,
                                    const char* label, bool active,
                                    const char* tooltip,
                                    const ImVec2& button_size) -> bool {
      const ImVec4 active_bg = gui::GetPrimaryVec4();
      const ImVec4 inactive_bg = gui::GetSurfaceContainerHighVec4();
      const ImVec4 inactive_hover = gui::GetSurfaceContainerHighestVec4();
      gui::StyleColorGuard colors(
          {{ImGuiCol_Button, active ? active_bg : inactive_bg},
           {ImGuiCol_ButtonHovered, active ? active_bg : inactive_hover},
           {ImGuiCol_ButtonActive, active ? active_bg : inactive_hover}});
      const std::string button_label =
          absl::StrFormat("%s %s##%s", icon, label, id);
      const bool clicked = ImGui::Button(button_label.c_str(), button_size);
      if (ImGui::IsItemHovered() && tooltip && *tooltip) {
        ImGui::SetTooltip("%s", tooltip);
      }
      return clicked;
    };
    const ImVec2 workflow_button_size(workflow_button_width,
                                      workflow_button_height);

    if (draw_workflow_button("workflow_workbench", ICON_MD_WORKSPACES,
                             "Workbench", dungeon_workbench_mode,
                             "Workbench mode: integrated room browser + inspector",
                             workflow_button_size)) {
      switch_to_dungeon_workbench_mode();
    }
    if (!stack_workflow_buttons) {
      ImGui::SameLine(0.0f, workflow_gap);
    }
    if (draw_workflow_button("workflow_windows", ICON_MD_VIEW_QUILT,
                             "Windows", !dungeon_workbench_mode,
                             "Window mode: standalone Room List + Room Matrix + room windows",
                             workflow_button_size)) {
      switch_to_dungeon_window_mode();
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
  }

  ImGui::Spacing();

  const bool rom_loaded = has_rom ? has_rom() : true;
  const bool disable_windows = !rom_loaded && category != "Emulator";

  const auto category_windows =
      window_manager_.GetWindowsInCategory(session_id, category);
  int visible_windows_in_category = 0;
  for (const auto& category_window : category_windows) {
    if (category_window.visibility_flag && *category_window.visibility_flag) {
      ++visible_windows_in_category;
    }
  }

  ImGui::TextDisabled("%d / %d visible", visible_windows_in_category,
                      static_cast<int>(category_windows.size()));
  ImGui::Spacing();

  const float action_gap = compact_spacing;
  const float action_min_button_width = 84.0f;
  const float action_available_width =
      std::max(1.0f, ImGui::GetContentRegionAvail().x);
  const bool stack_action_buttons =
      action_available_width <=
      (action_min_button_width * 3.0f + (action_gap * 2.0f));
  const float action_button_width =
      stack_action_buttons
          ? action_available_width
          : std::max(action_min_button_width,
                     (action_available_width - (action_gap * 2.0f)) / 3.0f);
  const float action_button_height =
      std::max(24.0f, gui::LayoutHelpers::GetStandardWidgetHeight());
  auto draw_action_button = [&](const char* id, const char* icon,
                                const char* label, const char* tooltip,
                                const ImVec2& button_size) -> bool {
    const std::string button_label =
        absl::StrFormat("%s %s##%s", icon, label, id);
    const bool clicked = ImGui::Button(button_label.c_str(), button_size);
    if (ImGui::IsItemHovered() && tooltip && *tooltip) {
      ImGui::SetTooltip("%s", tooltip);
    }
    return clicked;
  };
  const ImVec2 action_button_size(action_button_width, action_button_height);

  if (draw_action_button("open_window_browser", ICON_MD_APPS, "Browser",
                         "Open Window Browser", action_button_size)) {
    window_manager_.TriggerShowWindowBrowser();
  }
  if (!stack_action_buttons) {
    ImGui::SameLine(0.0f, action_gap);
  }

  const bool bulk_actions_enabled =
      !disable_windows && !(category == "Dungeon" && dungeon_workbench_mode);
  bool bulk_action_hovered = false;
  if (!bulk_actions_enabled) {
    ImGui::BeginDisabled();
  }
  if (draw_action_button("show_category_windows", ICON_MD_VISIBILITY, "Show",
                         "Show all windows in this category",
                         action_button_size)) {
    window_manager_.ShowAllWindowsInCategory(session_id, category);
  }
  if (!stack_action_buttons) {
    ImGui::SameLine(0.0f, action_gap);
  }
  if (!bulk_actions_enabled &&
      ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    bulk_action_hovered = true;
  }
  if (draw_action_button("hide_category_windows", ICON_MD_VISIBILITY_OFF,
                         "Hide", "Hide all windows in this category",
                         action_button_size)) {
    window_manager_.HideAllWindowsInCategory(session_id, category);
  }
  if (!bulk_actions_enabled) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      bulk_action_hovered = true;
    }
    if (bulk_action_hovered && category == "Dungeon" &&
        dungeon_workbench_mode) {
      ImGui::SetTooltip(
          "Switch to Window workflow to bulk-manage room windows.");
    }
    ImGui::EndDisabled();
  }

  ImGui::Spacing();

  if (disable_windows) {
    ImGui::TextUnformatted(ICON_MD_FOLDER_OPEN
                           " Open a ROM to enable this category");
    ImGui::Spacing();
  }

  if (disable_windows) {
    ImGui::BeginDisabled();
  }

  const auto pinned_windows = window_manager_.GetPinnedWindows();
  const ImVec4 disabled_text =
      ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
  auto window_text_color = [&](bool visible) -> ImVec4 {
    if (disable_windows) {
      return disabled_text;
    }
    return visible ? gui::GetOnSurfaceVec4() : gui::GetTextSecondaryVec4();
  };
  const float pin_button_side =
      std::max(20.0f, gui::LayoutHelpers::GetStandardWidgetHeight());
  const ImVec2 pin_button_size(pin_button_side, pin_button_side);
  auto draw_pin_toggle_button = [&](const std::string& widget_id,
                                    bool pinned) -> bool {
    ImGui::PushID(widget_id.c_str());
    const ImVec4 pin_col = pinned ? gui::ConvertColorToImVec4(theme.primary)
                                  : gui::GetTextSecondaryVec4();
    const bool clicked = gui::TransparentIconButton(
        pinned ? ICON_MD_PUSH_PIN : ICON_MD_PIN, pin_button_size,
        pinned ? "Unpin window" : "Pin window", pinned, pin_col,
        "window_sidebar", widget_id.c_str());
    ImGui::PopID();
    return clicked;
  };

  bool pinned_section_open = false;
  if (sidebar_search_[0] == '\0' && !pinned_windows.empty()) {
    bool has_pinned_in_category = false;
    for (const auto& window_id : pinned_windows) {
      const auto* window = window_manager_.GetWindowDescriptor(session_id, window_id);
      if (window && window->category == category) {
        has_pinned_in_category = true;
        break;
      }
    }

    if (has_pinned_in_category) {
      pinned_section_open = ImGui::CollapsingHeader(
          ICON_MD_PUSH_PIN " Pinned Windows",
          ImGuiTreeNodeFlags_DefaultOpen);
      if (pinned_section_open) {
        for (const auto& window_id : pinned_windows) {
          const auto* window = window_manager_.GetWindowDescriptor(session_id, window_id);
          if (!window || window->category != category) {
            continue;
          }

          const bool visible =
              window->visibility_flag ? *window->visibility_flag : false;

          if (draw_pin_toggle_button("pin_" + window->card_id, true)) {
            window_manager_.SetWindowPinned(session_id, window->card_id, false);
          }

          ImGui::SameLine(0.0f, compact_spacing);

          std::string label = absl::StrFormat("%s  %s", window->icon.c_str(),
                                              window->display_name.c_str());
          ImGui::PushID(
              (std::string("pinned_select_") + window->card_id).c_str());
          {
            gui::StyleColorGuard text_color(ImGuiCol_Text,
                                            window_text_color(visible));
            ImVec2 item_size(ImGui::GetContentRegionAvail().x,
                             pin_button_size.y);
            if (ImGui::Selectable(label.c_str(), visible,
                                  ImGuiSelectableFlags_None, item_size)) {
              const bool switched_mode =
                  ensure_dungeon_window_mode_for_window(window->card_id);
              if (switched_mode) {
                window_manager_.OpenWindow(session_id, window->card_id);
              } else {
                window_manager_.ToggleWindow(session_id, window->card_id);
              }

              const bool new_visible =
                  window->visibility_flag ? *window->visibility_flag : false;
              if (new_visible) {
                window_manager_.TriggerWindowClicked(window->category);
                const std::string window_name =
                    window_manager_.GetWorkspaceWindowName(*window);
                if (!window_name.empty()) {
                  ImGui::SetWindowFocus(window_name.c_str());
                }
              }
            }
          }
          ImGui::PopID();
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
      }
    }
  }

  auto windows = window_manager_.GetWindowsSortedByMRU(session_id, category);
  const bool window_content_open = gui::LayoutHelpers::BeginContentChild(
      "##WindowContent", ImVec2(0.0f, gui::UIConfig::kContentMinHeightList));
  if (window_content_open) {
    for (const auto& window : windows) {
      if (!MatchesWindowSearch(sidebar_search_, window.display_name,
                               window.card_id, window.shortcut_hint)) {
        continue;
      }

      const bool is_pinned = window_manager_.IsWindowPinned(window.card_id);
      if (pinned_section_open && is_pinned) {
        continue;
      }

      const bool visible =
          window.visibility_flag ? *window.visibility_flag : false;

      if (draw_pin_toggle_button("pin_" + window.card_id, is_pinned)) {
        window_manager_.SetWindowPinned(session_id, window.card_id, !is_pinned);
      }
      ImGui::SameLine(0.0f, compact_spacing);

      std::string label =
          absl::StrFormat("%s  %s", window.icon.c_str(), window.display_name.c_str());
      ImGui::PushID((std::string("window_select_") + window.card_id).c_str());
      {
        gui::StyleColorGuard text_color(ImGuiCol_Text,
                                        window_text_color(visible));
        ImVec2 item_size(ImGui::GetContentRegionAvail().x, pin_button_size.y);
        if (ImGui::Selectable(label.c_str(), visible, ImGuiSelectableFlags_None,
                              item_size)) {
          const bool switched_mode =
              ensure_dungeon_window_mode_for_window(window.card_id);
          if (switched_mode) {
            window_manager_.OpenWindow(session_id, window.card_id);
          } else {
            window_manager_.ToggleWindow(session_id, window.card_id);
          }

          const bool new_visible =
              window.visibility_flag ? *window.visibility_flag : false;
          if (new_visible) {
            window_manager_.MarkWindowRecentlyUsed(window.card_id);
            window_manager_.TriggerWindowClicked(window.category);
            const std::string window_name =
                window_manager_.GetWorkspaceWindowName(window);
            if (!window_name.empty()) {
              ImGui::SetWindowFocus(window_name.c_str());
            }
          }
        }
      }
      ImGui::PopID();

      if (ImGui::IsItemHovered() && !window.shortcut_hint.empty()) {
        ImGui::SetTooltip("%s", window.shortcut_hint.c_str());
      }
    }
  }
  gui::LayoutHelpers::EndContentChild();

  if (disable_windows) {
    ImGui::EndDisabled();
  }

  const float handle_width = 6.0f;
  const ImVec2 panel_pos = ImGui::GetWindowPos();
  const float panel_draw_height = ImGui::GetWindowHeight();
  ImGui::SetCursorScreenPos(
      ImVec2(panel_pos.x + panel_width - handle_width * 0.5f, panel_pos.y));
  ImGui::InvisibleButton("##SidePanelResizeHandle",
                         ImVec2(handle_width, panel_draw_height));
  const bool handle_hovered = ImGui::IsItemHovered();
  const bool handle_active = ImGui::IsItemActive();
  if (handle_hovered || handle_active) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
  }
  if (handle_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    window_manager_.ResetSidePanelWidth();
  }
  if (handle_active) {
    const float new_width = panel_width + ImGui::GetIO().MouseDelta.x;
    window_manager_.SetActiveSidePanelWidth(new_width, viewport->WorkSize.x);
    ImGui::SetTooltip("Width: %.0f px",
                      window_manager_.GetActiveSidePanelWidth(viewport->WorkSize.x));
  }

  ImVec4 handle_color = gui::GetOutlineVec4();
  handle_color.w = handle_active ? 0.95f : (handle_hovered ? 0.72f : 0.35f);
  ImGui::GetWindowDrawList()->AddLine(
      ImVec2(panel_pos.x + panel_width - 1.0f, panel_pos.y),
      ImVec2(panel_pos.x + panel_width - 1.0f,
             panel_pos.y + panel_draw_height),
      ImGui::GetColorU32(handle_color), handle_active ? 2.0f : 1.0f);
}

}  // namespace editor
}  // namespace yaze
