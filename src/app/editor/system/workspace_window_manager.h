#ifndef YAZE_APP_EDITOR_SYSTEM_WORKSPACE_WINDOW_MANAGER_H_
#define YAZE_APP_EDITOR_SYSTEM_WORKSPACE_WINDOW_MANAGER_H_

#include <algorithm>
#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "app/editor/core/event_bus.h"
#include "app/editor/events/core_events.h"
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
class Editor;
class ResourceWindowContent;

/**
 * @struct WindowDescriptor
 * @brief Metadata for a dockable editor window (formerly PanelInfo)
 */
struct WindowDescriptor {
  std::string card_id;  // Unique identifier (e.g., "dungeon.room_selector")
  std::string display_name;  // Human-readable name (e.g., "Room Selector")
  std::string
      window_title;  // ImGui window title for DockBuilder (e.g., " Rooms List")
  std::string icon;  // Material icon
  std::string category;  // Category (e.g., "Dungeon", "Graphics", "Palette")
  std::string workflow_group;
  std::string workflow_label;
  std::string workflow_description;
  int workflow_priority = 50;

  // Lifecycle behavior on editor switches (default: EditorBound).
  WindowLifecycle window_lifecycle = WindowLifecycle::EditorBound;

  // Optional context binding (default: none).
  WindowContextScope context_scope = WindowContextScope::kNone;

  WindowScope scope = WindowScope::kSession;
  enum class ShortcutScope {
    kGlobal,  // Available regardless of active editor
    kEditor,  // Only active within the owning editor
    kPanel    // Panel visibility/within-panel actions
  };
  std::string shortcut_hint;  // Display hint (e.g., "Ctrl+Shift+R")
  ShortcutScope shortcut_scope = ShortcutScope::kPanel;
  bool* visibility_flag;          // Pointer to bool controlling visibility
  std::function<void()> on_show;  // Callback when card is shown
  std::function<void()> on_hide;  // Callback when card is hidden
  int priority;                   // Display priority for menus (lower = higher)

  // Disabled state support for IDE-like behavior
  std::function<bool()>
      enabled_condition;  // Returns true if card is enabled (nullptr = always enabled)
  std::string disabled_tooltip;  // Tooltip shown when hovering disabled card

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

  /**
   * @brief Build the exact ImGui window name used by PanelWindow::Begin
   *
   * Uses the display label plus a stable ID suffix to keep DockBuilder and
   * runtime window identity aligned.
   */
  std::string GetImGuiWindowName() const {
    const std::string label =
        icon.empty() ? display_name : (icon + " " + display_name);
    if (!card_id.empty()) {
      return label + "##" + card_id;
    }
    return label;
  }
};

/**
 * @class WorkspaceWindowManager
 * @brief Central registry for all editor cards with session awareness and
 * dependency injection
 */
class WorkspaceWindowManager {
 public:
  WorkspaceWindowManager() = default;
  ~WorkspaceWindowManager() = default;

  // Non-copyable, non-movable
  WorkspaceWindowManager(const WorkspaceWindowManager&) = delete;
  WorkspaceWindowManager& operator=(const WorkspaceWindowManager&) = delete;
  WorkspaceWindowManager(WorkspaceWindowManager&&) = delete;
  WorkspaceWindowManager& operator=(WorkspaceWindowManager&&) = delete;

  // Special category for dashboard/welcome screen - suppresses panel drawing
  static constexpr const char* kDashboardCategory = "Dashboard";

  // ============================================================================
  // Session Lifecycle Management
  // ============================================================================

  void RegisterSession(size_t session_id);
  void UnregisterSession(size_t session_id);
  void SetActiveSession(size_t session_id);

  // ============================================================================
  // Context Keys (Optional Policy Engine)
  // ============================================================================

  /**
   * @brief Set a string key for a given context scope (room/selection/etc)
   *
   * This is an opt-in policy hook. WorkspaceWindowManager can apply default
   * rules when
   * context becomes invalid (e.g., selection cleared) to avoid stale panels.
   */
  void SetContextKey(size_t session_id, WindowContextScope scope,
                     std::string key);
  std::string GetContextKey(size_t session_id, WindowContextScope scope) const;

  // ============================================================================
  // Panel Registration
  // ============================================================================

  void RegisterPanel(size_t session_id, const WindowDescriptor& base_info);
  void RegisterWindow(size_t session_id, const WindowDescriptor& descriptor) {
    RegisterPanel(session_id, descriptor);
  }

  /**
   * @brief Register a legacy panel ID alias that resolves to a canonical ID.
   *
   * Use this when a panel has been renamed but persisted layout/user settings
   * may still reference the old ID.
   */
  void RegisterPanelAlias(const std::string& legacy_base_id,
                          const std::string& canonical_base_id);
  void RegisterWindowAlias(const std::string& legacy_base_id,
                           const std::string& canonical_base_id) {
    RegisterPanelAlias(legacy_base_id, canonical_base_id);
  }

  /**
   * @brief Resolve a panel ID through the alias table.
   * @return Canonical panel ID if alias exists, otherwise input ID.
   */
  std::string ResolvePanelAlias(const std::string& panel_id) const;
  std::string ResolveWindowAlias(const std::string& window_id) const {
    return ResolvePanelAlias(window_id);
  }

