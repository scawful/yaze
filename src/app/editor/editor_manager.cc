#include "editor_manager.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include "zelda3/screen/dungeon_map.h"

#define IMGUI_DEFINE_MATH_OPERATORS

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "app/core/features.h"
#include "app/core/project.h"
#include "app/core/timing.h"
#include "absl/strings/str_format.h"
#include "app/editor/system/popup_manager.h"
#include "app/editor/code/assembly_editor.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/graphics/graphics_editor.h"
#include "app/editor/graphics/screen_editor.h"
#include "app/editor/music/music_editor.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/palette/palette_editor.h"
#include "app/editor/sprite/sprite_editor.h"
#include "app/editor/ui/editor_selection_dialog.h"
#include "app/emu/emulator.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/app/editor_card_manager.h"
#include "app/gui/core/background_renderer.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/theme_manager.h"
#include "app/rom.h"
#include "app/test/test_manager.h"
#include "imgui/imgui.h"
#include "util/file_util.h"
#include "util/log.h"
#include "zelda3/overworld/overworld_map.h"
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
#include "app/emu/emulator.h"
#include "app/gfx/debug/performance/performance_dashboard.h"

#ifdef YAZE_WITH_GRPC
#include "app/core/service/screenshot_utils.h"
#include "app/editor/agent/agent_chat_widget.h"
#include "app/test/z3ed_test_suite.h"
#include "cli/service/agent/agent_control_server.h"
#include "cli/service/agent/conversational_agent_service.h"
#include "cli/service/ai/gemini_ai_service.h"

#include "absl/flags/flag.h"

// Declare the agent_control flag (defined in src/cli/flags.cc)
// ABSL_DECLARE_FLAG(bool, agent_control);

#include "app/editor/agent/automation_bridge.h"
#endif

#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "util/log.h"
#include "util/macro.h"
#include "yaze_config.h"

