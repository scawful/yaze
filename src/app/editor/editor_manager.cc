// Related header
#include "editor_manager.h"

// C system headers
#include <cstdint>
#include <cstring>

// C++ standard library headers
#include <chrono>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

// Third-party library headers
#define IMGUI_DEFINE_MATH_OPERATORS
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "imgui/imgui.h"

// Project headers
#include "app/application.h"
#include "app/editor/code/assembly_editor.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/editor.h"
#include "app/editor/graphics/graphics_editor.h"
#include "app/editor/graphics/screen_editor.h"
#include "app/editor/layout/layout_manager.h"
#include "app/editor/menu/activity_bar.h"
#include "app/editor/menu/menu_orchestrator.h"
#include "app/editor/music/music_editor.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/palette/palette_editor.h"
#include "app/editor/session_types.h"
#include "app/editor/sprite/sprite_editor.h"
#include "app/editor/system/editor_registry.h"
#include "app/editor/system/panel_manager.h"
#include "app/editor/system/shortcut_configurator.h"
#include "app/editor/ui/dashboard_panel.h"
#include "app/editor/ui/popup_manager.h"
#include "app/editor/ui/project_management_panel.h"
#include "app/editor/ui/settings_panel.h"
#include "app/editor/ui/toast_manager.h"
#include "app/editor/ui/ui_coordinator.h"
#include "app/emu/emulator.h"
#include "app/gfx/debug/performance/performance_dashboard.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/theme_manager.h"
#include "app/platform/timing.h"
#include "app/test/test_manager.h"
#include "core/features.h"
#include "core/project.h"
#include "editor/core/content_registry.h"
#include "editor/core/editor_context.h"
#include "editor/layout/layout_coordinator.h"
#include "editor/menu/right_panel_manager.h"
#include "editor/system/editor_activator.h"
#include "editor/system/shortcut_manager.h"
#include "editor/ui/rom_load_options_dialog.h"
#include "rom/rom.h"
#include "startup_flags.h"
#include "util/file_util.h"
#include "util/log.h"
#include "util/macro.h"
#include "yaze_config.h"
#include "zelda3/game_data.h"
#include "zelda3/resource_labels.h"
#include "zelda3/screen/dungeon_map.h"
#include "zelda3/sprite/sprite.h"

// Conditional platform headers
#ifdef __EMSCRIPTEN__
#include "app/platform/wasm/wasm_control_api.h"
#include "app/platform/wasm/wasm_loading_manager.h"
#include "app/platform/wasm/wasm_session_bridge.h"
#endif

// Conditional test headers
#ifdef YAZE_ENABLE_TESTING
#include "app/test/core_systems_test_suite.h"
#include "app/test/e2e_test_suite.h"
#include "app/test/integrated_test_suite.h"
#include "app/test/rom_dependent_test_suite.h"
#include "app/test/zscustomoverworld_test_suite.h"
#endif
#ifdef YAZE_ENABLE_GTEST
#include "app/test/unit_test_suite.h"
#endif
#ifdef YAZE_WITH_GRPC
#include "app/test/z3ed_test_suite.h"
#endif