  void RegisterPanel(size_t session_id, const std::string& card_id,
                     const std::string& display_name, const std::string& icon,
                     const std::string& category,
                     const std::string& shortcut_hint = "", int priority = 50,
                     std::function<void()> on_show = nullptr,
                     std::function<void()> on_hide = nullptr,
                     bool visible_by_default = false);

  void RegisterWindow(size_t session_id, const std::string& window_id,
                      const std::string& display_name, const std::string& icon,
                      const std::string& category,
                      const std::string& shortcut_hint = "", int priority = 50,
                      std::function<void()> on_show = nullptr,
                      std::function<void()> on_hide = nullptr,
                      bool visible_by_default = false) {
    RegisterPanel(session_id, window_id, display_name, icon, category,
                  shortcut_hint, priority, std::move(on_show),
                  std::move(on_hide), visible_by_default);
  }

  void UnregisterPanel(size_t session_id, const std::string& base_card_id);
  void UnregisterWindow(size_t session_id, const std::string& base_window_id) {
    UnregisterPanel(session_id, base_window_id);
  }
  void UnregisterPanelsWithPrefix(const std::string& prefix);
  void ClearAllPanels();
  void ClearAllWindows() { ClearAllPanels(); }

  // ============================================================================
  // WindowContent Instance Management (Phase 4)
  // ============================================================================

  /**
   * @brief Register a ContentRegistry-managed WindowContent instance
   * @param window The window content to register (ownership transferred)
   *
   * Registry window contents are stored without auto-registering descriptors.
   * Call RegisterRegistryWindowContentsForSession() to add descriptors per
   * session.
   */
  void RegisterRegistryWindowContent(std::unique_ptr<WindowContent> window);
  void RegisterRegistryWindow(std::unique_ptr<WindowContent> window) {
    RegisterRegistryWindowContent(std::move(window));
  }

  /**
   * @brief Register descriptors for all registry window contents in a session
   * @param session_id The session to register descriptors for
   *
   * Safe to call multiple times; descriptors are only created once.
   */
  void RegisterRegistryWindowContentsForSession(size_t session_id);
  void RegisterRegistryWindowsForSession(size_t session_id) {
    RegisterRegistryWindowContentsForSession(session_id);
  }

  /// @brief Returns the number of window contents registered via ContentRegistry
  size_t GetRegistryWindowContentCount() const {
    return registry_state_.registry_ids.size();
  }
  size_t GetRegistryWindowCount() const {
    return GetRegistryWindowContentCount();
  }

  /**
   * @brief Register a WindowContent instance for central drawing
   * @param window The window content to register (ownership transferred)
   *
   * This method:
   * 1. Stores the WindowContent instance
   * 2. Auto-registers a WindowDescriptor for sidebar/menu visibility
   * 3. Window content will be drawn by DrawAllVisiblePanels()
   */
  void RegisterWindowContent(std::unique_ptr<WindowContent> window);

  /**
   * @brief Unregister and destroy a WindowContent instance
   * @param window_id The window ID to unregister
   */
  void UnregisterWindowContent(const std::string& window_id);

  /**
   * @brief Get a WindowContent instance by ID
   * @param window_id The window ID
   * @return Pointer to window content, or nullptr if not found
   */
  WindowContent* GetWindowContent(const std::string& window_id);

  /**
   * @brief Draw all visible WindowContent instances (central drawing)
   *
   * Call this once per frame to draw all windows that have WindowContent
   * implementations. Windows without WindowContent instances are skipped
   * (they use manual drawing).
   */
  void DrawAllVisiblePanels();
  void DrawVisibleWindows() { DrawAllVisiblePanels(); }

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

  bool OpenWindow(size_t session_id, const std::string& base_window_id) {
    return OpenWindowImpl(session_id, base_window_id);
  }
  bool CloseWindow(size_t session_id, const std::string& base_window_id) {
    return CloseWindowImpl(session_id, base_window_id);
  }
  bool ToggleWindow(size_t session_id, const std::string& base_window_id) {
    return ToggleWindowImpl(session_id, base_window_id);
  }
  bool IsWindowOpen(size_t session_id,
                    const std::string& base_window_id) const {
    return IsWindowVisibleImpl(session_id, base_window_id);
  }
  bool* GetWindowVisibilityFlag(size_t session_id,
                                const std::string& base_window_id) {
    return GetVisibilityFlag(session_id, base_window_id);
  }

  // ============================================================================
  // Batch Operations
  // ============================================================================

  void ShowAllWindowsInSession(size_t session_id);
  void HideAllWindowsInSession(size_t session_id);
  void ShowAllWindowsInCategory(size_t session_id, const std::string& category);
  void HideAllWindowsInCategory(size_t session_id, const std::string& category);
  void ShowOnlyWindow(size_t session_id, const std::string& base_window_id);

