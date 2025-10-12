#include "editor_manager.h"

#include <algorithm>
#include <chrono>
#include <cstring>

#define IMGUI_DEFINE_MATH_OPERATORS

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "app/core/features.h"
#include "app/core/timing.h"
#include "util/file_util.h"
#include "app/gui/widgets/widget_id_registry.h"
#include "imgui/imgui.h"
#include "util/log.h"
#include "util/platform_paths.h"
#include "app/core/project.h"
#include "app/editor/code/assembly_editor.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/graphics/graphics_editor.h"
#include "app/editor/graphics/palette_editor.h"
#include "app/editor/graphics/screen_editor.h"
#include "app/editor/music/music_editor.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/sprite/sprite_editor.h"
#include "app/editor/ui/editor_selection_dialog.h"
#include "app/emu/emulator.h"
#include "app/gfx/arena.h"
#include "app/gfx/performance/performance_profiler.h"
#include "app/gui/background_renderer.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/gui/style.h"
#include "app/gui/theme_manager.h"
#include "app/rom.h"
#include "app/test/test_manager.h"
#include "app/zelda3/overworld/overworld_map.h"
#ifdef YAZE_ENABLE_TESTING
#include "app/test/e2e_test_suite.h"
#include "app/test/integrated_test_suite.h"
#include "app/test/rom_dependent_test_suite.h"
#include "app/test/zscustomoverworld_test_suite.h"
#endif
#ifdef YAZE_ENABLE_GTEST
#include "app/test/unit_test_suite.h"
#endif

#include "app/editor/system/settings_editor.h"
#include "app/editor/system/toast_manager.h"
#include "app/emu/emulator.h"
#include "app/gfx/performance/performance_dashboard.h"
#include "app/editor/editor.h"

#ifdef YAZE_WITH_GRPC
#include "app/core/service/screenshot_utils.h"
#include "app/editor/agent/agent_chat_widget.h"
#include "cli/service/agent/agent_control_server.h"
#include "cli/service/agent/conversational_agent_service.h"
#include "cli/service/ai/gemini_ai_service.h"
#include "app/test/z3ed_test_suite.h"

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

