#include "app/editor/menu/activity_bar.h"

#include <algorithm>

#include "absl/strings/str_format.h"
#include "app/editor/system/panel_manager.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/widgets/themed_widgets.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze {
namespace editor {

ActivityBar::ActivityBar(PanelManager& panel_manager)
    : panel_manager_(panel_manager) {}

void ActivityBar::Render(size_t session_id,
                         const std::string& active_category,
                         const std::vector<std::string>& all_categories,
                         const std::unordered_set<std::string>& active_editor_categories,
                         std::function<bool()> has_rom) {
  if (!panel_manager_.IsSidebarVisible()) return;

  DrawActivityBarStrip(session_id, active_category, all_categories,
                       active_editor_categories, has_rom);

  if (panel_manager_.IsPanelExpanded()) {
    DrawSidePanel(session_id, active_category, has_rom);
  }
}

void ActivityBar::DrawUtilityButtons(std::function<bool()> has_rom) {
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  bool rom_loaded = has_rom ? has_rom() : false;

  // Save ROM button
  {
    if (gui::TransparentIconButton(ICON_MD_FOLDER_OPEN, ImVec2(48.0f, 40.0f),
                                   "Open ROM (Ctrl+O)", false)) {
      panel_manager_.TriggerOpenRom();
    }

    if (gui::TransparentIconButton(ICON_MD_SAVE, ImVec2(48.0f, 40.0f),
                                   "Save ROM (Ctrl+S)", false)) {
      if (rom_loaded) panel_manager_.TriggerSaveRom();
    }

    // Undo
    if (gui::TransparentIconButton(ICON_MD_UNDO, ImVec2(48.0f, 40.0f),
                                   "Undo (Ctrl+Z)", false)) {
      if (rom_loaded) panel_manager_.TriggerUndo();
    }

    // Redo
    if (gui::TransparentIconButton(ICON_MD_REDO, ImVec2(48.0f, 40.0f),
                                   "Redo (Ctrl+Y)", false)) {
      if (rom_loaded) panel_manager_.TriggerRedo();
    }

    // Search
    if (gui::TransparentIconButton(ICON_MD_SEARCH, ImVec2(48.0f, 40.0f),
                                   "Global Search (Ctrl+Shift+F)", false)) {
      panel_manager_.TriggerShowSearch();
    }

    // Help
    if (gui::TransparentIconButton(ICON_MD_HELP_OUTLINE, ImVec2(48.0f, 40.0f),
                                   "Help (F1)", false)) {
      panel_manager_.TriggerShowHelp();
    }
  }

  // Separator line between utility buttons and editor categories
  ImGui::Spacing();
  ImVec2 sep_p1 = ImGui::GetCursorScreenPos();
  ImVec2 sep_p2 = ImVec2(sep_p1.x + 48.0f, sep_p1.y);
  ImGui::GetWindowDrawList()->AddLine(
      sep_p1, sep_p2, 
      ImGui::ColorConvertFloat4ToU32(gui::ConvertColorToImVec4(theme.border)),
      1.0f);
  ImGui::Spacing();
}

void ActivityBar::DrawActivityBarStrip(
    size_t session_id, const std::string& active_category,
    const std::vector<std::string>& all_categories,
    const std::unordered_set<std::string>& active_editor_categories,
    std::function<bool()> has_rom) {
  
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float viewport_height = viewport->WorkSize.y;
  const float bar_width = 48.0f; // Fixed width for Activity Bar

  // Position on left edge, full height
  ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y));
  ImGui::SetNextWindowSize(ImVec2(bar_width, viewport_height));

  ImVec4 bar_bg = gui::ConvertColorToImVec4(theme.surface);
  ImVec4 bar_border = gui::ConvertColorToImVec4(theme.text_disabled);

  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavFocus | 
      ImGuiWindowFlags_NoBringToFrontOnFocus;

  ImGui::PushStyleColor(ImGuiCol_WindowBg, bar_bg);
  ImGui::PushStyleColor(ImGuiCol_Border, bar_border);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 8.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 4.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

  if (ImGui::Begin("##ActivityBar", nullptr, flags)) {
    // Draw utility action buttons at the top
    DrawUtilityButtons(has_rom);

    bool rom_loaded = has_rom ? has_rom() : false;

    // Draw ALL editor categories (not just active ones)
    for (const auto& cat : all_categories) {
      bool is_selected = (cat == active_category) && panel_manager_.IsPanelExpanded();
      bool has_active_editor = active_editor_categories.count(cat) > 0;
      
      // Emulator is always available, others require ROM
      bool category_enabled = rom_loaded || (cat == "Emulator");

      // Active Indicator (Left Border) - shown when category is selected
      if (is_selected && category_enabled) {
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImGui::GetCursorScreenPos(),
            ImVec2(ImGui::GetCursorScreenPos().x + 3.0f, 
                   ImGui::GetCursorScreenPos().y + 40.0f),
            ImGui::ColorConvertFloat4ToU32(gui::ConvertColorToImVec4(theme.primary))); 
      }

      std::string icon = PanelManager::GetCategoryIcon(cat);
      
      // Use ThemedWidgets for consistent styling
      if (gui::TransparentIconButton(icon.c_str(), ImVec2(48.0f, 40.0f), 
                                                  nullptr, is_selected)) {
        if (category_enabled) {
          if (cat == active_category && panel_manager_.IsPanelExpanded()) {
            panel_manager_.TogglePanelExpanded();
          } else {
            panel_manager_.SetActiveCategory(cat);
            panel_manager_.SetPanelExpanded(true);
          }
        }
      }

      // Tooltip with status information
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::BeginTooltip();
        ImGui::Text("%s %s", icon.c_str(), cat.c_str());
        if (!category_enabled) {
          ImGui::TextColored(gui::ConvertColorToImVec4(theme.warning), 
                            "Open ROM required");
        } else if (has_active_editor) {
          ImGui::PushStyleColor(ImGuiCol_Text, gui::ConvertColorToImVec4(theme.success));
          ImGui::TextUnformatted("Editor open");
          ImGui::PopStyleColor();
        } else {
          ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextSecondaryVec4());
          ImGui::TextUnformatted("Click to view cards");
          ImGui::PopStyleColor();
        }
        ImGui::EndTooltip();
      }
    }
  }

  // Draw "More Actions" button at the bottom
  ImGui::SetCursorPosY(viewport_height - 48.0f);
  
  if (gui::TransparentIconButton(ICON_MD_MORE_HORIZ, ImVec2(48.0f, 48.0f))) {
    ImGui::OpenPopup("ActivityBarMoreMenu");
  }

  if (ImGui::BeginPopup("ActivityBarMoreMenu")) {
    if (ImGui::MenuItem(ICON_MD_FOLDER_OPEN " Open ROM")) {
      panel_manager_.TriggerOpenRom();
    }
    if (ImGui::MenuItem(ICON_MD_SETTINGS " Settings")) {
      panel_manager_.TriggerShowSettings();
    }
    if (ImGui::MenuItem(ICON_MD_HELP_OUTLINE " Help")) {
      panel_manager_.TriggerShowHelp();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Reset Layout")) {
      // TODO: Implement layout reset
    }
    ImGui::EndPopup();
  }

  ImGui::End();

  ImGui::PopStyleVar(3);
  ImGui::PopStyleColor(2);
}

