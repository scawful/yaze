#include "app/editor/menu/activity_bar.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <functional>
#include <string>
#include <unordered_set>
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

ActivityBar::ActivityBar(PanelManager& panel_manager)
    : panel_manager_(panel_manager) {}

void ActivityBar::Render(
    size_t session_id, const std::string& active_category,
    const std::vector<std::string>& all_categories,
    const std::unordered_set<std::string>& active_editor_categories,
    std::function<bool()> has_rom) {
  if (!panel_manager_.IsSidebarVisible())
    return;

  DrawActivityBarStrip(session_id, active_category, all_categories,
                       active_editor_categories, has_rom);

  if (panel_manager_.IsPanelExpanded()) {
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
  const float viewport_height =
      std::max(0.0f, viewport->WorkSize.y - top_inset);
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
    if (gui::TransparentIconButton(ICON_MD_SEARCH,
                                   gui::IconSize::ActivityBar(),
                                   "Global Search (Ctrl+Shift+F)", false,
                                   ImVec4(0, 0, 0, 0), "activity_bar",
                                   "search")) {
      panel_manager_.TriggerShowSearch();
    }

    // Separator
    ImGui::Spacing();
    ImVec2 sep_p1 = ImGui::GetCursorScreenPos();
    ImVec2 sep_p2 = ImVec2(sep_p1.x + gui::UIConfig::kActivityBarWidth,
                           sep_p1.y);
    ImGui::GetWindowDrawList()->AddLine(
        sep_p1, sep_p2,
        ImGui::ColorConvertFloat4ToU32(gui::ConvertColorToImVec4(theme.border)),
        1.0f);
    ImGui::Spacing();

    bool rom_loaded = has_rom ? has_rom() : false;

    // Draw ALL editor categories (not just active ones)
    for (const auto& cat : all_categories) {
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
          gui::ColoredText("Click to view panels",
                           gui::GetTextSecondaryVec4());
        }
        ImGui::EndTooltip();
      }
    }
  }

  // Draw "More Actions" button at the bottom
  ImGui::SetCursorPosY(viewport_height - 48.0f);

  if (gui::TransparentIconButton(
          ICON_MD_MORE_HORIZ, gui::IconSize::Large(), nullptr, false,
          ImVec4(0, 0, 0, 0), "activity_bar", "more_actions")) {
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
      PanelManager::GetSidePanelWidthForViewport(viewport->WorkSize.x);

  const float top_inset = gui::LayoutHelpers::GetTopInset();
  const float panel_height =
      std::max(0.0f, viewport->WorkSize.y - top_inset);

  gui::FixedPanel panel(
      "##SidePanel",
      ImVec2(viewport->WorkPos.x + bar_width,
             viewport->WorkPos.y + top_inset),
      ImVec2(panel_width, panel_height),
      {.bg = gui::ConvertColorToImVec4(theme.surface),
       .padding = {12.0f, 12.0f},
       .border_size = 1.0f,
       .rounding = 0.0f},
      ImGuiWindowFlags_NoFocusOnAppearing);

  if (panel) {
    // Header
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);  // Use default font
    ImGui::Text("%s", category.c_str());
    ImGui::PopFont();

    // Header Buttons (Right Aligned)
    float avail_width = ImGui::GetContentRegionAvail().x;
    float button_size = 28.0f;
    float spacing = 4.0f;
    float current_x = ImGui::GetCursorPosX() + avail_width - button_size;

    // Collapse Button (rightmost)
    ImGui::SameLine(current_x);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4.0f);
    if (ImGui::Button(ICON_MD_KEYBOARD_DOUBLE_ARROW_LEFT,
                      ImVec2(button_size, button_size))) {
      panel_manager_.SetPanelExpanded(false);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Collapse Panel");
    }

    // Close All Panels Button
    current_x -= (button_size + spacing);
    ImGui::SameLine(current_x);
    if (ImGui::Button(ICON_MD_CLOSE_FULLSCREEN,
                      ImVec2(button_size, button_size))) {
      panel_manager_.HideAllPanelsInCategory(session_id, category);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Close All Panels");
    }

    // Expand All Panels Button
    current_x -= (button_size + spacing);
    ImGui::SameLine(current_x);
    if (ImGui::Button(ICON_MD_OPEN_IN_FULL, ImVec2(button_size, button_size))) {
      panel_manager_.ShowAllPanelsInCategory(session_id, category);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Show All Panels");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Search Bar
    static char sidebar_search[256] = "";
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##SidebarSearch", ICON_MD_SEARCH " Filter...",
                             sidebar_search, sizeof(sidebar_search));
    ImGui::Spacing();

    const auto* agent_chat =
        panel_manager_.GetPanelDescriptor(session_id, "agent.chat");
    const auto* agent_config =
        panel_manager_.GetPanelDescriptor(session_id, "agent.configuration");
    const auto* agent_builder =
        panel_manager_.GetPanelDescriptor(session_id, "agent.builder");
    if (agent_chat || agent_config || agent_builder) {
      if (ImGui::CollapsingHeader(ICON_MD_SMART_TOY " Agent",
                                  ImGuiTreeNodeFlags_DefaultOpen)) {
        if (agent_chat) {
          if (ImGui::SmallButton(ICON_MD_CHAT " Chat")) {
            panel_manager_.ShowPanel(session_id, agent_chat->card_id);
          }
          ImGui::SameLine();
        }
        if (agent_config) {
          if (ImGui::SmallButton(ICON_MD_SETTINGS " Config")) {
            panel_manager_.ShowPanel(session_id, agent_config->card_id);
          }
          ImGui::SameLine();
        }
        if (agent_builder) {
          if (ImGui::SmallButton(ICON_MD_AUTO_FIX_HIGH " Builder")) {
            panel_manager_.ShowPanel(session_id, agent_builder->card_id);
          }
        }
        if (category != "Agent") {
          ImGui::Spacing();
          if (ImGui::SmallButton(ICON_MD_CHEVRON_RIGHT " Open Agent Sidebar")) {
            panel_manager_.SetActiveCategory("Agent");
            panel_manager_.SetPanelExpanded(true);
          }
        }
      }
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();
    }

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
            {
              gui::StyleColorGuard pin_color(
                  ImGuiCol_Text, gui::ConvertColorToImVec4(theme.primary));
              if (ImGui::SmallButton(
                      (std::string(ICON_MD_PUSH_PIN "##pin_") + card->card_id)
                          .c_str())) {
                panel_manager_.SetPanelPinned(card->card_id, false);
              }
            }
            if (ImGui::IsItemHovered()) {
              ImGui::SetTooltip("Unpin panel");
            }

            ImGui::SameLine();

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
                panel_manager_.TogglePanel(session_id, card->card_id);

                bool new_visible =
                    card->visibility_flag ? *card->visibility_flag : false;
                if (new_visible) {
                  panel_manager_.TriggerPanelClicked(card->category);
                  ImGui::SetWindowFocus(card->GetWindowTitle().c_str());
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

    // Calculate available height for cards vs file browser
    float available_height = ImGui::GetContentRegionAvail().y;
    bool has_file_browser = panel_manager_.HasFileBrowser(category);
    float cards_height =
        has_file_browser ? available_height * 0.4f : available_height;
    float file_browser_height = available_height - cards_height - 30.0f;

    // Panels section
    ImGui::BeginChild("##PanelContent", ImVec2(0, cards_height), false,
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
      {
        ImVec4 pin_col =
            is_pinned ? gui::ConvertColorToImVec4(theme.primary)
                      : gui::ConvertColorToImVec4(theme.text_disabled);
        gui::StyleColorGuard pin_color(ImGuiCol_Text, pin_col);
        if (ImGui::SmallButton(ICON_MD_PUSH_PIN)) {
          panel_manager_.SetPanelPinned(card.card_id, !is_pinned);
        }
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(is_pinned
                              ? "Unpin - panel will hide when switching editors"
                              : "Pin - keep visible across all editors");
      }
      ImGui::PopID();
      ImGui::SameLine();

      // Panel Item with Icon
      std::string label = absl::StrFormat("%s  %s", card.icon.c_str(),
                                          card.display_name.c_str());
      ImGui::PushID((std::string("panel_select_") + card.card_id).c_str());
      {
        gui::StyleColorGuard text_color(ImGuiCol_Text,
                                        panel_text_color(visible));
        ImVec2 item_size(ImGui::GetContentRegionAvail().x, 0.0f);
        if (ImGui::Selectable(label.c_str(), visible,
                              ImGuiSelectableFlags_None, item_size)) {
          panel_manager_.TogglePanel(session_id, card.card_id);

          bool new_visible =
              card.visibility_flag ? *card.visibility_flag : false;
          if (new_visible) {
            panel_manager_.MarkPanelRecentlyUsed(card.card_id);
            panel_manager_.TriggerPanelClicked(card.category);
            ImGui::SetWindowFocus(card.GetWindowTitle().c_str());
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

    // File browser section (if enabled for this category)
    if (has_file_browser) {
      ImGui::Spacing();
      ImGui::Separator();

      // Collapsible header for file browser
      gui::StyleColorGuard header_colors({
          {ImGuiCol_Header, gui::GetSurfaceContainerHighVec4()},
          {ImGuiCol_HeaderHovered, gui::GetSurfaceContainerHighestVec4()},
      });
      bool files_expanded = ImGui::CollapsingHeader(
          ICON_MD_FOLDER " Files", ImGuiTreeNodeFlags_DefaultOpen);

      if (files_expanded) {
        ImGui::BeginChild("##FileBrowser", ImVec2(0, file_browser_height),
                          false, ImGuiWindowFlags_None);
        auto* browser = panel_manager_.GetFileBrowser(category);
        if (browser) {
          browser->DrawCompact();
        }
        ImGui::EndChild();
      }
    }

    if (disable_cards) {
      ImGui::EndDisabled();
    }
  }
  // FixedPanel destructor handles End() + PopStyleVar/PopStyleColor
}

void ActivityBar::DrawPanelBrowser(size_t session_id, bool* p_open) {
  ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

  if (ImGui::Begin(
          absl::StrFormat("%s Panel Browser", ICON_MD_DASHBOARD).c_str(),
          p_open)) {
    static char search_filter[256] = "";
    static std::string category_filter = "All";

    // Search bar
    ImGui::SetNextItemWidth(300);
    ImGui::InputTextWithHint(
        "##Search",
        absl::StrFormat("%s Search panels...", ICON_MD_SEARCH).c_str(),
        search_filter, sizeof(search_filter));

    ImGui::SameLine();

    // Category filter
    if (ImGui::BeginCombo("##CategoryFilter", category_filter.c_str())) {
      if (ImGui::Selectable("All", category_filter == "All")) {
        category_filter = "All";
      }

      auto categories = panel_manager_.GetAllCategories(session_id);
      for (const auto& cat : categories) {
        if (ImGui::Selectable(cat.c_str(), category_filter == cat)) {
          category_filter = cat;
        }
      }
      ImGui::EndCombo();
    }

    ImGui::Separator();

    // Panel table
    if (ImGui::BeginTable("##PanelTable", 4,
                          ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Borders)) {
      ImGui::TableSetupColumn("Visible", ImGuiTableColumnFlags_WidthFixed, 60);
      ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed,
                              120);
      ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed,
                              100);
      ImGui::TableHeadersRow();

      auto cards = (category_filter == "All")
                       ? panel_manager_.GetPanelsInSession(session_id)
                       : std::vector<std::string>{};

      if (category_filter != "All") {
        auto cat_cards =
            panel_manager_.GetPanelsInCategory(session_id, category_filter);
        for (const auto& card : cat_cards) {
          cards.push_back(card.card_id);
        }
      }

      for (const auto& card_id : cards) {
        const auto* card =
            panel_manager_.GetPanelDescriptor(session_id, card_id);
        if (!card)
          continue;

        // Apply search filter
        std::string search_str = search_filter;
        if (!search_str.empty()) {
          std::string card_lower = card->display_name;
          std::transform(card_lower.begin(), card_lower.end(),
                         card_lower.begin(), ::tolower);
          std::transform(search_str.begin(), search_str.end(),
                         search_str.begin(), ::tolower);
          if (card_lower.find(search_str) == std::string::npos) {
            continue;
          }
        }

        ImGui::TableNextRow();

        // Visibility toggle
        ImGui::TableNextColumn();
        if (card->visibility_flag) {
          bool visible = *card->visibility_flag;
          if (ImGui::Checkbox(
                  absl::StrFormat("##vis_%s", card->card_id.c_str()).c_str(),
                  &visible)) {
            panel_manager_.TogglePanel(session_id, card->card_id);
            // Note: TogglePanel handles callbacks
          }
        }

        // Name with icon
        ImGui::TableNextColumn();
        ImGui::Text("%s %s", card->icon.c_str(), card->display_name.c_str());

        // Category
        ImGui::TableNextColumn();
        ImGui::Text("%s", card->category.c_str());

        // Shortcut
        ImGui::TableNextColumn();
        ImGui::TextDisabled("%s", card->shortcut_hint.c_str());
      }

      ImGui::EndTable();
    }
  }
  ImGui::End();
}

}  // namespace editor
}  // namespace yaze
