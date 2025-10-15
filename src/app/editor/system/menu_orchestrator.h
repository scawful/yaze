#ifndef YAZE_APP_EDITOR_SYSTEM_MENU_ORCHESTRATOR_H_
#define YAZE_APP_EDITOR_SYSTEM_MENU_ORCHESTRATOR_H_

#include <functional>
#include <string>

#include "absl/status/status.h"
#include "app/editor/editor.h"
#include "app/editor/ui/menu_builder.h"

namespace yaze {
namespace editor {

// Forward declarations to avoid circular dependencies
class EditorManager;
class RomFileManager;
class ProjectManager;
class EditorRegistry;
class SessionCoordinator;
class ToastManager;

/**
 * @class MenuOrchestrator
 * @brief Handles all menu building and UI coordination logic
 * 
 * Extracted from EditorManager to provide focused menu management:
 * - Menu structure and organization
 * - Menu item callbacks and shortcuts
 * - Editor-specific menu items
 * - Session-aware menu updates
 * - Menu state management
 * 
 * This class follows the Single Responsibility Principle by focusing solely
 * on menu construction and coordination, delegating actual operations to
 * specialized managers.
 */
class MenuOrchestrator {
 public:
  // Constructor takes references to the managers it coordinates with
  MenuOrchestrator(MenuBuilder& menu_builder,
                   RomFileManager& rom_manager,
                   ProjectManager& project_manager,
                   EditorRegistry& editor_registry,
                   SessionCoordinator& session_coordinator,
                   ToastManager& toast_manager);
  ~MenuOrchestrator() = default;
  
  // Non-copyable due to reference members
  MenuOrchestrator(const MenuOrchestrator&) = delete;
  MenuOrchestrator& operator=(const MenuOrchestrator&) = delete;

  // Main menu building interface
  void BuildMainMenu();
  void BuildFileMenu();
  void BuildEditMenu();
  void BuildViewMenu();
  void BuildToolsMenu();
  void BuildWindowMenu();
  void BuildHelpMenu();

  // Menu state management
  void ClearMenu();
  void RefreshMenu();
  
  // Menu item callbacks (delegated to appropriate managers)
  void OnOpenRom();
  void OnSaveRom();
  void OnSaveRomAs();
  void OnCreateProject();
  void OnOpenProject();
  void OnSaveProject();
  void OnSaveProjectAs();
  
  // Editor-specific menu actions
  void OnSwitchToEditor(EditorType editor_type);
  void OnShowEditorSelection();
  void OnShowDisplaySettings();
  
  // Session management menu actions
  void OnCreateNewSession();
  void OnDuplicateCurrentSession();
  void OnCloseCurrentSession();
  void OnSwitchToSession(size_t session_index);
  
  // Window management menu actions
  void OnShowAllWindows();
  void OnHideAllWindows();
  void OnResetWorkspaceLayout();
  void OnSaveWorkspaceLayout();
  void OnLoadWorkspaceLayout();
  
  // Tool menu actions
  void OnShowGlobalSearch();
  void OnShowPerformanceDashboard();
  void OnShowImGuiDemo();
  void OnShowImGuiMetrics();
  
  // Help menu actions
  void OnShowAbout();
  void OnShowKeyboardShortcuts();
  void OnShowUserGuide();

 private:
  // References to coordinated managers
  MenuBuilder& menu_builder_;
  RomFileManager& rom_manager_;
  ProjectManager& project_manager_;
  EditorRegistry& editor_registry_;
  SessionCoordinator& session_coordinator_;
  ToastManager& toast_manager_;
  
  // Menu state
  bool menu_needs_refresh_ = false;
  
  // Helper methods for menu construction
  void AddFileMenuItems();
  void AddEditMenuItems();
  void AddViewMenuItems();
  void AddToolsMenuItems();
  void AddWindowMenuItems();
  void AddHelpMenuItems();
  
  // Menu item validation helpers
  bool CanSaveRom() const;
  bool CanSaveProject() const;
  bool HasActiveRom() const;
  bool HasActiveProject() const;
  bool HasCurrentEditor() const;
  bool HasMultipleSessions() const;
  
  // Menu item text generation
  std::string GetRomFilename() const;
  std::string GetProjectName() const;
  std::string GetCurrentEditorName() const;
  
  // Shortcut key management
  std::string GetShortcutForAction(const std::string& action) const;
  void RegisterGlobalShortcuts();
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_MENU_ORCHESTRATOR_H_
