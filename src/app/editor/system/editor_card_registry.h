#ifndef YAZE_APP_EDITOR_SYSTEM_EDITOR_CARD_REGISTRY_H_
#define YAZE_APP_EDITOR_SYSTEM_EDITOR_CARD_REGISTRY_H_

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "app/editor/system/file_browser.h"

namespace yaze {
namespace editor {

// Forward declarations
class EditorCard;
enum class EditorType;

/**
 * @struct CardInfo
 * @brief Metadata for an editor card
 */
struct CardInfo {
  std::string card_id;       // Unique identifier (e.g., "dungeon.room_selector")
  std::string display_name;  // Human-readable name (e.g., "Room Selector")
  std::string window_title;  // ImGui window title for DockBuilder (e.g., " Rooms List")
  std::string icon;          // Material icon
  std::string category;      // Category (e.g., "Dungeon", "Graphics", "Palette")
  std::string shortcut_hint; // Display hint (e.g., "Ctrl+Shift+R")
  bool* visibility_flag;          // Pointer to bool controlling visibility
  EditorCard* card_instance;      // Pointer to actual card (optional)
  std::function<void()> on_show;  // Callback when card is shown
  std::function<void()> on_hide;  // Callback when card is hidden
  int priority;                   // Display priority for menus (lower = higher)

  // Disabled state support for IDE-like behavior
  std::function<bool()> enabled_condition;  // Returns true if card is enabled (nullptr = always enabled)
  std::string disabled_tooltip;             // Tooltip shown when hovering disabled card

  /**
   * @brief Get the effective window title for DockBuilder
   * @return window_title if set, otherwise generates from icon + display_name
   */
  std::string GetWindowTitle() const {
    if (!window_title.empty()) {
      return window_title;
    }
    // Generate from icon + display_name if window_title not explicitly set
    return icon + " " + display_name;
  }
};

/**
 * @class EditorCardRegistry
 * @brief Central registry for all editor cards with session awareness and
 * dependency injection
 */
class EditorCardRegistry {
 public:
  EditorCardRegistry() = default;
  ~EditorCardRegistry() = default;

  // Non-copyable, non-movable
  EditorCardRegistry(const EditorCardRegistry&) = delete;
  EditorCardRegistry& operator=(const EditorCardRegistry&) = delete;
  EditorCardRegistry(EditorCardRegistry&&) = delete;
  EditorCardRegistry& operator=(EditorCardRegistry&&) = delete;

  // ============================================================================
  // Session Lifecycle Management
  // ============================================================================

  void RegisterSession(size_t session_id);
  void UnregisterSession(size_t session_id);
  void SetActiveSession(size_t session_id);

  // ============================================================================
  // Card Registration
  // ============================================================================

  void RegisterCard(size_t session_id, const CardInfo& base_info);

  void RegisterCard(size_t session_id, const std::string& card_id,
                    const std::string& display_name, const std::string& icon,
                    const std::string& category,
                    const std::string& shortcut_hint = "", int priority = 50,
                    std::function<void()> on_show = nullptr,
                    std::function<void()> on_hide = nullptr,
                    bool visible_by_default = false);

  void UnregisterCard(size_t session_id, const std::string& base_card_id);
  void UnregisterCardsWithPrefix(const std::string& prefix);
  void ClearAllCards();

  // ============================================================================
  // Card Control (Programmatic)
  // ============================================================================

  bool ShowCard(size_t session_id, const std::string& base_card_id);
  bool HideCard(size_t session_id, const std::string& base_card_id);
  bool ToggleCard(size_t session_id, const std::string& base_card_id);
  bool IsCardVisible(size_t session_id, const std::string& base_card_id) const;
  bool* GetVisibilityFlag(size_t session_id, const std::string& base_card_id);

  // ============================================================================
  // Batch Operations
  // ============================================================================