EditorManager::EditorManager() : blank_editor_set_(nullptr, &user_settings_) {
  std::stringstream ss;
  ss << YAZE_VERSION_MAJOR << "." << YAZE_VERSION_MINOR << "."
     << YAZE_VERSION_PATCH;
  ss >> version_;
  context_.popup_manager = popup_manager_.get();
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

void EditorManager::Initialize(gfx::IRenderer* renderer, const std::string& filename) {
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

  // Initialize project file editor
  project_file_editor_.SetToastManager(&toast_manager_);

#ifdef YAZE_WITH_GRPC
  // Initialize the agent editor as a proper Editor (configuration dashboard)
  agent_editor_.set_context(&context_);
  agent_editor_.Initialize();
  agent_editor_.InitializeWithDependencies(&toast_manager_, &proposal_drawer_, nullptr);

  // Initialize and connect the chat history popup
  agent_chat_history_popup_.SetToastManager(&toast_manager_);
  if (agent_editor_.GetChatWidget()) {
    agent_editor_.GetChatWidget()->SetChatHistoryPopup(&agent_chat_history_popup_);
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
        const char* window_name = agent_editor_.GetChatWidget()->specific_window_name();
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
  
  z3ed_callbacks.accept_proposal = [this](const std::string& proposal_id) -> absl::Status {
    // Use ProposalDrawer's existing logic
    proposal_drawer_.Show();
    proposal_drawer_.FocusProposal(proposal_id);
    
    toast_manager_.Show(
        absl::StrFormat("%s View proposal %s in drawer to accept", ICON_MD_PREVIEW, proposal_id),
        ToastType::kInfo, 3.5f);
    
    return absl::OkStatus();
  };
  
  z3ed_callbacks.reject_proposal = [this](const std::string& proposal_id) -> absl::Status {
    // Use ProposalDrawer's existing logic
    proposal_drawer_.Show();
    proposal_drawer_.FocusProposal(proposal_id);
    
    toast_manager_.Show(
        absl::StrFormat("%s View proposal %s in drawer to reject", ICON_MD_PREVIEW, proposal_id),
        ToastType::kInfo, 3.0f);
    
    return absl::OkStatus();
  };
  
  z3ed_callbacks.list_proposals = []() -> absl::StatusOr<std::vector<std::string>> {
    // Return empty for now - ProposalDrawer handles the real list
    return std::vector<std::string>{};
  };
  
  z3ed_callbacks.diff_proposal = [this](const std::string& proposal_id) -> absl::StatusOr<std::string> {
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
    LOG_WARN("EditorManager", "Failed to load user settings: %s", status_.ToString().c_str());
  }

  // Initialize welcome screen callbacks
  welcome_screen_.SetOpenRomCallback([this]() {
    status_ = LoadRom();
    // LoadRom() already handles closing welcome screen and showing editor selection
  });
  
  welcome_screen_.SetNewProjectCallback([this]() {
    status_ = CreateNewProject();
    if (status_.ok()) {
      show_welcome_screen_ = false;
      welcome_screen_manually_closed_ = true;
    }
  });
  
  welcome_screen_.SetOpenProjectCallback([this](const std::string& filepath) {
    status_ = OpenRomOrProject(filepath);
    if (status_.ok()) {
      show_welcome_screen_ = false;
      welcome_screen_manually_closed_ = true;
    }
  });
  
  // Initialize editor selection dialog callback
  editor_selection_dialog_.SetSelectionCallback([this](EditorType type) {
    editor_selection_dialog_.MarkRecentlyUsed(type);
    
    // Handle agent editor separately (doesn't require ROM)
    if (type == EditorType::kAgent) {
#ifdef YAZE_WITH_GRPC
      agent_editor_.set_active(true);
#endif
      return;
    }
    
    if (!current_editor_set_) return;
    
    switch (type) {
      case EditorType::kOverworld:
        current_editor_set_->overworld_editor_.set_active(true);
        break;
      case EditorType::kDungeon:
        current_editor_set_->dungeon_editor_.set_active(true);
        break;
      case EditorType::kGraphics:
        current_editor_set_->graphics_editor_.set_active(true);
        break;
      case EditorType::kSprite:
        current_editor_set_->sprite_editor_.set_active(true);
        break;
      case EditorType::kMessage:
        current_editor_set_->message_editor_.set_active(true);
        break;
      case EditorType::kMusic:
        current_editor_set_->music_editor_.set_active(true);
        break;
      case EditorType::kPalette:
        current_editor_set_->palette_editor_.set_active(true);
        break;
      case EditorType::kScreen:
        current_editor_set_->screen_editor_.set_active(true);
        break;
      case EditorType::kAssembly:
        show_asm_editor_ = true;
        break;
      case EditorType::kEmulator:
        show_emulator_ = true;
        break;
      case EditorType::kSettings:
        current_editor_set_->settings_editor_.set_active(true);
        break;
      default:
        break;
    }
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
      [this]() { show_command_palette_ = true; });
  context_.shortcut_manager.RegisterShortcut(
      "Global Search", {ImGuiKey_K, ImGuiMod_Ctrl, ImGuiMod_Shift},
      [this]() { show_global_search_ = true; });

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

  // Editor shortcuts (Ctrl+1-9, Ctrl+0)
  context_.shortcut_manager.RegisterShortcut(
      "Overworld Editor", {ImGuiKey_1, ImGuiMod_Ctrl},
      [this]() { if (current_editor_set_) current_editor_set_->overworld_editor_.set_active(true); });
  context_.shortcut_manager.RegisterShortcut(
      "Dungeon Editor", {ImGuiKey_2, ImGuiMod_Ctrl},
      [this]() { if (current_editor_set_) current_editor_set_->dungeon_editor_.set_active(true); });
  context_.shortcut_manager.RegisterShortcut(
      "Graphics Editor", {ImGuiKey_3, ImGuiMod_Ctrl},
      [this]() { if (current_editor_set_) current_editor_set_->graphics_editor_.set_active(true); });
  context_.shortcut_manager.RegisterShortcut(
      "Sprite Editor", {ImGuiKey_4, ImGuiMod_Ctrl},
      [this]() { if (current_editor_set_) current_editor_set_->sprite_editor_.set_active(true); });
  context_.shortcut_manager.RegisterShortcut(
      "Message Editor", {ImGuiKey_5, ImGuiMod_Ctrl},
      [this]() { if (current_editor_set_) current_editor_set_->message_editor_.set_active(true); });
  context_.shortcut_manager.RegisterShortcut(
      "Music Editor", {ImGuiKey_6, ImGuiMod_Ctrl},
      [this]() { if (current_editor_set_) current_editor_set_->music_editor_.set_active(true); });
  context_.shortcut_manager.RegisterShortcut(
      "Palette Editor", {ImGuiKey_7, ImGuiMod_Ctrl},
      [this]() { if (current_editor_set_) current_editor_set_->palette_editor_.set_active(true); });
  context_.shortcut_manager.RegisterShortcut(
      "Screen Editor", {ImGuiKey_8, ImGuiMod_Ctrl},
      [this]() { if (current_editor_set_) current_editor_set_->screen_editor_.set_active(true); });
  context_.shortcut_manager.RegisterShortcut(
      "Assembly Editor", {ImGuiKey_9, ImGuiMod_Ctrl},
      [this]() { show_asm_editor_ = true; });
  context_.shortcut_manager.RegisterShortcut(
      "Settings Editor", {ImGuiKey_0, ImGuiMod_Ctrl},
      [this]() { if (current_editor_set_) current_editor_set_->settings_editor_.set_active(true); });
  
  // Editor Selection Dialog shortcut
  context_.shortcut_manager.RegisterShortcut(
      "Editor Selection", {ImGuiKey_E, ImGuiMod_Ctrl},
      [this]() { show_editor_selection_ = true; });
  
  // Card Browser shortcut
  context_.shortcut_manager.RegisterShortcut(
      "Card Browser", {ImGuiKey_B, ImGuiMod_Ctrl, ImGuiMod_Shift},
      [this]() { show_card_browser_ = true; });
  
  // === SIMPLIFIED CARD SHORTCUTS - Use Card Browser instead of individual shortcuts ===
  // Individual card shortcuts removed to prevent hash table overflow
  // Users can:
  // 1. Use Card Browser (Ctrl+Shift+B) to toggle any card
  // 2. Use compact card control button in menu bar
  // 3. Use View menu for category-based toggles
  
  // Only register essential category-level shortcuts
  context_.shortcut_manager.RegisterShortcut(
      "Show All Dungeon Cards", {ImGuiKey_D, ImGuiMod_Ctrl, ImGuiMod_Shift},
      []() { gui::EditorCardManager::Get().ShowAllCardsInCategory("Dungeon"); });
  context_.shortcut_manager.RegisterShortcut(
      "Show All Graphics Cards", {ImGuiKey_G, ImGuiMod_Ctrl, ImGuiMod_Shift},
      []() { gui::EditorCardManager::Get().ShowAllCardsInCategory("Graphics"); });
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
      [this]() { show_session_switcher_ = true; });
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

void EditorManager::OpenEditorAndCardsFromFlags(
    const std::string& editor_name, const std::string& cards_str) {
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
    auto* editor = current_editor_set_->active_editors_[static_cast<int>(editor_type_to_open)];
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
  
  // Draw editor selection dialog
  if (show_editor_selection_) {
    editor_selection_dialog_.Show(&show_editor_selection_);
  }
  
  // Draw card browser
  if (show_card_browser_) {
    gui::EditorCardManager::Get().DrawCardBrowser(&show_card_browser_);
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
    LOG_DEBUG("EditorManager",
             "EditorManager::Update - ROM changed, updating TestManager: %p -> "
             "%p",
             (void*)last_test_rom, (void*)current_rom_);
    test::TestManager::Get().SetCurrentRom(current_rom_);
    last_test_rom = current_rom_;
  }

  // Autosave timer
  if (user_settings_.prefs().autosave_enabled && current_rom_ && current_rom_->dirty()) {
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
    // Show welcome screen when no session is active, but only if not manually closed
    if (sessions_.empty() && !welcome_screen_manually_closed_) {
      show_welcome_screen_ = true;
    }
    // Don't auto-show here, let the manual control handle it
    return absl::OkStatus();
  }

  // Check if current ROM is valid
  if (!current_rom_) {
    // Only show welcome screen for truly empty state, not when ROM is loaded but current_rom_ is null
    if (sessions_.empty() && !welcome_screen_manually_closed_) {
      show_welcome_screen_ = true;
    }
    return absl::OkStatus();
  }

  // ROM is loaded and valid - don't auto-show welcome screen
  // Welcome screen should only be shown manually at this point

  // Iterate through ALL sessions to support multi-session docking
  for (size_t session_idx = 0; session_idx < sessions_.size(); ++session_idx) {
    auto& session = sessions_[session_idx];
    if (!session.rom.is_loaded())
      continue;  // Skip sessions with invalid ROMs

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
        bool is_card_based_editor = (editor->type() == EditorType::kDungeon);
        // TODO: Add EditorType::kGraphics, EditorType::kPalette when converted
        
        if (is_card_based_editor) {
          // Card-based editors create their own top-level windows
          // No parent wrapper needed - this allows independent docking
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
            toast_manager_.Show(
                absl::StrFormat("%s Error: %s", editor_name, status_.message()),
                editor::ToastType::kError, 8.0f);
          }

          // Restore context
          current_rom_ = prev_rom;
          current_editor_set_ = prev_editor_set;
          context_.session_id = prev_session_id;
          
        } else {
          // TRADITIONAL EDITORS: Wrap in Begin/End
          std::string window_title =
              GenerateUniqueEditorTitle(editor->type(), session_idx);

          // Set window to maximize on first open
          ImGui::SetNextWindowSize(ImGui::GetMainViewport()->WorkSize, ImGuiCond_FirstUseEver);
          ImGui::SetNextWindowPos(ImGui::GetMainViewport()->WorkPos, ImGuiCond_FirstUseEver);
          
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
              toast_manager_.Show(
                  absl::StrFormat("%s Error: %s", editor_name, status_.message()),
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

  if (show_performance_dashboard_) {
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
  if (!current_editor_set_ || !current_editor_) {
    return;
  }
  
  // Determine which category to show based on active editor
  std::string category;
  
  switch (current_editor_->type()) {
    case EditorType::kDungeon:
      category = "Dungeon";
      break;
    case EditorType::kGraphics:
      category = "Graphics";
      break;
    case EditorType::kScreen:
      category = "Screen";
      break;
    case EditorType::kSprite:
      category = "Sprite";
      break;
    case EditorType::kOverworld:
      category = "Overworld";
      break;
    case EditorType::kMessage:
      category = "Message";
      break;
    case EditorType::kPalette:
      // Palette editor doesn't use cards (uses internal tabs)
      return;
    case EditorType::kAssembly:
      // Assembly editor uses dynamic file tabs
      return;
    case EditorType::kEmulator:
      category = "Emulator";
      break;
    case EditorType::kMusic:
      // Music editor doesn't use cards yet
      return;
    default:
      return;  // No cards for this editor type
  }
  
  // Draw compact card control for the active editor's cards
  auto& card_manager = gui::EditorCardManager::Get();
  card_manager.DrawCompactCardControl(category);
  
  // Show visible/total count
  SameLine();
  card_manager.DrawInlineCardToggles(category);
}

void EditorManager::BuildModernMenu() {
  menu_builder_.Clear();
  
  // File Menu - enhanced with ROM features
  menu_builder_.BeginMenu("File")
    .Item("Open ROM", ICON_MD_FILE_OPEN, 
          [this]() { status_ = LoadRom(); }, "Ctrl+O")
    .Item("Save ROM", ICON_MD_SAVE,
          [this]() { status_ = SaveRom(); }, "Ctrl+S",
          [this]() { return current_rom_ && current_rom_->is_loaded(); })
    .Item("Save As...", ICON_MD_SAVE_AS,
          [this]() { popup_manager_->Show("Save As.."); },
          nullptr,
          [this]() { return current_rom_ && current_rom_->is_loaded(); })
    .Separator()
    .Item("New Project", ICON_MD_CREATE_NEW_FOLDER,
          [this]() { status_ = CreateNewProject(); })
    .Item("Open Project", ICON_MD_FOLDER_OPEN,
          [this]() { status_ = OpenProject(); })
    .Item("Save Project", ICON_MD_SAVE,
          [this]() { status_ = SaveProject(); },
          nullptr,
          [this]() { return !current_project_.filepath.empty(); })
    .Item("Save Project As...", ICON_MD_SAVE_AS,
          [this]() { status_ = SaveProjectAs(); },
          nullptr,
          [this]() { return !current_project_.filepath.empty(); })
    .Separator()
    .Item("ROM Information", ICON_MD_INFO,
          [this]() { popup_manager_->Show("ROM Info"); },
          nullptr,
          [this]() { return current_rom_ && current_rom_->is_loaded(); })
    .Item("Create Backup", ICON_MD_BACKUP,
          [this]() { 
            if (current_rom_ && current_rom_->is_loaded()) {
              Rom::SaveSettings settings;
              settings.backup = true;
              settings.filename = current_rom_->filename();
              status_ = current_rom_->SaveToFile(settings);
              if (status_.ok()) {
                toast_manager_.Show("Backup created successfully", ToastType::kSuccess);
              }
            }
          },
          nullptr,
          [this]() { return current_rom_ && current_rom_->is_loaded(); })
    .Item("Validate ROM", ICON_MD_CHECK_CIRCLE,
          [this]() {
            if (current_rom_ && current_rom_->is_loaded()) {
              auto result = current_project_.Validate();
              if (result.ok()) {
                toast_manager_.Show("ROM validation passed", ToastType::kSuccess);
              } else {
                toast_manager_.Show("ROM validation failed: " + std::string(result.message()), 
                    ToastType::kError);
              }
            }
          },
          nullptr,
          [this]() { return current_rom_ && current_rom_->is_loaded(); })
    .Separator()
    .Item("Settings", ICON_MD_SETTINGS,
          [this]() { current_editor_set_->settings_editor_.set_active(true); })
    .Separator()
    .Item("Quit", ICON_MD_EXIT_TO_APP,
          [this]() { quit_ = true; }, "Ctrl+Q")
    .EndMenu();
  
  // Edit Menu  
  menu_builder_.BeginMenu("Edit")
    .Item("Undo", ICON_MD_UNDO,
          [this]() { if (current_editor_) status_ = current_editor_->Undo(); }, "Ctrl+Z")
    .Item("Redo", ICON_MD_REDO,
          [this]() { if (current_editor_) status_ = current_editor_->Redo(); }, "Ctrl+Y")
    .Separator()
    .Item("Cut", ICON_MD_CONTENT_CUT,
          [this]() { if (current_editor_) status_ = current_editor_->Cut(); }, "Ctrl+X")
    .Item("Copy", ICON_MD_CONTENT_COPY,
          [this]() { if (current_editor_) status_ = current_editor_->Copy(); }, "Ctrl+C")
    .Item("Paste", ICON_MD_CONTENT_PASTE,
          [this]() { if (current_editor_) status_ = current_editor_->Paste(); }, "Ctrl+V")
    .Separator()
    .Item("Find", ICON_MD_SEARCH,
          [this]() { if (current_editor_) status_ = current_editor_->Find(); }, "Ctrl+F")
    .Item("Find in Files", ICON_MD_SEARCH,
          [this]() { show_global_search_ = true; }, "Ctrl+Shift+F")
    .EndMenu();
  
  // View Menu - editors and cards
  menu_builder_.BeginMenu("View")
    .Item("Editor Selection", ICON_MD_DASHBOARD,
          [this]() { show_editor_selection_ = true; }, "Ctrl+E")
    .Separator()
    .Item("Overworld", ICON_MD_MAP,
          [this]() { current_editor_set_->overworld_editor_.set_active(true); }, "Ctrl+1")
    .Item("Dungeon", ICON_MD_CASTLE,
          [this]() { current_editor_set_->dungeon_editor_.set_active(true); }, "Ctrl+2")
    .Item("Graphics", ICON_MD_IMAGE,
          [this]() { current_editor_set_->graphics_editor_.set_active(true); }, "Ctrl+3")
    .Item("Sprites", ICON_MD_TOYS,
          [this]() { current_editor_set_->sprite_editor_.set_active(true); }, "Ctrl+4")
    .Item("Messages", ICON_MD_CHAT_BUBBLE,
          [this]() { current_editor_set_->message_editor_.set_active(true); }, "Ctrl+5")
    .Item("Music", ICON_MD_MUSIC_NOTE,
          [this]() { current_editor_set_->music_editor_.set_active(true); }, "Ctrl+6")
    .Item("Palettes", ICON_MD_PALETTE,
          [this]() { current_editor_set_->palette_editor_.set_active(true); }, "Ctrl+7")
    .Item("Screens", ICON_MD_TV,
          [this]() { current_editor_set_->screen_editor_.set_active(true); }, "Ctrl+8")
    .Item("Hex Editor", ICON_MD_DATA_ARRAY,
          [this]() { show_memory_editor_ = true; }, "Ctrl+0")
#ifdef YAZE_WITH_GRPC
    .Item("AI Agent", ICON_MD_SMART_TOY,
          [this]() { agent_editor_.set_active(true); }, "Ctrl+Shift+A")
    .Item("Chat History", ICON_MD_CHAT,
          [this]() { agent_chat_history_popup_.Toggle(); }, "Ctrl+H")
    .Item("Proposal Drawer", ICON_MD_PREVIEW,
          [this]() { proposal_drawer_.Toggle(); }, "Ctrl+P")
#endif
    .Separator();
  
  // // Dynamic card menu sections (from EditorCardManager)
  // auto& card_manager = gui::EditorCardManager::Get();
  // card_manager.DrawViewMenuAll();
  
  menu_builder_
    .Separator()
    .Item("Card Browser", ICON_MD_DASHBOARD,
          [this]() { show_card_browser_ = true; }, "Ctrl+Shift+B")
    .Separator()
    .Item("Welcome Screen", ICON_MD_HOME,
          [this]() { show_welcome_screen_ = true; })
    .Item("Command Palette", ICON_MD_TERMINAL,
          [this]() { show_command_palette_ = true; }, "Ctrl+Shift+P")
    .Item("Emulator", ICON_MD_GAMEPAD,
          [this]() { show_emulator_ = true; },
          nullptr, nullptr,
          [this]() { return show_emulator_; })
    .EndMenu();
  
  // Window Menu - layout and session management
  menu_builder_.BeginMenu("Window")
    .BeginSubMenu("Sessions", ICON_MD_TAB)
      .Item("New Session", ICON_MD_ADD,
            [this]() { CreateNewSession(); }, "Ctrl+Shift+N")
      .Item("Duplicate Session", ICON_MD_CONTENT_COPY,
            [this]() { DuplicateCurrentSession(); },
            nullptr, [this]() { return current_rom_ != nullptr; })
      .Item("Close Session", ICON_MD_CLOSE,
            [this]() { CloseCurrentSession(); }, "Ctrl+Shift+W",
            [this]() { return sessions_.size() > 1; })
      .Separator()
      .Item("Session Switcher", ICON_MD_SWITCH_ACCOUNT,
            [this]() { show_session_switcher_ = true; }, "Ctrl+Tab",
            [this]() { return sessions_.size() > 1; })
      .Item("Session Manager", ICON_MD_VIEW_LIST,
            [this]() { show_session_manager_ = true; })
      .EndMenu()
    .Separator()
    .Item("Save Layout", ICON_MD_SAVE,
          [this]() { SaveWorkspaceLayout(); }, "Ctrl+Shift+S")
    .Item("Load Layout", ICON_MD_FOLDER_OPEN,
          [this]() { LoadWorkspaceLayout(); }, "Ctrl+Shift+O")
    .Item("Reset Layout", ICON_MD_RESET_TV,
          [this]() { ResetWorkspaceLayout(); })
    .Item("Layout Presets", ICON_MD_BOOKMARK,
          [this]() { show_layout_presets_ = true; })
    .Separator()
    .Item("Show All Windows", ICON_MD_VISIBILITY,
          [this]() { ShowAllWindows(); })
    .Item("Hide All Windows", ICON_MD_VISIBILITY_OFF,
          [this]() { HideAllWindows(); })
    .Item("Maximize Current", ICON_MD_FULLSCREEN,
          [this]() { MaximizeCurrentWindow(); }, "F11")
    .Item("Restore All", ICON_MD_FULLSCREEN_EXIT,
          [this]() { RestoreAllWindows(); })
    .Item("Close All Floating", ICON_MD_CLOSE_FULLSCREEN,
          [this]() { CloseAllFloatingWindows(); })
    .EndMenu();
  
  
#ifdef YAZE_WITH_GRPC
  // Collaboration Menu - combined Agent + Network features
  menu_builder_.BeginMenu("Collaborate")
    .Item("AI Agent Chat", ICON_MD_SMART_TOY,
          [this]() { agent_editor_.SetChatActive(true); }, "Ctrl+Shift+A",
          nullptr,
          [this]() { return agent_editor_.IsChatActive(); })
    .Item("Proposal Drawer", ICON_MD_RATE_REVIEW,
          [this]() { show_proposal_drawer_ = !show_proposal_drawer_; },
          nullptr, nullptr,
          [this]() { return show_proposal_drawer_; })
    .Separator()
    .Item("Host Session", ICON_MD_ADD_CIRCLE,
          [this]() {
            auto result = agent_editor_.HostSession("New Session");
            if (result.ok()) {
              toast_manager_.Show("Hosted session: " + result->session_name, 
                  ToastType::kSuccess);
            } else {
              toast_manager_.Show("Failed to host session: " + std::string(result.status().message()),
                  ToastType::kError);
            }
          })
    .Item("Join Session", ICON_MD_LOGIN,
          [this]() { popup_manager_->Show("Join Collaboration Session"); })
    .Item("Leave Session", ICON_MD_LOGOUT,
          [this]() {
            status_ = agent_editor_.LeaveSession();
            if (status_.ok()) {
              toast_manager_.Show("Left collaboration session", ToastType::kInfo);
            }
          },
          nullptr,
          [this]() { return agent_editor_.IsInSession(); })
    .Item("Refresh Session", ICON_MD_REFRESH,
          [this]() {
            auto result = agent_editor_.RefreshSession();
            if (result.ok()) {
              toast_manager_.Show("Session refreshed: " + std::to_string(result->participants.size()) + " participants", 
                  ToastType::kSuccess);
            }
          },
          nullptr,
          [this]() { return agent_editor_.IsInSession(); })
    .Separator()
    .Item("Connect to Server", ICON_MD_CLOUD_UPLOAD,
          [this]() { popup_manager_->Show("Connect to Server"); })
    .Item("Disconnect from Server", ICON_MD_CLOUD_OFF,
          [this]() {
            agent_editor_.DisconnectFromServer();
            toast_manager_.Show("Disconnected from server", ToastType::kInfo);
          },
          nullptr,
          [this]() { return agent_editor_.IsConnectedToServer(); })
    .Separator()
    .Item("Capture Active Editor", ICON_MD_SCREENSHOT,
          [this]() { 
            std::filesystem::path output;
            AgentEditor::CaptureConfig config;
            config.mode = AgentEditor::CaptureConfig::CaptureMode::kActiveEditor;
            status_ = agent_editor_.CaptureSnapshot(&output, config);
          })
    .Item("Capture Full Window", ICON_MD_FULLSCREEN,
          [this]() {
            std::filesystem::path output;
            AgentEditor::CaptureConfig config;
            config.mode = AgentEditor::CaptureConfig::CaptureMode::kFullWindow;
            status_ = agent_editor_.CaptureSnapshot(&output, config);
          })
    .Separator()
    .Item("Local Mode", ICON_MD_FOLDER,
          [this]() { /* Set local mode */ },
          nullptr, nullptr,
          [this]() { return agent_editor_.GetCurrentMode() == AgentEditor::CollaborationMode::kLocal; })
    .Item("Network Mode", ICON_MD_WIFI,
          [this]() { /* Set network mode */ },
          nullptr, nullptr,
          [this]() { return agent_editor_.GetCurrentMode() == AgentEditor::CollaborationMode::kNetwork; })
    .EndMenu();
#endif
  
  // Debug Menu - comprehensive development tools
  menu_builder_.BeginMenu("Debug");
  
#ifdef YAZE_ENABLE_TESTING
  // Testing and Validation section
  menu_builder_
    .Item("Test Dashboard", ICON_MD_SCIENCE,
          [this]() { show_test_dashboard_ = true; }, "Ctrl+T")
    .Item("Run All Tests", ICON_MD_PLAY_ARROW,
          [this]() { [[maybe_unused]] auto status = test::TestManager::Get().RunAllTests(); })
    .Item("Run Unit Tests", ICON_MD_INTEGRATION_INSTRUCTIONS,
          [this]() { [[maybe_unused]] auto status = test::TestManager::Get().RunTestsByCategory(test::TestCategory::kUnit); })
    .Item("Run Integration Tests", ICON_MD_MEMORY,
          [this]() { [[maybe_unused]] auto status = test::TestManager::Get().RunTestsByCategory(test::TestCategory::kIntegration); })
    .Item("Run UI Tests", ICON_MD_VISIBILITY,
          [this]() { [[maybe_unused]] auto status = test::TestManager::Get().RunTestsByCategory(test::TestCategory::kUI); })
    .Item("Run Performance Tests", ICON_MD_SPEED,
          [this]() { [[maybe_unused]] auto status = test::TestManager::Get().RunTestsByCategory(test::TestCategory::kPerformance); })
    .Item("Run Memory Tests", ICON_MD_STORAGE,
          [this]() { [[maybe_unused]] auto status = test::TestManager::Get().RunTestsByCategory(test::TestCategory::kMemory); })
    .Item("Clear Test Results", ICON_MD_CLEAR_ALL,
          [this]() { test::TestManager::Get().ClearResults(); })
    .Separator();
#endif
  
  // ROM and ASM Management
  menu_builder_
    .BeginSubMenu("ROM Analysis", ICON_MD_STORAGE)
      .Item("ROM Information", ICON_MD_INFO,
            [this]() { popup_manager_->Show("ROM Information"); },
            nullptr,
            [this]() { return current_rom_ && current_rom_->is_loaded(); })
#ifdef YAZE_ENABLE_TESTING
      .Item("Data Integrity Check", ICON_MD_ANALYTICS,
            [this]() { 
              if (current_rom_) {
                [[maybe_unused]] auto status = test::TestManager::Get().RunTestSuite("RomIntegrity");
                toast_manager_.Show("Running ROM integrity tests...", ToastType::kInfo);
              }
            },
            nullptr,
            [this]() { return current_rom_ && current_rom_->is_loaded(); })
      .Item("Test Save/Load", ICON_MD_SAVE_ALT,
            [this]() { 
              if (current_rom_) {
                [[maybe_unused]] auto status = test::TestManager::Get().RunTestSuite("RomSaveLoad");
                toast_manager_.Show("Running ROM save/load tests...", ToastType::kInfo);
              }
            },
            nullptr,
            [this]() { return current_rom_ && current_rom_->is_loaded(); })
#endif
      .EndMenu()
    .BeginSubMenu("ZSCustomOverworld", ICON_MD_CODE)
      .Item("Check ROM Version", ICON_MD_INFO,
            [this]() { 
              if (current_rom_) {
                uint8_t version = (*current_rom_)[zelda3::OverworldCustomASMHasBeenApplied];
                std::string version_str = (version == 0xFF) ? "Vanilla" : absl::StrFormat("v%d", version);
                toast_manager_.Show(absl::StrFormat("ROM: %s | ZSCustomOverworld: %s", 
                                                   current_rom_->title().c_str(), version_str.c_str()), 
                                   ToastType::kInfo, 5.0f);
              }
            },
            nullptr,
            [this]() { return current_rom_ && current_rom_->is_loaded(); })
      .Item("Upgrade ROM", ICON_MD_UPGRADE,
            [this]() { 
              if (current_rom_) {
                toast_manager_.Show("Use Overworld Editor to upgrade ROM version", 
                                   ToastType::kInfo, 4.0f);
              }
            },
            nullptr,
            [this]() { return current_rom_ && current_rom_->is_loaded(); })
      .Item("Toggle Custom Loading", ICON_MD_SETTINGS,
            [this]() { 
              auto& flags = core::FeatureFlags::get();
              flags.overworld.kLoadCustomOverworld = !flags.overworld.kLoadCustomOverworld;
              toast_manager_.Show(absl::StrFormat("Custom Overworld Loading: %s", 
                                                 flags.overworld.kLoadCustomOverworld ? "Enabled" : "Disabled"), 
                                 ToastType::kInfo);
            })
      .EndMenu()
    .BeginSubMenu("Asar Integration", ICON_MD_BUILD)
      .Item("Asar Status", ICON_MD_INFO,
            [this]() { popup_manager_->Show("Asar Integration"); })
      .Item("Toggle ASM Patch", ICON_MD_CODE,
            [this]() { 
              if (current_rom_) {
                auto& flags = core::FeatureFlags::get();
                flags.overworld.kApplyZSCustomOverworldASM = !flags.overworld.kApplyZSCustomOverworldASM;
                toast_manager_.Show(absl::StrFormat("ZSCustomOverworld ASM Application: %s", 
                                                   flags.overworld.kApplyZSCustomOverworldASM ? "Enabled" : "Disabled"), 
                                   ToastType::kInfo);
              }
            },
            nullptr,
            [this]() { return current_rom_ && current_rom_->is_loaded(); })
      .Item("Load ASM File", ICON_MD_FOLDER_OPEN,
            [this]() { 
              toast_manager_.Show("ASM file loading not yet implemented", 
                                 ToastType::kWarning);
            })
      .EndMenu()
    .Separator()
    // Development Tools
    .Item("Memory Editor", ICON_MD_MEMORY,
          [this]() { show_memory_editor_ = true; })
    .Item("Assembly Editor", ICON_MD_CODE,
          [this]() { show_asm_editor_ = true; })
    .Item("Feature Flags", ICON_MD_FLAG,
          [this]() { popup_manager_->Show("Feature Flags"); })
    .Separator()
    .Item("Performance Dashboard", ICON_MD_SPEED,
          [this]() { show_performance_dashboard_ = true; })
#ifdef YAZE_WITH_GRPC
    .Item("Agent Proposals", ICON_MD_PREVIEW,
          [this]() { proposal_drawer_.Toggle(); })
#endif
    .Separator()
    .Item("ImGui Demo", ICON_MD_HELP,
          [this]() { show_imgui_demo_ = true; },
          nullptr, nullptr,
          [this]() { return show_imgui_demo_; })
    .Item("ImGui Metrics", ICON_MD_ANALYTICS,
          [this]() { show_imgui_metrics_ = true; },
          nullptr, nullptr,
          [this]() { return show_imgui_metrics_; })
    .EndMenu();
  
  // Help Menu
  menu_builder_.BeginMenu("Help")
    .Item("Getting Started", ICON_MD_PLAY_ARROW,
          [this]() { popup_manager_->Show("Getting Started"); })
    .Item("About", ICON_MD_INFO,
          [this]() { popup_manager_->Show("About"); }, "F1")
    .EndMenu();
  
  menu_builder_.Draw();
}

void EditorManager::DrawMenuBarExtras() {
  auto* current_rom = GetCurrentRom();
  std::string version_text = absl::StrFormat("v%s", version_.c_str());
  float version_width = ImGui::CalcTextSize(version_text.c_str()).x;
  float session_rom_area_width = 280.0f;

  SameLine(ImGui::GetWindowWidth() - version_width - 10 - session_rom_area_width);

  if (GetActiveSessionCount() > 1) {
    if (ImGui::SmallButton(absl::StrFormat("%s%zu", ICON_MD_TAB, GetActiveSessionCount()).c_str())) {
      ShowSessionSwitcher();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Sessions: %zu active\nClick to switch", GetActiveSessionCount());
    }
    ImGui::SameLine();
  }

  if (current_rom && current_rom->is_loaded()) {
    if (ImGui::SmallButton(ICON_MD_APPS)) {
      ShowEditorSelection();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(ICON_MD_DASHBOARD " Editor Selection (Ctrl+E)");
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_DISPLAY_SETTINGS)) {
      ShowDisplaySettings();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(ICON_MD_TUNE " Display Settings");
    }
    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();
  }

  if (current_rom && current_rom->is_loaded()) {
    std::string rom_display = current_rom->title();
    if (rom_display.length() > 22) {
      rom_display = rom_display.substr(0, 19) + "...";
    }
    if (ImGui::SmallButton(absl::StrFormat("%s%s", rom_display.c_str(), current_rom->dirty() ? "*" : "").c_str())) {
      ImGui::OpenPopup("ROM Details");
    }
  } else {
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No ROM");
    ImGui::SameLine();
  }

  SameLine(ImGui::GetWindowWidth() - version_width - 10);
  ImGui::Text("%s", version_text.c_str());
}

void EditorManager::ShowSessionSwitcher() { show_session_switcher_ = true; }

void EditorManager::ShowEditorSelection() { show_editor_selection_ = true; }

void EditorManager::ShowDisplaySettings() { if (popup_manager_) popup_manager_->Show("Display Settings"); }

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
  if (show_memory_editor_ && current_editor_set_) {
    current_editor_set_->memory_editor_.Update(show_memory_editor_);
  }
  if (show_asm_editor_ && current_editor_set_) {
    current_editor_set_->assembly_editor_.Update(show_asm_editor_);
  }
  
  // Project file editor
  project_file_editor_.Draw();
  if (show_performance_dashboard_) {
    gfx::PerformanceDashboard::Get().SetVisible(true);
    gfx::PerformanceDashboard::Get().Update();
    gfx::PerformanceDashboard::Get().Render();
    if (!gfx::PerformanceDashboard::Get().IsVisible()) {
      show_performance_dashboard_ = false;
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

  // Welcome screen (accessible from View menu)
  if (show_welcome_screen_) {
    DrawWelcomeScreen();
  }

  if (show_emulator_) {
    Begin("Emulator", &show_emulator_, ImGuiWindowFlags_MenuBar);
    emulator_.Run(current_rom_);
    End();
  }

  // Enhanced Command Palette UI with Fuzzy Search
  if (show_command_palette_) {
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

    if (Begin(absl::StrFormat("%s Command Palette", ICON_MD_SEARCH).c_str(),
              &show_command_palette_, ImGuiWindowFlags_NoCollapse)) {

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
          absl::StrFormat("%s Search commands (fuzzy matching enabled)...", ICON_MD_SEARCH).c_str(),
          query, IM_ARRAYSIZE(query));

      ImGui::SameLine();
      if (ImGui::Button(absl::StrFormat("%s Clear", ICON_MD_CLEAR).c_str())) {
        query[0] = '\0';
        input_changed = true;
        selected_idx = 0;
      }

      Separator();

      // Fuzzy filter commands with scoring
      std::vector<std::pair<int, std::pair<std::string, std::string>>> scored_commands;
      std::string query_lower = query;
      std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), ::tolower);
      
      for (const auto& entry : context_.shortcut_manager.GetShortcuts()) {
        const auto& name = entry.first;
        const auto& shortcut = entry.second;
        
        std::string name_lower = name;
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
        
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
          while (text_idx < name_lower.length() && query_idx < query_lower.length()) {
            if (name_lower[text_idx] == query_lower[query_idx]) {
              score += 10;
              query_idx++;
            }
            text_idx++;
          }
          if (query_idx != query_lower.length()) score = 0;
        }
        
        if (score > 0) {
          std::string shortcut_text = shortcut.keys.empty()
              ? "" : absl::StrFormat("(%s)", PrintShortcut(shortcut.keys).c_str());
          scored_commands.push_back({score, {name, shortcut_text}});
        }
      }
      
      std::sort(scored_commands.begin(), scored_commands.end(),
                [](const auto& a, const auto& b) { return a.first > b.first; });

      // Display results with categories
      if (ImGui::BeginTabBar("CommandCategories")) {
        if (ImGui::BeginTabItem(ICON_MD_LIST " All Commands")) {
          if (ImGui::BeginTable("CommandPaletteTable", 3,
                                ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                                    ImGuiTableFlags_SizingStretchProp,
                                ImVec2(0, -30))) {

            ImGui::TableSetupColumn("Command", ImGuiTableColumnFlags_WidthStretch, 0.5f);
            ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthStretch, 0.3f);
            ImGui::TableSetupColumn("Score", ImGuiTableColumnFlags_WidthStretch, 0.2f);
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
                const auto& shortcuts = context_.shortcut_manager.GetShortcuts();
                auto it = shortcuts.find(command_name);
                if (it != shortcuts.end() && it->second.callback) {
                  it->second.callback();
                  show_command_palette_ = false;
                }
              }
              ImGui::PopID();

              ImGui::TableNextColumn();
              ImGui::TextDisabled("%s", shortcut_text.c_str());
              
              ImGui::TableNextColumn();
              if (score > 0) ImGui::TextDisabled("%d", score);
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
      ImGui::Text("%s %zu commands | Score: fuzzy match", ICON_MD_INFO, scored_commands.size());
      ImGui::SameLine();
      ImGui::TextDisabled("| ↑↓=Navigate | Enter=Execute | Esc=Close");
    }
    End();
  }

  // Enhanced Global Search UI
  if (show_global_search_) {
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

    if (Begin(
            absl::StrFormat("%s Global Search", ICON_MD_MANAGE_SEARCH).c_str(),
            &show_global_search_, ImGuiWindowFlags_NoCollapse)) {

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
                show_global_search_ = false;
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
                  show_global_search_ = false;
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

  Rom temp_rom;
  RETURN_IF_ERROR(temp_rom.LoadFromFile(file_name));

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
    sessions_.emplace_back(std::move(temp_rom), &user_settings_);
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
  show_welcome_screen_ = false;
  
  // Clear recent editors for fresh start with new ROM and show editor selection dialog
  editor_selection_dialog_.ClearRecentEditors();
  show_editor_selection_ = true;

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
    show_welcome_screen_ = false;
    editor_selection_dialog_.ClearRecentEditors();
    show_editor_selection_ = true;
  }
  return absl::OkStatus();
}

absl::Status EditorManager::CreateNewProject(const std::string& template_name) {
  auto dialog_path = util::FileDialogWrapper::ShowOpenFolderDialog();
  if (dialog_path.empty()) {
    return absl::OkStatus();  // User cancelled
  }

  // Show project creation dialog
  popup_manager_->Show("Create New Project");
  return absl::OkStatus();
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
    show_welcome_screen_ = false;
    editor_selection_dialog_.ClearRecentEditors();
    show_editor_selection_ = true;
  }

  // Apply workspace settings
  user_settings_.prefs().font_global_scale = current_project_.workspace_settings.font_global_scale;
  user_settings_.prefs().autosave_enabled = current_project_.workspace_settings.autosave_enabled;
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

    current_project_.workspace_settings.font_global_scale = user_settings_.prefs().font_global_scale;
    current_project_.workspace_settings.autosave_enabled = user_settings_.prefs().autosave_enabled;
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
  // Check session limit
  if (sessions_.size() >= 8) {
    popup_manager_->Show("Session Limit Warning");
    return;
  }

  // Create a blank session
  sessions_.emplace_back();
  RomSession& session = sessions_.back();
  
  // Set user settings for the blank session
  session.editors.set_user_settings(&user_settings_);

  // Wire editor contexts for new session
  for (auto* editor : session.editors.active_editors_) {
    editor->set_context(&context_);
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

  // Create a copy of the current ROM
  Rom rom_copy = *current_rom_;
  sessions_.emplace_back(std::move(rom_copy), &user_settings_);
  RomSession& session = sessions_.back();

  // Wire editor contexts
  for (auto* editor : session.editors.active_editors_) {
    editor->set_context(&context_);
  }

  toast_manager_.Show(
      absl::StrFormat("Session duplicated (Session %zu)", sessions_.size()),
      editor::ToastType::kSuccess);
}

void EditorManager::CloseCurrentSession() {
  if (GetActiveSessionCount() <= 1) {
    toast_manager_.Show("Cannot close the last active session",
                        editor::ToastType::kWarning);
    return;
  }

  // Find current session index
  size_t current_index = GetCurrentSessionIndex();

  // Switch to another active session before removing current one
  size_t next_index = 0;
  for (size_t i = 0; i < sessions_.size(); ++i) {
    if (i != current_index && sessions_[i].custom_name != "[CLOSED SESSION]") {
      next_index = i;
      break;
    }
  }

  current_rom_ = &sessions_[next_index].rom;
  current_editor_set_ = &sessions_[next_index].editors;
  test::TestManager::Get().SetCurrentRom(current_rom_);

  // Now remove the current session
  RemoveSession(current_index);

  toast_manager_.Show("Session closed successfully",
                      editor::ToastType::kSuccess);
}

void EditorManager::RemoveSession(size_t index) {
  if (index >= sessions_.size()) {
    toast_manager_.Show("Invalid session index for removal",
                        editor::ToastType::kError);
    return;
  }

  if (GetActiveSessionCount() <= 1) {
    toast_manager_.Show("Cannot remove the last active session",
                        editor::ToastType::kWarning);
    return;
  }

  // Get session info for logging
  std::string session_name = sessions_[index].GetDisplayName();

  // For now, mark the session as invalid instead of removing it from the deque
  // This is a safer approach until RomSession becomes fully movable
  sessions_[index].rom.Close();  // Close the ROM to mark as invalid
  sessions_[index].custom_name = "[CLOSED SESSION]";
  sessions_[index].filepath = "";

  LOG_DEBUG("EditorManager", "Marked session as closed: %s (index %zu)",
           session_name.c_str(), index);
  toast_manager_.Show(
      absl::StrFormat("Session marked as closed: %s", session_name),
      editor::ToastType::kInfo);

  // TODO: Implement proper session removal when EditorSet becomes movable
  // The current workaround marks sessions as closed instead of removing them
}

void EditorManager::SwitchToSession(size_t index) {
  if (index >= sessions_.size()) {
    toast_manager_.Show("Invalid session index", editor::ToastType::kError);
    return;
  }

  auto& session = sessions_[index];
  current_rom_ = &session.rom;
  current_editor_set_ = &session.editors;

  // Update test manager with current ROM for ROM-dependent tests
  util::logf("EditorManager: Setting ROM in TestManager - %p ('%s')",
             (void*)current_rom_,
             current_rom_ ? current_rom_->title().c_str() : "null");
  test::TestManager::Get().SetCurrentRom(current_rom_);

  std::string session_name = session.GetDisplayName();
  toast_manager_.Show(absl::StrFormat("Switched to %s", session_name),
                      editor::ToastType::kInfo);
}

size_t EditorManager::GetCurrentSessionIndex() const {
  for (size_t i = 0; i < sessions_.size(); ++i) {
    if (&sessions_[i].rom == current_rom_ &&
        sessions_[i].custom_name != "[CLOSED SESSION]") {
      return i;
    }
  }
  return 0;  // Default to first session if not found
}

size_t EditorManager::GetActiveSessionCount() const {
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

  if (sessions_.size() <= 1) {
    // Single session - use simple name
    return std::string(base_name);
  }

  // Multi-session - include session identifier
  const auto& session = sessions_[session_index];
  std::string session_name = session.GetDisplayName();

  // Truncate long session names
  if (session_name.length() > 20) {
    session_name = session_name.substr(0, 17) + "...";
  }

  return absl::StrFormat("%s - %s##session_%zu", base_name, session_name,
                         session_index);
}

void EditorManager::ResetWorkspaceLayout() {
  // Show confirmation popup first
  popup_manager_->Show("Layout Reset Confirm");
}

void EditorManager::SaveWorkspaceLayout() {
  ImGui::SaveIniSettingsToDisk("yaze_workspace.ini");
  toast_manager_.Show("Workspace layout saved", editor::ToastType::kSuccess);
}

void EditorManager::LoadWorkspaceLayout() {
  ImGui::LoadIniSettingsFromDisk("yaze_workspace.ini");
  toast_manager_.Show("Workspace layout loaded", editor::ToastType::kSuccess);
}

void EditorManager::ShowAllWindows() {
  if (!current_editor_set_)
    return;

  for (auto* editor : current_editor_set_->active_editors_) {
    editor->set_active(true);
  }
  show_imgui_demo_ = true;
  show_imgui_metrics_ = true;
  show_performance_dashboard_ = true;
#ifdef YAZE_ENABLE_TESTING
  show_test_dashboard_ = true;
#endif

  toast_manager_.Show("All windows shown", editor::ToastType::kInfo);
}

void EditorManager::HideAllWindows() {
  if (!current_editor_set_)
    return;

  for (auto* editor : current_editor_set_->active_editors_) {
    editor->set_active(false);
  }
  show_imgui_demo_ = false;
  show_imgui_metrics_ = false;
  show_performance_dashboard_ = false;
#ifdef YAZE_ENABLE_TESTING
  show_test_dashboard_ = false;
#endif

  toast_manager_.Show("All windows hidden", editor::ToastType::kInfo);
}

void EditorManager::MaximizeCurrentWindow() {
  // This would maximize the current focused window
  // Implementation depends on ImGui internal window management
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

void EditorManager::DrawSessionSwitcher() {
  if (!show_session_switcher_)
    return;

  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                          ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(700, 450), ImGuiCond_Appearing);

  if (ImGui::Begin(
          absl::StrFormat("%s Session Switcher", ICON_MD_SWITCH_ACCOUNT)
              .c_str(),
          &show_session_switcher_, ImGuiWindowFlags_NoCollapse)) {

    // Header with enhanced info
    ImGui::Text("%s %zu Sessions Available", ICON_MD_TAB, sessions_.size());
    ImGui::SameLine(ImGui::GetWindowWidth() - 120);
    if (ImGui::Button(absl::StrFormat("%s New", ICON_MD_ADD).c_str(),
                      ImVec2(50, 0))) {
      CreateNewSession();
    }
    ImGui::SameLine();
    if (ImGui::Button(absl::StrFormat("%s Manager", ICON_MD_SETTINGS).c_str(),
                      ImVec2(60, 0))) {
      show_session_manager_ = true;
    }

    ImGui::Separator();

    // Enhanced session list using table for better layout
    const float TABLE_HEIGHT = ImGui::GetContentRegionAvail().y -
                               50;  // Reserve space for close button

    if (ImGui::BeginTable("SessionSwitcherTable", 4,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingStretchProp |
                              ImGuiTableFlags_ScrollY |
                              ImGuiTableFlags_Resizable,
                          ImVec2(0, TABLE_HEIGHT))) {

      // Setup columns with proper sizing weights
      ImGui::TableSetupColumn("Session", ImGuiTableColumnFlags_WidthStretch,
                              0.3f);
      ImGui::TableSetupColumn("ROM Info", ImGuiTableColumnFlags_WidthStretch,
                              0.4f);
      ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed,
                              90.0f);
      ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed,
                              140.0f);
      ImGui::TableHeadersRow();

      for (size_t i = 0; i < sessions_.size(); ++i) {
        auto& session = sessions_[i];

        // Skip closed sessions
        if (session.custom_name == "[CLOSED SESSION]") {
          continue;
        }

        bool is_current = (&session.rom == current_rom_);

        ImGui::PushID(static_cast<int>(i));
        ImGui::TableNextRow(ImGuiTableRowFlags_None,
                            55.0f);  // Consistent row height

        // Session name column with better styling
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();

        if (is_current) {
          ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
          ImGui::Text("%s %s", ICON_MD_STAR, session.GetDisplayName().c_str());
          ImGui::PopStyleColor();
        } else {
          ImGui::Text("%s %s", ICON_MD_TAB, session.GetDisplayName().c_str());
        }

        // ROM info column with better information layout
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();

        if (session.rom.is_loaded()) {
          ImGui::Text("%s %s", ICON_MD_VIDEOGAME_ASSET,
                      session.rom.title().c_str());
          ImGui::Text("%.1f MB | %s", session.rom.size() / 1048576.0f,
                      session.rom.dirty() ? "Modified" : "Clean");
        } else {
          ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s No ROM loaded",
                             ICON_MD_WARNING);
        }

        // Status column with better visual indicators
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();

        if (session.rom.is_loaded()) {
          if (session.rom.dirty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "%s",
                               ICON_MD_EDIT);
          } else {
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%s",
                               ICON_MD_CHECK_CIRCLE);
          }
        } else {
          ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s",
                             ICON_MD_RADIO_BUTTON_UNCHECKED);
        }

        // Actions column with improved button layout
        ImGui::TableNextColumn();

        // Create button group for better alignment
        ImGui::BeginGroup();

        if (!is_current) {
          if (ImGui::Button("Switch")) {
            SwitchToSession(i);
            show_session_switcher_ = false;
          }
        } else {
          ImGui::BeginDisabled();
          ImGui::Button("Current");
          ImGui::EndDisabled();
        }

        ImGui::SameLine();
        if (ImGui::Button("Rename")) {
          session_to_rename_ = i;
          // Safe string copy with bounds checking
          const std::string& name = session.GetDisplayName();
          size_t copy_len =
              std::min(name.length(), sizeof(session_rename_buffer_) - 1);
          std::memcpy(session_rename_buffer_, name.c_str(), copy_len);
          session_rename_buffer_[copy_len] = '\0';
          show_session_rename_dialog_ = true;
        }

        ImGui::SameLine();

        // Close button logic
        bool can_close = GetActiveSessionCount() > 1;
        if (!can_close) {
          ImGui::BeginDisabled();
        }

        if (ImGui::Button("Close")) {
          if (is_current) {
            CloseCurrentSession();
          } else {
            // Remove non-current session directly
            RemoveSession(i);
            show_session_switcher_ =
                false;  // Close switcher since indices changed
          }
        }

        if (!can_close) {
          ImGui::EndDisabled();
        }

        ImGui::EndGroup();

        ImGui::PopID();
      }

      ImGui::EndTable();
    }

    ImGui::Separator();
    if (ImGui::Button(
            absl::StrFormat("%s Close Switcher", ICON_MD_CLOSE).c_str(),
            ImVec2(-1, 0))) {
      show_session_switcher_ = false;
    }
  }
  ImGui::End();
}

