#ifndef YAZE_APP_EDITOR_UI_UI_COORDINATOR_H_
#define YAZE_APP_EDITOR_UI_UI_COORDINATOR_H_

#include <string>
#include <memory>

#include "absl/status/status.h"
#include "app/editor/editor.h"
#include "app/editor/system/popup_manager.h"
#include "app/editor/ui/welcome_screen.h"
#include "app/gui/core/icons.h"

namespace yaze {
namespace editor {

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
  UICoordinator(EditorManager* editor_manager,
                RomFileManager& rom_manager,
                ProjectManager& project_manager,
                EditorRegistry& editor_registry,
                SessionCoordinator& session_coordinator,
                WindowDelegate& window_delegate,
                ToastManager& toast_manager,
                PopupManager& popup_manager,
                ShortcutManager& shortcut_manager);
  ~UICoordinator() = default;
  
  // Non-copyable due to reference members
  UICoordinator(const UICoordinator&) = delete;
  UICoordinator& operator=(const UICoordinator&) = delete;

  // Main UI drawing interface
  void DrawAllUI();
  void DrawMenuBarExtras();
  void DrawContextSensitiveCardControl();
  
  // Core UI components (actual ImGui rendering moved from EditorManager)
  void DrawCommandPalette();
  void DrawGlobalSearch();
  
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
  void ShowSessionSwitcher() { show_session_switcher_ = true; }
  void HideCurrentEditorCards();
  void ToggleCardSidebar() { show_card_sidebar_ = !show_card_sidebar_; }
  void ShowGlobalSearch() { show_global_search_ = true; }
  void ShowCommandPalette() { show_command_palette_ = true; }
  void ShowCardBrowser() { show_card_browser_ = true; }
  
  // Window visibility management
  void ShowAllWindows();
  void HideAllWindows();
  
  // UI state queries (EditorManager can check these)
  bool IsEditorSelectionVisible() const { return show_editor_selection_; }
  bool IsDisplaySettingsVisible() const { return show_display_settings_; }
  bool IsSessionSwitcherVisible() const { return show_session_switcher_; }
  bool IsWelcomeScreenVisible() const { return show_welcome_screen_; }
  bool IsWelcomeScreenManuallyClosed() const { return welcome_screen_manually_closed_; }
  bool IsGlobalSearchVisible() const { return show_global_search_; }
  bool IsPerformanceDashboardVisible() const { return show_performance_dashboard_; }
  bool IsCardBrowserVisible() const { return show_card_browser_; }
  bool IsCommandPaletteVisible() const { return show_command_palette_; }
  bool IsCardSidebarVisible() const { return show_card_sidebar_; }
  
  // UI state setters (for programmatic control)
  void SetEditorSelectionVisible(bool visible) { show_editor_selection_ = visible; }
  void SetDisplaySettingsVisible(bool visible) { show_display_settings_ = visible; }
  void SetSessionSwitcherVisible(bool visible) { show_session_switcher_ = visible; }
  void SetWelcomeScreenVisible(bool visible) { show_welcome_screen_ = visible; }
  void SetWelcomeScreenManuallyClosed(bool closed) { welcome_screen_manually_closed_ = closed; }
  void SetGlobalSearchVisible(bool visible) { show_global_search_ = visible; }
  void SetPerformanceDashboardVisible(bool visible) { show_performance_dashboard_ = visible; }
  void SetCardBrowserVisible(bool visible) { show_card_browser_ = visible; }
  void SetCommandPaletteVisible(bool visible) { show_command_palette_ = visible; }
  void SetCardSidebarVisible(bool visible) { show_card_sidebar_ = visible; }
  void SetImGuiDemoVisible(bool visible) { show_imgui_demo_ = visible; }
  void SetImGuiMetricsVisible(bool visible) { show_imgui_metrics_ = visible; }
  
  // Note: Theme styling is handled by ThemeManager, not UICoordinator

 private:
  // References to coordinated managers
  EditorManager* editor_manager_;
  RomFileManager& rom_manager_;
  ProjectManager& project_manager_;
  EditorRegistry& editor_registry_;
  SessionCoordinator& session_coordinator_;
  WindowDelegate& window_delegate_;
  ToastManager& toast_manager_;
  PopupManager& popup_manager_;
  ShortcutManager& shortcut_manager_;
  
  // UI state flags (UICoordinator owns all UI visibility state)
  bool show_editor_selection_ = false;
  bool show_display_settings_ = false;
  bool show_session_switcher_ = false;
  bool show_welcome_screen_ = true;
  bool welcome_screen_manually_closed_ = false;
  bool show_global_search_ = false;
  bool show_performance_dashboard_ = false;
  bool show_imgui_demo_ = false;
  bool show_imgui_metrics_ = false;
  bool show_test_dashboard_ = false;
  bool show_card_browser_ = false;
  bool show_command_palette_ = false;
  bool show_emulator_ = false;
  bool show_memory_editor_ = false;
  bool show_asm_editor_ = false;
  bool show_palette_editor_ = false;
  bool show_resource_label_manager_ = false;
  bool show_card_sidebar_ = false;
  
  // Welcome screen component
  std::unique_ptr<WelcomeScreen> welcome_screen_;
  
  // Helper methods for drawing operations
  void DrawSessionIndicator();
  void DrawVersionInfo();
  void DrawSessionTabs();
  void DrawSessionBadges();
  
  // Material Design component helpers
  void DrawMaterialButton(const std::string& text, const std::string& icon, 
                         std::function<void()> callback, bool enabled = true);
  void DrawMaterialCard(const std::string& title, const std::string& content);
  void DrawMaterialDialog(const std::string& title, std::function<void()> content);
  
  // Layout and positioning helpers
  void CenterWindow(const std::string& window_name);
  void PositionWindow(const std::string& window_name, float x, float y);
  void SetWindowSize(const std::string& window_name, float width, float height);
  
  // Icon and theming helpers
  std::string GetIconForEditor(EditorType type) const;
  std::string GetColorForEditor(EditorType type) const;
  void ApplyEditorTheme(EditorType type);
  
  // Session UI helpers
  void DrawSessionList();
  void DrawSessionControls();
  void DrawSessionInfo();
  void DrawSessionStatus();
  
  // Popup helpers
  void DrawHelpMenuPopups();
  void DrawSettingsPopups();
  void DrawProjectPopups();
  void DrawSessionPopups();
  
  // Window management helpers
  void DrawWindowControls();
  void DrawLayoutControls();
  void DrawDockingControls();
  
  // Performance and debug UI
  void DrawPerformanceUI();
  void DrawDebugUI();
  void DrawTestingUI();
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_UI_UI_COORDINATOR_H_
