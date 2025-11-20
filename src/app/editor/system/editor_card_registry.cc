#include "app/editor/system/editor_card_registry.h"

#include <algorithm>
#include <cstdio>

#include "absl/strings/str_format.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"
#include "util/log.h"

namespace yaze {
namespace editor {

// ============================================================================
// Session Lifecycle Management
// ============================================================================

void EditorCardRegistry::RegisterSession(size_t session_id) {
  if (session_cards_.find(session_id) == session_cards_.end()) {
    session_cards_[session_id] = std::vector<std::string>();
    session_card_mapping_[session_id] =
        std::unordered_map<std::string, std::string>();
    UpdateSessionCount();
    LOG_INFO("EditorCardRegistry", "Registered session %zu (total: %zu)",
             session_id, session_count_);
  }
}

void EditorCardRegistry::UnregisterSession(size_t session_id) {
  auto it = session_cards_.find(session_id);
  if (it != session_cards_.end()) {
    UnregisterSessionCards(session_id);
    session_cards_.erase(it);
    session_card_mapping_.erase(session_id);
    UpdateSessionCount();

    // Reset active session if it was the one being removed
    if (active_session_ == session_id) {
      active_session_ = 0;
      if (!session_cards_.empty()) {
        active_session_ = session_cards_.begin()->first;
      }
    }

    LOG_INFO("EditorCardRegistry", "Unregistered session %zu (total: %zu)",
             session_id, session_count_);
  }
}

void EditorCardRegistry::SetActiveSession(size_t session_id) {
  if (session_cards_.find(session_id) != session_cards_.end()) {
    active_session_ = session_id;
  }
}

// ============================================================================
// Card Registration
// ============================================================================

void EditorCardRegistry::RegisterCard(size_t session_id,
                                      const CardInfo& base_info) {
  RegisterSession(session_id);  // Ensure session exists

  std::string prefixed_id = MakeCardId(session_id, base_info.card_id);

  // Check if already registered to avoid duplicates
  if (cards_.find(prefixed_id) != cards_.end()) {
    LOG_WARN("EditorCardRegistry",
             "Card '%s' already registered, skipping duplicate",
             prefixed_id.c_str());
    return;
  }

  // Create new CardInfo with prefixed ID
  CardInfo prefixed_info = base_info;
  prefixed_info.card_id = prefixed_id;

  // If no visibility_flag provided, create centralized one
  if (!prefixed_info.visibility_flag) {
    centralized_visibility_[prefixed_id] = false;  // Hidden by default
    prefixed_info.visibility_flag = &centralized_visibility_[prefixed_id];
  }

  // Register the card
  cards_[prefixed_id] = prefixed_info;

  // Track in our session mapping
  session_cards_[session_id].push_back(prefixed_id);
  session_card_mapping_[session_id][base_info.card_id] = prefixed_id;

  LOG_INFO("EditorCardRegistry", "Registered card %s -> %s for session %zu",
           base_info.card_id.c_str(), prefixed_id.c_str(), session_id);
}

void EditorCardRegistry::RegisterCard(
    size_t session_id, const std::string& card_id,
    const std::string& display_name, const std::string& icon,
    const std::string& category, const std::string& shortcut_hint, int priority,
    std::function<void()> on_show, std::function<void()> on_hide,
    bool visible_by_default) {
  CardInfo info;
  info.card_id = card_id;
  info.display_name = display_name;
  info.icon = icon;
  info.category = category;
  info.shortcut_hint = shortcut_hint;
  info.priority = priority;
  info.visibility_flag = nullptr;  // Will be created in RegisterCard
  info.on_show = on_show;
  info.on_hide = on_hide;

  RegisterCard(session_id, info);

  // Set initial visibility if requested
  if (visible_by_default) {
    ShowCard(session_id, card_id);
  }
}

void EditorCardRegistry::UnregisterCard(size_t session_id,
                                        const std::string& base_card_id) {
  std::string prefixed_id = GetPrefixedCardId(session_id, base_card_id);
  if (prefixed_id.empty()) {
    return;
  }

  auto it = cards_.find(prefixed_id);
  if (it != cards_.end()) {
    LOG_INFO("EditorCardRegistry", "Unregistered card: %s",
             prefixed_id.c_str());
    cards_.erase(it);
    centralized_visibility_.erase(prefixed_id);

    // Remove from session tracking
    auto& session_card_list = session_cards_[session_id];
    session_card_list.erase(std::remove(session_card_list.begin(),
                                        session_card_list.end(), prefixed_id),
                            session_card_list.end());

    session_card_mapping_[session_id].erase(base_card_id);
  }
}

void EditorCardRegistry::UnregisterCardsWithPrefix(const std::string& prefix) {
  std::vector<std::string> to_remove;

  // Find all cards with the given prefix
  for (const auto& [card_id, card_info] : cards_) {
    if (card_id.find(prefix) == 0) {  // Starts with prefix
      to_remove.push_back(card_id);
    }
  }

  // Remove them
  for (const auto& card_id : to_remove) {
    cards_.erase(card_id);
    centralized_visibility_.erase(card_id);
    LOG_INFO("EditorCardRegistry", "Unregistered card with prefix '%s': %s",
             prefix.c_str(), card_id.c_str());
  }

  // Also clean up session tracking
  for (auto& [session_id, card_list] : session_cards_) {
    card_list.erase(std::remove_if(card_list.begin(), card_list.end(),
                                   [&prefix](const std::string& id) {
                                     return id.find(prefix) == 0;
                                   }),
                    card_list.end());
  }
}

void EditorCardRegistry::ClearAllCards() {
  cards_.clear();
  centralized_visibility_.clear();
  session_cards_.clear();
  session_card_mapping_.clear();
  session_count_ = 0;
  LOG_INFO("EditorCardRegistry", "Cleared all cards");
}

// ============================================================================
// Card Control (Programmatic, No GUI)
// ============================================================================

bool EditorCardRegistry::ShowCard(size_t session_id,
                                  const std::string& base_card_id) {
  std::string prefixed_id = GetPrefixedCardId(session_id, base_card_id);
  if (prefixed_id.empty()) {
    return false;
  }

  auto it = cards_.find(prefixed_id);
  if (it != cards_.end()) {
    if (it->second.visibility_flag) {
      *it->second.visibility_flag = true;
    }
    if (it->second.on_show) {
      it->second.on_show();
    }
    return true;
  }
  return false;
}

bool EditorCardRegistry::HideCard(size_t session_id,
                                  const std::string& base_card_id) {
  std::string prefixed_id = GetPrefixedCardId(session_id, base_card_id);
  if (prefixed_id.empty()) {
    return false;
  }

  auto it = cards_.find(prefixed_id);
  if (it != cards_.end()) {
    if (it->second.visibility_flag) {
      *it->second.visibility_flag = false;
    }
    if (it->second.on_hide) {
      it->second.on_hide();
    }
    return true;
  }
  return false;
}

bool EditorCardRegistry::ToggleCard(size_t session_id,
                                    const std::string& base_card_id) {
  std::string prefixed_id = GetPrefixedCardId(session_id, base_card_id);
  if (prefixed_id.empty()) {
    return false;
  }

  auto it = cards_.find(prefixed_id);
  if (it != cards_.end() && it->second.visibility_flag) {
    bool new_state = !(*it->second.visibility_flag);
    *it->second.visibility_flag = new_state;

    if (new_state && it->second.on_show) {
      it->second.on_show();
    } else if (!new_state && it->second.on_hide) {
      it->second.on_hide();
    }
    return true;
  }
  return false;
}

bool EditorCardRegistry::IsCardVisible(size_t session_id,
                                       const std::string& base_card_id) const {
  std::string prefixed_id = GetPrefixedCardId(session_id, base_card_id);
  if (prefixed_id.empty()) {
    return false;
  }

  auto it = cards_.find(prefixed_id);
  if (it != cards_.end() && it->second.visibility_flag) {
    return *it->second.visibility_flag;
  }
  return false;
}

bool* EditorCardRegistry::GetVisibilityFlag(size_t session_id,
                                            const std::string& base_card_id) {
  std::string prefixed_id = GetPrefixedCardId(session_id, base_card_id);
  if (prefixed_id.empty()) {
    return nullptr;
  }

  auto it = cards_.find(prefixed_id);
  if (it != cards_.end()) {
    return it->second.visibility_flag;
  }
  return nullptr;
}

// ============================================================================
// Batch Operations
// ============================================================================

void EditorCardRegistry::ShowAllCardsInSession(size_t session_id) {
  auto it = session_cards_.find(session_id);
  if (it != session_cards_.end()) {
    for (const auto& prefixed_card_id : it->second) {
      auto card_it = cards_.find(prefixed_card_id);
      if (card_it != cards_.end() && card_it->second.visibility_flag) {
        *card_it->second.visibility_flag = true;
        if (card_it->second.on_show) {
          card_it->second.on_show();
        }
      }
    }
  }
}

void EditorCardRegistry::HideAllCardsInSession(size_t session_id) {
  auto it = session_cards_.find(session_id);
  if (it != session_cards_.end()) {
    for (const auto& prefixed_card_id : it->second) {
      auto card_it = cards_.find(prefixed_card_id);
      if (card_it != cards_.end() && card_it->second.visibility_flag) {
        *card_it->second.visibility_flag = false;
        if (card_it->second.on_hide) {
          card_it->second.on_hide();
        }
      }
    }
  }
}

void EditorCardRegistry::ShowAllCardsInCategory(size_t session_id,
                                                const std::string& category) {
  auto it = session_cards_.find(session_id);
  if (it != session_cards_.end()) {
    for (const auto& prefixed_card_id : it->second) {
      auto card_it = cards_.find(prefixed_card_id);
      if (card_it != cards_.end() && card_it->second.category == category) {
        if (card_it->second.visibility_flag) {
          *card_it->second.visibility_flag = true;
        }
        if (card_it->second.on_show) {
          card_it->second.on_show();
        }
      }
    }
  }
}

void EditorCardRegistry::HideAllCardsInCategory(size_t session_id,
                                                const std::string& category) {
  auto it = session_cards_.find(session_id);
  if (it != session_cards_.end()) {
    for (const auto& prefixed_card_id : it->second) {
      auto card_it = cards_.find(prefixed_card_id);
      if (card_it != cards_.end() && card_it->second.category == category) {
        if (card_it->second.visibility_flag) {
          *card_it->second.visibility_flag = false;
        }
        if (card_it->second.on_hide) {
          card_it->second.on_hide();
        }
      }
    }
  }
}

void EditorCardRegistry::ShowOnlyCard(size_t session_id,
                                      const std::string& base_card_id) {
  // First get the category of the target card
  std::string prefixed_id = GetPrefixedCardId(session_id, base_card_id);
  if (prefixed_id.empty()) {
    return;
  }

  auto target_it = cards_.find(prefixed_id);
  if (target_it == cards_.end()) {
    return;
  }

  std::string category = target_it->second.category;

  // Hide all cards in the same category
  HideAllCardsInCategory(session_id, category);

  // Show the target card
  ShowCard(session_id, base_card_id);
}

// ============================================================================
// Query Methods
// ============================================================================

std::vector<std::string> EditorCardRegistry::GetCardsInSession(
    size_t session_id) const {
  auto it = session_cards_.find(session_id);
  if (it != session_cards_.end()) {
    return it->second;
  }
  return {};
}

std::vector<CardInfo> EditorCardRegistry::GetCardsInCategory(
    size_t session_id, const std::string& category) const {
  std::vector<CardInfo> result;

  auto it = session_cards_.find(session_id);
  if (it != session_cards_.end()) {
    for (const auto& prefixed_card_id : it->second) {
      auto card_it = cards_.find(prefixed_card_id);
      if (card_it != cards_.end() && card_it->second.category == category) {
        result.push_back(card_it->second);
      }
    }
  }

  // Sort by priority
  std::sort(result.begin(), result.end(),
            [](const CardInfo& a, const CardInfo& b) {
              return a.priority < b.priority;
            });

  return result;
}

std::vector<std::string> EditorCardRegistry::GetAllCategories(
    size_t session_id) const {
  std::vector<std::string> categories;

  auto it = session_cards_.find(session_id);
  if (it != session_cards_.end()) {
    for (const auto& prefixed_card_id : it->second) {
      auto card_it = cards_.find(prefixed_card_id);
      if (card_it != cards_.end()) {
        if (std::find(categories.begin(), categories.end(),
                      card_it->second.category) == categories.end()) {
          categories.push_back(card_it->second.category);
        }
      }
    }
  }
  return categories;
}

const CardInfo* EditorCardRegistry::GetCardInfo(
    size_t session_id, const std::string& base_card_id) const {
  std::string prefixed_id = GetPrefixedCardId(session_id, base_card_id);
  if (prefixed_id.empty()) {
    return nullptr;
  }

  auto it = cards_.find(prefixed_id);
  if (it != cards_.end()) {
    return &it->second;
  }
  return nullptr;
}

std::vector<std::string> EditorCardRegistry::GetAllCategories() const {
  std::vector<std::string> categories;
  for (const auto& [card_id, card_info] : cards_) {
    if (std::find(categories.begin(), categories.end(), card_info.category) ==
        categories.end()) {
      categories.push_back(card_info.category);
    }
  }
  return categories;
}

// ============================================================================
// View Menu Integration
// ============================================================================

void EditorCardRegistry::DrawViewMenuSection(size_t session_id,
                                             const std::string& category) {
  auto cards = GetCardsInCategory(session_id, category);

  if (cards.empty()) {
    return;
  }

  if (ImGui::BeginMenu(category.c_str())) {
    for (const auto& card : cards) {
      DrawCardMenuItem(card);
    }
    ImGui::EndMenu();
  }
}

void EditorCardRegistry::DrawViewMenuAll(size_t session_id) {
  auto categories = GetAllCategories(session_id);

  for (const auto& category : categories) {
    DrawViewMenuSection(session_id, category);
  }
}

// ============================================================================
// VSCode-Style Sidebar
// ============================================================================

void EditorCardRegistry::DrawSidebar(
    size_t session_id, const std::string& category,
    const std::vector<std::string>& active_categories,
    std::function<void(const std::string&)> on_category_switch,
    std::function<void()> on_collapse) {
  // Use ThemeManager for consistent theming
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  const float sidebar_width = GetSidebarWidth();

  // Fixed sidebar window on the left edge of screen - exactly like VSCode
  // Positioned below menu bar, spans full height, fixed 48px width
  ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetFrameHeight()));
  ImGui::SetNextWindowSize(
      ImVec2(sidebar_width, -1));  // Exactly 48px wide, full height