  void ShowAllCardsInSession(size_t session_id);
  void HideAllCardsInSession(size_t session_id);
  void ShowAllCardsInCategory(size_t session_id, const std::string& category);
  void HideAllCardsInCategory(size_t session_id, const std::string& category);
  void ShowOnlyCard(size_t session_id, const std::string& base_card_id);

  // ============================================================================
  // Query Methods
  // ============================================================================

  std::vector<std::string> GetCardsInSession(size_t session_id) const;
  std::vector<CardInfo> GetCardsInCategory(size_t session_id,
                                           const std::string& category) const;
  std::vector<std::string> GetAllCategories(size_t session_id) const;
  const CardInfo* GetCardInfo(size_t session_id,
                              const std::string& base_card_id) const;
  std::vector<std::string> GetAllCategories() const;

  static constexpr float GetSidebarWidth() { return 48.0f; }
  static constexpr float GetSidePanelWidth() { return 250.0f; }
  static constexpr float GetCollapsedSidebarWidth() { return 16.0f; }

  static std::string GetCategoryIcon(const std::string& category);

  /**
   * @brief Handle keyboard navigation in sidebar (click-to-focus modal)
   */
  void HandleSidebarKeyboardNav(size_t session_id,
                                const std::vector<CardInfo>& cards);

  bool SidebarHasFocus() const { return sidebar_has_focus_; }
  int GetFocusedCardIndex() const { return focused_card_index_; }

  void ToggleSidebarVisibility() {
    sidebar_visible_ = !sidebar_visible_;
    if (on_sidebar_state_changed_) {
      on_sidebar_state_changed_(sidebar_visible_, panel_expanded_);
    }
  }

  void SetSidebarVisible(bool visible) {
    if (sidebar_visible_ != visible) {
      sidebar_visible_ = visible;
      if (on_sidebar_state_changed_) {
        on_sidebar_state_changed_(sidebar_visible_, panel_expanded_);
      }
    }
  }

  bool IsSidebarVisible() const { return sidebar_visible_; }

  void TogglePanelExpanded() {
    panel_expanded_ = !panel_expanded_;
    if (on_sidebar_state_changed_) {
      on_sidebar_state_changed_(sidebar_visible_, panel_expanded_);
    }
  }

  void SetPanelExpanded(bool expanded) {
    if (panel_expanded_ != expanded) {
      panel_expanded_ = expanded;
      if (on_sidebar_state_changed_) {
        on_sidebar_state_changed_(sidebar_visible_, panel_expanded_);
      }
    }
  }

  bool IsPanelExpanded() const { return panel_expanded_; }

  // ============================================================================
  // Triggers (exposed for ActivityBar)
  // ============================================================================

  void TriggerShowEmulator() { if (on_show_emulator_) on_show_emulator_(); }
  void TriggerShowSettings() { if (on_show_settings_) on_show_settings_(); }
  void TriggerShowCardBrowser() { if (on_show_card_browser_) on_show_card_browser_(); }
  void TriggerSaveRom() { if (on_save_rom_) on_save_rom_(); }
  void TriggerUndo() { if (on_undo_) on_undo_(); }
  void TriggerRedo() { if (on_redo_) on_redo_(); }
  void TriggerShowSearch() { if (on_show_search_) on_show_search_(); }
  void TriggerShowHelp() { if (on_show_help_) on_show_help_(); }
  void TriggerCardClicked(const std::string& category) { if (on_card_clicked_) on_card_clicked_(category); }

  // ============================================================================
  // Utility Icon Callbacks (for sidebar quick access buttons)
  // ============================================================================

