#ifndef YAZE_APP_EDITOR_SYSTEM_EDITOR_CARD_REGISTRY_H_
#define YAZE_APP_EDITOR_SYSTEM_EDITOR_CARD_REGISTRY_H_

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "imgui/imgui.h"

namespace yaze {
namespace editor {

// Forward declaration
class EditorCard;

/**
 * @struct CardInfo
 * @brief Metadata for an editor card
 * 
 * Describes a registerable UI card that can be shown/hidden,
 * organized by category, and controlled programmatically.
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
 * @class EditorCardRegistry
 * @brief Central registry for all editor cards with session awareness and dependency injection
 * 
 * This class combines the functionality of EditorCardManager (global card management)
 * and SessionCardRegistry (session-aware prefixing) into a single, dependency-injected
 * component that can be passed to editors.
 * 
 * Design Philosophy:
 * - Dependency injection (no singleton pattern)
 * - Session-aware card ID prefixing for multi-session support
 * - Centralized visibility management
 * - View menu integration
 * - Workspace preset system
 * - No direct GUI dependency in registration logic
 * 
 * Session-Aware Card IDs:
 * - Single session: "dungeon.room_selector"
 * - Multiple sessions: "s0.dungeon.room_selector", "s1.dungeon.room_selector"
 * 
 * Usage:
 * ```cpp
 * // In EditorManager:
 * EditorCardRegistry card_registry;
 * EditorDependencies deps;
 * deps.card_registry = &card_registry;
 * 
 * // In Editor:
 * deps.card_registry->RegisterCard(deps.session_id, {
 *     .card_id = "dungeon.room_selector",
 *     .display_name = "Room Selector",
 *     .icon = ICON_MD_LIST,
 *     .category = "Dungeon",
 *     .on_show = []() {  }
 * });
 * 
 * // Programmatic control:
 * deps.card_registry->ShowCard(deps.session_id, "dungeon.room_selector");
 * ```
 */
class EditorCardRegistry {
 public:
  EditorCardRegistry() = default;
  ~EditorCardRegistry() = default;
  
  // Non-copyable, non-movable (manages pointers and callbacks)
  EditorCardRegistry(const EditorCardRegistry&) = delete;
  EditorCardRegistry& operator=(const EditorCardRegistry&) = delete;
  EditorCardRegistry(EditorCardRegistry&&) = delete;
  EditorCardRegistry& operator=(EditorCardRegistry&&) = delete;
  
  // ============================================================================
  // Session Lifecycle Management
  // ============================================================================
  
  /**
   * @brief Register a new session in the registry
   * @param session_id Unique session identifier
   * 
   * Creates internal tracking structures for the session.
   * Must be called before registering cards for a session.
   */
  void RegisterSession(size_t session_id);
  
  /**
   * @brief Unregister a session and all its cards
   * @param session_id Session identifier to remove
   * 
   * Automatically unregisters all cards associated with the session.
   */
  void UnregisterSession(size_t session_id);
  
  /**
   * @brief Set the currently active session
   * @param session_id Session to make active
   * 
   * Used for determining whether to apply card ID prefixing.
   */
  void SetActiveSession(size_t session_id);
  
  // ============================================================================
  // Card Registration
  // ============================================================================
  
  /**
   * @brief Register a card for a specific session
   * @param session_id Session this card belongs to
   * @param base_info Card metadata (ID will be automatically prefixed if needed)
   * 
   * The card_id in base_info should be the unprefixed ID. This method
   * automatically applies session prefixing when multiple sessions exist.
   */
  void RegisterCard(size_t session_id, const CardInfo& base_info);
  
  /**
   * @brief Register a card with inline parameters (convenience method)
   */
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
  
  /**
   * @brief Unregister a specific card
   * @param session_id Session the card belongs to
   * @param base_card_id Unprefixed card ID
   */
  void UnregisterCard(size_t session_id, const std::string& base_card_id);
  
  /**
   * @brief Unregister all cards with a given prefix
   * @param prefix Prefix to match (e.g., "s0" or "s1.dungeon")
   * 
   * Useful for cleaning up session cards or category cards.
   */
  void UnregisterCardsWithPrefix(const std::string& prefix);
  
  /**
   * @brief Remove all registered cards (use with caution)
   */
  void ClearAllCards();
  
  // ============================================================================
  // Card Control (Programmatic, No GUI)
  // ============================================================================
  
  /**
   * @brief Show a card programmatically
   * @param session_id Session the card belongs to
   * @param base_card_id Unprefixed card ID
   * @return true if card was found and shown
   */
  bool ShowCard(size_t session_id, const std::string& base_card_id);
  
  /**
   * @brief Hide a card programmatically
   */
  bool HideCard(size_t session_id, const std::string& base_card_id);
  
  /**
   * @brief Toggle a card's visibility
   */
  bool ToggleCard(size_t session_id, const std::string& base_card_id);
  
  /**
   * @brief Check if a card is currently visible
   */
  bool IsCardVisible(size_t session_id, const std::string& base_card_id) const;
  
  /**
   * @brief Get visibility flag pointer for a card
   * @return Pointer to bool controlling card visibility (for passing to EditorCard::Begin)
   */
  bool* GetVisibilityFlag(size_t session_id, const std::string& base_card_id);
  
  // ============================================================================
  // Batch Operations
  // ============================================================================
  
  /**
   * @brief Show all cards in a specific session
   */
  void ShowAllCardsInSession(size_t session_id);
  
  /**
   * @brief Hide all cards in a specific session
   */
  void HideAllCardsInSession(size_t session_id);
  
  /**
   * @brief Show all cards in a category for a session
   */
  void ShowAllCardsInCategory(size_t session_id, const std::string& category);
  
