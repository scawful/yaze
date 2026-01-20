#ifndef APP_EDITOR_SYSTEM_PANEL_MANAGER_H_
#define APP_EDITOR_SYSTEM_PANEL_MANAGER_H_

#include <functional>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "app/editor/system/editor_panel.h"
#include "app/editor/system/file_browser.h"

#ifndef YAZE_ENABLE_CARD_SHIM_DEPRECATION
#define YAZE_ENABLE_CARD_SHIM_DEPRECATION 1
#endif

#if YAZE_ENABLE_CARD_SHIM_DEPRECATION && defined(__has_cpp_attribute)
#if __has_cpp_attribute(deprecated)
#define YAZE_CARD_SHIM_DEPRECATED(msg) [[deprecated(msg)]]
#else
#define YAZE_CARD_SHIM_DEPRECATED(msg)
#endif
#else
#define YAZE_CARD_SHIM_DEPRECATED(msg)
#endif

namespace yaze {

namespace gui {
class PanelWindow;
}  // namespace gui

namespace editor {

// Forward declarations
enum class EditorType;

/**
 * @struct PanelDescriptor
 * @brief Metadata for an editor panel (formerly PanelInfo)
 */
struct PanelDescriptor {
  std::string card_id;       // Unique identifier (e.g., "dungeon.room_selector")
  std::string display_name;  // Human-readable name (e.g., "Room Selector")
  std::string window_title;  // ImGui window title for DockBuilder (e.g., " Rooms List")
  std::string icon;          // Material icon
  std::string category;      // Category (e.g., "Dungeon", "Graphics", "Palette")
  PanelScope scope = PanelScope::kSession;
  enum class ShortcutScope {
    kGlobal,   // Available regardless of active editor
    kEditor,   // Only active within the owning editor
    kPanel     // Panel visibility/within-panel actions
  };
  std::string shortcut_hint; // Display hint (e.g., "Ctrl+Shift+R")
  ShortcutScope shortcut_scope = ShortcutScope::kPanel;
  bool* visibility_flag;          // Pointer to bool controlling visibility
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
 * @class PanelManager
 * @brief Central registry for all editor cards with session awareness and
 * dependency injection
 */
class PanelManager {
 public:
  PanelManager() = default;
  ~PanelManager() = default;

  // Non-copyable, non-movable
  PanelManager(const PanelManager&) = delete;
  PanelManager& operator=(const PanelManager&) = delete;
  PanelManager(PanelManager&&) = delete;
  PanelManager& operator=(PanelManager&&) = delete;

  // Special category for dashboard/welcome screen - suppresses panel drawing
  static constexpr const char* kDashboardCategory = "Dashboard";

  // ============================================================================
  // Session Lifecycle Management
  // ============================================================================

  void RegisterSession(size_t session_id);
  void UnregisterSession(size_t session_id);
  void SetActiveSession(size_t session_id);

  // ============================================================================
  // Panel Registration
  // ============================================================================

  void RegisterPanel(size_t session_id, const PanelDescriptor& base_info);

  void RegisterPanel(size_t session_id, const std::string& card_id,
                    const std::string& display_name, const std::string& icon,
                    const std::string& category,
                    const std::string& shortcut_hint = "", int priority = 50,
                    std::function<void()> on_show = nullptr,
                    std::function<void()> on_hide = nullptr,
                    bool visible_by_default = false);

  void UnregisterPanel(size_t session_id, const std::string& base_card_id);
  void UnregisterPanelsWithPrefix(const std::string& prefix);
  void ClearAllPanels();

  // ============================================================================
  // EditorPanel Instance Management (Phase 4)
  // ============================================================================

  /**
   * @brief Register a ContentRegistry-managed EditorPanel instance
   * @param panel The panel to register (ownership transferred)
   *
   * Registry panels are stored without auto-registering descriptors. Call
   * RegisterRegistryPanelsForSession() to add descriptors per session.
   */
  void RegisterRegistryPanel(std::unique_ptr<EditorPanel> panel);

  /**
   * @brief Register descriptors for all registry panels in a session
   * @param session_id The session to register descriptors for
   *
   * Safe to call multiple times; descriptors are only created once.
   */
  void RegisterRegistryPanelsForSession(size_t session_id);

  /**
   * @brief Register an EditorPanel instance for central drawing
   * @param panel The panel to register (ownership transferred)
   *
   * This method:
   * 1. Stores the EditorPanel instance
   * 2. Auto-registers a PanelDescriptor for sidebar/menu visibility
   * 3. Panel will be drawn by DrawAllVisiblePanels()
   */
  void RegisterEditorPanel(std::unique_ptr<EditorPanel> panel);