void EditorManager::DrawSessionManager() {
  if (!show_session_manager_)
    return;

  if (ImGui::Begin(absl::StrCat(ICON_MD_VIEW_LIST, " Session Manager").c_str(),
                   &show_session_manager_)) {

    ImGui::Text("%s Session Management", ICON_MD_MANAGE_ACCOUNTS);

    if (ImGui::Button(absl::StrCat(ICON_MD_ADD, " New Session").c_str())) {
      CreateNewSession();
    }
    ImGui::SameLine();
    if (ImGui::Button(
            absl::StrCat(ICON_MD_CONTENT_COPY, " Duplicate Current").c_str()) &&
        current_rom_) {
      DuplicateCurrentSession();
    }

    ImGui::Separator();
    ImGui::Text("%s Active Sessions (%zu)", ICON_MD_TAB,
                GetActiveSessionCount());

    // Enhanced session management table with proper sizing
    const float AVAILABLE_HEIGHT = ImGui::GetContentRegionAvail().y;

    if (ImGui::BeginTable("SessionTable", 6,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable |
                              ImGuiTableFlags_ScrollY |
                              ImGuiTableFlags_SizingStretchProp |
                              ImGuiTableFlags_ContextMenuInBody,
                          ImVec2(0, AVAILABLE_HEIGHT))) {

      // Setup columns with explicit sizing for better control
      ImGui::TableSetupColumn("Session", ImGuiTableColumnFlags_WidthStretch,
                              0.15f);
      ImGui::TableSetupColumn("ROM Title", ImGuiTableColumnFlags_WidthStretch,
                              0.3f);
      ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 70.0f);
      ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed,
                              80.0f);
      ImGui::TableSetupColumn("Custom OW", ImGuiTableColumnFlags_WidthFixed,
                              80.0f);
      ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthStretch,
                              0.25f);
      ImGui::TableHeadersRow();

      for (size_t i = 0; i < sessions_.size(); ++i) {
        auto& session = sessions_[i];

        // Skip closed sessions in session manager too
        if (session.custom_name == "[CLOSED SESSION]") {
          continue;
        }

        bool is_current = (&session.rom == current_rom_);

        ImGui::TableNextRow(ImGuiTableRowFlags_None,
                            50.0f);  // Consistent row height
        ImGui::PushID(static_cast<int>(i));

        // Session name column
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        if (is_current) {
          ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%s Session %zu",
                             ICON_MD_STAR, i + 1);
        } else {
          ImGui::Text("%s Session %zu", ICON_MD_TAB, i + 1);
        }

        // ROM title column
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        std::string display_name = session.GetDisplayName();
        if (!session.custom_name.empty()) {
          ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "%s %s",
                             ICON_MD_EDIT, display_name.c_str());
        } else {
          // Use TextWrapped for long ROM titles
          ImGui::PushTextWrapPos(ImGui::GetCursorPos().x +
                                 ImGui::GetColumnWidth());
          ImGui::Text("%s", display_name.c_str());
          ImGui::PopTextWrapPos();
        }

        // File size column
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        if (session.rom.is_loaded()) {
          ImGui::Text("%.1f MB", session.rom.size() / 1048576.0f);
        } else {
          ImGui::TextDisabled("N/A");
        }

        // Status column
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        if (session.rom.is_loaded()) {
          if (session.rom.dirty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "%s",
                               ICON_MD_EDIT);
          } else {
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%s",
                               ICON_MD_CHECK_CIRCLE);
          }
        } else {
          ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s",
                             ICON_MD_RADIO_BUTTON_UNCHECKED);
        }

        // Custom Overworld checkbox column
        ImGui::TableNextColumn();

        // Center the checkbox vertically
        float checkbox_offset =
            (ImGui::GetFrameHeight() - ImGui::GetTextLineHeight()) * 0.5f;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + checkbox_offset);

        ImGui::PushID(
            static_cast<int>(i + 100));  // Different ID to avoid conflicts
        bool custom_ow_enabled =
            session.feature_flags.overworld.kLoadCustomOverworld;
        if (ImGui::Checkbox("##CustomOW", &custom_ow_enabled)) {
          session.feature_flags.overworld.kLoadCustomOverworld =
              custom_ow_enabled;
          if (is_current) {
            core::FeatureFlags::get().overworld.kLoadCustomOverworld =
                custom_ow_enabled;
          }
          toast_manager_.Show(
              absl::StrFormat("Session %zu: Custom Overworld %s", i + 1,
                              custom_ow_enabled ? "Enabled" : "Disabled"),
              editor::ToastType::kInfo);
        }
        ImGui::PopID();

        // Actions column with better button layout
        ImGui::TableNextColumn();

        // Create button group for better alignment
        ImGui::BeginGroup();

        if (!is_current) {
          if (ImGui::Button("Switch")) {
            SwitchToSession(i);
          }
        } else {
          ImGui::BeginDisabled();
          ImGui::Button("Current");
          ImGui::EndDisabled();
        }

        ImGui::SameLine();
        if (ImGui::Button("Rename")) {
          session_to_rename_ = i;
          // Safe string copy with bounds checking
          const std::string& name = session.GetDisplayName();
          size_t copy_len =
              std::min(name.length(), sizeof(session_rename_buffer_) - 1);
          std::memcpy(session_rename_buffer_, name.c_str(), copy_len);
          session_rename_buffer_[copy_len] = '\0';
          show_session_rename_dialog_ = true;
        }

        ImGui::SameLine();

        // Close button logic
        bool can_close = GetActiveSessionCount() > 1;
        if (!can_close || is_current) {
          ImGui::BeginDisabled();
        }

        if (ImGui::Button("Close")) {
          if (is_current) {
            CloseCurrentSession();
            break;  // Exit loop since current session was closed
          } else {
            // Remove non-current session directly
            RemoveSession(i);
            break;  // Exit loop since session indices changed
          }
        }

        if (!can_close || is_current) {
          ImGui::EndDisabled();
        }

        ImGui::EndGroup();

        ImGui::PopID();
      }

      ImGui::EndTable();
    }
  }
  ImGui::End();
}