namespace yaze::editor {

namespace {
// TODO: Move to EditorRegistry
std::string GetEditorName(EditorType type) {
  return kEditorNames[static_cast<int>(type)];
}

std::optional<EditorType> ParseEditorTypeFromString(absl::string_view name) {
  const std::string lower = absl::AsciiStrToLower(std::string(name));
  for (int i = 0; i < static_cast<int>(EditorType::kSettings) + 1; ++i) {
    const std::string candidate = absl::AsciiStrToLower(
        std::string(GetEditorName(static_cast<EditorType>(i))));
    if (candidate == lower) {
      return static_cast<EditorType>(i);
    }
  }
  return std::nullopt;
}

std::string StripSessionPrefix(absl::string_view panel_id) {
  if (panel_id.size() > 2 && panel_id[0] == 's' &&
      absl::ascii_isdigit(panel_id[1])) {
    const size_t dot = panel_id.find('.');
    if (dot != absl::string_view::npos) {
      return std::string(panel_id.substr(dot + 1));
    }
  }
  return std::string(panel_id);
}

}  // namespace

// Static registry of editors that use the card-based layout system
// These editors register their cards with EditorPanelManager and manage their
// own windows They do NOT need the traditional ImGui::Begin/End wrapper - they
// create cards internally
bool EditorManager::IsPanelBasedEditor(EditorType type) {
  return EditorRegistry::IsPanelBasedEditor(type);
}

void EditorManager::HideCurrentEditorPanels() {
  if (!current_editor_) {
    return;
  }

  // Using PanelManager directly
  std::string category =
      editor_registry_.GetEditorCategory(current_editor_->type());
  panel_manager_.HideAllPanelsInCategory(GetCurrentSessionId(), category);
}

void EditorManager::ResetWorkspaceLayout() {
  layout_coordinator_.ResetWorkspaceLayout();
}

void EditorManager::ApplyLayoutPreset(const std::string& preset_name) {
  layout_coordinator_.ApplyLayoutPreset(preset_name, GetCurrentSessionId());
}

void EditorManager::ResetCurrentEditorLayout() {
  if (!current_editor_) {
    toast_manager_.Show("No active editor to reset", ToastType::kWarning);
    return;
  }
  layout_coordinator_.ResetCurrentEditorLayout(current_editor_->type(),
                                               GetCurrentSessionId());
}

#ifdef YAZE_BUILD_AGENT_UI
void EditorManager::ShowAIAgent() {
  // Apply saved agent settings from the current project when opening the Agent
  // UI to respect the user's preferred provider/model.
  // TODO: Implement LoadAgentSettingsFromProject in AgentChat or AgentEditor
  agent_ui_.ShowAgent();
}

void EditorManager::ShowChatHistory() {
  agent_ui_.ShowChatHistory();
}
#endif

EditorManager::EditorManager()
    : project_manager_(&toast_manager_), rom_file_manager_(&toast_manager_) {
  std::stringstream ss;
  ss << YAZE_VERSION_MAJOR << "." << YAZE_VERSION_MINOR << "."
     << YAZE_VERSION_PATCH;
  ss >> version_;

  // Initialize Core Context
  editor_context_ = std::make_unique<GlobalEditorContext>(event_bus_);
  status_bar_.Initialize(editor_context_.get());

  // ============================================================================
  // DELEGATION INFRASTRUCTURE INITIALIZATION
  // ============================================================================
  // EditorManager delegates responsibilities to specialized components:
  // - SessionCoordinator: Multi-session UI and lifecycle management
  // - MenuOrchestrator: Menu building and action routing
  // - UICoordinator: UI drawing and state management
  // - RomFileManager: ROM file I/O operations
  // - ProjectManager: Project file operations
  // - PanelManager: Panel-based editor UI management
  // - ShortcutConfigurator: Keyboard shortcut registration
  // - WindowDelegate: Window layout operations
  // - PopupManager: Modal popup/dialog management
  //
  // EditorManager retains:
  // - Session storage (sessions_) and current pointers (current_rom_, etc.)
  // - Main update loop (iterates sessions, calls editor updates)
  // - Asset loading (Initialize/Load on all editors)
  // - Session ID tracking (current_session_id_)
  //
  // INITIALIZATION ORDER (CRITICAL):
  // 1. PopupManager - MUST be first, MenuOrchestrator/UICoordinator take ref to
  // it
  // 2. SessionCoordinator - Independent, can be early
  // 3. MenuOrchestrator - Depends on PopupManager, SessionCoordinator
  // 4. UICoordinator - Depends on PopupManager, SessionCoordinator
  // 5. ShortcutConfigurator - Created in Initialize(), depends on all above
  //
  // If this order is violated, you will get SIGSEGV crashes when menu callbacks
  // try to call popup_manager_.Show() with an uninitialized PopupManager!
  // ============================================================================

  // STEP 1: Initialize PopupManager FIRST
  popup_manager_ = std::make_unique<PopupManager>(this);
  popup_manager_->Initialize();  // Registers all popups with PopupID constants

  // STEP 2: Initialize SessionCoordinator (independent of popups)
  session_coordinator_ = std::make_unique<SessionCoordinator>(
      &panel_manager_, &toast_manager_, &user_settings_);

  // STEP 3: Initialize MenuOrchestrator (depends on popup_manager_,
  // session_coordinator_)
  menu_orchestrator_ = std::make_unique<MenuOrchestrator>(
      this, menu_builder_, rom_file_manager_, project_manager_,
      editor_registry_, *session_coordinator_, toast_manager_, *popup_manager_);

  // Wire up card registry for Panels submenu in View menu
  menu_orchestrator_->SetPanelManager(&panel_manager_);
  menu_orchestrator_->SetStatusBar(&status_bar_);
  menu_orchestrator_->SetUserSettings(&user_settings_);

  session_coordinator_->SetEditorManager(this);
  session_coordinator_->SetEventBus(&event_bus_);  // Enable event publishing
  ContentRegistry::Context::SetEventBus(&event_bus_);  // Global event bus access
  session_coordinator_->AddObserver(
      this);  // Register for session lifecycle events

  // STEP 4: Initialize UICoordinator (depends on popup_manager_,
  // session_coordinator_, panel_manager_)
  ui_coordinator_ = std::make_unique<UICoordinator>(
      this, rom_file_manager_, project_manager_, editor_registry_,
      panel_manager_, *session_coordinator_, window_delegate_, toast_manager_,
      *popup_manager_, shortcut_manager_);

  // STEP 4.5: Initialize LayoutManager (DockBuilder layouts for editors)
  layout_manager_ = std::make_unique<LayoutManager>();

  // STEP 4.6: Initialize RightPanelManager (right-side sliding panels)
  right_panel_manager_ = std::make_unique<RightPanelManager>();
  right_panel_manager_->SetToastManager(&toast_manager_);
  right_panel_manager_->SetProposalDrawer(&proposal_drawer_);
  right_panel_manager_->SetPropertiesPanel(&selection_properties_panel_);
  right_panel_manager_->SetShortcutManager(&shortcut_manager_);

  // Initialize ProjectManagementPanel for project/version management
  project_management_panel_ = std::make_unique<ProjectManagementPanel>();
  project_management_panel_->SetToastManager(&toast_manager_);
  project_management_panel_->SetSwapRomCallback([this]() {
    // Prompt user to select a new ROM for the project
    auto rom_path = util::FileDialogWrapper::ShowOpenFileDialog(
        util::MakeRomFileDialogOptions(false));
    if (!rom_path.empty()) {
      current_project_.rom_filename = rom_path;
      auto status = current_project_.Save();
      if (status.ok()) {
        toast_manager_.Show("Project ROM updated. Reload to apply changes.",
                            ToastType::kSuccess);
      } else {
        toast_manager_.Show("Failed to update project ROM", ToastType::kError);
      }
    }
  });
  project_management_panel_->SetReloadRomCallback([this]() {
    if (current_project_.project_opened() &&
        !current_project_.rom_filename.empty()) {
      auto status = LoadProjectWithRom();
      if (!status.ok()) {
        toast_manager_.Show(
            absl::StrFormat("Failed to reload ROM: %s", status.message()),
            ToastType::kError);
      }
    }
  });
  project_management_panel_->SetSaveProjectCallback([this]() {
    auto status = SaveProject();
    if (status.ok()) {
      toast_manager_.Show("Project saved", ToastType::kSuccess);
    } else {
      toast_manager_.Show(
          absl::StrFormat("Failed to save project: %s", status.message()),
          ToastType::kError);
    }
  });
  project_management_panel_->SetBrowseFolderCallback(
      [this](const std::string& type) {
        auto folder_path = util::FileDialogWrapper::ShowOpenFolderDialog();
        if (!folder_path.empty()) {
          if (type == "code") {
            current_project_.code_folder = folder_path;
            // Update assembly editor path
            if (auto* editor_set = GetCurrentEditorSet()) {
              editor_set->GetAssemblyEditor()->OpenFolder(folder_path);
              panel_manager_.SetFileBrowserPath("Assembly", folder_path);
            }
          } else if (type == "assets") {
            current_project_.assets_folder = folder_path;
          }
          toast_manager_.Show(absl::StrFormat("%s folder set: %s", type.c_str(),
                                              folder_path.c_str()),
                              ToastType::kSuccess);
        }
      });
  right_panel_manager_->SetProjectManagementPanel(
      project_management_panel_.get());

  // STEP 4.6.1: Initialize LayoutCoordinator (facade for layout operations)
  LayoutCoordinator::Dependencies layout_deps;
  layout_deps.layout_manager = layout_manager_.get();
  layout_deps.panel_manager = &panel_manager_;
  layout_deps.ui_coordinator = ui_coordinator_.get();
  layout_deps.toast_manager = &toast_manager_;
  layout_deps.status_bar = &status_bar_;
  layout_deps.right_panel_manager = right_panel_manager_.get();
  layout_coordinator_.Initialize(layout_deps);

  // STEP 4.6.2: Initialize EditorActivator (editor switching and jump navigation)
  EditorActivator::Dependencies activator_deps;
  activator_deps.panel_manager = &panel_manager_;
  activator_deps.layout_manager = layout_manager_.get();
  activator_deps.ui_coordinator = ui_coordinator_.get();
  activator_deps.right_panel_manager = right_panel_manager_.get();
  activator_deps.toast_manager = &toast_manager_;
  activator_deps.get_current_editor_set = [this]() {
    return GetCurrentEditorSet();
  };
  activator_deps.get_current_session_id = [this]() {
    return GetCurrentSessionId();
  };
  activator_deps.queue_deferred_action = [this](std::function<void()> action) {
    QueueDeferredAction(std::move(action));
  };
  editor_activator_.Initialize(activator_deps);

  // STEP 4.7: Initialize ActivityBar
  activity_bar_ = std::make_unique<ActivityBar>(panel_manager_);

  // Wire up PanelManager callbacks for ActivityBar buttons
  panel_manager_.SetShowHelpCallback([this]() {
    if (right_panel_manager_) {
      right_panel_manager_->TogglePanel(RightPanelManager::PanelType::kHelp);
    }
  });
  panel_manager_.SetShowSettingsCallback([this]() {
    if (right_panel_manager_) {
      right_panel_manager_->TogglePanel(
          RightPanelManager::PanelType::kSettings);
    }
  });

  // STEP 4.8: Initialize DashboardPanel
  dashboard_panel_ = std::make_unique<DashboardPanel>(this);
  panel_manager_.RegisterPanel(
      {.card_id = "dashboard.main",
       .display_name = "Dashboard",
       .window_title = " Dashboard",
       .icon = ICON_MD_DASHBOARD,
       .category = "Dashboard",
       .shortcut_hint = "F1",
       .visibility_flag = dashboard_panel_->visibility_flag(),
       .priority = 0});

  // STEP 5: ShortcutConfigurator created later in Initialize() method
  // It depends on all above coordinators being available
}

EditorManager::~EditorManager() {
  // Unregister as observer before destruction
  if (session_coordinator_) {
    session_coordinator_->RemoveObserver(this);
  }
}

// ============================================================================
// SessionObserver Implementation
// ============================================================================

void EditorManager::OnSessionSwitched(size_t new_index, RomSession* session) {
  // Update RightPanelManager with the new session's settings editor
  if (right_panel_manager_ && session) {
    right_panel_manager_->SetSettingsPanel(session->editors.GetSettingsPanel());
    // Set up StatusBar reference for live toggling
    if (auto* settings = session->editors.GetSettingsPanel()) {
      settings->SetStatusBar(&status_bar_);
    }
  }

  // Update properties panel with new ROM
  if (session) {
    selection_properties_panel_.SetRom(&session->rom);
  }

  // Update ContentRegistry context with current session's ROM
  ContentRegistry::Context::SetRom(session ? &session->rom : nullptr);

#ifdef YAZE_ENABLE_TESTING
  test::TestManager::Get().SetCurrentRom(session ? &session->rom : nullptr);
#endif

  LOG_DEBUG("EditorManager", "Session switched to %zu via observer", new_index);
}

void EditorManager::OnSessionCreated(size_t index, RomSession* session) {
  LOG_INFO("EditorManager", "Session %zu created via observer", index);
}

void EditorManager::OnSessionClosed(size_t index) {
  // Update ContentRegistry - it will be set to new active ROM on next switch
  // If no sessions remain, clear the context
  if (session_coordinator_ &&
      session_coordinator_->GetTotalSessionCount() == 0) {
    ContentRegistry::Context::Clear();
  }

#ifdef YAZE_ENABLE_TESTING
  // Update test manager - it will get the new current ROM on next switch
  test::TestManager::Get().SetCurrentRom(GetCurrentRom());
#endif

  LOG_INFO("EditorManager", "Session %zu closed via observer", index);
}

void EditorManager::OnSessionRomLoaded(size_t index, RomSession* session) {
  // Update ContentRegistry when ROM is loaded (if this is the active session)
  if (session && session_coordinator_ &&
      index == session_coordinator_->GetActiveSessionIndex()) {
    ContentRegistry::Context::SetRom(&session->rom);
  }

#ifdef YAZE_ENABLE_TESTING
  if (session) {
    test::TestManager::Get().SetCurrentRom(&session->rom);
  }
#endif

  LOG_INFO("EditorManager", "ROM loaded in session %zu via observer", index);
}

void EditorManager::InitializeTestSuites() {
  auto& test_manager = test::TestManager::Get();

#ifdef YAZE_ENABLE_TESTING
  // Register comprehensive test suites
  test_manager.RegisterTestSuite(std::make_unique<test::CoreSystemsTestSuite>());
  test_manager.RegisterTestSuite(std::make_unique<test::IntegratedTestSuite>());
  test_manager.RegisterTestSuite(
      std::make_unique<test::PerformanceTestSuite>());
  test_manager.RegisterTestSuite(std::make_unique<test::UITestSuite>());
  test_manager.RegisterTestSuite(
      std::make_unique<test::RomDependentTestSuite>());

  // Register new E2E and ZSCustomOverworld test suites
  test_manager.RegisterTestSuite(std::make_unique<test::E2ETestSuite>());
  test_manager.RegisterTestSuite(
      std::make_unique<test::ZSCustomOverworldTestSuite>());
#endif

  // Register Google Test suite if available
#ifdef YAZE_ENABLE_GTEST
  test_manager.RegisterTestSuite(std::make_unique<test::UnitTestSuite>());
#endif

  // Register z3ed AI Agent test suites (requires gRPC)
#ifdef YAZE_WITH_GRPC
  test::RegisterZ3edTestSuites();
#endif

  // Update resource monitoring to track Arena state
  test_manager.UpdateResourceStats();
}

void EditorManager::Initialize(gfx::IRenderer* renderer,
                               const std::string& filename) {
  renderer_ = renderer;

  // Inject card_registry into emulator and workspace_manager
  emulator_.set_panel_manager(&panel_manager_);
  workspace_manager_.set_panel_manager(&panel_manager_);

  // Point to a blank editor set when no ROM is loaded
  // current_editor_set_ = &blank_editor_set_;

  if (!filename.empty()) {
    PRINT_IF_ERROR(OpenRomOrProject(filename));
  }

  // Note: PopupManager is now initialized in constructor before
  // MenuOrchestrator This ensures all menu callbacks can safely call
  // popup_manager_.Show()

  // Register emulator cards early (emulator Initialize might not be called)
  // Using PanelManager directly
  panel_manager_.RegisterPanel({.card_id = "emulator.cpu_debugger",
                                .display_name = "CPU Debugger",
                                .window_title = " CPU Debugger",
                                .icon = ICON_MD_BUG_REPORT,
                                .category = "Emulator",
                                .priority = 10});
  panel_manager_.RegisterPanel({.card_id = "emulator.ppu_viewer",
                                .display_name = "PPU Viewer",
                                .window_title = " PPU Viewer",
                                .icon = ICON_MD_VIDEOGAME_ASSET,
                                .category = "Emulator",
                                .priority = 20});
  panel_manager_.RegisterPanel({.card_id = "emulator.memory_viewer",
                                .display_name = "Memory Viewer",
                                .window_title = " Memory Viewer",
                                .icon = ICON_MD_MEMORY,
                                .category = "Emulator",
                                .priority = 30});
  panel_manager_.RegisterPanel({.card_id = "emulator.breakpoints",
                                .display_name = "Breakpoints",
                                .window_title = " Breakpoints",
                                .icon = ICON_MD_STOP,
                                .category = "Emulator",
                                .priority = 40});
  panel_manager_.RegisterPanel({.card_id = "emulator.performance",
                                .display_name = "Performance",
                                .window_title = " Performance",
                                .icon = ICON_MD_SPEED,
                                .category = "Emulator",
                                .priority = 50});
  panel_manager_.RegisterPanel({.card_id = "emulator.ai_agent",
                                .display_name = "AI Agent",
                                .window_title = " AI Agent",
                                .icon = ICON_MD_SMART_TOY,
                                .category = "Emulator",
                                .priority = 60});
  panel_manager_.RegisterPanel({.card_id = "emulator.save_states",
                                .display_name = "Save States",
                                .window_title = " Save States",
                                .icon = ICON_MD_SAVE,
                                .category = "Emulator",
                                .priority = 70});
  panel_manager_.RegisterPanel({.card_id = "emulator.keyboard_config",
                                .display_name = "Keyboard Config",
                                .window_title = " Keyboard Config",
                                .icon = ICON_MD_KEYBOARD,
                                .category = "Emulator",
                                .priority = 80});
  panel_manager_.RegisterPanel({.card_id = "emulator.virtual_controller",
                                .display_name = "Virtual Controller",
                                .window_title = " Virtual Controller",
                                .icon = ICON_MD_SPORTS_ESPORTS,
                                .category = "Emulator",
                                .priority = 85});
  panel_manager_.RegisterPanel({.card_id = "emulator.apu_debugger",
                                .display_name = "APU Debugger",
                                .window_title = " APU Debugger",
                                .icon = ICON_MD_AUDIOTRACK,
                                .category = "Emulator",
                                .priority = 90});
  panel_manager_.RegisterPanel({.card_id = "emulator.audio_mixer",
                                .display_name = "Audio Mixer",
                                .window_title = " Audio Mixer",
                                .icon = ICON_MD_AUDIO_FILE,
                                .category = "Emulator",
                                .priority = 100});

  // Register memory/hex editor card
  panel_manager_.RegisterPanel({.card_id = "memory.hex_editor",
                                .display_name = "Hex Editor",
                                .window_title = ICON_MD_MEMORY " Hex Editor",
                                .icon = ICON_MD_MEMORY,
                                .category = "Memory",
                                .priority = 10});

  // Initialize project file editor
  project_file_editor_.SetToastManager(&toast_manager_);

  // Initialize agent UI (no-op when agent UI is disabled)
  agent_ui_.Initialize(&toast_manager_, &proposal_drawer_,
                       right_panel_manager_.get(), &panel_manager_);

  // Note: AgentControlServer is started from Application::Initialize()
  // after gRPC infrastructure is properly set up

  // Load critical user settings first
  status_ = user_settings_.Load();
  if (!status_.ok()) {
    LOG_WARN("EditorManager", "Failed to load user settings: %s",
             status_.ToString().c_str());
  }
  // Apply sprite naming preference globally.
  yaze::zelda3::SetPreferHmagicSpriteNames(
      user_settings_.prefs().prefer_hmagic_sprite_names);

  // Initialize WASM control and session APIs for browser/agent integration
#ifdef __EMSCRIPTEN__
  app::platform::WasmControlApi::Initialize(this);
  app::platform::WasmSessionBridge::Initialize(this);
  LOG_INFO("EditorManager", "WASM Control and Session APIs initialized");
#endif

  // Initialize ROM load options dialog callbacks
  rom_load_options_dialog_.SetConfirmCallback(
      [this](const RomLoadOptionsDialog::LoadOptions& options) {
        // Apply feature flags from dialog
        auto& flags = core::FeatureFlags::get();
        flags.overworld.kSaveOverworldMaps = options.save_overworld_maps;
        flags.overworld.kSaveOverworldEntrances =
            options.save_overworld_entrances;
        flags.overworld.kSaveOverworldExits = options.save_overworld_exits;
        flags.overworld.kSaveOverworldItems = options.save_overworld_items;
        flags.overworld.kLoadCustomOverworld = options.enable_custom_overworld;
        flags.kSaveDungeonMaps = options.save_dungeon_maps;
        flags.kSaveAllPalettes = options.save_all_palettes;
        flags.kSaveGfxGroups = options.save_gfx_groups;

        // Create project if requested
        if (options.create_project && !options.project_name.empty()) {
          project_manager_.SetProjectRom(GetCurrentRom()->filename());
          auto status = project_manager_.FinalizeProjectCreation(
              options.project_name, options.project_path);
          if (!status.ok()) {
            toast_manager_.Show("Failed to create project", ToastType::kError);
          } else {
            toast_manager_.Show("Project created: " + options.project_name,
                                ToastType::kSuccess);
          }
        }

        // Close dialog and show editor selection
        show_rom_load_options_ = false;
        if (ui_coordinator_) {
          // dashboard_panel_->ClearRecentEditors();
          ui_coordinator_->SetEditorSelectionVisible(true);
        }

        LOG_INFO("EditorManager", "ROM load options applied: preset=%s",
                 options.selected_preset.c_str());
      });

  // Initialize welcome screen callbacks
  welcome_screen_.SetOpenRomCallback([this]() {
    status_ = LoadRom();
    // LoadRom() already handles closing welcome screen and showing editor
    // selection
  });

  welcome_screen_.SetNewProjectCallback([this]() {
    status_ = CreateNewProject();
    if (status_.ok() && ui_coordinator_) {
      ui_coordinator_->SetWelcomeScreenVisible(false);
      ui_coordinator_->SetWelcomeScreenManuallyClosed(true);
    }
  });

  welcome_screen_.SetNewProjectWithTemplateCallback(
      [this](const std::string& template_name) {
        // Set the template for the ROM load options dialog
        status_ = CreateNewProject(template_name);
        if (status_.ok() && ui_coordinator_) {
          ui_coordinator_->SetWelcomeScreenVisible(false);
          ui_coordinator_->SetWelcomeScreenManuallyClosed(true);
        }
      });

  welcome_screen_.SetOpenProjectCallback([this](const std::string& filepath) {
    status_ = OpenRomOrProject(filepath);
    if (status_.ok() && ui_coordinator_) {
      ui_coordinator_->SetWelcomeScreenVisible(false);
      ui_coordinator_->SetWelcomeScreenManuallyClosed(true);
    }
  });

  welcome_screen_.SetOpenAgentCallback([this]() {
#ifdef YAZE_BUILD_AGENT_UI
    ShowAIAgent();
#endif
    // Keep welcome screen visible - user may want to do other things
  });

  // Initialize editor selection dialog callback
  // editor_selection_dialog_.SetSelectionCallback([this](EditorType type) {
  //   editor_selection_dialog_.MarkRecentlyUsed(type);
  //   // Pass true for from_dialog so the dialog isn't automatically dismissed
  //   SwitchToEditor(type, /*force_visible=*/false, /*from_dialog=*/true);
  // });

  // Load user settings - this must happen after context is initialized
  // Apply font scale after loading
  ImGui::GetIO().FontGlobalScale = user_settings_.prefs().font_global_scale;

  // Apply welcome screen preference
  if (ui_coordinator_ && !user_settings_.prefs().show_welcome_on_startup) {
    ui_coordinator_->SetWelcomeScreenVisible(false);
    ui_coordinator_->SetWelcomeScreenManuallyClosed(true);
  }

  // Defer workspace presets loading to avoid initialization crashes
  // This will be called lazily when workspace features are accessed

  // Set up sidebar utility icon callbacks
  panel_manager_.SetShowEmulatorCallback([this]() {
    if (ui_coordinator_) {
      ui_coordinator_->SetEmulatorVisible(true);
    }
  });
  panel_manager_.SetShowSettingsCallback(
      [this]() { SwitchToEditor(EditorType::kSettings); });
  panel_manager_.SetShowPanelBrowserCallback([this]() {
    if (ui_coordinator_) {
      ui_coordinator_->ShowPanelBrowser();
    }
  });

  // Set up sidebar action button callbacks
  panel_manager_.SetSaveRomCallback([this]() {
    if (GetCurrentRom() && GetCurrentRom()->is_loaded()) {
      auto status = SaveRom();
      if (status.ok()) {
        toast_manager_.Show("ROM saved successfully", ToastType::kSuccess);
      } else {
        toast_manager_.Show(
            absl::StrFormat("Failed to save ROM: %s", status.message()),
            ToastType::kError);
      }
    }
  });

  panel_manager_.SetUndoCallback([this]() {
    if (auto* current_editor = GetCurrentEditor()) {
      auto status = current_editor->Undo();
      if (!status.ok()) {
        toast_manager_.Show(
            absl::StrFormat("Undo failed: %s", status.message()),
            ToastType::kError);
      }
    }
  });

  panel_manager_.SetRedoCallback([this]() {
    if (auto* current_editor = GetCurrentEditor()) {
      auto status = current_editor->Redo();
      if (!status.ok()) {
        toast_manager_.Show(
            absl::StrFormat("Redo failed: %s", status.message()),
            ToastType::kError);
      }
    }
  });

  panel_manager_.SetShowSearchCallback([this]() {
    if (ui_coordinator_) {
      ui_coordinator_->ShowGlobalSearch();
    }
  });

  panel_manager_.SetShowShortcutsCallback([this]() {
    if (ui_coordinator_) {
      // Shortcut configuration is part of Settings
      SwitchToEditor(EditorType::kSettings);
    }
  });

  panel_manager_.SetShowCommandPaletteCallback([this]() {
    if (ui_coordinator_) {
      ui_coordinator_->ShowCommandPalette();
    }
  });

  panel_manager_.SetShowHelpCallback([this]() {
    if (popup_manager_) {
      popup_manager_->Show(PopupID::kAbout);
    }
  });

  // Set up sidebar state change callbacks for persistence
  // IMPORTANT: Register callbacks BEFORE applying state to avoid triggering Save() during init
  panel_manager_.SetSidebarStateChangedCallback(
      [this](bool visible, bool expanded) {
        user_settings_.prefs().sidebar_visible = visible;
        user_settings_.prefs().sidebar_panel_expanded = expanded;
        PRINT_IF_ERROR(user_settings_.Save());
      });

  panel_manager_.SetCategoryChangedCallback(
      [this](const std::string& category) {
        user_settings_.prefs().sidebar_active_category = category;
        PRINT_IF_ERROR(user_settings_.Save());
      });

  panel_manager_.SetOnPanelClickedCallback([this](const std::string& category) {
    EditorType type = EditorRegistry::GetEditorTypeFromCategory(category);
    // Switch to the editor associated with this card's category
    // This ensures clicking a card opens/focuses the parent editor
    if (type != EditorType::kSettings && type != EditorType::kUnknown) {
      SwitchToEditor(type, true);
    }
  });

  // Handle Activity Bar category selection - dismisses dashboard
  panel_manager_.SetOnCategorySelectedCallback(
      [this](const std::string& category) {
        // Transition startup surface to Editor state (dismisses dashboard)
        if (ui_coordinator_) {
          ui_coordinator_->SetStartupSurface(StartupSurface::kEditor);
        }
      });

  // Enable file browser for Assembly category
  panel_manager_.EnableFileBrowser("Assembly");

  // Set up file clicked callback to open files in Assembly editor
  panel_manager_.SetFileClickedCallback(
      [this](const std::string& category, const std::string& path) {
        if (category == "Assembly") {
          // Open the file in the Assembly editor
          if (auto* editor_set = GetCurrentEditorSet()) {
            editor_set->GetAssemblyEditor()->ChangeActiveFile(path);
            // Make sure Assembly editor is active
            SwitchToEditor(EditorType::kAssembly, true);
          }
        }
      });

  // Apply sidebar state from settings AFTER registering callbacks
  // This triggers the callbacks but they should be safe now
  panel_manager_.SetSidebarVisible(user_settings_.prefs().sidebar_visible);
  panel_manager_.SetPanelExpanded(
      user_settings_.prefs().sidebar_panel_expanded);
  if (!user_settings_.prefs().sidebar_active_category.empty()) {
    panel_manager_.SetActiveCategory(
        user_settings_.prefs().sidebar_active_category);
  }

  // Initialize testing system only when tests are enabled
#ifdef YAZE_ENABLE_TESTING
  InitializeTestSuites();
#endif

  // TestManager will be updated when ROMs are loaded via SetCurrentRom calls

  ShortcutDependencies shortcut_deps;
  shortcut_deps.editor_manager = this;
  shortcut_deps.editor_registry = &editor_registry_;
  shortcut_deps.menu_orchestrator = menu_orchestrator_.get();
  shortcut_deps.rom_file_manager = &rom_file_manager_;
  shortcut_deps.project_manager = &project_manager_;
  shortcut_deps.session_coordinator = session_coordinator_.get();
  shortcut_deps.ui_coordinator = ui_coordinator_.get();
  shortcut_deps.workspace_manager = &workspace_manager_;
  shortcut_deps.popup_manager = popup_manager_.get();
  shortcut_deps.toast_manager = &toast_manager_;
  shortcut_deps.panel_manager = &panel_manager_;

  ConfigureEditorShortcuts(shortcut_deps, &shortcut_manager_);
  ConfigureMenuShortcuts(shortcut_deps, &shortcut_manager_);
}

void EditorManager::OpenEditorAndPanelsFromFlags(
    const std::string& editor_name, const std::string& panels_str) {
  const bool has_editor = !editor_name.empty();
  const bool has_panels = !panels_str.empty();

  if (!has_editor && !has_panels) {
    return;
  }

  LOG_INFO("EditorManager",
           "Processing startup flags: editor='%s', panels='%s'",
           editor_name.c_str(), panels_str.c_str());

  std::optional<EditorType> editor_type_to_open =
      has_editor ? ParseEditorTypeFromString(editor_name) : std::nullopt;
  if (has_editor && !editor_type_to_open.has_value()) {
    LOG_WARN("EditorManager", "Unknown editor specified via flag: %s",
             editor_name.c_str());
  } else if (editor_type_to_open.has_value()) {
    // Use EditorActivator to ensure layouts and default panels are initialized
    SwitchToEditor(*editor_type_to_open, true, /*from_dialog=*/true);
  }

  // Open panels via PanelManager - works for any editor type
  if (!has_panels) {
    return;
  }

  const size_t session_id = GetCurrentSessionId();
  std::string last_known_category = panel_manager_.GetActiveCategory();
  bool applied_category_from_panel = false;

  for (absl::string_view token :
       absl::StrSplit(panels_str, ',', absl::SkipWhitespace())) {
    if (token.empty()) {
      continue;
    }
    std::string panel_name = std::string(absl::StripAsciiWhitespace(token));
    LOG_DEBUG("EditorManager", "Attempting to open panel: '%s'",
              panel_name.c_str());

    const std::string lower_name = absl::AsciiStrToLower(panel_name);
    if (lower_name == "welcome" || lower_name == "welcome_screen") {
      if (ui_coordinator_) {
        ui_coordinator_->SetWelcomeScreenBehavior(StartupVisibility::kShow);
      }
      continue;
    }
    if (lower_name == "dashboard" || lower_name == "dashboard.main" ||
        lower_name == "editor_selection") {
      if (dashboard_panel_) {
        dashboard_panel_->Show();
      }
      if (ui_coordinator_) {
        ui_coordinator_->SetDashboardBehavior(StartupVisibility::kShow);
      }
      panel_manager_.SetActiveCategory(PanelManager::kDashboardCategory);
      continue;
    }

    // Special case: "Room <id>" opens a dungeon room
    if (absl::StartsWith(panel_name, "Room ")) {
      if (auto* editor_set = GetCurrentEditorSet()) {
        try {
          int room_id = std::stoi(panel_name.substr(5));
          editor_set->GetDungeonEditor()->add_room(room_id);
        } catch (const std::exception& e) {
          LOG_WARN("EditorManager", "Invalid room ID format: %s",
                   panel_name.c_str());
        }
      }
      continue;
    }

    std::optional<std::string> resolved_panel;
    if (panel_manager_.GetPanelDescriptor(session_id, panel_name)) {
      resolved_panel = panel_name;
    } else {
      for (const auto& [prefixed_id, descriptor] :
           panel_manager_.GetAllPanelDescriptors()) {
        const std::string base_id = StripSessionPrefix(prefixed_id);
        const std::string card_lower = absl::AsciiStrToLower(base_id);
        const std::string display_lower =
            absl::AsciiStrToLower(descriptor.display_name);

        if (card_lower == lower_name || display_lower == lower_name) {
          resolved_panel = base_id;
          break;
        }
      }
    }

    if (!resolved_panel.has_value()) {
      LOG_WARN("EditorManager",
               "Unknown panel '%s' from --open_panels (known count: %zu)",
               panel_name.c_str(),
               panel_manager_.GetAllPanelDescriptors().size());
      continue;
    }

    if (panel_manager_.ShowPanel(session_id, *resolved_panel)) {
      const auto* descriptor =
          panel_manager_.GetPanelDescriptor(session_id, *resolved_panel);
      if (descriptor && !applied_category_from_panel &&
          descriptor->category != PanelManager::kDashboardCategory) {
        panel_manager_.SetActiveCategory(descriptor->category);
        applied_category_from_panel = true;
      } else if (!applied_category_from_panel && descriptor &&
                 descriptor->category.empty() && !last_known_category.empty()) {
        panel_manager_.SetActiveCategory(last_known_category);
      }
    } else {
      LOG_WARN("EditorManager", "Failed to show panel '%s'",
               resolved_panel->c_str());
    }
  }
}

void EditorManager::ApplyStartupVisibility(const AppConfig& config) {
  welcome_mode_override_ = config.welcome_mode;
  dashboard_mode_override_ = config.dashboard_mode;
  sidebar_mode_override_ = config.sidebar_mode;
  ApplyStartupVisibilityOverrides();
}

void EditorManager::ApplyStartupVisibilityOverrides() {
  if (ui_coordinator_) {
    ui_coordinator_->SetWelcomeScreenBehavior(welcome_mode_override_);
    ui_coordinator_->SetDashboardBehavior(dashboard_mode_override_);
  }

  if (sidebar_mode_override_ != StartupVisibility::kAuto) {
    const bool sidebar_visible =
        sidebar_mode_override_ == StartupVisibility::kShow;
    panel_manager_.SetSidebarVisible(sidebar_visible);
    if (ui_coordinator_) {
      ui_coordinator_->SetPanelSidebarVisible(sidebar_visible);
    }
  }

  // Force sidebar panel to collapse if Welcome Screen or Dashboard is explicitly shown
  // This prevents visual overlap/clutter on startup
  if (welcome_mode_override_ == StartupVisibility::kShow ||
      dashboard_mode_override_ == StartupVisibility::kShow) {
    panel_manager_.SetPanelExpanded(false);
  }

  if (dashboard_panel_) {
    if (dashboard_mode_override_ == StartupVisibility::kHide) {
      dashboard_panel_->Hide();
    } else if (dashboard_mode_override_ == StartupVisibility::kShow) {
      dashboard_panel_->Show();
    }
  }
}

void EditorManager::ProcessStartupActions(const AppConfig& config) {
  ApplyStartupVisibility(config);
  // Handle startup editor and panels
  std::string panels_str;
  for (size_t i = 0; i < config.open_panels.size(); ++i) {
    if (i > 0)
      panels_str += ",";
    panels_str += config.open_panels[i];
  }
  OpenEditorAndPanelsFromFlags(config.startup_editor, panels_str);

  // Handle jump targets
  if (config.jump_to_room >= 0) {
    JumpToDungeonRoom(config.jump_to_room);
  }
  if (config.jump_to_map >= 0) {
    JumpToOverworldMap(config.jump_to_map);
  }
}

/**
 * @brief Main update loop for the editor application
 *
 * DELEGATION FLOW:
 * 1. Update timing manager for accurate delta time
 * 2. Draw popups (PopupManager) - modal dialogs across all sessions
 * 3. Execute shortcuts (ShortcutManager) - keyboard input handling
 * 4. Draw toasts (ToastManager) - user notifications
 * 5. Iterate all sessions and update active editors
 * 6. Draw session UI (SessionCoordinator) - session switcher, manager
 * 7. Draw sidebar (PanelManager) - card-based editor UI
 *
 * Note: EditorManager retains the main loop to coordinate multi-session
 * updates, but delegates specific drawing/state operations to specialized
 * components.
 */
absl::Status EditorManager::Update() {
  // Process deferred actions from previous frame (both EditorManager and LayoutCoordinator)
  // This ensures actions that modify ImGui state (like layout resets)
  // are executed safely outside of menu/popup rendering contexts
  layout_coordinator_.ProcessDeferredActions();
  if (!deferred_actions_.empty()) {
    std::vector<std::function<void()>> actions_to_execute;
    actions_to_execute.swap(deferred_actions_);
    for (auto& action : actions_to_execute) {
      action();
    }
  }

  // Update timing manager for accurate delta time across the application
  // This fixes animation timing issues that occur when mouse isn't moving
  // or window loses focus
  TimingManager::Get().Update();

  // Check for layout rebuild requests and execute if needed (delegated to LayoutCoordinator)
  bool is_emulator_visible =
      ui_coordinator_ && ui_coordinator_->IsEmulatorVisible();
  EditorType current_type =
      current_editor_ ? current_editor_->type() : EditorType::kUnknown;
  layout_coordinator_.ProcessLayoutRebuild(current_type, is_emulator_visible);

  // Execute keyboard shortcuts (registered via ShortcutConfigurator)
  ExecuteShortcuts(shortcut_manager_);

  // Draw editor selection dialog (managed by UICoordinator)
  if (ui_coordinator_ && ui_coordinator_->IsEditorSelectionVisible()) {
    dashboard_panel_->Show();
    dashboard_panel_->Draw();
    if (!dashboard_panel_->IsVisible()) {
      ui_coordinator_->SetEditorSelectionVisible(false);
    }
  }

  // Draw ROM load options dialog (ZSCustomOverworld, feature flags, project)
  if (show_rom_load_options_) {
    rom_load_options_dialog_.Draw(&show_rom_load_options_);
  }

  // Draw panel browser (managed by UICoordinator)
  if (ui_coordinator_ && ui_coordinator_->IsPanelBrowserVisible()) {
    bool show = true;
    if (activity_bar_) {
      activity_bar_->DrawPanelBrowser(GetCurrentSessionId(), &show);
    }
    if (!show) {
      ui_coordinator_->SetPanelBrowserVisible(false);
    }
  }

  // Update agent editor dashboard (chat drawn via RightPanelManager)
  status_ = agent_ui_.Update();

  // Draw background grid effects for the entire viewport
  if (ui_coordinator_) {
    ui_coordinator_->DrawBackground();
  }

  // Ensure TestManager always has the current ROM
  static Rom* last_test_rom = nullptr;
  auto* current_rom = GetCurrentRom();
  if (last_test_rom != current_rom) {
    LOG_DEBUG(
        "EditorManager",
        "EditorManager::Update - ROM changed, updating TestManager: %p -> "
        "%p",
        (void*)last_test_rom, (void*)current_rom);
    test::TestManager::Get().SetCurrentRom(current_rom);
    last_test_rom = current_rom;
  }

  // CRITICAL: Draw UICoordinator UI components FIRST (before ROM checks)
  // This ensures Welcome Screen, Command Palette, etc. work even without ROM
  // loaded
  if (ui_coordinator_) {
    ui_coordinator_->DrawAllUI();
  }

  // Get current editor set for sidebar/panel logic (needed before early return)
  auto* current_editor_set = GetCurrentEditorSet();

  // Draw sidebar BEFORE early return so it appears even when no ROM is loaded
  // This fixes the issue where sidebar/panel drawing was unreachable without ROM
  if (ui_coordinator_ && ui_coordinator_->IsPanelSidebarVisible()) {
    // Get ALL editor categories (static list, always shown)
    auto all_categories = EditorRegistry::GetAllEditorCategories();

    // Track which editors are currently active (for visual highlighting)
    std::unordered_set<std::string> active_editor_categories;

    if (current_editor_set && session_coordinator_) {
      // ROM is loaded - collect active editors for highlighting
      for (size_t session_idx = 0;
           session_idx < session_coordinator_->GetTotalSessionCount();
           ++session_idx) {
        auto* session = static_cast<RomSession*>(
            session_coordinator_->GetSession(session_idx));
        if (!session || !session->rom.is_loaded()) {
          continue;
        }

        for (auto* editor : session->editors.active_editors_) {
          if (*editor->active() && IsPanelBasedEditor(editor->type())) {
            std::string category =
                EditorRegistry::GetEditorCategory(editor->type());
            active_editor_categories.insert(category);
          }
        }
      }

      // Add Emulator to active categories when it's visible
      if (ui_coordinator_->IsEmulatorVisible()) {
        active_editor_categories.insert("Emulator");
      }
    }

    // Determine which category to show in sidebar
    std::string sidebar_category = panel_manager_.GetActiveCategory();

    // If no active category, default to first in list
    if (sidebar_category.empty() && !all_categories.empty()) {
      sidebar_category = all_categories[0];
      panel_manager_.SetActiveCategory(sidebar_category);
    }

    // Callback to check if ROM is loaded (for category enabled state)
    auto has_rom_callback = [this]() -> bool {
      auto* rom = GetCurrentRom();
      return rom && rom->is_loaded();
    };

    // Draw VSCode-style sidebar with ALL categories (highlighting active ones)
    // Activity Bar is hidden until ROM is loaded (per startup flow design)
    if (activity_bar_ && ui_coordinator_->ShouldShowActivityBar()) {
      activity_bar_->Render(GetCurrentSessionId(), sidebar_category,
                            all_categories, active_editor_categories,
                            has_rom_callback);
    }
  }

  // Draw right panel BEFORE early return (agent chat, proposals, settings)
  if (right_panel_manager_) {
    right_panel_manager_->SetRom(GetCurrentRom());
    right_panel_manager_->Draw();
  }

  // Update and draw status bar
  status_bar_.SetRom(GetCurrentRom());
  if (session_coordinator_) {
    status_bar_.SetSessionInfo(GetCurrentSessionId(),
                               session_coordinator_->GetActiveSessionCount());
  }
  status_bar_.Draw();

  // Autosave timer
  if (user_settings_.prefs().autosave_enabled && current_rom &&
      current_rom->dirty()) {
    autosave_timer_ += ImGui::GetIO().DeltaTime;
    if (autosave_timer_ >= user_settings_.prefs().autosave_interval) {
      autosave_timer_ = 0.0f;
      Rom::SaveSettings s;
      s.backup = true;
      s.save_new = false;
      auto st = current_rom->SaveToFile(s);
      if (st.ok()) {
        toast_manager_.Show("Autosave completed", editor::ToastType::kSuccess);
      } else {
        toast_manager_.Show(std::string(st.message()),
                            editor::ToastType::kError, 5.0f);
      }
    }
  } else {
    autosave_timer_ = 0.0f;
  }

  // Check if ROM is loaded before allowing editor updates
  if (!current_editor_set) {
    // No ROM loaded - welcome screen shown by UICoordinator above
    // Sidebar and right panel have already been drawn above
    return absl::OkStatus();
  }

  // Check if current ROM is valid
  if (!current_rom) {
    // No ROM loaded - welcome screen shown by UICoordinator above
    return absl::OkStatus();
  }

  // ROM is loaded and valid - don't auto-show welcome screen
  // Welcome screen should only be shown manually at this point

  // Delegate session updates to SessionCoordinator
  if (session_coordinator_) {
    session_coordinator_->UpdateSessions();
  }

  // Central panel drawing - once per frame for all EditorPanel instances
  // This draws panels based on active category, respecting pinned and persistent panels
  panel_manager_.DrawAllVisiblePanels();

  if (ui_coordinator_ && ui_coordinator_->IsPerformanceDashboardVisible()) {
    gfx::PerformanceDashboard::Get().Render();
  }

  // Proposal drawer is now drawn through RightPanelManager
  // Removed duplicate direct call - DrawProposalsPanel() in RightPanelManager handles it

  // Update ROM context for agent UI
  if (current_rom && current_rom->is_loaded()) {
    agent_ui_.SetRomContext(current_rom);
    agent_ui_.SetProjectContext(&current_project_);
    // Pass AsarWrapper instance from AssemblyEditor
    if (auto* editor_set = GetCurrentEditorSet()) {
      agent_ui_.SetAsarWrapperContext(
          editor_set->GetAssemblyEditor()->asar_wrapper());
    }
  }

  // Draw SessionCoordinator UI components
  if (session_coordinator_) {
    session_coordinator_->DrawSessionSwitcher();
    session_coordinator_->DrawSessionManager();
    session_coordinator_->DrawSessionRenameDialog();
  }

  return absl::OkStatus();
}

// DrawContextSensitivePanelControl removed - card control is now in the sidebar

/**
 * @brief Draw the main menu bar
 *
 * DELEGATION:
 * - Menu items: MenuOrchestrator::BuildMainMenu()
 * - ROM selector: EditorManager::DrawRomSelector() (inline, needs current_rom_
 * access)
 * - Menu bar extras: UICoordinator::DrawMenuBarExtras() (session indicator,
 * version)
 *
 * Note: ROM selector stays in EditorManager because it needs direct access to
 * sessions_ and current_rom_ for the combo box. Could be extracted to
 * SessionCoordinator in future.
 */
void EditorManager::DrawMenuBar() {
  static bool show_display_settings = false;

  if (ImGui::BeginMenuBar()) {
    // Sidebar toggle - Activity Bar visibility
    // Consistent button styling with other menubar buttons
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          gui::GetSurfaceContainerHighVec4());
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          gui::GetSurfaceContainerHighestVec4());

