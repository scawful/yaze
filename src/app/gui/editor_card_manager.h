#ifndef YAZE_APP_GUI_EDITOR_CARD_MANAGER_H
#define YAZE_APP_GUI_EDITOR_CARD_MANAGER_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "imgui/imgui.h"

namespace yaze {
namespace gui {

class EditorCard;  // Forward declaration

/**
 * @brief Metadata for an editor card
 */
struct CardInfo {
  std::string card_id;           // Unique identifier (e.g., "dungeon.room_selector")
  std::string display_name;      // Human-readable name (e.g., "Room Selector")
  std::string icon;              // Material icon
  std::string category;          // Category (e.g., "Dungeon", "Graphics", "Palette")
  std::string shortcut_hint;     // Display hint (e.g., "Ctrl+Shift+R")
  bool* visibility_flag;         // Pointer to bool controlling visibility
  EditorCard* card_instance;     // Pointer to actual card (optional)
  std::function<void()> on_show; // Callback when card is shown
  std::function<void()> on_hide; // Callback when card is hidden
  int priority;                  // Display priority for menus (lower = higher)
};

/**
 * @brief Central registry and manager for all editor cards
 * 
 * This singleton provides:
 * - Global card registration across all editors
 * - View menu integration
 * - Keyboard shortcut management
 * - Workspace preset system
 * - Quick search/filter
 * - Programmatic card control without GUI coupling
 * 
 * Design Philosophy:
 * - Cards register themselves on creation
 * - Manager provides unified interface for visibility control
 * - No direct GUI dependency in card logic
 * - Supports dynamic card creation/destruction
 * 
 * Usage:
 * ```cpp
 * // In editor initialization:
 * auto& manager = EditorCardManager::Get();
 * manager.RegisterCard({
 *     .card_id = "dungeon.room_selector",
 *     .display_name = "Room Selector",
 *     .icon = ICON_MD_LIST,
 *     .category = "Dungeon",
 *     .visibility_flag = &show_room_selector_,
 *     .on_show = []() { printf("Room selector opened\n"); }
 * });
 * 
 * // Programmatic control:
 * manager.ShowCard("dungeon.room_selector");
 * manager.HideCard("dungeon.room_selector");
 * manager.ToggleCard("dungeon.room_selector");
 * 
 * // In View menu:
 * manager.DrawViewMenuSection("Dungeon");
 * ```
 */
class EditorCardManager {
 public:
  static EditorCardManager& Get();
  
  // Registration
  void RegisterCard(const CardInfo& info);
  void UnregisterCard(const std::string& card_id);
  void ClearAllCards();
  
  // Card control (programmatic, no GUI)
  bool ShowCard(const std::string& card_id);
  bool HideCard(const std::string& card_id);
  bool ToggleCard(const std::string& card_id);
  bool IsCardVisible(const std::string& card_id) const;
  
  // Batch operations
  void ShowAllCardsInCategory(const std::string& category);
  void HideAllCardsInCategory(const std::string& category);
  void ShowOnlyCard(const std::string& card_id);  // Hide all others in category
  
  // Query
  std::vector<CardInfo> GetCardsInCategory(const std::string& category) const;
  std::vector<std::string> GetAllCategories() const;
  const CardInfo* GetCardInfo(const std::string& card_id) const;
  
  // View menu integration
  void DrawViewMenuSection(const std::string& category);
  void DrawViewMenuAll();  // Draw all categories as submenus
  
  // Card browser UI
  void DrawCardBrowser(bool* p_open);  // Visual card browser/toggler
  void DrawCardBrowserTable(const char* search_filter, const std::string& category_filter);
  void DrawPresetsTab();
  
  // Workspace presets
  struct WorkspacePreset {
    std::string name;
    std::vector<std::string> visible_cards;  // Card IDs
    std::string description;
  };
  
  void SavePreset(const std::string& name, const std::string& description = "");
  bool LoadPreset(const std::string& name);
  void DeletePreset(const std::string& name);
  std::vector<WorkspacePreset> GetPresets() const;
  
  // Quick actions
  void ShowAll();  // Show all registered cards
  void HideAll();  // Hide all registered cards
  void ResetToDefaults();  // Reset to default visibility state
  
  // Statistics
  size_t GetCardCount() const { return cards_.size(); }
  size_t GetVisibleCardCount() const;
  
 private:
  EditorCardManager() = default;
  ~EditorCardManager() = default;
  EditorCardManager(const EditorCardManager&) = delete;
  EditorCardManager& operator=(const EditorCardManager&) = delete;
  
  std::unordered_map<std::string, CardInfo> cards_;
  std::unordered_map<std::string, WorkspacePreset> presets_;
  
  // Helper methods
  void SavePresetsToFile();
  void LoadPresetsFromFile();
};

/**
 * @brief RAII helper for auto-registering cards
 * 
 * Usage:
 * ```cpp
 * class MyEditor {
 *   CardRegistration room_selector_reg_;
 *   
 *   MyEditor() {
 *     room_selector_reg_ = RegisterCard({
 *       .card_id = "myeditor.room_selector",
 *       .display_name = "Room Selector",
 *       .visibility_flag = &show_room_selector_
 *     });
 *   }
 * };
 * ```
 */
class CardRegistration {
 public:
  CardRegistration() = default;
  explicit CardRegistration(const std::string& card_id) : card_id_(card_id) {}
  
  ~CardRegistration() {
    if (!card_id_.empty()) {
      EditorCardManager::Get().UnregisterCard(card_id_);
    }
  }
  
  // No copy, allow move
  CardRegistration(const CardRegistration&) = delete;
  CardRegistration& operator=(const CardRegistration&) = delete;
  CardRegistration(CardRegistration&& other) noexcept : card_id_(std::move(other.card_id_)) {
    other.card_id_.clear();
  }
  CardRegistration& operator=(CardRegistration&& other) noexcept {
    if (this != &other) {
      card_id_ = std::move(other.card_id_);
      other.card_id_.clear();
    }
    return *this;
  }
  
 private:
  std::string card_id_;
};

// Convenience function for registration
inline CardRegistration RegisterCard(const CardInfo& info) {
  EditorCardManager::Get().RegisterCard(info);
  return CardRegistration(info.card_id);
}

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_EDITOR_CARD_MANAGER_H

