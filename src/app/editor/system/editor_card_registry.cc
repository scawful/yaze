#include "app/editor/system/editor_card_registry.h"

#include <algorithm>
#include <cstdio>
#include <fstream>

#include "absl/strings/str_format.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"
#include "nlohmann/json.hpp"
#include "util/log.h"
#include "util/platform_paths.h"

namespace yaze {
namespace editor {

// ============================================================================
// Category Icon Mapping
// ============================================================================

std::string EditorCardRegistry::GetCategoryIcon(const std::string& category) {
  if (category == "Dungeon") return ICON_MD_CASTLE;
  if (category == "Overworld") return ICON_MD_MAP;
  if (category == "Graphics") return ICON_MD_IMAGE;
  if (category == "Palette") return ICON_MD_PALETTE;
  if (category == "Sprite") return ICON_MD_PERSON;
  if (category == "Music") return ICON_MD_MUSIC_NOTE;
  if (category == "Message") return ICON_MD_MESSAGE;
  if (category == "Screen") return ICON_MD_TV;
  if (category == "Emulator") return ICON_MD_VIDEOGAME_ASSET;
  if (category == "Assembly") return ICON_MD_CODE;
  if (category == "Settings") return ICON_MD_SETTINGS;
  if (category == "Memory") return ICON_MD_MEMORY;
  return ICON_MD_FOLDER;  // Default for unknown categories
}

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
// Sidebar Keyboard Navigation
// ============================================================================

void EditorCardRegistry::HandleSidebarKeyboardNav(
    size_t session_id, const std::vector<CardInfo>& cards) {
  // Click to focus - only focus if sidebar window is hovered and mouse clicked
  if (!sidebar_has_focus_ && ImGui::IsWindowHovered(ImGuiHoveredFlags_None) &&
      ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    sidebar_has_focus_ = true;
    focused_card_index_ = cards.empty() ? -1 : 0;
  }

  // No navigation if not focused or no cards
  if (!sidebar_has_focus_ || cards.empty()) {
    return;
  }

  // Escape to unfocus
  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    sidebar_has_focus_ = false;
    focused_card_index_ = -1;
    return;
  }

  int card_count = static_cast<int>(cards.size());

  // Arrow keys / vim keys navigation
  if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) ||
      ImGui::IsKeyPressed(ImGuiKey_J)) {
    focused_card_index_ = std::min(focused_card_index_ + 1, card_count - 1);
  }
  if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) ||
      ImGui::IsKeyPressed(ImGuiKey_K)) {
    focused_card_index_ = std::max(focused_card_index_ - 1, 0);
  }

  // Home/End for quick navigation
  if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
    focused_card_index_ = 0;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_End)) {
    focused_card_index_ = card_count - 1;
  }

  // Enter/Space to toggle card visibility
  if (focused_card_index_ >= 0 && focused_card_index_ < card_count) {
    if (ImGui::IsKeyPressed(ImGuiKey_Enter) ||
        ImGui::IsKeyPressed(ImGuiKey_Space)) {
      const auto& card = cards[focused_card_index_];
      ToggleCard(session_id, card.card_id);
    }
  }
}

// ============================================================================
// VSCode-Style Sidebar (Activity Bar + Side Panel)
// ============================================================================

void EditorCardRegistry::Render(
    size_t session_id, const std::string& category,
    const std::vector<std::string>& active_categories,
    std::function<void(const std::string&)> on_category_select,
    std::function<bool()> has_rom) {
  
  if (!sidebar_visible_) return;

  // Draw the Activity Bar (Icons)
  DrawActivityBar(session_id, category, active_categories, on_category_select, has_rom);

  // Draw the Side Panel (Content) if expanded
  if (panel_expanded_) {
    DrawSidePanel(session_id, category, has_rom);
  }
}