  ImGuiWindowFlags sidebar_flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoFocusOnAppearing |
      ImGuiWindowFlags_NoNavFocus;

  // VSCode-style dark sidebar background with visible border
  ImVec4 sidebar_bg = ImVec4(0.18f, 0.18f, 0.20f, 1.0f);
  ImVec4 sidebar_border = ImVec4(0.4f, 0.4f, 0.45f, 1.0f);

  ImGui::PushStyleColor(ImGuiCol_WindowBg, sidebar_bg);
  ImGui::PushStyleColor(ImGuiCol_Border, sidebar_border);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 8.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 6.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);

  if (ImGui::Begin("##EditorCardSidebar", nullptr, sidebar_flags)) {
    // Category switcher buttons at top (only if multiple editors are active)
    if (active_categories.size() > 1) {
      ImVec4 accent = gui::ConvertColorToImVec4(theme.accent);
      ImVec4 inactive = gui::ConvertColorToImVec4(theme.button);

      for (const auto& cat : active_categories) {
        bool is_current = (cat == category);

        // Highlight current category with accent color
        if (is_current) {
          ImGui::PushStyleColor(ImGuiCol_Button,
                                ImVec4(accent.x, accent.y, accent.z, 0.8f));
          ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                ImVec4(accent.x, accent.y, accent.z, 1.0f));
        } else {
          ImGui::PushStyleColor(ImGuiCol_Button, inactive);
          ImGui::PushStyleColor(
              ImGuiCol_ButtonHovered,
              gui::ConvertColorToImVec4(theme.button_hovered));
        }

        // Show first letter of category as button label
        std::string btn_label = cat.empty() ? "?" : std::string(1, cat[0]);
        if (ImGui::Button(btn_label.c_str(), ImVec2(40.0f, 32.0f))) {
          // Switch to this category/editor
          if (on_category_switch) {
            on_category_switch(cat);
          } else {
            SetActiveCategory(cat);
          }
        }

        ImGui::PopStyleColor(2);

        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s Editor\nClick to switch", cat.c_str());
        }
      }

      ImGui::Dummy(ImVec2(0, 2.0f));
      ImGui::Separator();
      ImGui::Spacing();
    }

    // Get cards for current category
    auto cards = GetCardsInCategory(session_id, category);

    // Set this category as active when showing cards
    if (!cards.empty()) {
      SetActiveCategory(category);
    }

    // Close All and Show All buttons (only if cards exist)
    if (!cards.empty()) {
      ImVec4 error_color = gui::ConvertColorToImVec4(theme.error);
      ImGui::PushStyleColor(ImGuiCol_Button,
                            ImVec4(error_color.x * 0.6f, error_color.y * 0.6f,
                                   error_color.z * 0.6f, 0.9f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, error_color);
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                            ImVec4(error_color.x * 1.2f, error_color.y * 1.2f,
                                   error_color.z * 1.2f, 1.0f));

      if (ImGui::Button(ICON_MD_CLOSE, ImVec2(40.0f, 36.0f))) {
        HideAllCardsInCategory(session_id, category);
      }

      ImGui::PopStyleColor(3);

      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Close All %s Cards", category.c_str());
      }

      // Show All button
      ImVec4 success_color = gui::ConvertColorToImVec4(theme.success);
      ImGui::PushStyleColor(
          ImGuiCol_Button,
          ImVec4(success_color.x * 0.6f, success_color.y * 0.6f,
                 success_color.z * 0.6f, 0.7f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, success_color);
      ImGui::PushStyleColor(
          ImGuiCol_ButtonActive,
          ImVec4(success_color.x * 1.2f, success_color.y * 1.2f,
                 success_color.z * 1.2f, 1.0f));

      if (ImGui::Button(ICON_MD_DONE_ALL, ImVec2(40.0f, 36.0f))) {
        ShowAllCardsInCategory(session_id, category);
      }

      ImGui::PopStyleColor(3);

      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Show All %s Cards", category.c_str());
      }

      ImGui::Dummy(ImVec2(0, 2.0f));

      // Draw individual card toggle buttons
      ImVec4 accent_color = gui::ConvertColorToImVec4(theme.accent);
      ImVec4 button_bg = gui::ConvertColorToImVec4(theme.button);

      for (const auto& card : cards) {
        ImGui::PushID(card.card_id.c_str());

        bool is_active = card.visibility_flag && *card.visibility_flag;

        // Highlight active cards with accent color
        if (is_active) {
          ImGui::PushStyleColor(
              ImGuiCol_Button,
              ImVec4(accent_color.x, accent_color.y, accent_color.z, 0.5f));
          ImGui::PushStyleColor(
              ImGuiCol_ButtonHovered,
              ImVec4(accent_color.x, accent_color.y, accent_color.z, 0.7f));
          ImGui::PushStyleColor(ImGuiCol_ButtonActive, accent_color);
        } else {
          ImGui::PushStyleColor(ImGuiCol_Button, button_bg);
          ImGui::PushStyleColor(
              ImGuiCol_ButtonHovered,
              gui::ConvertColorToImVec4(theme.button_hovered));
          ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                gui::ConvertColorToImVec4(theme.button_active));
        }

        // Icon-only button for each card
        if (ImGui::Button(card.icon.c_str(), ImVec2(40.0f, 40.0f))) {
          ToggleCard(session_id, card.card_id);
          SetActiveCategory(category);
        }

        ImGui::PopStyleColor(3);

        // Show tooltip with card name and shortcut
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
          SetActiveCategory(category);

          ImGui::SetTooltip(
              "%s\n%s", card.display_name.c_str(),
              card.shortcut_hint.empty() ? "" : card.shortcut_hint.c_str());
        }

        ImGui::PopID();
      }
    }  // End if (!cards.empty())

    // Card Browser and Collapse buttons at bottom
    if (on_collapse) {
      ImGui::Dummy(ImVec2(0, 10.0f));
      ImGui::Separator();
      ImGui::Spacing();

      // Collapse button
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.22f, 0.9f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                            ImVec4(0.3f, 0.3f, 0.32f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                            ImVec4(0.25f, 0.25f, 0.27f, 1.0f));

      if (ImGui::Button(ICON_MD_KEYBOARD_ARROW_LEFT, ImVec2(40.0f, 36.0f))) {
        on_collapse();
      }

      ImGui::PopStyleColor(3);

      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Hide Sidebar\nCtrl+B");
      }
    }
  }
  ImGui::End();

  ImGui::PopStyleVar(3);    // WindowPadding, ItemSpacing, WindowBorderSize
  ImGui::PopStyleColor(2);  // WindowBg, Border
}

