#include "app/editor/menu/activity_bar.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <functional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

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

ActivityBar::ActivityBar(WorkspaceWindowManager& window_manager,
                         std::function<bool()> is_dungeon_workbench_mode,
                         std::function<void(bool)> set_dungeon_workflow_mode)
    : window_manager_(window_manager),
      window_browser_(window_manager),
      window_sidebar_(window_manager, std::move(is_dungeon_workbench_mode),
                      std::move(set_dungeon_workflow_mode)) {}

void ActivityBar::Render(
    size_t session_id, const std::string& active_category,
    const std::vector<std::string>& all_categories,
    const std::unordered_set<std::string>& active_editor_categories,
    std::function<bool()> has_rom, std::function<bool()> is_rom_dirty) {
  if (!window_manager_.IsSidebarVisible())
    return;

  // When the startup dashboard is active there are no meaningful left-panel
  // cards; keep the activity rail visible but collapse the side panel.
  const bool dashboard_active =
      (active_category == WorkspaceWindowManager::kDashboardCategory);
  if (dashboard_active && window_manager_.IsSidebarExpanded()) {
    window_manager_.SetSidebarExpanded(false);
  }

  DrawActivityBarStrip(session_id, active_category, all_categories,
                       active_editor_categories, has_rom,
                       std::move(is_rom_dirty));

  if (window_manager_.IsSidebarExpanded() && !dashboard_active) {
    DrawSidePanel(session_id, active_category, has_rom);
  }
}

