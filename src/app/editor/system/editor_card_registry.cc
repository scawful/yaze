#define IMGUI_DEFINE_MATH_OPERATORS

#include "app/editor/system/editor_card_registry.h"

#include <algorithm>
#include <cstdio>
#include <fstream>

#include "absl/strings/str_format.h"
#include "app/editor/system/editor_registry.h"
#include "app/editor/ui/layout_presets.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/widgets/themed_widgets.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"  // For ImGuiWindow and FindWindowByName
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
  if (category == "Agent") return ICON_MD_SMART_TOY;
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
    const std::vector<std::string>& all_categories,
    const std::unordered_set<std::string>& active_editor_categories,
    std::function<void(const std::string&)> on_category_select,
    std::function<bool()> has_rom) {
  
  if (!sidebar_visible_) return;

  // Store active editors for visual feedback
  active_editor_categories_ = active_editor_categories;

  // Draw the Activity Bar (Icons)
  DrawActivityBar(session_id, category, all_categories, active_editor_categories,
                  on_category_select, has_rom);

  // Draw the Side Panel (Content) if expanded
  if (panel_expanded_) {
    DrawSidePanel(session_id, category, has_rom);
  }
}

void EditorCardRegistry::DrawUtilityButtons(std::function<bool()> has_rom) {
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  bool rom_loaded = has_rom ? has_rom() : false;

  ImVec4 inactive = gui::ConvertColorToImVec4(theme.text_secondary);
  ImVec4 disabled = ImVec4(inactive.x, inactive.y, inactive.z, 0.4f);

  // Button style - transparent background


  // Save ROM button
  {
    bool is_active = false; // Utility buttons don't have active state logic here yet
    bool is_disabled = !rom_loaded;
    
    // Save
    if (gui::TransparentIconButton(ICON_MD_SAVE, ImVec2(48.0f, 32.0f), 
                                                "Save ROM (Ctrl+S)", false)) {
      if (rom_loaded && on_save_rom_) on_save_rom_();
    }

    // Undo
    if (gui::TransparentIconButton(ICON_MD_UNDO, ImVec2(48.0f, 32.0f), 
                                                "Undo (Ctrl+Z)", false)) {
      if (rom_loaded && on_undo_) on_undo_();
    }

    // Redo
    if (gui::TransparentIconButton(ICON_MD_REDO, ImVec2(48.0f, 32.0f), 
                                                "Redo (Ctrl+Y)", false)) {
      if (rom_loaded && on_redo_) on_redo_();
    }

    // Search
    if (gui::TransparentIconButton(ICON_MD_SEARCH, ImVec2(48.0f, 32.0f), 
                                                "Global Search (Ctrl+Shift+F)", false)) {
      if (on_show_search_) on_show_search_();
    }

    // Help
    if (gui::TransparentIconButton(ICON_MD_HELP_OUTLINE, ImVec2(48.0f, 32.0f), 
                                                "Help (F1)", false)) {
      if (on_show_help_) on_show_help_();
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

void EditorCardRegistry::DrawActivityBar(
    size_t session_id, const std::string& active_category,
    const std::vector<std::string>& all_categories,
    const std::unordered_set<std::string>& active_editor_categories,
    std::function<void(const std::string&)> on_category_select,
    std::function<bool()> has_rom) {
  // on_category_select is reserved for future use (e.g., "Open Editor" button in panel header)
  (void)session_id;
  (void)on_category_select;
  
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

    ImVec4 accent = gui::ConvertColorToImVec4(theme.accent);
    ImVec4 inactive = gui::ConvertColorToImVec4(theme.text_secondary);
    ImVec4 disabled = ImVec4(inactive.x, inactive.y, inactive.z, 0.4f);

    bool rom_loaded = has_rom ? has_rom() : false;

    // Draw ALL editor categories (not just active ones)
    for (const auto& cat : all_categories) {
      bool is_selected = (cat == active_category) && panel_expanded_;
      bool has_active_editor = active_editor_categories.count(cat) > 0;
      
      // Emulator is always available, others require ROM
      bool category_enabled = rom_loaded || (cat == "Emulator");

      // Active Indicator (Left Border) - shown when category is selected
      if (is_selected && category_enabled) {
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImGui::GetCursorScreenPos(),
            ImVec2(ImGui::GetCursorScreenPos().x + 3.0f, 
                   ImGui::GetCursorScreenPos().y + 40.0f),
            ImGui::ColorConvertFloat4ToU32(gui::ConvertColorToImVec4(theme.primary))); // Use primary instead of accent
      }

      std::string icon = GetCategoryIcon(cat);
      
      // Use ThemedWidgets for consistent styling
      // is_active = is_selected (highlights icon with primary color)
      if (gui::TransparentIconButton(icon.c_str(), ImVec2(48.0f, 40.0f), 
                                                  nullptr, is_selected)) {
        if (category_enabled) {
          if (cat == active_category && panel_expanded_) {
            TogglePanelExpanded();
          } else {
            SetActiveCategory(cat);
            SetPanelExpanded(true);
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
    // Note: Collapse button moved to menu bar for better UX
  }

  // Draw "More Actions" button at the bottom
  ImGui::SetCursorPosY(viewport_height - 48.0f);
  
  if (gui::TransparentIconButton(ICON_MD_MORE_HORIZ, ImVec2(48.0f, 40.0f))) {
    ImGui::OpenPopup("ActivityBarMoreMenu");
  }

  if (ImGui::BeginPopup("ActivityBarMoreMenu")) {
    if (ImGui::MenuItem(ICON_MD_SETTINGS " Settings")) {
      if (on_show_settings_) on_show_settings_();
    }
    if (ImGui::MenuItem(ICON_MD_HELP_OUTLINE " Help")) {
      if (on_show_help_) on_show_help_();
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

    // Header Buttons (Right Aligned)
    float avail_width = ImGui::GetContentRegionAvail().x;
    float button_size = 24.0f;
    float spacing = 4.0f;
    float current_x = ImGui::GetCursorPosX() + avail_width - button_size;

    // Collapse Button
    ImGui::SameLine(current_x);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4.0f);
    if (ImGui::Button(ICON_MD_KEYBOARD_DOUBLE_ARROW_LEFT, ImVec2(button_size, button_size))) {
      SetPanelExpanded(false);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Collapse Panel");
    }

    // Close All Button
    current_x -= (button_size + spacing);
    ImGui::SameLine(current_x);
    if (ImGui::Button(ICON_MD_CLOSE_FULLSCREEN, ImVec2(button_size, button_size))) {
      HideAllCardsInCategory(session_id, category);
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

    // Content - Reusing GetCardsInCategory logic
    auto cards = GetCardsInCategory(session_id, category);

    // Calculate available height for cards vs file browser
    float available_height = ImGui::GetContentRegionAvail().y;
    bool has_file_browser =
        category_file_browsers_.find(category) != category_file_browsers_.end();
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

      // Card Item with Icon
      std::string label =
          absl::StrFormat("%s  %s", card.icon.c_str(), card.display_name.c_str());
      if (ImGui::Selectable(label.c_str(), visible)) {
        // Toggle visibility
        ToggleCard(session_id, card.card_id);

        // Get the new visibility state after toggle
        bool new_visible = card.visibility_flag ? *card.visibility_flag : false;

        if (new_visible) {
          // Card was just shown - activate the associated editor
          if (on_card_clicked_) {
            on_card_clicked_(card.category);
          }

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
        auto* browser = category_file_browsers_[category].get();
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

void EditorCardRegistry::ResetToDefaults(size_t session_id,
                                         EditorType editor_type) {
  // Get category for this editor
  std::string category = EditorRegistry::GetEditorCategory(editor_type);
  if (category.empty()) {
    LOG_WARN("EditorCardRegistry",
             "No category found for editor type %d, skipping reset",
             static_cast<int>(editor_type));
    return;
  }

  // Hide all cards in this category first
  HideAllCardsInCategory(session_id, category);

  // Get default cards from LayoutPresets
  auto default_cards = LayoutPresets::GetDefaultCards(editor_type);

  // Show each default card
  for (const auto& card_id : default_cards) {
    if (ShowCard(session_id, card_id)) {
      LOG_INFO("EditorCardRegistry", "Showing default card: %s",
               card_id.c_str());
    }
  }

  LOG_INFO("EditorCardRegistry",
           "Reset %s editor to defaults (%zu cards visible)", category.c_str(),
           default_cards.size());
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

      // If card is being shown, activate the corresponding editor
      if (*info.visibility_flag && on_card_clicked_) {
        on_card_clicked_(info.category);
      }
    }
  }

  if (is_active) {
    ImGui::PopStyleColor();
  }
}

// =============================================================================
// File Browser Integration
// =============================================================================

FileBrowser* EditorCardRegistry::GetFileBrowser(const std::string& category) {
  auto it = category_file_browsers_.find(category);
  if (it != category_file_browsers_.end()) {
    return it->second.get();
  }
  return nullptr;
}

void EditorCardRegistry::EnableFileBrowser(const std::string& category,
                                           const std::string& root_path) {
  if (category_file_browsers_.find(category) == category_file_browsers_.end()) {
    auto browser = std::make_unique<FileBrowser>();

    // Set callback to forward file clicks
    browser->SetFileClickedCallback(
        [this, category](const std::string& path) {
          if (on_file_clicked_) {
            on_file_clicked_(category, path);
          }
          // Also activate the editor for this category
          if (on_card_clicked_) {
            on_card_clicked_(category);
          }
        });

    if (!root_path.empty()) {
      browser->SetRootPath(root_path);
    }

    // Set defaults for Assembly file browser
    if (category == "Assembly") {
      browser->SetFileFilter({".asm", ".s", ".65c816", ".inc", ".h"});
    }

    category_file_browsers_[category] = std::move(browser);
    LOG_INFO("EditorCardRegistry", "Enabled file browser for category: %s",
             category.c_str());
  }
}

void EditorCardRegistry::DisableFileBrowser(const std::string& category) {
  category_file_browsers_.erase(category);
}

bool EditorCardRegistry::HasFileBrowser(const std::string& category) const {
  return category_file_browsers_.find(category) !=
         category_file_browsers_.end();
}

void EditorCardRegistry::SetFileBrowserPath(const std::string& category,
                                            const std::string& path) {
  auto it = category_file_browsers_.find(category);
  if (it != category_file_browsers_.end()) {
    it->second->SetRootPath(path);
  }
}

// =============================================================================
// Card Validation
// =============================================================================

EditorCardRegistry::CardValidationResult EditorCardRegistry::ValidateCard(
    const std::string& card_id) const {
  CardValidationResult result;
  result.card_id = card_id;

  auto it = cards_.find(card_id);
  if (it == cards_.end()) {
    result.expected_title = "";
    result.found_in_imgui = false;
    result.message = "Card not registered";
    return result;
  }

  const CardInfo& info = it->second;
  result.expected_title = info.GetWindowTitle();

  // Check if ImGui has a window with this title
  ImGuiWindow* window = ImGui::FindWindowByName(result.expected_title.c_str());
  result.found_in_imgui = (window != nullptr);

  if (result.found_in_imgui) {
    result.message = "OK - Window found";
  } else {
    result.message = "FAIL - No window with title: " + result.expected_title;
  }

  return result;
}

std::vector<EditorCardRegistry::CardValidationResult>
EditorCardRegistry::ValidateCards() const {
  std::vector<CardValidationResult> results;
  results.reserve(cards_.size());

  for (const auto& [card_id, info] : cards_) {
    results.push_back(ValidateCard(card_id));
  }

  return results;
}

void EditorCardRegistry::DrawValidationReport(bool* p_open) {
  if (!p_open || !*p_open) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Card Validation Report", p_open)) {
    ImGui::End();
    return;
  }

  ImGui::TextWrapped(
      "This report shows registered cards and whether their window titles "
      "match actual ImGui windows. Failed cards may have typos in their "
      "window_title or the window may not have been drawn yet.");

  ImGui::Separator();

  // Refresh button
  static std::vector<CardValidationResult> cached_results;
  if (ImGui::Button(ICON_MD_REFRESH " Refresh")) {
    cached_results = ValidateCards();
  }

  ImGui::SameLine();
  ImGui::TextDisabled("(%zu cards registered)", cards_.size());

  ImGui::Separator();

  // Results table
  if (ImGui::BeginTable("ValidationTable", 4,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_ScrollY)) {
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 60.0f);
    ImGui::TableSetupColumn("Card ID", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Window Title", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    for (const auto& result : cached_results) {
      ImGui::TableNextRow();

      // Status column
      ImGui::TableNextColumn();
      if (result.found_in_imgui) {
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), ICON_MD_CHECK_CIRCLE);
      } else {
        ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), ICON_MD_ERROR);
      }

      // Card ID column
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(result.card_id.c_str());

      // Window Title column
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(result.expected_title.c_str());

      // Message column
      ImGui::TableNextColumn();
      if (!result.found_in_imgui) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "%s",
                           result.message.c_str());
      } else {
        ImGui::TextUnformatted(result.message.c_str());
      }
    }

    ImGui::EndTable();
  }

  // Summary
  int pass_count = 0;
  int fail_count = 0;
  for (const auto& result : cached_results) {
    if (result.found_in_imgui) {
      pass_count++;
    } else {
      fail_count++;
    }
  }

  ImGui::Separator();
  ImGui::Text("Summary: ");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%d passed", pass_count);
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "%d failed", fail_count);

  ImGui::End();
}

}  // namespace editor
}  // namespace yaze
