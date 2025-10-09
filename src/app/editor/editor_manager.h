#ifndef YAZE_APP_EDITOR_EDITOR_MANAGER_H
#define YAZE_APP_EDITOR_EDITOR_MANAGER_H

#define IMGUI_DEFINE_MATH_OPERATORS

#include "imgui/imgui.h"

#include <deque>
#include <vector>

#include "absl/status/status.h"
#include "app/core/features.h"
#include "app/core/project.h"
#include "app/editor/code/assembly_editor.h"
#include "app/editor/code/memory_editor.h"
#include "app/editor/ui/menu_builder.h"
#include "app/editor/code/project_file_editor.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/graphics/graphics_editor.h"
#include "app/editor/graphics/palette_editor.h"
#include "app/editor/graphics/screen_editor.h"
#include "app/editor/message/message_editor.h"
#include "app/editor/music/music_editor.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/sprite/sprite_editor.h"
#include "app/editor/system/popup_manager.h"
#include "app/editor/system/proposal_drawer.h"
#include "app/editor/agent/agent_chat_history_popup.h"
#ifdef YAZE_WITH_GRPC
#include "app/editor/agent/agent_editor.h"
#include "app/editor/agent/automation_bridge.h"
#endif
#include "app/editor/system/settings_editor.h"
#include "app/editor/system/toast_manager.h"
#include "app/editor/ui/editor_selection_dialog.h"
#include "app/editor/ui/welcome_screen.h"
#include "app/emu/emulator.h"
#include "app/gfx/performance_dashboard.h"
#include "app/rom.h"
#include "yaze_config.h"

#ifdef YAZE_WITH_GRPC
#endif

namespace yaze {
namespace editor {

/**
 * @class EditorSet
 * @brief Contains a complete set of editors for a single ROM instance
 */
class EditorSet {
 public:
  explicit EditorSet(Rom* rom = nullptr)
      : assembly_editor_(rom),
        dungeon_editor_(rom),
        graphics_editor_(rom),
        music_editor_(rom),
        overworld_editor_(rom),
        palette_editor_(rom),
        screen_editor_(rom),
        sprite_editor_(rom),
        settings_editor_(rom),
        message_editor_(rom),
        memory_editor_(rom) {
    active_editors_ = {&overworld_editor_, &dungeon_editor_, &graphics_editor_,
                       &palette_editor_,   &sprite_editor_,  &message_editor_,
                       &music_editor_,     &screen_editor_,  &settings_editor_,
                       &assembly_editor_};
  }

  AssemblyEditor assembly_editor_;
  DungeonEditorV2 dungeon_editor_;
  GraphicsEditor graphics_editor_;
  MusicEditor music_editor_;
  OverworldEditor overworld_editor_;
  PaletteEditor palette_editor_;
  ScreenEditor screen_editor_;
  SpriteEditor sprite_editor_;
  SettingsEditor settings_editor_;
  MessageEditor message_editor_;
  MemoryEditorWithDiffChecker memory_editor_;

  std::vector<Editor*> active_editors_;
};

/**
 * @class EditorManager
 * @brief The EditorManager controls the main editor window and manages the
 * various editor classes.
 *
 * The EditorManager class contains instances of various editor classes such as
 * AssemblyEditor, DungeonEditor, GraphicsEditor, MusicEditor, OverworldEditor,
 * PaletteEditor, ScreenEditor, and SpriteEditor. The current_editor_ member
 * variable points to the currently active editor in the tab view.
 *
 */
class EditorManager {
 public:
  EditorManager() {
    std::stringstream ss;
    ss << YAZE_VERSION_MAJOR << "." << YAZE_VERSION_MINOR << "."
       << YAZE_VERSION_PATCH;
    ss >> version_;
    context_.popup_manager = popup_manager_.get();
  }

  void Initialize(gfx::IRenderer* renderer, const std::string& filename = "");
  absl::Status Update();
  void DrawMenuBar();

  auto emulator() -> emu::Emulator& { return emulator_; }
  auto quit() const { return quit_; }
  auto version() const { return version_; }

  absl::Status SetCurrentRom(Rom* rom);
  auto GetCurrentRom() -> Rom* { return current_rom_; }
  auto GetCurrentEditorSet() -> EditorSet* { return current_editor_set_; }
  auto overworld() -> yaze::zelda3::Overworld* { return &current_editor_set_->overworld_editor_.overworld(); }

  // Get current session's feature flags (falls back to global if no session)
  core::FeatureFlags::Flags* GetCurrentFeatureFlags() {
    size_t current_index = GetCurrentSessionIndex();
    if (current_index < sessions_.size()) {
      return &sessions_[current_index].feature_flags;
    }
    return &core::FeatureFlags::get();  // Fallback to global
  }

  void SetFontGlobalScale(float scale) {
    font_global_scale_ = scale;
    ImGui::GetIO().FontGlobalScale = scale;
    SaveUserSettings();
  }
  
  void BuildModernMenu();
  
  // Jump-to functionality for cross-editor navigation
  void JumpToDungeonRoom(int room_id);
  void JumpToOverworldMap(int map_id);
  void SwitchToEditor(EditorType editor_type);

 private:
  void DrawWelcomeScreen();
  absl::Status DrawRomSelector();
  absl::Status LoadRom();
  absl::Status LoadAssets();
  absl::Status SaveRom();
  absl::Status SaveRomAs(const std::string& filename);
  absl::Status OpenRomOrProject(const std::string& filename);