// ============================================================================
// Compact Controls for Menu Bar
// ============================================================================

void EditorCardRegistry::DrawCompactCardControl(size_t session_id,
                                                const std::string& category) {
  auto cards = GetCardsInCategory(session_id, category);

  if (cards.empty()) {
    return;
  }

  // Compact dropdown
  if (ImGui::BeginCombo(
          "##CardControl",
          absl::StrFormat("%s Cards", ICON_MD_DASHBOARD).c_str())) {
    for (const auto& card : cards) {
      bool visible = card.visibility_flag ? *card.visibility_flag : false;
      if (ImGui::MenuItem(absl::StrFormat("%s %s", card.icon.c_str(),
                                          card.display_name.c_str())
                              .c_str(),
                          nullptr, visible)) {
        ToggleCard(session_id, card.card_id);
      }
    }
    ImGui::EndCombo();
  }
}

void EditorCardRegistry::DrawInlineCardToggles(size_t session_id,
                                               const std::string& category) {
  auto cards = GetCardsInCategory(session_id, category);

  size_t visible_count = 0;
  for (const auto& card : cards) {
    if (card.visibility_flag && *card.visibility_flag) {
      visible_count++;
    }
  }

  ImGui::Text("(%zu/%zu)", visible_count, cards.size());
}

