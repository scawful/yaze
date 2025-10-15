#ifndef YAZE_APP_EDITOR_SYSTEM_SESSION_CARD_REGISTRY_H_
#define YAZE_APP_EDITOR_SYSTEM_SESSION_CARD_REGISTRY_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "app/gui/app/editor_card_manager.h"

namespace yaze {
namespace editor {

/**
 * @class SessionCardRegistry
 * @brief Manages session-scoped card registration with automatic prefixing
 * 
 * This class wraps EditorCardManager to provide session awareness:
 * - Automatically prefixes card IDs when multiple sessions exist
 * - Manages per-session card lifecycle (create/destroy/switch)
 * - Maintains session-specific card visibility state
 * - Provides clean API for session-aware card operations
 * 
 * Card ID Format:
 * - Single session: "dungeon.room_selector"
 * - Multiple sessions: "s0.dungeon.room_selector", "s1.dungeon.room_selector"
 */
class SessionCardRegistry {
 public:
  SessionCardRegistry() = default;
  ~SessionCardRegistry() = default;

  // Session lifecycle management
  void RegisterSession(size_t session_id);
  void UnregisterSession(size_t session_id);
  void SetActiveSession(size_t session_id);
  
  // Card registration with session awareness
  void RegisterCard(size_t session_id, const gui::CardInfo& base_info);
  void RegisterCard(size_t session_id,
                   const std::string& card_id,
                   const std::string& display_name,
                   const std::string& icon,
                   const std::string& category,
                   const std::string& shortcut_hint = "",
                   int priority = 50,
                   std::function<void()> on_show = nullptr,
                   std::function<void()> on_hide = nullptr,
                   bool visible_by_default = false);
  
  // Card control with session awareness
  bool ShowCard(size_t session_id, const std::string& base_card_id);
  bool HideCard(size_t session_id, const std::string& base_card_id);
  bool ToggleCard(size_t session_id, const std::string& base_card_id);
  bool IsCardVisible(size_t session_id, const std::string& base_card_id) const;
  
  // Batch operations
  void ShowAllCardsInSession(size_t session_id);
  void HideAllCardsInSession(size_t session_id);
  void ShowAllCardsInCategory(size_t session_id, const std::string& category);
  void HideAllCardsInCategory(size_t session_id, const std::string& category);
  
  // Utility methods
  std::string MakeCardId(size_t session_id, const std::string& base_id) const;
  bool ShouldPrefixCards() const { return session_count_ > 1; }
  size_t GetSessionCount() const { return session_count_; }
  size_t GetActiveSession() const { return active_session_; }
  
  // Query methods
  std::vector<std::string> GetCardsInSession(size_t session_id) const;
  std::vector<std::string> GetCardsInCategory(size_t session_id, const std::string& category) const;
  std::vector<std::string> GetAllCategories(size_t session_id) const;
  
  // Direct access to underlying card manager (for UI components)
  gui::EditorCardManager& GetCardManager() { return gui::EditorCardManager::Get(); }
  const gui::EditorCardManager& GetCardManager() const { return gui::EditorCardManager::Get(); }

 private:
  size_t session_count_ = 0;
  size_t active_session_ = 0;
  
  // Maps session_id -> vector of card IDs registered for that session
  std::unordered_map<size_t, std::vector<std::string>> session_cards_;
  
  // Maps session_id -> map of base_card_id -> prefixed_card_id
  std::unordered_map<size_t, std::unordered_map<std::string, std::string>> session_card_mapping_;
  
  // Helper methods
  void UpdateSessionCount();
  std::string GetPrefixedCardId(size_t session_id, const std::string& base_id) const;
  void UnregisterSessionCards(size_t session_id);
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_SESSION_CARD_REGISTRY_H_