    // Highlight when active/visible
    if (panel_manager_.IsSidebarVisible()) {
      ImGui::PushStyleColor(ImGuiCol_Text, gui::GetPrimaryVec4());
    } else {
      ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextSecondaryVec4());
    }

    if (ui_coordinator_ && ui_coordinator_->IsPanelSidebarVisible()) {
      if (ImGui::SmallButton(ICON_MD_MENU)) {
        panel_manager_.ToggleSidebarVisibility();
      }
    } else {
      if (ImGui::SmallButton(ICON_MD_MENU_OPEN)) {
        panel_manager_.ToggleSidebarVisibility();
      }
    }

    ImGui::PopStyleColor(4);

    if (ImGui::IsItemHovered()) {
      const char* tooltip = panel_manager_.IsSidebarVisible()
                                ? "Hide Activity Bar (Ctrl+B)"
                                : "Show Activity Bar (Ctrl+B)";
      ImGui::SetTooltip("%s", tooltip);
    }

    // Delegate menu building to MenuOrchestrator
    if (menu_orchestrator_) {
      menu_orchestrator_->BuildMainMenu();
    }

    // Delegate menu bar extras to UICoordinator (status cluster on right)
    if (ui_coordinator_) {
      ui_coordinator_->DrawMenuBarExtras();
    }

    ImGui::EndMenuBar();
  }

  if (show_display_settings) {
    // Use the popup manager instead of a separate window
    popup_manager_->Show(PopupID::kDisplaySettings);
    show_display_settings = false;  // Close the old-style window
  }

  // ImGui debug windows (delegated to UICoordinator for visibility state)
  if (ui_coordinator_ && ui_coordinator_->IsImGuiDemoVisible()) {
    bool visible = true;
    ImGui::ShowDemoWindow(&visible);
    if (!visible) {
      ui_coordinator_->SetImGuiDemoVisible(false);
    }
  }

  if (ui_coordinator_ && ui_coordinator_->IsImGuiMetricsVisible()) {
    bool visible = true;
    ImGui::ShowMetricsWindow(&visible);
    if (!visible) {
      ui_coordinator_->SetImGuiMetricsVisible(false);
    }
  }

  // Using PanelManager directly
  if (auto* editor_set = GetCurrentEditorSet()) {
    // Pass the actual visibility flag pointer so the X button works
    bool* hex_visibility =
        panel_manager_.GetVisibilityFlag("memory.hex_editor");
    if (hex_visibility && *hex_visibility) {
      editor_set->GetMemoryEditor()->Update(*hex_visibility);
    }

    if (ui_coordinator_ && ui_coordinator_->IsAsmEditorVisible()) {
      bool visible = true;
      editor_set->GetAssemblyEditor()->Update(visible);
      if (!visible) {
        ui_coordinator_->SetAsmEditorVisible(false);
      }
    }
  }

  // Project file editor
  project_file_editor_.Draw();
  if (ui_coordinator_ && ui_coordinator_->IsPerformanceDashboardVisible()) {
    gfx::PerformanceDashboard::Get().SetVisible(true);
    gfx::PerformanceDashboard::Get().Update();
    gfx::PerformanceDashboard::Get().Render();
    if (!gfx::PerformanceDashboard::Get().IsVisible()) {
      ui_coordinator_->SetPerformanceDashboardVisible(false);
    }
  }

  // Testing interface (only when tests are enabled)