// ============================================================================
// Card Browser UI
// ============================================================================

void EditorCardRegistry::DrawCardBrowser(size_t session_id, bool* p_open) {
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

      auto categories = GetAllCategories(session_id);
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

      auto cards = (category_filter == "All") ? GetCardsInSession(session_id)
                                              : std::vector<std::string>{};

      if (category_filter != "All") {
        auto cat_cards = GetCardsInCategory(session_id, category_filter);
        for (const auto& card : cat_cards) {
          cards.push_back(card.card_id);
        }
      }

      for (const auto& card_id : cards) {
        auto card_it = cards_.find(card_id);
        if (card_it == cards_.end()) continue;

        const auto& card = card_it->second;

        // Apply search filter
        std::string search_str = search_filter;
        if (!search_str.empty()) {
          std::string card_lower = card.display_name;
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
        if (card.visibility_flag) {
          bool visible = *card.visibility_flag;
          if (ImGui::Checkbox(
                  absl::StrFormat("##vis_%s", card.card_id.c_str()).c_str(),
                  &visible)) {
            *card.visibility_flag = visible;
            if (visible && card.on_show) {
              card.on_show();
            } else if (!visible && card.on_hide) {
              card.on_hide();
            }
          }
        }

        // Name with icon
        ImGui::TableNextColumn();
        ImGui::Text("%s %s", card.icon.c_str(), card.display_name.c_str());

        // Category
        ImGui::TableNextColumn();
        ImGui::Text("%s", card.category.c_str());

        // Shortcut
        ImGui::TableNextColumn();
        ImGui::TextDisabled("%s", card.shortcut_hint.c_str());
      }

      ImGui::EndTable();
    }
  }
  ImGui::End();
}