void ActivityBar::DrawSidePanel(
    size_t session_id, const std::string& category,
    std::function<bool()> has_rom) {
    
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float bar_width = PanelManager::GetSidebarWidth();
  const float panel_width = PanelManager::GetSidePanelWidth();

  ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + bar_width, viewport->WorkPos.y));
  ImGui::SetNextWindowSize(ImVec2(panel_width, viewport->WorkSize.y));

  ImVec4 panel_bg = gui::ConvertColorToImVec4(theme.surface); 
  
  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoFocusOnAppearing;

  ImGui::PushStyleColor(ImGuiCol_WindowBg, panel_bg);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f); // Right border
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f)); // Consistent padding

  if (ImGui::Begin("##SidePanel", nullptr, flags)) {
    // Header
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);  // Use default font
    ImGui::Text("%s", category.c_str());
    ImGui::PopFont();

    // Header Buttons (Right Aligned)
    float avail_width = ImGui::GetContentRegionAvail().x;
    float button_size = 24.0f;
    float spacing = 4.0f;
    float current_x = ImGui::GetCursorPosX() + avail_width - button_size;

    // Collapse Button
    ImGui::SameLine(current_x);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4.0f);
    if (ImGui::Button(ICON_MD_KEYBOARD_DOUBLE_ARROW_LEFT, ImVec2(button_size, button_size))) {
      panel_manager_.SetPanelExpanded(false);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Collapse Panel");
    }

    // Close All Button
    current_x -= (button_size + spacing);
    ImGui::SameLine(current_x);
    if (ImGui::Button(ICON_MD_CLOSE_FULLSCREEN, ImVec2(button_size, button_size))) {
      panel_manager_.HideAllPanelsInCategory(session_id, category);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Close All Cards in Category");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Search Bar
    static char sidebar_search[256] = "";
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##SidebarSearch", ICON_MD_SEARCH " Filter...", sidebar_search, sizeof(sidebar_search));
    ImGui::Spacing();

    // Disable non-emulator categories when no ROM is loaded
    const bool rom_loaded = has_rom ? has_rom() : true;
    const bool disable_cards = !rom_loaded && category != "Emulator";
    if (disable_cards) {
      ImGui::TextUnformatted(
          ICON_MD_FOLDER_OPEN " Open a ROM to enable this category");
      ImGui::Spacing();
    }

    if (disable_cards) {
      ImGui::BeginDisabled();
    }

    // Note: Favorites and Recent logic uses private members of PanelManager.
    // We need to expose them via public methods in PanelManager.
    const auto& favorite_cards = panel_manager_.GetFavoriteCards();
    const auto& recent_cards = panel_manager_.GetRecentCards();

    // --- Favorites Section ---
    if (sidebar_search[0] == '\0' && !favorite_cards.empty()) {
      bool has_favorites_in_category = false;
      // Check if there are any favorites in this category
      for (const auto& card_id : favorite_cards) {
        const auto* card = panel_manager_.GetPanelDescriptor(session_id, card_id);
        if (card && card->category == category) {
          has_favorites_in_category = true;
          break;
        }
      }

      if (has_favorites_in_category) {
        if (ImGui::CollapsingHeader(ICON_MD_STAR " Favorites", ImGuiTreeNodeFlags_DefaultOpen)) {
          for (const auto& card_id : favorite_cards) {
            const auto* card = panel_manager_.GetPanelDescriptor(session_id, card_id);
            if (!card || card->category != category) continue;
            
            bool visible = card->visibility_flag ? *card->visibility_flag : false;
            
            // Star button (filled)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.0f, 1.0f)); // Gold
            if (ImGui::SmallButton((std::string(ICON_MD_STAR "##fav_") + card->card_id).c_str())) {
              panel_manager_.ToggleFavorite(card->card_id);
            }
            ImGui::PopStyleColor();
            
            ImGui::SameLine();
            
            // Card Item
            std::string label = absl::StrFormat("%s  %s", card->icon.c_str(), card->display_name.c_str());
            if (ImGui::Selectable(label.c_str(), visible)) {
              panel_manager_.TogglePanel(session_id, card->card_id);
              
              bool new_visible = card->visibility_flag ? *card->visibility_flag : false;
              if (new_visible) {
                panel_manager_.TriggerPanelClicked(card->category);
                ImGui::SetWindowFocus(card->GetWindowTitle().c_str());
              }
            }
          }
          ImGui::Spacing();
          ImGui::Separator();
          ImGui::Spacing();
        }
      }
    }

    // --- Recent Section ---
    if (sidebar_search[0] == '\0' && !recent_cards.empty()) {
      bool has_recents_in_category = false;
      for (const auto& card_id : recent_cards) {
        const auto* card = panel_manager_.GetPanelDescriptor(session_id, card_id);
        if (card && card->category == category) {
          has_recents_in_category = true;
          break;
        }
      }

      if (has_recents_in_category) {
        if (ImGui::CollapsingHeader(ICON_MD_HISTORY " Recent", ImGuiTreeNodeFlags_DefaultOpen)) {
          for (const auto& card_id : recent_cards) {
            const auto* card = panel_manager_.GetPanelDescriptor(session_id, card_id);
            if (!card || card->category != category) continue;
            
            bool visible = card->visibility_flag ? *card->visibility_flag : false;
            
            // Favorite Toggle Button
            bool is_fav = panel_manager_.IsFavorite(card->card_id);
            ImGui::PushID((std::string("recent_") + card->card_id).c_str());
            if (is_fav) {
              ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.0f, 1.0f)); // Gold
              if (ImGui::SmallButton(ICON_MD_STAR)) {
                panel_manager_.ToggleFavorite(card->card_id);
              }
              ImGui::PopStyleColor();
            } else {
              ImGui::PushStyleColor(ImGuiCol_Text, gui::ConvertColorToImVec4(theme.text_disabled));
              if (ImGui::SmallButton(ICON_MD_STAR_BORDER)) {
                panel_manager_.ToggleFavorite(card->card_id);
              }
              ImGui::PopStyleColor();
            }
            ImGui::PopID();
            ImGui::SameLine();
            
            // Card Item
            std::string label = absl::StrFormat("%s  %s", card->icon.c_str(), card->display_name.c_str());
            if (ImGui::Selectable(label.c_str(), visible)) {
              panel_manager_.TogglePanel(session_id, card->card_id);
              
              bool new_visible = card->visibility_flag ? *card->visibility_flag : false;
              if (new_visible) {
                panel_manager_.AddToRecent(card->card_id); // Move to top
                panel_manager_.TriggerPanelClicked(card->category);
                ImGui::SetWindowFocus(card->GetWindowTitle().c_str());
              }
            }
          }
          ImGui::Spacing();
          ImGui::Separator();
          ImGui::Spacing();
        }
      }
    }

    // Content - Reusing GetPanelsInCategory logic
    auto cards = panel_manager_.GetPanelsInCategory(session_id, category);

    // Calculate available height for cards vs file browser
    float available_height = ImGui::GetContentRegionAvail().y;
    bool has_file_browser = panel_manager_.HasFileBrowser(category);
    float cards_height =
        has_file_browser ? available_height * 0.4f : available_height;
    float file_browser_height = available_height - cards_height - 30.0f;

    // Cards section
    ImGui::BeginChild("##PanelContent", ImVec2(0, cards_height), false,
                      ImGuiWindowFlags_None);
    for (const auto& card : cards) {
      // Apply search filter
      if (sidebar_search[0] != '\0') {
        std::string search_str = sidebar_search;
        std::string card_name = card.display_name;
        std::transform(search_str.begin(), search_str.end(), search_str.begin(), ::tolower);
        std::transform(card_name.begin(), card_name.end(), card_name.begin(), ::tolower);
        if (card_name.find(search_str) == std::string::npos) {
          continue;
        }
      }

      bool visible = card.visibility_flag ? *card.visibility_flag : false;

      // Favorite Toggle Button
      bool is_fav = panel_manager_.IsFavorite(card.card_id);
      ImGui::PushID(card.card_id.c_str());
      if (is_fav) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.0f, 1.0f)); // Gold
        if (ImGui::SmallButton(ICON_MD_STAR)) {
          panel_manager_.ToggleFavorite(card.card_id);
        }
        ImGui::PopStyleColor();
      } else {
        ImGui::PushStyleColor(ImGuiCol_Text, gui::ConvertColorToImVec4(theme.text_disabled));
        if (ImGui::SmallButton(ICON_MD_STAR_BORDER)) {
          panel_manager_.ToggleFavorite(card.card_id);
        }
        ImGui::PopStyleColor();
      }
      ImGui::PopID();
      ImGui::SameLine();

      // Card Item with Icon
      std::string label =
          absl::StrFormat("%s  %s", card.icon.c_str(), card.display_name.c_str());
      if (ImGui::Selectable(label.c_str(), visible)) {
        // Toggle visibility
        panel_manager_.TogglePanel(session_id, card.card_id);

        // Get the new visibility state after toggle
        bool new_visible = card.visibility_flag ? *card.visibility_flag : false;

        if (new_visible) {
          panel_manager_.AddToRecent(card.card_id); // Track recent

          // Card was just shown - activate the associated editor
          panel_manager_.TriggerPanelClicked(card.category);

          // Focus the card window so it comes to front
          std::string window_title = card.GetWindowTitle();
          ImGui::SetWindowFocus(window_title.c_str());
        }
      }

      // Shortcut Hint
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
      ImGui::PushStyleColor(ImGuiCol_Header,
                            gui::GetSurfaceContainerHighVec4());
      ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                            gui::GetSurfaceContainerHighestVec4());
      bool files_expanded = ImGui::CollapsingHeader(
          ICON_MD_FOLDER " Files", ImGuiTreeNodeFlags_DefaultOpen);
      ImGui::PopStyleColor(2);

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
  ImGui::End();

  ImGui::PopStyleVar(3);
  ImGui::PopStyleColor(1);
}