void EditorManager::DrawLayoutPresets() {
  if (!show_layout_presets_)
    return;

  if (ImGui::Begin(absl::StrCat(ICON_MD_BOOKMARK, " Layout Presets").c_str(),
                   &show_layout_presets_)) {

    ImGui::Text("%s Predefined Layouts", ICON_MD_DASHBOARD);

    // Predefined layouts
    if (ImGui::Button(
            absl::StrCat(ICON_MD_DEVELOPER_MODE, " Developer Layout").c_str(),
            ImVec2(-1, 40))) {
      LoadDeveloperLayout();
    }
    ImGui::SameLine();
    ImGui::Text("Code editing, debugging, testing");

    if (ImGui::Button(
            absl::StrCat(ICON_MD_DESIGN_SERVICES, " Designer Layout").c_str(),
            ImVec2(-1, 40))) {
      LoadDesignerLayout();
    }
    ImGui::SameLine();
    ImGui::Text("Graphics, palettes, sprites");

    if (ImGui::Button(absl::StrCat(ICON_MD_GAMEPAD, " Modder Layout").c_str(),
                      ImVec2(-1, 40))) {
      LoadModderLayout();
    }
    ImGui::SameLine();
    ImGui::Text("All gameplay editors");

    ImGui::Separator();
    ImGui::Text("%s Custom Presets", ICON_MD_BOOKMARK);

    // Lazy load workspace presets when UI is accessed
    if (!workspace_manager_.workspace_presets_loaded()) {
      RefreshWorkspacePresets();
    }

    for (const auto& preset : workspace_manager_.workspace_presets()) {
      if (ImGui::Button(
              absl::StrFormat("%s %s", ICON_MD_BOOKMARK, preset.c_str())
                  .c_str(),
              ImVec2(-1, 0))) {
        LoadWorkspacePreset(preset);
        toast_manager_.Show(
            absl::StrFormat("Loaded preset: %s", preset.c_str()),
            editor::ToastType::kSuccess);
      }
    }

    if (workspace_manager_.workspace_presets().empty()) {
      ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                         "No custom presets saved");
    }

    ImGui::Separator();
    if (ImGui::Button(
            absl::StrCat(ICON_MD_ADD, " Save Current Layout").c_str())) {
      show_save_workspace_preset_ = true;
    }
  }
  ImGui::End();
}

