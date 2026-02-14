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
#include "app/editor/system/panel_manager.h"
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

ActivityBar::ActivityBar(PanelManager& panel_manager,
                         std::function<bool()> is_dungeon_workbench_mode,
                         std::function<void(bool)> set_dungeon_workflow_mode)
    : panel_manager_(panel_manager),
      is_dungeon_workbench_mode_(std::move(is_dungeon_workbench_mode)),
      set_dungeon_workflow_mode_(std::move(set_dungeon_workflow_mode)) {}

void ActivityBar::Render(
    size_t session_id, const std::string& active_category,
    const std::vector<std::string>& all_categories,
    const std::unordered_set<std::string>& active_editor_categories,
    std::function<bool()> has_rom) {
  if (!panel_manager_.IsSidebarVisible())
    return;

  // When the startup dashboard is active there are no meaningful left-panel
  // cards; keep the activity rail visible but collapse the side panel.
  const bool dashboard_active =
      (active_category == PanelManager::kDashboardCategory);
  if (dashboard_active && panel_manager_.IsPanelExpanded()) {
    panel_manager_.SetPanelExpanded(false);
  }

  DrawActivityBarStrip(session_id, active_category, all_categories,
                       active_editor_categories, has_rom);

  if (panel_manager_.IsPanelExpanded() && !dashboard_active) {
    DrawSidePanel(session_id, active_category, has_rom);
  }
}

void ActivityBar::DrawActivityBarStrip(
    size_t session_id, const std::string& active_category,
    const std::vector<std::string>& all_categories,
    const std::unordered_set<std::string>& active_editor_categories,
    std::function<bool()> has_rom) {

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
      panel_manager_.TriggerShowSearch();
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
      if (cat == PanelManager::kDashboardCategory) {
        continue;
      }
      bool is_selected = (cat == active_category);
      bool panel_expanded = panel_manager_.IsPanelExpanded();
      bool has_active_editor = active_editor_categories.count(cat) > 0;

      // Emulator is always available, others require ROM
      bool category_enabled =
          rom_loaded || (cat == "Emulator") || (cat == "Agent");

      // Get category-specific theme colors for expressive appearance
      auto cat_theme = PanelManager::GetCategoryTheme(cat);
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

      std::string icon = PanelManager::GetCategoryIcon(cat);

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
            panel_manager_.TogglePanelExpanded();
          } else {
            panel_manager_.SetActiveCategory(cat);
            panel_manager_.SetPanelExpanded(true);
            // Notify that a category was selected (dismisses dashboard)
            panel_manager_.TriggerCategorySelected(cat);
          }
        }
      }
      if (!category_enabled) {
        ImGui::EndDisabled();
      }

      // Tooltip with status information
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::BeginTooltip();
        ImGui::Text("%s %s", icon.c_str(), cat.c_str());
        if (!category_enabled) {
          gui::ColoredText("Open ROM required",
                           gui::ConvertColorToImVec4(theme.warning));
        } else if (has_active_editor) {
          gui::ColoredText("Editor open",
                           gui::ConvertColorToImVec4(theme.success));
        } else {
          gui::ColoredText("Click to view panels", gui::GetTextSecondaryVec4());
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
      panel_manager_.TriggerShowCommandPalette();
    }
    if (ImGui::MenuItem(ICON_MD_KEYBOARD " Keyboard Shortcuts")) {
      panel_manager_.TriggerShowShortcuts();
    }
    ImGui::Separator();
    if (ImGui::MenuItem(ICON_MD_FOLDER_OPEN " Open ROM")) {
      panel_manager_.TriggerOpenRom();
    }
    if (ImGui::MenuItem(ICON_MD_SETTINGS " Settings")) {
      panel_manager_.TriggerShowSettings();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Reset Layout")) {
      panel_manager_.TriggerResetLayout();
    }
    ImGui::EndPopup();
  }
  // FixedPanel destructor handles End() + PopStyleVar/PopStyleColor
}