void EditorCardRegistry::DrawActivityBar(
    size_t session_id, const std::string& active_category,
    const std::vector<std::string>& active_categories,
    std::function<void(const std::string&)> on_category_select,
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
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 8.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f); // Right border only ideally

  if (ImGui::Begin("##ActivityBar", nullptr, flags)) {
    ImVec4 accent = gui::ConvertColorToImVec4(theme.accent);
    ImVec4 inactive = gui::ConvertColorToImVec4(theme.text_secondary);

    for (const auto& cat : active_categories) {
      bool is_active = (cat == active_category) && panel_expanded_;
      
      // ROM check - Emulator now treated same as others (requires ROM for enabled state)
      bool rom_loaded = has_rom ? has_rom() : true;
      bool category_enabled = rom_loaded; 

      // Active Indicator (Left Border)
      if (cat == active_category && category_enabled) {
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImGui::GetCursorScreenPos(),
            ImVec2(ImGui::GetCursorScreenPos().x + 3.0f, ImGui::GetCursorScreenPos().y + 40.0f),
            ImGui::ColorConvertFloat4ToU32(accent));
      }

      // Button Style
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1,1,1,0.1f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1,1,1,0.2f));
      
      // Text/Icon Color
      ImVec4 icon_color = (cat == active_category && category_enabled) ? accent : inactive;
      if (!category_enabled) icon_color = ImVec4(inactive.x, inactive.y, inactive.z, 0.4f);
      ImGui::PushStyleColor(ImGuiCol_Text, icon_color);

      std::string icon = GetCategoryIcon(cat);
      if (ImGui::Button(icon.c_str(), ImVec2(48.0f, 40.0f))) {
        if (category_enabled) {
          if (cat == active_category) {
            // Toggle panel if clicking same category
            TogglePanelExpanded();
          } else {
            // Switch category and ensure open
            if (on_category_select) on_category_select(cat);
            else SetActiveCategory(cat);
            SetPanelExpanded(true);
          }
        }
      }

      // Tooltip
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::BeginTooltip();
        ImGui::Text("%s %s", icon.c_str(), cat.c_str());
        if (!category_enabled) {
          ImGui::TextColored(gui::ConvertColorToImVec4(theme.warning), "Open ROM required");
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextSecondaryVec4());
            ImGui::TextUnformatted("Click to open");
            ImGui::PopStyleColor();
        }
        ImGui::EndTooltip();
      }

      ImGui::PopStyleColor(4); // Button colors + Text
    }

    // Bottom Section (Settings, Collapse)
    float bottom_height = 40.0f + 8.0f; // Button + padding
    float avail_y = ImGui::GetContentRegionAvail().y;
    if (avail_y > bottom_height) {
        ImGui::Dummy(ImVec2(0, avail_y - bottom_height));
    }

    // Collapse Button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, gui::GetSurfaceContainerHighVec4());
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, gui::GetSurfaceContainerHighestVec4());
    ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextSecondaryVec4());

    if (ImGui::Button(ICON_MD_CHEVRON_LEFT, ImVec2(48.0f, 40.0f))) {
        SetSidebarVisible(false);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Hide Activity Bar");
    }

    ImGui::PopStyleColor(4);
  }
  ImGui::End();

  ImGui::PopStyleVar(3);
  ImGui::PopStyleColor(2);
}