  /**
   * @brief Unregister and destroy an EditorPanel instance
   * @param panel_id The panel ID to unregister
   */
  void UnregisterEditorPanel(const std::string& panel_id);

  /**
   * @brief Get an EditorPanel instance by ID
   * @param panel_id The panel ID
   * @return Pointer to panel, or nullptr if not found
   */
  EditorPanel* GetEditorPanel(const std::string& panel_id);

  /**
   * @brief Draw all visible EditorPanel instances (central drawing)
   *
   * Call this once per frame to draw all panels that have EditorPanel
   * implementations. Panels without EditorPanel instances are skipped
   * (they use manual drawing).
   */
  void DrawAllVisiblePanels();

  /**
   * @brief Handle editor/category switching for panel visibility
   * @param from_category The category being switched away from
   * @param to_category The category being switched to
   *
   * This method:
   * 1. Hides non-pinned, non-persistent panels from the previous category
   * 2. Shows default panels for the new category
   * 3. Updates the active category
   */
  void OnEditorSwitch(const std::string& from_category,
                      const std::string& to_category);

  // ============================================================================
  // Panel Control (Programmatic)
  // ============================================================================

  bool ShowPanel(size_t session_id, const std::string& base_card_id);
  bool HidePanel(size_t session_id, const std::string& base_card_id);
  bool TogglePanel(size_t session_id, const std::string& base_card_id);
  bool IsPanelVisible(size_t session_id, const std::string& base_card_id) const;
  bool* GetVisibilityFlag(size_t session_id, const std::string& base_card_id);

  // ============================================================================
  // Batch Operations
  // ============================================================================

  void ShowAllPanelsInSession(size_t session_id);
  void HideAllPanelsInSession(size_t session_id);
  void ShowAllPanelsInCategory(size_t session_id, const std::string& category);
  void HideAllPanelsInCategory(size_t session_id, const std::string& category);
  void ShowOnlyPanel(size_t session_id, const std::string& base_card_id);

  // ============================================================================
  // Query Methods
  // ============================================================================

  std::vector<std::string> GetPanelsInSession(size_t session_id) const;
  std::vector<PanelDescriptor> GetPanelsInCategory(size_t session_id,
                                           const std::string& category) const;
  std::vector<std::string> GetAllCategories(size_t session_id) const;
  const PanelDescriptor* GetPanelDescriptor(size_t session_id,
                              const std::string& base_card_id) const;

  /**
   * @brief Get all panel descriptors (for layout designer, panel browser, etc.)
   * @return Map of panel_id -> PanelDescriptor
   */
  const std::unordered_map<std::string, PanelDescriptor>& GetAllPanelDescriptors() const {
    return cards_;
  }

  std::vector<std::string> GetAllCategories() const;

  static constexpr float GetSidebarWidth() { return 48.0f; }
  static constexpr float GetSidePanelWidth() { return 250.0f; }
  static float GetSidePanelWidthForViewport(float viewport_width) {
    float width = GetSidePanelWidth();
    const float max_width = viewport_width * 0.28f;
    if (viewport_width > 0.0f && max_width > 0.0f && width > max_width) {
      width = max_width;
    }
    return width;
  }
  static constexpr float GetCollapsedSidebarWidth() { return 16.0f; }

  static std::string GetCategoryIcon(const std::string& category);

  /**
   * @brief Get the expressive theme color for a category
   * @param category The category name
   * @return ImVec4 color for the category (used for active icon/glow effects)
   */
  struct CategoryTheme {
    float r, g, b, a;  // Icon color when active
    float glow_r, glow_g, glow_b;  // Glow/accent color (same hue)
  };
  static CategoryTheme GetCategoryTheme(const std::string& category);

  /**
   * @brief Handle keyboard navigation in sidebar (click-to-focus modal)
   */
  void HandleSidebarKeyboardNav(size_t session_id,
                                const std::vector<PanelDescriptor>& cards);

  bool SidebarHasFocus() const { return sidebar_has_focus_; }
  int GetFocusedPanelIndex() const { return focused_card_index_; }

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
  void TriggerShowPanelBrowser() { if (on_show_panel_browser_) on_show_panel_browser_(); }
  void TriggerSaveRom() { if (on_save_rom_) on_save_rom_(); }
  void TriggerUndo() { if (on_undo_) on_undo_(); }
  void TriggerRedo() { if (on_redo_) on_redo_(); }
  void TriggerShowSearch() { if (on_show_search_) on_show_search_(); }
  void TriggerShowShortcuts() { if (on_show_shortcuts_) on_show_shortcuts_(); }
  void TriggerShowCommandPalette() { if (on_show_command_palette_) on_show_command_palette_(); }
  void TriggerShowHelp() { if (on_show_help_) on_show_help_(); }
  void TriggerOpenRom() { if (on_open_rom_) on_open_rom_(); }
  void TriggerPanelClicked(const std::string& category) { if (on_card_clicked_) on_card_clicked_(category); }
  void TriggerCategorySelected(const std::string& category) { if (on_category_selected_) on_category_selected_(category); }

