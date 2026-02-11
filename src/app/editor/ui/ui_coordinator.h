#ifndef YAZE_APP_EDITOR_UI_UI_COORDINATOR_H_
#define YAZE_APP_EDITOR_UI_UI_COORDINATOR_H_

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "app/editor/editor.h"
#include "app/editor/system/command_palette.h"
#include "app/editor/ui/popup_manager.h"
#include "app/editor/ui/welcome_screen.h"
#include "app/gui/core/icons.h"
#include "app/startup_flags.h"

namespace yaze {
namespace editor {

/**
 * @brief Represents the current startup surface state
 *
 * Single source of truth for startup UI visibility:
 * - kWelcome: No ROM loaded, showing onboarding screen
 * - kDashboard: ROM loaded, no active editor (editor chooser)
 * - kEditor: Active editor/category selected
 */
enum class StartupSurface {
  kWelcome,    // No ROM, showing onboarding
  kDashboard,  // ROM loaded, no active editor
  kEditor,     // Active editor/category
};

// Forward declarations to avoid circular dependencies
class EditorManager;
class RomFileManager;
class ProjectManager;
class EditorRegistry;
class SessionCoordinator;
class ToastManager;
class WindowDelegate;
class ShortcutManager;

/**
 * @class UICoordinator
 * @brief Handles all UI drawing operations and state management
 *
 * Extracted from EditorManager to provide focused UI coordination:
 * - Drawing operations (menus, dialogs, screens)
 * - UI state management (visibility, focus, layout)
 * - Popup and dialog coordination
 * - Welcome screen and session UI
 * - Material Design theming and icons
 *
 * This class follows the Single Responsibility Principle by focusing solely
 * on UI presentation and user interaction, delegating business logic to
 * specialized managers.
 */
class UICoordinator {
 public:
  // Constructor takes references to the managers it coordinates with
  UICoordinator(EditorManager* editor_manager, RomFileManager& rom_manager,
                ProjectManager& project_manager,
                EditorRegistry& editor_registry,
                PanelManager& card_registry,
                SessionCoordinator& session_coordinator,
                WindowDelegate& window_delegate, ToastManager& toast_manager,
                PopupManager& popup_manager, ShortcutManager& shortcut_manager);
  ~UICoordinator() = default;

  // Non-copyable due to reference members
  UICoordinator(const UICoordinator&) = delete;
  UICoordinator& operator=(const UICoordinator&) = delete;

  // Main UI drawing interface
  void DrawBackground();
  void DrawAllUI();
  void DrawMenuBarExtras();
  void DrawNotificationBell(bool show_dirty, bool has_dirty_rom,
                            bool show_session, bool has_multiple_sessions);
  void DrawSessionButton();

  // Core UI components (actual ImGui rendering moved from EditorManager)
  void DrawCommandPalette();
  void DrawPanelFinder();
  void DrawGlobalSearch();
  void DrawWorkspacePresetDialogs();

  // Session UI components
  void DrawSessionSwitcher();
  void DrawSessionManager();
  void DrawSessionRenameDialog();
  void DrawLayoutPresets();

  // Welcome screen and project UI
  void DrawWelcomeScreen();
  void DrawProjectHelp();

  // Window management UI
  void DrawWindowManagementUI();

  // Popup and dialog management
  void DrawAllPopups();
  void ShowPopup(const std::string& popup_name);
  void HidePopup(const std::string& popup_name);

  // UI state management
  void ShowEditorSelection() { show_editor_selection_ = true; }
  void ShowDisplaySettings();
  void ShowSaveWorkspacePresetDialog() { show_save_workspace_preset_ = true; }
  void ShowLoadWorkspacePresetDialog() { show_load_workspace_preset_ = true; }
  // Session switcher is now managed by SessionCoordinator
  void ShowSessionSwitcher();
  // Sidebar visibility delegates to PanelManager (single source of truth)
  void TogglePanelSidebar();
  void ShowGlobalSearch() { show_global_search_ = true; }
  void ShowCommandPalette() { show_command_palette_ = true; }
  void ShowPanelFinder() { show_panel_finder_ = true; }
  void ShowPanelBrowser() { show_panel_browser_ = true; }