void EditorCardRegistry::DrawSidePanel(
    size_t session_id, const std::string& category,
    std::function<bool()> has_rom) {
    
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float bar_width = GetSidebarWidth();
  const float panel_width = GetSidePanelWidth();

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

    // Close button aligned to right
    float avail_width = ImGui::GetContentRegionAvail().x;
    ImGui::SameLine(ImGui::GetCursorPosX() + avail_width - 28.0f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4.0f);  // Center vertically with text
    if (ImGui::Button(ICON_MD_CLOSE, ImVec2(24.0f, 24.0f))) {
      SetPanelExpanded(false);
    }

    ImGui::Spacing();
    ImGui::Separator();
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

    // Content - Reusing GetCardsInCategory logic
    auto cards = GetCardsInCategory(session_id, category);

    ImGui::BeginChild("##PanelContent", ImVec2(0, 0), false,
                      ImGuiWindowFlags_None);
    for (const auto& card : cards) {
      bool visible = card.visibility_flag ? *card.visibility_flag : false;

      // Card Item with Icon
      std::string label =
          absl::StrFormat("%s  %s", card.icon.c_str(), card.display_name.c_str());
      if (ImGui::Selectable(label.c_str(), visible)) {
        ToggleCard(session_id, card.card_id);
      }

      // Shortcut Hint
      if (ImGui::IsItemHovered() && !card.shortcut_hint.empty()) {
        ImGui::SetTooltip("%s", card.shortcut_hint.c_str());
      }
    }
    ImGui::EndChild();

    if (disable_cards) {
      ImGui::EndDisabled();
    }
  }
  ImGui::End();

  ImGui::PopStyleVar(3);
  ImGui::PopStyleColor(1);
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
        if (card_it == cards_.end())
          continue;

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
  auto config_dir_result = util::PlatformPaths::GetConfigDirectory();
  if (!config_dir_result.ok()) {
    LOG_ERROR("EditorCardRegistry", "Failed to get config directory: %s",
              config_dir_result.status().ToString().c_str());
    return;
  }

  std::filesystem::path presets_file = *config_dir_result / "layout_presets.json";

  try {
    nlohmann::json j;
    j["version"] = 1;
    j["presets"] = nlohmann::json::object();

    for (const auto& [name, preset] : presets_) {
      nlohmann::json preset_json;
      preset_json["name"] = preset.name;
      preset_json["description"] = preset.description;
      preset_json["visible_cards"] = preset.visible_cards;
      j["presets"][name] = preset_json;
    }

    std::ofstream file(presets_file);
    if (!file.is_open()) {
      LOG_ERROR("EditorCardRegistry", "Failed to open file for writing: %s",
                presets_file.string().c_str());
      return;
    }

    file << j.dump(2);
    file.close();

    LOG_INFO("EditorCardRegistry", "Saved %zu presets to %s", presets_.size(),
             presets_file.string().c_str());
  } catch (const std::exception& e) {
    LOG_ERROR("EditorCardRegistry", "Error saving presets: %s", e.what());
  }
}

void EditorCardRegistry::LoadPresetsFromFile() {
  auto config_dir_result = util::PlatformPaths::GetConfigDirectory();
  if (!config_dir_result.ok()) {
    LOG_WARN("EditorCardRegistry", "Failed to get config directory: %s",
             config_dir_result.status().ToString().c_str());
    return;
  }

  std::filesystem::path presets_file = *config_dir_result / "layout_presets.json";

  if (!util::PlatformPaths::Exists(presets_file)) {
    LOG_INFO("EditorCardRegistry", "No presets file found at %s",
             presets_file.string().c_str());
    return;
  }

  try {
    std::ifstream file(presets_file);
    if (!file.is_open()) {
      LOG_WARN("EditorCardRegistry", "Failed to open presets file: %s",
               presets_file.string().c_str());
      return;
    }

    nlohmann::json j;
    file >> j;
    file.close();

    if (!j.contains("presets")) {
      LOG_WARN("EditorCardRegistry", "Invalid presets file format");
      return;
    }

    size_t loaded_count = 0;
    for (auto& [name, preset_json] : j["presets"].items()) {
      WorkspacePreset preset;
      preset.name = preset_json.value("name", name);
      preset.description = preset_json.value("description", "");

      if (preset_json.contains("visible_cards")) {
        for (const auto& card : preset_json["visible_cards"]) {
          preset.visible_cards.push_back(card.get<std::string>());
        }
      }

      presets_[name] = preset;
      loaded_count++;
    }

    LOG_INFO("EditorCardRegistry", "Loaded %zu presets from %s", loaded_count,
             presets_file.string().c_str());
  } catch (const std::exception& e) {
    LOG_ERROR("EditorCardRegistry", "Error loading presets: %s", e.what());
  }
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