bool EditorManager::HasDuplicateSession(const std::string& filepath) {
  for (const auto& session : sessions_) {
    if (session.filepath == filepath) {
      return true;
    }
  }
  return false;
}

void EditorManager::RenameSession(size_t index, const std::string& new_name) {
  if (index < sessions_.size()) {
    sessions_[index].custom_name = new_name;
    toast_manager_.Show(
        absl::StrFormat("Session renamed to: %s", new_name.c_str()),
        editor::ToastType::kSuccess);
  }
}

void EditorManager::DrawSessionRenameDialog() {
  if (!show_session_rename_dialog_)
    return;

  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                          ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(400, 150), ImGuiCond_Appearing);

  if (ImGui::Begin("Rename Session", &show_session_rename_dialog_,
                   ImGuiWindowFlags_NoResize)) {
    if (session_to_rename_ < sessions_.size()) {
      const auto& session = sessions_[session_to_rename_];

      ImGui::Text("Rename Session:");
      ImGui::Text("Current: %s", session.GetDisplayName().c_str());
      ImGui::Separator();

      ImGui::InputText("New Name", session_rename_buffer_,
                       sizeof(session_rename_buffer_));

      ImGui::Separator();
      if (ImGui::Button("Rename", ImVec2(120, 0))) {
        std::string new_name(session_rename_buffer_);
        if (!new_name.empty()) {
          RenameSession(session_to_rename_, new_name);
        }
        show_session_rename_dialog_ = false;
      }

      ImGui::SameLine();
      if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        show_session_rename_dialog_ = false;
      }
    }
  }
  ImGui::End();
}