// ============================================================================
// Workspace Presets
// ============================================================================

void EditorCardRegistry::SavePreset(const std::string& name,
                                    const std::string& description) {
  WorkspacePreset preset;
  preset.name = name;
  preset.description = description;

  // Collect all visible cards across all sessions
  for (const auto& [card_id, card_info] : cards_) {
    if (card_info.visibility_flag && *card_info.visibility_flag) {
      preset.visible_cards.push_back(card_id);
    }
  }

  presets_[name] = preset;
  SavePresetsToFile();
  LOG_INFO("EditorCardRegistry", "Saved preset: %s (%zu cards)", name.c_str(),
           preset.visible_cards.size());
}

bool EditorCardRegistry::LoadPreset(const std::string& name) {
  auto it = presets_.find(name);
  if (it == presets_.end()) {
    return false;
  }

  // First hide all cards
  for (auto& [card_id, card_info] : cards_) {
    if (card_info.visibility_flag) {
      *card_info.visibility_flag = false;
    }
  }

  // Then show preset cards
  for (const auto& card_id : it->second.visible_cards) {
    auto card_it = cards_.find(card_id);
    if (card_it != cards_.end() && card_it->second.visibility_flag) {
      *card_it->second.visibility_flag = true;
      if (card_it->second.on_show) {
        card_it->second.on_show();
      }
    }
  }

  LOG_INFO("EditorCardRegistry", "Loaded preset: %s", name.c_str());
  return true;
}

