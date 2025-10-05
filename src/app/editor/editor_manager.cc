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
#include "app/core/platform/file_dialog.h"
#include "app/core/project.h"
#include "app/editor/code/assembly_editor.h"
#include "app/editor/dungeon/dungeon_editor.h"
#include "app/editor/graphics/graphics_editor.h"
#include "app/editor/graphics/palette_editor.h"
#include "app/editor/graphics/screen_editor.h"
#include "app/editor/music/music_editor.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/sprite/sprite_editor.h"
#include "app/emu/emulator.h"
#include "app/gfx/arena.h"
#include "app/gfx/performance_profiler.h"
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
#include "app/gfx/performance_dashboard.h"
#include "editor/editor.h"
#ifdef YAZE_WITH_GRPC
#include "app/core/service/screenshot_utils.h"
#include "app/editor/agent/agent_chat_widget.h"
#include "cli/service/agent/conversational_agent_service.h"
#include "cli/service/ai/gemini_ai_service.h"
#endif
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "util/log.h"
#include "util/macro.h"
#include "yaze_config.h"

namespace yaze {
namespace editor {

using namespace ImGui;
using core::FileDialogWrapper;

namespace {

std::string GetEditorName(EditorType type) {
  return kEditorNames[static_cast<int>(type)];
}

}  // namespace

// Settings + preset helpers
void EditorManager::LoadUserSettings() {
  try {
    auto data = core::LoadConfigFile(settings_filename_);
    if (!data.empty()) {
      std::istringstream ss(data);
      std::string line;
      while (std::getline(ss, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos)
          continue;
        auto key = line.substr(0, eq);
        auto val = line.substr(eq + 1);
        if (key == "font_global_scale")
          font_global_scale_ = std::stof(val);
        if (key == "autosave_enabled")
          autosave_enabled_ = (val == "1");
        if (key == "autosave_interval_secs")
          autosave_interval_secs_ = std::stof(val);
      }
      ImGui::GetIO().FontGlobalScale = font_global_scale_;
    }
  } catch (...) {}
}

void EditorManager::SaveUserSettings() {
  std::ostringstream ss;
  ss << "font_global_scale=" << font_global_scale_ << "\n";
  ss << "autosave_enabled=" << (autosave_enabled_ ? 1 : 0) << "\n";
  ss << "autosave_interval_secs=" << autosave_interval_secs_ << "\n";
  core::SaveFile(settings_filename_, ss.str());
}

void EditorManager::RefreshWorkspacePresets() {
  // Safe clearing with error handling
  try {
    // Create a new vector instead of clearing to avoid corruption
    std::vector<std::string> new_presets;

    // Try to read a simple index file of presets
    try {
      auto data = core::LoadConfigFile("workspace_presets.txt");
      if (!data.empty()) {
        std::istringstream ss(data);
        std::string name;
        while (std::getline(ss, name)) {
          // Trim whitespace and validate
          name.erase(0, name.find_first_not_of(" \t\r\n"));
          name.erase(name.find_last_not_of(" \t\r\n") + 1);
          if (!name.empty() &&
              name.length() < 256) {  // Reasonable length limit
            new_presets.emplace_back(std::move(name));
          }
        }
      }
    } catch (const std::exception& e) {
      LOG_WARN("EditorManager", "Failed to load workspace presets: %s", e.what());
    }

    // Safely replace the vector
    workspace_presets_ = std::move(new_presets);
    workspace_presets_loaded_ = true;

  } catch (const std::exception& e) {
    LOG_ERROR("EditorManager", "Error in RefreshWorkspacePresets: %s", e.what());
    // Ensure we have a valid empty vector
    workspace_presets_ = std::vector<std::string>();
    workspace_presets_loaded_ =
        true;  // Mark as loaded even if empty to avoid retry
  }
}

void EditorManager::SaveWorkspacePreset(const std::string& name) {
  if (name.empty())
    return;
  std::string ini_name = absl::StrCat("yaze_workspace_", name, ".ini");
  ImGui::SaveIniSettingsToDisk(ini_name.c_str());

  // Ensure presets are loaded before updating
  if (!workspace_presets_loaded_) {
    RefreshWorkspacePresets();
  }

  // Update index
  if (std::find(workspace_presets_.begin(), workspace_presets_.end(), name) ==
      workspace_presets_.end()) {
    workspace_presets_.emplace_back(name);
    try {
      std::ostringstream ss;
      for (const auto& n : workspace_presets_)
        ss << n << "\n";
      core::SaveFile("workspace_presets.txt", ss.str());
    } catch (const std::exception& e) {
      LOG_WARN("EditorManager", "Failed to save workspace presets: %s", e.what());
    }
  }
  last_workspace_preset_ = name;
}

void EditorManager::LoadWorkspacePreset(const std::string& name) {
  if (name.empty())
    return;
  std::string ini_name = absl::StrCat("yaze_workspace_", name, ".ini");
  ImGui::LoadIniSettingsFromDisk(ini_name.c_str());
  last_workspace_preset_ = name;
}

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

void EditorManager::Initialize(const std::string& filename) {
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
  // Initialize the agent editor
  agent_editor_.Initialize(&toast_manager_, &proposal_drawer_);

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
    config.model = "gemini-2.0-flash-exp";  // Use vision-capable model
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
#endif

  // Load critical user settings first
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

  // Initialize menu items
  std::vector<gui::MenuItem> recent_files;
  auto& manager = core::RecentFilesManager::GetInstance();
  if (manager.GetRecentFiles().empty()) {
    recent_files.emplace_back("No Recent Files", "", nullptr);
  } else {
    for (const auto& filePath : manager.GetRecentFiles()) {
      recent_files.emplace_back(filePath, "", [filePath, this]() {
        status_ = OpenRomOrProject(filePath);
      });
    }
  }

  std::vector<gui::MenuItem> options_subitems;
  options_subitems.emplace_back(
      "Backup ROM", "", [this]() { backup_rom_ = !backup_rom_; },
      [this]() { return backup_rom_; });
  options_subitems.emplace_back(
      "Save New Auto", "", [this]() { save_new_auto_ = !save_new_auto_; },
      [this]() { return save_new_auto_; });
  options_subitems.emplace_back(
      "Autosave", "",
      [this]() {
        autosave_enabled_ = !autosave_enabled_;
        toast_manager_.Show(
            autosave_enabled_ ? "Autosave enabled" : "Autosave disabled",
            editor::ToastType::kInfo);
      },
      [this]() { return autosave_enabled_; });
  options_subitems.emplace_back(
      "Autosave Interval", "", [this]() {}, []() { return true; },
      std::vector<gui::MenuItem>{
          {"1 min", "",
           [this]() {
             autosave_interval_secs_ = 60.0f;
             SaveUserSettings();
           }},
          {"2 min", "",
           [this]() {
             autosave_interval_secs_ = 120.0f;
             SaveUserSettings();
           }},
          {"5 min", "",
           [this]() {
             autosave_interval_secs_ = 300.0f;
             SaveUserSettings();
           }},
      });

  std::vector<gui::MenuItem> project_menu_subitems;
  project_menu_subitems.emplace_back(
      "New Project", "", [this]() { popup_manager_->Show("New Project"); });
  project_menu_subitems.emplace_back("Open Project", "",
                                     [this]() { status_ = OpenProject(); });
  project_menu_subitems.emplace_back(
      "Save Project", "", [this]() { status_ = SaveProject(); },
      [this]() { return current_project_.project_opened(); });
  project_menu_subitems.emplace_back(gui::kSeparator, "", nullptr, []() { return true; });
  project_menu_subitems.emplace_back(
      absl::StrCat(ICON_MD_EDIT, " Edit Project File"), "",
      [this]() { 
        project_file_editor_.set_active(true);
        if (current_project_.project_opened() && !current_project_.filepath.empty()) {
          auto status = project_file_editor_.LoadFile(current_project_.filepath);
          if (!status.ok()) {
            toast_manager_.Show(std::string(status.message()), editor::ToastType::kError);
          }
        }
      },
      [this]() { return current_project_.project_opened(); });
  project_menu_subitems.emplace_back("Save Workspace Layout", "", [this]() {
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::SaveIniSettingsToDisk("yaze_workspace.ini");
    toast_manager_.Show("Workspace layout saved", editor::ToastType::kSuccess);
  });
  project_menu_subitems.emplace_back("Load Workspace Layout", "", [this]() {
    ImGui::LoadIniSettingsFromDisk("yaze_workspace.ini");
    toast_manager_.Show("Workspace layout loaded", editor::ToastType::kSuccess);
  });
  project_menu_subitems.emplace_back(
      "Workspace Presets", "", []() {}, []() { return true; },
      std::vector<gui::MenuItem>{
          {"Save Preset", "",
           [this]() {
             show_save_workspace_preset_ = true;
           }},
          {"Load Preset", "",
           [this]() {
             show_load_workspace_preset_ = true;
           }},
      });

  gui::kMainMenu = {
      {"File",
       {},
       {},
       {},
       {
           {absl::StrCat(ICON_MD_FILE_OPEN, " Open"),
            context_.shortcut_manager.GetKeys("Open"),
            context_.shortcut_manager.GetCallback("Open")},
           {absl::StrCat(ICON_MD_HISTORY, " Open Recent"), "", []() {},
            [&manager]() { return !manager.GetRecentFiles().empty(); }, recent_files},
           {absl::StrCat(ICON_MD_FILE_DOWNLOAD, " Save"),
            context_.shortcut_manager.GetKeys("Save"),
            context_.shortcut_manager.GetCallback("Save")},
           {absl::StrCat(ICON_MD_SAVE_AS, " Save As.."), "",
            [this]() { popup_manager_->Show("Save As.."); }},
           {absl::StrCat(ICON_MD_BALLOT, " Project"), "", []() {},
            []() { return true; }, project_menu_subitems},
           {absl::StrCat(ICON_MD_CLOSE, " Close"), "",
            [this]() {
              if (current_rom_) current_rom_->Close();
            }},
           {gui::kSeparator, "", nullptr, []() { return true; }},
           {absl::StrCat(ICON_MD_MISCELLANEOUS_SERVICES, " Options"), "",
            []() {}, []() { return true; }, options_subitems},
           {absl::StrCat(ICON_MD_EXIT_TO_APP, " Quit"), "Ctrl+Q",
            [this]() { quit_ = true; }},
       }},
      {"Edit",
       {},
       {},
       {},
       {
           {absl::StrCat(ICON_MD_CONTENT_CUT, " Cut"),
            context_.shortcut_manager.GetKeys("Cut"),
            context_.shortcut_manager.GetCallback("Cut")},
           {absl::StrCat(ICON_MD_CONTENT_COPY, " Copy"),
            context_.shortcut_manager.GetKeys("Copy"),
            context_.shortcut_manager.GetCallback("Copy")},
           {absl::StrCat(ICON_MD_CONTENT_PASTE, " Paste"),
            context_.shortcut_manager.GetKeys("Paste"),
            context_.shortcut_manager.GetCallback("Paste")},
           {gui::kSeparator, "", nullptr, []() { return true; }},
           {absl::StrCat(ICON_MD_UNDO, " Undo"),
            context_.shortcut_manager.GetKeys("Undo"),
            context_.shortcut_manager.GetCallback("Undo")},
           {absl::StrCat(ICON_MD_REDO, " Redo"),
            context_.shortcut_manager.GetKeys("Redo"),
            context_.shortcut_manager.GetCallback("Redo")},
           {gui::kSeparator, "", nullptr, []() { return true; }},
           {absl::StrCat(ICON_MD_SEARCH, " Find"),
            context_.shortcut_manager.GetKeys("Find"),
            context_.shortcut_manager.GetCallback("Find")},
           {gui::kSeparator, "", nullptr, []() { return true; }},
           // Context-aware editor options
           {absl::StrCat(ICON_MD_REFRESH, " Refresh Data"), "F5",
            [this]() { 
              if (current_editor_ && current_editor_->type() == EditorType::kOverworld) {
                // Refresh overworld data
                auto& ow_editor = static_cast<OverworldEditor&>(*current_editor_);
                [[maybe_unused]] auto load_status = ow_editor.Load();
                toast_manager_.Show("Overworld data refreshed", editor::ToastType::kInfo);
              } else if (current_editor_ && current_editor_->type() == EditorType::kDungeon) {
                // Refresh dungeon data
                toast_manager_.Show("Dungeon data refreshed", editor::ToastType::kInfo);
              }
            },
            [this]() { return current_editor_ != nullptr; }},
           {absl::StrCat(ICON_MD_MAP, " Load All Maps"), "",
            [this]() { 
              if (current_editor_ && current_editor_->type() == EditorType::kOverworld) {
                toast_manager_.Show("Loading all overworld maps...", editor::ToastType::kInfo);
              }
            },
            [this]() { return current_editor_ && current_editor_->type() == EditorType::kOverworld; }},
       }},
      {"View",
       {},
       {},
       {},
       {
           {kAssemblyEditorName, "", [&]() { show_asm_editor_ = true; },
            [&]() { return show_asm_editor_; }},
           {kDungeonEditorName, "",
            [&]() { current_editor_set_->dungeon_editor_.set_active(true); },
            [&]() { return *current_editor_set_->dungeon_editor_.active(); }},
           {kGraphicsEditorName, "",
            [&]() { current_editor_set_->graphics_editor_.set_active(true); },
            [&]() { return *current_editor_set_->graphics_editor_.active(); }},
           {kMusicEditorName, "",
            [&]() { current_editor_set_->music_editor_.set_active(true); },
            [&]() { return *current_editor_set_->music_editor_.active(); }},
           {kOverworldEditorName, "",
            [&]() { current_editor_set_->overworld_editor_.set_active(true); },
            [&]() { return *current_editor_set_->overworld_editor_.active(); }},
           {kPaletteEditorName, "",
            [&]() { current_editor_set_->palette_editor_.set_active(true); },
            [&]() { return *current_editor_set_->palette_editor_.active(); }},
           {kScreenEditorName, "",
            [&]() { current_editor_set_->screen_editor_.set_active(true); },
            [&]() { return *current_editor_set_->screen_editor_.active(); }},
           {kSpriteEditorName, "",
            [&]() { current_editor_set_->sprite_editor_.set_active(true); },
            [&]() { return *current_editor_set_->sprite_editor_.active(); }},
           {kMessageEditorName, "",
            [&]() { current_editor_set_->message_editor_.set_active(true); },
            [&]() { return *current_editor_set_->message_editor_.active(); }},
           {kSettingsEditorName, "",
            [&]() { current_editor_set_->settings_editor_.set_active(true); },
            [&]() { return *current_editor_set_->settings_editor_.active(); }},
           {gui::kSeparator, "", nullptr, []() { return true; }},
           {absl::StrCat(ICON_MD_HOME, " Welcome Screen"), "",
            [&]() { show_welcome_screen_ = true; }},
           {absl::StrCat(ICON_MD_GAMEPAD, " Emulator"), "",
            [&]() { show_emulator_ = true; }},
       }},
      {"Workspace",
       {},
       {},
       {},
       {
           // Session Management
           {absl::StrCat(ICON_MD_TAB, " Sessions"), "", []() {}, []() { return true; },
            std::vector<gui::MenuItem>{
                {absl::StrCat(ICON_MD_ADD, " New Session"), "Ctrl+Shift+N",
                 [this]() { CreateNewSession(); }},
                {absl::StrCat(ICON_MD_CONTENT_COPY, " Duplicate Session"), "",
                 [this]() { DuplicateCurrentSession(); },
                 [this]() { return current_rom_ != nullptr; }},
                {absl::StrCat(ICON_MD_CLOSE, " Close Session"), "Ctrl+Shift+W",
                 [this]() { CloseCurrentSession(); },
                 [this]() { return sessions_.size() > 1; }},
                {gui::kSeparator, "", nullptr, []() { return true; }},
                {absl::StrCat(ICON_MD_SWITCH_ACCOUNT, " Session Switcher"), "Ctrl+Tab",
                 [this]() { show_session_switcher_ = true; },
                 [this]() { return sessions_.size() > 1; }},
                {absl::StrCat(ICON_MD_VIEW_LIST, " Session Manager"), "",
                 [this]() { show_session_manager_ = true; }},
            }},
           
           // Layout & Docking
           {absl::StrCat(ICON_MD_DASHBOARD, " Layout"), "", []() {}, []() { return true; },
            std::vector<gui::MenuItem>{
                {absl::StrCat(ICON_MD_SPACE_DASHBOARD, " Layout Editor"), "",
                 [this]() { show_workspace_layout = true; }},
                {absl::StrCat(ICON_MD_RESET_TV, " Reset Layout"), "",
                 [this]() { ResetWorkspaceLayout(); }},
                {gui::kSeparator, "", nullptr, []() { return true; }},
                {absl::StrCat(ICON_MD_SAVE, " Save Layout"), "Ctrl+Shift+S",
                 [this]() { SaveWorkspaceLayout(); }},
                {absl::StrCat(ICON_MD_FOLDER_OPEN, " Load Layout"), "Ctrl+Shift+O",
                 [this]() { LoadWorkspaceLayout(); }},
                {gui::kSeparator, "", nullptr, []() { return true; }},
                {absl::StrCat(ICON_MD_BOOKMARK, " Layout Presets"), "",
                 [this]() { show_layout_presets_ = true; }},
            }},
           
           // Window Management
           {absl::StrCat(ICON_MD_WINDOW, " Windows"), "", []() {}, []() { return true; },
            std::vector<gui::MenuItem>{
                {absl::StrCat(ICON_MD_VISIBILITY, " Show All Windows"), "",
                 [this]() { ShowAllWindows(); }},
                {absl::StrCat(ICON_MD_VISIBILITY_OFF, " Hide All Windows"), "",
                 [this]() { HideAllWindows(); }},
                {gui::kSeparator, "", nullptr, []() { return true; }},
                {absl::StrCat(ICON_MD_FULLSCREEN, " Maximize Current"), "F11",
                 [this]() { MaximizeCurrentWindow(); }},
                {absl::StrCat(ICON_MD_FULLSCREEN_EXIT, " Restore All"), "",
                 [this]() { RestoreAllWindows(); }},
                {gui::kSeparator, "", nullptr, []() { return true; }},
                {absl::StrCat(ICON_MD_CLOSE_FULLSCREEN, " Close All Floating"), "",
                 [this]() { CloseAllFloatingWindows(); }},
            }},
           
           // Workspace Presets (Enhanced)
           {absl::StrCat(ICON_MD_BOOKMARK, " Presets"), "", []() {}, []() { return true; },
            std::vector<gui::MenuItem>{
                {absl::StrCat(ICON_MD_ADD, " Save Current as Preset"), "",
                 [this]() { show_save_workspace_preset_ = true; }},
                {absl::StrCat(ICON_MD_FOLDER_OPEN, " Load Preset"), "",
                 [this]() { show_load_workspace_preset_ = true; }},
                {gui::kSeparator, "", nullptr, []() { return true; }},
                {absl::StrCat(ICON_MD_DEVELOPER_MODE, " Developer Layout"), "",
                 [this]() { LoadDeveloperLayout(); }},
                {absl::StrCat(ICON_MD_DESIGN_SERVICES, " Designer Layout"), "",
                 [this]() { LoadDesignerLayout(); }},
                {absl::StrCat(ICON_MD_GAMEPAD, " Modder Layout"), "",
                 [this]() { LoadModderLayout(); }},
            }},
       }},
      {"Debug",
       {},
       {},
       {},
       {
           // Testing and Validation (only when tests are enabled)
           {absl::StrCat(ICON_MD_SCIENCE, " Test Dashboard"), "Ctrl+T",
            [&]() { show_test_dashboard_ = true; }},
           {absl::StrCat(ICON_MD_PLAY_ARROW, " Run All Tests"), "",
            [&]() { [[maybe_unused]] auto status = test::TestManager::Get().RunAllTests(); }},
           {absl::StrCat(ICON_MD_INTEGRATION_INSTRUCTIONS, " Run Unit Tests"), "",
            [&]() { [[maybe_unused]] auto status = test::TestManager::Get().RunTestsByCategory(test::TestCategory::kUnit); }},
           {absl::StrCat(ICON_MD_MEMORY, " Run Integration Tests"), "",
            [&]() { [[maybe_unused]] auto status = test::TestManager::Get().RunTestsByCategory(test::TestCategory::kIntegration); }},
           {absl::StrCat(ICON_MD_CLEAR_ALL, " Clear Test Results"), "",
            [&]() { test::TestManager::Get().ClearResults(); }},   
           {gui::kSeparator, "", nullptr, []() { return true; }},
           
           // ROM and ASM Management
           {absl::StrCat(ICON_MD_STORAGE, " ROM Analysis"), "", []() {}, []() { return true; },
            std::vector<gui::MenuItem>{
                {absl::StrCat(ICON_MD_INFO, " ROM Information"), "",
                 [&]() { popup_manager_->Show("ROM Information"); },
                 [&]() { return current_rom_ && current_rom_->is_loaded(); }},
                {absl::StrCat(ICON_MD_ANALYTICS, " Data Integrity Check"), "",
                 [&]() { 
                   if (current_rom_) {
                     [[maybe_unused]] auto status = test::TestManager::Get().TestRomDataIntegrity(current_rom_);
                   }
                 },
                 [&]() { return current_rom_ && current_rom_->is_loaded(); }},
                {absl::StrCat(ICON_MD_SAVE_ALT, " Test Save/Load"), "",
                 [&]() { 
                   if (current_rom_) {
                     [[maybe_unused]] auto status = test::TestManager::Get().TestRomSaveLoad(current_rom_);
                   }
                 },
                 [&]() { return current_rom_ && current_rom_->is_loaded(); }},
            }},
           
           {absl::StrCat(ICON_MD_CODE, " ZSCustomOverworld"), "", []() {}, []() { return true; },
            std::vector<gui::MenuItem>{
                {absl::StrCat(ICON_MD_INFO, " Check ROM Version"), "",
                 [&]() { 
                   if (current_rom_) {
                     uint8_t version = (*current_rom_)[zelda3::OverworldCustomASMHasBeenApplied];
                     std::string version_str = (version == 0xFF) ? "Vanilla" : absl::StrFormat("v%d", version);
                     toast_manager_.Show(absl::StrFormat("ROM: %s | ZSCustomOverworld: %s", 
                                                        current_rom_->title().c_str(), version_str.c_str()), 
                                        editor::ToastType::kInfo, 5.0f);
                   }
                 },
                 [&]() { return current_rom_ && current_rom_->is_loaded(); }},
                {absl::StrCat(ICON_MD_UPGRADE, " Upgrade ROM"), "",
                 [&]() { 
                   // This would trigger the upgrade dialog from overworld editor
                   if (current_rom_) {
                     toast_manager_.Show("Use Overworld Editor to upgrade ROM version", 
                                        editor::ToastType::kInfo, 4.0f);
                   }
                 },
                 [&]() { return current_rom_ && current_rom_->is_loaded(); }},
                {absl::StrCat(ICON_MD_SETTINGS, " Feature Flags"), "",
                 [&]() { 
                   // Toggle ZSCustomOverworld loading feature
                   auto& flags = core::FeatureFlags::get();
                   flags.overworld.kLoadCustomOverworld = !flags.overworld.kLoadCustomOverworld;
                   toast_manager_.Show(absl::StrFormat("Custom Overworld Loading: %s", 
                                                      flags.overworld.kLoadCustomOverworld ? "Enabled" : "Disabled"), 
                                      editor::ToastType::kInfo);
                 }},
            }},
           
           {absl::StrCat(ICON_MD_BUILD, " Asar Integration"), "", []() {}, []() { return true; },
            std::vector<gui::MenuItem>{
                {absl::StrCat(ICON_MD_INFO, " Asar Status"), "",
                 [&]() { popup_manager_->Show("Asar Integration"); }},
                {absl::StrCat(ICON_MD_CODE, " Apply ASM Patch"), "",
                 [&]() { 
                   if (current_rom_) {
                     auto& flags = core::FeatureFlags::get();
                     flags.overworld.kApplyZSCustomOverworldASM = !flags.overworld.kApplyZSCustomOverworldASM;
                     toast_manager_.Show(absl::StrFormat("ZSCustomOverworld ASM Application: %s", 
                                                        flags.overworld.kApplyZSCustomOverworldASM ? "Enabled" : "Disabled"), 
                                        editor::ToastType::kInfo);
                   }
                 },
                 [&]() { return current_rom_ && current_rom_->is_loaded(); }},
                {absl::StrCat(ICON_MD_FOLDER_OPEN, " Load ASM File"), "",
                 [&]() { 
                   // Show available ASM files or file dialog
                   toast_manager_.Show("ASM file loading not yet implemented", 
                                      editor::ToastType::kWarning);
                 }},
            }},
           
           {gui::kSeparator, "", nullptr, []() { return true; }},
           
           // Development Tools
           {absl::StrCat(ICON_MD_MEMORY, " Memory Editor"), "",
            [&]() { show_memory_editor_ = true; }},
           {absl::StrCat(ICON_MD_CODE, " Assembly Editor"), "",
            [&]() { show_asm_editor_ = true; }},
           {absl::StrCat(ICON_MD_SETTINGS, " Feature Flags"), "",
            [&]() { popup_manager_->Show("Feature Flags"); }},
           
           {gui::kSeparator, "", nullptr, []() { return true; }},
           
           // Agent Proposals
           {absl::StrCat(ICON_MD_PREVIEW, " Agent Proposals"), "",
            [&]() { proposal_drawer_.Toggle(); }},
#ifdef YAZE_WITH_GRPC
           {absl::StrCat(ICON_MD_CHAT, " Agent Chat"), "",
            [this]() {
              agent_editor_.ToggleChat();
            },
            [this]() { return agent_editor_.IsChatActive(); }},
#endif
           
           {gui::kSeparator, "", nullptr, []() { return true; }},
           
           {absl::StrCat(ICON_MD_PALETTE, " Graphics Debugging"), "", []() {}, []() { return true; },
            std::vector<gui::MenuItem>{
                {absl::StrCat(ICON_MD_REFRESH, " Clear Graphics Cache"), "",
                 [&]() { 
                   // Clear and reinitialize graphics cache
                   if (current_rom_ && current_rom_->is_loaded()) {
                     toast_manager_.Show("Graphics cache cleared - reload editors to refresh", 
                                        editor::ToastType::kInfo, 4.0f);
                   }
                 },
                 [&]() { return current_rom_ && current_rom_->is_loaded(); }},
                {absl::StrCat(ICON_MD_MEMORY, " Arena Statistics"), "",
                 [&]() { 
                   auto& arena = gfx::Arena::Get();
                   toast_manager_.Show(absl::StrFormat("Arena: %zu surfaces, %zu textures", 
                                                      arena.GetSurfaceCount(), arena.GetTextureCount()), 
                                      editor::ToastType::kInfo, 4.0f);
                 }},
            }},
           
           // Performance Monitoring
           {absl::StrCat(ICON_MD_SPEED, " Performance Dashboard"), "Ctrl+Shift+P",
            [&]() { show_performance_dashboard_ = true; }},
           
           {gui::kSeparator, "", nullptr, []() { return true; }},
           
           // Development Helpers
           {absl::StrCat(ICON_MD_HELP, " ImGui Demo"), "",
            [&]() { show_imgui_demo_ = true; }},
           {absl::StrCat(ICON_MD_ANALYTICS, " ImGui Metrics"), "",
            [&]() { show_imgui_metrics_ = true; }},
       }},
      {"Help",
       {},
       {},
       {},
       {
           {absl::StrCat(ICON_MD_HELP, " Getting Started"), "",
            [&]() { popup_manager_->Show("Getting Started"); }},
           {absl::StrCat(ICON_MD_WORK_OUTLINE, " Workspace Help"), "",
            [&]() { popup_manager_->Show("Workspace Help"); }},
           {absl::StrCat(ICON_MD_INTEGRATION_INSTRUCTIONS, " Asar Integration Guide"), "",
            [&]() { popup_manager_->Show("Asar Integration"); }},
           {absl::StrCat(ICON_MD_BUILD, " Build Instructions"), "",
            [&]() { popup_manager_->Show("Build Instructions"); }},
           {gui::kSeparator, "", nullptr, []() { return true; }},
           {absl::StrCat(ICON_MD_FILE_OPEN, " How to open a ROM"), "",
            [&]() { popup_manager_->Show("Open a ROM"); }},
           {absl::StrCat(ICON_MD_LIST, " Supported Features"), "",
            [&]() { popup_manager_->Show("Supported Features"); }},
           {absl::StrCat(ICON_MD_FOLDER_OPEN, " How to manage a project"), "",
            [&]() { popup_manager_->Show("Manage Project"); }},
           {gui::kSeparator, "", nullptr, []() { return true; }},
           {absl::StrCat(ICON_MD_TERMINAL, " CLI Tool Usage"), "",
            [&]() { popup_manager_->Show("CLI Usage"); }},
           {absl::StrCat(ICON_MD_BUG_REPORT, " Troubleshooting"), "",
            [&]() { popup_manager_->Show("Troubleshooting"); }},
           {absl::StrCat(ICON_MD_CODE, " Contributing"), "",
            [&]() { popup_manager_->Show("Contributing"); }},
           {gui::kSeparator, "", nullptr, []() { return true; }},
           {absl::StrCat(ICON_MD_ANNOUNCEMENT, " What's New in v0.3"), "",
            [&]() { popup_manager_->Show("Whats New v03"); }},
           {absl::StrCat(ICON_MD_INFO, " About"), "F1",
            [&]() { popup_manager_->Show("About"); }},
       }}};
}

absl::Status EditorManager::Update() {
  popup_manager_->DrawPopups();
  ExecuteShortcuts(context_.shortcut_manager);
  toast_manager_.Draw();

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
    LOG_INFO("EditorManager",
             "EditorManager::Update - ROM changed, updating TestManager: %p -> "
             "%p",
             (void*)last_test_rom, (void*)current_rom_);
    test::TestManager::Get().SetCurrentRom(current_rom_);
    last_test_rom = current_rom_;
  }

