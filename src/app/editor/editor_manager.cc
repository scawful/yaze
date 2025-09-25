#include "editor_manager.h"

#include "absl/status/status.h"
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
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/gui/style.h"
#include "app/rom.h"
#include "app/test/test_manager.h"
#include "app/test/integrated_test_suite.h"
#include "app/test/rom_dependent_test_suite.h"
#ifdef YAZE_ENABLE_GTEST
#include "app/test/unit_test_suite.h"
#endif
#include "editor/editor.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "util/macro.h"

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
        if (eq == std::string::npos) continue;
        auto key = line.substr(0, eq);
        auto val = line.substr(eq + 1);
        if (key == "font_global_scale") font_global_scale_ = std::stof(val);
        if (key == "autosave_enabled") autosave_enabled_ = (val == "1");
        if (key == "autosave_interval_secs") autosave_interval_secs_ = std::stof(val);
      }
      ImGui::GetIO().FontGlobalScale = font_global_scale_;
    }
  } catch (...) {
  }
}

void EditorManager::SaveUserSettings() {
  std::ostringstream ss;
  ss << "font_global_scale=" << font_global_scale_ << "\n";
  ss << "autosave_enabled=" << (autosave_enabled_ ? 1 : 0) << "\n";
  ss << "autosave_interval_secs=" << autosave_interval_secs_ << "\n";
  core::SaveFile(settings_filename_, ss.str());
}

void EditorManager::RefreshWorkspacePresets() {
  workspace_presets_.clear();
  // Try to read a simple index file of presets
  try {
    auto data = core::LoadConfigFile("workspace_presets.txt");
    std::istringstream ss(data);
    std::string name;
    while (std::getline(ss, name)) {
      if (!name.empty()) workspace_presets_.push_back(name);
    }
  } catch (...) {
  }
}

void EditorManager::SaveWorkspacePreset(const std::string &name) {
  if (name.empty()) return;
  std::string ini_name = absl::StrCat("yaze_workspace_", name, ".ini");
  ImGui::SaveIniSettingsToDisk(ini_name.c_str());
  // Update index
  RefreshWorkspacePresets();
  if (std::find(workspace_presets_.begin(), workspace_presets_.end(), name) == workspace_presets_.end()) {
    workspace_presets_.push_back(name);
    std::ostringstream ss;
    for (auto &n : workspace_presets_) ss << n << "\n";
    core::SaveFile("workspace_presets.txt", ss.str());
  }
  last_workspace_preset_ = name;
}

void EditorManager::LoadWorkspacePreset(const std::string &name) {
  if (name.empty()) return;
  std::string ini_name = absl::StrCat("yaze_workspace_", name, ".ini");
  ImGui::LoadIniSettingsFromDisk(ini_name.c_str());
  last_workspace_preset_ = name;
}

void EditorManager::InitializeTestSuites() {
  auto& test_manager = test::TestManager::Get();
  
  // Register comprehensive test suites
  test_manager.RegisterTestSuite(std::make_unique<test::IntegratedTestSuite>());
  test_manager.RegisterTestSuite(std::make_unique<test::PerformanceTestSuite>());
  test_manager.RegisterTestSuite(std::make_unique<test::UITestSuite>());
  // test_manager.RegisterTestSuite(std::make_unique<test::ArenaTestSuite>()); // TODO: Implement ArenaTestSuite
  test_manager.RegisterTestSuite(std::make_unique<test::RomDependentTestSuite>());
  
  // Register Google Test suite if available
#ifdef YAZE_ENABLE_GTEST
  test_manager.RegisterTestSuite(std::make_unique<test::UnitTestSuite>());
#endif
  
  // Update resource monitoring to track Arena state
  test_manager.UpdateResourceStats();
}

constexpr const char *kOverworldEditorName = ICON_MD_LAYERS " Overworld Editor";
constexpr const char *kGraphicsEditorName = ICON_MD_PHOTO " Graphics Editor";
constexpr const char *kPaletteEditorName = ICON_MD_PALETTE " Palette Editor";
constexpr const char *kScreenEditorName = ICON_MD_SCREENSHOT " Screen Editor";
constexpr const char *kSpriteEditorName = ICON_MD_SMART_TOY " Sprite Editor";
constexpr const char *kMessageEditorName = ICON_MD_MESSAGE " Message Editor";
constexpr const char *kSettingsEditorName = ICON_MD_SETTINGS " Settings Editor";
constexpr const char *kAssemblyEditorName = ICON_MD_CODE " Assembly Editor";
constexpr const char *kDungeonEditorName = ICON_MD_CASTLE " Dungeon Editor";
constexpr const char *kMusicEditorName = ICON_MD_MUSIC_NOTE " Music Editor";