  YAZE_CARD_SHIM_DEPRECATED("Use ShowAllWindowsInSession() instead.")
  void ShowAllPanelsInSession(size_t session_id) {
    ShowAllWindowsInSession(session_id);
  }
  YAZE_CARD_SHIM_DEPRECATED("Use HideAllWindowsInSession() instead.")
  void HideAllPanelsInSession(size_t session_id) {
    HideAllWindowsInSession(session_id);
  }
  YAZE_CARD_SHIM_DEPRECATED("Use ShowAllWindowsInCategory() instead.")
  void ShowAllPanelsInCategory(size_t session_id, const std::string& category) {
    ShowAllWindowsInCategory(session_id, category);
  }
  YAZE_CARD_SHIM_DEPRECATED("Use HideAllWindowsInCategory() instead.")
  void HideAllPanelsInCategory(size_t session_id, const std::string& category) {
    HideAllWindowsInCategory(session_id, category);
  }
  YAZE_CARD_SHIM_DEPRECATED("Use ShowOnlyWindow() instead.")
  void ShowOnlyPanel(size_t session_id, const std::string& base_card_id) {
    ShowOnlyWindow(session_id, base_card_id);
  }

  // ============================================================================
  // Query Methods
  // ============================================================================

  std::vector<std::string> GetWindowsInSession(size_t session_id) const {
    return GetWindowsInSessionImpl(session_id);
  }
  std::vector<WindowDescriptor> GetWindowsInCategory(
      size_t session_id, const std::string& category) const {
    return GetWindowsInCategoryImpl(session_id, category);
  }
  std::vector<std::string> GetAllCategories(size_t session_id) const;
  std::vector<std::string> GetAllWindowCategories(size_t session_id) const {
    return GetAllCategories(session_id);
  }
  const WindowDescriptor* GetWindowDescriptor(
      size_t session_id, const std::string& base_window_id) const {
    return GetWindowDescriptorImpl(session_id, base_window_id);
  }

  /**
   * @brief Get all panel descriptors (for layout designer, panel browser, etc.)
   * @return Map of panel_id -> WindowDescriptor
   */
  const std::unordered_map<std::string, WindowDescriptor>&
  GetAllWindowDescriptors() const {
    return registry_state_.descriptors;
  }

  std::vector<std::string> GetAllCategories() const;
  std::vector<std::string> GetAllWindowCategories() const {
    return GetAllCategories();
  }

  // ============================================================================
  // State Persistence
  // ============================================================================

  /**
   * @brief Get list of currently visible panel IDs for a session
   * @param session_id The session to query
   * @return Vector of base panel IDs that are currently visible
   */
  std::vector<std::string> GetVisibleWindowIds(size_t session_id) const {
    return GetVisibleWindowIdsImpl(session_id);
  }

  /**
   * @brief Set which panels should be visible for a session
   * @param session_id The session to modify
   * @param panel_ids Vector of base panel IDs to make visible (others hidden)
   */
  void SetVisibleWindows(size_t session_id,
                         const std::vector<std::string>& window_ids) {
    SetVisibleWindowsImpl(session_id, window_ids);
  }

  /**
   * @brief Serialize panel visibility state for persistence
   * @param session_id The session to serialize
   * @return Map of base_panel_id -> visible (for serialization)
   */
  std::unordered_map<std::string, bool> SerializeVisibilityState(
      size_t session_id) const;

  /**
   * @brief Restore panel visibility state from persistence
   * @param session_id The session to restore
   * @param state Map of base_panel_id -> visible
   */
  void RestoreVisibilityState(
      size_t session_id, const std::unordered_map<std::string, bool>& state,
      bool publish_events = false);

  /**
   * @brief Serialize pinned panel state for persistence
   * @return Map of base_panel_id -> pinned
   */
  std::unordered_map<std::string, bool> SerializePinnedState() const;

  /**
   * @brief Restore pinned panel state from persistence
   * @param state Map of base_panel_id -> pinned
   */
  void RestorePinnedState(const std::unordered_map<std::string, bool>& state);

  /**
   * @brief Resolve the exact ImGui window name for a panel by base ID
   * @return Label + stable ID (e.g., "📋 Properties##dungeon.properties")
   */
  std::string GetWorkspaceWindowName(size_t session_id,
                                     const std::string& base_window_id) const {
    return GetWindowNameImpl(session_id, base_window_id);
  }
  std::string GetWorkspaceWindowName(const WindowDescriptor& descriptor) const {
    return GetWindowNameImpl(descriptor);
  }