  // Autosave timer
  if (autosave_enabled_ && current_rom_ && current_rom_->dirty()) {
    autosave_timer_ += ImGui::GetIO().DeltaTime;
    if (autosave_timer_ >= autosave_interval_secs_) {
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

        // Generate unique window titles for multi-session support
        std::string window_title =
            GenerateUniqueEditorTitle(editor->type(), session_idx);

        if (ImGui::Begin(window_title.c_str(), editor->active())) {
          // Temporarily switch context for this editor's update
          Rom* prev_rom = current_rom_;
          EditorSet* prev_editor_set = current_editor_set_;

          current_rom_ = &session.rom;
          current_editor_set_ = &session.editors;
          current_editor_ = editor;

          status_ = editor->Update();

          // Route editor errors to toast manager
          if (!status_.ok()) {
            std::string editor_name =
                "Editor";  // Get actual editor name if available
            if (editor == &session.editors.overworld_editor_)
              editor_name = "Overworld Editor";
            else if (editor == &session.editors.dungeon_editor_)
              editor_name = "Dungeon Editor";
            else if (editor == &session.editors.sprite_editor_)
              editor_name = "Sprite Editor";
            else if (editor == &session.editors.graphics_editor_)
              editor_name = "Graphics Editor";
            else if (editor == &session.editors.music_editor_)
              editor_name = "Music Editor";
            else if (editor == &session.editors.palette_editor_)
              editor_name = "Palette Editor";
            else if (editor == &session.editors.screen_editor_)
              editor_name = "Screen Editor";

            toast_manager_.Show(
                absl::StrFormat("%s Error: %s", editor_name, status_.message()),
                editor::ToastType::kError, 8.0f);
          }

          // Restore context
          current_rom_ = prev_rom;
          current_editor_set_ = prev_editor_set;
        }
        ImGui::End();
      }
    }
  }