  /**
   * @brief Initialize command palette with all discoverable commands
   * @param session_id Current session ID for panel commands
   */
  void InitializeCommandPalette(size_t session_id);

  /**
   * @brief Refresh command palette commands (call after session switch)
   * @param session_id New session ID
   */
  void RefreshCommandPalette(size_t session_id);

  // Menu bar visibility (for WASM/web app mode)
  bool IsMenuBarVisible() const { return show_menu_bar_; }
  void SetMenuBarVisible(bool visible) { show_menu_bar_ = visible; }
  void ToggleMenuBar() { show_menu_bar_ = !show_menu_bar_; }

  // Draw floating menu bar restore button (when menu bar is hidden)
  void DrawMenuBarRestoreButton();

  // Window visibility management
  void ShowAllWindows();
  void HideAllWindows();

  // UI state queries (EditorManager can check these)
  bool IsEditorSelectionVisible() const { return show_editor_selection_; }
  bool IsDisplaySettingsVisible() const { return show_display_settings_; }
  // Session switcher visibility managed by SessionCoordinator
  bool IsSessionSwitcherVisible() const;
  bool IsWelcomeScreenVisible() const { return show_welcome_screen_; }
  bool IsWelcomeScreenManuallyClosed() const {
    return welcome_screen_manually_closed_;
  }
  bool IsGlobalSearchVisible() const { return show_global_search_; }
  bool IsPerformanceDashboardVisible() const {
    return show_performance_dashboard_;
  }
  bool IsPanelBrowserVisible() const { return show_panel_browser_; }
  bool IsCommandPaletteVisible() const { return show_command_palette_; }
  // Sidebar visibility delegates to PanelManager (single source of truth)
  bool IsPanelSidebarVisible() const;
  bool IsImGuiDemoVisible() const { return show_imgui_demo_; }
  bool IsImGuiMetricsVisible() const { return show_imgui_metrics_; }
  // Emulator visibility delegates to PanelManager (single source of truth)
  bool IsEmulatorVisible() const;
  bool IsMemoryEditorVisible() const { return show_memory_editor_; }
  bool IsAsmEditorVisible() const { return show_asm_editor_; }
  bool IsPaletteEditorVisible() const { return show_palette_editor_; }
  bool IsResourceLabelManagerVisible() const {
    return show_resource_label_manager_;
  }
  bool IsAIAgentVisible() const { return show_ai_agent_; }
  bool IsChatHistoryVisible() const { return show_chat_history_; }
  bool IsProposalDrawerVisible() const { return show_proposal_drawer_; }

  // UI state setters (for programmatic control)
  void SetEditorSelectionVisible(bool visible) {
    show_editor_selection_ = visible;
  }
  void SetDisplaySettingsVisible(bool visible) {
    show_display_settings_ = visible;
  }
  // Session switcher state managed by SessionCoordinator
  void SetSessionSwitcherVisible(bool visible);
  void SetWelcomeScreenVisible(bool visible) { show_welcome_screen_ = visible; }
  void SetWelcomeScreenManuallyClosed(bool closed) {
    welcome_screen_manually_closed_ = closed;
  }
  void SetWelcomeScreenBehavior(StartupVisibility mode);
  void SetGlobalSearchVisible(bool visible) { show_global_search_ = visible; }
  void SetPerformanceDashboardVisible(bool visible) {
    show_performance_dashboard_ = visible;
  }
  void SetPanelBrowserVisible(bool visible) { show_panel_browser_ = visible; }
  void SetCommandPaletteVisible(bool visible) {
    show_command_palette_ = visible;
  }
  // Sidebar visibility delegates to PanelManager (single source of truth)
  void SetPanelSidebarVisible(bool visible);
  void SetImGuiDemoVisible(bool visible) { show_imgui_demo_ = visible; }
  void SetImGuiMetricsVisible(bool visible) { show_imgui_metrics_ = visible; }
  // Emulator visibility delegates to PanelManager (single source of truth)
  void SetEmulatorVisible(bool visible);
  void SetMemoryEditorVisible(bool visible) { show_memory_editor_ = visible; }
  void SetAsmEditorVisible(bool visible) { show_asm_editor_ = visible; }
  void SetPaletteEditorVisible(bool visible) { show_palette_editor_ = visible; }
  void SetResourceLabelManagerVisible(bool visible) {
    show_resource_label_manager_ = visible;
  }
  void SetDashboardBehavior(StartupVisibility mode);
  void SetAIAgentVisible(bool visible) { show_ai_agent_ = visible; }