  /**
   * @brief Hide all cards in a category for a session
   */
  void HideAllCardsInCategory(size_t session_id, const std::string& category);
  
  /**
   * @brief Show only one card, hiding all others in its category
   */
  void ShowOnlyCard(size_t session_id, const std::string& base_card_id);
  
  // ============================================================================
  // Query Methods
  // ============================================================================
  
  /**
   * @brief Get all cards registered for a session
   * @return Vector of prefixed card IDs
   */
  std::vector<std::string> GetCardsInSession(size_t session_id) const;
  
  /**
   * @brief Get cards in a specific category for a session
   */
  std::vector<CardInfo> GetCardsInCategory(size_t session_id, const std::string& category) const;
  
  /**
   * @brief Get all categories for a session
   */
  std::vector<std::string> GetAllCategories(size_t session_id) const;
  
  /**
   * @brief Get card metadata
   * @param session_id Session the card belongs to
   * @param base_card_id Unprefixed card ID
   */
  const CardInfo* GetCardInfo(size_t session_id, const std::string& base_card_id) const;
  
  /**
   * @brief Get all registered categories across all sessions
   */
  std::vector<std::string> GetAllCategories() const;
  
  // ============================================================================
  // View Menu Integration
  // ============================================================================
  
  /**
   * @brief Draw view menu section for a category
   */
  void DrawViewMenuSection(size_t session_id, const std::string& category);
  
  /**
   * @brief Draw all categories as view menu submenus
   */
  void DrawViewMenuAll(size_t session_id);
  
  // ============================================================================
  // VSCode-Style Sidebar
  // ============================================================================
  
  /**
   * @brief Draw sidebar for a category with session filtering
   */
  void DrawSidebar(size_t session_id,
                  const std::string& category,
                  const std::vector<std::string>& active_categories = {},
                  std::function<void(const std::string&)> on_category_switch = nullptr,
                  std::function<void()> on_collapse = nullptr);
  
  static constexpr float GetSidebarWidth() { return 48.0f; }
  
  // ============================================================================
  // Compact Controls for Menu Bar
  // ============================================================================
  
  /**
   * @brief Draw compact card control for active editor's cards
   */
  void DrawCompactCardControl(size_t session_id, const std::string& category);
  
  /**
   * @brief Draw minimal inline card toggles
   */
  void DrawInlineCardToggles(size_t session_id, const std::string& category);
  
  // ============================================================================
  // Card Browser UI
  // ============================================================================
  
  /**
   * @brief Draw visual card browser/toggler
   */
  void DrawCardBrowser(size_t session_id, bool* p_open);
  
  // ============================================================================
  // Workspace Presets
  // ============================================================================
  
  struct WorkspacePreset {
    std::string name;
    std::vector<std::string> visible_cards;  // Card IDs
    std::string description;
  };
  
  void SavePreset(const std::string& name, const std::string& description = "");
  bool LoadPreset(const std::string& name);
  void DeletePreset(const std::string& name);
  std::vector<WorkspacePreset> GetPresets() const;
  
  // ============================================================================
  // Quick Actions
  // ============================================================================
  
  void ShowAll(size_t session_id);
  void HideAll(size_t session_id);
  void ResetToDefaults(size_t session_id);
  
  // ============================================================================
  // Statistics
  // ============================================================================
  
  size_t GetCardCount() const { return cards_.size(); }
  size_t GetVisibleCardCount(size_t session_id) const;
  size_t GetSessionCount() const { return session_count_; }
  
  // ============================================================================
  // Session Prefixing Utilities
  // ============================================================================
  
  /**
   * @brief Generate session-aware card ID
   * @param session_id Session identifier
   * @param base_id Unprefixed card ID
   * @return Prefixed ID if multiple sessions, otherwise base ID
   * 
   * Examples:
   * - Single session: "dungeon.room_selector" → "dungeon.room_selector"
   * - Multi-session: "dungeon.room_selector" → "s0.dungeon.room_selector"
   */
  std::string MakeCardId(size_t session_id, const std::string& base_id) const;
  
  /**
   * @brief Check if card IDs should be prefixed
   * @return true if session_count > 1
   */
  bool ShouldPrefixCards() const { return session_count_ > 1; }

 private:
  // Core card storage (prefixed IDs → CardInfo)
  std::unordered_map<std::string, CardInfo> cards_;
  
  // Centralized visibility flags for cards without external flags
  std::unordered_map<std::string, bool> centralized_visibility_;
  
  // Session tracking
  size_t session_count_ = 0;
  size_t active_session_ = 0;
  
  // Maps session_id → vector of prefixed card IDs registered for that session
  std::unordered_map<size_t, std::vector<std::string>> session_cards_;
  
  // Maps session_id → (base_card_id → prefixed_card_id)
  std::unordered_map<size_t, std::unordered_map<std::string, std::string>> session_card_mapping_;
  
  // Workspace presets
  std::unordered_map<std::string, WorkspacePreset> presets_;
  
  // Active category tracking
  std::string active_category_;
  std::vector<std::string> recent_categories_;
  static constexpr size_t kMaxRecentCategories = 5;
  
  // Helper methods
  void UpdateSessionCount();
  std::string GetPrefixedCardId(size_t session_id, const std::string& base_id) const;
  void UnregisterSessionCards(size_t session_id);
  void SavePresetsToFile();
  void LoadPresetsFromFile();
  
  // UI drawing helpers (internal)
  void DrawCardMenuItem(const CardInfo& info);
  void DrawCardInSidebar(const CardInfo& info, bool is_active);
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_EDITOR_CARD_REGISTRY_H_