void ActivityBar::DrawSidePanel(size_t session_id, const std::string& category,
                                std::function<bool()> has_rom) {

  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float bar_width = PanelManager::GetSidebarWidth();
  const float panel_width =
      panel_manager_.GetActiveSidePanelWidth(viewport->WorkSize.x);

  const float top_inset = gui::LayoutHelpers::GetTopInset();
  const auto safe_area = gui::LayoutHelpers::GetSafeAreaInsets();
  const float panel_height =
      std::max(0.0f, viewport->WorkSize.y - top_inset - safe_area.bottom);

  gui::FixedPanel panel(
      "##SidePanel",
      ImVec2(viewport->WorkPos.x + bar_width, viewport->WorkPos.y + top_inset),
      ImVec2(panel_width, panel_height),
      {.bg = gui::ConvertColorToImVec4(theme.surface),
       .padding = {10.0f, 10.0f},
       .border_size = 1.0f,
       .rounding = 0.0f},
      ImGuiWindowFlags_NoFocusOnAppearing);

  if (panel) {
    // Header
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);  // Use default font
    ImGui::Text("%s", category.c_str());
    ImGui::PopFont();

    // Collapse button (right-aligned)
    float avail_width = ImGui::GetContentRegionAvail().x;
    float button_size = 24.0f;
    ImGui::SameLine(ImGui::GetCursorPosX() + avail_width - button_size);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4.0f);
    if (ImGui::Button(ICON_MD_KEYBOARD_DOUBLE_ARROW_LEFT,
                      ImVec2(button_size, button_size))) {
      panel_manager_.SetPanelExpanded(false);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Collapse Panel");
    }

    ImGui::Separator();
    ImGui::Spacing();

    // Search Bar
    static char sidebar_search[256] = "";
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##SidebarSearch", ICON_MD_SEARCH " Filter...",
                             sidebar_search, sizeof(sidebar_search));

    const auto is_dungeon_panel_mode_card =
        [](const std::string& panel_id) -> bool {
      const bool is_room_window = panel_id.rfind("dungeon.room_", 0) == 0;
      return panel_id == "dungeon.room_selector" ||
             panel_id == "dungeon.room_matrix" || is_room_window;
    };
    auto read_dungeon_workbench_mode = [&]() -> bool {
      if (category != "Dungeon") {
        return false;
      }
      if (is_dungeon_workbench_mode_) {
        return is_dungeon_workbench_mode_();
      }
      return panel_manager_.IsPanelVisible(session_id, "dungeon.workbench");
    };
    bool dungeon_workbench_mode = read_dungeon_workbench_mode();

    auto switch_to_dungeon_workbench_mode = [&]() {
      if (category != "Dungeon") {
        return;
      }
      if (set_dungeon_workflow_mode_) {
        set_dungeon_workflow_mode_(true);
        dungeon_workbench_mode = read_dungeon_workbench_mode();
        return;
      }
      panel_manager_.ShowPanel(session_id, "dungeon.workbench");
      for (const auto& descriptor :
           panel_manager_.GetPanelsInCategory(session_id, "Dungeon")) {
        const std::string& panel_id = descriptor.card_id;
        if (panel_id == "dungeon.workbench") {
          continue;
        }
        if (panel_manager_.IsPanelPinned(session_id, panel_id)) {
          continue;
        }
        if (is_dungeon_panel_mode_card(panel_id)) {
          panel_manager_.HidePanel(session_id, panel_id);
        }
      }
      dungeon_workbench_mode = read_dungeon_workbench_mode();
    };

    auto switch_to_dungeon_panel_mode = [&]() {
      if (category != "Dungeon") {
        return;
      }
      if (set_dungeon_workflow_mode_) {
        set_dungeon_workflow_mode_(false);
        dungeon_workbench_mode = read_dungeon_workbench_mode();
        return;
      }
      panel_manager_.HidePanel(session_id, "dungeon.workbench");
      panel_manager_.ShowPanel(session_id, "dungeon.room_selector");
      panel_manager_.ShowPanel(session_id, "dungeon.room_matrix");
      dungeon_workbench_mode = read_dungeon_workbench_mode();
    };

    auto ensure_dungeon_panel_mode_for_card =
        [&](const std::string& panel_id) -> bool {
      if (category != "Dungeon" || !dungeon_workbench_mode ||
          !is_dungeon_panel_mode_card(panel_id)) {
        return false;
      }
      switch_to_dungeon_panel_mode();
      return true;
    };

    if (category == "Dungeon") {
      ImGui::Spacing();
      ImGui::TextDisabled(ICON_MD_WORKSPACES " Workflow");
      ImGui::SameLine();
      if (gui::TransparentIconButton(
              ICON_MD_WORKSPACES, gui::IconSize::Toolbar(),
              "Workbench mode: integrated room browser + inspector",
              dungeon_workbench_mode, gui::GetPrimaryVec4(), "activity_bar",
              "dungeon_workflow_workbench")) {
        switch_to_dungeon_workbench_mode();
      }
      ImGui::SameLine();
      if (gui::TransparentIconButton(
              ICON_MD_VIEW_QUILT, gui::IconSize::Toolbar(),
              "Panel mode: standalone Room List + Room Matrix + room windows",
              !dungeon_workbench_mode, gui::GetPrimaryVec4(), "activity_bar",
              "dungeon_workflow_panels")) {
        switch_to_dungeon_panel_mode();
      }
      ImGui::SameLine(0.0f, 6.0f);
      ImGui::TextDisabled(dungeon_workbench_mode ? "Workbench" : "Panels");
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();
    }

    ImGui::Spacing();

    // Disable non-emulator categories when no ROM is loaded
    const bool rom_loaded = has_rom ? has_rom() : true;
    const bool disable_cards = !rom_loaded && category != "Emulator";
    if (disable_cards) {
      ImGui::TextUnformatted(ICON_MD_FOLDER_OPEN
                             " Open a ROM to enable this category");
      ImGui::Spacing();
    }

    if (disable_cards) {
      ImGui::BeginDisabled();
    }

    // Get pinned panels
    const auto pinned_cards = panel_manager_.GetPinnedPanels();
    const ImVec4 disabled_text =
        ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    auto panel_text_color = [&](bool visible) -> ImVec4 {
      if (disable_cards) {
        return disabled_text;
      }
      return visible ? gui::GetOnSurfaceVec4() : gui::GetTextSecondaryVec4();
    };
    auto draw_pin_toggle_button = [&](const std::string& widget_id,
                                      bool pinned) -> bool {
      const ImVec4 pin_col =
          pinned ? gui::ConvertColorToImVec4(theme.primary)
                 : gui::ConvertColorToImVec4(theme.text_disabled);
      gui::StyleColorGuard pin_color(ImGuiCol_Text, pin_col);
      gui::StyleVarGuard compact_padding(ImGuiStyleVar_FramePadding,
                                         ImVec2(1.5f, 0.5f));
      return ImGui::SmallButton(
          absl::StrFormat("%s##%s", ICON_MD_PUSH_PIN, widget_id).c_str());
    };

    // --- Pinned Section (panels that persist across editors) ---
    if (sidebar_search[0] == '\0' && !pinned_cards.empty()) {
      bool has_pinned_in_category = false;
      for (const auto& card_id : pinned_cards) {
        const auto* card =
            panel_manager_.GetPanelDescriptor(session_id, card_id);
        if (card && card->category == category) {
          has_pinned_in_category = true;
          break;
        }
      }

      if (has_pinned_in_category) {
        if (ImGui::CollapsingHeader(ICON_MD_PUSH_PIN " Pinned",
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
          for (const auto& card_id : pinned_cards) {
            const auto* card =
                panel_manager_.GetPanelDescriptor(session_id, card_id);
            if (!card || card->category != category)
              continue;

            bool visible =
                card->visibility_flag ? *card->visibility_flag : false;

            // Unpin button
            if (draw_pin_toggle_button("pin_" + card->card_id, true)) {
              panel_manager_.SetPanelPinned(card->card_id, false);
            }
            if (ImGui::IsItemHovered()) {
              ImGui::SetTooltip("Unpin panel");
            }

            ImGui::SameLine(0.0f, 6.0f);

            // Panel Item
            std::string label = absl::StrFormat("%s  %s", card->icon.c_str(),
                                                card->display_name.c_str());
            ImGui::PushID(
                (std::string("pinned_select_") + card->card_id).c_str());
            {
              gui::StyleColorGuard text_color(ImGuiCol_Text,
                                              panel_text_color(visible));
              ImVec2 item_size(ImGui::GetContentRegionAvail().x, 0.0f);
              if (ImGui::Selectable(label.c_str(), visible,
                                    ImGuiSelectableFlags_None, item_size)) {
                ensure_dungeon_panel_mode_for_card(card->card_id);
                panel_manager_.TogglePanel(session_id, card->card_id);

                bool new_visible =
                    card->visibility_flag ? *card->visibility_flag : false;
                if (new_visible) {
                  panel_manager_.TriggerPanelClicked(card->category);
                  const std::string window_name =
                      panel_manager_.GetPanelWindowName(*card);
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

    // Content - panels sorted by MRU (pinned first, then most recently used)
    auto cards = panel_manager_.GetPanelsSortedByMRU(session_id, category);

    // Panels section (uses all available height)
    ImGui::BeginChild("##PanelContent", ImVec2(0, 0), false,
                      ImGuiWindowFlags_None);
    for (const auto& card : cards) {
      // Apply search filter
      if (sidebar_search[0] != '\0') {
        std::string search_str = sidebar_search;
        std::string card_name = card.display_name;
        std::transform(search_str.begin(), search_str.end(), search_str.begin(),
                       ::tolower);
        std::transform(card_name.begin(), card_name.end(), card_name.begin(),
                       ::tolower);
        if (card_name.find(search_str) == std::string::npos) {
          continue;
        }
      }

      bool visible = card.visibility_flag ? *card.visibility_flag : false;

      // Pin Toggle Button
      bool is_pinned = panel_manager_.IsPanelPinned(card.card_id);
      ImGui::PushID(card.card_id.c_str());
      if (draw_pin_toggle_button("pin_" + card.card_id, is_pinned)) {
        panel_manager_.SetPanelPinned(card.card_id, !is_pinned);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(is_pinned
                              ? "Unpin - panel will hide when switching editors"
                              : "Pin - keep visible across all editors");
      }
      ImGui::PopID();
      ImGui::SameLine(0.0f, 6.0f);

      // Panel Item with Icon
      std::string label = absl::StrFormat("%s  %s", card.icon.c_str(),
                                          card.display_name.c_str());
      ImGui::PushID((std::string("panel_select_") + card.card_id).c_str());
      {
        gui::StyleColorGuard text_color(ImGuiCol_Text,
                                        panel_text_color(visible));
        ImVec2 item_size(ImGui::GetContentRegionAvail().x, 0.0f);
        if (ImGui::Selectable(label.c_str(), visible, ImGuiSelectableFlags_None,
                              item_size)) {
          ensure_dungeon_panel_mode_for_card(card.card_id);
          panel_manager_.TogglePanel(session_id, card.card_id);

          bool new_visible =
              card.visibility_flag ? *card.visibility_flag : false;
          if (new_visible) {
            panel_manager_.MarkPanelRecentlyUsed(card.card_id);
            panel_manager_.TriggerPanelClicked(card.category);
            const std::string window_name =
                panel_manager_.GetPanelWindowName(card);
            if (!window_name.empty()) {
              ImGui::SetWindowFocus(window_name.c_str());
            }
          }
        }
      }
      ImGui::PopID();

      // Shortcut hint on hover
      if (ImGui::IsItemHovered() && !card.shortcut_hint.empty()) {
        ImGui::SetTooltip("%s", card.shortcut_hint.c_str());
      }
    }
    ImGui::EndChild();

    if (disable_cards) {
      ImGui::EndDisabled();
    }

    // VSCode-style drag resize handle on the right edge of the side panel.
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
      panel_manager_.ResetSidePanelWidth();
    }
    if (handle_active) {
      const float new_width = panel_width + ImGui::GetIO().MouseDelta.x;
      panel_manager_.SetActiveSidePanelWidth(new_width, viewport->WorkSize.x);
      ImGui::SetTooltip(
          "Width: %.0f px",
          panel_manager_.GetActiveSidePanelWidth(viewport->WorkSize.x));
    }

    ImVec4 handle_color = gui::GetOutlineVec4();
    handle_color.w = handle_active ? 0.95f : (handle_hovered ? 0.72f : 0.35f);
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(panel_pos.x + panel_width - 1.0f, panel_pos.y),
        ImVec2(panel_pos.x + panel_width - 1.0f,
               panel_pos.y + panel_draw_height),
        ImGui::GetColorU32(handle_color), handle_active ? 2.0f : 1.0f);
  }
  // FixedPanel destructor handles End() + PopStyleVar/PopStyleColor
}

void ActivityBar::DrawPanelBrowser(size_t session_id, bool* p_open) {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImVec2 max_window_size(1600.0f, 1000.0f);
  ImVec2 default_window_size(1080.0f, 700.0f);
  if (viewport) {
    max_window_size = ImVec2(std::max(760.0f, viewport->WorkSize.x * 0.95f),
                             std::max(520.0f, viewport->WorkSize.y * 0.95f));
    default_window_size = ImVec2(
        std::clamp(viewport->WorkSize.x * 0.72f, 880.0f, max_window_size.x),
        std::clamp(viewport->WorkSize.y * 0.76f, 600.0f, max_window_size.y));
    const ImVec2 center(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                        viewport->WorkPos.y + viewport->WorkSize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  }

  ImGui::SetNextWindowSize(default_window_size, ImGuiCond_Appearing);
  ImGui::SetNextWindowSizeConstraints(ImVec2(780, 520), max_window_size);

  if (ImGui::Begin(
          absl::StrFormat("%s Panel Browser", ICON_MD_DASHBOARD).c_str(),
          p_open, ImGuiWindowFlags_NoDocking)) {
    static char search_filter[256] = "";
    static std::string category_filter = "All";

    std::vector<std::string> categories =
        panel_manager_.GetAllCategories(session_id);
    std::sort(categories.begin(), categories.end());
    if (category_filter != "All" &&
        std::find(categories.begin(), categories.end(), category_filter) ==
            categories.end()) {
      category_filter = "All";
    }

    const auto all_cards = panel_manager_.GetPanelsInSession(session_id);
    auto count_cards = [&](const std::string& category,
                           bool visible_only) -> int {
      int count = 0;
      for (const auto& card_id : all_cards) {
        const auto* card =
            panel_manager_.GetPanelDescriptor(session_id, card_id);
        if (!card) {
          continue;
        }
        if (category != "All" && card->category != category) {
          continue;
        }
        const bool visible = card->visibility_flag && *card->visibility_flag;
        if (!visible_only || visible) {
          ++count;
        }
      }
      return count;
    };

    ImGui::SetNextItemWidth(
        std::max(280.0f, ImGui::GetContentRegionAvail().x * 0.45f));
    ImGui::InputTextWithHint(
        "##Search",
        absl::StrFormat("%s Search panels...", ICON_MD_SEARCH).c_str(),
        search_filter, sizeof(search_filter));
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_CLEAR " Clear")) {
      search_filter[0] = '\0';
    }
    ImGui::SameLine();
    if (category_filter == "All") {
      if (ImGui::Button(ICON_MD_VISIBILITY " Show All")) {
        panel_manager_.ShowAllPanelsInSession(session_id);
      }
      ImGui::SameLine();
      if (ImGui::Button(ICON_MD_VISIBILITY_OFF " Hide All")) {
        panel_manager_.HideAllPanelsInSession(session_id);
      }
    } else {
      if (ImGui::Button(ICON_MD_VISIBILITY " Show Category")) {
        panel_manager_.ShowAllPanelsInCategory(session_id, category_filter);
      }
      ImGui::SameLine();
      if (ImGui::Button(ICON_MD_VISIBILITY_OFF " Hide Category")) {
        panel_manager_.HideAllPanelsInCategory(session_id, category_filter);
      }
    }

    ImGui::Separator();

    const float content_height = ImGui::GetContentRegionAvail().y;
    const float max_sidebar_width =
        std::max(220.0f, ImGui::GetContentRegionAvail().x * 0.50f);
    float category_sidebar_width =
        std::clamp(panel_manager_.GetPanelBrowserCategoryWidth(), 220.0f,
                   max_sidebar_width);

    if (ImGui::BeginChild("##PanelBrowserCategories",
                          ImVec2(category_sidebar_width, content_height),
                          true)) {
      const int visible_total = count_cards("All", true);
      std::string all_label =
          absl::StrFormat("%s All Panels (%d/%d)", ICON_MD_DASHBOARD,
                          visible_total, static_cast<int>(all_cards.size()));
      if (ImGui::Selectable(all_label.c_str(), category_filter == "All")) {
        category_filter = "All";
      }
      ImGui::Separator();

      for (const auto& cat : categories) {
        const int category_total = count_cards(cat, false);
        if (category_total <= 0) {
          continue;
        }
        const int visible_in_category = count_cards(cat, true);
        const std::string icon = PanelManager::GetCategoryIcon(cat);
        std::string label =
            absl::StrFormat("%s %s (%d/%d)", icon.c_str(), cat.c_str(),
                            visible_in_category, category_total);
        if (ImGui::Selectable(label.c_str(), category_filter == cat)) {
          category_filter = cat;
        }
      }
    }
    ImGui::EndChild();

    ImGui::SameLine(0.0f, 0.0f);

    const float splitter_width = 6.0f;
    const ImVec2 splitter_pos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##PanelBrowserSplitter",
                           ImVec2(splitter_width, content_height));
    const bool splitter_hovered = ImGui::IsItemHovered();
    const bool splitter_active = ImGui::IsItemActive();
    if (splitter_hovered || splitter_active) {
      ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    if (splitter_hovered &&
        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      panel_manager_.SetPanelBrowserCategoryWidth(
          PanelManager::GetDefaultPanelBrowserCategoryWidth());
      category_sidebar_width =
          std::clamp(panel_manager_.GetPanelBrowserCategoryWidth(), 220.0f,
                     max_sidebar_width);
    }
    if (splitter_active) {
      category_sidebar_width =
          std::clamp(category_sidebar_width + ImGui::GetIO().MouseDelta.x,
                     220.0f, max_sidebar_width);
      panel_manager_.SetPanelBrowserCategoryWidth(category_sidebar_width);
      ImGui::SetTooltip("Width: %.0f px", category_sidebar_width);
    }

    ImVec4 splitter_color = gui::GetOutlineVec4();
    splitter_color.w =
        splitter_active ? 0.95f : (splitter_hovered ? 0.72f : 0.35f);
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(splitter_pos.x + splitter_width * 0.5f, splitter_pos.y),
        ImVec2(splitter_pos.x + splitter_width * 0.5f,
               splitter_pos.y + content_height),
        ImGui::GetColorU32(splitter_color), splitter_active ? 2.0f : 1.0f);

    ImGui::SameLine(0.0f, 0.0f);

    if (ImGui::BeginChild("##PanelBrowserTable", ImVec2(0, content_height),
                          false)) {
      if (ImGui::BeginTable("##PanelTable", 5,
                            ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_Borders |
                                ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Visible", ImGuiTableColumnFlags_WidthFixed,
                                60);
        ImGui::TableSetupColumn("Pin", ImGuiTableColumnFlags_WidthFixed, 44);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed,
                                130);
        ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed,
                                110);
        ImGui::TableHeadersRow();

        auto cards =
            (category_filter == "All") ? all_cards : std::vector<std::string>{};
        if (category_filter != "All") {
          auto category_cards =
              panel_manager_.GetPanelsInCategory(session_id, category_filter);
          cards.reserve(category_cards.size());
          for (const auto& card : category_cards) {
            cards.push_back(card.card_id);
          }
        }

        for (const auto& card_id : cards) {
          const auto* card =
              panel_manager_.GetPanelDescriptor(session_id, card_id);
          if (!card) {
            continue;
          }

          std::string search_str = search_filter;
          if (!search_str.empty()) {
            std::string card_lower = card->display_name;
            std::transform(card_lower.begin(), card_lower.end(),
                           card_lower.begin(), [](unsigned char c) {
                             return static_cast<char>(std::tolower(c));
                           });
            std::transform(search_str.begin(), search_str.end(),
                           search_str.begin(), [](unsigned char c) {
                             return static_cast<char>(std::tolower(c));
                           });
            if (card_lower.find(search_str) == std::string::npos) {
              continue;
            }
          }

          ImGui::TableNextRow();

          ImGui::TableNextColumn();
          if (card->visibility_flag) {
            bool visible = *card->visibility_flag;
            if (ImGui::Checkbox(
                    absl::StrFormat("##vis_%s", card->card_id.c_str()).c_str(),
                    &visible)) {
              panel_manager_.TogglePanel(session_id, card->card_id);
            }
          }

          ImGui::TableNextColumn();
          const bool is_pinned = panel_manager_.IsPanelPinned(card->card_id);
          const ImVec4 pin_color =
              is_pinned ? gui::GetPrimaryVec4() : gui::GetTextDisabledVec4();
          {
            gui::StyleColorGuard pin_text(ImGuiCol_Text, pin_color);
            if (ImGui::SmallButton(absl::StrFormat("%s##pin_%s",
                                                   ICON_MD_PUSH_PIN,
                                                   card->card_id.c_str())
                                       .c_str())) {
              panel_manager_.SetPanelPinned(card->card_id, !is_pinned);
            }
          }
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(is_pinned ? "Unpin panel" : "Pin panel");
          }

          ImGui::TableNextColumn();
          ImGui::Text("%s %s", card->icon.c_str(), card->display_name.c_str());
          if (ImGui::IsItemHovered() &&
              ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            panel_manager_.ShowPanel(session_id, card->card_id);
            const std::string window_name =
                panel_manager_.GetPanelWindowName(*card);
            if (!window_name.empty()) {
              ImGui::SetWindowFocus(window_name.c_str());
            }
          }

          ImGui::TableNextColumn();
          ImGui::TextUnformatted(card->category.c_str());

          ImGui::TableNextColumn();
          if (card->shortcut_hint.empty()) {
            ImGui::TextDisabled("-");
          } else {
            ImGui::TextDisabled("%s", card->shortcut_hint.c_str());
          }
        }

        ImGui::EndTable();
      }
    }
    ImGui::EndChild();
  }
  ImGui::End();
}

}  // namespace editor
}  // namespace yaze