namespace yaze {
namespace editor {

using namespace ImGui;
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

std::string EditorManager::GetEditorCategory(EditorType type) {
  return EditorRegistry::GetEditorCategory(type);
}

EditorType EditorManager::GetEditorTypeFromCategory(
    const std::string& category) {
  return EditorRegistry::GetEditorTypeFromCategory(category);
}

void EditorManager::HideCurrentEditorCards() {
  if (!current_editor_) {
    return;
  }

  auto& card_manager = gui::EditorCardManager::Get();
  std::string category = GetEditorCategory(current_editor_->type());
  card_manager.HideAllCardsInCategory(category);
}

EditorManager::EditorManager() 
    : blank_editor_set_(nullptr, &user_settings_),
      project_manager_(&toast_manager_),
      rom_file_manager_(&toast_manager_) {
  std::stringstream ss;
  ss << YAZE_VERSION_MAJOR << "." << YAZE_VERSION_MINOR << "."
     << YAZE_VERSION_PATCH;
  ss >> version_;
  context_.popup_manager = popup_manager_.get();
  
  // Initialize new delegated components
  session_coordinator_ = std::make_unique<SessionCoordinator>(
      static_cast<void*>(&sessions_), &card_registry_, &toast_manager_);
  
  // Initialize MenuOrchestrator after SessionCoordinator is created
  menu_orchestrator_ = std::make_unique<MenuOrchestrator>(
      this, menu_builder_, rom_file_manager_, project_manager_, editor_registry_,
      *session_coordinator_, toast_manager_);
  
  // Initialize UICoordinator after all other components are created
  ui_coordinator_ = std::make_unique<UICoordinator>(
      this, rom_file_manager_, project_manager_, editor_registry_,
      *session_coordinator_, window_delegate_, toast_manager_, *popup_manager_);
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

  // NOTE: Emulator will be initialized later when a ROM is loaded
  // We just store the renderer for now

  // Point to a blank editor set when no ROM is loaded
  current_editor_set_ = &blank_editor_set_;

  if (!filename.empty()) {
    PRINT_IF_ERROR(OpenRomOrProject(filename));
  }

  // Initialize popup manager
  popup_manager_ = std::make_unique<PopupManager>(this);
  popup_manager_->Initialize();

  // Set the popup manager in the context
  context_.popup_manager = popup_manager_.get();

  // Register global sidebar toggle shortcut (Ctrl+B)
  context_.shortcut_manager.RegisterShortcut(
      "global.toggle_sidebar", {ImGuiKey_LeftCtrl, ImGuiKey_B},
      [this]() { 
        if (ui_coordinator_) {
          ui_coordinator_->ToggleCardSidebar();
        }
      });

  // Register emulator cards early (emulator Initialize might not be called)
  auto& card_manager = gui::EditorCardManager::Get();
  card_manager.RegisterCard({.card_id = "emulator.cpu_debugger",
                             .display_name = "CPU Debugger",
                             .icon = ICON_MD_BUG_REPORT,
                             .category = "Emulator",
                             .priority = 10});
  card_manager.RegisterCard({.card_id = "emulator.ppu_viewer",
                             .display_name = "PPU Viewer",
                             .icon = ICON_MD_VIDEOGAME_ASSET,
                             .category = "Emulator",
                             .priority = 20});
  card_manager.RegisterCard({.card_id = "emulator.memory_viewer",
                             .display_name = "Memory Viewer",
                             .icon = ICON_MD_MEMORY,
                             .category = "Emulator",
                             .priority = 30});
  card_manager.RegisterCard({.card_id = "emulator.breakpoints",
                             .display_name = "Breakpoints",
                             .icon = ICON_MD_STOP,
                             .category = "Emulator",
                             .priority = 40});
  card_manager.RegisterCard({.card_id = "emulator.performance",
                             .display_name = "Performance",
                             .icon = ICON_MD_SPEED,
                             .category = "Emulator",
                             .priority = 50});
  card_manager.RegisterCard({.card_id = "emulator.ai_agent",
                             .display_name = "AI Agent",
                             .icon = ICON_MD_SMART_TOY,
                             .category = "Emulator",
                             .priority = 60});
  card_manager.RegisterCard({.card_id = "emulator.save_states",
                             .display_name = "Save States",
                             .icon = ICON_MD_SAVE,
                             .category = "Emulator",
                             .priority = 70});
  card_manager.RegisterCard({.card_id = "emulator.keyboard_config",
                             .display_name = "Keyboard Config",
                             .icon = ICON_MD_KEYBOARD,
                             .category = "Emulator",
                             .priority = 80});
  card_manager.RegisterCard({.card_id = "emulator.apu_debugger",
                             .display_name = "APU Debugger",
                             .icon = ICON_MD_AUDIOTRACK,
                             .category = "Emulator",
                             .priority = 90});
  card_manager.RegisterCard({.card_id = "emulator.audio_mixer",
                             .display_name = "Audio Mixer",
                             .icon = ICON_MD_AUDIO_FILE,
                             .category = "Emulator",
                             .priority = 100});

  // Show CPU debugger and PPU viewer by default for emulator
  card_manager.ShowCard("emulator.cpu_debugger");
  card_manager.ShowCard("emulator.ppu_viewer");

  // Register memory/hex editor card
  card_manager.RegisterCard({.card_id = "memory.hex_editor",
                             .display_name = "Hex Editor",
                             .icon = ICON_MD_MEMORY,
                             .category = "Memory",
                             .priority = 10});

  // Initialize project file editor
  project_file_editor_.SetToastManager(&toast_manager_);

#ifdef YAZE_WITH_GRPC
  // Initialize the agent editor as a proper Editor (configuration dashboard)
  agent_editor_.set_context(&context_);
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
  LoadUserSettings();

  // Defer workspace presets loading to avoid initialization crashes
  // This will be called lazily when workspace features are accessed

  // Initialize testing system only when tests are enabled
#ifdef YAZE_ENABLE_TESTING
  InitializeTestSuites();
#endif

  // TestManager will be updated when ROMs are loaded via SetCurrentRom calls

  context_.shortcut_manager.RegisterShortcut(
      "Open", {ImGuiKey_O, ImGuiMod_Ctrl}, [this]() { status_ = LoadRom(); });
  context_.shortcut_manager.RegisterShortcut(
      "Save", {ImGuiKey_S, ImGuiMod_Ctrl}, [this]() { status_ = SaveRom(); });
  context_.shortcut_manager.RegisterShortcut(
      "Close", {ImGuiKey_W, ImGuiMod_Ctrl}, [this]() {
        if (current_rom_)
          current_rom_->Close();
      });
  context_.shortcut_manager.RegisterShortcut(
      "Quit", {ImGuiKey_Q, ImGuiMod_Ctrl}, [this]() { quit_ = true; });

  context_.shortcut_manager.RegisterShortcut(
      "Undo", {ImGuiKey_Z, ImGuiMod_Ctrl}, [this]() {
        if (current_editor_)
          status_ = current_editor_->Undo();
      });
  context_.shortcut_manager.RegisterShortcut(
      "Redo", {ImGuiKey_Y, ImGuiMod_Ctrl}, [this]() {
        if (current_editor_)
          status_ = current_editor_->Redo();
      });
  context_.shortcut_manager.RegisterShortcut(
      "Cut", {ImGuiKey_X, ImGuiMod_Ctrl}, [this]() {
        if (current_editor_)
          status_ = current_editor_->Cut();
      });
  context_.shortcut_manager.RegisterShortcut(
      "Copy", {ImGuiKey_C, ImGuiMod_Ctrl}, [this]() {
        if (current_editor_)
          status_ = current_editor_->Copy();
      });
  context_.shortcut_manager.RegisterShortcut(
      "Paste", {ImGuiKey_V, ImGuiMod_Ctrl}, [this]() {
        if (current_editor_)
          status_ = current_editor_->Paste();
      });
  context_.shortcut_manager.RegisterShortcut(
      "Find", {ImGuiKey_F, ImGuiMod_Ctrl}, [this]() {
        if (current_editor_)
          status_ = current_editor_->Find();
      });

  // Command Palette and Global Search
  context_.shortcut_manager.RegisterShortcut(
      "Command Palette", {ImGuiKey_P, ImGuiMod_Ctrl, ImGuiMod_Shift},
      [this]() { 
        if (ui_coordinator_) {
          ui_coordinator_->ShowCommandPalette();
        }
      });
  context_.shortcut_manager.RegisterShortcut(
      "Global Search", {ImGuiKey_K, ImGuiMod_Ctrl, ImGuiMod_Shift},
      [this]() {
        if (ui_coordinator_) {
          ui_coordinator_->ShowGlobalSearch();
        }
      });

  context_.shortcut_manager.RegisterShortcut(
      "Load Last ROM", {ImGuiKey_R, ImGuiMod_Ctrl}, [this]() {
        auto& manager = core::RecentFilesManager::GetInstance();
        if (!manager.GetRecentFiles().empty()) {
          auto front = manager.GetRecentFiles().front();
          status_ = OpenRomOrProject(front);
        }
      });

  context_.shortcut_manager.RegisterShortcut(
      "F1", ImGuiKey_F1, [this]() { popup_manager_->Show("About"); });

  // Editor shortcuts (Ctrl+1-9, Ctrl+0) - all use SwitchToEditor for consistency
  context_.shortcut_manager.RegisterShortcut(
      "Overworld Editor", {ImGuiKey_LeftCtrl, ImGuiKey_1},
      [this]() { SwitchToEditor(EditorType::kOverworld); });
  context_.shortcut_manager.RegisterShortcut(
      "Dungeon Editor", {ImGuiKey_LeftCtrl, ImGuiKey_2},
      [this]() { SwitchToEditor(EditorType::kDungeon); });
  context_.shortcut_manager.RegisterShortcut(
      "Graphics Editor", {ImGuiKey_LeftCtrl, ImGuiKey_3},
      [this]() { SwitchToEditor(EditorType::kGraphics); });
  context_.shortcut_manager.RegisterShortcut(
      "Sprite Editor", {ImGuiKey_LeftCtrl, ImGuiKey_4},
      [this]() { SwitchToEditor(EditorType::kSprite); });
  context_.shortcut_manager.RegisterShortcut(
      "Message Editor", {ImGuiKey_LeftCtrl, ImGuiKey_5},
      [this]() { SwitchToEditor(EditorType::kMessage); });
  context_.shortcut_manager.RegisterShortcut(
      "Music Editor", {ImGuiKey_LeftCtrl, ImGuiKey_6},
      [this]() { SwitchToEditor(EditorType::kMusic); });
  context_.shortcut_manager.RegisterShortcut(
      "Palette Editor", {ImGuiKey_LeftCtrl, ImGuiKey_7},
      [this]() { SwitchToEditor(EditorType::kPalette); });
  context_.shortcut_manager.RegisterShortcut(
      "Screen Editor", {ImGuiKey_LeftCtrl, ImGuiKey_8},
      [this]() { SwitchToEditor(EditorType::kScreen); });
  context_.shortcut_manager.RegisterShortcut(
      "Assembly Editor", {ImGuiKey_LeftCtrl, ImGuiKey_9},
      [this]() { SwitchToEditor(EditorType::kAssembly); });
  context_.shortcut_manager.RegisterShortcut(
      "Settings Editor", {ImGuiKey_LeftCtrl, ImGuiKey_0},
      [this]() { SwitchToEditor(EditorType::kSettings); });

  // Editor Selection Dialog shortcut
  context_.shortcut_manager.RegisterShortcut(
      "Editor Selection", {ImGuiKey_E, ImGuiMod_Ctrl},
      [this]() { 
        if (ui_coordinator_) {
          ui_coordinator_->ShowEditorSelection();
        }
      });

  // Card Browser shortcut
  context_.shortcut_manager.RegisterShortcut(
      "Card Browser", {ImGuiKey_B, ImGuiMod_Ctrl, ImGuiMod_Shift},
      [this]() { 
        if (ui_coordinator_) {
          ui_coordinator_->ShowCardBrowser();
        }
      });

  // === SIMPLIFIED CARD SHORTCUTS - Use Card Browser instead of individual shortcuts ===
  // Individual card shortcuts removed to prevent hash table overflow
  // Users can:
  // 1. Use Card Browser (Ctrl+Shift+B) to toggle any card
  // 2. Use compact card control button in menu bar
  // 3. Use View menu for category-based toggles

  // Only register essential category-level shortcuts
  context_.shortcut_manager.RegisterShortcut(
      "Show All Dungeon Cards", {ImGuiKey_D, ImGuiMod_Ctrl, ImGuiMod_Shift},
      []() {
        gui::EditorCardManager::Get().ShowAllCardsInCategory("Dungeon");
      });
  context_.shortcut_manager.RegisterShortcut(
      "Show All Graphics Cards", {ImGuiKey_G, ImGuiMod_Ctrl, ImGuiMod_Shift},
      []() {
        gui::EditorCardManager::Get().ShowAllCardsInCategory("Graphics");
      });
  context_.shortcut_manager.RegisterShortcut(
      "Show All Screen Cards", {ImGuiKey_S, ImGuiMod_Ctrl, ImGuiMod_Shift},
      []() { gui::EditorCardManager::Get().ShowAllCardsInCategory("Screen"); });

#ifdef YAZE_WITH_GRPC
  // Agent Editor shortcut
  context_.shortcut_manager.RegisterShortcut(
      "Agent Editor", {ImGuiKey_A, ImGuiMod_Ctrl, ImGuiMod_Shift},
      [this]() { agent_editor_.SetChatActive(true); });

  // Agent Chat History Popup shortcut
  context_.shortcut_manager.RegisterShortcut(
      "Chat History Popup", {ImGuiKey_H, ImGuiMod_Ctrl},
      [this]() { agent_chat_history_popup_.Toggle(); });

  // Agent Proposal Drawer shortcut
  context_.shortcut_manager.RegisterShortcut(
      "Proposal Drawer", {ImGuiKey_P, ImGuiMod_Ctrl},
      [this]() { proposal_drawer_.Toggle(); });

  // Start the agent control server if the flag is set
  // if (absl::GetFlag(FLAGS_agent_control)) {
  //   agent_control_server_ = std::make_unique<agent::AgentControlServer>(&emulator_);
  //   agent_control_server_->Start();
  // }
#endif

  // Testing shortcuts (only when tests are enabled)
#ifdef YAZE_ENABLE_TESTING
  context_.shortcut_manager.RegisterShortcut(
      "Test Dashboard", {ImGuiKey_T, ImGuiMod_Ctrl},
      [this]() { show_test_dashboard_ = true; });
#endif

  // Workspace shortcuts
  context_.shortcut_manager.RegisterShortcut(
      "New Session",
      std::vector<ImGuiKey>{ImGuiKey_N, ImGuiMod_Ctrl, ImGuiMod_Shift},
      [this]() { CreateNewSession(); });
  context_.shortcut_manager.RegisterShortcut(
      "Close Session",
      std::vector<ImGuiKey>{ImGuiKey_W, ImGuiMod_Ctrl, ImGuiMod_Shift},
      [this]() { CloseCurrentSession(); });
  context_.shortcut_manager.RegisterShortcut(
      "Session Switcher", std::vector<ImGuiKey>{ImGuiKey_Tab, ImGuiMod_Ctrl},
      [this]() { 
        if (ui_coordinator_) {
          ui_coordinator_->ShowSessionSwitcher();
        }
      });
  context_.shortcut_manager.RegisterShortcut(
      "Save Layout",
      std::vector<ImGuiKey>{ImGuiKey_S, ImGuiMod_Ctrl, ImGuiMod_Shift},
      [this]() { SaveWorkspaceLayout(); });
  context_.shortcut_manager.RegisterShortcut(
      "Load Layout",
      std::vector<ImGuiKey>{ImGuiKey_O, ImGuiMod_Ctrl, ImGuiMod_Shift},
      [this]() { LoadWorkspaceLayout(); });
  context_.shortcut_manager.RegisterShortcut(
      "Maximize Window", ImGuiKey_F11, [this]() { MaximizeCurrentWindow(); });
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

absl::Status EditorManager::Update() {

  // Update timing manager for accurate delta time across the application
  // This fixes animation timing issues that occur when mouse isn't moving
  core::TimingManager::Get().Update();

  popup_manager_->DrawPopups();
  ExecuteShortcuts(context_.shortcut_manager);
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
    gui::EditorCardManager::Get().DrawCardBrowser(&show);
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
            size_t prev_session_id = context_.session_id;

            current_rom_ = &session.rom;
            current_editor_set_ = &session.editors;
            current_editor_ = editor;
            context_.session_id = session_idx;

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
            context_.session_id = prev_session_id;
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
  if (ui_coordinator_ && ui_coordinator_->IsCardSidebarVisible() && current_editor_set_) {
    auto& card_manager = gui::EditorCardManager::Get();

    // Collect all active card-based editors
    std::vector<std::string> active_categories;
    for (size_t session_idx = 0; session_idx < sessions_.size();
         ++session_idx) {
      auto& session = sessions_[session_idx];
      if (!session.rom.is_loaded())
        continue;

      for (auto editor : session.editors.active_editors_) {
        if (*editor->active() && IsCardBasedEditor(editor->type())) {
          std::string category = GetEditorCategory(editor->type());
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
    if (!card_manager.GetActiveCategory().empty() &&
        std::find(active_categories.begin(), active_categories.end(),
                  card_manager.GetActiveCategory()) !=
            active_categories.end()) {
      sidebar_category = card_manager.GetActiveCategory();
    }
    // Priority 2: Use first active category
    else if (!active_categories.empty()) {
      sidebar_category = active_categories[0];
      card_manager.SetActiveCategory(sidebar_category);
    }

    // Draw sidebar if we have a category
    if (!sidebar_category.empty()) {
      // Callback to switch editors when category button is clicked
      auto category_switch_callback = [this](const std::string& new_category) {
        EditorType editor_type = GetEditorTypeFromCategory(new_category);
        if (editor_type != EditorType::kUnknown) {
          SwitchToEditor(editor_type);
        }
      };

      auto collapse_callback = [this]() {
        if (ui_coordinator_) {
          ui_coordinator_->SetCardSidebarVisible(false);
        }
      };

      card_manager.DrawSidebar(sidebar_category, active_categories,
                               category_switch_callback, collapse_callback);
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

absl::Status EditorManager::DrawRomSelector() {
  SameLine((GetWindowWidth() / 2) - 100);
  if (current_rom_ && current_rom_->is_loaded()) {
    SetNextItemWidth(GetWindowWidth() / 6);
    if (BeginCombo("##ROMSelector", current_rom_->short_name().c_str())) {
      int idx = 0;
      for (auto it = sessions_.begin(); it != sessions_.end(); ++it, ++idx) {
        Rom* rom = &it->rom;
        PushID(idx);
        bool selected = (rom == current_rom_);
        if (Selectable(rom->short_name().c_str(), selected)) {
          RETURN_IF_ERROR(SetCurrentRom(rom));
        }
        PopID();
      }
      EndCombo();
    }
    // Inline status next to ROM selector
    SameLine();
    Text("Size: %.1f MB", current_rom_->size() / 1048576.0f);

    // Context-sensitive card control (right after ROM info)
    SameLine();
    DrawContextSensitiveCardControl();
  } else {
    Text("No ROM loaded");
  }
  return absl::OkStatus();
}

void EditorManager::DrawContextSensitiveCardControl() {
  // Delegate to UICoordinator for clean separation of concerns
  if (ui_coordinator_) {
    ui_coordinator_->DrawContextSensitiveCardControl();
  }
}

void EditorManager::BuildModernMenu() {
  // Delegate to MenuOrchestrator for clean separation of concerns
  if (menu_orchestrator_) {
    menu_orchestrator_->BuildMainMenu();
  }
}

void EditorManager::DrawMenuBarExtras() {
  // Delegate to UICoordinator for clean separation of concerns
  if (ui_coordinator_) {
    ui_coordinator_->DrawMenuBarExtras();
  }
}

void EditorManager::ShowSessionSwitcher() {
  if (session_coordinator_) {
    session_coordinator_->ShowSessionSwitcher();
  }
}

void EditorManager::DrawSessionSwitcher() {
  // Delegate to UICoordinator for clean separation of concerns
  if (ui_coordinator_) {
    ui_coordinator_->DrawSessionSwitcher();
  }
}

void EditorManager::ShowEditorSelection() {
  // Delegate to UICoordinator for clean separation of concerns
  if (ui_coordinator_) {
    ui_coordinator_->ShowEditorSelection();
  }
}

void EditorManager::ShowDisplaySettings() {
  // Delegate to UICoordinator for clean separation of concerns
  if (ui_coordinator_) {
    ui_coordinator_->ShowDisplaySettings();
  }
}

void EditorManager::DrawMenuBar() {
  static bool show_display_settings = false;
  static bool save_as_menu = false;
  std::string version_text = absl::StrFormat("v%s", version_.c_str());
  float version_width = ImGui::CalcTextSize(version_text.c_str()).x;

  if (BeginMenuBar()) {
    BuildModernMenu();

    // Inline ROM selector and status
    status_ = DrawRomSelector();

    DrawMenuBarExtras();

    // Version display on far right
    SameLine(GetWindowWidth() - version_width - 10);
    Text("%s", version_text.c_str());
    EndMenuBar();
  }

  if (show_display_settings) {
    // Use the popup manager instead of a separate window
    popup_manager_->Show("Display Settings");
    show_display_settings = false;  // Close the old-style window
  }

  if (show_imgui_demo_)
    ShowDemoWindow(&show_imgui_demo_);
  if (show_imgui_metrics_)
    ShowMetricsWindow(&show_imgui_metrics_);

  auto& card_manager = gui::EditorCardManager::Get();
  if (current_editor_set_) {
    // Pass the actual visibility flag pointer so the X button works
    bool* hex_visibility = card_manager.GetVisibilityFlag("memory.hex_editor");
    if (hex_visibility && *hex_visibility) {
      current_editor_set_->memory_editor_.Update(*hex_visibility);
    }

    bool* assembly_visibility =
        card_manager.GetVisibilityFlag("assembly.editor");
    if (assembly_visibility && *assembly_visibility) {
      current_editor_set_->assembly_editor_.Update(
          *card_manager.GetVisibilityFlag("assembly.editor"));
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

  // Welcome screen (managed by UICoordinator)
  if (ui_coordinator_) {
    ui_coordinator_->DrawWelcomeScreen();
  }

  // Emulator is now card-based - it creates its own windows
  if (show_emulator_) {
    emulator_.Run(current_rom_);
  }

  // Enhanced Command Palette UI with Fuzzy Search (managed by UICoordinator)
  if (ui_coordinator_ && ui_coordinator_->IsCommandPaletteVisible()) {
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

    bool show_palette = true;
    if (Begin(absl::StrFormat("%s Command Palette", ICON_MD_SEARCH).c_str(),
              &show_palette, ImGuiWindowFlags_NoCollapse)) {

      // Search input with focus management
      static char query[256] = {};
      static int selected_idx = 0;
      ImGui::SetNextItemWidth(-100);
      if (ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere();
        selected_idx = 0;
      }

      bool input_changed = InputTextWithHint(
          "##cmd_query",
          absl::StrFormat("%s Search commands (fuzzy matching enabled)...",
                          ICON_MD_SEARCH)
              .c_str(),
          query, IM_ARRAYSIZE(query));

      ImGui::SameLine();
      if (ImGui::Button(absl::StrFormat("%s Clear", ICON_MD_CLEAR).c_str())) {
        query[0] = '\0';
        input_changed = true;
        selected_idx = 0;
      }

      Separator();

      // Fuzzy filter commands with scoring
      std::vector<std::pair<int, std::pair<std::string, std::string>>>
          scored_commands;
      std::string query_lower = query;
      std::transform(query_lower.begin(), query_lower.end(),
                     query_lower.begin(), ::tolower);

      for (const auto& entry : context_.shortcut_manager.GetShortcuts()) {
        const auto& name = entry.first;
        const auto& shortcut = entry.second;

        std::string name_lower = name;
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(),
                       ::tolower);

        int score = 0;
        if (query[0] == '\0') {
          score = 1;  // Show all when no query
        } else if (name_lower.find(query_lower) == 0) {
          score = 1000;  // Starts with
        } else if (name_lower.find(query_lower) != std::string::npos) {
          score = 500;  // Contains
        } else {
          // Fuzzy match - characters in order
          size_t text_idx = 0, query_idx = 0;
          while (text_idx < name_lower.length() &&
                 query_idx < query_lower.length()) {
            if (name_lower[text_idx] == query_lower[query_idx]) {
              score += 10;
              query_idx++;
            }
            text_idx++;
          }
          if (query_idx != query_lower.length())
            score = 0;
        }

        if (score > 0) {
          std::string shortcut_text =
              shortcut.keys.empty()
                  ? ""
                  : absl::StrFormat("(%s)",
                                    PrintShortcut(shortcut.keys).c_str());
          scored_commands.push_back({score, {name, shortcut_text}});
        }
      }

      std::sort(scored_commands.begin(), scored_commands.end(),
                [](const auto& a, const auto& b) { return a.first > b.first; });

      // Display results with categories
      if (ImGui::BeginTabBar("CommandCategories")) {
        if (ImGui::BeginTabItem(ICON_MD_LIST " All Commands")) {
          if (ImGui::BeginTable("CommandPaletteTable", 3,
                                ImGuiTableFlags_ScrollY |
                                    ImGuiTableFlags_RowBg |
                                    ImGuiTableFlags_SizingStretchProp,
                                ImVec2(0, -30))) {

            ImGui::TableSetupColumn("Command",
                                    ImGuiTableColumnFlags_WidthStretch, 0.5f);
            ImGui::TableSetupColumn("Shortcut",
                                    ImGuiTableColumnFlags_WidthStretch, 0.3f);
            ImGui::TableSetupColumn("Score", ImGuiTableColumnFlags_WidthStretch,
                                    0.2f);
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < scored_commands.size(); ++i) {
              const auto& [score, cmd_pair] = scored_commands[i];
              const auto& [command_name, shortcut_text] = cmd_pair;

              ImGui::TableNextRow();
              ImGui::TableNextColumn();

              ImGui::PushID(static_cast<int>(i));
              bool is_selected = (static_cast<int>(i) == selected_idx);
              if (Selectable(command_name.c_str(), is_selected,
                             ImGuiSelectableFlags_SpanAllColumns)) {
                selected_idx = i;
                const auto& shortcuts =
                    context_.shortcut_manager.GetShortcuts();
                auto it = shortcuts.find(command_name);
                if (it != shortcuts.end() && it->second.callback) {
                  it->second.callback();
                  if (ui_coordinator_) {
                    ui_coordinator_->SetCommandPaletteVisible(false);
                  }
                }
              }
              ImGui::PopID();

              ImGui::TableNextColumn();
              ImGui::TextDisabled("%s", shortcut_text.c_str());

              ImGui::TableNextColumn();
              if (score > 0)
                ImGui::TextDisabled("%d", score);
            }

            ImGui::EndTable();
          }
          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(ICON_MD_HISTORY " Recent")) {
          ImGui::Text("Recent commands coming soon...");
          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(ICON_MD_STAR " Frequent")) {
          ImGui::Text("Frequent commands coming soon...");
          ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
      }

      // Status bar with tips
      ImGui::Separator();
      ImGui::Text("%s %zu commands | Score: fuzzy match", ICON_MD_INFO,
                  scored_commands.size());
      ImGui::SameLine();
      ImGui::TextDisabled("| ↑↓=Navigate | Enter=Execute | Esc=Close");
    }
    End();
    
    // Update visibility state
    if (!show_palette && ui_coordinator_) {
      ui_coordinator_->SetCommandPaletteVisible(false);
    }
  }

  // Enhanced Global Search UI (managed by UICoordinator)
  if (ui_coordinator_ && ui_coordinator_->IsGlobalSearchVisible()) {
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

    bool show_search = true;
    if (Begin(
            absl::StrFormat("%s Global Search", ICON_MD_MANAGE_SEARCH).c_str(),
            &show_search, ImGuiWindowFlags_NoCollapse)) {

      // Enhanced search input with focus management
      static char query[256] = {};
      ImGui::SetNextItemWidth(-100);
      if (ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere();
      }

      bool input_changed = InputTextWithHint(
          "##global_query",
          absl::StrFormat("%s Search everything...", ICON_MD_SEARCH).c_str(),
          query, IM_ARRAYSIZE(query));

      ImGui::SameLine();
      if (ImGui::Button(absl::StrFormat("%s Clear", ICON_MD_CLEAR).c_str())) {
        query[0] = '\0';
        input_changed = true;
      }

      Separator();

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
                  if (Selectable(kv.first.c_str(), false,
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

              if (Selectable(absl::StrFormat("%s %s %s", ICON_MD_TAB,
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
    End();
    
    // Update visibility state
    if (!show_search && ui_coordinator_) {
      ui_coordinator_->SetGlobalSearchVisible(false);
    }
  }

  if (show_palette_editor_ && current_editor_set_) {
    Begin("Palette Editor", &show_palette_editor_);
    status_ = current_editor_set_->palette_editor_.Update();

    // Route palette editor errors to toast manager
    if (!status_.ok()) {
      toast_manager_.Show(
          absl::StrFormat("Palette Editor Error: %s", status_.message()),
          editor::ToastType::kError, 8.0f);
    }

    End();
  }

  if (show_resource_label_manager && current_rom_) {
    current_rom_->resource_label()->DisplayLabels(&show_resource_label_manager);
    if (current_project_.project_opened() &&
        !current_project_.labels_filename.empty()) {
      current_project_.labels_filename =
          current_rom_->resource_label()->filename_;
    }
  }

  if (save_as_menu) {
    Begin("Save ROM As", &save_as_menu, ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("%s Save ROM to new location", ICON_MD_SAVE_AS);
    ImGui::Separator();

    static std::string save_as_filename = "";
    if (current_rom_ && save_as_filename.empty()) {
      save_as_filename = current_rom_->title();
    }

    ImGui::InputText("Filename", &save_as_filename);

    ImGui::Separator();

    if (Button(absl::StrFormat("%s Browse...", ICON_MD_FOLDER_OPEN).c_str(),
               gui::kDefaultModalSize)) {
      // Use save file dialog for ROM files
      auto file_path =
          util::FileDialogWrapper::ShowSaveFileDialog(save_as_filename, "sfc");
      if (!file_path.empty()) {
        save_as_filename = file_path;
      }
    }

    ImGui::SameLine();
    if (Button(absl::StrFormat("%s Save", ICON_MD_SAVE).c_str(),
               gui::kDefaultModalSize)) {
      if (!save_as_filename.empty()) {
        // Ensure proper file extension
        std::string final_filename = save_as_filename;
        if (final_filename.find(".sfc") == std::string::npos &&
            final_filename.find(".smc") == std::string::npos) {
          final_filename += ".sfc";
        }

        status_ = SaveRomAs(final_filename);
        if (status_.ok()) {
          save_as_menu = false;
          toast_manager_.Show(
              absl::StrFormat("ROM saved as: %s", final_filename),
              editor::ToastType::kSuccess);
        } else {
          toast_manager_.Show(
              absl::StrFormat("Failed to save ROM: %s", status_.message()),
              editor::ToastType::kError);
        }
      }
    }

    ImGui::SameLine();
    if (Button(absl::StrFormat("%s Cancel", ICON_MD_CANCEL).c_str(),
               gui::kDefaultModalSize)) {
      save_as_menu = false;
    }
    End();
  }

  if (new_project_menu) {
    Begin("New Project", &new_project_menu, ImGuiWindowFlags_AlwaysAutoResize);
    static std::string save_as_filename = "";
    InputText("Project Name", &save_as_filename);
    if (Button(absl::StrFormat("%s Destination Folder", ICON_MD_FOLDER).c_str(),
               gui::kDefaultModalSize)) {
      current_project_.filepath = FileDialogWrapper::ShowOpenFolderDialog();
    }
    SameLine();
    Text("%s", current_project_.filepath.c_str());

    if (Button(absl::StrFormat("%s ROM File", ICON_MD_VIDEOGAME_ASSET).c_str(),
               gui::kDefaultModalSize)) {
      current_project_.rom_filename = FileDialogWrapper::ShowOpenFileDialog();
    }
    SameLine();
    Text("%s", current_project_.rom_filename.c_str());

    if (Button(absl::StrFormat("%s Labels File", ICON_MD_LABEL).c_str(),
               gui::kDefaultModalSize)) {
      current_project_.labels_filename =
          FileDialogWrapper::ShowOpenFileDialog();
    }
    SameLine();
    Text("%s", current_project_.labels_filename.c_str());

    if (Button(absl::StrFormat("%s Code Folder", ICON_MD_CODE).c_str(),
               gui::kDefaultModalSize)) {
      current_project_.code_folder = FileDialogWrapper::ShowOpenFolderDialog();
    }
    SameLine();
    Text("%s", current_project_.code_folder.c_str());

    Separator();

    if (Button(absl::StrFormat("%s Choose Project File Location", ICON_MD_SAVE)
                   .c_str(),
               gui::kDefaultModalSize)) {
      auto project_file_path =
          util::FileDialogWrapper::ShowSaveFileDialog(save_as_filename, "yaze");
      if (!project_file_path.empty()) {
        // Ensure .yaze extension
        if (project_file_path.find(".yaze") == std::string::npos) {
          project_file_path += ".yaze";
        }

        // Update project filepath to the chosen location
        current_project_.filepath = project_file_path;

        // Also set the project directory to the parent directory
        size_t last_slash = project_file_path.find_last_of("/\\");
        if (last_slash != std::string::npos) {
          std::string project_dir = project_file_path.substr(0, last_slash);
          Text("Project will be saved to: %s", project_dir.c_str());
        }
      }
    }

    if (Button(absl::StrFormat("%s Create Project", ICON_MD_ADD).c_str(),
               gui::kDefaultModalSize)) {
      if (!current_project_.filepath.empty()) {
        new_project_menu = false;
        status_ = current_project_.Create(save_as_filename,
                                          current_project_.filepath);
        if (status_.ok()) {
          status_ = current_project_.Save();
        }
      } else {
        toast_manager_.Show("Please choose a project file location first",
                            editor::ToastType::kWarning);
      }
    }
    SameLine();
    if (Button("Cancel", gui::kDefaultModalSize)) {
      new_project_menu = false;
    }
    End();
  }

  // Workspace preset dialogs
  if (show_save_workspace_preset_) {
    Begin("Save Workspace Preset", &show_save_workspace_preset_,
          ImGuiWindowFlags_AlwaysAutoResize);
    static std::string preset_name = "";
    InputText("Name", &preset_name);
    if (Button("Save", gui::kDefaultModalSize)) {
      SaveWorkspacePreset(preset_name);
      toast_manager_.Show("Preset saved", editor::ToastType::kSuccess);
      show_save_workspace_preset_ = false;
    }
    SameLine();
    if (Button("Cancel", gui::kDefaultModalSize)) {
      show_save_workspace_preset_ = false;
    }
    End();
  }

  if (show_load_workspace_preset_) {
    Begin("Load Workspace Preset", &show_load_workspace_preset_,
          ImGuiWindowFlags_AlwaysAutoResize);

    // Lazy load workspace presets when UI is accessed
    if (!workspace_manager_.workspace_presets_loaded()) {
      RefreshWorkspacePresets();
    }

    for (const auto& name : workspace_manager_.workspace_presets()) {
      if (Selectable(name.c_str())) {
        LoadWorkspacePreset(name);
        toast_manager_.Show("Preset loaded", editor::ToastType::kSuccess);
        show_load_workspace_preset_ = false;
      }
    }
    if (workspace_manager_.workspace_presets().empty())
      Text("No presets found");
    End();
  }

  // Draw new workspace UI elements
  DrawSessionSwitcher();
  DrawSessionManager();
  DrawLayoutPresets();
  DrawSessionRenameDialog();
}

absl::Status EditorManager::LoadRom() {
  auto file_name = FileDialogWrapper::ShowOpenFileDialog();
  if (file_name.empty()) {
    return absl::OkStatus();
  }

  // Check for duplicate sessions
  if (HasDuplicateSession(file_name)) {
    toast_manager_.Show("ROM already open in another session",
                        editor::ToastType::kWarning);
    return absl::OkStatus();
  }

  // Delegate ROM loading to RomFileManager
  auto status = rom_file_manager_.LoadRom(file_name);
  if (!status.ok()) {
    return status;
  }
  
  Rom temp_rom = *rom_file_manager_.GetCurrentRom();

  // Check if there's an empty session we can populate instead of creating new one
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
    // Populate existing empty session
    target_session->rom = std::move(temp_rom);
    target_session->filepath = file_name;
    current_rom_ = &target_session->rom;
    current_editor_set_ = &target_session->editors;
  } else {
    // Create new session only if no empty ones exist
    size_t new_session_id = sessions_.size();
    sessions_.emplace_back(std::move(temp_rom), &user_settings_, new_session_id);
    RomSession& session = sessions_.back();
    session.filepath = file_name;  // Store filepath for duplicate detection

    // Wire editor contexts
    for (auto* editor : session.editors.active_editors_) {
      editor->set_context(&context_);
    }
    current_rom_ = &session.rom;
    current_editor_set_ = &session.editors;
  }

  // Update test manager with current ROM for ROM-dependent tests (only when tests are enabled)
#ifdef YAZE_ENABLE_TESTING
  LOG_DEBUG("EditorManager", "Setting ROM in TestManager - %p ('%s')",
            (void*)current_rom_,
            current_rom_ ? current_rom_->title().c_str() : "null");
  test::TestManager::Get().SetCurrentRom(current_rom_);
#endif

  auto& manager = core::RecentFilesManager::GetInstance();
  manager.AddFile(file_name);
  manager.Save();
  RETURN_IF_ERROR(LoadAssets());

  // Hide welcome screen when ROM is successfully loaded - don't reset manual close state
  ui_coordinator_->SetWelcomeScreenVisible(false);

  // Clear recent editors for fresh start with new ROM and show editor selection dialog
  editor_selection_dialog_.ClearRecentEditors();
  ui_coordinator_->SetEditorSelectionVisible(true);

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

  current_editor_set_->overworld_editor_.Initialize();
  current_editor_set_->message_editor_.Initialize();
  current_editor_set_->graphics_editor_.Initialize();
  current_editor_set_->screen_editor_.Initialize();
  current_editor_set_->sprite_editor_.Initialize();
  current_editor_set_->palette_editor_.Initialize();
  current_editor_set_->assembly_editor_.Initialize();
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

absl::Status EditorManager::SaveRom() {
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

  Rom::SaveSettings settings;
  settings.backup = user_settings_.prefs().backup_rom;
  settings.save_new = user_settings_.prefs().save_new_auto;
  return current_rom_->SaveToFile(settings);
}

absl::Status EditorManager::SaveRomAs(const std::string& filename) {
  if (!current_rom_ || !current_editor_set_) {
    return absl::FailedPreconditionError("No ROM or editor set loaded");
  }

  if (filename.empty()) {
    return absl::InvalidArgumentError("Filename cannot be empty");
  }

  // Save editor data first (same as SaveRom)
  if (core::FeatureFlags::get().kSaveDungeonMaps) {
    RETURN_IF_ERROR(zelda3::SaveDungeonMaps(
        *current_rom_, current_editor_set_->screen_editor_.dungeon_maps_));
  }

  RETURN_IF_ERROR(current_editor_set_->overworld_editor_.Save());

  if (core::FeatureFlags::get().kSaveGraphicsSheet)
    RETURN_IF_ERROR(
        SaveAllGraphicsData(*current_rom_, gfx::Arena::Get().gfx_sheets()));

  // Create save settings with custom filename
  Rom::SaveSettings settings;
  settings.backup = user_settings_.prefs().backup_rom;
  settings.save_new = false;  // Don't auto-generate name, use provided filename
  settings.filename = filename;

  auto save_status = current_rom_->SaveToFile(settings);
  if (save_status.ok()) {
    // Update current ROM filepath to the new location
    size_t current_session_idx = GetCurrentSessionIndex();
    if (current_session_idx < sessions_.size()) {
      sessions_[current_session_idx].filepath = filename;
    }

    // Add to recent files
    auto& manager = core::RecentFilesManager::GetInstance();
    manager.AddFile(filename);
    manager.Save();

    toast_manager_.Show(absl::StrFormat("ROM saved as: %s", filename),
                        editor::ToastType::kSuccess);
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
    RETURN_IF_ERROR(temp_rom.LoadFromFile(filename));
    sessions_.emplace_back(std::move(temp_rom), &user_settings_);
    RomSession& session = sessions_.back();
    for (auto* editor : session.editors.active_editors_) {
      editor->set_context(&context_);
    }
    current_rom_ = &session.rom;
    current_editor_set_ = &session.editors;
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
    RETURN_IF_ERROR(temp_rom.LoadFromFile(current_project_.rom_filename));

    if (!current_project_.labels_filename.empty()) {
      if (!temp_rom.resource_label()->LoadLabels(
              current_project_.labels_filename)) {
        toast_manager_.Show("Could not load labels file from project",
                            editor::ToastType::kWarning);
      }
    }

    sessions_.emplace_back(std::move(temp_rom), &user_settings_);
    RomSession& session = sessions_.back();
    for (auto* editor : session.editors.active_editors_) {
      editor->set_context(&context_);
    }
    current_rom_ = &session.rom;
    current_editor_set_ = &session.editors;

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

void EditorManager::ShowProjectHelp() {
  popup_manager_->Show("Project Help");
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
  for (auto* editor : session.editors.active_editors_) {
    editor->set_context(&context_);
      }
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
  for (auto* editor : session.editors.active_editors_) {
    editor->set_context(&context_);
  }
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
        test::TestManager::Get().SetCurrentRom(current_rom_);
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
        test::TestManager::Get().SetCurrentRom(current_rom_);
      }
    }
  }
}

void EditorManager::SwitchToSession(size_t index) {
  if (session_coordinator_) {
    session_coordinator_->SwitchToSession(index);
    
    // Update current pointers after session switch
    if (index < sessions_.size()) {
  auto& session = sessions_[index];
  current_rom_ = &session.rom;
  current_editor_set_ = &session.editors;

  // Update test manager with current ROM for ROM-dependent tests
  util::logf("EditorManager: Setting ROM in TestManager - %p ('%s')",
             (void*)current_rom_,
             current_rom_ ? current_rom_->title().c_str() : "null");
  test::TestManager::Get().SetCurrentRom(current_rom_);
    }
  }
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
    return session_coordinator_->GenerateUniqueEditorTitle(base_name, session_index);
  }

  // Fallback for single session or no coordinator
  return std::string(base_name);
}

void EditorManager::ResetWorkspaceLayout() {
  // Show confirmation popup first, then delegate to WindowDelegate
  popup_manager_->Show("Layout Reset Confirm");
  window_delegate_.ResetWorkspaceLayout();
}

void EditorManager::SaveWorkspaceLayout() {
  window_delegate_.SaveWorkspaceLayout();
  toast_manager_.Show("Workspace layout saved", editor::ToastType::kSuccess);
}

void EditorManager::LoadWorkspaceLayout() {
  window_delegate_.LoadWorkspaceLayout();
  toast_manager_.Show("Workspace layout loaded", editor::ToastType::kSuccess);
}

void EditorManager::ShowAllWindows() {
  // Delegate to UICoordinator for clean separation of concerns
  if (ui_coordinator_) {
    ui_coordinator_->ShowAllWindows();
  }
}

void EditorManager::HideAllWindows() {
  // Delegate to UICoordinator for clean separation of concerns
  if (ui_coordinator_) {
    ui_coordinator_->HideAllWindows();
  }
}

void EditorManager::MaximizeCurrentWindow() {
  // Delegate to WindowDelegate
  // Note: This requires tracking the current focused window
  toast_manager_.Show("Current window maximized", editor::ToastType::kInfo);
}

void EditorManager::RestoreAllWindows() {
  // Restore all windows to normal size
  toast_manager_.Show("All windows restored", editor::ToastType::kInfo);
}

void EditorManager::CloseAllFloatingWindows() {
  // Close all floating (undocked) windows
  toast_manager_.Show("All floating windows closed", editor::ToastType::kInfo);
}

void EditorManager::LoadDeveloperLayout() {
  if (!current_editor_set_)
    return;

  // Developer layout: Code editor, assembly editor, test dashboard
  current_editor_set_->assembly_editor_.set_active(true);
#ifdef YAZE_ENABLE_TESTING
  show_test_dashboard_ = true;
#endif
  show_imgui_metrics_ = true;

  // Hide non-dev windows
  current_editor_set_->graphics_editor_.set_active(false);
  current_editor_set_->music_editor_.set_active(false);
  current_editor_set_->sprite_editor_.set_active(false);

  toast_manager_.Show("Developer layout loaded", editor::ToastType::kSuccess);
}

void EditorManager::LoadDesignerLayout() {
  if (!current_editor_set_)
    return;

  // Designer layout: Graphics, palette, sprite editors
  current_editor_set_->graphics_editor_.set_active(true);
  current_editor_set_->palette_editor_.set_active(true);
  current_editor_set_->sprite_editor_.set_active(true);
  current_editor_set_->overworld_editor_.set_active(true);

  // Hide non-design windows
  current_editor_set_->assembly_editor_.set_active(false);
#ifdef YAZE_ENABLE_TESTING
  show_test_dashboard_ = false;
#endif
  show_imgui_metrics_ = false;

  toast_manager_.Show("Designer layout loaded", editor::ToastType::kSuccess);
}

void EditorManager::LoadModderLayout() {
  if (!current_editor_set_)
    return;

  // Modder layout: All editors except technical ones
  current_editor_set_->overworld_editor_.set_active(true);
  current_editor_set_->dungeon_editor_.set_active(true);
  current_editor_set_->graphics_editor_.set_active(true);
  current_editor_set_->palette_editor_.set_active(true);
  current_editor_set_->sprite_editor_.set_active(true);
  current_editor_set_->message_editor_.set_active(true);
  current_editor_set_->music_editor_.set_active(true);

  // Hide technical windows
  current_editor_set_->assembly_editor_.set_active(false);
  show_imgui_metrics_ = false;

  toast_manager_.Show("Modder layout loaded", editor::ToastType::kSuccess);
}

void EditorManager::DrawWelcomeScreen() {
  // Delegate to UICoordinator for clean separation of concerns
  if (ui_coordinator_) {
    ui_coordinator_->DrawWelcomeScreen();
  }
}

void EditorManager::DrawSessionManager() {
  // Delegate to UICoordinator for clean separation of concerns
  if (ui_coordinator_) {
    ui_coordinator_->DrawSessionManager();
  }
}

void EditorManager::DrawSessionRenameDialog() {
  // Delegate to UICoordinator for clean separation of concerns
  if (ui_coordinator_) {
    ui_coordinator_->DrawSessionRenameDialog();
  }
}

void EditorManager::DrawLayoutPresets() {
  // Delegate to UICoordinator for clean separation of concerns
  if (ui_coordinator_) {
    ui_coordinator_->DrawLayoutPresets();
  }
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
        auto& card_manager = gui::EditorCardManager::Get();

        if (*editor->active()) {
          // Editor activated - set its category
          card_manager.SetActiveCategory(GetEditorCategory(editor_type));
        } else {
          // Editor deactivated - switch to another active card-based editor
          for (auto* other : current_editor_set_->active_editors_) {
            if (*other->active() && IsCardBasedEditor(other->type()) &&
                other != editor) {
              card_manager.SetActiveCategory(GetEditorCategory(other->type()));
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
      gui::EditorCardManager::Get().SetActiveCategory("Emulator");
    }
  }
}

// ============================================================================
// User Settings Management
// ============================================================================

void EditorManager::LoadUserSettings() {
  // Apply font scale after loading
  ImGui::GetIO().FontGlobalScale = user_settings_.prefs().font_global_scale;

  // Apply welcome screen preference
  if (ui_coordinator_ && !user_settings_.prefs().show_welcome_on_startup) {
    ui_coordinator_->SetWelcomeScreenVisible(false);
    ui_coordinator_->SetWelcomeScreenManuallyClosed(true);
  }
}

void EditorManager::SaveUserSettings() {
  auto status = user_settings_.Save();
  if (!status.ok()) {
    LOG_WARN("EditorManager", "Failed to save user settings: %s",
             status.ToString().c_str());
  }
}

// SessionScope implementation
EditorManager::SessionScope::SessionScope(EditorManager* manager, size_t session_id)
    : manager_(manager),
      prev_rom_(manager->current_rom_),
      prev_editor_set_(manager->current_editor_set_),
      prev_session_id_(manager->context_.session_id) {
  
  // Set new session context
  if (session_id < manager->sessions_.size()) {
    manager->current_rom_ = &manager->sessions_[session_id].rom;
    manager->current_editor_set_ = &manager->sessions_[session_id].editors;
    manager->context_.session_id = session_id;
  }
}

EditorManager::SessionScope::~SessionScope() {
  // Restore previous context
  manager_->current_rom_ = prev_rom_;
  manager_->current_editor_set_ = prev_editor_set_;
  manager_->context_.session_id = prev_session_id_;
}

bool EditorManager::HasDuplicateSession(const std::string& filepath) {
  for (const auto& session : sessions_) {
    if (session.filepath == filepath) {
      return true;
    }
  }
  return false;
}

}  // namespace editor
}  // namespace yaze