#ifdef YAZE_ENABLE_TESTING
  if (show_test_dashboard_) {
    auto& test_manager = test::TestManager::Get();
    test_manager.UpdateResourceStats();  // Update monitoring data
    test_manager.DrawTestDashboard(&show_test_dashboard_);
  }
#endif

  // Update proposal drawer ROM context (drawing handled by RightPanelManager)
  proposal_drawer_.SetRom(GetCurrentRom());

  // Agent chat history popup (left side)
  agent_ui_.DrawPopups();

  // Welcome screen is now drawn by UICoordinator::DrawAllUI()
  // Removed duplicate call to avoid showing welcome screen twice

  // Emulator handling - run with UI when visible, or headless when running in background
  if (auto* current_rom = GetCurrentRom()) {
    if (ui_coordinator_ && ui_coordinator_->IsEmulatorVisible()) {
      // Full emulator with UI
      emulator_.Run(current_rom);
    } else if (emulator_.running() && emulator_.is_snes_initialized()) {
      // Emulator running in background (e.g., for music editor playback)
      // Use audio-focused mode when available for lower overhead and authentic sound
      if (emulator_.is_audio_focus_mode()) {
        emulator_.RunAudioFrame();
      } else {
        emulator_.RunFrameOnly();
      }
    }
  }

  // Enhanced Global Search UI (managed by UICoordinator)
  // No longer here - handled by ui_coordinator_->DrawAllUI()

  // NOTE: Editor updates are handled by SessionCoordinator::UpdateSessions()
  // which is called in EditorManager::Update(). Removed duplicate update loop
  // here that was causing EditorPanel::Begin() to be called twice per frame,
  // resulting in duplicate rendering detection logs.

  if (ui_coordinator_ && ui_coordinator_->IsResourceLabelManagerVisible() &&
      GetCurrentRom()) {
    bool visible = true;
    GetCurrentRom()->resource_label()->DisplayLabels(&visible);
    if (current_project_.project_opened() &&
        !current_project_.labels_filename.empty()) {
      current_project_.labels_filename =
          GetCurrentRom()->resource_label()->filename_;
    }
    if (!visible) {
      ui_coordinator_->SetResourceLabelManagerVisible(false);
    }
  }

  // Workspace preset dialogs are now in UICoordinator

  // Layout presets UI (session dialogs are drawn by SessionCoordinator at lines
  // 907-915)
  if (ui_coordinator_) {
    ui_coordinator_->DrawLayoutPresets();
  }
}