  // ============================================================================
  // Utility Icon Callbacks (for sidebar quick access buttons)
  // ============================================================================

  void SetShowEmulatorCallback(std::function<void()> cb) {
    on_show_emulator_ = std::move(cb);
  }
  void SetShowSettingsCallback(std::function<void()> cb) {
    on_show_settings_ = std::move(cb);
  }
  void SetShowPanelBrowserCallback(std::function<void()> cb) {
    on_show_panel_browser_ = std::move(cb);
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
  void SetShowShortcutsCallback(std::function<void()> cb) {
    on_show_shortcuts_ = std::move(cb);
  }
  void SetShowCommandPaletteCallback(std::function<void()> cb) {
    on_show_command_palette_ = std::move(cb);
  }
  void SetShowHelpCallback(std::function<void()> cb) {
    on_show_help_ = std::move(cb);
  }
  void SetOpenRomCallback(std::function<void()> cb) {
    on_open_rom_ = std::move(cb);
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
    std::vector<std::string> visible_cards;  // Panel IDs
    std::string description;
  };

  void SavePreset(const std::string& name, const std::string& description = "");
  bool LoadPreset(const std::string& name);
  void DeletePreset(const std::string& name);
  std::vector<WorkspacePreset> GetPresets() const;

  // ============================================================================
  // Panel Validation (for catching window title mismatches)
  // ============================================================================

  struct PanelValidationResult {
    std::string card_id;
    std::string expected_title;  // From PanelDescriptor::GetWindowTitle()
    bool found_in_imgui;         // Whether ImGui found a window with this title
    std::string message;         // Human-readable status
  };

  std::vector<PanelValidationResult> ValidatePanels() const;
  PanelValidationResult ValidatePanel(const std::string& card_id) const;

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

  size_t GetPanelCount() const { return cards_.size(); }
  size_t GetVisiblePanelCount(size_t session_id) const;
  size_t GetSessionCount() const { return session_count_; }

  // ============================================================================
  // Session Prefixing Utilities
  // ============================================================================

  std::string MakePanelId(size_t session_id, const std::string& base_id) const;
  std::string MakePanelId(size_t session_id, const std::string& base_id,
                          PanelScope scope) const;
  bool ShouldPrefixPanels() const { return session_count_ > 1; }

  // ============================================================================
  // Convenience Methods (for EditorManager direct usage without session_id)
  // ============================================================================

  void RegisterPanel(const PanelDescriptor& base_info) {
    RegisterPanel(active_session_, base_info);
  }
  void UnregisterPanel(const std::string& base_card_id) {
    UnregisterPanel(active_session_, base_card_id);
  }
  bool ShowPanel(const std::string& base_card_id) {
    return ShowPanel(active_session_, base_card_id);
  }
  bool HidePanel(const std::string& base_card_id) {
    return HidePanel(active_session_, base_card_id);
  }
  bool IsPanelVisible(const std::string& base_card_id) const {
    return IsPanelVisible(active_session_, base_card_id);
  }
  void HideAllPanelsInCategory(const std::string& category) {
    HideAllPanelsInCategory(active_session_, category);
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
  void ShowAllPanelsInCategory(const std::string& category) {
    ShowAllPanelsInCategory(active_session_, category);
  }
  bool* GetVisibilityFlag(const std::string& base_card_id) {
    return GetVisibilityFlag(active_session_, base_card_id);
  }
  void ShowAll() { ShowAll(active_session_); }
  void HideAll() { HideAll(active_session_); }
  void SetOnPanelClickedCallback(std::function<void(const std::string&)> callback) {
    on_card_clicked_ = std::move(callback);
  }
  void SetOnCategorySelectedCallback(std::function<void(const std::string&)> callback) {
    on_category_selected_ = std::move(callback);
  }

  size_t GetActiveSessionId() const { return active_session_; }

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
  const std::vector<std::string>& GetRecentPanels() const { return recent_cards_; }
  const std::unordered_set<std::string>& GetFavoritePanels() const { return favorite_cards_; }

  // ============================================================================
  // Pinning (Phase 3 scaffold)
  // ============================================================================

  void SetPanelPinned(size_t session_id, const std::string& base_card_id, bool pinned);
  bool IsPanelPinned(size_t session_id, const std::string& base_card_id) const;
  std::vector<std::string> GetPinnedPanels(size_t session_id) const;

  void SetPanelPinned(const std::string& base_card_id, bool pinned);
  bool IsPanelPinned(const std::string& base_card_id) const;
  std::vector<std::string> GetPinnedPanels() const;

  // ============================================================================
  // Resource Management (Phase 6)
  // ============================================================================

  /**
   * @brief Enforce limits on resource panels (LRU eviction)
   * @param resource_type The type of resource (e.g., "room", "song")
   *
   * Checks if the number of open panels of this type exceeds the limit.
   * If so, closes and unregisters the least recently used panel.
   */
  void EnforceResourceLimits(const std::string& resource_type);

  /**
   * @brief Mark a panel as recently used (for LRU)
   * @param panel_id The panel ID
   */
  void MarkPanelUsed(const std::string& panel_id);

 private:
  // ... existing private members ...

  // Resource panel tracking: type -> list of panel_ids (front = LRU, back = MRU)
  std::unordered_map<std::string, std::list<std::string>> resource_panels_;
  
  // Map panel_id -> resource_type for quick lookups
  std::unordered_map<std::string, std::string> panel_resource_types_;

  // ... existing private members ...
  // Core card storage (prefixed IDs → PanelDescriptor)
  std::unordered_map<std::string, PanelDescriptor> cards_;

  // EditorPanel instance storage (panel_id → EditorPanel)
  // Panels with instances are drawn by DrawAllVisiblePanels()
  std::unordered_map<std::string, std::unique_ptr<EditorPanel>> panel_instances_;
  std::unordered_set<std::string> registry_panel_ids_;
  std::unordered_set<std::string> global_panel_ids_;

  // Favorites and Recent tracking
  std::unordered_set<std::string> favorite_cards_;
  std::vector<std::string> recent_cards_;
  static constexpr size_t kMaxRecentPanels = 10;

  // Centralized visibility flags for cards without external flags
  std::unordered_map<std::string, bool> centralized_visibility_;
  // Pinned state tracking (prefixed ID → pinned)
  std::unordered_map<std::string, bool> pinned_panels_;

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
  bool sidebar_visible_ = false;    // Controls Activity Bar visibility (0px vs 48px)
  bool panel_expanded_ = false;     // Controls Side Panel visibility (0px vs 250px) - starts collapsed

  // Keyboard navigation state (click-to-focus modal)
  int focused_card_index_ = -1;    // Currently focused card index (-1 = none)
  bool sidebar_has_focus_ = false; // Whether sidebar has keyboard focus

  // Unified visibility state (single source of truth)
  bool emulator_visible_ = false;  // Emulator window visibility

  // Utility icon callbacks
  std::function<void()> on_show_emulator_;
  std::function<void()> on_show_settings_;
  std::function<void()> on_show_panel_browser_;
  std::function<void()> on_save_rom_;
  std::function<void()> on_undo_;
  std::function<void()> on_redo_;
  std::function<void()> on_show_search_;
  std::function<void()> on_show_shortcuts_;
  std::function<void()> on_show_command_palette_;
  std::function<void()> on_show_help_;
  std::function<void()> on_open_rom_;

  // State change callbacks
  std::function<void(bool visible, bool expanded)> on_sidebar_state_changed_;
  std::function<void(const std::string&)> on_category_changed_;
  std::function<void(const std::string&)> on_card_clicked_;
  std::function<void(const std::string&)> on_category_selected_;  // Activity Bar icon clicked
  std::function<void(bool)> on_emulator_visibility_changed_;
  std::function<void(const std::string&, const std::string&)> on_file_clicked_;

  // File browser for categories that support it (e.g., Assembly)
  std::unordered_map<std::string, std::unique_ptr<FileBrowser>>
      category_file_browsers_;

  // Tracking active editor categories for visual feedback
  std::unordered_set<std::string> active_editor_categories_;

  // Helper methods
  void UpdateSessionCount();
  std::string GetPrefixedPanelId(size_t session_id,
                                const std::string& base_id) const;
  void RegisterPanelDescriptorForSession(size_t session_id,
                                         const EditorPanel& panel);
  void TrackPanelForSession(size_t session_id, const std::string& base_id,
                            const std::string& panel_id);
  void UnregisterSessionPanels(size_t session_id);
  void SavePresetsToFile();
  void LoadPresetsFromFile();
};

}  // namespace editor
}  // namespace yaze

#undef YAZE_CARD_SHIM_DEPRECATED

#endif  // APP_EDITOR_SYSTEM_PANEL_MANAGER_H_
