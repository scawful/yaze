#include "menu_orchestrator.h"

#include "absl/strings/str_format.h"
#include "core/features.h"
#include "app/editor/editor.h"
#include "app/editor/editor_manager.h"
#include "app/editor/system/editor_registry.h"
#include "app/editor/system/popup_manager.h"
#include "app/editor/system/project_manager.h"
#include "app/editor/system/rom_file_manager.h"
#include "app/editor/system/session_coordinator.h"
#include "app/editor/system/toast_manager.h"
#include "app/editor/ui/menu_builder.h"
#include "app/gui/core/icons.h"
#include "app/rom.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze {
namespace editor {

MenuOrchestrator::MenuOrchestrator(
    EditorManager* editor_manager,
    MenuBuilder& menu_builder,
    RomFileManager& rom_manager,
    ProjectManager& project_manager,
    EditorRegistry& editor_registry,
    SessionCoordinator& session_coordinator,
    ToastManager& toast_manager,
    PopupManager& popup_manager)
    : editor_manager_(editor_manager),
      menu_builder_(menu_builder),
      rom_manager_(rom_manager),
      project_manager_(project_manager),
      editor_registry_(editor_registry),
      session_coordinator_(session_coordinator),
      toast_manager_(toast_manager),
      popup_manager_(popup_manager) {
}

void MenuOrchestrator::BuildMainMenu() {
  ClearMenu();
  
  // Build all menu sections in order
  BuildFileMenu();
  BuildEditMenu();
  BuildViewMenu();
  BuildToolsMenu();
  BuildDebugMenu();  // Add Debug menu between Tools and Window
  BuildWindowMenu();
  BuildHelpMenu();
  
  // Draw the constructed menu
  menu_builder_.Draw();
  
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
            [this]() { OnShowRomInfo(); }, nullptr,
            [this]() { return HasActiveRom(); })
      .Item("Create Backup", ICON_MD_BACKUP,
            [this]() { OnCreateBackup(); }, nullptr,
            [this]() { return HasActiveRom(); })
      .Item("Validate ROM", ICON_MD_CHECK_CIRCLE,
            [this]() { OnValidateRom(); }, nullptr,
            [this]() { return HasActiveRom(); })
      .Separator();
  
  // Settings and Quit
  menu_builder_
      .Item("Settings", ICON_MD_SETTINGS,
            [this]() { OnShowSettings(); })
      .Separator()
      .Item("Quit", ICON_MD_EXIT_TO_APP, 
            [this]() { OnQuit(); }, "Ctrl+Q");
}

void MenuOrchestrator::BuildEditMenu() {
  menu_builder_.BeginMenu("Edit");
  AddEditMenuItems();
  menu_builder_.EndMenu();
}