void EditorCardRegistry::DeletePreset(const std::string& name) {
  presets_.erase(name);
  SavePresetsToFile();
}

std::vector<EditorCardRegistry::WorkspacePreset>
EditorCardRegistry::GetPresets() const {
  std::vector<WorkspacePreset> result;
  for (const auto& [name, preset] : presets_) {
    result.push_back(preset);
  }
  return result;
}

// ============================================================================
// Quick Actions
// ============================================================================

void EditorCardRegistry::ShowAll(size_t session_id) {
  ShowAllCardsInSession(session_id);
}

void EditorCardRegistry::HideAll(size_t session_id) {
  HideAllCardsInSession(session_id);
}

void EditorCardRegistry::ResetToDefaults(size_t session_id) {
  // Hide all cards first
  HideAllCardsInSession(session_id);

  // TODO: Load default visibility from config file or hardcoded defaults
  LOG_INFO("EditorCardRegistry", "Reset to defaults for session %zu",
           session_id);
}

// ============================================================================
// Statistics
// ============================================================================

size_t EditorCardRegistry::GetVisibleCardCount(size_t session_id) const {
  size_t count = 0;
  auto it = session_cards_.find(session_id);
  if (it != session_cards_.end()) {
    for (const auto& prefixed_card_id : it->second) {
      auto card_it = cards_.find(prefixed_card_id);
      if (card_it != cards_.end() && card_it->second.visibility_flag) {
        if (*card_it->second.visibility_flag) {
          count++;
        }
      }
    }
  }
  return count;
}