  static constexpr float GetSidebarWidth() { return 48.0f; }
  static constexpr float GetSidePanelWidth() { return 300.0f; }
  struct SidePanelWidthBounds {
    float min_width;
    float max_width;
  };
  static SidePanelWidthBounds GetSidePanelWidthBounds(float viewport_width);
  static float GetSidePanelWidthForViewport(float viewport_width) {
    if (viewport_width <= 0.0f) {
      return GetSidePanelWidth();
    }

    // Keep side panel useful on compact/touch widths while preserving
    // desktop defaults on large displays.
    if (viewport_width < 900.0f) {
      const float preferred = viewport_width * 0.50f;
      const float min_width = std::max(220.0f, viewport_width * 0.40f);
      const float max_width =
          std::max(min_width, std::min(380.0f, viewport_width * 0.58f));
      return std::clamp(preferred, min_width, max_width);
    }

    if (viewport_width < 1200.0f) {
      const float preferred = viewport_width * 0.34f;
      const float min_width = 300.0f;
      const float max_width = std::min(420.0f, viewport_width * 0.42f);
      return std::clamp(preferred, min_width, max_width);
    }

    // iPad Pro landscape sits in this range; avoid falling back to a narrow
    // desktop sidebar width that feels cramped for touch navigation.
    if (viewport_width < 1400.0f) {
      const float preferred = viewport_width * 0.25f;
      const float min_width = 300.0f;
      const float max_width = 380.0f;
      return std::clamp(preferred, min_width, max_width);
    }

    float width = GetSidePanelWidth();
    const float max_width = viewport_width * 0.30f;
    if (max_width > 0.0f && width > max_width) {
      width = max_width;
    }
    return width;
  }
  float GetActiveSidePanelWidth(float viewport_width) const;
  void SetActiveSidePanelWidth(float width, float viewport_width = 0.0f,
                               bool notify = true);
  void ResetSidePanelWidth(bool notify = true);
  float GetStoredSidePanelWidth() const { return browser_state_.sidebar_width; }
  void SetStoredSidePanelWidth(float width, bool notify = false) {
    SetActiveSidePanelWidth(width, 0.0f, notify);
  }
  void SetSidePanelWidthChangedCallback(std::function<void(float)> cb) {
    browser_state_.on_sidebar_width_changed = std::move(cb);
  }

  YAZE_CARD_SHIM_DEPRECATED("Use GetWindowBrowserCategoryWidth() instead.")
  float GetPanelBrowserCategoryWidth() const {
    return browser_state_.window_browser_category_width;
  }
  float GetWindowBrowserCategoryWidth() const {
    return browser_state_.window_browser_category_width;
  }
  YAZE_CARD_SHIM_DEPRECATED("Use SetWindowBrowserCategoryWidth() instead.")
  void SetPanelBrowserCategoryWidth(float width, bool notify = true);
  void SetWindowBrowserCategoryWidth(float width, bool notify = true) {
    SetPanelBrowserCategoryWidth(width, notify);
  }
  void SetPanelBrowserCategoryWidthChangedCallback(
      std::function<void(float)> cb) {
    browser_state_.on_window_browser_width_changed = std::move(cb);
  }
  void SetWindowBrowserCategoryWidthChangedCallback(
      std::function<void(float)> cb) {
    SetPanelBrowserCategoryWidthChangedCallback(std::move(cb));
  }
  YAZE_CARD_SHIM_DEPRECATED(
      "Use GetDefaultWindowBrowserCategoryWidth() instead.")
  static constexpr float GetDefaultPanelBrowserCategoryWidth() {
    return 260.0f;
  }
  static constexpr float GetDefaultWindowBrowserCategoryWidth() {
    return 260.0f;
  }

  static constexpr float GetCollapsedSidebarWidth() { return 16.0f; }

  static std::string GetCategoryIcon(const std::string& category);

  /**
   * @brief Get the expressive theme color for a category
   * @param category The category name
   * @return ImVec4 color for the category (used for active icon/glow effects)
   */
  struct CategoryTheme {
    float r, g, b, a;              // Icon color when active
    float glow_r, glow_g, glow_b;  // Glow/accent color (same hue)
  };
  static CategoryTheme GetCategoryTheme(const std::string& category);

  /**
   * @brief Handle keyboard navigation in sidebar (click-to-focus modal)
   */
  void HandleSidebarKeyboardNav(size_t session_id,
                                const std::vector<WindowDescriptor>& cards);

  bool SidebarHasFocus() const { return browser_state_.sidebar_has_focus; }
  int GetFocusedPanelIndex() const {
    return browser_state_.focused_window_index;
  }
  int GetFocusedWindowIndex() const { return GetFocusedPanelIndex(); }

  void ToggleSidebarVisibility() {
    browser_state_.sidebar_visible = !browser_state_.sidebar_visible;
    if (browser_state_.on_sidebar_state_changed) {
      browser_state_.on_sidebar_state_changed(browser_state_.sidebar_visible,
                                              browser_state_.sidebar_expanded);
    }
  }

  void SetSidebarVisible(bool visible, bool notify = true) {
    if (browser_state_.sidebar_visible != visible) {
      browser_state_.sidebar_visible = visible;
      if (notify && browser_state_.on_sidebar_state_changed) {
        browser_state_.on_sidebar_state_changed(
            browser_state_.sidebar_visible, browser_state_.sidebar_expanded);
      }
    }
  }

  bool IsSidebarVisible() const { return browser_state_.sidebar_visible; }

  YAZE_CARD_SHIM_DEPRECATED("Use ToggleSidebarExpanded() instead.")
  void ToggleWindowImplExpanded() { ToggleSidebarExpanded(); }

  void ToggleSidebarExpanded() {
    browser_state_.sidebar_expanded = !browser_state_.sidebar_expanded;
    if (browser_state_.on_sidebar_state_changed) {
      browser_state_.on_sidebar_state_changed(browser_state_.sidebar_visible,
                                              browser_state_.sidebar_expanded);
    }
  }

  YAZE_CARD_SHIM_DEPRECATED("Use SetSidebarExpanded() instead.")
  void SetPanelExpanded(bool expanded, bool notify = true) {
    SetSidebarExpanded(expanded, notify);
  }