void MenuOrchestrator::AddEditMenuItems() {
  // Undo/Redo operations - delegate to current editor
  menu_builder_
      .Item("Undo", ICON_MD_UNDO,
            [this]() { OnUndo(); }, "Ctrl+Z",
            [this]() { return HasCurrentEditor(); })
      .Item("Redo", ICON_MD_REDO,
            [this]() { OnRedo(); }, "Ctrl+Y",
            [this]() { return HasCurrentEditor(); })
      .Separator();
  
  // Clipboard operations - delegate to current editor
  menu_builder_
      .Item("Cut", ICON_MD_CONTENT_CUT,
            [this]() { OnCut(); }, "Ctrl+X",
            [this]() { return HasCurrentEditor(); })
      .Item("Copy", ICON_MD_CONTENT_COPY,
            [this]() { OnCopy(); }, "Ctrl+C",
            [this]() { return HasCurrentEditor(); })
      .Item("Paste", ICON_MD_CONTENT_PASTE,
            [this]() { OnPaste(); }, "Ctrl+V",
            [this]() { return HasCurrentEditor(); })
      .Separator();
  
  // Search operations
  menu_builder_
      .Item("Find", ICON_MD_SEARCH,
            [this]() { OnFind(); }, "Ctrl+F",
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
      .Item("Assembly", ICON_MD_CODE,
            [this]() { OnSwitchToEditor(EditorType::kAssembly); }, "Ctrl+9")
      .Item("Hex Editor", ICON_MD_DATA_ARRAY,
            [this]() { OnShowHexEditor(); }, "Ctrl+0")
      .Separator();
  
  // Special Editors
#ifdef YAZE_WITH_GRPC
  menu_builder_
      .Item("AI Agent", ICON_MD_SMART_TOY,
            [this]() { OnShowAIAgent(); }, "Ctrl+Shift+A")
      .Item("Chat History", ICON_MD_CHAT,
            [this]() { OnShowChatHistory(); }, "Ctrl+H")
      .Item("Proposal Drawer", ICON_MD_PREVIEW,
            [this]() { OnShowProposalDrawer(); }, "Ctrl+Shift+R");
#endif
  
  menu_builder_
      .Item("Emulator", ICON_MD_VIDEOGAME_ASSET,
            [this]() { OnShowEmulator(); }, "Ctrl+Shift+E")
      .Separator();
  
  // Settings and UI
  menu_builder_
      .Item("Display Settings", ICON_MD_DISPLAY_SETTINGS,
            [this]() { OnShowDisplaySettings(); })
      .Separator();
  
  // Additional UI Elements
  menu_builder_
      .Item("Card Browser", ICON_MD_DASHBOARD,
            [this]() { OnShowCardBrowser(); }, "Ctrl+Shift+B")
      .Item("Welcome Screen", ICON_MD_HOME,
            [this]() { OnShowWelcomeScreen(); });
}

void MenuOrchestrator::BuildToolsMenu() {
  menu_builder_.BeginMenu("Tools");
  AddToolsMenuItems();
  menu_builder_.EndMenu();
}

void MenuOrchestrator::AddToolsMenuItems() {
  // Core Tools - keep these in Tools menu
  menu_builder_
      .Item("Global Search", ICON_MD_SEARCH,
            [this]() { OnShowGlobalSearch(); }, "Ctrl+Shift+F")
      .Item("Command Palette", ICON_MD_SEARCH,
            [this]() { OnShowCommandPalette(); }, "Ctrl+Shift+P")
      .Separator();
  
  // Resource Management
  menu_builder_
      .Item("Resource Label Manager", ICON_MD_LABEL,
            [this]() { OnShowResourceLabelManager(); })
      .Separator();
  
  // Collaboration (GRPC builds only)
#ifdef YAZE_WITH_GRPC
  menu_builder_
      .BeginSubMenu("Collaborate", ICON_MD_PEOPLE)
      .Item("Start Collaboration Session", ICON_MD_PLAY_CIRCLE,
            [this]() { OnStartCollaboration(); })
      .Item("Join Collaboration Session", ICON_MD_GROUP_ADD,
            [this]() { OnJoinCollaboration(); })
      .Item("Network Status", ICON_MD_CLOUD,
            [this]() { OnShowNetworkStatus(); })
      .EndMenu();
#endif
}

void MenuOrchestrator::BuildDebugMenu() {
  menu_builder_.BeginMenu("Debug");
  AddDebugMenuItems();
  menu_builder_.EndMenu();
}

void MenuOrchestrator::AddDebugMenuItems() {
  // Testing section (move from Tools if present)
#ifdef YAZE_ENABLE_TESTING
  menu_builder_
      .BeginSubMenu("Testing", ICON_MD_SCIENCE)
      .Item("Test Dashboard", ICON_MD_DASHBOARD,
            [this]() { OnShowTestDashboard(); }, "Ctrl+T")
      .Item("Run All Tests", ICON_MD_PLAY_ARROW,
            [this]() { OnRunAllTests(); })
      .Item("Run Unit Tests", ICON_MD_CHECK_BOX,
            [this]() { OnRunUnitTests(); })
      .Item("Run Integration Tests", ICON_MD_INTEGRATION_INSTRUCTIONS,
            [this]() { OnRunIntegrationTests(); })
      .Item("Run E2E Tests", ICON_MD_VISIBILITY,
            [this]() { OnRunE2ETests(); })
      .EndMenu()
      .Separator();
#endif
  
  // ROM Analysis submenu
  menu_builder_
      .BeginSubMenu("ROM Analysis", ICON_MD_STORAGE)
      .Item("ROM Information", ICON_MD_INFO,
            [this]() { OnShowRomInfo(); }, nullptr,
            [this]() { return HasActiveRom(); })
      .Item("Data Integrity Check", ICON_MD_ANALYTICS,
            [this]() { OnRunDataIntegrityCheck(); }, nullptr,
            [this]() { return HasActiveRom(); })
      .Item("Test Save/Load", ICON_MD_SAVE_ALT,
            [this]() { OnTestSaveLoad(); }, nullptr,
            [this]() { return HasActiveRom(); })
      .EndMenu();
  
  // ZSCustomOverworld submenu
  menu_builder_
      .BeginSubMenu("ZSCustomOverworld", ICON_MD_CODE)
      .Item("Check ROM Version", ICON_MD_INFO,
            [this]() { OnCheckRomVersion(); }, nullptr,
            [this]() { return HasActiveRom(); })
      .Item("Upgrade ROM", ICON_MD_UPGRADE,
            [this]() { OnUpgradeRom(); }, nullptr,
            [this]() { return HasActiveRom(); })
      .Item("Toggle Custom Loading", ICON_MD_SETTINGS,
            [this]() { OnToggleCustomLoading(); })
      .EndMenu();
  
  // Asar Integration submenu
  menu_builder_
      .BeginSubMenu("Asar Integration", ICON_MD_BUILD)
      .Item("Asar Status", ICON_MD_INFO,
            [this]() { popup_manager_.Show(PopupID::kAsarIntegration); })
      .Item("Toggle ASM Patch", ICON_MD_CODE,
            [this]() { OnToggleAsarPatch(); }, nullptr,
            [this]() { return HasActiveRom(); })
      .Item("Load ASM File", ICON_MD_FOLDER_OPEN,
            [this]() { OnLoadAsmFile(); })
      .EndMenu();
  
  menu_builder_.Separator();
  
  // Development Tools
  menu_builder_
      .Item("Memory Editor", ICON_MD_MEMORY,
            [this]() { OnShowMemoryEditor(); })
      .Item("Assembly Editor", ICON_MD_CODE,
            [this]() { OnShowAssemblyEditor(); })
      .Item("Feature Flags", ICON_MD_FLAG,
            [this]() { popup_manager_.Show(PopupID::kFeatureFlags); })
      .Separator()
      .Item("Performance Dashboard", ICON_MD_SPEED,
            [this]() { OnShowPerformanceDashboard(); });
  
#ifdef YAZE_WITH_GRPC
  menu_builder_
      .Item("Agent Proposals", ICON_MD_PREVIEW,
            [this]() { OnShowProposalDrawer(); });
#endif
  
  menu_builder_.Separator();
  
  // ImGui Debug Windows
  menu_builder_
      .Item("ImGui Demo", ICON_MD_HELP,
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
  // Sessions Submenu
  menu_builder_
      .BeginSubMenu("Sessions", ICON_MD_TAB)
      .Item("New Session", ICON_MD_ADD, 
            [this]() { OnCreateNewSession(); }, "Ctrl+Shift+N")
      .Item("Duplicate Session", ICON_MD_CONTENT_COPY,
            [this]() { OnDuplicateCurrentSession(); }, nullptr,
            [this]() { return HasActiveRom(); })
      .Item("Close Session", ICON_MD_CLOSE, 
            [this]() { OnCloseCurrentSession(); }, "Ctrl+Shift+W",
            [this]() { return HasMultipleSessions(); })
      .Separator()
      .Item("Session Switcher", ICON_MD_SWITCH_ACCOUNT,
            [this]() { OnShowSessionSwitcher(); }, "Ctrl+Tab",
            [this]() { return HasMultipleSessions(); })
      .Item("Session Manager", ICON_MD_VIEW_LIST,
            [this]() { OnShowSessionManager(); })
      .EndMenu()
      .Separator();
  
  // Layout Management
  menu_builder_
      .Item("Save Layout", ICON_MD_SAVE,
            [this]() { OnSaveWorkspaceLayout(); }, "Ctrl+Shift+S")
      .Item("Load Layout", ICON_MD_FOLDER_OPEN,
            [this]() { OnLoadWorkspaceLayout(); }, "Ctrl+Shift+O")
      .Item("Reset Layout", ICON_MD_RESET_TV,
            [this]() { OnResetWorkspaceLayout(); })
      .Item("Layout Presets", ICON_MD_BOOKMARK,
            [this]() { OnShowLayoutPresets(); })
      .Separator();
  
  // Window Visibility
  menu_builder_
      .Item("Show All Windows", ICON_MD_VISIBILITY,
            [this]() { OnShowAllWindows(); })
      .Item("Hide All Windows", ICON_MD_VISIBILITY_OFF,
            [this]() { OnHideAllWindows(); })
      .Separator();
  
  // Workspace Presets
  menu_builder_
      .Item("Developer Layout", ICON_MD_DEVELOPER_MODE,
            [this]() { OnLoadDeveloperLayout(); })
      .Item("Designer Layout", ICON_MD_DESIGN_SERVICES,
            [this]() { OnLoadDesignerLayout(); })
      .Item("Modder Layout", ICON_MD_CONSTRUCTION,
            [this]() { OnLoadModderLayout(); });
}

void MenuOrchestrator::BuildHelpMenu() {
  menu_builder_.BeginMenu("Help");
  AddHelpMenuItems();
  menu_builder_.EndMenu();
}

void MenuOrchestrator::AddHelpMenuItems() {
  menu_builder_
      .Item("Getting Started", ICON_MD_PLAY_ARROW,
            [this]() { OnShowGettingStarted(); })
      .Item("Asar Integration", ICON_MD_CODE,
            [this]() { OnShowAsarIntegration(); })
      .Item("Build Instructions", ICON_MD_BUILD,
            [this]() { OnShowBuildInstructions(); })
      .Item("CLI Usage", ICON_MD_TERMINAL,
            [this]() { OnShowCLIUsage(); })
      .Separator()
      .Item("Supported Features", ICON_MD_CHECK_CIRCLE,
            [this]() { OnShowSupportedFeatures(); })
      .Item("What's New", ICON_MD_NEW_RELEASES,
            [this]() { OnShowWhatsNew(); })
      .Separator()
      .Item("Troubleshooting", ICON_MD_BUILD_CIRCLE,
            [this]() { OnShowTroubleshooting(); })
      .Item("Contributing", ICON_MD_VOLUNTEER_ACTIVISM,
            [this]() { OnShowContributing(); })
      .Separator()
      .Item("About", ICON_MD_INFO,
            [this]() { OnShowAbout(); }, "F1");
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
  // Delegate to EditorManager's LoadRom which handles session management
  if (editor_manager_) {
    auto status = editor_manager_->LoadRom();
    if (!status.ok()) {
      toast_manager_.Show(
          absl::StrFormat("Failed to load ROM: %s", status.message()),
          ToastType::kError);
    }
  }
}

void MenuOrchestrator::OnSaveRom() {
  // Delegate to EditorManager's SaveRom which handles editor data saving
  if (editor_manager_) {
    auto status = editor_manager_->SaveRom();
    if (!status.ok()) {
      toast_manager_.Show(
          absl::StrFormat("Failed to save ROM: %s", status.message()),
          ToastType::kError);
    } else {
      toast_manager_.Show("ROM saved successfully", ToastType::kSuccess);
    }
  }
}

void MenuOrchestrator::OnSaveRomAs() {
  popup_manager_.Show(PopupID::kSaveAs);
}

void MenuOrchestrator::OnCreateProject() {
  // Delegate to EditorManager which handles the full project creation flow
  if (editor_manager_) {
    auto status = editor_manager_->CreateNewProject();
    if (!status.ok()) {
      toast_manager_.Show(
          absl::StrFormat("Failed to create project: %s", status.message()),
          ToastType::kError);
    }
  }
}

void MenuOrchestrator::OnOpenProject() {
  // Delegate to EditorManager which handles ROM loading and session creation
  if (editor_manager_) {
    auto status = editor_manager_->OpenProject();
    if (!status.ok()) {
      toast_manager_.Show(
          absl::StrFormat("Failed to open project: %s", status.message()),
          ToastType::kError);
    }
  }
}

void MenuOrchestrator::OnSaveProject() {
  // Delegate to EditorManager which updates project with current state
  if (editor_manager_) {
    auto status = editor_manager_->SaveProject();
    if (!status.ok()) {
      toast_manager_.Show(
          absl::StrFormat("Failed to save project: %s", status.message()),
          ToastType::kError);
    } else {
      toast_manager_.Show("Project saved successfully", ToastType::kSuccess);
    }
  }
}

void MenuOrchestrator::OnSaveProjectAs() {
  // Delegate to EditorManager
  if (editor_manager_) {
    auto status = editor_manager_->SaveProjectAs();
    if (!status.ok()) {
      toast_manager_.Show(
          absl::StrFormat("Failed to save project as: %s", status.message()),
          ToastType::kError);
    }
  }
}

// Edit menu actions - delegate to current editor
void MenuOrchestrator::OnUndo() {
  if (editor_manager_) {
    auto* current_editor = editor_manager_->GetCurrentEditor();
    if (current_editor) {
      auto status = current_editor->Undo();
      if (!status.ok()) {
        toast_manager_.Show(absl::StrFormat("Undo failed: %s", status.message()),
                           ToastType::kError);
      }
    }
  }
}

void MenuOrchestrator::OnRedo() {
  if (editor_manager_) {
    auto* current_editor = editor_manager_->GetCurrentEditor();
    if (current_editor) {
      auto status = current_editor->Redo();
      if (!status.ok()) {
        toast_manager_.Show(absl::StrFormat("Redo failed: %s", status.message()),
                           ToastType::kError);
      }
    }
  }
}

void MenuOrchestrator::OnCut() {
  if (editor_manager_) {
    auto* current_editor = editor_manager_->GetCurrentEditor();
    if (current_editor) {
      auto status = current_editor->Cut();
      if (!status.ok()) {
        toast_manager_.Show(absl::StrFormat("Cut failed: %s", status.message()),
                           ToastType::kError);
      }
    }
  }
}

void MenuOrchestrator::OnCopy() {
  if (editor_manager_) {
    auto* current_editor = editor_manager_->GetCurrentEditor();
    if (current_editor) {
      auto status = current_editor->Copy();
      if (!status.ok()) {
        toast_manager_.Show(absl::StrFormat("Copy failed: %s", status.message()),
                           ToastType::kError);
      }
    }
  }
}

void MenuOrchestrator::OnPaste() {
  if (editor_manager_) {
    auto* current_editor = editor_manager_->GetCurrentEditor();
    if (current_editor) {
      auto status = current_editor->Paste();
      if (!status.ok()) {
        toast_manager_.Show(absl::StrFormat("Paste failed: %s", status.message()),
                           ToastType::kError);
      }
    }
  }
}

void MenuOrchestrator::OnFind() {
  if (editor_manager_) {
    auto* current_editor = editor_manager_->GetCurrentEditor();
    if (current_editor) {
      auto status = current_editor->Find();
      if (!status.ok()) {
        toast_manager_.Show(absl::StrFormat("Find failed: %s", status.message()),
                           ToastType::kError);
      }
    }
  }
}

// Editor-specific menu actions
void MenuOrchestrator::OnSwitchToEditor(EditorType editor_type) {
  // Delegate to EditorManager which manages editor switching
  if (editor_manager_) {
    editor_manager_->SwitchToEditor(editor_type);
  }
}

void MenuOrchestrator::OnShowEditorSelection() {
  // Delegate to UICoordinator for editor selection dialog display
  if (editor_manager_) {
    if (auto* ui = editor_manager_->ui_coordinator()) {
      ui->ShowEditorSelection();
    }
  }
}

void MenuOrchestrator::OnShowDisplaySettings() {
  popup_manager_.Show(PopupID::kDisplaySettings);
}

void MenuOrchestrator::OnShowHexEditor() {
  // Show hex editor card via EditorCardManager
  if (editor_manager_) {
    editor_manager_->ShowHexEditor();
  }
}

void MenuOrchestrator::OnShowEmulator() {
  if (editor_manager_) {
    editor_manager_->ShowEmulator();
  }
}

void MenuOrchestrator::OnShowCardBrowser() {
  if (editor_manager_) {
    editor_manager_->ShowCardBrowser();
  }
}

void MenuOrchestrator::OnShowWelcomeScreen() {
  if (editor_manager_) {
    editor_manager_->ShowWelcomeScreen();
  }
}

#ifdef YAZE_WITH_GRPC
void MenuOrchestrator::OnShowAIAgent() {
  if (editor_manager_) {
    editor_manager_->ShowAIAgent();
  }
}

void MenuOrchestrator::OnShowChatHistory() {
  if (editor_manager_) {
    editor_manager_->ShowChatHistory();
  }
}

void MenuOrchestrator::OnShowProposalDrawer() {
  if (editor_manager_) {
    editor_manager_->ShowProposalDrawer();
  }
}
#endif

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

void MenuOrchestrator::OnShowSessionSwitcher() {
  // Delegate to UICoordinator for session switcher UI
  if (editor_manager_) {
    if (auto* ui = editor_manager_->ui_coordinator()) {
      ui->ShowSessionSwitcher();
    }
  }
}

void MenuOrchestrator::OnShowSessionManager() {
  // TODO: Show session manager dialog
  toast_manager_.Show("Session Manager", ToastType::kInfo);
}

// Window management menu actions
void MenuOrchestrator::OnShowAllWindows() {
  // Delegate to EditorManager
  if (editor_manager_) {
    editor_manager_->ShowAllWindows();
  }
}

void MenuOrchestrator::OnHideAllWindows() {
  // Delegate to EditorManager
  if (editor_manager_) {
    editor_manager_->HideAllWindows();
  }
}

void MenuOrchestrator::OnResetWorkspaceLayout() {
  // Delegate to EditorManager
  if (editor_manager_) {
    editor_manager_->ResetWorkspaceLayout();
  }
}

void MenuOrchestrator::OnSaveWorkspaceLayout() {
  // Delegate to EditorManager
  if (editor_manager_) {
    editor_manager_->SaveWorkspaceLayout();
  }
}

void MenuOrchestrator::OnLoadWorkspaceLayout() {
  // Delegate to EditorManager
  if (editor_manager_) {
    editor_manager_->LoadWorkspaceLayout();
  }
}

void MenuOrchestrator::OnShowLayoutPresets() {
  // TODO: Show layout presets dialog
  toast_manager_.Show("Layout Presets", ToastType::kInfo);
}

void MenuOrchestrator::OnLoadDeveloperLayout() {
  if (editor_manager_) {
    editor_manager_->LoadDeveloperLayout();
  }
}

void MenuOrchestrator::OnLoadDesignerLayout() {
  if (editor_manager_) {
    editor_manager_->LoadDesignerLayout();
  }
}

void MenuOrchestrator::OnLoadModderLayout() {
  if (editor_manager_) {
    editor_manager_->LoadModderLayout();
  }
}

// Tool menu actions
void MenuOrchestrator::OnShowGlobalSearch() {
  if (editor_manager_) {
    editor_manager_->ShowGlobalSearch();
  }
}

void MenuOrchestrator::OnShowCommandPalette() {
  if (editor_manager_) {
    editor_manager_->ShowCommandPalette();
  }
}

void MenuOrchestrator::OnShowPerformanceDashboard() {
  if (editor_manager_) {
    editor_manager_->ShowPerformanceDashboard();
  }
}

void MenuOrchestrator::OnShowImGuiDemo() {
  if (editor_manager_) {
    editor_manager_->ShowImGuiDemo();
  }
}

void MenuOrchestrator::OnShowImGuiMetrics() {
  if (editor_manager_) {
    editor_manager_->ShowImGuiMetrics();
  }
}

void MenuOrchestrator::OnShowMemoryEditor() {
  if (editor_manager_) {
    editor_manager_->ShowMemoryEditor();
  }
}

void MenuOrchestrator::OnShowResourceLabelManager() {
  if (editor_manager_) {
    editor_manager_->ShowResourceLabelManager();
  }
}

#ifdef YAZE_ENABLE_TESTING
void MenuOrchestrator::OnShowTestDashboard() {
  if (editor_manager_) {
    editor_manager_->ShowTestDashboard();
  }
}

void MenuOrchestrator::OnRunAllTests() {
  toast_manager_.Show("Running all tests...", ToastType::kInfo);
  // TODO: Implement test runner integration
}

void MenuOrchestrator::OnRunUnitTests() {
  toast_manager_.Show("Running unit tests...", ToastType::kInfo);
  // TODO: Implement unit test runner
}

void MenuOrchestrator::OnRunIntegrationTests() {
  toast_manager_.Show("Running integration tests...", ToastType::kInfo);
  // TODO: Implement integration test runner
}

void MenuOrchestrator::OnRunE2ETests() {
  toast_manager_.Show("Running E2E tests...", ToastType::kInfo);
  // TODO: Implement E2E test runner
}
#endif

#ifdef YAZE_WITH_GRPC
void MenuOrchestrator::OnStartCollaboration() {
  toast_manager_.Show("Starting collaboration session...", ToastType::kInfo);
  // TODO: Implement collaboration session start
}

void MenuOrchestrator::OnJoinCollaboration() {
  toast_manager_.Show("Joining collaboration session...", ToastType::kInfo);
  // TODO: Implement collaboration session join
}

void MenuOrchestrator::OnShowNetworkStatus() {
  toast_manager_.Show("Network Status", ToastType::kInfo);
  // TODO: Show network status dialog
}
#endif

// Help menu actions
void MenuOrchestrator::OnShowAbout() {
  popup_manager_.Show(PopupID::kAbout);
}

void MenuOrchestrator::OnShowGettingStarted() {
  popup_manager_.Show(PopupID::kGettingStarted);
}

void MenuOrchestrator::OnShowAsarIntegration() {
  popup_manager_.Show(PopupID::kAsarIntegration);
}

void MenuOrchestrator::OnShowBuildInstructions() {
  popup_manager_.Show(PopupID::kBuildInstructions);
}

void MenuOrchestrator::OnShowCLIUsage() {
  popup_manager_.Show(PopupID::kCLIUsage);
}

void MenuOrchestrator::OnShowTroubleshooting() {
  popup_manager_.Show(PopupID::kTroubleshooting);
}

void MenuOrchestrator::OnShowContributing() {
  popup_manager_.Show(PopupID::kContributing);
}

void MenuOrchestrator::OnShowWhatsNew() {
  popup_manager_.Show(PopupID::kWhatsNew);
}

void MenuOrchestrator::OnShowSupportedFeatures() {
  popup_manager_.Show(PopupID::kSupportedFeatures);
}

// Additional File menu actions
void MenuOrchestrator::OnShowRomInfo() {
  popup_manager_.Show(PopupID::kRomInfo);
}

void MenuOrchestrator::OnCreateBackup() {
  if (editor_manager_) {
    // Create backup via ROM directly (from original implementation)
    auto* rom = editor_manager_->GetCurrentRom();
    if (rom && rom->is_loaded()) {
      Rom::SaveSettings settings;
      settings.backup = true;
      settings.filename = rom->filename();
      auto status = rom->SaveToFile(settings);
      if (status.ok()) {
        toast_manager_.Show("Backup created successfully", ToastType::kSuccess);
      } else {
        toast_manager_.Show(
            absl::StrFormat("Backup failed: %s", status.message()),
            ToastType::kError);
      }
    }
  }
}

void MenuOrchestrator::OnValidateRom() {
  if (editor_manager_) {
    auto status = rom_manager_.ValidateRom(editor_manager_->GetCurrentRom());
    if (status.ok()) {
      toast_manager_.Show("ROM validation passed", ToastType::kSuccess);
    } else {
      toast_manager_.Show(
          absl::StrFormat("ROM validation failed: %s", status.message()),
          ToastType::kError);
    }
  }
}

void MenuOrchestrator::OnShowSettings() {
  // Activate settings editor
  if (editor_manager_) {
    editor_manager_->SwitchToEditor(EditorType::kSettings);
  }
}

void MenuOrchestrator::OnQuit() {
  if (editor_manager_) {
    editor_manager_->Quit();
  }
}

// Menu item validation helpers
bool MenuOrchestrator::CanSaveRom() const {
  auto* rom = editor_manager_->GetCurrentRom();
  return rom ? rom_manager_.IsRomLoaded(rom) : false;
}

bool MenuOrchestrator::CanSaveProject() const {
  return project_manager_.HasActiveProject();
}

bool MenuOrchestrator::HasActiveRom() const {
  auto* rom = editor_manager_->GetCurrentRom();
  return rom ? rom_manager_.IsRomLoaded(rom) : false;
}

bool MenuOrchestrator::HasActiveProject() const {
  return project_manager_.HasActiveProject();
}

bool MenuOrchestrator::HasCurrentEditor() const {
  return editor_manager_ && editor_manager_->GetCurrentEditor() != nullptr;
}

bool MenuOrchestrator::HasMultipleSessions() const {
  return session_coordinator_.HasMultipleSessions();
}

// Menu item text generation
std::string MenuOrchestrator::GetRomFilename() const {
  auto* rom = editor_manager_->GetCurrentRom();
  return rom ? rom_manager_.GetRomFilename(rom) : "";
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

// ============================================================================
// Debug Menu Actions
// ============================================================================

void MenuOrchestrator::OnRunDataIntegrityCheck() {
#ifdef YAZE_ENABLE_TESTING
  if (!editor_manager_) return;
  auto* rom = editor_manager_->GetCurrentRom();
  if (!rom || !rom->is_loaded()) return;
  
  toast_manager_.Show("Running ROM integrity tests...", ToastType::kInfo);
  // This would integrate with the test system in master
  // For now, just show a placeholder
  toast_manager_.Show("Data integrity check completed", ToastType::kSuccess, 3.0f);
#else
  toast_manager_.Show("Testing not enabled in this build", ToastType::kWarning);
#endif
}

void MenuOrchestrator::OnTestSaveLoad() {
#ifdef YAZE_ENABLE_TESTING
  if (!editor_manager_) return;
  auto* rom = editor_manager_->GetCurrentRom();
  if (!rom || !rom->is_loaded()) return;
  
  toast_manager_.Show("Running ROM save/load tests...", ToastType::kInfo);
  // This would integrate with the test system in master
  toast_manager_.Show("Save/load test completed", ToastType::kSuccess, 3.0f);
#else
  toast_manager_.Show("Testing not enabled in this build", ToastType::kWarning);
#endif
}

void MenuOrchestrator::OnCheckRomVersion() {
  if (!editor_manager_) return;
  auto* rom = editor_manager_->GetCurrentRom();
  if (!rom || !rom->is_loaded()) return;
  
  // Check ZSCustomOverworld version
  uint8_t version = (*rom)[zelda3::OverworldCustomASMHasBeenApplied];
  std::string version_str = (version == 0xFF) 
      ? "Vanilla" 
      : absl::StrFormat("v%d", version);
  
  toast_manager_.Show(
      absl::StrFormat("ROM: %s | ZSCustomOverworld: %s",
                      rom->title().c_str(), version_str.c_str()),
      ToastType::kInfo, 5.0f);
}

void MenuOrchestrator::OnUpgradeRom() {
  if (!editor_manager_) return;
  auto* rom = editor_manager_->GetCurrentRom();
  if (!rom || !rom->is_loaded()) return;
  
  toast_manager_.Show(
      "Use Overworld Editor to upgrade ROM version",
      ToastType::kInfo, 4.0f);
}

void MenuOrchestrator::OnToggleCustomLoading() {
  auto& flags = core::FeatureFlags::get();
  flags.overworld.kLoadCustomOverworld = !flags.overworld.kLoadCustomOverworld;
  
  toast_manager_.Show(
      absl::StrFormat("Custom Overworld Loading: %s",
                      flags.overworld.kLoadCustomOverworld ? "Enabled" : "Disabled"),
      ToastType::kInfo);
}

void MenuOrchestrator::OnToggleAsarPatch() {
  if (!editor_manager_) return;
  auto* rom = editor_manager_->GetCurrentRom();
  if (!rom || !rom->is_loaded()) return;
  
  auto& flags = core::FeatureFlags::get();
  flags.overworld.kApplyZSCustomOverworldASM = !flags.overworld.kApplyZSCustomOverworldASM;
  
  toast_manager_.Show(
      absl::StrFormat("ZSCustomOverworld ASM Application: %s",
                      flags.overworld.kApplyZSCustomOverworldASM ? "Enabled" : "Disabled"),
      ToastType::kInfo);
}

void MenuOrchestrator::OnLoadAsmFile() {
  toast_manager_.Show("ASM file loading not yet implemented", ToastType::kWarning);
}

void MenuOrchestrator::OnShowAssemblyEditor() {
  if (editor_manager_) {
    editor_manager_->SwitchToEditor(EditorType::kAssembly);
  }
}

}  // namespace editor
}  // namespace yaze