  // Enhanced project management
  absl::Status CreateNewProject(
      const std::string& template_name = "Basic ROM Hack");
  absl::Status OpenProject();
  absl::Status SaveProject();
  absl::Status SaveProjectAs();
  absl::Status ImportProject(const std::string& project_path);
  absl::Status RepairCurrentProject();
  void ShowProjectHelp();

  // Testing system
  void InitializeTestSuites();

  bool quit_ = false;
  bool backup_rom_ = false;
  bool save_new_auto_ = true;
  bool autosave_enabled_ = false;
  float autosave_interval_secs_ = 120.0f;
  float autosave_timer_ = 0.0f;
  bool new_project_menu = false;

  bool show_emulator_ = false;
  bool show_memory_editor_ = false;
  bool show_asm_editor_ = false;
  bool show_imgui_metrics_ = false;
  bool show_imgui_demo_ = false;
  bool show_palette_editor_ = false;
  bool show_resource_label_manager = false;
  bool show_workspace_layout = false;
  bool show_save_workspace_preset_ = false;
  bool show_load_workspace_preset_ = false;
  bool show_session_switcher_ = false;
  bool show_session_manager_ = false;
  bool show_layout_presets_ = false;
  bool show_homepage_ = true;
  bool show_command_palette_ = false;
  bool show_global_search_ = false;
  bool show_session_rename_dialog_ = false;
  bool show_welcome_screen_ = false;
  bool welcome_screen_manually_closed_ = false;
  bool show_card_browser_ = false;
  size_t session_to_rename_ = 0;
  char session_rename_buffer_[256] = {};

  // Testing interface
  bool show_test_dashboard_ = false;
  bool show_performance_dashboard_ = false;

  // Agent proposal drawer
  ProposalDrawer proposal_drawer_;
  bool show_proposal_drawer_ = false;

#ifdef YAZE_WITH_GRPC
  AutomationBridge harness_telemetry_bridge_;
#endif

  // Agent chat history popup
  AgentChatHistoryPopup agent_chat_history_popup_;
  bool show_chat_history_popup_ = false;

  // Project file editor
  ProjectFileEditor project_file_editor_;
  
  // Editor selection dialog
  EditorSelectionDialog editor_selection_dialog_;
  bool show_editor_selection_ = false;
  
  // Welcome screen
  WelcomeScreen welcome_screen_;

#ifdef YAZE_WITH_GRPC
  // Agent editor - manages chat, collaboration, and network coordination
  AgentEditor agent_editor_;
#endif

  std::string version_ = "";
  std::string settings_filename_ = "settings.ini";
  float font_global_scale_ = 1.0f;
  std::vector<std::string> workspace_presets_;
  std::string last_workspace_preset_ = "";
  std::string status_message_ = "";
  bool workspace_presets_loaded_ = false;

  absl::Status status_;
  emu::Emulator emulator_;

  struct RomSession {
    Rom rom;
    EditorSet editors;
    std::string custom_name;  // User-defined session name
    std::string filepath;     // ROM filepath for duplicate detection
    core::FeatureFlags::Flags feature_flags;  // Per-session feature flags

    RomSession() = default;
    explicit RomSession(Rom&& r) : rom(std::move(r)), editors(&rom) {
      filepath = rom.filename();
      // Initialize with default feature flags
      feature_flags = core::FeatureFlags::Flags{};
    }

    // Get display name (custom name or ROM title)
    std::string GetDisplayName() const {
      if (!custom_name.empty()) {
        return custom_name;
      }
      return rom.title().empty() ? "Untitled Session" : rom.title();
    }
  };

  std::deque<RomSession> sessions_;
  Rom* current_rom_ = nullptr;
  EditorSet* current_editor_set_ = nullptr;
  Editor* current_editor_ = nullptr;
  EditorSet blank_editor_set_{};
  
  gfx::IRenderer* renderer_ = nullptr;

  core::YazeProject current_project_;
  EditorContext context_;
  std::unique_ptr<PopupManager> popup_manager_;
  ToastManager toast_manager_;
  MenuBuilder menu_builder_;

  // Settings helpers
  void LoadUserSettings();
  void SaveUserSettings();

  void RefreshWorkspacePresets();
  void SaveWorkspacePreset(const std::string& name);
  void LoadWorkspacePreset(const std::string& name);

  // Workspace management
  void CreateNewSession();
  void DuplicateCurrentSession();
  void CloseCurrentSession();
  void RemoveSession(size_t index);
  void SwitchToSession(size_t index);
  size_t GetCurrentSessionIndex() const;
  size_t GetActiveSessionCount() const;
  void ResetWorkspaceLayout();

  // Multi-session editor management
  std::string GenerateUniqueEditorTitle(EditorType type,
                                        size_t session_index) const;
  void SaveWorkspaceLayout();
  void LoadWorkspaceLayout();
  void ShowAllWindows();
  void HideAllWindows();
  void MaximizeCurrentWindow();
  void RestoreAllWindows();
  void CloseAllFloatingWindows();
  void LoadDeveloperLayout();
  void LoadDesignerLayout();
  void LoadModderLayout();

  // Session management helpers
  bool HasDuplicateSession(const std::string& filepath);
  void RenameSession(size_t index, const std::string& new_name);
  std::string GenerateUniqueEditorTitle(EditorType type, size_t session_index);

  // UI drawing helpers
  void DrawSessionSwitcher();
  void DrawSessionManager();
  void DrawLayoutPresets();
  void DrawSessionRenameDialog();
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_EDITOR_MANAGER_H
