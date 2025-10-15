#include "editor_manager.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#define IMGUI_DEFINE_MATH_OPERATORS

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "app/core/features.h"
#include "app/core/project.h"
#include "app/core/timing.h"
#include "app/editor/code/assembly_editor.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/graphics/graphics_editor.h"
#include "app/editor/graphics/screen_editor.h"
#include "app/editor/music/music_editor.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/palette/palette_editor.h"
#include "app/editor/sprite/sprite_editor.h"
#include "app/editor/system/editor_card_registry.h"
#include "app/editor/system/editor_registry.h"
#include "app/editor/system/menu_orchestrator.h"
#include "app/editor/system/popup_manager.h"
#include "app/editor/system/shortcut_configurator.h"
#include "app/editor/ui/editor_selection_dialog.h"
#include "app/editor/ui/ui_coordinator.h"
#include "app/emu/emulator.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/core/background_renderer.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/theme_manager.h"
#include "app/rom.h"
#include "app/test/test_manager.h"
#include "imgui/imgui.h"
#include "util/file_util.h"
#include "util/log.h"
#include "zelda3/screen/dungeon_map.h"

#ifdef YAZE_ENABLE_TESTING
#include "app/test/e2e_test_suite.h"
#include "app/test/integrated_test_suite.h"
#include "app/test/rom_dependent_test_suite.h"
#include "app/test/zscustomoverworld_test_suite.h"
#endif
#ifdef YAZE_ENABLE_GTEST
#include "app/test/unit_test_suite.h"
#endif

#include "app/editor/editor.h"
#include "app/editor/system/settings_editor.h"
#include "app/editor/system/toast_manager.h"
#include "app/gfx/debug/performance/performance_dashboard.h"

#ifdef YAZE_WITH_GRPC
#include "app/core/service/screenshot_utils.h"
#include "app/editor/agent/agent_chat_widget.h"
#include "app/test/z3ed_test_suite.h"
#include "cli/service/agent/agent_control_server.h"
#include "cli/service/agent/conversational_agent_service.h"
#include "cli/service/ai/gemini_ai_service.h"
#include "app/editor/agent/automation_bridge.h"
#endif

#include "imgui/misc/cpp/imgui_stdlib.h"
#include "util/macro.h"
#include "yaze_config.h"

namespace yaze {
namespace editor {

using util::FileDialogWrapper;

namespace {

std::string GetEditorName(EditorType type) {
  return kEditorNames[static_cast<int>(type)];
}

}  // namespace

// Static registry of editors that use the card-based layout system
// These editors register their cards with EditorCardManager and manage their own windows
// They do NOT need the traditional ImGui::Begin/End wrapper - they create cards internally
bool EditorManager::IsCardBasedEditor(EditorType type) {
  return EditorRegistry::IsCardBasedEditor(type);
}

void EditorManager::HideCurrentEditorCards() {
  if (!current_editor_) {
    return;
  }

  // Using EditorCardRegistry directly
  std::string category =
  editor_registry_.GetEditorCategory(current_editor_->type());
  card_registry_.HideAllCardsInCategory(category);
}

void EditorManager::ShowHexEditor() {
  // Using EditorCardRegistry directly
  card_registry_.ShowCard("memory.hex_editor");
}

#ifdef YAZE_WITH_GRPC
void EditorManager::ShowAIAgent() {
  agent_editor_.set_active(true);
}

void EditorManager::ShowChatHistory() {
  agent_chat_history_popup_.Toggle();
}
#endif

EditorManager::EditorManager() 
    : blank_editor_set_(nullptr, &user_settings_),
      project_manager_(&toast_manager_),
      rom_file_manager_(&toast_manager_) {
  std::stringstream ss;
  ss << YAZE_VERSION_MAJOR << "." << YAZE_VERSION_MINOR << "."
     << YAZE_VERSION_PATCH;
  ss >> version_;
  
  // ============================================================================
  // DELEGATION INFRASTRUCTURE INITIALIZATION
  // ============================================================================
  // EditorManager delegates responsibilities to specialized components:
  // - SessionCoordinator: Multi-session UI and lifecycle management
  // - MenuOrchestrator: Menu building and action routing
  // - UICoordinator: UI drawing and state management
  // - RomFileManager: ROM file I/O operations
  // - ProjectManager: Project file operations
  // - EditorCardRegistry: Card-based editor UI management
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
  // 1. PopupManager - MUST be first, MenuOrchestrator/UICoordinator take ref to it
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
      static_cast<void*>(&sessions_), &card_registry_, &toast_manager_);
  
  // STEP 3: Initialize MenuOrchestrator (depends on popup_manager_, session_coordinator_)
  menu_orchestrator_ = std::make_unique<MenuOrchestrator>(
      this, menu_builder_, rom_file_manager_, project_manager_,
      editor_registry_, *session_coordinator_, toast_manager_, *popup_manager_);
  
  // STEP 4: Initialize UICoordinator (depends on popup_manager_, session_coordinator_, card_registry_)
  ui_coordinator_ = std::make_unique<UICoordinator>(
      this, rom_file_manager_, project_manager_, editor_registry_, card_registry_,
      *session_coordinator_, window_delegate_, toast_manager_, *popup_manager_,
      shortcut_manager_);
  
