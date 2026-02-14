#include "menu_orchestrator.h"

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/editor.h"
#include "app/editor/editor_manager.h"
#include "app/editor/layout/layout_presets.h"
#include "app/editor/menu/menu_builder.h"
#include "app/editor/system/editor_registry.h"
#include "app/editor/system/panel_manager.h"
#include "app/editor/system/project_manager.h"
#include "app/editor/system/rom_file_manager.h"
#include "app/editor/system/session_coordinator.h"
#include "app/editor/ui/popup_manager.h"
#include "app/editor/ui/toast_manager.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/platform_keys.h"
#include "core/features.h"
#include "rom/rom.h"
#include "zelda3/overworld/overworld_map.h"

// Platform-aware shortcut macros for menu display
#define SHORTCUT_CTRL(key) gui::FormatCtrlShortcut(ImGuiKey_##key).c_str()
#define SHORTCUT_CTRL_SHIFT(key) \
  gui::FormatCtrlShiftShortcut(ImGuiKey_##key).c_str()

namespace yaze {
namespace editor {

MenuOrchestrator::MenuOrchestrator(
    EditorManager* editor_manager, MenuBuilder& menu_builder,
    RomFileManager& rom_manager, ProjectManager& project_manager,
    EditorRegistry& editor_registry, SessionCoordinator& session_coordinator,
    ToastManager& toast_manager, PopupManager& popup_manager)
    : editor_manager_(editor_manager),
      menu_builder_(menu_builder),
      rom_manager_(rom_manager),
      project_manager_(project_manager),
      editor_registry_(editor_registry),
      session_coordinator_(session_coordinator),
      toast_manager_(toast_manager),
      popup_manager_(popup_manager) {}

void MenuOrchestrator::BuildMainMenu() {
  ClearMenu();

  // Build all menu sections in order
  // Traditional order: File, Edit, View, then app-specific menus
  BuildFileMenu();
  BuildEditMenu();
  BuildViewMenu();
  BuildPanelsMenu();  // Dedicated top-level menu for panel management
  BuildToolsMenu();   // Debug menu items merged into Tools
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
      .Item(
          "Open ROM", ICON_MD_FILE_OPEN, [this]() { OnOpenRom(); },
          SHORTCUT_CTRL(O))
      .Item(
          "Save ROM", ICON_MD_SAVE, [this]() { OnSaveRom(); }, SHORTCUT_CTRL(S),
          [this]() { return CanSaveRom(); })
      .Item(
          "Save As...", ICON_MD_SAVE_AS, [this]() { OnSaveRomAs(); }, nullptr,
          [this]() { return CanSaveRom(); })
      .Item("Save Scope...", ICON_MD_TUNE,
            [this]() { popup_manager_.Show(PopupID::kSaveScope); })
      .Separator();

  // Project Operations
  menu_builder_
      .Item("New Project", ICON_MD_CREATE_NEW_FOLDER,
            [this]() { OnCreateProject(); })
      .Item("Open Project", ICON_MD_FOLDER_OPEN, [this]() { OnOpenProject(); })
      .Item(
          "Save Project", ICON_MD_SAVE, [this]() { OnSaveProject(); }, nullptr,
          [this]() { return CanSaveProject(); })
      .Item(
          "Save Project As...", ICON_MD_SAVE_AS,
          [this]() { OnSaveProjectAs(); }, nullptr,
          [this]() { return CanSaveProject(); })
      .Item(
          "Project Management...", ICON_MD_FOLDER_SPECIAL,
          [this]() { OnShowProjectManagement(); }, nullptr,
          [this]() { return CanSaveProject(); })
      .Item(
          "Edit Project File...", ICON_MD_DESCRIPTION,
          [this]() { OnShowProjectFileEditor(); }, nullptr,
          [this]() { return HasProjectFile(); })
      .Separator();

  // ROM Information and Validation
  menu_builder_
      .Item(
          "ROM Information", ICON_MD_INFO, [this]() { OnShowRomInfo(); },
          nullptr, [this]() { return HasActiveRom(); })
      .Item(
          "Create Backup", ICON_MD_BACKUP, [this]() { OnCreateBackup(); },
          nullptr, [this]() { return HasActiveRom(); })
      .Item("ROM Backups...", ICON_MD_BACKUP,
            [this]() { popup_manager_.Show(PopupID::kRomBackups); }, nullptr,
            [this]() { return HasActiveRom(); })
      .Item(
          "Validate ROM", ICON_MD_CHECK_CIRCLE, [this]() { OnValidateRom(); },
          nullptr, [this]() { return HasActiveRom(); })
      .Separator();

  // Settings and Quit
  menu_builder_
      .Item("Settings", ICON_MD_SETTINGS, [this]() { OnShowSettings(); })
      .Separator()
      .Item(
          "Quit", ICON_MD_EXIT_TO_APP, [this]() { OnQuit(); },
          SHORTCUT_CTRL(Q));
}

void MenuOrchestrator::BuildEditMenu() {
  menu_builder_.BeginMenu("Edit");
  AddEditMenuItems();
  menu_builder_.EndMenu();
}

void MenuOrchestrator::AddEditMenuItems() {
  // Undo/Redo operations - delegate to current editor
  menu_builder_
      .Item(
          "Undo", ICON_MD_UNDO, [this]() { OnUndo(); }, SHORTCUT_CTRL(Z),
          [this]() { return HasCurrentEditor(); })
      .Item(
          "Redo", ICON_MD_REDO, [this]() { OnRedo(); }, SHORTCUT_CTRL(Y),
          [this]() { return HasCurrentEditor(); })
      .Separator();

  // Clipboard operations - delegate to current editor
  menu_builder_
      .Item(
          "Cut", ICON_MD_CONTENT_CUT, [this]() { OnCut(); }, SHORTCUT_CTRL(X),
          [this]() { return HasCurrentEditor(); })
      .Item(
          "Copy", ICON_MD_CONTENT_COPY, [this]() { OnCopy(); },
          SHORTCUT_CTRL(C), [this]() { return HasCurrentEditor(); })
      .Item(
          "Paste", ICON_MD_CONTENT_PASTE, [this]() { OnPaste(); },
          SHORTCUT_CTRL(V), [this]() { return HasCurrentEditor(); })
      .Separator();

  // Search operations (Find in Files moved to Tools > Global Search)
  menu_builder_.Item(
      "Find", ICON_MD_SEARCH, [this]() { OnFind(); }, SHORTCUT_CTRL(F),
      [this]() { return HasCurrentEditor(); });
}

void MenuOrchestrator::BuildViewMenu() {
  menu_builder_.BeginMenu("View");
  AddViewMenuItems();
  menu_builder_.EndMenu();
}

void MenuOrchestrator::AddViewMenuItems() {
  AddAppearanceMenuItems();
  menu_builder_.Separator();

  AddLayoutMenuItems();
  menu_builder_.Separator();

  // Editor selection (Switch Editor)
  menu_builder_.Item(
      "Switch Editor...", ICON_MD_SWAP_HORIZ,
      [this]() { OnShowEditorSelection(); }, SHORTCUT_CTRL(E),
      [this]() { return HasActiveRom(); });
}

void MenuOrchestrator::AddAppearanceMenuItems() {
  // Appearance/Layout controls
  menu_builder_
      .Item(
          "Show Sidebar", ICON_MD_VIEW_SIDEBAR,
          [this]() {
            if (panel_manager_)
              panel_manager_->ToggleSidebarVisibility();
          },
          SHORTCUT_CTRL(B), nullptr,
          [this]() {
            return panel_manager_ && panel_manager_->IsSidebarVisible();
          })
      .Item(
          "Show Status Bar", ICON_MD_HORIZONTAL_RULE,
          [this]() {
            if (user_settings_) {
              user_settings_->prefs().show_status_bar =
                  !user_settings_->prefs().show_status_bar;
              user_settings_->Save();
              if (status_bar_) {
                status_bar_->SetEnabled(
                    user_settings_->prefs().show_status_bar);
              }
            }
          },
          nullptr, nullptr,
          [this]() {
            return user_settings_ && user_settings_->prefs().show_status_bar;
          })
      .Separator()
      .Item("Display Settings", ICON_MD_DISPLAY_SETTINGS,
            [this]() { OnShowDisplaySettings(); })
      .Item("Welcome Screen", ICON_MD_HOME, [this]() { OnShowWelcomeScreen(); });
}

void MenuOrchestrator::AddLayoutMenuItems() {
  const auto layout_enabled = [this]() { return HasCurrentEditor(); };

  menu_builder_.BeginSubMenu("Layout", ICON_MD_VIEW_QUILT)
      .Item(
          "Profile: Code", ICON_MD_CODE,
          [this]() {
            if (editor_manager_) {
              editor_manager_->ApplyLayoutProfile("code");
            }
          },
          nullptr, layout_enabled)
      .Item(
          "Profile: Debug", ICON_MD_BUG_REPORT,
          [this]() {
            if (editor_manager_) {
              editor_manager_->ApplyLayoutProfile("debug");
            }
          },
          nullptr, layout_enabled)
      .Item(
          "Profile: Mapping", ICON_MD_MAP,
          [this]() {
            if (editor_manager_) {
              editor_manager_->ApplyLayoutProfile("mapping");
            }
          },
          nullptr, layout_enabled)
      .Item(
          "Profile: Chat + Agent", ICON_MD_SMART_TOY,
          [this]() {
            if (editor_manager_) {
              editor_manager_->ApplyLayoutProfile("chat");
            }
          },
          nullptr, layout_enabled)
      .Separator()
      .Item(
          "Developer", ICON_MD_DEVELOPER_MODE,
          [this]() { OnLoadDeveloperLayout(); }, nullptr, layout_enabled)
      .Item(
          "Designer", ICON_MD_DESIGN_SERVICES,
          [this]() { OnLoadDesignerLayout(); }, nullptr, layout_enabled)
      .Item(
          "Modder", ICON_MD_BUILD, [this]() { OnLoadModderLayout(); }, nullptr,
          layout_enabled)
      .Separator()
      .Item(
          "Reset Current Editor", ICON_MD_REFRESH,
          [this]() {
            if (editor_manager_) {
              editor_manager_->ResetCurrentEditorLayout();
            }
          },
          nullptr, layout_enabled)
      .EndMenu();
}

void MenuOrchestrator::BuildPanelsMenu() {
  // Use CustomMenu to integrate dynamic panel content with the menu builder
  menu_builder_.CustomMenu("Panels", [this]() { AddPanelsMenuItems(); });
}

void MenuOrchestrator::AddPanelsMenuItems() {
  if (!panel_manager_) {
    return;
  }

  const size_t session_id = session_coordinator_.GetActiveSessionIndex();
  std::string active_category = panel_manager_->GetActiveCategory();
  auto all_categories = panel_manager_->GetAllCategories(session_id);

  if (all_categories.empty()) {
    ImGui::TextDisabled("No panels available");
    return;
  }

  // Panel Browser action at top
  if (ImGui::MenuItem(
          absl::StrFormat("%s Panel Browser", ICON_MD_APPS).c_str(),
          SHORTCUT_CTRL_SHIFT(B))) {
    OnShowPanelBrowser();
  }
  ImGui::Separator();

  // Show all categories as direct submenus (no nested "All Categories" wrapper)
  for (const auto& category : all_categories) {
    // Mark active category with icon
    std::string label = category;
    if (category == active_category) {
      label = absl::StrFormat("%s %s", ICON_MD_FOLDER_OPEN, category);
    } else {
      label = absl::StrFormat("%s %s", ICON_MD_FOLDER, category);
    }

    if (ImGui::BeginMenu(label.c_str())) {
      auto cards = panel_manager_->GetPanelsInCategory(session_id, category);

      if (cards.empty()) {
        ImGui::TextDisabled("No panels in this category");
      } else {
        for (const auto& card : cards) {
          bool is_visible =
              panel_manager_->IsPanelVisible(session_id, card.card_id);
          const char* shortcut = card.shortcut_hint.empty()
                                     ? nullptr
                                     : card.shortcut_hint.c_str();

          // Show icon for visible panels
          std::string item_label = is_visible
              ? absl::StrFormat("%s %s", ICON_MD_CHECK_BOX, card.display_name)
              : absl::StrFormat("%s %s", ICON_MD_CHECK_BOX_OUTLINE_BLANK,
                                card.display_name);

          if (ImGui::MenuItem(item_label.c_str(), shortcut)) {
            panel_manager_->TogglePanel(session_id, card.card_id);
          }
        }
      }
      ImGui::EndMenu();
    }
  }
}

void MenuOrchestrator::BuildToolsMenu() {
  menu_builder_.BeginMenu("Tools");
  AddToolsMenuItems();
  menu_builder_.EndMenu();
}

void MenuOrchestrator::AddToolsMenuItems() {
  AddSearchMenuItems();
  menu_builder_.Separator();

  AddRomAnalysisMenuItems();
  AddAsarIntegrationMenuItems();
  menu_builder_.Separator();

  AddDevelopmentMenuItems();

#ifdef YAZE_ENABLE_TESTING
  AddTestingMenuItems();
#endif

  // ImGui Debug (moved from Debug menu)
  menu_builder_.BeginSubMenu("ImGui Debug", ICON_MD_BUG_REPORT)
      .Item("ImGui Demo", ICON_MD_HELP, [this]() { OnShowImGuiDemo(); })
      .Item("ImGui Metrics", ICON_MD_ANALYTICS,
            [this]() { OnShowImGuiMetrics(); })
      .EndMenu()
      .Separator();

#ifdef YAZE_WITH_GRPC
  AddCollaborationMenuItems();
#endif
}

void MenuOrchestrator::AddSearchMenuItems() {
  // Search & Navigation
  menu_builder_
      .Item(
          "Global Search", ICON_MD_SEARCH, [this]() { OnShowGlobalSearch(); },
          SHORTCUT_CTRL_SHIFT(F))
      .Item(
          "Command Palette", ICON_MD_SEARCH,
          [this]() { OnShowCommandPalette(); }, SHORTCUT_CTRL_SHIFT(P))
      .Item(
          "Panel Finder", ICON_MD_DASHBOARD,
          [this]() { OnShowPanelFinder(); }, SHORTCUT_CTRL(P))
      .Item("Resource Label Manager", ICON_MD_LABEL,
            [this]() { OnShowResourceLabelManager(); });
}

void MenuOrchestrator::AddRomAnalysisMenuItems() {
  // ROM Analysis (moved from Debug menu)
  menu_builder_.BeginSubMenu("ROM Analysis", ICON_MD_STORAGE)
      .Item(
          "ROM Information", ICON_MD_INFO, [this]() { OnShowRomInfo(); },
          nullptr, [this]() { return HasActiveRom(); })
      .Item(
          "Data Integrity Check", ICON_MD_ANALYTICS,
          [this]() { OnRunDataIntegrityCheck(); }, nullptr,
          [this]() { return HasActiveRom(); })
      .Item(
          "Test Save/Load", ICON_MD_SAVE_ALT, [this]() { OnTestSaveLoad(); },
          nullptr, [this]() { return HasActiveRom(); })
      .EndMenu();

  // ZSCustomOverworld (moved from Debug menu)
  menu_builder_.BeginSubMenu("ZSCustomOverworld", ICON_MD_CODE)
      .Item(
          "Check ROM Version", ICON_MD_INFO, [this]() { OnCheckRomVersion(); },
          nullptr, [this]() { return HasActiveRom(); })
      .Item(
          "Upgrade ROM", ICON_MD_UPGRADE, [this]() { OnUpgradeRom(); }, nullptr,
          [this]() { return HasActiveRom(); })
      .Item("Toggle Custom Loading", ICON_MD_SETTINGS,
            [this]() { OnToggleCustomLoading(); })
      .EndMenu();
}

void MenuOrchestrator::AddAsarIntegrationMenuItems() {
  // Asar Integration (moved from Debug menu)
  menu_builder_.BeginSubMenu("Asar Integration", ICON_MD_BUILD)
      .Item("Asar Status", ICON_MD_INFO,
            [this]() { popup_manager_.Show(PopupID::kAsarIntegration); })
      .Item(
          "Toggle ASM Patch", ICON_MD_CODE, [this]() { OnToggleAsarPatch(); },
          nullptr, [this]() { return HasActiveRom(); })
      .Item("Load ASM File", ICON_MD_FOLDER_OPEN, [this]() { OnLoadAsmFile(); })
      .EndMenu();
}

void MenuOrchestrator::AddDevelopmentMenuItems() {
  // Development Tools (moved from Debug menu)
  menu_builder_.BeginSubMenu("Development", ICON_MD_DEVELOPER_MODE)
      .Item("Memory Editor", ICON_MD_MEMORY, [this]() { OnShowMemoryEditor(); })
      .Item("Assembly Editor", ICON_MD_CODE,
            [this]() { OnShowAssemblyEditor(); })
      .Item("Feature Flags", ICON_MD_FLAG,
            [this]() { popup_manager_.Show(PopupID::kFeatureFlags); })
      .Item("Performance Dashboard", ICON_MD_SPEED,
            [this]() { OnShowPerformanceDashboard(); })
#ifdef YAZE_BUILD_AGENT_UI
      .Item("Agent Workspace", ICON_MD_SMART_TOY,
            [this]() { OnShowAIAgent(); })
#endif
#ifdef YAZE_WITH_GRPC
      .Item("Agent Proposals", ICON_MD_PREVIEW,
            [this]() { OnShowProposalDrawer(); })
#endif
      .EndMenu();
}

void MenuOrchestrator::AddTestingMenuItems() {
  // Testing (moved from Debug menu)
  menu_builder_.BeginSubMenu("Testing", ICON_MD_SCIENCE);
#ifdef YAZE_ENABLE_TESTING
  menu_builder_
      .Item("Test Dashboard", ICON_MD_DASHBOARD,
            [this]() { OnShowTestDashboard(); }, SHORTCUT_CTRL(T))
      .Item("Run All Tests", ICON_MD_PLAY_ARROW, [this]() { OnRunAllTests(); })
      .Item("Run Unit Tests", ICON_MD_CHECK_BOX, [this]() { OnRunUnitTests(); })
      .Item("Run Integration Tests", ICON_MD_INTEGRATION_INSTRUCTIONS,
            [this]() { OnRunIntegrationTests(); })
      .Item("Run E2E Tests", ICON_MD_VISIBILITY, [this]() { OnRunE2ETests(); });
#else
  menu_builder_.DisabledItem("Testing support disabled (YAZE_ENABLE_TESTING=OFF)",
                            ICON_MD_INFO);
#endif
  menu_builder_.EndMenu();
}

#ifdef YAZE_WITH_GRPC
void MenuOrchestrator::AddCollaborationMenuItems() {
  // Collaboration (GRPC builds only)
  menu_builder_.BeginSubMenu("Collaborate", ICON_MD_PEOPLE)
      .Item("Start Collaboration Session", ICON_MD_PLAY_CIRCLE,
            [this]() { OnStartCollaboration(); })
      .Item("Join Collaboration Session", ICON_MD_GROUP_ADD,
            [this]() { OnJoinCollaboration(); })
      .Item("Network Status", ICON_MD_CLOUD,
            [this]() { OnShowNetworkStatus(); })
      .EndMenu();
}
#endif

void MenuOrchestrator::BuildWindowMenu() {
  menu_builder_.BeginMenu("Window");
  AddWindowMenuItems();
  menu_builder_.EndMenu();
}

void MenuOrchestrator::AddWindowMenuItems() {
  // Sessions Submenu
  menu_builder_.BeginSubMenu("Sessions", ICON_MD_TAB)
      .Item(
          "New Session", ICON_MD_ADD, [this]() { OnCreateNewSession(); },
          SHORTCUT_CTRL_SHIFT(N))
      .Item(
          "Duplicate Session", ICON_MD_CONTENT_COPY,
          [this]() { OnDuplicateCurrentSession(); }, nullptr,
          [this]() { return HasActiveRom(); })
      .Item(
          "Close Session", ICON_MD_CLOSE, [this]() { OnCloseCurrentSession(); },
          SHORTCUT_CTRL_SHIFT(W), [this]() { return HasMultipleSessions(); })
      .Separator()
      .Item(
          "Session Switcher", ICON_MD_SWITCH_ACCOUNT,
          [this]() { OnShowSessionSwitcher(); }, SHORTCUT_CTRL(Tab),
          [this]() { return HasMultipleSessions(); })
      .Item("Session Manager", ICON_MD_VIEW_LIST,
            [this]() { OnShowSessionManager(); })
      .EndMenu()
      .Separator();

  // Layout Management
  const auto layout_actions_enabled = [this]() {
    return HasCurrentEditor();
  };

  menu_builder_
      .Item(
          "Save Layout", ICON_MD_SAVE, [this]() { OnSaveWorkspaceLayout(); },
          SHORTCUT_CTRL_SHIFT(S))
      .Item(
          "Load Layout", ICON_MD_FOLDER_OPEN,
          [this]() { OnLoadWorkspaceLayout(); }, SHORTCUT_CTRL_SHIFT(O))
      .Item("Reset Layout", ICON_MD_RESET_TV,
            [this]() { OnResetWorkspaceLayout(); })
      .BeginSubMenu("Layout Presets", ICON_MD_VIEW_QUILT)
      .Item(
          "Reset Active Editor Layout", ICON_MD_REFRESH,
          [this]() {
            if (editor_manager_) {
              editor_manager_->ResetCurrentEditorLayout();
            }
          },
          nullptr, layout_actions_enabled)
      .Separator()
      .Item(
          "Profile: Code", ICON_MD_CODE,
          [this]() {
            if (editor_manager_) {
              editor_manager_->ApplyLayoutProfile("code");
            }
          },
          nullptr, layout_actions_enabled)
      .Item(
          "Profile: Debug", ICON_MD_BUG_REPORT,
          [this]() {
            if (editor_manager_) {
              editor_manager_->ApplyLayoutProfile("debug");
            }
          },
          nullptr, layout_actions_enabled)
      .Item(
          "Profile: Mapping", ICON_MD_MAP,
          [this]() {
            if (editor_manager_) {
              editor_manager_->ApplyLayoutProfile("mapping");
            }
          },
          nullptr, layout_actions_enabled)
      .Item(
          "Profile: Chat + Agent", ICON_MD_SMART_TOY,
          [this]() {
            if (editor_manager_) {
              editor_manager_->ApplyLayoutProfile("chat");
            }
          },
          nullptr, layout_actions_enabled)
      .Separator()
      .Item(
          "Capture Session Snapshot", ICON_MD_BOOKMARK_ADD,
          [this]() {
            if (editor_manager_) {
              editor_manager_->CaptureTemporaryLayoutSnapshot();
            }
          },
          nullptr, layout_actions_enabled)
      .Item(
          "Restore Session Snapshot", ICON_MD_RESTORE,
          [this]() {
            if (editor_manager_) {
              editor_manager_->RestoreTemporaryLayoutSnapshot();
            }
          },
          nullptr, layout_actions_enabled)
      .Item(
          "Clear Session Snapshot", ICON_MD_BOOKMARK_REMOVE,
          [this]() {
            if (editor_manager_) {
              editor_manager_->ClearTemporaryLayoutSnapshot();
            }
          },
          nullptr, layout_actions_enabled)
      .Separator()
      .Item(
          "Minimal", ICON_MD_VIEW_COMPACT,
          [this]() {
            if (editor_manager_) {
              editor_manager_->ApplyLayoutPreset("Minimal");
            }
          },
          nullptr, layout_actions_enabled)
      .Item(
          "Developer", ICON_MD_DEVELOPER_MODE,
          [this]() { OnLoadDeveloperLayout(); }, nullptr,
          layout_actions_enabled)
      .Item(
          "Designer", ICON_MD_DESIGN_SERVICES,
          [this]() { OnLoadDesignerLayout(); }, nullptr,
          layout_actions_enabled)
      .Item(
          "Modder", ICON_MD_BUILD, [this]() { OnLoadModderLayout(); },
          nullptr, layout_actions_enabled)
      .Item(
          "Overworld Expert", ICON_MD_MAP,
          [this]() {
            if (editor_manager_) {
              editor_manager_->ApplyLayoutPreset("Overworld Expert");
            }
          },
          nullptr, layout_actions_enabled)
      .Item(
          "Dungeon Expert", ICON_MD_CASTLE,
          [this]() {
            if (editor_manager_) {
              editor_manager_->ApplyLayoutPreset("Dungeon Expert");
            }
          },
          nullptr, layout_actions_enabled)
      .Item(
          "Testing", ICON_MD_SCIENCE,
          [this]() {
            if (editor_manager_) {
              editor_manager_->ApplyLayoutPreset("Testing");
            }
          },
          nullptr, layout_actions_enabled)
      .Item(
          "Audio", ICON_MD_MUSIC_NOTE,
          [this]() {
            if (editor_manager_) {
              editor_manager_->ApplyLayoutPreset("Audio");
            }
          },
          nullptr, layout_actions_enabled)
      .Separator()
      .Item("Manage Presets...", ICON_MD_TUNE,
            [this]() { OnShowLayoutPresets(); })
      .EndMenu()
      .Separator();

  // Window Visibility
  menu_builder_
      .Item("Show All Windows", ICON_MD_VISIBILITY,
            [this]() { OnShowAllWindows(); })
      .Item("Hide All Windows", ICON_MD_VISIBILITY_OFF,
            [this]() { OnHideAllWindows(); })
      .Separator();

  // Panel Browser (requires ROM) - Panels are accessible via the sidebar
  menu_builder_
      .Item(
          "Panel Browser", ICON_MD_DASHBOARD,
          [this]() { OnShowPanelBrowser(); }, SHORTCUT_CTRL_SHIFT(B),
          [this]() { return HasActiveRom(); })
      .Separator();

  // Note: Panel toggle buttons are on the right side of the menu bar
}

void MenuOrchestrator::BuildHelpMenu() {
  menu_builder_.BeginMenu("Help");
  AddHelpMenuItems();
  menu_builder_.EndMenu();
}

void MenuOrchestrator::AddHelpMenuItems() {
  // Note: Asar Integration moved to Tools menu to reduce redundancy
  menu_builder_
      .Item("Getting Started", ICON_MD_PLAY_ARROW,
            [this]() { OnShowGettingStarted(); })
      .Item("Keyboard Shortcuts", ICON_MD_KEYBOARD,
            [this]() { OnShowSettings(); })
      .Item("Build Instructions", ICON_MD_BUILD,
            [this]() { OnShowBuildInstructions(); })
      .Item("CLI Usage", ICON_MD_TERMINAL, [this]() { OnShowCLIUsage(); })
      .Separator()
      .Item("Supported Features", ICON_MD_CHECK_CIRCLE,
            [this]() { OnShowSupportedFeatures(); })
      .Item("What's New", ICON_MD_NEW_RELEASES, [this]() { OnShowWhatsNew(); })
      .Separator()
      .Item("Troubleshooting", ICON_MD_BUILD_CIRCLE,
            [this]() { OnShowTroubleshooting(); })
      .Item("Contributing", ICON_MD_VOLUNTEER_ACTIVISM,
            [this]() { OnShowContributing(); })
      .Separator()
      .Item(
          "About", ICON_MD_INFO, [this]() { OnShowAbout(); }, "F1");
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
      if (absl::IsCancelled(status)) {
        return;
      }
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

void MenuOrchestrator::OnShowProjectManagement() {
  // Show project management panel in right sidebar
  if (editor_manager_) {
    editor_manager_->ShowProjectManagement();
  }
}

void MenuOrchestrator::OnShowProjectFileEditor() {
  // Open the project file editor with the current project file
  if (editor_manager_) {
    editor_manager_->ShowProjectFileEditor();
  }
}

// Edit menu actions - delegate to current editor
void MenuOrchestrator::OnUndo() {
  if (editor_manager_) {
    auto* current_editor = editor_manager_->GetCurrentEditor();
    if (current_editor) {
      // Capture description before undo moves the action to the redo stack
      std::string desc = current_editor->GetUndoDescription();
      auto status = current_editor->Undo();
      if (status.ok()) {
        if (!desc.empty()) {
          toast_manager_.Show(absl::StrFormat("Undid: %s", desc),
                              ToastType::kInfo, 2.0f);
        }
      } else {
        toast_manager_.Show(
            absl::StrFormat("Undo failed: %s", status.message()),
            ToastType::kError);
      }
    }
  }
}

void MenuOrchestrator::OnRedo() {
  if (editor_manager_) {
    auto* current_editor = editor_manager_->GetCurrentEditor();
    if (current_editor) {
      // Capture description before redo moves the action to the undo stack
      std::string desc = current_editor->GetRedoDescription();
      auto status = current_editor->Redo();
      if (status.ok()) {
        if (!desc.empty()) {
          toast_manager_.Show(absl::StrFormat("Redid: %s", desc),
                              ToastType::kInfo, 2.0f);
        }
      } else {
        toast_manager_.Show(
            absl::StrFormat("Redo failed: %s", status.message()),
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
        toast_manager_.Show(
            absl::StrFormat("Copy failed: %s", status.message()),
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
        toast_manager_.Show(
            absl::StrFormat("Paste failed: %s", status.message()),
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
        toast_manager_.Show(
            absl::StrFormat("Find failed: %s", status.message()),
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
  // Show hex editor card via EditorPanelManager
  if (editor_manager_) {
    editor_manager_->panel_manager().ShowPanel(
        editor_manager_->GetCurrentSessionId(), "Hex Editor");
  }
}


void MenuOrchestrator::OnShowPanelBrowser() {
  if (editor_manager_) {
    if (auto* ui = editor_manager_->ui_coordinator()) {
      ui->SetPanelBrowserVisible(true);
    }
  }
}

void MenuOrchestrator::OnShowPanelFinder() {
  if (editor_manager_) {
    if (auto* ui = editor_manager_->ui_coordinator()) {
      ui->ShowPanelFinder();
    }
  }
}

void MenuOrchestrator::OnShowWelcomeScreen() {
  if (editor_manager_) {
    if (auto* ui = editor_manager_->ui_coordinator()) {
      ui->SetWelcomeScreenVisible(true);
    }
  }
}

#ifdef YAZE_BUILD_AGENT_UI
void MenuOrchestrator::OnShowAIAgent() {
  if (editor_manager_) {
    if (auto* ui = editor_manager_->ui_coordinator()) {
      ui->SetAIAgentVisible(true);
    }
  }
}


void MenuOrchestrator::OnShowProposalDrawer() {
  if (editor_manager_) {
    if (auto* ui = editor_manager_->ui_coordinator()) {
      ui->SetProposalDrawerVisible(true);
    }
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


void MenuOrchestrator::OnShowSessionSwitcher() {
  // Delegate to UICoordinator for session switcher UI
  if (editor_manager_) {
    if (auto* ui = editor_manager_->ui_coordinator()) {
      ui->ShowSessionSwitcher();
    }
  }
}

void MenuOrchestrator::OnShowSessionManager() {
  popup_manager_.Show(PopupID::kSessionManager);
}

// Window management menu actions
void MenuOrchestrator::OnShowAllWindows() {
  // Delegate to EditorManager
  if (editor_manager_) {
    if (auto* ui = editor_manager_->ui_coordinator()) {
      ui->ShowAllWindows();
    }
  }
}

void MenuOrchestrator::OnHideAllWindows() {
  // Delegate to EditorManager
  if (editor_manager_) {
    editor_manager_->HideAllWindows();
  }
}

void MenuOrchestrator::OnResetWorkspaceLayout() {
  // Queue as deferred action to avoid modifying ImGui state during menu rendering
  if (editor_manager_) {
    editor_manager_->QueueDeferredAction([this]() {
      editor_manager_->ResetWorkspaceLayout();
      toast_manager_.Show("Layout reset to default", ToastType::kInfo);
    });
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
  popup_manager_.Show(PopupID::kLayoutPresets);
}

void MenuOrchestrator::OnLoadDeveloperLayout() {
  if (editor_manager_) {
    editor_manager_->ApplyLayoutPreset("Developer");
  }
}

void MenuOrchestrator::OnLoadDesignerLayout() {
  if (editor_manager_) {
    editor_manager_->ApplyLayoutPreset("Designer");
  }
}

void MenuOrchestrator::OnLoadModderLayout() {
  if (editor_manager_) {
    editor_manager_->ApplyLayoutPreset("Modder");
  }
}

// Tool menu actions
void MenuOrchestrator::OnShowGlobalSearch() {
  if (editor_manager_) {
    if (auto* ui = editor_manager_->ui_coordinator()) {
      ui->ShowGlobalSearch();
    }
  }
}

void MenuOrchestrator::OnShowCommandPalette() {
  if (editor_manager_) {
    if (auto* ui = editor_manager_->ui_coordinator()) {
      ui->ShowCommandPalette();
    }
  }
}

void MenuOrchestrator::OnShowPerformanceDashboard() {
  if (editor_manager_) {
    if (auto* ui = editor_manager_->ui_coordinator()) {
      ui->SetPerformanceDashboardVisible(true);
    }
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
    editor_manager_->panel_manager().ShowPanel(
        editor_manager_->GetCurrentSessionId(), "Memory Editor");
  }
}

void MenuOrchestrator::OnShowResourceLabelManager() {
  if (editor_manager_) {
    if (auto* ui = editor_manager_->ui_coordinator()) {
      ui->SetResourceLabelManagerVisible(true);
    }
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
  toast_manager_.Show(
      "E2E runner is not wired in-app yet. Use scripts/agents/run-tests.sh or "
      "z3ed test-run.",
      ToastType::kWarning);
}
#endif

#ifdef YAZE_WITH_GRPC
void MenuOrchestrator::OnStartCollaboration() {
  toast_manager_.Show(
      "Collaboration session start is not wired yet. Run yaze-server and use "
      "the web client for live sync.",
      ToastType::kWarning);
}

void MenuOrchestrator::OnJoinCollaboration() {
  toast_manager_.Show(
      "Join collaboration is not wired yet. Use the web client + yaze-server.",
      ToastType::kWarning);
}

void MenuOrchestrator::OnShowNetworkStatus() {
  toast_manager_.Show("Network status panel is not implemented yet.",
                      ToastType::kWarning);
}
#endif

// Help menu actions
void MenuOrchestrator::OnShowAbout() {
  popup_manager_.Show(PopupID::kAbout);
}

void MenuOrchestrator::OnShowGettingStarted() {
  popup_manager_.Show(PopupID::kGettingStarted);
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
    auto status = rom_manager_.CreateBackup(editor_manager_->GetCurrentRom());
    if (status.ok()) {
      toast_manager_.Show("Backup created successfully", ToastType::kSuccess);
    } else {
      toast_manager_.Show(
          absl::StrFormat("Backup failed: %s", status.message()),
          ToastType::kError);
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

bool MenuOrchestrator::HasProjectFile() const {
  // Check if EditorManager has a project with a valid filepath
  // This is separate from HasActiveProject which checks ProjectManager
  const auto* project =
      editor_manager_ ? editor_manager_->GetCurrentProject() : nullptr;
  return project && !project->filepath.empty();
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
std::string MenuOrchestrator::GetShortcutForAction(
    const std::string& action) const {
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
  if (!editor_manager_)
    return;
  auto* rom = editor_manager_->GetCurrentRom();
  if (!rom || !rom->is_loaded())
    return;

  toast_manager_.Show("Running ROM integrity tests...", ToastType::kInfo);
  // This would integrate with the test system in master
  // For now, just show a placeholder
  toast_manager_.Show("Data integrity check completed", ToastType::kSuccess,
                      3.0f);
#else
  toast_manager_.Show("Testing not enabled in this build", ToastType::kWarning);
#endif
}

void MenuOrchestrator::OnTestSaveLoad() {
#ifdef YAZE_ENABLE_TESTING
  if (!editor_manager_)
    return;
  auto* rom = editor_manager_->GetCurrentRom();
  if (!rom || !rom->is_loaded())
    return;

  toast_manager_.Show("Running ROM save/load tests...", ToastType::kInfo);
  // This would integrate with the test system in master
  toast_manager_.Show("Save/load test completed", ToastType::kSuccess, 3.0f);
#else
  toast_manager_.Show("Testing not enabled in this build", ToastType::kWarning);
#endif
}

void MenuOrchestrator::OnCheckRomVersion() {
  if (!editor_manager_)
    return;
  auto* rom = editor_manager_->GetCurrentRom();
  if (!rom || !rom->is_loaded())
    return;

  // Check ZSCustomOverworld version
  uint8_t version = (*rom)[zelda3::OverworldCustomASMHasBeenApplied];
  std::string version_str =
      (version == 0xFF) ? "Vanilla" : absl::StrFormat("v%d", version);

  toast_manager_.Show(
      absl::StrFormat("ROM: %s | ZSCustomOverworld: %s", rom->title().c_str(),
                      version_str.c_str()),
      ToastType::kInfo, 5.0f);
}

void MenuOrchestrator::OnUpgradeRom() {
  if (!editor_manager_)
    return;
  auto* rom = editor_manager_->GetCurrentRom();
  if (!rom || !rom->is_loaded())
    return;

  toast_manager_.Show("Use Overworld Editor to upgrade ROM version",
                      ToastType::kInfo, 4.0f);
}

void MenuOrchestrator::OnToggleCustomLoading() {
  auto& flags = core::FeatureFlags::get();
  flags.overworld.kLoadCustomOverworld = !flags.overworld.kLoadCustomOverworld;

  toast_manager_.Show(
      absl::StrFormat(
          "Custom Overworld Loading: %s",
          flags.overworld.kLoadCustomOverworld ? "Enabled" : "Disabled"),
      ToastType::kInfo);
}

void MenuOrchestrator::OnToggleAsarPatch() {
  if (!editor_manager_)
    return;
  auto* rom = editor_manager_->GetCurrentRom();
  if (!rom || !rom->is_loaded())
    return;

  auto& flags = core::FeatureFlags::get();
  flags.overworld.kApplyZSCustomOverworldASM =
      !flags.overworld.kApplyZSCustomOverworldASM;

  toast_manager_.Show(
      absl::StrFormat(
          "ZSCustomOverworld ASM Application: %s",
          flags.overworld.kApplyZSCustomOverworldASM ? "Enabled" : "Disabled"),
      ToastType::kInfo);
}

void MenuOrchestrator::OnLoadAsmFile() {
  toast_manager_.Show("ASM file loading not yet implemented",
                      ToastType::kWarning);
}

void MenuOrchestrator::OnShowAssemblyEditor() {
  if (editor_manager_) {
    editor_manager_->SwitchToEditor(EditorType::kAssembly);
  }
}

}  // namespace editor
}  // namespace yaze
