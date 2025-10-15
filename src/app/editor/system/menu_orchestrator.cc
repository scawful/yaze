#include "menu_orchestrator.h"

#include "absl/strings/str_format.h"
#include "app/editor/editor.h"
#include "app/editor/system/editor_registry.h"
#include "app/editor/system/project_manager.h"
#include "app/editor/system/rom_file_manager.h"
#include "app/editor/system/toast_manager.h"
#include "app/editor/ui/menu_builder.h"
#include "app/editor/system/session_coordinator.h"
#include "app/gui/core/icons.h"

namespace yaze {
namespace editor {

MenuOrchestrator::MenuOrchestrator(
    MenuBuilder& menu_builder,
    RomFileManager& rom_manager,
    ProjectManager& project_manager,
    EditorRegistry& editor_registry,
    SessionCoordinator& session_coordinator,
    ToastManager& toast_manager)
    : menu_builder_(menu_builder),
      rom_manager_(rom_manager),
      project_manager_(project_manager),
      editor_registry_(editor_registry),
      session_coordinator_(session_coordinator),
      toast_manager_(toast_manager) {
}

void MenuOrchestrator::BuildMainMenu() {
  ClearMenu();
  
  // Build all menu sections in order
  BuildFileMenu();
  BuildEditMenu();
  BuildViewMenu();
  BuildToolsMenu();
  BuildWindowMenu();
  BuildHelpMenu();
  
  menu_needs_refresh_ = false;
}

void MenuOrchestrator::BuildFileMenu() {
  menu_builder_.BeginMenu("File");
  AddFileMenuItems();
  menu_builder_.EndMenu();
}

void MenuOrchestrator::AddFileMenuItems() {
  // ROM Operations
  menu_builder_
      .Item("Open ROM", ICON_MD_FILE_OPEN, 
            [this]() { OnOpenRom(); }, "Ctrl+O")
      .Item("Save ROM", ICON_MD_SAVE, 
            [this]() { OnSaveRom(); }, "Ctrl+S",
            [this]() { return CanSaveRom(); })
      .Item("Save As...", ICON_MD_SAVE_AS,
            [this]() { OnSaveRomAs(); }, nullptr,
            [this]() { return CanSaveRom(); })
      .Separator();
  
  // Project Operations
  menu_builder_
      .Item("New Project", ICON_MD_CREATE_NEW_FOLDER,
            [this]() { OnCreateProject(); })
      .Item("Open Project", ICON_MD_FOLDER_OPEN,
            [this]() { OnOpenProject(); })
      .Item("Save Project", ICON_MD_SAVE, 
            [this]() { OnSaveProject(); }, nullptr,
            [this]() { return CanSaveProject(); })
      .Item("Save Project As...", ICON_MD_SAVE_AS,
            [this]() { OnSaveProjectAs(); }, nullptr,
            [this]() { return CanSaveProject(); })
      .Separator();
  
  // ROM Information and Validation
  menu_builder_
      .Item("ROM Information", ICON_MD_INFO,
            [this]() { 
              // TODO: Show ROM info popup
              toast_manager_.Show("ROM Information", ToastType::kInfo);
            }, nullptr,
            [this]() { return HasActiveRom(); })
      .Item("Create Backup", ICON_MD_BACKUP,
            [this]() {
              if (HasActiveRom()) {
                auto status = rom_manager_.CreateBackup();
                if (!status.ok()) {
                  toast_manager_.Show(
                      absl::StrFormat("Backup failed: %s", status.message()),
                      ToastType::kError);
                }
              }
            }, nullptr,
            [this]() { return HasActiveRom(); })
      .Item("Validate ROM", ICON_MD_CHECK_CIRCLE,
            [this]() {
              if (HasActiveRom()) {
                auto status = rom_manager_.ValidateRom();
                if (!status.ok()) {
                  toast_manager_.Show(
                      absl::StrFormat("Validation failed: %s", status.message()),
                      ToastType::kError);
                }
              }
            }, nullptr,
            [this]() { return HasActiveRom(); })
      .Separator();
  
  // Settings and Quit
  menu_builder_
      .Item("Settings", ICON_MD_SETTINGS,
            [this]() { 
              // TODO: Show settings editor
              toast_manager_.Show("Settings", ToastType::kInfo);
            })
      .Separator()
      .Item("Quit", ICON_MD_EXIT_TO_APP, 
            [this]() { 
              // TODO: Signal quit to EditorManager
              toast_manager_.Show("Quit requested", ToastType::kInfo);
            }, "Ctrl+Q");
}

void MenuOrchestrator::BuildEditMenu() {
  menu_builder_.BeginMenu("Edit");
  AddEditMenuItems();
  menu_builder_.EndMenu();
}

void MenuOrchestrator::AddEditMenuItems() {
  // Undo/Redo operations
  menu_builder_
      .Item("Undo", ICON_MD_UNDO,
            [this]() {
              // TODO: Delegate to current editor
              toast_manager_.Show("Undo", ToastType::kInfo);
            }, "Ctrl+Z",
            [this]() { return HasCurrentEditor(); })
      .Item("Redo", ICON_MD_REDO,
            [this]() {
              // TODO: Delegate to current editor
              toast_manager_.Show("Redo", ToastType::kInfo);
            }, "Ctrl+Y",
            [this]() { return HasCurrentEditor(); })
      .Separator();
  
  // Clipboard operations
  menu_builder_
      .Item("Cut", ICON_MD_CONTENT_CUT,
            [this]() {
              // TODO: Delegate to current editor
              toast_manager_.Show("Cut", ToastType::kInfo);
            }, "Ctrl+X",
            [this]() { return HasCurrentEditor(); })
      .Item("Copy", ICON_MD_CONTENT_COPY,
            [this]() {
              // TODO: Delegate to current editor
              toast_manager_.Show("Copy", ToastType::kInfo);
            }, "Ctrl+C",
            [this]() { return HasCurrentEditor(); })
      .Item("Paste", ICON_MD_CONTENT_PASTE,
            [this]() {
              // TODO: Delegate to current editor
              toast_manager_.Show("Paste", ToastType::kInfo);
            }, "Ctrl+V",
            [this]() { return HasCurrentEditor(); })
      .Separator();
  
  // Search operations
  menu_builder_
      .Item("Find", ICON_MD_SEARCH,
            [this]() {
              // TODO: Delegate to current editor
              toast_manager_.Show("Find", ToastType::kInfo);
            }, "Ctrl+F",
            [this]() { return HasCurrentEditor(); })
      .Item("Find in Files", ICON_MD_SEARCH,
            [this]() { OnShowGlobalSearch(); }, "Ctrl+Shift+F");
}

void MenuOrchestrator::BuildViewMenu() {
  menu_builder_.BeginMenu("View");
  AddViewMenuItems();
  menu_builder_.EndMenu();
}

void MenuOrchestrator::AddViewMenuItems() {
  // Editor Selection
  menu_builder_
      .Item("Editor Selection", ICON_MD_DASHBOARD,
            [this]() { OnShowEditorSelection(); }, "Ctrl+E")
      .Separator();
  
  // Individual Editor Shortcuts
  menu_builder_
      .Item("Overworld", ICON_MD_MAP,
            [this]() { OnSwitchToEditor(EditorType::kOverworld); }, "Ctrl+1")
      .Item("Dungeon", ICON_MD_CASTLE,
            [this]() { OnSwitchToEditor(EditorType::kDungeon); }, "Ctrl+2")
      .Item("Graphics", ICON_MD_IMAGE,
            [this]() { OnSwitchToEditor(EditorType::kGraphics); }, "Ctrl+3")
      .Item("Sprites", ICON_MD_TOYS,
            [this]() { OnSwitchToEditor(EditorType::kSprite); }, "Ctrl+4")
      .Item("Messages", ICON_MD_CHAT_BUBBLE,
            [this]() { OnSwitchToEditor(EditorType::kMessage); }, "Ctrl+5")
      .Item("Music", ICON_MD_MUSIC_NOTE,
            [this]() { OnSwitchToEditor(EditorType::kMusic); }, "Ctrl+6")
      .Item("Palettes", ICON_MD_PALETTE,
            [this]() { OnSwitchToEditor(EditorType::kPalette); }, "Ctrl+7")
      .Item("Screens", ICON_MD_TV,
            [this]() { OnSwitchToEditor(EditorType::kScreen); }, "Ctrl+8")
      .Item("Hex Editor", ICON_MD_DATA_ARRAY,
            [this]() { OnSwitchToEditor(EditorType::kHex); }, "Ctrl+9")
      .Item("Assembly", ICON_MD_CODE,
            [this]() { OnSwitchToEditor(EditorType::kAssembly); }, "Ctrl+0")
      .Separator();
  
  // Display Settings
  menu_builder_
      .Item("Display Settings", ICON_MD_DISPLAY_SETTINGS,
            [this]() { OnShowDisplaySettings(); });
}

void MenuOrchestrator::BuildToolsMenu() {
  menu_builder_.BeginMenu("Tools");
  AddToolsMenuItems();
  menu_builder_.EndMenu();
}

void MenuOrchestrator::AddToolsMenuItems() {
  // Development Tools
  menu_builder_
      .Item("Global Search", ICON_MD_SEARCH,
            [this]() { OnShowGlobalSearch(); }, "Ctrl+Shift+F")
      .Item("Performance Dashboard", ICON_MD_SPEED,
            [this]() { OnShowPerformanceDashboard(); })
      .Separator();
  
  // Debug Tools
  menu_builder_
      .Item("ImGui Demo", ICON_MD_BUG_REPORT,
            [this]() { OnShowImGuiDemo(); })
      .Item("ImGui Metrics", ICON_MD_ANALYTICS,
            [this]() { OnShowImGuiMetrics(); });
}

void MenuOrchestrator::BuildWindowMenu() {
  menu_builder_.BeginMenu("Window");
  AddWindowMenuItems();
  menu_builder_.EndMenu();
}

void MenuOrchestrator::AddWindowMenuItems() {
  // Window Management
  menu_builder_
      .Item("Show All Windows", ICON_MD_VISIBILITY,
            [this]() { OnShowAllWindows(); })
      .Item("Hide All Windows", ICON_MD_VISIBILITY_OFF,
            [this]() { OnHideAllWindows(); })
      .Separator();
  
  // Layout Management
  menu_builder_
      .Item("Reset Layout", ICON_MD_RESTORE,
            [this]() { OnResetWorkspaceLayout(); })
      .Item("Save Layout", ICON_MD_SAVE,
            [this]() { OnSaveWorkspaceLayout(); })
      .Item("Load Layout", ICON_MD_FOLDER_OPEN,
            [this]() { OnLoadWorkspaceLayout(); });
  
  // Session Management (if multiple sessions)
  if (HasMultipleSessions()) {
    menu_builder_
        .Separator()
        .Item("New Session", ICON_MD_ADD,
              [this]() { OnCreateNewSession(); })
        .Item("Duplicate Session", ICON_MD_CONTENT_COPY,
              [this]() { OnDuplicateCurrentSession(); })
        .Item("Close Session", ICON_MD_CLOSE,
              [this]() { OnCloseCurrentSession(); });
  }
}

void MenuOrchestrator::BuildHelpMenu() {
  menu_builder_.BeginMenu("Help");
  AddHelpMenuItems();
  menu_builder_.EndMenu();
}

void MenuOrchestrator::AddHelpMenuItems() {
  menu_builder_
      .Item("User Guide", ICON_MD_HELP,
            [this]() { OnShowUserGuide(); })
      .Item("Keyboard Shortcuts", ICON_MD_KEYBOARD,
            [this]() { OnShowKeyboardShortcuts(); })
      .Separator()
      .Item("About", ICON_MD_INFO,
            [this]() { OnShowAbout(); });
}

// Menu state management
void MenuOrchestrator::ClearMenu() {
  menu_builder_.Clear();
}

void MenuOrchestrator::RefreshMenu() {
  menu_needs_refresh_ = true;
}

// Menu item callbacks - delegate to appropriate managers
void MenuOrchestrator::OnOpenRom() {
  auto status = rom_manager_.LoadRom();
  if (!status.ok()) {
    toast_manager_.Show(
        absl::StrFormat("Failed to load ROM: %s", status.message()),
        ToastType::kError);
  }
}

void MenuOrchestrator::OnSaveRom() {
  auto status = rom_manager_.SaveRom();
  if (!status.ok()) {
    toast_manager_.Show(
        absl::StrFormat("Failed to save ROM: %s", status.message()),
        ToastType::kError);
  }
}

void MenuOrchestrator::OnSaveRomAs() {
  // TODO: Show save dialog and delegate to rom_manager_
  toast_manager_.Show("Save ROM As", ToastType::kInfo);
}

void MenuOrchestrator::OnCreateProject() {
  auto status = project_manager_.CreateNewProject();
  if (!status.ok()) {
    toast_manager_.Show(
        absl::StrFormat("Failed to create project: %s", status.message()),
        ToastType::kError);
  }
}

void MenuOrchestrator::OnOpenProject() {
  auto status = project_manager_.OpenProject();
  if (!status.ok()) {
    toast_manager_.Show(
        absl::StrFormat("Failed to open project: %s", status.message()),
        ToastType::kError);
  }
}

void MenuOrchestrator::OnSaveProject() {
  auto status = project_manager_.SaveProject();
  if (!status.ok()) {
    toast_manager_.Show(
        absl::StrFormat("Failed to save project: %s", status.message()),
        ToastType::kError);
  }
}

void MenuOrchestrator::OnSaveProjectAs() {
  auto status = project_manager_.SaveProjectAs();
  if (!status.ok()) {
    toast_manager_.Show(
        absl::StrFormat("Failed to save project as: %s", status.message()),
        ToastType::kError);
  }
}

// Editor-specific menu actions
void MenuOrchestrator::OnSwitchToEditor(EditorType editor_type) {
  editor_registry_.SwitchToEditor(editor_type);
  toast_manager_.Show(
      absl::StrFormat("Switched to %s", 
                      editor_registry_.GetEditorDisplayName(editor_type)),
      ToastType::kInfo);
}

void MenuOrchestrator::OnShowEditorSelection() {
  // TODO: Show editor selection dialog
  toast_manager_.Show("Editor Selection", ToastType::kInfo);
}

void MenuOrchestrator::OnShowDisplaySettings() {
  // TODO: Show display settings dialog
  toast_manager_.Show("Display Settings", ToastType::kInfo);
}

// Session management menu actions
void MenuOrchestrator::OnCreateNewSession() {
  session_coordinator_.CreateNewSession();
}

void MenuOrchestrator::OnDuplicateCurrentSession() {
  session_coordinator_.DuplicateCurrentSession();
}

void MenuOrchestrator::OnCloseCurrentSession() {
  session_coordinator_.CloseCurrentSession();
}

void MenuOrchestrator::OnSwitchToSession(size_t session_index) {
  session_coordinator_.SwitchToSession(session_index);
}

// Window management menu actions
void MenuOrchestrator::OnShowAllWindows() {
  // TODO: Delegate to WindowDelegate
  toast_manager_.Show("Show All Windows", ToastType::kInfo);
}

void MenuOrchestrator::OnHideAllWindows() {
  // TODO: Delegate to WindowDelegate
  toast_manager_.Show("Hide All Windows", ToastType::kInfo);
}

void MenuOrchestrator::OnResetWorkspaceLayout() {
  // TODO: Delegate to WindowDelegate
  toast_manager_.Show("Reset Workspace Layout", ToastType::kInfo);
}

void MenuOrchestrator::OnSaveWorkspaceLayout() {
  // TODO: Delegate to WindowDelegate
  toast_manager_.Show("Save Workspace Layout", ToastType::kInfo);
}

void MenuOrchestrator::OnLoadWorkspaceLayout() {
  // TODO: Delegate to WindowDelegate
  toast_manager_.Show("Load Workspace Layout", ToastType::kInfo);
}

// Tool menu actions
void MenuOrchestrator::OnShowGlobalSearch() {
  // TODO: Show global search dialog
  toast_manager_.Show("Global Search", ToastType::kInfo);
}

void MenuOrchestrator::OnShowPerformanceDashboard() {
  // TODO: Show performance dashboard
  toast_manager_.Show("Performance Dashboard", ToastType::kInfo);
}

void MenuOrchestrator::OnShowImGuiDemo() {
  // TODO: Show ImGui demo
  toast_manager_.Show("ImGui Demo", ToastType::kInfo);
}

void MenuOrchestrator::OnShowImGuiMetrics() {
  // TODO: Show ImGui metrics
  toast_manager_.Show("ImGui Metrics", ToastType::kInfo);
}

// Help menu actions
void MenuOrchestrator::OnShowAbout() {
  // TODO: Show about dialog
  toast_manager_.Show("About YAZE", ToastType::kInfo);
}

void MenuOrchestrator::OnShowKeyboardShortcuts() {
  // TODO: Show keyboard shortcuts dialog
  toast_manager_.Show("Keyboard Shortcuts", ToastType::kInfo);
}

void MenuOrchestrator::OnShowUserGuide() {
  // TODO: Show user guide
  toast_manager_.Show("User Guide", ToastType::kInfo);
}

// Menu item validation helpers
bool MenuOrchestrator::CanSaveRom() const {
  return rom_manager_.IsRomLoaded();
}

bool MenuOrchestrator::CanSaveProject() const {
  return project_manager_.HasActiveProject();
}

bool MenuOrchestrator::HasActiveRom() const {
  return rom_manager_.IsRomLoaded();
}

bool MenuOrchestrator::HasActiveProject() const {
  return project_manager_.HasActiveProject();
}

bool MenuOrchestrator::HasCurrentEditor() const {
  // TODO: Check if there's a current active editor
  return true;  // Placeholder
}

bool MenuOrchestrator::HasMultipleSessions() const {
  return session_coordinator_.HasMultipleSessions();
}

// Menu item text generation
std::string MenuOrchestrator::GetRomFilename() const {
  return rom_manager_.GetRomFilename();
}

std::string MenuOrchestrator::GetProjectName() const {
  return project_manager_.GetProjectName();
}

std::string MenuOrchestrator::GetCurrentEditorName() const {
  // TODO: Get current editor name
  return "Unknown Editor";
}

// Shortcut key management
std::string MenuOrchestrator::GetShortcutForAction(const std::string& action) const {
  // TODO: Implement shortcut mapping
  return "";
}

void MenuOrchestrator::RegisterGlobalShortcuts() {
  // TODO: Register global keyboard shortcuts
}

}  // namespace editor
}  // namespace yaze