  void SetShowEmulatorCallback(std::function<void()> cb) {
    on_show_emulator_ = std::move(cb);
  }
  void SetShowSettingsCallback(std::function<void()> cb) {
    on_show_settings_ = std::move(cb);
  }
  void SetShowCardBrowserCallback(std::function<void()> cb) {
    on_show_card_browser_ = std::move(cb);
  }
  void SetSaveRomCallback(std::function<void()> cb) {
    on_save_rom_ = std::move(cb);
  }
  void SetUndoCallback(std::function<void()> cb) {
    on_undo_ = std::move(cb);
  }
  void SetRedoCallback(std::function<void()> cb) {
    on_redo_ = std::move(cb);
  }
  void SetShowSearchCallback(std::function<void()> cb) {
    on_show_search_ = std::move(cb);
  }
  void SetShowHelpCallback(std::function<void()> cb) {
    on_show_help_ = std::move(cb);
  }
  void SetSidebarStateChangedCallback(
      std::function<void(bool, bool)> cb) {
    on_sidebar_state_changed_ = std::move(cb);
  }

  // ============================================================================
  // Unified Visibility Management (single source of truth)
  // ============================================================================

  bool IsEmulatorVisible() const { return emulator_visible_; }
  void SetEmulatorVisible(bool visible) {
    if (emulator_visible_ != visible) {
      emulator_visible_ = visible;
      if (on_emulator_visibility_changed_) {
        on_emulator_visibility_changed_(visible);
      }
    }
  }
  void ToggleEmulatorVisible() { SetEmulatorVisible(!emulator_visible_); }
  void SetEmulatorVisibilityChangedCallback(std::function<void(bool)> cb) {
    on_emulator_visibility_changed_ = std::move(cb);
  }
  void SetCategoryChangedCallback(std::function<void(const std::string&)> cb) {
    on_category_changed_ = std::move(cb);
  }

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
  // Card Validation (for catching window title mismatches)
  // ============================================================================

  struct CardValidationResult {
    std::string card_id;
    std::string expected_title;  // From CardInfo::GetWindowTitle()
    bool found_in_imgui;         // Whether ImGui found a window with this title
    std::string message;         // Human-readable status
  };

  std::vector<CardValidationResult> ValidateCards() const;
  CardValidationResult ValidateCard(const std::string& card_id) const;

  // ============================================================================
  // Quick Actions
  // ============================================================================

  void ShowAll(size_t session_id);
  void HideAll(size_t session_id);
  void ResetToDefaults(size_t session_id);
  void ResetToDefaults(size_t session_id, EditorType editor_type);

  // ============================================================================
  // Statistics
  // ============================================================================

  size_t GetCardCount() const { return cards_.size(); }
  size_t GetVisibleCardCount(size_t session_id) const;
  size_t GetSessionCount() const { return session_count_; }

  // ============================================================================
  // Session Prefixing Utilities
  // ============================================================================

  std::string MakeCardId(size_t session_id, const std::string& base_id) const;
  bool ShouldPrefixCards() const { return session_count_ > 1; }

  // ============================================================================
  // Convenience Methods (for EditorManager direct usage without session_id)
  // ============================================================================

  void RegisterCard(const CardInfo& base_info) {
    RegisterCard(active_session_, base_info);
  }
  bool ShowCard(const std::string& base_card_id) {
    return ShowCard(active_session_, base_card_id);
  }
  bool HideCard(const std::string& base_card_id) {
    return HideCard(active_session_, base_card_id);
  }
  bool IsCardVisible(const std::string& base_card_id) const {
    return IsCardVisible(active_session_, base_card_id);
  }
  void HideAllCardsInCategory(const std::string& category) {
    HideAllCardsInCategory(active_session_, category);
  }
  std::string GetActiveCategory() const { return active_category_; }
  void SetActiveCategory(const std::string& category) {
    if (active_category_ != category) {
      active_category_ = category;
      if (on_category_changed_) {
        on_category_changed_(category);
      }
    }
  }
  void ShowAllCardsInCategory(const std::string& category) {
    ShowAllCardsInCategory(active_session_, category);
  }
  bool* GetVisibilityFlag(const std::string& base_card_id) {
    return GetVisibilityFlag(active_session_, base_card_id);
  }
  void ShowAll() { ShowAll(active_session_); }
  void HideAll() { HideAll(active_session_); }
  void SetOnCardClickedCallback(std::function<void(const std::string&)> callback) {
    on_card_clicked_ = std::move(callback);
  }