  // STEP 5: ShortcutConfigurator created later in Initialize() method
  // It depends on all above coordinators being available
}

EditorManager::~EditorManager() = default;

void EditorManager::InitializeTestSuites() {
  auto& test_manager = test::TestManager::Get();

#ifdef YAZE_ENABLE_TESTING
  // Register comprehensive test suites
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

constexpr const char* kOverworldEditorName = ICON_MD_LAYERS " Overworld Editor";
constexpr const char* kGraphicsEditorName = ICON_MD_PHOTO " Graphics Editor";
constexpr const char* kPaletteEditorName = ICON_MD_PALETTE " Palette Editor";
constexpr const char* kScreenEditorName = ICON_MD_SCREENSHOT " Screen Editor";
constexpr const char* kSpriteEditorName = ICON_MD_SMART_TOY " Sprite Editor";
constexpr const char* kMessageEditorName = ICON_MD_MESSAGE " Message Editor";
constexpr const char* kSettingsEditorName = ICON_MD_SETTINGS " Settings Editor";
constexpr const char* kAssemblyEditorName = ICON_MD_CODE " Assembly Editor";
constexpr const char* kDungeonEditorName = ICON_MD_CASTLE " Dungeon Editor";
constexpr const char* kMusicEditorName = ICON_MD_MUSIC_NOTE " Music Editor";

void EditorManager::Initialize(gfx::IRenderer* renderer,
                               const std::string& filename) {
  renderer_ = renderer;

  // Inject card_registry into emulator and workspace_manager
  emulator_.set_card_registry(&card_registry_);
  workspace_manager_.set_card_registry(&card_registry_);

  // Point to a blank editor set when no ROM is loaded
  current_editor_set_ = &blank_editor_set_;

  if (!filename.empty()) {
    PRINT_IF_ERROR(OpenRomOrProject(filename));
  }

  // Note: PopupManager is now initialized in constructor before MenuOrchestrator
  // This ensures all menu callbacks can safely call popup_manager_.Show()

  // Register emulator cards early (emulator Initialize might not be called)
  // Using EditorCardRegistry directly
  card_registry_.RegisterCard({.card_id = "emulator.cpu_debugger",
                             .display_name = "CPU Debugger",
                             .icon = ICON_MD_BUG_REPORT,
                             .category = "Emulator",
                             .priority = 10});
  card_registry_.RegisterCard({.card_id = "emulator.ppu_viewer",
                             .display_name = "PPU Viewer",
                             .icon = ICON_MD_VIDEOGAME_ASSET,
                             .category = "Emulator",
                             .priority = 20});
  card_registry_.RegisterCard({.card_id = "emulator.memory_viewer",
                             .display_name = "Memory Viewer",
                             .icon = ICON_MD_MEMORY,
                             .category = "Emulator",
                             .priority = 30});
  card_registry_.RegisterCard({.card_id = "emulator.breakpoints",
                             .display_name = "Breakpoints",
                             .icon = ICON_MD_STOP,
                             .category = "Emulator",
                             .priority = 40});
  card_registry_.RegisterCard({.card_id = "emulator.performance",
                             .display_name = "Performance",
                             .icon = ICON_MD_SPEED,
                             .category = "Emulator",
                             .priority = 50});
  card_registry_.RegisterCard({.card_id = "emulator.ai_agent",
                             .display_name = "AI Agent",
                             .icon = ICON_MD_SMART_TOY,
                             .category = "Emulator",
                             .priority = 60});
  card_registry_.RegisterCard({.card_id = "emulator.save_states",
                             .display_name = "Save States",
                             .icon = ICON_MD_SAVE,
                             .category = "Emulator",
                             .priority = 70});
  card_registry_.RegisterCard({.card_id = "emulator.keyboard_config",
                             .display_name = "Keyboard Config",
                             .icon = ICON_MD_KEYBOARD,
                             .category = "Emulator",
                             .priority = 80});
  card_registry_.RegisterCard({.card_id = "emulator.apu_debugger",
                             .display_name = "APU Debugger",
                             .icon = ICON_MD_AUDIOTRACK,
                             .category = "Emulator",
                             .priority = 90});
  card_registry_.RegisterCard({.card_id = "emulator.audio_mixer",
                             .display_name = "Audio Mixer",
                             .icon = ICON_MD_AUDIO_FILE,
                             .category = "Emulator",
                             .priority = 100});

  // Show CPU debugger and PPU viewer by default for emulator
  card_registry_.ShowCard("emulator.cpu_debugger");
  card_registry_.ShowCard("emulator.ppu_viewer");

  // Register memory/hex editor card
  card_registry_.RegisterCard({.card_id = "memory.hex_editor",
                             .display_name = "Hex Editor",
                             .icon = ICON_MD_MEMORY,
                             .category = "Memory",
                             .priority = 10});

  // Initialize project file editor
  project_file_editor_.SetToastManager(&toast_manager_);

#ifdef YAZE_WITH_GRPC
  // Initialize the agent editor as a proper Editor (configuration dashboard)
  // TODO: pass agent editor dependencies once agent editor is modernized
  agent_editor_.Initialize();
  agent_editor_.InitializeWithDependencies(&toast_manager_, &proposal_drawer_,
                                           nullptr);

  // Initialize and connect the chat history popup
  agent_chat_history_popup_.SetToastManager(&toast_manager_);
  if (agent_editor_.GetChatWidget()) {
    agent_editor_.GetChatWidget()->SetChatHistoryPopup(
        &agent_chat_history_popup_);
  }

  // Set up multimodal (vision) callbacks for Gemini
  AgentChatWidget::MultimodalCallbacks multimodal_callbacks;
  multimodal_callbacks.capture_snapshot =
      [this](std::filesystem::path* output_path) -> absl::Status {
    using CaptureMode = AgentChatWidget::CaptureMode;

    absl::StatusOr<yaze::test::ScreenshotArtifact> result;

    // Capture based on selected mode
    switch (agent_editor_.GetChatWidget()->capture_mode()) {
      case CaptureMode::kFullWindow:
        result = yaze::test::CaptureHarnessScreenshot("");
        break;

      case CaptureMode::kActiveEditor:
        result = yaze::test::CaptureActiveWindow("");
        if (!result.ok()) {
          // Fallback to full window if no active window
          result = yaze::test::CaptureHarnessScreenshot("");
        }
        break;

      case CaptureMode::kSpecificWindow: {
        const char* window_name =
            agent_editor_.GetChatWidget()->specific_window_name();
        if (window_name && std::strlen(window_name) > 0) {
          result = yaze::test::CaptureWindowByName(window_name, "");
          if (!result.ok()) {
            // Fallback to active window if specific window not found
            result = yaze::test::CaptureActiveWindow("");
          }
        } else {
          result = yaze::test::CaptureActiveWindow("");
        }
        if (!result.ok()) {
          result = yaze::test::CaptureHarnessScreenshot("");
        }
        break;
      }
    }

    if (!result.ok()) {
      return result.status();
    }
    *output_path = result->file_path;
    return absl::OkStatus();
  };
  multimodal_callbacks.send_to_gemini =
      [this](const std::filesystem::path& image_path,
             const std::string& prompt) -> absl::Status {
    // Get Gemini API key from environment
    const char* api_key = std::getenv("GEMINI_API_KEY");
    if (!api_key || std::strlen(api_key) == 0) {
      return absl::FailedPreconditionError(
          "GEMINI_API_KEY environment variable not set");
    }

    // Create Gemini service
    cli::GeminiConfig config;
    config.api_key = api_key;
    config.model = "gemini-2.5-flash";  // Use vision-capable model
    config.verbose = false;

    cli::GeminiAIService gemini_service(config);

    // Generate multimodal response
    auto response =
        gemini_service.GenerateMultimodalResponse(image_path.string(), prompt);
    if (!response.ok()) {
      return response.status();
    }

    // Add the response to chat history
    cli::agent::ChatMessage agent_msg;
    agent_msg.sender = cli::agent::ChatMessage::Sender::kAgent;
    agent_msg.message = response->text_response;
    agent_msg.timestamp = absl::Now();
    agent_editor_.GetChatWidget()->SetRomContext(current_rom_);

    return absl::OkStatus();
  };
  agent_editor_.GetChatWidget()->SetMultimodalCallbacks(multimodal_callbacks);

  // Set up Z3ED command callbacks for proposal management
  AgentChatWidget::Z3EDCommandCallbacks z3ed_callbacks;

  z3ed_callbacks.accept_proposal =
      [this](const std::string& proposal_id) -> absl::Status {
    // Use ProposalDrawer's existing logic
    proposal_drawer_.Show();
    proposal_drawer_.FocusProposal(proposal_id);

    toast_manager_.Show(
        absl::StrFormat("%s View proposal %s in drawer to accept",
                        ICON_MD_PREVIEW, proposal_id),
        ToastType::kInfo, 3.5f);

    return absl::OkStatus();
  };

  z3ed_callbacks.reject_proposal =
      [this](const std::string& proposal_id) -> absl::Status {
    // Use ProposalDrawer's existing logic
    proposal_drawer_.Show();
    proposal_drawer_.FocusProposal(proposal_id);

    toast_manager_.Show(
        absl::StrFormat("%s View proposal %s in drawer to reject",
                        ICON_MD_PREVIEW, proposal_id),
        ToastType::kInfo, 3.0f);

    return absl::OkStatus();
  };

  z3ed_callbacks.list_proposals =
      []() -> absl::StatusOr<std::vector<std::string>> {
    // Return empty for now - ProposalDrawer handles the real list
    return std::vector<std::string>{};
  };

  z3ed_callbacks.diff_proposal =
      [this](const std::string& proposal_id) -> absl::StatusOr<std::string> {
    // Open drawer to show diff
    proposal_drawer_.Show();
    proposal_drawer_.FocusProposal(proposal_id);
    return "See diff in proposal drawer";
  };

  agent_editor_.GetChatWidget()->SetZ3EDCommandCallbacks(z3ed_callbacks);

  AgentChatWidget::AutomationCallbacks automation_callbacks;
  automation_callbacks.open_harness_dashboard = [this]() {
    test::TestManager::Get().ShowHarnessDashboard();
  };
  automation_callbacks.show_active_tests = [this]() {
    test::TestManager::Get().ShowHarnessActiveTests();
  };
  automation_callbacks.replay_last_plan = [this]() {
    test::TestManager::Get().ReplayLastPlan();
  };
  automation_callbacks.focus_proposal = [this](const std::string& proposal_id) {
    proposal_drawer_.Show();
    proposal_drawer_.FocusProposal(proposal_id);
  };
  agent_editor_.GetChatWidget()->SetAutomationCallbacks(automation_callbacks);

  harness_telemetry_bridge_.SetChatWidget(agent_editor_.GetChatWidget());
  test::TestManager::Get().SetHarnessListener(&harness_telemetry_bridge_);
#endif

  // Load critical user settings first
  status_ = user_settings_.Load();
  if (!status_.ok()) {
    LOG_WARN("EditorManager", "Failed to load user settings: %s",
             status_.ToString().c_str());
  }

  // Initialize welcome screen callbacks
  welcome_screen_.SetOpenRomCallback([this]() {
    status_ = LoadRom();
    // LoadRom() already handles closing welcome screen and showing editor selection
  });

  welcome_screen_.SetNewProjectCallback([this]() {
    status_ = CreateNewProject();
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

  // Initialize editor selection dialog callback
  editor_selection_dialog_.SetSelectionCallback([this](EditorType type) {
    editor_selection_dialog_.MarkRecentlyUsed(type);
    SwitchToEditor(type);  // Use centralized method
  });

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

  ConfigureEditorShortcuts(shortcut_deps, &shortcut_manager_);
  ConfigureMenuShortcuts(shortcut_deps, &shortcut_manager_);
}

void EditorManager::OpenEditorAndCardsFromFlags(const std::string& editor_name,
                                                const std::string& cards_str) {
  if (editor_name.empty()) {
    return;
  }

  LOG_INFO("EditorManager", "Processing startup flags: editor='%s', cards='%s'",
           editor_name.c_str(), cards_str.c_str());

  EditorType editor_type_to_open = EditorType::kUnknown;
  for (int i = 0; i < static_cast<int>(EditorType::kSettings); ++i) {
    if (GetEditorName(static_cast<EditorType>(i)) == editor_name) {
      editor_type_to_open = static_cast<EditorType>(i);
      break;
    }
  }

  if (editor_type_to_open == EditorType::kUnknown) {
    LOG_WARN("EditorManager", "Unknown editor specified via flag: %s",
             editor_name.c_str());
    return;
  }

  // Activate the main editor window
  if (current_editor_set_) {
    auto* editor = current_editor_set_
                       ->active_editors_[static_cast<int>(editor_type_to_open)];
    if (editor) {
      editor->set_active(true);
    }
  }

  // Handle specific cards for the Dungeon Editor
  if (editor_type_to_open == EditorType::kDungeon && !cards_str.empty()) {
    std::stringstream ss(cards_str);
    std::string card_name;
    while (std::getline(ss, card_name, ',')) {
      // Trim whitespace
      card_name.erase(0, card_name.find_first_not_of(" \t"));
      card_name.erase(card_name.find_last_not_of(" \t") + 1);

      LOG_DEBUG("EditorManager", "Attempting to open card: '%s'",
                card_name.c_str());

      if (card_name == "Rooms List") {
        current_editor_set_->dungeon_editor_.show_room_selector_ = true;
      } else if (card_name == "Room Matrix") {
        current_editor_set_->dungeon_editor_.show_room_matrix_ = true;
      } else if (card_name == "Entrances List") {
        current_editor_set_->dungeon_editor_.show_entrances_list_ = true;
      } else if (card_name == "Room Graphics") {
        current_editor_set_->dungeon_editor_.show_room_graphics_ = true;
      } else if (card_name == "Object Editor") {
        current_editor_set_->dungeon_editor_.show_object_editor_ = true;
      } else if (card_name == "Palette Editor") {
        current_editor_set_->dungeon_editor_.show_palette_editor_ = true;
      } else if (absl::StartsWith(card_name, "Room ")) {
        try {
          int room_id = std::stoi(card_name.substr(5));
          current_editor_set_->dungeon_editor_.add_room(room_id);
        } catch (const std::exception& e) {
          LOG_WARN("EditorManager", "Invalid room ID format: %s",
                   card_name.c_str());
        }
      } else {
        LOG_WARN("EditorManager", "Unknown card name for Dungeon Editor: %s",
                 card_name.c_str());
      }
    }
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
 * 7. Draw sidebar (EditorCardRegistry) - card-based editor UI
 * 
 * Note: EditorManager retains the main loop to coordinate multi-session updates,
 * but delegates specific drawing/state operations to specialized components.
 */
absl::Status EditorManager::Update() {

  // Update timing manager for accurate delta time across the application
  // This fixes animation timing issues that occur when mouse isn't moving
  core::TimingManager::Get().Update();

  // Delegate to PopupManager for modal dialog rendering
  popup_manager_->DrawPopups();
  
  // Execute keyboard shortcuts (registered via ShortcutConfigurator)
  ExecuteShortcuts(shortcut_manager_);
  
  // Delegate to ToastManager for notification rendering
  toast_manager_.Draw();

  // Draw editor selection dialog (managed by UICoordinator)
  if (ui_coordinator_ && ui_coordinator_->IsEditorSelectionVisible()) {
    bool show = true;
    editor_selection_dialog_.Show(&show);
    if (!show) {
      ui_coordinator_->SetEditorSelectionVisible(false);
    }
  }

  // Draw card browser (managed by UICoordinator)
  if (ui_coordinator_ && ui_coordinator_->IsCardBrowserVisible()) {
    bool show = true;
    card_registry_.DrawCardBrowser(&show);
    if (!show) {
      ui_coordinator_->SetCardBrowserVisible(false);
    }
  }

#ifdef YAZE_WITH_GRPC
  // Update agent editor dashboard
  status_ = agent_editor_.Update();

  // Draw chat widget separately (always visible when active)
  if (agent_editor_.GetChatWidget()) {
    agent_editor_.GetChatWidget()->Draw();
  }
#endif

  // Draw background grid effects for the entire viewport
  if (ImGui::GetCurrentContext()) {
    ImDrawList* bg_draw_list = ImGui::GetBackgroundDrawList();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    auto& theme_manager = gui::ThemeManager::Get();
    auto current_theme = theme_manager.GetCurrentTheme();
    auto& bg_renderer = gui::BackgroundRenderer::Get();

    // Draw grid covering the entire main viewport
    ImVec2 grid_pos = viewport->WorkPos;
    ImVec2 grid_size = viewport->WorkSize;
    bg_renderer.RenderDockingBackground(bg_draw_list, grid_pos, grid_size,
                                        current_theme.primary);
  }

  // Ensure TestManager always has the current ROM
  static Rom* last_test_rom = nullptr;
  if (last_test_rom != current_rom_) {
    LOG_DEBUG(
        "EditorManager",
        "EditorManager::Update - ROM changed, updating TestManager: %p -> "
        "%p",
        (void*)last_test_rom, (void*)current_rom_);
    test::TestManager::Get().SetCurrentRom(current_rom_);
    last_test_rom = current_rom_;
  }

  // Autosave timer
  if (user_settings_.prefs().autosave_enabled && current_rom_ &&
      current_rom_->dirty()) {
    autosave_timer_ += ImGui::GetIO().DeltaTime;
    if (autosave_timer_ >= user_settings_.prefs().autosave_interval) {
      autosave_timer_ = 0.0f;
      Rom::SaveSettings s;
      s.backup = true;
      s.save_new = false;
      auto st = current_rom_->SaveToFile(s);
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
  if (!current_editor_set_) {
    // Note: Welcome screen auto-show is now handled by UICoordinator
    return absl::OkStatus();
  }

  // Check if current ROM is valid
  if (!current_rom_) {
    // Note: Welcome screen auto-show is now handled by UICoordinator
    return absl::OkStatus();
  }

  // ROM is loaded and valid - don't auto-show welcome screen
  // Welcome screen should only be shown manually at this point

  // Iterate through ALL sessions to support multi-session docking
  for (size_t session_idx = 0; session_idx < sessions_.size(); ++session_idx) {
    auto& session = sessions_[session_idx];
    if (!session.rom.is_loaded())
      continue;  // Skip sessions with invalid ROMs

    // Use RAII SessionScope for clean context switching
    SessionScope scope(this, session_idx);

    for (auto editor : session.editors.active_editors_) {
      if (*editor->active()) {
        if (editor->type() == EditorType::kOverworld) {
          auto& overworld_editor = static_cast<OverworldEditor&>(*editor);
          if (overworld_editor.jump_to_tab() != -1) {
            session.editors.dungeon_editor_.set_active(true);
            // Set the dungeon editor to the jump to tab
            session.editors.dungeon_editor_.add_room(
                overworld_editor.jump_to_tab());
            overworld_editor.jump_to_tab_ = -1;
          }
        }

        // CARD-BASED EDITORS: Don't wrap in Begin/End, they manage own windows
        bool is_card_based_editor = IsCardBasedEditor(editor->type());

        if (is_card_based_editor) {
          // Card-based editors create their own top-level windows
          // No parent wrapper needed - this allows independent docking
          current_editor_ = editor;

          status_ = editor->Update();

          // Route editor errors to toast manager
          if (!status_.ok()) {
            std::string editor_name = GetEditorName(editor->type());
            toast_manager_.Show(
                absl::StrFormat("%s Error: %s", editor_name, status_.message()),
                editor::ToastType::kError, 8.0f);
          }

        } else {
          // TRADITIONAL EDITORS: Wrap in Begin/End
          std::string window_title =
              GenerateUniqueEditorTitle(editor->type(), session_idx);

          // Set window to maximize on first open
          ImGui::SetNextWindowSize(ImGui::GetMainViewport()->WorkSize,
                                   ImGuiCond_FirstUseEver);
          ImGui::SetNextWindowPos(ImGui::GetMainViewport()->WorkPos,
                                  ImGuiCond_FirstUseEver);

          if (ImGui::Begin(window_title.c_str(), editor->active(),
                           ImGuiWindowFlags_None)) {  // Allow full docking
            // Temporarily switch context for this editor's update
            Rom* prev_rom = current_rom_;
            EditorSet* prev_editor_set = current_editor_set_;
            size_t prev_session_id = current_session_id_;

            current_rom_ = &session.rom;
            current_editor_set_ = &session.editors;
            current_editor_ = editor;
            current_session_id_ = session_idx;

            status_ = editor->Update();

            // Route editor errors to toast manager
            if (!status_.ok()) {
              std::string editor_name = GetEditorName(editor->type());
              toast_manager_.Show(absl::StrFormat("%s Error: %s", editor_name,
                                                  status_.message()),
                                  editor::ToastType::kError, 8.0f);
            }

            // Restore context
            current_rom_ = prev_rom;
            current_editor_set_ = prev_editor_set;
            current_session_id_ = prev_session_id;
          }
          ImGui::End();
        }
      }
    }
  }

  if (ui_coordinator_ && ui_coordinator_->IsPerformanceDashboardVisible()) {
    gfx::PerformanceDashboard::Get().Render();
  }

  // Always draw proposal drawer (it manages its own visibility)
  proposal_drawer_.Draw();

#ifdef YAZE_WITH_GRPC
  // Update ROM context for agent editor
  if (current_rom_ && current_rom_->is_loaded()) {
    agent_editor_.SetRomContext(current_rom_);
  }
#endif

  // Draw unified sidebar LAST so it appears on top of all other windows
  if (ui_coordinator_ && ui_coordinator_->IsCardSidebarVisible() &&
      current_editor_set_) {
    // Using EditorCardRegistry directly

    // Collect all active card-based editors
    std::vector<std::string> active_categories;
    for (size_t session_idx = 0; session_idx < sessions_.size();
         ++session_idx) {
      auto& session = sessions_[session_idx];
      if (!session.rom.is_loaded())
        continue;

      for (auto editor : session.editors.active_editors_) {
        if (*editor->active() && IsCardBasedEditor(editor->type())) {
          std::string category = EditorRegistry::GetEditorCategory(editor->type());
          if (std::find(active_categories.begin(), active_categories.end(),
                        category) == active_categories.end()) {
            active_categories.push_back(category);
          }
        }
      }
    }

    // Determine which category to show in sidebar
    std::string sidebar_category;

    // Priority 1: Use active_category from card manager (user's last interaction)
    if (!card_registry_.GetActiveCategory().empty() &&
        std::find(active_categories.begin(), active_categories.end(),
                  card_registry_.GetActiveCategory()) !=
            active_categories.end()) {
      sidebar_category = card_registry_.GetActiveCategory();
    }
    // Priority 2: Use first active category
    else if (!active_categories.empty()) {
      sidebar_category = active_categories[0];
      card_registry_.SetActiveCategory(sidebar_category);
    }

    // Draw sidebar if we have a category
    if (!sidebar_category.empty()) {
      // Callback to switch editors when category button is clicked
      auto category_switch_callback = [this](const std::string& new_category) {
        EditorType editor_type = EditorRegistry::GetEditorTypeFromCategory(new_category);
        if (editor_type != EditorType::kUnknown) {
          SwitchToEditor(editor_type);
        }
      };

      auto collapse_callback = [this]() {
        if (ui_coordinator_) {
          ui_coordinator_->SetCardSidebarVisible(false);
        }
      };

      card_registry_.DrawSidebar(sidebar_category, active_categories,
                               category_switch_callback, collapse_callback);
    }
  }

  // Draw SessionCoordinator UI components
  if (session_coordinator_) {
    session_coordinator_->DrawSessionSwitcher();
    session_coordinator_->DrawSessionManager();
    // TODO: Decide which is actually used.
    // if (ui_coordinator_) {
    //   ui_coordinator_->DrawSessionManager();
    // }
    session_coordinator_->DrawSessionRenameDialog();
  }

  // Draw UICoordinator UI components (Command Palette, Global Search, etc.)
  // CRITICAL: This must be called for Command Palette and other UI windows to appear
  if (ui_coordinator_) {
    ui_coordinator_->DrawAllUI();
  }

  return absl::OkStatus();
}

absl::Status EditorManager::DrawRomSelector() {
  ImGui::SameLine((ImGui::GetWindowWidth() / 2) - 100);
  if (current_rom_ && current_rom_->is_loaded()) {
    ImGui::SetNextItemWidth(ImGui::GetWindowWidth() / 6);
    if (ImGui::BeginCombo("##ROMSelector", current_rom_->short_name().c_str())) {
      int idx = 0;
      for (auto it = sessions_.begin(); it != sessions_.end(); ++it, ++idx) {
        Rom* rom = &it->rom;
        ImGui::PushID(idx);
        bool selected = (rom == current_rom_);
        if (ImGui::Selectable(rom->short_name().c_str(), selected)) {
          RETURN_IF_ERROR(SetCurrentRom(rom));
        }
        ImGui::PopID();
      }
      ImGui::EndCombo();
    }
    // Inline status next to ROM selector
    ImGui::SameLine();
    ImGui::Text("Size: %.1f MB", current_rom_->size() / 1048576.0f);

    // Context-sensitive card control (right after ROM info)
    ImGui::SameLine();
    DrawContextSensitiveCardControl();
  } else {
    ImGui::Text("No ROM loaded");
  }
  return absl::OkStatus();
}

void EditorManager::DrawContextSensitiveCardControl() {
  // Delegate to UICoordinator for clean separation of concerns
  if (ui_coordinator_) {
    ui_coordinator_->DrawContextSensitiveCardControl();
  }
}

/**
 * @brief Draw the main menu bar
 * 
 * DELEGATION:
 * - Menu items: MenuOrchestrator::BuildMainMenu()
 * - ROM selector: EditorManager::DrawRomSelector() (inline, needs current_rom_ access)
 * - Menu bar extras: UICoordinator::DrawMenuBarExtras() (session indicator, version)
 * 
 * Note: ROM selector stays in EditorManager because it needs direct access to sessions_
 * and current_rom_ for the combo box. Could be extracted to SessionCoordinator in future.
 */
void EditorManager::DrawMenuBar() {
  static bool show_display_settings = false;

  if (ImGui::BeginMenuBar()) {
    // Delegate menu building to MenuOrchestrator
    if (menu_orchestrator_) {
      menu_orchestrator_->BuildMainMenu();
    }

    // Inline ROM selector (requires direct session access)
    status_ = DrawRomSelector();

    // Delegate menu bar extras to UICoordinator (session indicator, version display)
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

  // Using EditorCardRegistry directly
  if (current_editor_set_) {
    // Pass the actual visibility flag pointer so the X button works
    bool* hex_visibility =
        card_registry_.GetVisibilityFlag("memory.hex_editor");
    if (hex_visibility && *hex_visibility) {
      current_editor_set_->memory_editor_.Update(*hex_visibility);
    }

    bool* assembly_visibility =
        card_registry_.GetVisibilityFlag("assembly.editor");
    if (assembly_visibility && *assembly_visibility) {
      current_editor_set_->assembly_editor_.Update(
          *card_registry_.GetVisibilityFlag("assembly.editor"));
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

  // Agent proposal drawer (right side)
  proposal_drawer_.SetRom(current_rom_);
  proposal_drawer_.Draw();

  // Agent chat history popup (left side)
  agent_chat_history_popup_.Draw();

  // Welcome screen is now drawn by UICoordinator::DrawAllUI()
  // Removed duplicate call to avoid showing welcome screen twice

  // TODO: Fix emulator not appearing
  if (show_emulator_) {
    emulator_.Run(current_rom_);
  }

  // Enhanced Global Search UI (managed by UICoordinator)
  // TODO: Move this to UI
  if (ui_coordinator_ && ui_coordinator_->IsGlobalSearchVisible()) {
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

    bool show_search = true;
    if (ImGui::Begin(
            absl::StrFormat("%s Global Search", ICON_MD_MANAGE_SEARCH).c_str(),
            &show_search, ImGuiWindowFlags_NoCollapse)) {

      // Enhanced search input with focus management
      static char query[256] = {};
      ImGui::SetNextItemWidth(-100);
      if (ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere();
      }

      bool input_changed = ImGui::InputTextWithHint(
          "##global_query",
          absl::StrFormat("%s Search everything...", ICON_MD_SEARCH).c_str(),
          query, IM_ARRAYSIZE(query));

      ImGui::SameLine();
      if (ImGui::Button(absl::StrFormat("%s Clear", ICON_MD_CLEAR).c_str())) {
        query[0] = '\0';
        input_changed = true;
      }

      ImGui::Separator();

      // Tabbed search results for better organization
      if (ImGui::BeginTabBar("SearchResultTabs")) {

        // Recent Files Tab
        if (ImGui::BeginTabItem(
                absl::StrFormat("%s Recent Files", ICON_MD_HISTORY).c_str())) {
          auto& manager = core::RecentFilesManager::GetInstance();
          auto recent_files = manager.GetRecentFiles();

          if (ImGui::BeginTable("RecentFilesTable", 3,
                                ImGuiTableFlags_ScrollY |
                                    ImGuiTableFlags_RowBg |
                                    ImGuiTableFlags_SizingStretchProp)) {

            ImGui::TableSetupColumn("File", ImGuiTableColumnFlags_WidthStretch,
                                    0.6f);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed,
                                    80.0f);
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed,
                                    100.0f);
            ImGui::TableHeadersRow();

            for (const auto& file : recent_files) {
              if (query[0] != '\0' && file.find(query) == std::string::npos)
                continue;

              ImGui::TableNextRow();
              ImGui::TableNextColumn();
              ImGui::Text("%s", util::GetFileName(file).c_str());

              ImGui::TableNextColumn();
              std::string ext = util::GetFileExtension(file);
              if (ext == "sfc" || ext == "smc") {
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%s ROM",
                                   ICON_MD_VIDEOGAME_ASSET);
              } else if (ext == "yaze") {
                ImGui::TextColored(ImVec4(0.2f, 0.6f, 0.8f, 1.0f), "%s Project",
                                   ICON_MD_FOLDER);
              } else {
                ImGui::Text("%s File", ICON_MD_DESCRIPTION);
              }

              ImGui::TableNextColumn();
              ImGui::PushID(file.c_str());
              if (ImGui::Button("Open")) {
                status_ = OpenRomOrProject(file);
                if (ui_coordinator_) {
                  ui_coordinator_->SetGlobalSearchVisible(false);
                }
              }
              ImGui::PopID();
            }

            ImGui::EndTable();
          }
          ImGui::EndTabItem();
        }

        // Labels Tab (only if ROM is loaded)
        if (current_rom_ && current_rom_->resource_label()) {
          if (ImGui::BeginTabItem(
                  absl::StrFormat("%s Labels", ICON_MD_LABEL).c_str())) {
            auto& labels = current_rom_->resource_label()->labels_;

            if (ImGui::BeginTable("LabelsTable", 3,
                                  ImGuiTableFlags_ScrollY |
                                      ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_SizingStretchProp)) {

              ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed,
                                      100.0f);
              ImGui::TableSetupColumn("Label",
                                      ImGuiTableColumnFlags_WidthStretch, 0.4f);
              ImGui::TableSetupColumn("Value",
                                      ImGuiTableColumnFlags_WidthStretch, 0.6f);
              ImGui::TableHeadersRow();

              for (const auto& type_pair : labels) {
                for (const auto& kv : type_pair.second) {
                  if (query[0] != '\0' &&
                      kv.first.find(query) == std::string::npos &&
                      kv.second.find(query) == std::string::npos)
                    continue;

                  ImGui::TableNextRow();
                  ImGui::TableNextColumn();
                  ImGui::Text("%s", type_pair.first.c_str());

                  ImGui::TableNextColumn();
                  if (ImGui::Selectable(kv.first.c_str(), false,
                                 ImGuiSelectableFlags_SpanAllColumns)) {
                    // Future: navigate to related editor/location
                  }

                  ImGui::TableNextColumn();
                  ImGui::TextDisabled("%s", kv.second.c_str());
                }
              }

              ImGui::EndTable();
            }
            ImGui::EndTabItem();
          }
        }

        // Sessions Tab
        if (GetActiveSessionCount() > 1) {
          if (ImGui::BeginTabItem(
                  absl::StrFormat("%s Sessions", ICON_MD_TAB).c_str())) {
            ImGui::Text("Search and switch between active sessions:");

            for (size_t i = 0; i < sessions_.size(); ++i) {
              auto& session = sessions_[i];
              if (session.custom_name == "[CLOSED SESSION]")
                continue;

              std::string session_info = session.GetDisplayName();
              if (query[0] != '\0' &&
                  session_info.find(query) == std::string::npos)
                continue;

              bool is_current = (&session.rom == current_rom_);
              if (is_current) {
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
              }

              if (ImGui::Selectable(absl::StrFormat("%s %s %s", ICON_MD_TAB,
                                             session_info.c_str(),
                                             is_current ? "(Current)" : "")
                                 .c_str())) {
                if (!is_current) {
                  SwitchToSession(i);
                  if (ui_coordinator_) {
                    ui_coordinator_->SetGlobalSearchVisible(false);
                  }
                }
              }

              if (is_current) {
                ImGui::PopStyleColor();
              }
            }
            ImGui::EndTabItem();
          }
        }

        ImGui::EndTabBar();
      }

      // Status bar
      ImGui::Separator();
      ImGui::Text("%s Global search across all YAZE data", ICON_MD_INFO);
    }
    ImGui::End();
    
    // Update visibility state
    if (!show_search && ui_coordinator_) {
      ui_coordinator_->SetGlobalSearchVisible(false);
    }
  }

  if (show_palette_editor_ && current_editor_set_) {
    ImGui::Begin("Palette Editor", &show_palette_editor_);
    status_ = current_editor_set_->palette_editor_.Update();

    // Route palette editor errors to toast manager
    if (!status_.ok()) {
      toast_manager_.Show(
          absl::StrFormat("Palette Editor Error: %s", status_.message()),
          editor::ToastType::kError, 8.0f);
    }

    ImGui::End();
  }

  if (show_resource_label_manager && current_rom_) {
    current_rom_->resource_label()->DisplayLabels(&show_resource_label_manager);
    if (current_project_.project_opened() &&
        !current_project_.labels_filename.empty()) {
      current_project_.labels_filename =
          current_rom_->resource_label()->filename_;
    }
  }

  // Workspace preset dialogs
  if (show_save_workspace_preset_) {
    ImGui::Begin("Save Workspace Preset", &show_save_workspace_preset_,
          ImGuiWindowFlags_AlwaysAutoResize);
    static std::string preset_name = "";
    ImGui::InputText("Name", &preset_name);
    if (ImGui::Button("Save", gui::kDefaultModalSize)) {
      SaveWorkspacePreset(preset_name);
      toast_manager_.Show("Preset saved", editor::ToastType::kSuccess);
      show_save_workspace_preset_ = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", gui::kDefaultModalSize)) {
      show_save_workspace_preset_ = false;
    }
    ImGui::End();
  }

  if (show_load_workspace_preset_) {
    ImGui::Begin("Load Workspace Preset", &show_load_workspace_preset_,
          ImGuiWindowFlags_AlwaysAutoResize);

    // Lazy load workspace presets when UI is accessed
    if (!workspace_manager_.workspace_presets_loaded()) {
      RefreshWorkspacePresets();
    }

    for (const auto& name : workspace_manager_.workspace_presets()) {
      if (ImGui::Selectable(name.c_str())) {
        LoadWorkspacePreset(name);
        toast_manager_.Show("Preset loaded", editor::ToastType::kSuccess);
        show_load_workspace_preset_ = false;
      }
    }
    if (workspace_manager_.workspace_presets().empty())
      ImGui::Text("No presets found");
    ImGui::End();
  }

  // Layout presets UI (session dialogs are drawn by SessionCoordinator at lines 907-915)
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
 * - Session management: EditorManager (searches for empty session or creates new)
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
  auto file_name = util::FileDialogWrapper::ShowOpenFileDialog();
  if (file_name.empty()) {
    return absl::OkStatus();
  }

  if (HasDuplicateSession(file_name)) {
    toast_manager_.Show("ROM already open in another session",
                        editor::ToastType::kWarning);
    return absl::OkStatus();
  }

  // Delegate ROM loading to RomFileManager
  Rom temp_rom;
  RETURN_IF_ERROR(rom_file_manager_.LoadRom(&temp_rom, file_name));

  RomSession* target_session = nullptr;
  for (auto& session : sessions_) {
    if (!session.rom.is_loaded()) {
      target_session = &session;
      LOG_DEBUG("EditorManager", "Found empty session to populate with ROM: %s",
                file_name.c_str());
      break;
    }
  }

  if (target_session) {
    target_session->rom = std::move(temp_rom);
    target_session->filepath = file_name;
    current_rom_ = &target_session->rom;
    current_editor_set_ = &target_session->editors;
    ConfigureEditorDependencies(current_editor_set_, &target_session->rom,
                                target_session->editors.session_id());
  } else {
    size_t new_session_id = sessions_.size();
    sessions_.emplace_back(std::move(temp_rom), &user_settings_, new_session_id);
    RomSession& session = sessions_.back();
    session.filepath = file_name;
    current_rom_ = &session.rom;
    current_editor_set_ = &session.editors;
    ConfigureEditorDependencies(current_editor_set_, current_rom_, new_session_id);
  }

#ifdef YAZE_ENABLE_TESTING
  test::TestManager::Get().SetCurrentRom(current_rom_);
#endif

  auto& manager = core::RecentFilesManager::GetInstance();
  manager.AddFile(file_name);
  manager.Save();

  RETURN_IF_ERROR(LoadAssets());

  if (ui_coordinator_) {
  ui_coordinator_->SetWelcomeScreenVisible(false);
  editor_selection_dialog_.ClearRecentEditors();
  ui_coordinator_->SetEditorSelectionVisible(true);
  }

  return absl::OkStatus();
}

absl::Status EditorManager::LoadAssets() {
  if (!current_rom_ || !current_editor_set_) {
    return absl::FailedPreconditionError("No ROM or editor set loaded");
  }

  auto start_time = std::chrono::steady_clock::now();

  // Set renderer for emulator (lazy initialization happens in Run())
  if (renderer_) {
    emulator_.set_renderer(renderer_);
  }

  // Initialize all editors - this registers their cards with EditorCardRegistry
  // and sets up any editor-specific resources. Must be called before Load().
  current_editor_set_->overworld_editor_.Initialize();
  current_editor_set_->message_editor_.Initialize();
  current_editor_set_->graphics_editor_.Initialize();
  current_editor_set_->screen_editor_.Initialize();
  current_editor_set_->sprite_editor_.Initialize();
  current_editor_set_->palette_editor_.Initialize();
  current_editor_set_->assembly_editor_.Initialize();
  current_editor_set_->music_editor_.Initialize();
  current_editor_set_->settings_editor_.Initialize();  // Initialize settings editor to register System cards
  // Initialize the dungeon editor with the renderer
  current_editor_set_->dungeon_editor_.Initialize(renderer_, current_rom_);
  ASSIGN_OR_RETURN(*gfx::Arena::Get().mutable_gfx_sheets(),
                   LoadAllGraphicsData(*current_rom_));
  RETURN_IF_ERROR(current_editor_set_->overworld_editor_.Load());
  RETURN_IF_ERROR(current_editor_set_->dungeon_editor_.Load());
  RETURN_IF_ERROR(current_editor_set_->screen_editor_.Load());
  RETURN_IF_ERROR(current_editor_set_->settings_editor_.Load());
  RETURN_IF_ERROR(current_editor_set_->sprite_editor_.Load());
  RETURN_IF_ERROR(current_editor_set_->message_editor_.Load());
  RETURN_IF_ERROR(current_editor_set_->music_editor_.Load());
  RETURN_IF_ERROR(current_editor_set_->palette_editor_.Load());

  gfx::PerformanceProfiler::Get().PrintSummary();

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
  if (!current_rom_ || !current_editor_set_) {
    return absl::FailedPreconditionError("No ROM or editor set loaded");
  }

  // Save editor-specific data first
  if (core::FeatureFlags::get().kSaveDungeonMaps) {
    RETURN_IF_ERROR(zelda3::SaveDungeonMaps(
        *current_rom_, current_editor_set_->screen_editor_.dungeon_maps_));
  }

  RETURN_IF_ERROR(current_editor_set_->overworld_editor_.Save());

  if (core::FeatureFlags::get().kSaveGraphicsSheet)
    RETURN_IF_ERROR(
        SaveAllGraphicsData(*current_rom_, gfx::Arena::Get().gfx_sheets()));

  // Delegate final ROM file writing to RomFileManager
  return rom_file_manager_.SaveRom(current_rom_);
}

absl::Status EditorManager::SaveRomAs(const std::string& filename) {
  if (!current_rom_ || !current_editor_set_) {
    return absl::FailedPreconditionError("No ROM or editor set loaded");
  }

  if (core::FeatureFlags::get().kSaveDungeonMaps) {
    RETURN_IF_ERROR(zelda3::SaveDungeonMaps(
        *current_rom_, current_editor_set_->screen_editor_.dungeon_maps_));
  }

  RETURN_IF_ERROR(current_editor_set_->overworld_editor_.Save());

  if (core::FeatureFlags::get().kSaveGraphicsSheet)
    RETURN_IF_ERROR(
        SaveAllGraphicsData(*current_rom_, gfx::Arena::Get().gfx_sheets()));

  auto save_status = rom_file_manager_.SaveRomAs(current_rom_, filename);
  if (save_status.ok()) {
    size_t current_session_idx = GetCurrentSessionIndex();
    if (current_session_idx < sessions_.size()) {
      sessions_[current_session_idx].filepath = filename;
    }

    auto& manager = core::RecentFilesManager::GetInstance();
    manager.AddFile(filename);
    manager.Save();
  }

  return save_status;
}

absl::Status EditorManager::OpenRomOrProject(const std::string& filename) {
  if (filename.empty()) {
    return absl::OkStatus();
  }
  if (absl::StrContains(filename, ".yaze")) {
    RETURN_IF_ERROR(current_project_.Open(filename));
    RETURN_IF_ERROR(OpenProject());
  } else {
    Rom temp_rom;
    RETURN_IF_ERROR(rom_file_manager_.LoadRom(&temp_rom, filename));
    size_t new_session_id = sessions_.size();
    sessions_.emplace_back(std::move(temp_rom), &user_settings_, new_session_id);
    RomSession& session = sessions_.back();
    current_rom_ = &session.rom;
    current_editor_set_ = &session.editors;
    ConfigureEditorDependencies(current_editor_set_, current_rom_, new_session_id);
    RETURN_IF_ERROR(LoadAssets());

    // Hide welcome screen and show editor selection when ROM is loaded
    ui_coordinator_->SetWelcomeScreenVisible(false);
    editor_selection_dialog_.ClearRecentEditors();
    ui_coordinator_->SetEditorSelectionVisible(true);
  }
  return absl::OkStatus();
}

absl::Status EditorManager::CreateNewProject(const std::string& template_name) {
  // Delegate to ProjectManager
  auto status = project_manager_.CreateNewProject(template_name);
  if (status.ok()) {
    current_project_ = project_manager_.GetCurrentProject();
  // Show project creation dialog
  popup_manager_->Show("Create New Project");
  }
  return status;
}

absl::Status EditorManager::OpenProject() {
  auto file_path = util::FileDialogWrapper::ShowOpenFileDialog();
  if (file_path.empty()) {
    return absl::OkStatus();
  }

  core::YazeProject new_project;
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

  // Load ROM if specified in project
  if (!current_project_.rom_filename.empty()) {
    Rom temp_rom;
    RETURN_IF_ERROR(
        rom_file_manager_.LoadRom(&temp_rom, current_project_.rom_filename));
    size_t new_session_id = sessions_.size();
    sessions_.emplace_back(std::move(temp_rom), &user_settings_, new_session_id);
    RomSession& session = sessions_.back();
    current_rom_ = &session.rom;
    current_editor_set_ = &session.editors;
    ConfigureEditorDependencies(current_editor_set_, current_rom_, new_session_id);

    // Apply project feature flags to the session
    session.feature_flags = current_project_.feature_flags;

    // Update test manager with current ROM for ROM-dependent tests (only when tests are enabled)
#ifdef YAZE_ENABLE_TESTING
    LOG_DEBUG("EditorManager", "Setting ROM in TestManager - %p ('%s')",
              (void*)current_rom_,
              current_rom_ ? current_rom_->title().c_str() : "null");
    test::TestManager::Get().SetCurrentRom(current_rom_);
#endif

    if (current_editor_set_ && !current_project_.code_folder.empty()) {
      current_editor_set_->assembly_editor_.OpenFolder(
          current_project_.code_folder);
    }

    RETURN_IF_ERROR(LoadAssets());

    // Hide welcome screen and show editor selection when project ROM is loaded
    ui_coordinator_->SetWelcomeScreenVisible(false);
    editor_selection_dialog_.ClearRecentEditors();
    ui_coordinator_->SetEditorSelectionVisible(true);
  }

  // Apply workspace settings
  user_settings_.prefs().font_global_scale =
      current_project_.workspace_settings.font_global_scale;
  user_settings_.prefs().autosave_enabled =
      current_project_.workspace_settings.autosave_enabled;
  user_settings_.prefs().autosave_interval =
      current_project_.workspace_settings.autosave_interval_secs;
  ImGui::GetIO().FontGlobalScale = user_settings_.prefs().font_global_scale;

  // Add to recent files
  auto& manager = core::RecentFilesManager::GetInstance();
  manager.AddFile(current_project_.filepath);
  manager.Save();

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
  if (current_rom_ && current_editor_set_) {
    size_t session_idx = GetCurrentSessionIndex();
    if (session_idx < sessions_.size()) {
      current_project_.feature_flags = sessions_[session_idx].feature_flags;
    }

    current_project_.workspace_settings.font_global_scale =
        user_settings_.prefs().font_global_scale;
    current_project_.workspace_settings.autosave_enabled =
        user_settings_.prefs().autosave_enabled;
    current_project_.workspace_settings.autosave_interval_secs =
        user_settings_.prefs().autosave_interval;

    // Save recent files
    auto& manager = core::RecentFilesManager::GetInstance();
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
    auto& manager = core::RecentFilesManager::GetInstance();
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
  core::YazeProject imported_project;

  if (project_path.ends_with(".zsproj")) {
    RETURN_IF_ERROR(imported_project.ImportZScreamProject(project_path));
    toast_manager_.Show(
        "ZScream project imported successfully. Please configure ROM and "
        "folders.",
        editor::ToastType::kInfo, 5.0f);
  } else {
    RETURN_IF_ERROR(imported_project.Open(project_path));
  }

  current_project_ = std::move(imported_project);
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

  for (auto& session : sessions_) {
    if (&session.rom == rom) {
      current_rom_ = &session.rom;
      current_editor_set_ = &session.editors;

      // Update test manager with current ROM for ROM-dependent tests
      test::TestManager::Get().SetCurrentRom(current_rom_);

      return absl::OkStatus();
    }
  }
  // If ROM wasn't found in existing sessions, treat as new session.
  // Copying an external ROM object is avoided; instead, fail.
  return absl::NotFoundError("ROM not found in existing sessions");
}

void EditorManager::CreateNewSession() {
  if (session_coordinator_) {
    session_coordinator_->CreateNewSession();
    
    // Wire editor contexts for new session
    if (!sessions_.empty()) {
  RomSession& session = sessions_.back();
  session.editors.set_user_settings(&user_settings_);
      ConfigureEditorDependencies(&session.editors, &session.rom,
                                  session.editors.session_id());
      current_session_id_ = session.editors.session_id();
    }
  }

  // Don't switch to the new session automatically
  toast_manager_.Show(
      absl::StrFormat("New session created (Session %zu)", sessions_.size()),
      editor::ToastType::kSuccess);

  // Show session manager if user has multiple sessions now
  if (sessions_.size() > 2) {
    toast_manager_.Show(
        "Tip: Use Workspace → Sessions → Session Switcher for quick navigation",
        editor::ToastType::kInfo, 5.0f);
  }
}

void EditorManager::DuplicateCurrentSession() {
  if (!current_rom_) {
    toast_manager_.Show("No current ROM to duplicate",
                        editor::ToastType::kWarning);
    return;
  }

  if (session_coordinator_) {
    session_coordinator_->DuplicateCurrentSession();
    
    // Wire editor contexts for duplicated session
    if (!sessions_.empty()) {
      RomSession& session = sessions_.back();
      ConfigureEditorDependencies(&session.editors, &session.rom,
                                  session.editors.session_id());
      current_session_id_ = session.editors.session_id();
    }
  }
}

void EditorManager::CloseCurrentSession() {
  if (session_coordinator_) {
    session_coordinator_->CloseCurrentSession();
    
    // Update current pointers after session change
    if (!sessions_.empty()) {
      size_t active_index = session_coordinator_->GetActiveSessionIndex();
      if (active_index < sessions_.size()) {
        current_rom_ = &sessions_[active_index].rom;
        current_editor_set_ = &sessions_[active_index].editors;
        current_session_id_ = active_index;
#ifdef YAZE_ENABLE_TESTING
        test::TestManager::Get().SetCurrentRom(current_rom_);
#endif
      }
    }
  }
}

void EditorManager::RemoveSession(size_t index) {
  if (session_coordinator_) {
    session_coordinator_->RemoveSession(index);
    
    // Update current pointers after session change
    if (!sessions_.empty()) {
      size_t active_index = session_coordinator_->GetActiveSessionIndex();
      if (active_index < sessions_.size()) {
        current_rom_ = &sessions_[active_index].rom;
        current_editor_set_ = &sessions_[active_index].editors;
        current_session_id_ = active_index;
#ifdef YAZE_ENABLE_TESTING
        test::TestManager::Get().SetCurrentRom(current_rom_);
#endif
      }
    }
  }
}

void EditorManager::SwitchToSession(size_t index) {
  if (!session_coordinator_) {
    return;
  }

    session_coordinator_->SwitchToSession(index);
    
  if (index >= sessions_.size()) {
    return;
  }

  auto& session = sessions_[index];
  current_rom_ = &session.rom;
  current_editor_set_ = &session.editors;
  current_session_id_ = index;

#ifdef YAZE_ENABLE_TESTING
  test::TestManager::Get().SetCurrentRom(current_rom_);
#endif
}

size_t EditorManager::GetCurrentSessionIndex() const {
  if (session_coordinator_) {
    return session_coordinator_->GetActiveSessionIndex();
  }
  
  // Fallback to finding by ROM pointer
  for (size_t i = 0; i < sessions_.size(); ++i) {
    if (&sessions_[i].rom == current_rom_ &&
        sessions_[i].custom_name != "[CLOSED SESSION]") {
      return i;
    }
  }
  return 0;  // Default to first session if not found
}

size_t EditorManager::GetActiveSessionCount() const {
  if (session_coordinator_) {
    return session_coordinator_->GetActiveSessionCount();
  }
  
  // Fallback to counting non-closed sessions
  size_t count = 0;
  for (const auto& session : sessions_) {
    if (session.custom_name != "[CLOSED SESSION]") {
      count++;
    }
  }
  return count;
}

std::string EditorManager::GenerateUniqueEditorTitle(
    EditorType type, size_t session_index) const {
  const char* base_name = kEditorNames[static_cast<int>(type)];

  // Delegate to SessionCoordinator for multi-session title generation
  if (session_coordinator_) {
    return session_coordinator_->GenerateUniqueEditorTitle(base_name,
                                                           session_index);
  }

  // Fallback for single session or no coordinator
  return std::string(base_name);
}

// ============================================================================
// Jump-to Functionality for Cross-Editor Navigation
// ============================================================================

void EditorManager::JumpToDungeonRoom(int room_id) {
  if (!current_editor_set_)
    return;

  // Switch to dungeon editor
  SwitchToEditor(EditorType::kDungeon);

  // Open the room in the dungeon editor
  current_editor_set_->dungeon_editor_.add_room(room_id);
}

void EditorManager::JumpToOverworldMap(int map_id) {
  if (!current_editor_set_)
    return;

  // Switch to overworld editor
  SwitchToEditor(EditorType::kOverworld);

  // Set the current map in the overworld editor
  current_editor_set_->overworld_editor_.set_current_map(map_id);
}

void EditorManager::SwitchToEditor(EditorType editor_type) {
  if (!current_editor_set_)
    return;

  // Toggle the editor
  for (auto* editor : current_editor_set_->active_editors_) {
    if (editor->type() == editor_type) {
      editor->toggle_active();

      if (IsCardBasedEditor(editor_type)) {
        // Using EditorCardRegistry directly

        if (*editor->active()) {
          // Editor activated - set its category
          card_registry_.SetActiveCategory(
              EditorRegistry::GetEditorCategory(editor_type));
        } else {
          // Editor deactivated - switch to another active card-based editor
          for (auto* other : current_editor_set_->active_editors_) {
            if (*other->active() && IsCardBasedEditor(other->type()) &&
                other != editor) {
            card_registry_.SetActiveCategory(
                EditorRegistry::GetEditorCategory(other->type()));
              break;
            }
          }
        }
      }
      return;
    }
  }

  // Handle non-editor-class cases
  if (editor_type == EditorType::kAssembly) {
    show_asm_editor_ = !show_asm_editor_;
  } else if (editor_type == EditorType::kEmulator) {
    show_emulator_ = !show_emulator_;
    if (show_emulator_) {
      card_registry_.SetActiveCategory("Emulator");
    }
  }
}

// SessionScope implementation
EditorManager::SessionScope::SessionScope(EditorManager* manager,
                                          size_t session_id)
    : manager_(manager),
      prev_rom_(manager->current_rom_),
      prev_editor_set_(manager->current_editor_set_),
      prev_session_id_(manager->current_session_id_) {
  
  // Set new session context
  if (session_id < manager->sessions_.size()) {
    manager->current_rom_ = &manager->sessions_[session_id].rom;
    manager->current_editor_set_ = &manager->sessions_[session_id].editors;
    manager->current_session_id_ = session_id;
  }
}

EditorManager::SessionScope::~SessionScope() {
  // Restore previous context
  manager_->current_rom_ = prev_rom_;
  manager_->current_editor_set_ = prev_editor_set_;
  manager_->current_session_id_ = prev_session_id_;
}

bool EditorManager::HasDuplicateSession(const std::string& filepath) {
  for (const auto& session : sessions_) {
    if (session.filepath == filepath) {
      return true;
    }
  }
  return false;
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
 */
void EditorManager::ConfigureEditorDependencies(EditorSet* editor_set, Rom* rom,
                                                size_t session_id) {
  if (!editor_set) {
    return;
  }

  EditorDependencies deps;
  deps.rom = rom;
  deps.session_id = session_id;
  deps.card_registry = &card_registry_;
  deps.toast_manager = &toast_manager_;
  deps.popup_manager = popup_manager_.get();
  deps.shortcut_manager = &shortcut_manager_;
  deps.shared_clipboard = &shared_clipboard_;
  deps.user_settings = &user_settings_;
  deps.renderer = renderer_;

  editor_set->ApplyDependencies(deps);
}

void EditorSet::ApplyDependencies(
    const EditorDependencies& dependencies) {
  // Inject dependencies into migrated editors using base class method
  // Note: ROM pointer comes from constructor, dependencies provide managers/services
  for (auto* editor : active_editors_) {
    editor->SetDependencies(dependencies);
  }
  // Non-editor members need manual ROM updates if dependencies.rom differs
  memory_editor_.set_rom(dependencies.rom);
}

}  // namespace editor
}  // namespace yaze