/**
 * @brief Load a ROM file into a new or existing session
 *
 * DELEGATION:
 * - File dialog: util::FileDialogWrapper
 * - ROM loading: RomFileManager::LoadRom()
 * - Session management: EditorManager (searches for empty session or creates
 * new)
 * - Dependency injection: ConfigureEditorDependencies()
 * - Asset loading: LoadAssets() (calls Initialize/Load on all editors)
 * - UI updates: UICoordinator (hides welcome, shows editor selection)
 *
 * FLOW:
 * 1. Show file dialog and get filename
 * 2. Check for duplicate sessions (prevent opening same ROM twice)
 * 3. Load ROM via RomFileManager into temp_rom
 * 4. Find empty session or create new session
 * 5. Move ROM into session and set current pointers
 * 6. Configure editor dependencies for the session
 * 7. Load all editor assets
 * 8. Update UI state and recent files
 */
absl::Status EditorManager::LoadRom() {
  auto load_from_path = [this](const std::string& file_name) -> absl::Status {
    if (file_name.empty()) {
      return absl::OkStatus();
    }

    // Check if this is a project file - route to project loading
    if (absl::StrContains(file_name, ".yaze")) {
      return OpenRomOrProject(file_name);
    }

    if (session_coordinator_->HasDuplicateSession(file_name)) {
      toast_manager_.Show("ROM already open in another session",
                          editor::ToastType::kWarning);
      return absl::OkStatus();
    }

    // Delegate ROM loading to RomFileManager
    Rom temp_rom;
    RETURN_IF_ERROR(rom_file_manager_.LoadRom(&temp_rom, file_name));

    auto session_or = session_coordinator_->CreateSessionFromRom(
        std::move(temp_rom), file_name);
    if (!session_or.ok()) {
      return session_or.status();
    }

    ConfigureEditorDependencies(GetCurrentEditorSet(), GetCurrentRom(),
                                GetCurrentSessionId());

    // Initialize resource labels for LoadRom() - use defaults with current project settings
    auto& label_provider = zelda3::GetResourceLabels();
    label_provider.SetProjectLabels(&current_project_.resource_labels);
    label_provider.SetPreferHMagicNames(
        current_project_.workspace_settings.prefer_hmagic_names);
    LOG_INFO("EditorManager", "Initialized ResourceLabelProvider for LoadRom");

#ifdef YAZE_ENABLE_TESTING
    test::TestManager::Get().SetCurrentRom(GetCurrentRom());
#endif

    auto& manager = project::RecentFilesManager::GetInstance();
    manager.AddFile(file_name);
    manager.Save();

    RETURN_IF_ERROR(LoadAssets());

    if (ui_coordinator_) {
      ui_coordinator_->SetWelcomeScreenVisible(false);

      // Show ROM load options dialog for ZSCustomOverworld and feature settings
      rom_load_options_dialog_.Open(GetCurrentRom());
      show_rom_load_options_ = true;
    }

    return absl::OkStatus();
  };

#if defined(__APPLE__) && TARGET_OS_IOS == 1
  util::FileDialogWrapper::ShowOpenFileDialogAsync(
      util::MakeRomFileDialogOptions(false),
      [this, load_from_path](const std::string& file_name) {
        auto status = load_from_path(file_name);
        if (!status.ok()) {
          toast_manager_.Show(
              absl::StrFormat("Failed to load ROM: %s", status.message()),
              ToastType::kError);
        }
      });
  return absl::OkStatus();
#else
  auto file_name = util::FileDialogWrapper::ShowOpenFileDialog(
      util::MakeRomFileDialogOptions(false));
  return load_from_path(file_name);
#endif
}