// ============================================================================
// Session Prefixing Utilities
// ============================================================================

std::string EditorCardRegistry::MakeCardId(size_t session_id,
                                           const std::string& base_id) const {
  if (ShouldPrefixCards()) {
    return absl::StrFormat("s%zu.%s", session_id, base_id);
  }
  return base_id;
}

// ============================================================================
// Helper Methods (Private)
// ============================================================================

void EditorCardRegistry::UpdateSessionCount() {
  session_count_ = session_cards_.size();
}

std::string EditorCardRegistry::GetPrefixedCardId(
    size_t session_id, const std::string& base_id) const {
  auto session_it = session_card_mapping_.find(session_id);
  if (session_it != session_card_mapping_.end()) {
    auto card_it = session_it->second.find(base_id);
    if (card_it != session_it->second.end()) {
      return card_it->second;
    }
  }

  // Fallback: try unprefixed ID (for single session or direct access)
  if (cards_.find(base_id) != cards_.end()) {
    return base_id;
  }

  return "";  // Card not found
}

void EditorCardRegistry::UnregisterSessionCards(size_t session_id) {
  auto it = session_cards_.find(session_id);
  if (it != session_cards_.end()) {
    for (const auto& prefixed_card_id : it->second) {
      cards_.erase(prefixed_card_id);
      centralized_visibility_.erase(prefixed_card_id);
    }
  }
}