  // ============================================================================
  // File Browser Integration
  // ============================================================================

  FileBrowser* GetFileBrowser(const std::string& category);
  void EnableFileBrowser(const std::string& category,
                         const std::string& root_path = "");
  void DisableFileBrowser(const std::string& category);
  bool HasFileBrowser(const std::string& category) const;
  void SetFileBrowserPath(const std::string& category, const std::string& path);
  void SetFileClickedCallback(
      std::function<void(const std::string& category, const std::string& path)>
          callback) {
    on_file_clicked_ = std::move(callback);
  }

  // ============================================================================
  // Favorites and Recent
  // ============================================================================

  void ToggleFavorite(const std::string& card_id);
  bool IsFavorite(const std::string& card_id) const;
  void AddToRecent(const std::string& card_id);
  const std::vector<std::string>& GetRecentCards() const { return recent_cards_; }
  const std::unordered_set<std::string>& GetFavoriteCards() const { return favorite_cards_; }

 private:
  // Core card storage (prefixed IDs → CardInfo)
  std::unordered_map<std::string, CardInfo> cards_;

  // Favorites and Recent tracking
  std::unordered_set<std::string> favorite_cards_;
  std::vector<std::string> recent_cards_;
  static constexpr size_t kMaxRecentCards = 10;

  // Centralized visibility flags for cards without external flags
  std::unordered_map<std::string, bool> centralized_visibility_;

  // Session tracking
  size_t session_count_ = 0;
  size_t active_session_ = 0;

  // Maps session_id → vector of prefixed card IDs registered for that session
  std::unordered_map<size_t, std::vector<std::string>> session_cards_;

  // Maps session_id → (base_card_id → prefixed_card_id)
  std::unordered_map<size_t, std::unordered_map<std::string, std::string>>
      session_card_mapping_;

  // Workspace presets
  std::unordered_map<std::string, WorkspacePreset> presets_;

  // Active category tracking
  std::string active_category_;
  std::vector<std::string> recent_categories_;
  static constexpr size_t kMaxRecentCategories = 5;

  // Sidebar state
  bool sidebar_visible_ = true;    // Controls Activity Bar visibility (0px vs 48px)
  bool panel_expanded_ = true;     // Controls Side Panel visibility (0px vs 250px)

  // Keyboard navigation state (click-to-focus modal)
  int focused_card_index_ = -1;    // Currently focused card index (-1 = none)
  bool sidebar_has_focus_ = false; // Whether sidebar has keyboard focus

  // Unified visibility state (single source of truth)
  bool emulator_visible_ = false;  // Emulator window visibility

  // Utility icon callbacks
  std::function<void()> on_show_emulator_;
  std::function<void()> on_show_settings_;
  std::function<void()> on_show_card_browser_;
  std::function<void()> on_save_rom_;
  std::function<void()> on_undo_;
  std::function<void()> on_redo_;
  std::function<void()> on_show_search_;
  std::function<void()> on_show_help_;

  // State change callbacks
  std::function<void(bool visible, bool expanded)> on_sidebar_state_changed_;
  std::function<void(const std::string&)> on_category_changed_;
  std::function<void(const std::string&)> on_card_clicked_;
  std::function<void(bool)> on_emulator_visibility_changed_;
  std::function<void(const std::string&, const std::string&)> on_file_clicked_;

  // File browser for categories that support it (e.g., Assembly)
  std::unordered_map<std::string, std::unique_ptr<FileBrowser>>
      category_file_browsers_;

  // Tracking active editor categories for visual feedback
  std::unordered_set<std::string> active_editor_categories_;

  // Helper methods
  void UpdateSessionCount();
  std::string GetPrefixedCardId(size_t session_id,
                                const std::string& base_id) const;
  void UnregisterSessionCards(size_t session_id);
  void SavePresetsToFile();
  void LoadPresetsFromFile();
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_EDITOR_CARD_REGISTRY_H_