absl::Status EditorManager::LoadAssets(uint64_t passed_handle) {
  auto* current_rom = GetCurrentRom();
  auto* current_editor_set = GetCurrentEditorSet();
  if (!current_rom || !current_editor_set) {
    return absl::FailedPreconditionError("No ROM or editor set loaded");
  }

  auto start_time = std::chrono::steady_clock::now();

#ifdef __EMSCRIPTEN__
  // Use passed handle if provided, otherwise create new one
  auto loading_handle =
      passed_handle != 0
          ? static_cast<app::platform::WasmLoadingManager::LoadingHandle>(
                passed_handle)
          : app::platform::WasmLoadingManager::BeginLoading(
                "Loading Editor Assets");

  // Progress starts at 10% (ROM already loaded), goes to 100%
  constexpr float kStartProgress = 0.10f;
  constexpr float kEndProgress = 1.0f;
  constexpr int kTotalSteps = 11;  // Graphics + 8 editors + profiler + finish
  int current_step = 0;
  auto update_progress = [&](const std::string& message) {
    current_step++;
    float progress =
        kStartProgress + (kEndProgress - kStartProgress) *
                             (static_cast<float>(current_step) / kTotalSteps);
    app::platform::WasmLoadingManager::UpdateProgress(loading_handle, progress);
    app::platform::WasmLoadingManager::UpdateMessage(loading_handle, message);
  };
  // RAII guard to ensure loading indicator is closed even on early return
  auto cleanup_loading = [&]() {
    app::platform::WasmLoadingManager::EndLoading(loading_handle);
  };
  struct LoadingGuard {
    std::function<void()> cleanup;
    bool dismissed = false;
    ~LoadingGuard() {
      if (!dismissed)
        cleanup();
    }
    void dismiss() { dismissed = true; }
  } loading_guard{cleanup_loading};
#else
  (void)passed_handle;  // Unused on non-WASM
#endif

  // Set renderer for emulator (lazy initialization happens in Run())
  if (renderer_) {
    emulator_.set_renderer(renderer_);
  }

  // Initialize all editors - this registers their cards with PanelManager
  // and sets up any editor-specific resources. Must be called before Load().
  current_editor_set->GetOverworldEditor()->Initialize();
  current_editor_set->GetMessageEditor()->Initialize();
  current_editor_set->GetGraphicsEditor()->Initialize();
  current_editor_set->GetScreenEditor()->Initialize();
  current_editor_set->GetSpriteEditor()->Initialize();
  current_editor_set->GetPaletteEditor()->Initialize();
  current_editor_set->GetAssemblyEditor()->Initialize();
  current_editor_set->GetMusicEditor()->Initialize();

  // Initialize the dungeon editor with the renderer
  current_editor_set->GetDungeonEditor()->Initialize(renderer_, current_rom);

#ifdef __EMSCRIPTEN__
  update_progress("Loading graphics sheets...");
#endif
  // Get current session's GameData and load Zelda3-specific game data
  auto* current_session = session_coordinator_->GetActiveRomSession();
  if (!current_session) {
    return absl::FailedPreconditionError("No active ROM session");
  }

  // Load all Zelda3-specific data (metadata, palettes, gfx groups, graphics)
  RETURN_IF_ERROR(
      zelda3::LoadGameData(*current_rom, current_session->game_data));

  // Copy loaded graphics to Arena for global access
  *gfx::Arena::Get().mutable_gfx_sheets() =
      current_session->game_data.gfx_bitmaps;

  // Propagate GameData to all editors that need it
  auto* game_data = &current_session->game_data;
  current_editor_set->GetDungeonEditor()->SetGameData(game_data);
  current_editor_set->GetOverworldEditor()->SetGameData(game_data);
  current_editor_set->GetGraphicsEditor()->SetGameData(game_data);
  current_editor_set->GetScreenEditor()->SetGameData(game_data);
  current_editor_set->GetPaletteEditor()->SetGameData(game_data);
  current_editor_set->GetSpriteEditor()->SetGameData(game_data);
  current_editor_set->GetMessageEditor()->SetGameData(game_data);

#ifdef __EMSCRIPTEN__
  update_progress("Loading overworld...");
#endif
  RETURN_IF_ERROR(current_editor_set->GetOverworldEditor()->Load());

#ifdef __EMSCRIPTEN__
  update_progress("Loading dungeons...");
#endif
  RETURN_IF_ERROR(current_editor_set->GetDungeonEditor()->Load());

#ifdef __EMSCRIPTEN__
  update_progress("Loading screen editor...");
#endif
  RETURN_IF_ERROR(current_editor_set->GetScreenEditor()->Load());

#ifdef __EMSCRIPTEN__
  update_progress("Loading graphics editor...");
#endif
  RETURN_IF_ERROR(current_editor_set->GetGraphicsEditor()->Load());

#ifdef __EMSCRIPTEN__
  update_progress("Loading settings...");
#endif
  // Settings panel doesn't need Load()
  // RETURN_IF_ERROR(current_editor_set->settings_editor_.Load());

#ifdef __EMSCRIPTEN__
  update_progress("Loading sprites...");
#endif
  RETURN_IF_ERROR(current_editor_set->GetSpriteEditor()->Load());

#ifdef __EMSCRIPTEN__
  update_progress("Loading messages...");
#endif
  RETURN_IF_ERROR(current_editor_set->GetMessageEditor()->Load());

#ifdef __EMSCRIPTEN__
  update_progress("Loading music...");
#endif
  RETURN_IF_ERROR(current_editor_set->GetMusicEditor()->Load());

#ifdef __EMSCRIPTEN__
  update_progress("Loading palettes...");
#endif
  RETURN_IF_ERROR(current_editor_set->GetPaletteEditor()->Load());

#ifdef __EMSCRIPTEN__
  update_progress("Finishing up...");
#endif

  // Set up RightPanelManager with session's settings editor
  if (right_panel_manager_) {
    auto* settings = current_editor_set->GetSettingsPanel();
    right_panel_manager_->SetSettingsPanel(settings);
    // Also update project context for settings panel
    if (settings) {
      settings->SetProject(&current_project_);
    }
  }

  // Set up StatusBar reference on settings panel for live toggling
  if (auto* settings = current_editor_set->GetSettingsPanel()) {
    settings->SetStatusBar(&status_bar_);
  }

  // Apply user preferences to status bar
  status_bar_.SetEnabled(user_settings_.prefs().show_status_bar);

  gfx::PerformanceProfiler::Get().PrintSummary();

#ifdef __EMSCRIPTEN__
  // Dismiss the guard and manually close - we completed successfully
  loading_guard.dismiss();
  app::platform::WasmLoadingManager::EndLoading(loading_handle);
#endif

  auto end_time = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);
  LOG_DEBUG("EditorManager", "ROM assets loaded in %lld ms", duration.count());

  return absl::OkStatus();
}

/**
 * @brief Save the current ROM file
 *
 * DELEGATION:
 * - Editor data saving: Each editor's Save() method (overworld, dungeon, etc.)
 * - ROM file writing: RomFileManager::SaveRom()
 *
 * RESPONSIBILITIES STILL IN EDITORMANAGER:
 * - Coordinating editor saves (dungeon maps, overworld maps, graphics sheets)
 * - Checking feature flags to determine what to save
 * - Accessing current session's editors
 *
 * This stays in EditorManager because it requires knowledge of all editors
 * and the order in which they must be saved to maintain ROM integrity.
 */
absl::Status EditorManager::SaveRom() {
  auto* current_rom = GetCurrentRom();
  auto* current_editor_set = GetCurrentEditorSet();
  if (!current_rom || !current_editor_set) {
    return absl::FailedPreconditionError("No ROM or editor set loaded");
  }

  // Save editor-specific data first
  if (core::FeatureFlags::get().kSaveDungeonMaps) {
    RETURN_IF_ERROR(zelda3::SaveDungeonMaps(
        *current_rom, current_editor_set->GetScreenEditor()->dungeon_maps_));
  }

  RETURN_IF_ERROR(current_editor_set->GetOverworldEditor()->Save());

  if (core::FeatureFlags::get().kSaveGraphicsSheet)
    RETURN_IF_ERROR(zelda3::SaveAllGraphicsData(
        *current_rom, gfx::Arena::Get().gfx_sheets()));

  // Delegate final ROM file writing to RomFileManager
  return rom_file_manager_.SaveRom(current_rom);
}

absl::Status EditorManager::SaveRomAs(const std::string& filename) {
  auto* current_rom = GetCurrentRom();
  if (!current_rom) {
    return absl::FailedPreconditionError("No ROM loaded");
  }

  // Reuse SaveRom() logic for editor-specific data saving
  RETURN_IF_ERROR(SaveRom());

  // Now save to the new filename
  auto save_status = rom_file_manager_.SaveRomAs(current_rom, filename);
  if (save_status.ok()) {
    // Update session filepath
    if (session_coordinator_) {
      auto* session = session_coordinator_->GetActiveRomSession();
      if (session) {
        session->filepath = filename;
      }
    }

    // Add to recent files
    auto& manager = project::RecentFilesManager::GetInstance();
    manager.AddFile(filename);
    manager.Save();
  }

  return save_status;
}