void ActivityBar::DrawActivityBarStrip(
    size_t session_id, const std::string& active_category,
    const std::vector<std::string>& all_categories,
    const std::unordered_set<std::string>& active_editor_categories,
    std::function<bool()> has_rom, std::function<bool()> is_rom_dirty) {

  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float top_inset = gui::LayoutHelpers::GetTopInset();
  const auto safe_area = gui::LayoutHelpers::GetSafeAreaInsets();
  const float viewport_height =
      std::max(0.0f, viewport->WorkSize.y - top_inset - safe_area.bottom);
  const float bar_width = gui::UIConfig::kActivityBarWidth;

  constexpr ImGuiWindowFlags kExtraFlags =
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoFocusOnAppearing |
      ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBringToFrontOnFocus;

  gui::FixedPanel bar(
      "##ActivityBar",
      ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + top_inset),
      ImVec2(bar_width, viewport_height),
      {.bg = gui::ConvertColorToImVec4(theme.surface),
       .border = gui::ConvertColorToImVec4(theme.text_disabled),
       .padding = {0.0f, 8.0f},
       .spacing = {0.0f, 8.0f},
       .border_size = 1.0f},
      kExtraFlags);

  if (bar) {

    // Global Search / Command Palette at top
    if (gui::TransparentIconButton(ICON_MD_SEARCH, gui::IconSize::ActivityBar(),
                                   "Global Search (Ctrl+Shift+F)", false,
                                   ImVec4(0, 0, 0, 0), "activity_bar",
                                   "search")) {
      window_manager_.TriggerShowSearch();
    }

    // Separator
    ImGui::Spacing();
    ImVec2 sep_p1 = ImGui::GetCursorScreenPos();
    ImVec2 sep_p2 =
        ImVec2(sep_p1.x + gui::UIConfig::kActivityBarWidth, sep_p1.y);
    ImGui::GetWindowDrawList()->AddLine(
        sep_p1, sep_p2,
        ImGui::ColorConvertFloat4ToU32(gui::ConvertColorToImVec4(theme.border)),
        1.0f);
    ImGui::Spacing();

    bool rom_loaded = has_rom ? has_rom() : false;

    // Draw ALL editor categories (not just active ones)
    for (const auto& cat : all_categories) {
      if (cat == WorkspaceWindowManager::kDashboardCategory) {
        continue;
      }
      bool is_selected = (cat == active_category);
      bool panel_expanded = window_manager_.IsSidebarExpanded();
      bool has_active_editor = active_editor_categories.count(cat) > 0;

      // Emulator is always available, others require ROM
      bool category_enabled =
          rom_loaded || (cat == "Emulator") || (cat == "Agent");

      // Get category-specific theme colors for expressive appearance
      auto cat_theme = WorkspaceWindowManager::GetCategoryTheme(cat);
      ImVec4 cat_color(cat_theme.r, cat_theme.g, cat_theme.b, cat_theme.a);
      ImVec4 glow_color(cat_theme.glow_r, cat_theme.glow_g, cat_theme.glow_b,
                        1.0f);

      // Active Indicator with category-specific colors
      if (is_selected && category_enabled && panel_expanded) {
        ImVec2 pos = ImGui::GetCursorScreenPos();

        // Outer glow shadow (subtle, category color at 15% opacity)
        ImVec4 outer_glow = glow_color;
        outer_glow.w = 0.15f;
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(pos.x - 1.0f, pos.y - 1.0f),
            ImVec2(pos.x + 49.0f, pos.y + 41.0f),
            ImGui::ColorConvertFloat4ToU32(outer_glow), 4.0f);

        // Background highlight (category glow at 30% opacity)
        ImVec4 highlight = glow_color;
        highlight.w = 0.30f;
        ImGui::GetWindowDrawList()->AddRectFilled(
            pos, ImVec2(pos.x + 48.0f, pos.y + 40.0f),
            ImGui::ColorConvertFloat4ToU32(highlight), 2.0f);

        // Left accent border (4px wide, category-specific color)
        ImGui::GetWindowDrawList()->AddRectFilled(
            pos, ImVec2(pos.x + 4.0f, pos.y + 40.0f),
            ImGui::ColorConvertFloat4ToU32(cat_color));
      }

      std::string icon = WorkspaceWindowManager::GetCategoryIcon(cat);

      // Subtle indicator even when collapsed
      if (is_selected && category_enabled && !panel_expanded) {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec4 highlight = glow_color;
        highlight.w = 0.15f;
        ImGui::GetWindowDrawList()->AddRectFilled(
            pos, ImVec2(pos.x + 48.0f, pos.y + 40.0f),
            ImGui::ColorConvertFloat4ToU32(highlight), 2.0f);
        ImVec4 accent = cat_color;
        accent.w = 0.6f;
        ImGui::GetWindowDrawList()->AddRectFilled(
            pos, ImVec2(pos.x + 3.0f, pos.y + 40.0f),
            ImGui::ColorConvertFloat4ToU32(accent));
      }

      // Dim indicator for categories whose editor is open but not currently
      // selected. Makes "what's open" readable at a glance without competing
      // with the full selection glow above.
      if (!is_selected && category_enabled && has_active_editor) {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec4 dim_accent = cat_color;
        dim_accent.w = 0.35f;
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(pos.x + 45.0f, pos.y + 8.0f),
            ImVec2(pos.x + 48.0f, pos.y + 32.0f),
            ImGui::ColorConvertFloat4ToU32(dim_accent), 1.5f);
      }

      // Always pass category color so inactive icons remain visible
      ImVec4 icon_color = cat_color;
      if (!category_enabled) {
        ImGui::BeginDisabled();
      }
      if (gui::TransparentIconButton(icon.c_str(), gui::IconSize::ActivityBar(),
                                     nullptr, is_selected, icon_color,
                                     "activity_bar", cat.c_str())) {
        if (category_enabled) {
          if (cat == active_category && panel_expanded) {
            window_manager_.ToggleSidebarExpanded();
          } else {
            window_manager_.SetActiveCategory(cat);
            window_manager_.SetSidebarExpanded(true);
            // Notify that a category was selected (dismisses dashboard)
            window_manager_.TriggerCategorySelected(cat);
          }
        }
      }
      if (!category_enabled) {
        ImGui::EndDisabled();
      }

      // Dirty-ROM dot badge on the currently selected category's icon.
      // We draw after the button so it paints on top.
      const bool rom_dirty = is_rom_dirty ? is_rom_dirty() : false;
      if (is_selected && category_enabled && rom_dirty) {
        ImVec2 last_min = ImGui::GetItemRectMin();
        ImVec2 last_max = ImGui::GetItemRectMax();
        ImVec2 dot_center(last_max.x - 7.0f, last_min.y + 7.0f);
        ImVec4 dot_color = gui::ConvertColorToImVec4(theme.warning);
        ImGui::GetWindowDrawList()->AddCircleFilled(
            dot_center, 3.5f, ImGui::ColorConvertFloat4ToU32(dot_color));
      }

      // Tooltip with status information
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::BeginTooltip();
        ImGui::Text("%s %s", icon.c_str(), cat.c_str());
        if (!category_enabled) {
          gui::ColoredText("Open ROM required",
                           gui::ConvertColorToImVec4(theme.warning));
        } else if (has_active_editor) {
          gui::ColoredText(is_selected ? "Active editor"
                                       : "Editor open (click to focus)",
                           gui::ConvertColorToImVec4(theme.success));
        } else {
          gui::ColoredText("Click to view windows",
                           gui::GetTextSecondaryVec4());
        }
        if (is_selected && rom_dirty) {
          gui::ColoredText("ROM has unsaved changes",
                           gui::ConvertColorToImVec4(theme.warning));
        }
        ImGui::EndTooltip();
      }
    }
  }

  // Draw "More Actions" button at the bottom
  ImGui::SetCursorPosY(viewport_height - 48.0f);

  if (gui::TransparentIconButton(ICON_MD_MORE_HORIZ, gui::IconSize::Large(),
                                 nullptr, false, ImVec4(0, 0, 0, 0),
                                 "activity_bar", "more_actions")) {
    ImGui::OpenPopup("ActivityBarMoreMenu");
  }

  if (ImGui::BeginPopup("ActivityBarMoreMenu")) {
    if (ImGui::MenuItem(ICON_MD_TERMINAL " Command Palette")) {
      window_manager_.TriggerShowCommandPalette();
    }
    if (ImGui::MenuItem(ICON_MD_KEYBOARD " Keyboard Shortcuts")) {
      window_manager_.TriggerShowShortcuts();
    }
    ImGui::Separator();
    if (ImGui::MenuItem(ICON_MD_FOLDER_OPEN " Open ROM / Project")) {
      window_manager_.TriggerOpenRom();
    }
    if (ImGui::MenuItem(ICON_MD_SETTINGS " Settings")) {
      window_manager_.TriggerShowSettings();
    }
    // Reset Layout lives under Windows → Layout ▸ Reset Layout now.
    ImGui::EndPopup();
  }
  // FixedPanel destructor handles End() + PopStyleVar/PopStyleColor
}

void ActivityBar::DrawSidePanel(size_t session_id, const std::string& category,
                                std::function<bool()> has_rom) {
  window_sidebar_.Draw(session_id, category, std::move(has_rom));
}

void ActivityBar::DrawWindowBrowser(size_t session_id, bool* p_open) {
  window_browser_.Draw(session_id, p_open);
}

}  // namespace editor
}  // namespace yaze