  // Startup surface management (single source of truth)
  StartupSurface GetCurrentStartupSurface() const { return current_startup_surface_; }
  void SetStartupSurface(StartupSurface surface);
  bool ShouldShowWelcome() const;
  bool ShouldShowDashboard() const;
  bool ShouldShowActivityBar() const;
  void SetChatHistoryVisible(bool visible) { show_chat_history_ = visible; }
  void SetProposalDrawerVisible(bool visible) { show_proposal_drawer_ = visible; }

  // Note: Theme styling is handled by ThemeManager, not UICoordinator

 private:
  // References to coordinated managers
  EditorManager* editor_manager_;
  RomFileManager& rom_manager_;
  ProjectManager& project_manager_;
  EditorRegistry& editor_registry_;
  PanelManager& panel_manager_;
  SessionCoordinator& session_coordinator_;
  WindowDelegate& window_delegate_;
  ToastManager& toast_manager_;
  PopupManager& popup_manager_;
  ShortcutManager& shortcut_manager_;

  // UI state flags (UICoordinator owns all UI visibility state)
  bool show_editor_selection_ = false;
  bool show_display_settings_ = false;
  // show_session_switcher_ removed - managed by SessionCoordinator
  bool show_welcome_screen_ = true;
  bool welcome_screen_manually_closed_ = false;
  bool show_global_search_ = false;
  bool show_performance_dashboard_ = false;
  bool show_imgui_demo_ = false;
  bool show_imgui_metrics_ = false;
  bool show_test_dashboard_ = false;
  bool show_panel_browser_ = false;
  bool show_panel_finder_ = false;
  bool show_command_palette_ = false;
  // show_emulator_ removed - now managed by PanelManager
  // show_panel_sidebar_ removed - now managed by PanelManager
  bool show_memory_editor_ = false;
  bool show_asm_editor_ = false;
  bool show_palette_editor_ = false;
  bool show_resource_label_manager_ = false;
  bool show_ai_agent_ = false;
  bool show_chat_history_ = false;
  bool show_proposal_drawer_ = false;
  bool show_save_workspace_preset_ = false;
  bool show_load_workspace_preset_ = false;
  bool show_menu_bar_ = true;      // Menu bar visible by default
  StartupVisibility welcome_behavior_override_ = StartupVisibility::kAuto;
  StartupVisibility dashboard_behavior_override_ = StartupVisibility::kAuto;

  // Single source of truth for startup surface state
  StartupSurface current_startup_surface_ = StartupSurface::kWelcome;

  // Command Palette state
  CommandPalette command_palette_;
  bool command_palette_initialized_ = false;
  char command_palette_query_[256] = {};
  int command_palette_selected_idx_ = 0;

  // Panel Finder state
  char panel_finder_query_[256] = {};
  int panel_finder_selected_idx_ = 0;

  // Global Search state
  char global_search_query_[256] = {};

  // Welcome screen component
  std::unique_ptr<WelcomeScreen> welcome_screen_;


  // Menu bar icon button helper - provides consistent styling for all menubar buttons
  // Returns true if button was clicked
  bool DrawMenuBarIconButton(const char* icon, const char* tooltip,
                             bool is_active = false);

  // Calculate width of a menubar icon button (icon + frame padding)
  static float GetMenuBarIconButtonWidth();

  // Material Design component helpers
  void DrawMaterialButton(const std::string& text, const std::string& icon,
                          const ImVec4& color, std::function<void()> callback,
                          bool enabled = true);

  // Mobile helpers
  bool IsCompactLayout() const;
  void DrawMobileNavigation();

  // Layout and positioning helpers
  void CenterWindow(const std::string& window_name);
  void PositionWindow(const std::string& window_name, float x, float y);
  void SetWindowSize(const std::string& window_name, float width, float height);

};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_UI_UI_COORDINATOR_H_