absl::Status EditorManager::OpenRomOrProject(const std::string& filename) {
  LOG_INFO("EditorManager", "OpenRomOrProject called with: '%s'",
           filename.c_str());
  if (filename.empty()) {
    LOG_INFO("EditorManager", "Empty filename provided, skipping load.");
    return absl::OkStatus();
  }

#ifdef __EMSCRIPTEN__
  // Start loading indicator early for WASM builds
  auto loading_handle =
      app::platform::WasmLoadingManager::BeginLoading("Loading ROM");
  app::platform::WasmLoadingManager::UpdateMessage(loading_handle,
                                                   "Reading ROM file...");
  // RAII guard to ensure loading indicator is closed even on early return
  struct LoadingGuard {
    app::platform::WasmLoadingManager::LoadingHandle handle;
    bool dismissed = false;
    ~LoadingGuard() {
      if (!dismissed)
        app::platform::WasmLoadingManager::EndLoading(handle);
    }
    void dismiss() { dismissed = true; }
  } loading_guard{loading_handle};
#endif

  if (absl::StrContains(filename, ".yaze")) {
    // Open the project file
    RETURN_IF_ERROR(current_project_.Open(filename));

    // Initialize VersionManager for the project
    version_manager_ =
        std::make_unique<core::VersionManager>(&current_project_);
    version_manager_->InitializeGit();  // Try to init git if configured

    // Load ROM directly from project - don't prompt user
    return LoadProjectWithRom();
  } else {
#ifdef __EMSCRIPTEN__
    app::platform::WasmLoadingManager::UpdateProgress(loading_handle, 0.05f);
    app::platform::WasmLoadingManager::UpdateMessage(loading_handle,
                                                     "Loading ROM data...");
#endif
    Rom temp_rom;
    RETURN_IF_ERROR(rom_file_manager_.LoadRom(&temp_rom, filename));

    auto session_or = session_coordinator_->CreateSessionFromRom(
        std::move(temp_rom), filename);
    if (!session_or.ok()) {
      return session_or.status();
    }
    RomSession* session = *session_or;

    ConfigureEditorDependencies(GetCurrentEditorSet(), GetCurrentRom(),
                                GetCurrentSessionId());

    // Apply project feature flags to both session and global singleton
    session->feature_flags = current_project_.feature_flags;
    core::FeatureFlags::get() = current_project_.feature_flags;

    // Initialize resource labels for ROM-only loading (use defaults)
    // This ensures labels are available before any editors access them
    auto& label_provider = zelda3::GetResourceLabels();
    label_provider.SetProjectLabels(&current_project_.resource_labels);
    label_provider.SetPreferHMagicNames(
        current_project_.workspace_settings.prefer_hmagic_names);
    LOG_INFO("EditorManager",
             "Initialized ResourceLabelProvider for ROM-only load");

    // Update test manager with current ROM for ROM-dependent tests (only when
    // tests are enabled)
#ifdef YAZE_ENABLE_TESTING
    LOG_DEBUG("EditorManager", "Setting ROM in TestManager - %p ('%s')",
              (void*)GetCurrentRom(),
              GetCurrentRom() ? GetCurrentRom()->title().c_str() : "null");
    test::TestManager::Get().SetCurrentRom(GetCurrentRom());
#endif

    if (auto* editor_set = GetCurrentEditorSet();
        editor_set && !current_project_.code_folder.empty()) {
      editor_set->GetAssemblyEditor()->OpenFolder(current_project_.code_folder);
      // Also set the sidebar file browser path
      panel_manager_.SetFileBrowserPath("Assembly",
                                        current_project_.code_folder);
    }

#ifdef __EMSCRIPTEN__
    app::platform::WasmLoadingManager::UpdateProgress(loading_handle, 0.10f);
    app::platform::WasmLoadingManager::UpdateMessage(loading_handle,
                                                     "Initializing editors...");
    // Pass the loading handle to LoadAssets and dismiss our guard
    // LoadAssets will manage closing the indicator when done
    loading_guard.dismiss();
    RETURN_IF_ERROR(LoadAssets(loading_handle));
#else
    RETURN_IF_ERROR(LoadAssets());
#endif

    // Hide welcome screen and show editor selection when ROM is loaded
    ui_coordinator_->SetWelcomeScreenVisible(false);
    // dashboard_panel_->ClearRecentEditors();
    ui_coordinator_->SetEditorSelectionVisible(true);

    // Set Dashboard category to suppress panel drawing until user selects an editor
    panel_manager_.SetActiveCategory(PanelManager::kDashboardCategory);
  }
  return absl::OkStatus();
}

absl::Status EditorManager::CreateNewProject(const std::string& template_name) {
  // Delegate to ProjectManager
  auto status = project_manager_.CreateNewProject(template_name);
  if (status.ok()) {
    current_project_ = project_manager_.GetCurrentProject();

    // Trigger ROM selection dialog - projects need a ROM to be useful
    // LoadRom() opens file dialog and shows ROM load options when ROM is loaded
    status = LoadRom();
#if !(defined(__APPLE__) && TARGET_OS_IOS == 1)
    if (status.ok() && ui_coordinator_) {
      ui_coordinator_->SetWelcomeScreenVisible(false);
      ui_coordinator_->SetWelcomeScreenManuallyClosed(true);
    }
#endif
  }
  return status;
}

absl::Status EditorManager::OpenProject() {
  auto open_project_from_path =
      [this](const std::string& file_path) -> absl::Status {
    if (file_path.empty()) {
      return absl::OkStatus();
    }

    project::YazeProject new_project;
    RETURN_IF_ERROR(new_project.Open(file_path));

    // Validate project
    auto validation_status = new_project.Validate();
    if (!validation_status.ok()) {
      toast_manager_.Show(absl::StrFormat("Project validation failed: %s",
                                          validation_status.message()),
                          editor::ToastType::kWarning, 5.0f);

      // Ask user if they want to repair
      popup_manager_->Show("Project Repair");
    }

    current_project_ = std::move(new_project);

    // Initialize VersionManager for the project
    version_manager_ =
        std::make_unique<core::VersionManager>(&current_project_);
    version_manager_->InitializeGit();

    return LoadProjectWithRom();
  };

#if defined(__APPLE__) && TARGET_OS_IOS == 1
  util::FileDialogWrapper::ShowOpenFileDialogAsync(
      util::FileDialogOptions{},
      [this, open_project_from_path](const std::string& file_path) {
        auto status = open_project_from_path(file_path);
        if (!status.ok()) {
          toast_manager_.Show(
              absl::StrFormat("Failed to open project: %s", status.message()),
              ToastType::kError);
        }
      });
  return absl::OkStatus();
#else
  auto file_path = util::FileDialogWrapper::ShowOpenFileDialog();
  return open_project_from_path(file_path);
#endif
}

absl::Status EditorManager::LoadProjectWithRom() {
  // Check if project has a ROM file specified
  if (current_project_.rom_filename.empty()) {
    // No ROM specified - prompt user to select one
    toast_manager_.Show(
        "Project has no ROM file configured. Please select a ROM.",
        editor::ToastType::kInfo);
#if defined(__APPLE__) && TARGET_OS_IOS == 1
    util::FileDialogWrapper::ShowOpenFileDialogAsync(
        util::MakeRomFileDialogOptions(false),
        [this](const std::string& rom_path) {
          if (rom_path.empty()) {
            return;
          }
          current_project_.rom_filename = rom_path;
          auto save_status = current_project_.Save();
          if (!save_status.ok()) {
            toast_manager_.Show(
                absl::StrFormat("Failed to update project ROM: %s",
                                save_status.message()),
                ToastType::kError);
            return;
          }
          auto status = LoadProjectWithRom();
          if (!status.ok()) {
            toast_manager_.Show(
                absl::StrFormat("Failed to load project ROM: %s",
                                status.message()),
                ToastType::kError);
          }
        });
    return absl::OkStatus();
#else
    auto rom_path = util::FileDialogWrapper::ShowOpenFileDialog(
        util::MakeRomFileDialogOptions(false));
    if (rom_path.empty()) {
      return absl::OkStatus();
    }
    current_project_.rom_filename = rom_path;
    // Save updated project
    RETURN_IF_ERROR(current_project_.Save());
#endif
  }

  // Load ROM from project
  Rom temp_rom;
  auto load_status =
      rom_file_manager_.LoadRom(&temp_rom, current_project_.rom_filename);
  if (!load_status.ok()) {
    // ROM file not found or invalid - prompt user to select new ROM
    toast_manager_.Show(
        absl::StrFormat("Could not load ROM '%s': %s. Please select a new ROM.",
                        current_project_.rom_filename, load_status.message()),
        editor::ToastType::kWarning, 5.0f);
#if defined(__APPLE__) && TARGET_OS_IOS == 1
    util::FileDialogWrapper::ShowOpenFileDialogAsync(
        util::MakeRomFileDialogOptions(false),
        [this](const std::string& rom_path) {
          if (rom_path.empty()) {
            return;
          }
          current_project_.rom_filename = rom_path;
          auto save_status = current_project_.Save();
          if (!save_status.ok()) {
            toast_manager_.Show(
                absl::StrFormat("Failed to update project ROM: %s",
                                save_status.message()),
                ToastType::kError);
            return;
          }
          auto status = LoadProjectWithRom();
          if (!status.ok()) {
            toast_manager_.Show(
                absl::StrFormat("Failed to load project ROM: %s",
                                status.message()),
                ToastType::kError);
          }
        });
    return absl::OkStatus();
#else
    auto rom_path = util::FileDialogWrapper::ShowOpenFileDialog(
        util::MakeRomFileDialogOptions(false));
    if (rom_path.empty()) {
      return absl::OkStatus();
    }
    current_project_.rom_filename = rom_path;
    RETURN_IF_ERROR(current_project_.Save());
    RETURN_IF_ERROR(rom_file_manager_.LoadRom(&temp_rom, rom_path));
#endif
  }

  auto session_or = session_coordinator_->CreateSessionFromRom(
      std::move(temp_rom), current_project_.rom_filename);
  if (!session_or.ok()) {
    return session_or.status();
  }
  RomSession* session = *session_or;

  ConfigureEditorDependencies(GetCurrentEditorSet(), GetCurrentRom(),
                              GetCurrentSessionId());

  // Apply project feature flags to both session and global singleton
  session->feature_flags = current_project_.feature_flags;
  core::FeatureFlags::get() = current_project_.feature_flags;

  // Update test manager with current ROM for ROM-dependent tests (only when
  // tests are enabled)
#ifdef YAZE_ENABLE_TESTING
  LOG_DEBUG("EditorManager", "Setting ROM in TestManager - %p ('%s')",
            (void*)GetCurrentRom(),
            GetCurrentRom() ? GetCurrentRom()->title().c_str() : "null");
  test::TestManager::Get().SetCurrentRom(GetCurrentRom());
#endif

  if (auto* editor_set = GetCurrentEditorSet();
      editor_set && !current_project_.code_folder.empty()) {
    editor_set->GetAssemblyEditor()->OpenFolder(current_project_.code_folder);
    // Also set the sidebar file browser path
    panel_manager_.SetFileBrowserPath("Assembly", current_project_.code_folder);
  }

  RETURN_IF_ERROR(LoadAssets());

  // Hide welcome screen and show editor selection when project ROM is loaded
  if (ui_coordinator_) {
    ui_coordinator_->SetWelcomeScreenVisible(false);
    ui_coordinator_->SetEditorSelectionVisible(true);
  }

  // Set Dashboard category to suppress panel drawing until user selects an editor
  panel_manager_.SetActiveCategory(PanelManager::kDashboardCategory);

  // Apply workspace settings
  user_settings_.prefs().font_global_scale =
      current_project_.workspace_settings.font_global_scale;
  user_settings_.prefs().autosave_enabled =
      current_project_.workspace_settings.autosave_enabled;
  user_settings_.prefs().autosave_interval =
      current_project_.workspace_settings.autosave_interval_secs;
  ImGui::GetIO().FontGlobalScale = user_settings_.prefs().font_global_scale;

  // Initialize resource labels early - before any editors access them
  current_project_.InitializeResourceLabelProvider();
  LOG_INFO("EditorManager",
           "Initialized ResourceLabelProvider with project labels");

  // Add to recent files
  auto& manager = project::RecentFilesManager::GetInstance();
  manager.AddFile(current_project_.filepath);
  manager.Save();

  // Update project management panel with loaded project
  if (project_management_panel_) {
    project_management_panel_->SetProject(&current_project_);
    project_management_panel_->SetVersionManager(version_manager_.get());
    project_management_panel_->SetRom(GetCurrentRom());
  }

  toast_manager_.Show(absl::StrFormat("Project '%s' loaded successfully",
                                      current_project_.GetDisplayName()),
                      editor::ToastType::kSuccess);

  return absl::OkStatus();
}

