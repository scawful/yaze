#define IMGUI_DEFINE_MATH_OPERATORS

#include "app/editor/system/editor_card_registry.h"

#include <algorithm>
#include <cstdio>
#include <fstream>

#include "absl/strings/str_format.h"
#include "app/editor/system/editor_registry.h"
#include "app/editor/ui/layout_presets.h"
#include "app/gui/core/icons.h"
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
// Favorites and Recent
// ============================================================================

void EditorCardRegistry::ToggleFavorite(const std::string& card_id) {
  if (favorite_cards_.find(card_id) != favorite_cards_.end()) {
    favorite_cards_.erase(card_id);
  } else {
    favorite_cards_.insert(card_id);
  }
  // TODO: Persist favorites to user settings
}

bool EditorCardRegistry::IsFavorite(const std::string& card_id) const {
  return favorite_cards_.find(card_id) != favorite_cards_.end();
}

void EditorCardRegistry::AddToRecent(const std::string& card_id) {
  // Remove if already exists (to move to front)
  auto it = std::find(recent_cards_.begin(), recent_cards_.end(), card_id);
  if (it != recent_cards_.end()) {
    recent_cards_.erase(it);
  }

  // Add to front
  recent_cards_.insert(recent_cards_.begin(), card_id);

  // Trim if needed
  if (recent_cards_.size() > kMaxRecentCards) {
    recent_cards_.resize(kMaxRecentCards);
  }
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

}  // namespace editor
}  // namespace yaze