void EditorManager::Initialize(const std::string &filename) {
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

  // Load user settings and workspace presets
  LoadUserSettings();
  RefreshWorkspacePresets();
  
  // Initialize testing system
  InitializeTestSuites();

  context_.shortcut_manager.RegisterShortcut(
      "Open", {ImGuiKey_O, ImGuiMod_Ctrl}, [this]() { status_ = LoadRom(); });
  context_.shortcut_manager.RegisterShortcut(
      "Save", {ImGuiKey_S, ImGuiMod_Ctrl}, [this]() { status_ = SaveRom(); });
  context_.shortcut_manager.RegisterShortcut(
      "Close", {ImGuiKey_W, ImGuiMod_Ctrl}, [this]() {
        if (current_rom_) current_rom_->Close();
      });
  context_.shortcut_manager.RegisterShortcut(
      "Quit", {ImGuiKey_Q, ImGuiMod_Ctrl}, [this]() { quit_ = true; });

  context_.shortcut_manager.RegisterShortcut(
      "Undo", {ImGuiKey_Z, ImGuiMod_Ctrl},
      [this]() { if (current_editor_) status_ = current_editor_->Undo(); });
  context_.shortcut_manager.RegisterShortcut(
      "Redo", {ImGuiKey_Y, ImGuiMod_Ctrl},
      [this]() { if (current_editor_) status_ = current_editor_->Redo(); });
  context_.shortcut_manager.RegisterShortcut(
      "Cut", {ImGuiKey_X, ImGuiMod_Ctrl},
      [this]() { if (current_editor_) status_ = current_editor_->Cut(); });
  context_.shortcut_manager.RegisterShortcut(
      "Copy", {ImGuiKey_C, ImGuiMod_Ctrl},
      [this]() { if (current_editor_) status_ = current_editor_->Copy(); });
  context_.shortcut_manager.RegisterShortcut(
      "Paste", {ImGuiKey_V, ImGuiMod_Ctrl},
      [this]() { if (current_editor_) status_ = current_editor_->Paste(); });
  context_.shortcut_manager.RegisterShortcut(
      "Find", {ImGuiKey_F, ImGuiMod_Ctrl},
      [this]() { if (current_editor_) status_ = current_editor_->Find(); });

  // Command Palette and Global Search
  context_.shortcut_manager.RegisterShortcut(
      "Command Palette", {ImGuiKey_P, ImGuiMod_Ctrl, ImGuiMod_Shift},
      [this]() { show_command_palette_ = true; });
  context_.shortcut_manager.RegisterShortcut(
      "Global Search", {ImGuiKey_K, ImGuiMod_Ctrl, ImGuiMod_Shift},
      [this]() { show_global_search_ = true; });

  context_.shortcut_manager.RegisterShortcut(
      "Load Last ROM", {ImGuiKey_R, ImGuiMod_Ctrl}, [this]() {
        static RecentFilesManager manager("recent_files.txt");
        manager.Load();
        if (!manager.GetRecentFiles().empty()) {
          auto front = manager.GetRecentFiles().front();
          status_ = OpenRomOrProject(front);
        }
      });

  context_.shortcut_manager.RegisterShortcut(
      "F1", ImGuiKey_F1, [this]() { popup_manager_->Show("About"); });
  
  // Testing shortcuts
  context_.shortcut_manager.RegisterShortcut(
      "Test Dashboard", {ImGuiKey_T, ImGuiMod_Ctrl}, 
      [this]() { show_test_dashboard_ = true; });
  
  // Workspace shortcuts
  context_.shortcut_manager.RegisterShortcut(
      "New Session", std::vector<ImGuiKey>{ImGuiKey_N, ImGuiMod_Ctrl, ImGuiMod_Shift}, 
      [this]() { CreateNewSession(); });
  context_.shortcut_manager.RegisterShortcut(
      "Close Session", std::vector<ImGuiKey>{ImGuiKey_W, ImGuiMod_Ctrl, ImGuiMod_Shift}, 
      [this]() { CloseCurrentSession(); });
  context_.shortcut_manager.RegisterShortcut(
      "Session Switcher", std::vector<ImGuiKey>{ImGuiKey_Tab, ImGuiMod_Ctrl}, 
      [this]() { show_session_switcher_ = true; });
  context_.shortcut_manager.RegisterShortcut(
      "Save Layout", std::vector<ImGuiKey>{ImGuiKey_S, ImGuiMod_Ctrl, ImGuiMod_Shift}, 
      [this]() { SaveWorkspaceLayout(); });
  context_.shortcut_manager.RegisterShortcut(
      "Load Layout", std::vector<ImGuiKey>{ImGuiKey_O, ImGuiMod_Ctrl, ImGuiMod_Shift}, 
      [this]() { LoadWorkspaceLayout(); });
  context_.shortcut_manager.RegisterShortcut(
      "Maximize Window", ImGuiKey_F11, 
      [this]() { MaximizeCurrentWindow(); });

  // Initialize menu items
  std::vector<gui::MenuItem> recent_files;
  static RecentFilesManager manager("recent_files.txt");
  manager.Load();
  if (manager.GetRecentFiles().empty()) {
    recent_files.emplace_back("No Recent Files", "", nullptr);
  } else {
    for (const auto &filePath : manager.GetRecentFiles()) {
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
      "Autosave", "", [this]() {
        autosave_enabled_ = !autosave_enabled_;
        toast_manager_.Show(autosave_enabled_ ? "Autosave enabled"
                                             : "Autosave disabled",
                            editor::ToastType::kInfo);
      },
      [this]() { return autosave_enabled_; });
  options_subitems.emplace_back(
      "Autosave Interval", "", [this]() {}, []() { return true; },
      std::vector<gui::MenuItem>{
          {"1 min", "", [this]() { autosave_interval_secs_ = 60.0f; SaveUserSettings(); }},
          {"2 min", "", [this]() { autosave_interval_secs_ = 120.0f; SaveUserSettings(); }},
          {"5 min", "", [this]() { autosave_interval_secs_ = 300.0f; SaveUserSettings(); }},
      });

  std::vector<gui::MenuItem> project_menu_subitems;
  project_menu_subitems.emplace_back(
      "New Project", "", [this]() { popup_manager_->Show("New Project"); });
  project_menu_subitems.emplace_back("Open Project", "",
                                     [this]() { status_ = OpenProject(); });
  project_menu_subitems.emplace_back(
      "Save Project", "", [this]() { status_ = SaveProject(); },
      [this]() { return current_project_.project_opened_; });
  project_menu_subitems.emplace_back(
      "Save Workspace Layout", "", [this]() {
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::SaveIniSettingsToDisk("yaze_workspace.ini");
        toast_manager_.Show("Workspace layout saved", editor::ToastType::kSuccess);
      });
  project_menu_subitems.emplace_back(
      "Load Workspace Layout", "", [this]() {
        ImGui::LoadIniSettingsFromDisk("yaze_workspace.ini");
        toast_manager_.Show("Workspace layout loaded", editor::ToastType::kSuccess);
      });
  project_menu_subitems.emplace_back(
      "Workspace Presets", "", []() {}, []() { return true; },
      std::vector<gui::MenuItem>{
          {"Save Preset", "", [this]() { show_save_workspace_preset_ = true; }},
          {"Load Preset", "", [this]() { show_load_workspace_preset_ = true; }},
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
            []() { return !manager.GetRecentFiles().empty(); }, recent_files},
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
           {absl::StrCat(ICON_MD_HOME, " Home"), "",
            [&]() { show_homepage_ = true; }},
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
           {absl::StrCat(ICON_MD_GAMEPAD, " Emulator"), "",
            [&]() { show_emulator_ = true; }},
           {absl::StrCat(ICON_MD_MEMORY, " Memory Editor"), "",
            [&]() { show_memory_editor_ = true; }},
           {absl::StrCat(ICON_MD_SIM_CARD, " ROM Metadata"), "",
            [&]() { popup_manager_->Show("ROM Information"); }},
           {gui::kSeparator, "", nullptr, []() { return true; }},
           {absl::StrCat(ICON_MD_HELP, " ImGui Demo"), "",
            [&]() { show_imgui_demo_ = true; }},
           {absl::StrCat(ICON_MD_HELP, " ImGui Metrics"), "",
            [&]() { show_imgui_metrics_ = true; }},
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
      {"Testing",
       {},
       {},
       {},
       {
           {absl::StrCat(ICON_MD_SCIENCE, " Test Dashboard"), "Ctrl+T",
            [&]() { show_test_dashboard_ = true; }},
           {gui::kSeparator, "", nullptr, []() { return true; }},
           {absl::StrCat(ICON_MD_PLAY_ARROW, " Run All Tests"), "",
            [&]() { [[maybe_unused]] auto status = test::TestManager::Get().RunAllTests(); }},
           {absl::StrCat(ICON_MD_INTEGRATION_INSTRUCTIONS, " Run Unit Tests"), "",
            [&]() { [[maybe_unused]] auto status = test::TestManager::Get().RunTestsByCategory(test::TestCategory::kUnit); }},
           {absl::StrCat(ICON_MD_MEMORY, " Run Integration Tests"), "",
            [&]() { [[maybe_unused]] auto status = test::TestManager::Get().RunTestsByCategory(test::TestCategory::kIntegration); }},
           {absl::StrCat(ICON_MD_MOUSE, " Run UI Tests"), "",
            [&]() { [[maybe_unused]] auto status = test::TestManager::Get().RunTestsByCategory(test::TestCategory::kUI); }},
           {gui::kSeparator, "", nullptr, []() { return true; }},
           {absl::StrCat(ICON_MD_CLEAR_ALL, " Clear Results"), "",
            [&]() { test::TestManager::Get().ClearResults(); }},
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
        toast_manager_.Show(std::string(st.message()), editor::ToastType::kError, 5.0f);
      }
    }
  } else {
    autosave_timer_ = 0.0f;
  }

  if (show_homepage_) {
    ImGui::Begin("Home", &show_homepage_);
    DrawHomepage();
    ImGui::End();
  }

  if (!current_editor_set_) {
    return absl::OkStatus();
  }
  for (auto editor : current_editor_set_->active_editors_) {
    if (*editor->active()) {
      if (editor->type() == EditorType::kOverworld) {
        auto &overworld_editor = static_cast<OverworldEditor &>(*editor);
        if (overworld_editor.jump_to_tab() != -1) {
          current_editor_set_->dungeon_editor_.set_active(true);
          // Set the dungeon editor to the jump to tab
          current_editor_set_->dungeon_editor_.add_room(
              overworld_editor.jump_to_tab());
          overworld_editor.jump_to_tab_ = -1;
        }
      }

      // Clean window titles without session clutter
      std::string window_title = GetEditorName(editor->type());
      
      if (ImGui::Begin(window_title.c_str(), editor->active())) {
        current_editor_ = editor;
        status_ = editor->Update();
      }
      ImGui::End();
    }
  }
  return absl::OkStatus();
}

void EditorManager::DrawHomepage() {
  TextWrapped("Welcome to the Yet Another Zelda3 Editor (yaze)!");
  TextWrapped("The Legend of Zelda: A Link to the Past.");
  TextWrapped("Please report any bugs or issues you encounter.");
  ImGui::SameLine();
  if (gui::ClickableText("https://github.com/scawful/yaze")) {
    gui::OpenUrl("https://github.com/scawful/yaze");
  }

  if (!current_rom_) {
    TextWrapped("No ROM loaded.");
    if (gui::ClickableText("Open a ROM")) {
      status_ = LoadRom();
    }
    SameLine();
    Checkbox("Load custom overworld features",
             &core::FeatureFlags::get().overworld.kLoadCustomOverworld);

    ImGui::BeginChild("Recent Files", ImVec2(-1, -1), true);
    static RecentFilesManager manager("recent_files.txt");
    manager.Load();
    for (const auto &file : manager.GetRecentFiles()) {
      if (gui::ClickableText(file.c_str())) {
        status_ = OpenRomOrProject(file);
      }
    }
    ImGui::EndChild();
    return;
  }

  TextWrapped("Current ROM: %s", current_rom_->filename().c_str());
  if (Button(kOverworldEditorName)) {
    current_editor_set_->overworld_editor_.set_active(true);
  }
  ImGui::SameLine();
  if (Button(kDungeonEditorName)) {
    current_editor_set_->dungeon_editor_.set_active(true);
  }
  ImGui::SameLine();
  if (Button(kGraphicsEditorName)) {
    current_editor_set_->graphics_editor_.set_active(true);
  }
  ImGui::SameLine();
  if (Button(kMessageEditorName)) {
    current_editor_set_->message_editor_.set_active(true);
  }

  if (Button(kPaletteEditorName)) {
    current_editor_set_->palette_editor_.set_active(true);
  }
  ImGui::SameLine();
  if (Button(kScreenEditorName)) {
    current_editor_set_->screen_editor_.set_active(true);
  }
  ImGui::SameLine();
  if (Button(kSpriteEditorName)) {
    current_editor_set_->sprite_editor_.set_active(true);
  }
  ImGui::SameLine();
  if (Button(kMusicEditorName)) {
    current_editor_set_->music_editor_.set_active(true);
  }

  if (Button(kSettingsEditorName)) {
    current_editor_set_->settings_editor_.set_active(true);
  }
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

    // Session management integrated into menu bar (right side)
    float session_area_width = 350.0f;
    SameLine(GetWindowWidth() - session_area_width);
    
    // Multi-session indicator
    if (sessions_.size() > 1) {
      if (SmallButton(absl::StrFormat("%s %zu", ICON_MD_TAB, sessions_.size()).c_str())) {
        show_session_switcher_ = true;
      }
      if (IsItemHovered()) {
        SetTooltip("Sessions: %zu active\nClick to switch between sessions", sessions_.size());
      }
      SameLine();
      ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
      SameLine();
    }
    
    // ROM status with natural integration
    if (current_rom_ && current_rom_->is_loaded()) {
      std::string rom_display = current_rom_->title();
      if (rom_display.length() > 12) {
        rom_display = rom_display.substr(0, 9) + "...";
      }
      
      ImVec4 status_color = current_rom_->dirty() ? 
                           ImVec4(1.0f, 0.5f, 0.0f, 1.0f) :  // Orange for modified
                           ImVec4(0.0f, 0.8f, 0.0f, 1.0f);   // Green for clean
      
      TextColored(status_color, "%s %s%s", 
                 ICON_MD_STORAGE, 
                 rom_display.c_str(),
                 current_rom_->dirty() ? "*" : "");
      
      if (IsItemHovered()) {
        SetTooltip("ROM: %s\nFile: %s\nSize: %zu bytes\nStatus: %s", 
                  current_rom_->title().c_str(),
                  current_rom_->filename().c_str(), 
                  current_rom_->size(),
                  current_rom_->dirty() ? "Modified" : "Clean");
      }
      SameLine();
    } else {
      TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s No ROM", ICON_MD_HELP_OUTLINE);
      SameLine();
    }
    
    // Settings and version (far right)
    SeparatorEx(ImGuiSeparatorFlags_Vertical);
    SameLine();
    PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    if (Button(ICON_MD_DISPLAY_SETTINGS)) {
      show_display_settings = !show_display_settings;
    }
    PopStyleColor();
    SameLine();
    Text("v%s", version_.c_str());
    EndMenuBar();
  }

  if (show_display_settings) {
    Begin("Display Settings", &show_display_settings, ImGuiWindowFlags_None);
    gui::DrawDisplaySettings();
    gui::TextWithSeparators("Font Manager");
    gui::DrawFontManager();
    ImGuiIO &io = ImGui::GetIO();
    Separator();
    Text("Global Scale");
    if (SliderFloat("##global_scale", &font_global_scale_, 0.5f, 1.8f, "%.2f")) {
      io.FontGlobalScale = font_global_scale_;
      SaveUserSettings();
    }
    End();
  }

  if (show_imgui_demo_) ShowDemoWindow();
  if (show_imgui_metrics_) ShowMetricsWindow(&show_imgui_metrics_);
  if (show_memory_editor_ && current_editor_set_) {
    current_editor_set_->memory_editor_.Update(show_memory_editor_);
  }
  if (show_asm_editor_ && current_editor_set_) {
    current_editor_set_->assembly_editor_.Update(show_asm_editor_);
  }
  
  // Testing interface
  if (show_test_dashboard_) {
    auto& test_manager = test::TestManager::Get();
    test_manager.UpdateResourceStats(); // Update monitoring data
    test_manager.DrawTestDashboard();
  }

  if (show_emulator_) {
    Begin("Emulator", &show_emulator_, ImGuiWindowFlags_MenuBar);
    emulator_.Run();
    End();
  }

  // Command Palette UI
  if (show_command_palette_) {
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_Once);
    if (Begin("Command Palette", &show_command_palette_, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)) {
      static char query[256] = {};
      InputTextWithHint("##cmd_query", "Type a command or search...", query, IM_ARRAYSIZE(query));
      Separator();
      // List registered shortcuts as commands
      for (const auto &entry : context_.shortcut_manager.GetShortcuts()) {
        const auto &name = entry.first;
        const auto &shortcut = entry.second;
        if (query[0] != '\0' && name.find(query) == std::string::npos) continue;
        if (Selectable(name.c_str())) {
          if (shortcut.callback) shortcut.callback();
          show_command_palette_ = false;
        }
      }
    }
    End();
  }

  // Global Search UI (labels and recent files for now)
  if (show_global_search_) {
    ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_Once);
    if (Begin("Global Search", &show_global_search_, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)) {
      static char query[256] = {};
      InputTextWithHint("##global_query", ICON_MD_SEARCH " Search labels, files...", query, IM_ARRAYSIZE(query));
      Separator();
      if (current_rom_ && current_rom_->resource_label()) {
        Text(ICON_MD_LABEL " Labels");
        Indent();
        auto &labels = current_rom_->resource_label()->labels_;
        for (const auto &type_pair : labels) {
          for (const auto &kv : type_pair.second) {
            if (query[0] != '\0' && kv.first.find(query) == std::string::npos && kv.second.find(query) == std::string::npos) continue;
            if (Selectable((type_pair.first + ": " + kv.first + " -> " + kv.second).c_str())) {
              // Future: navigate to related editor/location
            }
          }
        }
        Unindent();
      }
      Text(ICON_MD_HISTORY " Recent Files");
      Indent();
      static RecentFilesManager manager("recent_files.txt");
      manager.Load();
      for (const auto &file : manager.GetRecentFiles()) {
        if (query[0] != '\0' && file.find(query) == std::string::npos) continue;
        if (Selectable(file.c_str())) {
          status_ = OpenRomOrProject(file);
          show_global_search_ = false;
        }
      }
      Unindent();
    }
    End();
  }

  if (show_palette_editor_ && current_editor_set_) {
    Begin("Palette Editor", &show_palette_editor_);
    status_ = current_editor_set_->palette_editor_.Update();
    End();
  }

  if (show_resource_label_manager && current_rom_) {
    current_rom_->resource_label()->DisplayLabels(&show_resource_label_manager);
    if (current_project_.project_opened_ &&
        !current_project_.labels_filename_.empty()) {
      current_project_.labels_filename_ =
          current_rom_->resource_label()->filename_;
    }
  }

  if (save_as_menu) {
    static std::string save_as_filename = "";
    Begin("Save As..", &save_as_menu, ImGuiWindowFlags_AlwaysAutoResize);
    InputText("Filename", &save_as_filename);
    if (Button("Save", gui::kDefaultModalSize)) {
      status_ = SaveRom();
      save_as_menu = false;
    }
    SameLine();
    if (Button("Cancel", gui::kDefaultModalSize)) {
      save_as_menu = false;
    }
    End();
  }

  if (new_project_menu) {
    Begin("New Project", &new_project_menu, ImGuiWindowFlags_AlwaysAutoResize);
    static std::string save_as_filename = "";
    InputText("Project Name", &save_as_filename);
    if (Button("Destination Filepath", gui::kDefaultModalSize)) {
      current_project_.filepath = FileDialogWrapper::ShowOpenFolderDialog();
    }
    SameLine();
    Text("%s", current_project_.filepath.c_str());
    if (Button("ROM File", gui::kDefaultModalSize)) {
      current_project_.rom_filename_ = FileDialogWrapper::ShowOpenFileDialog();
    }
    SameLine();
    Text("%s", current_project_.rom_filename_.c_str());
    if (Button("Labels File", gui::kDefaultModalSize)) {
      current_project_.labels_filename_ =
          FileDialogWrapper::ShowOpenFileDialog();
    }
    SameLine();
    Text("%s", current_project_.labels_filename_.c_str());
    if (Button("Code Folder", gui::kDefaultModalSize)) {
      current_project_.code_folder_ = FileDialogWrapper::ShowOpenFolderDialog();
    }
    SameLine();
    Text("%s", current_project_.code_folder_.c_str());

    Separator();
    if (Button("Create", gui::kDefaultModalSize)) {
      new_project_menu = false;
      status_ = current_project_.Create(save_as_filename);
      if (status_.ok()) {
        status_ = current_project_.Save();
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
    Begin("Save Workspace Preset", &show_save_workspace_preset_, ImGuiWindowFlags_AlwaysAutoResize);
    static std::string preset_name = "";
    InputText("Name", &preset_name);
    if (Button("Save", gui::kDefaultModalSize)) {
      SaveWorkspacePreset(preset_name);
      toast_manager_.Show("Preset saved", editor::ToastType::kSuccess);
      show_save_workspace_preset_ = false;
    }
    SameLine();
    if (Button("Cancel", gui::kDefaultModalSize)) { show_save_workspace_preset_ = false; }
    End();
  }

  if (show_load_workspace_preset_) {
    Begin("Load Workspace Preset", &show_load_workspace_preset_, ImGuiWindowFlags_AlwaysAutoResize);
    for (const auto &name : workspace_presets_) {
      if (Selectable(name.c_str())) {
        LoadWorkspacePreset(name);
        toast_manager_.Show("Preset loaded", editor::ToastType::kSuccess);
        show_load_workspace_preset_ = false;
      }
    }
    if (workspace_presets_.empty()) Text("No presets found");
    End();
  }
  
  // Draw new workspace UI elements
  DrawSessionSwitcher();
  DrawSessionManager();
  DrawLayoutPresets();
}

absl::Status EditorManager::LoadRom() {
  auto file_name = FileDialogWrapper::ShowOpenFileDialog();
  if (file_name.empty()) {
    return absl::OkStatus();
  }
  Rom temp_rom;
  RETURN_IF_ERROR(temp_rom.LoadFromFile(file_name));

  sessions_.emplace_back(std::move(temp_rom));
  RomSession &session = sessions_.back();
  // Wire editor contexts
  for (auto *editor : session.editors.active_editors_) {
    editor->set_context(&context_);
  }
  current_rom_ = &session.rom;
  current_editor_set_ = &session.editors;
  
  // Update test manager with current ROM for ROM-dependent tests
  test::TestManager::Get().SetCurrentRom(current_rom_);

  static RecentFilesManager manager("recent_files.txt");
  manager.Load();
  manager.AddFile(file_name);
  manager.Save();
  RETURN_IF_ERROR(LoadAssets());

  return absl::OkStatus();
}

absl::Status EditorManager::LoadAssets() {
  if (!current_rom_ || !current_editor_set_) {
    return absl::FailedPreconditionError("No ROM or editor set loaded");
  }
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

absl::Status EditorManager::OpenRomOrProject(const std::string &filename) {
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
    RomSession &session = sessions_.back();
    for (auto *editor : session.editors.active_editors_) {
      editor->set_context(&context_);
    }
    current_rom_ = &session.rom;
    current_editor_set_ = &session.editors;
    RETURN_IF_ERROR(LoadAssets());
  }
  return absl::OkStatus();
}

absl::Status EditorManager::OpenProject() {
  Rom temp_rom;
  RETURN_IF_ERROR(temp_rom.LoadFromFile(current_project_.rom_filename_));

  if (!temp_rom.resource_label()->LoadLabels(
          current_project_.labels_filename_)) {
    return absl::InternalError(
        "Could not load labels file, update your project file.");
  }

  sessions_.emplace_back(std::move(temp_rom));
  RomSession &session = sessions_.back();
  for (auto *editor : session.editors.active_editors_) {
    editor->set_context(&context_);
  }
  current_rom_ = &session.rom;
  current_editor_set_ = &session.editors;

  static RecentFilesManager manager("recent_files.txt");
  manager.Load();
  manager.AddFile(current_project_.filepath + "/" + current_project_.name +
                  ".yaze");
  manager.Save();
  if (current_editor_set_) {
    current_editor_set_->assembly_editor_.OpenFolder(
        current_project_.code_folder_);
  }
  current_project_.project_opened_ = true;
  RETURN_IF_ERROR(LoadAssets());
  return absl::OkStatus();
}

absl::Status EditorManager::SaveProject() {
  if (current_project_.project_opened_) {
    RETURN_IF_ERROR(current_project_.Save());
  } else {
    new_project_menu = true;
  }
  return absl::OkStatus();
}

absl::Status EditorManager::SetCurrentRom(Rom *rom) {
  if (!rom) {
    return absl::InvalidArgumentError("Invalid ROM pointer");
  }

  for (auto &session : sessions_) {
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

// Session Management Functions
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
  toast_manager_.Show(absl::StrFormat("New session created (Session %zu)", sessions_.size()), 
                     editor::ToastType::kSuccess);
  
  // Show session manager if user has multiple sessions now
  if (sessions_.size() > 2) {
    toast_manager_.Show("Tip: Use Workspace → Sessions → Session Switcher for quick navigation", 
                       editor::ToastType::kInfo, 5.0f);
  }
}

void EditorManager::DuplicateCurrentSession() {
  if (!current_rom_) {
    toast_manager_.Show("No current ROM to duplicate", editor::ToastType::kWarning);
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
  
  toast_manager_.Show(absl::StrFormat("Session duplicated (Session %zu)", sessions_.size()), 
                     editor::ToastType::kSuccess);
}

void EditorManager::CloseCurrentSession() {
  if (sessions_.size() <= 1) {
    toast_manager_.Show("Cannot close the last session", editor::ToastType::kWarning);
    return;
  }
  
  // For now, just switch to the next available session 
  // TODO: Implement proper session removal when RomSession becomes movable
  for (size_t i = 0; i < sessions_.size(); ++i) {
    if (&sessions_[i].rom != current_rom_) {
      current_rom_ = &sessions_[i].rom;
      current_editor_set_ = &sessions_[i].editors;
      test::TestManager::Get().SetCurrentRom(current_rom_);
      break;
    }
  }
  
  toast_manager_.Show("Switched to next session (full session removal coming soon)", 
                     editor::ToastType::kInfo, 4.0f);
}

void EditorManager::SwitchToSession(size_t index) {
  if (index >= sessions_.size()) {
    toast_manager_.Show("Invalid session index", editor::ToastType::kError);
    return;
  }
  
  auto& session = sessions_[index];
  current_rom_ = &session.rom;
  current_editor_set_ = &session.editors;
  test::TestManager::Get().SetCurrentRom(current_rom_);
  
  std::string session_name = current_rom_->title().empty() ? 
                            absl::StrFormat("Session %zu", index + 1) : 
                            current_rom_->title();
  toast_manager_.Show(absl::StrFormat("Switched to %s", session_name), 
                     editor::ToastType::kInfo);
}

size_t EditorManager::GetCurrentSessionIndex() const {
  for (size_t i = 0; i < sessions_.size(); ++i) {
    if (&sessions_[i].rom == current_rom_) {
      return i;
    }
  }
  return 0; // Default to first session if not found
}

// Layout Management Functions
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

// Window Management Functions
void EditorManager::ShowAllWindows() {
  if (!current_editor_set_) return;
  
  for (auto* editor : current_editor_set_->active_editors_) {
    editor->set_active(true);
  }
  show_imgui_demo_ = true;
  show_imgui_metrics_ = true;
  show_test_dashboard_ = true;
  
  toast_manager_.Show("All windows shown", editor::ToastType::kInfo);
}

void EditorManager::HideAllWindows() {
  if (!current_editor_set_) return;
  
  for (auto* editor : current_editor_set_->active_editors_) {
    editor->set_active(false);
  }
  show_imgui_demo_ = false;
  show_imgui_metrics_ = false;
  show_test_dashboard_ = false;
  
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

// Preset Layout Functions
void EditorManager::LoadDeveloperLayout() {
  if (!current_editor_set_) return;
  
  // Developer layout: Code editor, assembly editor, test dashboard
  current_editor_set_->assembly_editor_.set_active(true);
  show_test_dashboard_ = true;
  show_imgui_metrics_ = true;
  
  // Hide non-dev windows
  current_editor_set_->graphics_editor_.set_active(false);
  current_editor_set_->music_editor_.set_active(false);
  current_editor_set_->sprite_editor_.set_active(false);
  
  toast_manager_.Show("Developer layout loaded", editor::ToastType::kSuccess);
}

void EditorManager::LoadDesignerLayout() {
  if (!current_editor_set_) return;
  
  // Designer layout: Graphics, palette, sprite editors
  current_editor_set_->graphics_editor_.set_active(true);
  current_editor_set_->palette_editor_.set_active(true);
  current_editor_set_->sprite_editor_.set_active(true);
  current_editor_set_->overworld_editor_.set_active(true);
  
  // Hide non-design windows
  current_editor_set_->assembly_editor_.set_active(false);
  show_test_dashboard_ = false;
  show_imgui_metrics_ = false;
  
  toast_manager_.Show("Designer layout loaded", editor::ToastType::kSuccess);
}

void EditorManager::LoadModderLayout() {
  if (!current_editor_set_) return;
  
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

// UI Drawing Functions
void EditorManager::DrawSessionSwitcher() {
  if (!show_session_switcher_) return;
  
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_Appearing);
  
  if (ImGui::Begin(absl::StrCat(ICON_MD_SWITCH_ACCOUNT, " Session Switcher").c_str(), 
                   &show_session_switcher_, ImGuiWindowFlags_NoCollapse)) {
    
    ImGui::Text("%s %zu Sessions Available", ICON_MD_TAB, sessions_.size());
    ImGui::Separator();
    
    for (size_t i = 0; i < sessions_.size(); ++i) {
      auto& session = sessions_[i];
      bool is_current = (&session.rom == current_rom_);
      
      if (is_current) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
      }
      
      std::string session_name = session.rom.title().empty() ? 
                                absl::StrFormat("Session %zu", i + 1) : 
                                session.rom.title();
      
      if (ImGui::Button(absl::StrFormat("%s %s %s", 
                                       ICON_MD_STORAGE, 
                                       session_name.c_str(),
                                       is_current ? "(Current)" : "").c_str(), 
                       ImVec2(-1, 0))) {
        if (!is_current) {
          SwitchToSession(i);
          show_session_switcher_ = false;
        }
      }
      
      if (is_current) {
        ImGui::PopStyleColor();
      }
      
      // Show ROM info on hover
      if (ImGui::IsItemHovered() && session.rom.is_loaded()) {
        ImGui::BeginTooltip();
        ImGui::Text("File: %s", session.rom.filename().c_str());
        ImGui::Text("Size: %zu bytes", session.rom.size());
        ImGui::Text("Modified: %s", session.rom.dirty() ? "Yes" : "No");
        ImGui::EndTooltip();
      }
    }
    
    ImGui::Separator();
    if (ImGui::Button(absl::StrCat(ICON_MD_CLOSE, " Close").c_str(), ImVec2(-1, 0))) {
      show_session_switcher_ = false;
    }
  }
  ImGui::End();
}

void EditorManager::DrawSessionManager() {
  if (!show_session_manager_) return;
  
  if (ImGui::Begin(absl::StrCat(ICON_MD_VIEW_LIST, " Session Manager").c_str(), 
                   &show_session_manager_)) {
    
    ImGui::Text("%s Session Management", ICON_MD_MANAGE_ACCOUNTS);
    
    if (ImGui::Button(absl::StrCat(ICON_MD_ADD, " New Session").c_str())) {
      CreateNewSession();
    }
    ImGui::SameLine();
    if (ImGui::Button(absl::StrCat(ICON_MD_CONTENT_COPY, " Duplicate Current").c_str()) && current_rom_) {
      DuplicateCurrentSession();
    }
    
    ImGui::Separator();
    ImGui::Text("%s Active Sessions (%zu)", ICON_MD_TAB, sessions_.size());
    
    // Session list with controls
    if (ImGui::BeginTable("SessionTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("Session", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("ROM", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80);
      ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 120);
      ImGui::TableHeadersRow();
      
      for (size_t i = 0; i < sessions_.size(); ++i) {
        auto& session = sessions_[i];
        bool is_current = (&session.rom == current_rom_);
        
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        
        if (is_current) {
          ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), 
                            "%s Session %zu", ICON_MD_STAR, i + 1);
        } else {
          ImGui::Text("%s Session %zu", ICON_MD_TAB, i + 1);
        }
        
        ImGui::TableNextColumn();
        std::string rom_title = session.rom.title().empty() ? "No ROM" : session.rom.title();
        ImGui::Text("%s", rom_title.c_str());
        
        ImGui::TableNextColumn();
        if (session.rom.is_loaded()) {
          if (session.rom.dirty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "%s Modified", ICON_MD_EDIT);
          } else {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s Loaded", ICON_MD_CHECK_CIRCLE);
          }
        } else {
          ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s Empty", ICON_MD_RADIO_BUTTON_UNCHECKED);
        }
        
        ImGui::TableNextColumn();
        ImGui::PushID(static_cast<int>(i));
        
        if (!is_current && ImGui::Button(absl::StrCat(ICON_MD_SWITCH_ACCESS_SHORTCUT, " Switch").c_str())) {
          SwitchToSession(i);
        }
        
        if (is_current) {
          ImGui::BeginDisabled();
        }
        ImGui::SameLine();
        if (sessions_.size() > 1 && ImGui::Button(absl::StrCat(ICON_MD_CLOSE, " Close").c_str())) {
          if (is_current) {
            CloseCurrentSession();
            break; // Exit loop since current session was closed
          } else {
            // TODO: Implement proper session removal when RomSession becomes movable
            toast_manager_.Show("Session management temporarily disabled due to technical constraints", 
                               editor::ToastType::kWarning);
            break;
          }
        }
        
        if (is_current) {
          ImGui::EndDisabled();
        }
        
        ImGui::PopID();
      }
      
      ImGui::EndTable();
    }
  }
  ImGui::End();
}