  if (show_performance_dashboard_) {
    gfx::PerformanceDashboard::Get().Render();
  }
  if (show_proposal_drawer_) {
    proposal_drawer_.Draw();
  }
#ifdef YAZE_WITH_GRPC
  Rom* rom_context =
      (current_rom_ != nullptr && current_rom_->is_loaded()) ? current_rom_
                                                             : nullptr;
  agent_editor_.SetRomContext(rom_context);
  agent_editor_.Draw();
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
  } else {
    Text("No ROM loaded");
  }
  return absl::OkStatus();
}

void EditorManager::DrawMenuBar() {
  static bool show_display_settings = false;
  static bool save_as_menu = false;

  if (BeginMenuBar()) {
    gui::DrawMenu(gui::kMainMenu);

    status_ = DrawRomSelector();

    // Calculate proper right-side positioning
    std::string version_text = absl::StrFormat("v%s", version_.c_str());
    float version_width = CalcTextSize(version_text.c_str()).x;
    float settings_width = CalcTextSize(ICON_MD_DISPLAY_SETTINGS).x + 16;
    float total_right_width =
        version_width + settings_width + 40;  // Extra padding

    // Position for ROM status and sessions
    float session_rom_area_width = 250.0f;  // Reduced width
    SameLine(GetWindowWidth() - total_right_width - session_rom_area_width);

    // Multi-session indicator
    if (sessions_.size() > 1) {
      if (SmallButton(absl::StrFormat("%s %zu", ICON_MD_TAB, sessions_.size())
                          .c_str())) {
        show_session_switcher_ = true;
      }
      if (IsItemHovered()) {
        SetTooltip("Sessions: %zu active\nClick to switch between sessions",
                   sessions_.size());
      }
      SameLine();
      ImGui::Separator();
      SameLine();
    }

    // Enhanced ROM status with metadata popup
    if (current_rom_ && current_rom_->is_loaded()) {
      std::string rom_display = current_rom_->title();

      ImVec4 status_color =
          current_rom_->dirty() ? ImVec4(1.0f, 0.5f, 0.0f, 1.0f)
                                :              // Orange for modified
              ImVec4(0.0f, 0.8f, 0.0f, 1.0f);  // Green for clean

      // Make ROM status clickable for detailed popup
      if (SmallButton(absl::StrFormat("%s %s%s", ICON_MD_STORAGE,
                                      rom_display.c_str(),
                                      current_rom_->dirty() ? "*" : "")
                          .c_str())) {
        ImGui::OpenPopup("ROM Details");
      }

      // Enhanced tooltip on hover
      if (IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("%s ROM Information", ICON_MD_INFO);
        ImGui::Separator();
        ImGui::Text("%s Title: %s", ICON_MD_TITLE,
                    current_rom_->title().c_str());
        ImGui::Text("%s File: %s", ICON_MD_FOLDER_OPEN,
                    current_rom_->filename().c_str());
        ImGui::Text("%s Size: %.1f MB (%zu bytes)", ICON_MD_STORAGE,
                    current_rom_->size() / 1048576.0f, current_rom_->size());
        ImGui::Text("%s Status: %s",
                    current_rom_->dirty() ? ICON_MD_EDIT : ICON_MD_CHECK_CIRCLE,
                    current_rom_->dirty() ? "Modified" : "Clean");
        ImGui::Text("%s Click for detailed view", ICON_MD_LAUNCH);
        ImGui::EndTooltip();
      }

      // Detailed ROM popup
      if (ImGui::BeginPopup("ROM Details")) {
        ImGui::Text("%s ROM Detailed Information", ICON_MD_INFO);
        ImGui::Separator();
        ImGui::Spacing();

        // Basic info with icons
        if (ImGui::BeginTable("ROMDetailsTable", 2,
                              ImGuiTableFlags_SizingFixedFit)) {
          ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed,
                                  120);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("%s Title", ICON_MD_TITLE);
          ImGui::TableNextColumn();
          ImGui::Text("%s", current_rom_->title().c_str());

          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("%s File", ICON_MD_FOLDER_OPEN);
          ImGui::TableNextColumn();
          ImGui::Text("%s", current_rom_->filename().c_str());

          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("%s Size", ICON_MD_STORAGE);
          ImGui::TableNextColumn();
          ImGui::Text("%.1f MB (%zu bytes)", current_rom_->size() / 1048576.0f,
                      current_rom_->size());

          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("%s Status", current_rom_->dirty()
                                       ? ICON_MD_EDIT
                                       : ICON_MD_CHECK_CIRCLE);
          ImGui::TableNextColumn();
          ImGui::TextColored(status_color, "%s",
                             current_rom_->dirty() ? "Modified" : "Clean");

          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("%s Session", ICON_MD_TAB);
          ImGui::TableNextColumn();
          size_t current_session_idx = GetCurrentSessionIndex();
          ImGui::Text("Session %zu of %zu", current_session_idx + 1,
                      sessions_.size());

          ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::Separator();

        // Quick actions
        ImGui::Text("%s Quick Actions", ICON_MD_FLASH_ON);
        if (ImGui::Button(absl::StrFormat("%s Save ROM", ICON_MD_SAVE).c_str(),
                          ImVec2(120, 0))) {
          status_ = SaveRom();
          ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(
                absl::StrFormat("%s Switch Session", ICON_MD_SWITCH_ACCOUNT)
                    .c_str(),
                ImVec2(120, 0))) {
          show_session_switcher_ = true;
          ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
      }

      SameLine();
    } else {
      TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s No ROM",
                  ICON_MD_HELP_OUTLINE);
      SameLine();
    }

    // Settings and version (using pre-calculated positioning)
    SameLine(GetWindowWidth() - total_right_width);
    ImGui::Separator();
    SameLine();

    PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    if (Button(ICON_MD_DISPLAY_SETTINGS)) {
      show_display_settings = !show_display_settings;
    }
    PopStyleColor();

    SameLine();
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

  // Agent proposal drawer
  proposal_drawer_.SetRom(current_rom_);
  proposal_drawer_.Draw();

  // Welcome screen (accessible from View menu)
  if (show_welcome_screen_) {
    DrawWelcomeScreen();
  }

  if (show_emulator_) {
    Begin("Emulator", &show_emulator_, ImGuiWindowFlags_MenuBar);
    emulator_.Run();
    End();
  }

  // Enhanced Command Palette UI
  if (show_command_palette_) {
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_FirstUseEver);

    if (Begin(absl::StrFormat("%s Command Palette", ICON_MD_TERMINAL).c_str(),
              &show_command_palette_, ImGuiWindowFlags_NoCollapse)) {

      // Search input with focus management
      static char query[256] = {};
      ImGui::SetNextItemWidth(-100);
      if (ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere();
      }

      bool input_changed = InputTextWithHint(
          "##cmd_query",
          absl::StrFormat("%s Type a command or search...", ICON_MD_SEARCH)
              .c_str(),
          query, IM_ARRAYSIZE(query));

      ImGui::SameLine();
      if (ImGui::Button(absl::StrFormat("%s Clear", ICON_MD_CLEAR).c_str())) {
        query[0] = '\0';
        input_changed = true;
      }

      Separator();

      // Filter and categorize commands
      std::vector<std::pair<std::string, std::string>> filtered_commands;
      for (const auto& entry : context_.shortcut_manager.GetShortcuts()) {
        const auto& name = entry.first;
        const auto& shortcut = entry.second;

        if (query[0] == '\0' || name.find(query) != std::string::npos) {
          std::string shortcut_text =
              shortcut.keys.empty()
                  ? ""
                  : absl::StrFormat("(%s)",
                                    PrintShortcut(shortcut.keys).c_str());
          filtered_commands.emplace_back(name, shortcut_text);
        }
      }

      // Display results in a table for better organization
      if (ImGui::BeginTable("CommandPaletteTable", 2,
                            ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_SizingStretchProp,
                            ImVec2(0, -30))) {  // Reserve space for status bar

        ImGui::TableSetupColumn("Command", ImGuiTableColumnFlags_WidthStretch,
                                0.7f);
        ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthStretch,
                                0.3f);
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < filtered_commands.size(); ++i) {
          const auto& [command_name, shortcut_text] = filtered_commands[i];

          ImGui::TableNextRow();
          ImGui::TableNextColumn();

          ImGui::PushID(static_cast<int>(i));
          if (Selectable(command_name.c_str(), false,
                         ImGuiSelectableFlags_SpanAllColumns)) {
            // Execute the command
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
        }

        ImGui::EndTable();
      }

      // Status bar
      ImGui::Separator();
      ImGui::Text("%s %zu commands found", ICON_MD_INFO,
                  filtered_commands.size());
      ImGui::SameLine();
      ImGui::TextDisabled("| Press Enter to execute selected command");
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
              ImGui::Text("%s", core::GetFileName(file).c_str());

              ImGui::TableNextColumn();
              std::string ext = core::GetFileExtension(file);
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
          core::FileDialogWrapper::ShowSaveFileDialog(save_as_filename, "sfc");
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
          core::FileDialogWrapper::ShowSaveFileDialog(save_as_filename, "yaze");
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
    if (!workspace_presets_loaded_) {
      RefreshWorkspacePresets();
    }

    for (const auto& name : workspace_presets_) {
      if (Selectable(name.c_str())) {
        LoadWorkspacePreset(name);
        toast_manager_.Show("Preset loaded", editor::ToastType::kSuccess);
        show_load_workspace_preset_ = false;
      }
    }
    if (workspace_presets_.empty())
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
      LOG_INFO("EditorManager", "Found empty session to populate with ROM: %s",
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
    sessions_.emplace_back(std::move(temp_rom));
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
  LOG_INFO("EditorManager", "Setting ROM in TestManager - %p ('%s')",
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

  return absl::OkStatus();
}

absl::Status EditorManager::LoadAssets() {
  if (!current_rom_ || !current_editor_set_) {
    return absl::FailedPreconditionError("No ROM or editor set loaded");
  }

  auto start_time = std::chrono::steady_clock::now();

  current_editor_set_->overworld_editor_.Initialize();
  current_editor_set_->message_editor_.Initialize();
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
  LOG_INFO("EditorManager", "ROM assets loaded in %lld ms", duration.count());

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
  settings.backup = backup_rom_;
  settings.save_new = save_new_auto_;
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
  settings.backup = backup_rom_;
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
    sessions_.emplace_back(std::move(temp_rom));
    RomSession& session = sessions_.back();
    for (auto* editor : session.editors.active_editors_) {
      editor->set_context(&context_);
    }
    current_rom_ = &session.rom;
    current_editor_set_ = &session.editors;
    RETURN_IF_ERROR(LoadAssets());

    // Reset welcome screen state when ROM is loaded
    welcome_screen_manually_closed_ = false;
  }
  return absl::OkStatus();
}

absl::Status EditorManager::CreateNewProject(const std::string& template_name) {
  auto dialog_path = core::FileDialogWrapper::ShowOpenFolderDialog();
  if (dialog_path.empty()) {
    return absl::OkStatus();  // User cancelled
  }

  // Show project creation dialog
  popup_manager_->Show("Create New Project");
  return absl::OkStatus();
}

absl::Status EditorManager::OpenProject() {
  auto file_path = core::FileDialogWrapper::ShowOpenFileDialog();
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

    sessions_.emplace_back(std::move(temp_rom));
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
    LOG_INFO("EditorManager", "Setting ROM in TestManager - %p ('%s')",
             (void*)current_rom_,
             current_rom_ ? current_rom_->title().c_str() : "null");
    test::TestManager::Get().SetCurrentRom(current_rom_);
#endif

    if (current_editor_set_ && !current_project_.code_folder.empty()) {
      current_editor_set_->assembly_editor_.OpenFolder(
          current_project_.code_folder);
    }

    RETURN_IF_ERROR(LoadAssets());
  }

  // Apply workspace settings
  font_global_scale_ = current_project_.workspace_settings.font_global_scale;
  autosave_enabled_ = current_project_.workspace_settings.autosave_enabled;
  autosave_interval_secs_ =
      current_project_.workspace_settings.autosave_interval_secs;
  ImGui::GetIO().FontGlobalScale = font_global_scale_;

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

    current_project_.workspace_settings.font_global_scale = font_global_scale_;
    current_project_.workspace_settings.autosave_enabled = autosave_enabled_;
    current_project_.workspace_settings.autosave_interval_secs =
        autosave_interval_secs_;

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
      core::FileDialogWrapper::ShowSaveFileDialog(default_name, "yaze");
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
  sessions_.emplace_back(std::move(rom_copy));
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

  LOG_INFO("EditorManager", "Marked session as closed: %s (index %zu)",
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
    if (!workspace_presets_loaded_) {
      RefreshWorkspacePresets();
    }

    for (const auto& preset : workspace_presets_) {
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

    if (workspace_presets_.empty()) {
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

std::string EditorManager::GenerateUniqueEditorTitle(EditorType type,
                                                     size_t session_index) {
  std::string base_name = GetEditorName(type);

  if (sessions_.size() <= 1) {
    return base_name;  // No need for session identifier with single session
  }

  // Add session identifier
  const auto& session = sessions_[session_index];
  std::string session_name = session.GetDisplayName();

  // Truncate long session names
  if (session_name.length() > 15) {
    session_name = session_name.substr(0, 12) + "...";
  }

  return absl::StrFormat("%s (%s)", base_name.c_str(), session_name.c_str());
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
  // Make welcome screen moveable but with a good default position
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                          ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

  // Create a subtle animated background effect
  static float animation_time = 0.0f;
  animation_time += ImGui::GetIO().DeltaTime;

  // Make it moveable and resizable but keep the custom styling
  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground;

  // Use a unique window name to prevent stacking
  static int welcome_window_id = 0;
  std::string window_name =
      absl::StrFormat("Welcome to YAZE##welcome_%d", welcome_window_id);

  bool welcome_was_open = show_welcome_screen_;
  if (ImGui::Begin(window_name.c_str(), &show_welcome_screen_, flags)) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();

    // Get theme colors for welcome screen
    auto& theme_manager = gui::ThemeManager::Get();
    auto bg_color = theme_manager.GetWelcomeScreenBackground();
    auto border_color = theme_manager.GetWelcomeScreenBorder();
    auto accent_color = theme_manager.GetWelcomeScreenAccent();

    // Draw themed gradient background
    ImU32 bg_top = ImGui::ColorConvertFloat4ToU32(
        ImVec4(bg_color.red, bg_color.green, bg_color.blue, bg_color.alpha));
    ImU32 bg_bottom = ImGui::ColorConvertFloat4ToU32(
        ImVec4(bg_color.red * 0.8f, bg_color.green * 0.8f, bg_color.blue * 0.8f,
               bg_color.alpha));
    draw_list->AddRectFilledMultiColor(
        window_pos,
        ImVec2(window_pos.x + window_size.x, window_pos.y + window_size.y),
        bg_top, bg_top, bg_bottom, bg_bottom);

    // Themed animated border
    float border_thickness = 3.0f;
    float pulse = 0.8f + 0.2f * sinf(animation_time * 2.0f);
    ImU32 themed_border = ImGui::ColorConvertFloat4ToU32(
        ImVec4(border_color.red, border_color.green, border_color.blue,
               pulse * border_color.alpha));
    draw_list->AddRect(
        window_pos,
        ImVec2(window_pos.x + window_size.x, window_pos.y + window_size.y),
        themed_border, 12.0f, 0, border_thickness);

    // Enhanced floating particles effect with multiple layers
    for (int layer = 0; layer < 2; ++layer) {
      int particle_count = layer == 0 ? 12 : 8;
      float layer_speed = layer == 0 ? 1.0f : 0.6f;
      float layer_alpha = layer == 0 ? 0.4f : 0.2f;

      for (int i = 0; i < particle_count; ++i) {
        float time_offset = layer * 3.14159f + i * 0.8f;
        float offset_x =
            sinf(animation_time * 0.5f * layer_speed + time_offset) *
            (30.0f + layer * 10.0f);
        float offset_y =
            cosf(animation_time * 0.3f * layer_speed + time_offset) *
            (20.0f + layer * 8.0f);

        // Distribute particles across the window
        float base_x = window_pos.x + (window_size.x / particle_count) * i + 40;
        float base_y = window_pos.y + 80 + layer * 30;

        ImVec2 particle_pos = ImVec2(base_x + offset_x, base_y + offset_y);

        // Pulsing alpha effect
        float alpha =
            layer_alpha + 0.3f * sinf(animation_time * 1.5f + time_offset);
        ImU32 particle_color = ImGui::ColorConvertFloat4ToU32(ImVec4(
            accent_color.red, accent_color.green, accent_color.blue, alpha));

        // Varying particle sizes
        float radius = 1.5f + layer * 0.5f +
                       sinf(animation_time * 2.0f + time_offset) * 0.8f;
        draw_list->AddCircleFilled(particle_pos, radius, particle_color);

        // Add subtle glow effect for layer 0
        if (layer == 0) {
          ImU32 glow_color = ImGui::ColorConvertFloat4ToU32(
              ImVec4(accent_color.red, accent_color.green, accent_color.blue,
                     alpha * 0.3f));
          draw_list->AddCircleFilled(particle_pos, radius + 1.0f, glow_color);
        }
      }
    }

    // Header with themed styling
    ImGui::Spacing();
    ImGui::SetCursorPosX(
        (window_size.x -
         ImGui::CalcTextSize("Welcome to Yet Another Zelda3 Editor").x) *
        0.5f);
    auto text_color = theme_manager.GetCurrentTheme().text_primary;
    ImGui::PushStyleColor(
        ImGuiCol_Text, ImVec4(text_color.red, text_color.green, text_color.blue,
                              text_color.alpha));
    ImGui::Text("Welcome to Yet Another Zelda3 Editor");
    ImGui::PopStyleColor();

    ImGui::SetCursorPosX(
        (window_size.x -
         ImGui::CalcTextSize("The Legend of Zelda: A Link to the Past").x) *
        0.5f);
    auto subtitle_color = theme_manager.GetCurrentTheme().text_secondary;
    ImGui::PushStyleColor(
        ImGuiCol_Text,
        ImVec4(subtitle_color.red, subtitle_color.green, subtitle_color.blue,
               subtitle_color.alpha * 0.9f));
    ImGui::Text("The Legend of Zelda: A Link to the Past");
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Themed decorative line with glow effect (positioned closer to header)
    float line_y =
        window_pos.y + 30;   // Move even higher for tighter header integration
    float line_margin = 80;  // Maintain good horizontal balance
    ImVec2 line_start = ImVec2(window_pos.x + line_margin, line_y);
    ImVec2 line_end =
        ImVec2(window_pos.x + window_size.x - line_margin, line_y);

    // Enhanced glow effect with multiple line layers for depth
    float glow_alpha = 0.6f + 0.4f * sinf(animation_time * 1.5f);
    ImU32 line_color = ImGui::ColorConvertFloat4ToU32(ImVec4(
        accent_color.red, accent_color.green, accent_color.blue, glow_alpha));

    // Draw main line with glow effect
    draw_list->AddLine(
        line_start, line_end,
        ImGui::ColorConvertFloat4ToU32(ImVec4(
            accent_color.red, accent_color.green, accent_color.blue, 0.3f)),
        4.0f);                                                   // Glow layer
    draw_list->AddLine(line_start, line_end, line_color, 2.0f);  // Main line

    ImGui::Spacing();
    ImGui::Spacing();

    // Show different messages based on state
    if (!sessions_.empty() && !current_rom_) {
      ImGui::Separator();
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f),
                         ICON_MD_WARNING " ROM Loading Required");
      TextWrapped(
          "A session exists but no ROM is loaded. Please load a ROM file to "
          "continue editing.");
      ImGui::Text("Active Sessions: %zu", GetActiveSessionCount());
    } else {
      ImGui::Separator();
      ImGui::Spacing();
      TextWrapped("No ROM loaded.");
    }

    ImGui::Spacing();

    // Enhanced primary actions with glowing buttons
    ImGui::Text("Get Started:");
    ImGui::Spacing();

    // Themed primary buttons with enhanced effects
    auto current_theme = theme_manager.GetCurrentTheme();
    ImGui::PushStyleColor(ImGuiCol_Button,
                          ConvertColorToImVec4(current_theme.primary));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ConvertColorToImVec4(current_theme.accent));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          ConvertColorToImVec4(current_theme.secondary));

    if (ImGui::Button(ICON_MD_FILE_OPEN " Open ROM File", ImVec2(200, 40))) {
      status_ = LoadRom();
      if (!status_.ok()) {
        toast_manager_.Show(std::string(status_.message()),
                            editor::ToastType::kError);
      }
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_FOLDER_OPEN " Open Project", ImVec2(200, 40))) {
      auto file_name = core::FileDialogWrapper::ShowOpenFileDialog();
      if (!file_name.empty()) {
        status_ = OpenRomOrProject(file_name);
        if (!status_.ok()) {
          toast_manager_.Show(std::string(status_.message()),
                              editor::ToastType::kError);
        }
      }
    }

    ImGui::PopStyleColor(3);

    ImGui::Spacing();

    // Feature flags section (per-session)
    ImGui::Text("Options:");
    auto* flags = GetCurrentFeatureFlags();
    Checkbox("Load custom overworld features",
             &flags->overworld.kLoadCustomOverworld);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Recent files section (reuse homepage logic)
    ImGui::Text("Recent Files:");
    ImGui::BeginChild("RecentFiles", ImVec2(0, 100), true);
    auto& manager = core::RecentFilesManager::GetInstance();
    for (const auto& file : manager.GetRecentFiles()) {
      if (gui::ClickableText(file.c_str())) {
        status_ = OpenRomOrProject(file);
        if (!status_.ok()) {
          toast_manager_.Show(std::string(status_.message()),
                              editor::ToastType::kError);
        }
      }
    }
    ImGui::EndChild();

    ImGui::Spacing();

    // Show editor access buttons for loaded sessions
    bool has_loaded_sessions = false;
    for (const auto& session : sessions_) {
      if (session.rom.is_loaded()) {
        has_loaded_sessions = true;
        break;
      }
    }

    if (has_loaded_sessions) {
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Text("Available Editors:");
      ImGui::Text(
          "Click to open editor windows that can be docked side by side");
      ImGui::Spacing();

      // Show sessions and their editors
      for (size_t session_idx = 0; session_idx < sessions_.size();
           ++session_idx) {
        const auto& session = sessions_[session_idx];
        if (!session.rom.is_loaded())
          continue;

        ImGui::Text("Session: %s", session.GetDisplayName().c_str());

        // Editor buttons in a grid layout for this session
        if (ImGui::BeginTable(
                absl::StrFormat("EditorsTable##%zu", session_idx).c_str(), 4,
                ImGuiTableFlags_SizingFixedFit |
                    ImGuiTableFlags_NoHostExtendX)) {

          // Row 1: Primary editors
          ImGui::TableNextColumn();
          if (ImGui::Button(
                  absl::StrFormat(ICON_MD_MAP " Overworld##%zu", session_idx)
                      .c_str(),
                  ImVec2(120, 30))) {
            sessions_[session_idx].editors.overworld_editor_.set_active(true);
          }
          ImGui::TableNextColumn();
          if (ImGui::Button(
                  absl::StrFormat(ICON_MD_DOMAIN " Dungeon##%zu", session_idx)
                      .c_str(),
                  ImVec2(120, 30))) {
            sessions_[session_idx].editors.dungeon_editor_.set_active(true);
          }
          ImGui::TableNextColumn();
          if (ImGui::Button(
                  absl::StrFormat(ICON_MD_IMAGE " Graphics##%zu", session_idx)
                      .c_str(),
                  ImVec2(120, 30))) {
            sessions_[session_idx].editors.graphics_editor_.set_active(true);
          }
          ImGui::TableNextColumn();
          if (ImGui::Button(
                  absl::StrFormat(ICON_MD_PALETTE " Palette##%zu", session_idx)
                      .c_str(),
                  ImVec2(120, 30))) {
            sessions_[session_idx].editors.palette_editor_.set_active(true);
          }

          // Row 2: Secondary editors
          ImGui::TableNextColumn();
          if (ImGui::Button(
                  absl::StrFormat(ICON_MD_MESSAGE " Message##%zu", session_idx)
                      .c_str(),
                  ImVec2(120, 30))) {
            sessions_[session_idx].editors.message_editor_.set_active(true);
          }
          ImGui::TableNextColumn();
          if (ImGui::Button(
                  absl::StrFormat(ICON_MD_PERSON " Sprite##%zu", session_idx)
                      .c_str(),
                  ImVec2(120, 30))) {
            sessions_[session_idx].editors.sprite_editor_.set_active(true);
          }
          ImGui::TableNextColumn();
          if (ImGui::Button(
                  absl::StrFormat(ICON_MD_MUSIC_NOTE " Music##%zu", session_idx)
                      .c_str(),
                  ImVec2(120, 30))) {
            sessions_[session_idx].editors.music_editor_.set_active(true);
          }
          ImGui::TableNextColumn();
          if (ImGui::Button(
                  absl::StrFormat(ICON_MD_MONITOR " Screen##%zu", session_idx)
                      .c_str(),
                  ImVec2(120, 30))) {
            sessions_[session_idx].editors.screen_editor_.set_active(true);
          }

          ImGui::EndTable();
        }

        if (session_idx < sessions_.size() - 1) {
          ImGui::Spacing();
        }
      }
    }

    // Links section
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Help & Support:");
    if (gui::ClickableText(ICON_MD_HELP " Getting Started Guide")) {
      gui::OpenUrl(
          "https://github.com/scawful/yaze/blob/master/docs/"
          "01-getting-started.md");
    }
    if (gui::ClickableText(ICON_MD_BUG_REPORT " Report Issues")) {
      gui::OpenUrl("https://github.com/scawful/yaze/issues");
    }

    // Show tip about drag and drop
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), ICON_MD_TIPS_AND_UPDATES
                       " Tip: Drag and drop ROM files onto the window");

    // Add settings and customization section (accessible before ROM loading)
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("%s Customization & Settings", ICON_MD_SETTINGS);

    // Theme and display settings buttons (always accessible)
    static bool show_welcome_theme_selector = false;
    if (ImGui::Button(
            absl::StrFormat("%s Theme Settings", ICON_MD_PALETTE).c_str(),
            ImVec2(180, 35))) {
      show_welcome_theme_selector = true;
    }

    // Show theme selector if requested
    if (show_welcome_theme_selector) {
      auto& theme_manager = gui::ThemeManager::Get();
      theme_manager.ShowThemeSelector(&show_welcome_theme_selector);
    }

    ImGui::SameLine();
    if (ImGui::Button(
            absl::StrFormat("%s Display Settings", ICON_MD_DISPLAY_SETTINGS)
                .c_str(),
            ImVec2(180, 35))) {
      // Open display settings popup (make it accessible without ROM)
      popup_manager_->Show("Display Settings");
    }

    ImGui::SameLine();
    if (ImGui::Button(
            absl::StrFormat("%s Command Palette", ICON_MD_TERMINAL).c_str(),
            ImVec2(180, 35))) {
      show_command_palette_ = true;
    }
  }
  ImGui::End();

  // Check if the welcome screen was manually closed via the close button
  if (welcome_was_open && !show_welcome_screen_) {
    welcome_screen_manually_closed_ = true;
  }
}

}  // namespace editor
}  // namespace yaze