void EditorManager::DrawWelcomeScreen() {
  // Use the new WelcomeScreen class for a modern, feature-rich experience
  welcome_screen_.RefreshRecentProjects();
  bool was_open = show_welcome_screen_;
  bool action_taken = welcome_screen_.Show(&show_welcome_screen_);

  // Check if the welcome screen was manually closed via the close button
  if (was_open && !show_welcome_screen_) {
    welcome_screen_manually_closed_ = true;
    welcome_screen_.MarkManuallyClosed();
  }
}

// ============================================================================
// Jump-to Functionality for Cross-Editor Navigation
// ============================================================================

void EditorManager::JumpToDungeonRoom(int room_id) {
  if (!current_editor_set_) return;
  
  // Switch to dungeon editor
  SwitchToEditor(EditorType::kDungeon);
  
  // Open the room in the dungeon editor
  current_editor_set_->dungeon_editor_.add_room(room_id);
}

void EditorManager::JumpToOverworldMap(int map_id) {
  if (!current_editor_set_) return;
  
  // Switch to overworld editor
  SwitchToEditor(EditorType::kOverworld);
  
  // Set the current map in the overworld editor
  current_editor_set_->overworld_editor_.set_current_map(map_id);
}

void EditorManager::SwitchToEditor(EditorType editor_type) {
  // Find the editor tab and activate it
  for (size_t i = 0; i < current_editor_set_->active_editors_.size(); ++i) {
    if (current_editor_set_->active_editors_[i]->type() == editor_type) {
      current_editor_set_->active_editors_[i]->set_active(true);
      
      // Set editor as the current/focused one
      // This will make it visible when tabs are rendered
      break;
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
  if (!user_settings_.prefs().show_welcome_on_startup) {
    show_welcome_screen_ = false;
    welcome_screen_manually_closed_ = true;
  }
}

void EditorManager::SaveUserSettings() {
  auto status = user_settings_.Save();
  if (!status.ok()) {
    LOG_WARN("EditorManager", "Failed to save user settings: %s", status.ToString().c_str());
  }
}

}  // namespace editor
}  // namespace yaze