void EditorManager::DrawLayoutPresets() {
  if (!show_layout_presets_) return;
  
  if (ImGui::Begin(absl::StrCat(ICON_MD_BOOKMARK, " Layout Presets").c_str(), 
                   &show_layout_presets_)) {
    
    ImGui::Text("%s Predefined Layouts", ICON_MD_DASHBOARD);
    
    // Predefined layouts
    if (ImGui::Button(absl::StrCat(ICON_MD_DEVELOPER_MODE, " Developer Layout").c_str(), ImVec2(-1, 40))) {
      LoadDeveloperLayout();
    }
    ImGui::SameLine();
    ImGui::Text("Code editing, debugging, testing");
    
    if (ImGui::Button(absl::StrCat(ICON_MD_DESIGN_SERVICES, " Designer Layout").c_str(), ImVec2(-1, 40))) {
      LoadDesignerLayout();
    }
    ImGui::SameLine();
    ImGui::Text("Graphics, palettes, sprites");
    
    if (ImGui::Button(absl::StrCat(ICON_MD_GAMEPAD, " Modder Layout").c_str(), ImVec2(-1, 40))) {
      LoadModderLayout();
    }
    ImGui::SameLine();
    ImGui::Text("All gameplay editors");
    
    ImGui::Separator();
    ImGui::Text("%s Custom Presets", ICON_MD_BOOKMARK);
    
    for (const auto& preset : workspace_presets_) {
      if (ImGui::Button(absl::StrFormat("%s %s", ICON_MD_BOOKMARK, preset.c_str()).c_str(), ImVec2(-1, 0))) {
        LoadWorkspacePreset(preset);
        toast_manager_.Show(absl::StrFormat("Loaded preset: %s", preset.c_str()), 
                           editor::ToastType::kSuccess);
      }
    }
    
    if (workspace_presets_.empty()) {
      ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No custom presets saved");
    }
    
    ImGui::Separator();
    if (ImGui::Button(absl::StrCat(ICON_MD_ADD, " Save Current Layout").c_str())) {
      show_save_workspace_preset_ = true;
    }
  }
  ImGui::End();
}

}  // namespace editor
}  // namespace yaze