void EditorCardRegistry::SavePresetsToFile() {
  // TODO: Implement file I/O for presets
  LOG_INFO("EditorCardRegistry", "SavePresetsToFile() - not yet implemented");
}

void EditorCardRegistry::LoadPresetsFromFile() {
  // TODO: Implement file I/O for presets
  LOG_INFO("EditorCardRegistry", "LoadPresetsFromFile() - not yet implemented");
}

void EditorCardRegistry::DrawCardMenuItem(const CardInfo& info) {
  bool visible = info.visibility_flag ? *info.visibility_flag : false;

  std::string label =
      absl::StrFormat("%s %s", info.icon.c_str(), info.display_name.c_str());

  const char* shortcut =
      info.shortcut_hint.empty() ? nullptr : info.shortcut_hint.c_str();

  if (ImGui::MenuItem(label.c_str(), shortcut, visible)) {
    if (info.visibility_flag) {
      *info.visibility_flag = !visible;
      if (*info.visibility_flag && info.on_show) {
        info.on_show();
      } else if (!*info.visibility_flag && info.on_hide) {
        info.on_hide();
      }
    }
  }
}

void EditorCardRegistry::DrawCardInSidebar(const CardInfo& info,
                                           bool is_active) {
  if (is_active) {
    ImGui::PushStyleColor(ImGuiCol_Button, gui::GetPrimaryVec4());
  }

  if (ImGui::Button(
          absl::StrFormat("%s %s", info.icon.c_str(), info.display_name.c_str())
              .c_str(),
          ImVec2(-1, 0))) {
    if (info.visibility_flag) {
      *info.visibility_flag = !*info.visibility_flag;
      if (*info.visibility_flag && info.on_show) {
        info.on_show();
      } else if (!*info.visibility_flag && info.on_hide) {
        info.on_hide();
      }
    }
  }

  if (is_active) {
    ImGui::PopStyleColor();
  }
}

}  // namespace editor
}  // namespace yaze