  void SetSidebarExpanded(bool expanded, bool notify = true) {
    if (browser_state_.sidebar_expanded != expanded) {
      browser_state_.sidebar_expanded = expanded;
      if (notify && browser_state_.on_sidebar_state_changed) {
        browser_state_.on_sidebar_state_changed(
            browser_state_.sidebar_visible, browser_state_.sidebar_expanded);
      }
    }
  }

  YAZE_CARD_SHIM_DEPRECATED("Use IsSidebarExpanded() instead.")
  bool IsPanelExpanded() const { return IsSidebarExpanded(); }
  bool IsSidebarExpanded() const { return browser_state_.sidebar_expanded; }

  // ============================================================================
  // EventBus Integration
  // ============================================================================

  void SetEventBus(EventBus* event_bus) { event_bus_ = event_bus; }

  // ============================================================================
  // Triggers (exposed for ActivityBar) - prefer EventBus, fallback to callbacks
  // ============================================================================

  void TriggerShowEmulator() {
    if (event_bus_) {
      event_bus_->Publish(UIActionRequestEvent::ShowEmulator());
    }
  }
  void TriggerShowSettings() {
    if (event_bus_) {
      event_bus_->Publish(UIActionRequestEvent::ShowSettings());
    }
  }
  YAZE_CARD_SHIM_DEPRECATED("Use TriggerShowWindowBrowser() instead.")
  void TriggerShowPanelBrowser() { TriggerShowWindowBrowser(); }

  void TriggerShowWindowBrowser() {
    if (event_bus_) {
      event_bus_->Publish(UIActionRequestEvent::Create(
          UIActionRequestEvent::Action::kShowPanelBrowser));
    }
  }
  void TriggerSaveRom() {
    if (event_bus_) {
      event_bus_->Publish(UIActionRequestEvent::SaveRom());
    }
  }
  void TriggerUndo() {
    if (event_bus_) {
      event_bus_->Publish(UIActionRequestEvent::Undo());
    }
  }
  void TriggerRedo() {
    if (event_bus_) {
      event_bus_->Publish(UIActionRequestEvent::Redo());
    }
  }
  void TriggerShowSearch() {
    if (event_bus_) {
      event_bus_->Publish(UIActionRequestEvent::Create(
          UIActionRequestEvent::Action::kShowSearch));
    }
  }
  void TriggerShowShortcuts() {
    if (event_bus_) {
      event_bus_->Publish(UIActionRequestEvent::Create(
          UIActionRequestEvent::Action::kShowShortcuts));
    }
  }
  void TriggerShowCommandPalette() {
    if (event_bus_) {
      event_bus_->Publish(UIActionRequestEvent::ShowCommandPalette());
    }
  }
  void TriggerShowHelp() {
    if (event_bus_) {
      event_bus_->Publish(UIActionRequestEvent::Create(
          UIActionRequestEvent::Action::kShowHelp));
    }
  }
  void TriggerShowAgentChatSidebar() {
    if (event_bus_) {
      event_bus_->Publish(UIActionRequestEvent::ShowAgentChatSidebar());
    }
  }
  void TriggerShowAgentProposalsSidebar() {
    if (event_bus_) {
      event_bus_->Publish(UIActionRequestEvent::ShowAgentProposalsSidebar());
    }
  }
  void TriggerResetLayout() {
    if (event_bus_) {
      event_bus_->Publish(UIActionRequestEvent::Create(
          UIActionRequestEvent::Action::kResetLayout));
    }
  }
  void TriggerOpenRom() {
    if (event_bus_) {
      event_bus_->Publish(UIActionRequestEvent::OpenRom());
    }
  }
  YAZE_CARD_SHIM_DEPRECATED("Use TriggerWindowClicked() instead.")
  void TriggerPanelClicked(const std::string& category) {
    TriggerWindowClicked(category);
  }
  void TriggerWindowClicked(const std::string& category) {
    if (browser_state_.on_window_clicked)
      browser_state_.on_window_clicked(category);
  }
  void TriggerCategorySelected(const std::string& category) {
    if (browser_state_.on_category_selected) {
      browser_state_.on_category_selected(category);
    }
  }
  void TriggerWindowCategorySelected(const std::string& category) {
    TriggerCategorySelected(category);
  }

  // ============================================================================
  // Utility Icon Callbacks (DEPRECATED: subscribe to UIActionRequestEvent instead)
  // ============================================================================