void ActivityBar::DrawCardBrowser(size_t session_id, bool* p_open) {
  ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

  if (ImGui::Begin(
          absl::StrFormat("%s Card Browser", ICON_MD_DASHBOARD).c_str(),
          p_open)) {
    static char search_filter[256] = "";
    static std::string category_filter = "All";

    // Search bar
    ImGui::SetNextItemWidth(300);
    ImGui::InputTextWithHint(
        "##Search",
        absl::StrFormat("%s Search cards...", ICON_MD_SEARCH).c_str(),
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

    // Card table
    if (ImGui::BeginTable("##CardTable", 4,
                          ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Borders)) {
      ImGui::TableSetupColumn("Visible", ImGuiTableColumnFlags_WidthFixed, 60);
      ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed,
                              120);
      ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed,
                              100);
      ImGui::TableHeadersRow();

      auto cards = (category_filter == "All") ? panel_manager_.GetPanelsInSession(session_id)
                                              : std::vector<std::string>{};

      if (category_filter != "All") {
        auto cat_cards = panel_manager_.GetPanelsInCategory(session_id, category_filter);
        for (const auto& card : cat_cards) {
          cards.push_back(card.card_id);
        }
      }

      for (const auto& card_id : cards) {
        const auto* card = panel_manager_.GetPanelDescriptor(session_id, card_id);
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

} // namespace editor
} // namespace yaze