absl::Status EditorManager::SaveProject() {
  if (!current_project_.project_opened()) {
    return CreateNewProject();
  }

  // Update project with current settings
  if (GetCurrentRom() && GetCurrentEditorSet()) {
    if (session_coordinator_) {
      auto* session = session_coordinator_->GetActiveRomSession();
      if (session) {
        current_project_.feature_flags = session->feature_flags;
      }
    }

    current_project_.workspace_settings.font_global_scale =
        user_settings_.prefs().font_global_scale;
    current_project_.workspace_settings.autosave_enabled =
        user_settings_.prefs().autosave_enabled;
    current_project_.workspace_settings.autosave_interval_secs =
        user_settings_.prefs().autosave_interval;

    // Save recent files
    auto& manager = project::RecentFilesManager::GetInstance();
    current_project_.workspace_settings.recent_files.clear();
    for (const auto& file : manager.GetRecentFiles()) {
      current_project_.workspace_settings.recent_files.push_back(file);
    }
  }

  return current_project_.Save();
}

absl::Status EditorManager::SaveProjectAs() {
  // Get current project name for default filename
  std::string default_name = current_project_.project_opened()
                                 ? current_project_.GetDisplayName()
                                 : "untitled_project";

  auto file_path =
      util::FileDialogWrapper::ShowSaveFileDialog(default_name, "yaze");
  if (file_path.empty()) {
    return absl::OkStatus();
  }

  // Ensure .yaze extension
  if (file_path.find(".yaze") == std::string::npos) {
    file_path += ".yaze";
  }

  // Update project filepath and save
  std::string old_filepath = current_project_.filepath;
  current_project_.filepath = file_path;

  auto save_status = current_project_.Save();
  if (save_status.ok()) {
    // Add to recent files
    auto& manager = project::RecentFilesManager::GetInstance();
    manager.AddFile(file_path);
    manager.Save();

    toast_manager_.Show(absl::StrFormat("Project saved as: %s", file_path),
                        editor::ToastType::kSuccess);
  } else {
    // Restore old filepath on failure
    current_project_.filepath = old_filepath;
    toast_manager_.Show(
        absl::StrFormat("Failed to save project: %s", save_status.message()),
        editor::ToastType::kError);
  }

  return save_status;
}

absl::Status EditorManager::ImportProject(const std::string& project_path) {
  // Delegate to ProjectManager for import logic
  RETURN_IF_ERROR(project_manager_.ImportProject(project_path));
  // Sync local project reference
  current_project_ = project_manager_.GetCurrentProject();
  return absl::OkStatus();
}

absl::Status EditorManager::RepairCurrentProject() {
  if (!current_project_.project_opened()) {
    return absl::FailedPreconditionError("No project is currently open");
  }

  RETURN_IF_ERROR(current_project_.RepairProject());
  toast_manager_.Show("Project repaired successfully",
                      editor::ToastType::kSuccess);

  return absl::OkStatus();
}

absl::Status EditorManager::SetCurrentRom(Rom* rom) {
  if (!rom) {
    return absl::InvalidArgumentError("Invalid ROM pointer");
  }

  // We need to find the session that owns this ROM.
  // This is inefficient but SetCurrentRom is rare.
  if (session_coordinator_) {
    for (size_t i = 0; i < session_coordinator_->GetTotalSessionCount(); ++i) {
      auto* session =
          static_cast<RomSession*>(session_coordinator_->GetSession(i));
      if (session && &session->rom == rom) {
        session_coordinator_->SwitchToSession(i);
        // Update test manager with current ROM for ROM-dependent tests
        test::TestManager::Get().SetCurrentRom(GetCurrentRom());
        return absl::OkStatus();
      }
    }
  }
  // If ROM wasn't found in existing sessions, treat as new session.
  // Copying an external ROM object is avoided; instead, fail.
  return absl::NotFoundError("ROM not found in existing sessions");
}

void EditorManager::CreateNewSession() {
  if (session_coordinator_) {
    session_coordinator_->CreateNewSession();
    // Toast messages are now shown by SessionCoordinator
  }
}

void EditorManager::DuplicateCurrentSession() {
  if (session_coordinator_) {
    session_coordinator_->DuplicateCurrentSession();
  }
}

void EditorManager::CloseCurrentSession() {
  if (session_coordinator_) {
    session_coordinator_->CloseCurrentSession();
  }
}

void EditorManager::RemoveSession(size_t index) {
  if (session_coordinator_) {
    session_coordinator_->RemoveSession(index);
  }
}

void EditorManager::SwitchToSession(size_t index) {
  if (session_coordinator_) {
    // Delegate to SessionCoordinator - cross-cutting concerns
    // are handled by OnSessionSwitched() observer callback
    session_coordinator_->SwitchToSession(index);
  }
}

size_t EditorManager::GetCurrentSessionIndex() const {
  return session_coordinator_ ? session_coordinator_->GetActiveSessionIndex()
                              : 0;
}

size_t EditorManager::GetActiveSessionCount() const {
  return session_coordinator_ ? session_coordinator_->GetActiveSessionCount()
                              : 0;
}

std::string EditorManager::GenerateUniqueEditorTitle(
    EditorType type, size_t session_index) const {
  const char* base_name = kEditorNames[static_cast<int>(type)];
  return session_coordinator_ ? session_coordinator_->GenerateUniqueEditorTitle(
                                    base_name, session_index)
                              : std::string(base_name);
}

// ============================================================================
// Jump-to Functionality for Cross-Editor Navigation
// ============================================================================

void EditorManager::JumpToDungeonRoom(int room_id) {
  editor_activator_.JumpToDungeonRoom(room_id);
}

void EditorManager::JumpToOverworldMap(int map_id) {
  editor_activator_.JumpToOverworldMap(map_id);
}

void EditorManager::SwitchToEditor(EditorType editor_type, bool force_visible,
                                   bool from_dialog) {
  // Special case: Agent editor requires EditorManager-specific handling
#ifdef YAZE_BUILD_AGENT_UI
  if (editor_type == EditorType::kAgent) {
    ShowAIAgent();
    return;
  }
#endif

  // Delegate all other editor switching to EditorActivator
  editor_activator_.SwitchToEditor(editor_type, force_visible, from_dialog);
}

void EditorManager::ConfigureSession(RomSession* session) {
  if (!session)
    return;
  session->editors.set_user_settings(&user_settings_);
  ConfigureEditorDependencies(&session->editors, &session->rom,
                              session->editors.session_id());
}

// SessionScope implementation
EditorManager::SessionScope::SessionScope(EditorManager* manager,
                                          size_t session_id)
    : manager_(manager),
      prev_rom_(manager->GetCurrentRom()),
      prev_editor_set_(manager->GetCurrentEditorSet()),
      prev_session_id_(manager->GetCurrentSessionId()) {
  // Set new session context
  manager_->session_coordinator_->SwitchToSession(session_id);
}

EditorManager::SessionScope::~SessionScope() {
  // Restore previous context
  manager_->session_coordinator_->SwitchToSession(prev_session_id_);
}

bool EditorManager::HasDuplicateSession(const std::string& filepath) {
  return session_coordinator_ &&
         session_coordinator_->HasDuplicateSession(filepath);
}

/**
 * @brief Injects dependencies into all editors within an EditorSet
 *
 * This function is called whenever a new session is created or a ROM is loaded
 * into an existing session. It configures the EditorDependencies struct with
 * pointers to all the managers and services that editors need, then applies
 * them to the editor set.
 *
 * @param editor_set The set of editors to configure
 * @param rom The ROM instance for this session
 * @param session_id The unique ID for this session
 *
 * Dependencies injected:
 * - rom: The ROM data for this session
 * - session_id: For creating session-aware card IDs
 * - card_registry: For registering and managing editor cards
 * - toast_manager: For showing user notifications
 * - popup_manager: For displaying modal dialogs
 * - shortcut_manager: For editor-specific shortcuts (future)
 * - shared_clipboard: For cross-editor data sharing (e.g. tile copying)
 * - user_settings: For accessing user preferences
 * - renderer: For graphics operations (dungeon/overworld editors)
 * - emulator: For accessing emulator functionality (music editor playback)
 */
void EditorManager::ShowProjectManagement() {
  if (right_panel_manager_) {
    // Update project panel context before showing
    if (project_management_panel_) {
      project_management_panel_->SetProject(&current_project_);
      project_management_panel_->SetVersionManager(version_manager_.get());
      project_management_panel_->SetRom(GetCurrentRom());
    }
    right_panel_manager_->TogglePanel(RightPanelManager::PanelType::kProject);
  }
}

void EditorManager::ShowProjectFileEditor() {
  // Load the current project file into the editor
  if (!current_project_.filepath.empty()) {
    auto status = project_file_editor_.LoadFile(current_project_.filepath);
    if (!status.ok()) {
      toast_manager_.Show(
          absl::StrFormat("Failed to load project file: %s", status.message()),
          ToastType::kError);
      return;
    }
  }
  // Set the project pointer for label import functionality
  project_file_editor_.SetProject(&current_project_);
  // Activate the editor window
  project_file_editor_.set_active(true);
}

void EditorManager::ConfigureEditorDependencies(EditorSet* editor_set, Rom* rom,
                                                size_t session_id) {
  if (!editor_set) {
    return;
  }

  EditorDependencies deps;
  deps.rom = rom;
  deps.session_id = session_id;
  deps.panel_manager = &panel_manager_;
  deps.toast_manager = &toast_manager_;
  deps.popup_manager = popup_manager_.get();
  deps.shortcut_manager = &shortcut_manager_;
  deps.shared_clipboard = &shared_clipboard_;
  deps.user_settings = &user_settings_;
  deps.project = &current_project_;
  deps.version_manager = version_manager_.get();
  deps.renderer = renderer_;
  deps.emulator = &emulator_;

  editor_set->ApplyDependencies(deps);

  // If configuring the active session, update the properties panel
  if (session_id == GetCurrentSessionId()) {
    selection_properties_panel_.SetRom(rom);
  }
}

}  // namespace yaze::editor