  void SetSidebarStateChangedCallback(std::function<void(bool, bool)> cb) {
    browser_state_.on_sidebar_state_changed = std::move(cb);
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
    browser_state_.on_category_changed = std::move(cb);
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
  // Panel Validation (for catching ImGui window-name mismatches)
  // ============================================================================

  struct WindowValidationResult {
    std::string card_id;
    std::string expected_title;  // Exact ImGui window name for docking/focus
    bool found_in_imgui;         // Whether ImGui found a window with this title
    std::string message;         // Human-readable status
  };

  std::vector<WindowValidationResult> ValidateWindows() const;
  WindowValidationResult ValidateWindow(const std::string& card_id) const;

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

  YAZE_CARD_SHIM_DEPRECATED("Use GetWindowCount() instead.")
  size_t GetPanelCount() const { return registry_state_.descriptors.size(); }
  size_t GetWindowCount() const { return registry_state_.descriptors.size(); }
  size_t GetVisibleWindowCount(size_t session_id) const;
  YAZE_CARD_SHIM_DEPRECATED("Use GetVisibleWindowCount() instead.")
  size_t GetVisiblePanelCount(size_t session_id) const {
    return GetVisibleWindowCount(session_id);
  }
  size_t GetSessionCount() const { return session_state_.session_count; }

  // ============================================================================
  // Session Prefixing Utilities
  // ============================================================================

  std::string MakeWindowId(size_t session_id, const std::string& base_id) const;
  std::string MakeWindowId(size_t session_id, const std::string& base_id,
                           WindowScope scope) const;
  YAZE_CARD_SHIM_DEPRECATED("Use MakeWindowId() instead.")
  std::string MakePanelId(size_t session_id, const std::string& base_id) const {
    return MakeWindowId(session_id, base_id);
  }
  YAZE_CARD_SHIM_DEPRECATED("Use MakeWindowId() instead.")
  std::string MakePanelId(size_t session_id, const std::string& base_id,
                          WindowScope scope) const {
    return MakeWindowId(session_id, base_id, scope);
  }
  bool ShouldPrefixPanels() const { return session_count_ > 1; }

  // ============================================================================
  // Convenience Methods (for EditorManager direct usage without session_id)
  // ============================================================================

  void RegisterPanel(const WindowDescriptor& base_info) {
    RegisterPanel(active_session_, base_info);
  }
  void RegisterWindow(const WindowDescriptor& descriptor) {
    RegisterPanel(active_session_, descriptor);
  }
  void UnregisterPanel(const std::string& base_card_id) {
    UnregisterPanel(active_session_, base_card_id);
  }
  void UnregisterWindow(const std::string& base_window_id) {
    UnregisterPanel(active_session_, base_window_id);
  }
  bool OpenWindow(const std::string& base_window_id) {
    return OpenWindowImpl(active_session_, base_window_id);
  }
  bool CloseWindow(const std::string& base_window_id) {
    return CloseWindowImpl(active_session_, base_window_id);
  }
  bool IsWindowOpen(const std::string& base_window_id) const {
    return IsWindowVisibleImpl(active_session_, base_window_id);
  }
  void HideAllWindowsInCategory(const std::string& category) {
    HideAllWindowsInCategory(active_session_, category);
  }
  YAZE_CARD_SHIM_DEPRECATED("Use HideAllWindowsInCategory() instead.")
  void HideAllPanelsInCategory(const std::string& category) {
    HideAllWindowsInCategory(active_session_, category);
  }
  std::string GetActiveCategory() const {
    return browser_state_.active_category;
  }
  void SetActiveCategory(const std::string& category, bool notify = true) {
    if (browser_state_.active_category != category) {
      browser_state_.active_category = category;
      if (notify && browser_state_.on_category_changed) {
        browser_state_.on_category_changed(category);
      }
    }
  }
  void ShowAllWindowsInCategory(const std::string& category) {
    ShowAllWindowsInCategory(active_session_, category);
  }
  YAZE_CARD_SHIM_DEPRECATED("Use ShowAllWindowsInCategory() instead.")
  void ShowAllPanelsInCategory(const std::string& category) {
    ShowAllWindowsInCategory(active_session_, category);
  }
  bool* GetWindowVisibilityFlag(const std::string& base_window_id) {
    return GetVisibilityFlag(active_session_, base_window_id);
  }
  void ShowAll() { ShowAll(active_session_); }
  void HideAll() { HideAll(active_session_); }
  void SetOnWindowClickedCallback(
      std::function<void(const std::string&)> callback) {
    browser_state_.on_window_clicked = std::move(callback);
  }
  void SetOnWindowCategorySelectedCallback(
      std::function<void(const std::string&)> callback) {
    browser_state_.on_category_selected = std::move(callback);
  }
  void SetEditorResolver(std::function<Editor*(const std::string&)> resolver) {
    editor_resolver_ = std::move(resolver);
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
  // Favorites (delegates to Pinned) and Recent
  // ============================================================================

  // Favorites are now aliases for pinned panels
  void ToggleFavorite(const std::string& card_id) {
    SetWindowPinned(card_id, !IsWindowPinned(card_id));
  }
  bool IsFavorite(const std::string& card_id) const {
    return IsWindowPinned(card_id);
  }

  void MarkWindowRecentlyUsed(const std::string& window_id) {
    MarkWindowRecentlyUsedImpl(window_id);
  }

  std::vector<WindowDescriptor> GetWindowsSortedByMRU(
      size_t session_id, const std::string& category) const {
    return GetWindowsSortedByMRUImpl(session_id, category);
  }

  uint64_t GetWindowMRUTime(const std::string& window_id) const {
    return GetWindowMRUTimeImpl(window_id);
  }

  // ============================================================================
  // Pinning (Phase 3 scaffold)
  // ============================================================================

  void SetWindowPinned(size_t session_id, const std::string& base_window_id,
                       bool pinned) {
    SetWindowPinnedImpl(session_id, base_window_id, pinned);
  }
  bool IsWindowPinned(size_t session_id,
                      const std::string& base_window_id) const {
    return IsWindowPinnedImpl(session_id, base_window_id);
  }
  std::vector<std::string> GetPinnedWindows(size_t session_id) const {
    return GetPinnedWindowsImpl(session_id);
  }

  void SetWindowPinned(const std::string& base_window_id, bool pinned) {
    SetWindowPinnedImpl(base_window_id, pinned);
  }
  bool IsWindowPinned(const std::string& base_window_id) const {
    return IsWindowPinnedImpl(base_window_id);
  }
  std::vector<std::string> GetPinnedWindows() const {
    return GetPinnedWindowsImpl();
  }

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
  void EnforceResourceWindowLimits(const std::string& resource_type) {
    EnforceResourceLimits(resource_type);
  }

  /**
   * @brief Mark a panel as recently used (for LRU)
   * @param panel_id The panel ID
   */
  void MarkPanelUsed(const std::string& panel_id);

  WindowContent* FindPanelInstance(const std::string& prefixed_panel_id,
                                   const std::string& base_panel_id);
  const WindowContent* FindPanelInstance(
      const std::string& prefixed_panel_id,
      const std::string& base_panel_id) const;

 private:
  struct WindowContextScopeHash {
    size_t operator()(WindowContextScope scope) const noexcept {
      return static_cast<size_t>(scope);
    }
  };

  void ApplyContextPolicy(size_t session_id, WindowContextScope scope,
                          const std::string& old_key,
                          const std::string& new_key);
  WindowDescriptor* FindDescriptorByPrefixedId(const std::string& prefixed_id);
  const WindowDescriptor* FindDescriptorByPrefixedId(
      const std::string& prefixed_id) const;
  std::vector<std::string>* FindSessionWindowIds(size_t session_id);
  const std::vector<std::string>* FindSessionWindowIds(size_t session_id) const;
  std::unordered_map<std::string, std::string>* FindSessionWindowMapping(
      size_t session_id);
  const std::unordered_map<std::string, std::string>* FindSessionWindowMapping(
      size_t session_id) const;
  std::unordered_map<std::string, std::string>* FindSessionReverseWindowMapping(
      size_t session_id);
  const std::unordered_map<std::string, std::string>*
  FindSessionReverseWindowMapping(size_t session_id) const;
  void PublishWindowVisibilityChanged(size_t session_id,
                                      const std::string& prefixed_window_id,
                                      const std::string& base_window_id,
                                      const std::string& category,
                                      bool visible) const;
  bool OpenWindowImpl(size_t session_id, const std::string& base_card_id);
  bool CloseWindowImpl(size_t session_id, const std::string& base_card_id);
  bool ToggleWindowImpl(size_t session_id, const std::string& base_card_id);
  bool IsWindowVisibleImpl(size_t session_id,
                           const std::string& base_card_id) const;
  bool* GetVisibilityFlag(size_t session_id, const std::string& base_card_id);
  std::vector<std::string> GetWindowsInSessionImpl(size_t session_id) const;
  std::vector<WindowDescriptor> GetWindowsInCategoryImpl(
      size_t session_id, const std::string& category) const;
  std::vector<std::string> GetVisibleWindowIdsImpl(size_t session_id) const;
  void SetVisibleWindowsImpl(size_t session_id,
                             const std::vector<std::string>& panel_ids);
  const WindowDescriptor* GetWindowDescriptorImpl(
      size_t session_id, const std::string& base_card_id) const;
  std::string GetWindowNameImpl(size_t session_id,
                                const std::string& base_card_id) const;
  std::string GetWindowNameImpl(const WindowDescriptor& descriptor) const;
  void MarkWindowRecentlyUsedImpl(const std::string& card_id);
  std::vector<WindowDescriptor> GetWindowsSortedByMRUImpl(
      size_t session_id, const std::string& category) const;
  uint64_t GetWindowMRUTimeImpl(const std::string& card_id) const {
    auto iter = last_used_at_.find(card_id);
    return (iter != last_used_at_.end()) ? iter->second : 0;
  }
  void SetWindowPinnedImpl(size_t session_id, const std::string& base_card_id,
                           bool pinned);
  bool IsWindowPinnedImpl(size_t session_id,
                          const std::string& base_card_id) const;
  std::vector<std::string> GetPinnedWindowsImpl(size_t session_id) const;
  void SetWindowPinnedImpl(const std::string& base_card_id, bool pinned);
  bool IsWindowPinnedImpl(const std::string& base_card_id) const;
  std::vector<std::string> GetPinnedWindowsImpl() const;
  size_t GetResourceWindowLimit(const std::string& resource_type) const;
  void TrackResourceWindow(const std::string& panel_id,
                           ResourceWindowContent* resource_panel);
  void UntrackResourceWindow(const std::string& panel_id);
  std::string SelectResourceWindowForEviction(
      const std::list<std::string>& panel_ids) const;
  std::string ResolveBaseWindowId(const std::string& panel_id) const;
  std::string GetBaseIdForPrefixedId(size_t session_id,
                                     const std::string& prefixed_id) const;

  // ... existing private members ...

  struct ResourceWindowState {
    std::unordered_map<std::string, std::list<std::string>> window_ids_by_type;
    std::unordered_map<std::string, std::string> window_type_by_id;
  };

  ResourceWindowState resource_state_;

  std::unordered_map<std::string, std::list<std::string>>& resource_panels_ =
      resource_state_.window_ids_by_type;
  std::unordered_map<std::string, std::string>& panel_resource_types_ =
      resource_state_.window_type_by_id;

  struct WindowRegistryState {
    std::unordered_map<std::string, WindowDescriptor> descriptors;
    std::unordered_map<std::string, std::unique_ptr<WindowContent>> instances;
    std::unordered_set<std::string> registry_ids;
    std::unordered_set<std::string> global_ids;
    std::unordered_map<std::string, bool> centralized_visibility;
    std::unordered_map<std::string, std::string> aliases;
  };

  struct WindowSessionState {
    std::unordered_map<std::string, uint64_t> last_used_at;
    uint64_t mru_counter = 0;
    std::unordered_map<std::string, bool> pinned_windows;
    size_t session_count = 0;
    size_t active_session = 0;
    std::unordered_map<size_t, std::vector<std::string>> session_windows;
    std::unordered_map<size_t, std::unordered_map<std::string, std::string>>
        session_window_mapping;
    std::unordered_map<size_t, std::unordered_map<std::string, std::string>>
        session_reverse_window_mapping;
    std::unordered_map<size_t,
                       std::unordered_map<WindowContextScope, std::string,
                                          WindowContextScopeHash>>
        session_context_keys;
  };

  WindowRegistryState registry_state_;
  WindowSessionState session_state_;

  // Legacy field aliases kept during the panel->window migration while methods
  // are gradually moved over to the grouped state containers above.
  std::unordered_map<std::string, WindowDescriptor>& cards_ =
      registry_state_.descriptors;
  std::unordered_map<std::string, std::unique_ptr<WindowContent>>&
      panel_instances_ = registry_state_.instances;
  std::unordered_set<std::string>& registry_panel_ids_ =
      registry_state_.registry_ids;
  std::unordered_set<std::string>& global_panel_ids_ =
      registry_state_.global_ids;
  std::unordered_map<std::string, bool>& centralized_visibility_ =
      registry_state_.centralized_visibility;
  std::unordered_map<std::string, std::string>& panel_id_aliases_ =
      registry_state_.aliases;

  std::unordered_map<std::string, uint64_t>& last_used_at_ =
      session_state_.last_used_at;
  uint64_t& mru_counter_ = session_state_.mru_counter;
  std::unordered_map<std::string, bool>& pinned_panels_ =
      session_state_.pinned_windows;
  size_t& session_count_ = session_state_.session_count;
  size_t& active_session_ = session_state_.active_session;
  std::unordered_map<size_t, std::vector<std::string>>& session_cards_ =
      session_state_.session_windows;
  std::unordered_map<size_t, std::unordered_map<std::string, std::string>>&
      session_card_mapping_ = session_state_.session_window_mapping;
  std::unordered_map<size_t, std::unordered_map<std::string, std::string>>&
      session_reverse_card_mapping_ =
          session_state_.session_reverse_window_mapping;
  std::unordered_map<size_t, std::unordered_map<WindowContextScope, std::string,
                                                WindowContextScopeHash>>&
      session_context_keys_ = session_state_.session_context_keys;

  // Workspace presets
  std::unordered_map<std::string, WorkspacePreset> presets_;

  struct WindowBrowserState {
    static constexpr size_t kMaxRecentCategories = 5;

    std::string active_category;
    std::vector<std::string> recent_categories;
    bool sidebar_visible = false;
    bool sidebar_expanded = false;
    float sidebar_width = 0.0f;
    float window_browser_category_width = GetDefaultPanelBrowserCategoryWidth();
    int focused_window_index = -1;
    bool sidebar_has_focus = false;
    std::function<void(bool visible, bool expanded)> on_sidebar_state_changed;
    std::function<void(float width)> on_sidebar_width_changed;
    std::function<void(float width)> on_window_browser_width_changed;
    std::function<void(const std::string&)> on_category_changed;
    std::function<void(const std::string&)> on_window_clicked;
    std::function<void(const std::string&)> on_category_selected;
    std::unordered_map<std::string, std::unique_ptr<FileBrowser>>
        category_file_browsers;
  };

  WindowBrowserState browser_state_;

  // Unified visibility state (single source of truth)
  bool emulator_visible_ = false;  // Emulator window visibility

  // EventBus for action events (preferred over callbacks)
  EventBus* event_bus_ = nullptr;

  // State change callbacks
  std::function<void(bool)> on_emulator_visibility_changed_;
  std::function<void(const std::string&, const std::string&)> on_file_clicked_;
  std::function<Editor*(const std::string&)> editor_resolver_;

  // Helper methods
  void UpdateSessionCount();
  std::string GetPrefixedWindowId(size_t session_id,
                                  const std::string& base_id) const;
  void RegisterPanelDescriptorForSession(size_t session_id,
                                         const WindowContent& panel);
  void TrackPanelForSession(size_t session_id, const std::string& base_id,
                            const std::string& panel_id);
  void UnregisterSessionPanels(size_t session_id);
  void SavePresetsToFile();
  void LoadPresetsFromFile();
};

}  // namespace editor
}  // namespace yaze

#undef YAZE_CARD_SHIM_DEPRECATED

#endif  // YAZE_APP_EDITOR_SYSTEM_WORKSPACE_WINDOW_MANAGER_H_
