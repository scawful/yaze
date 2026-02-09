#ifndef YAZE_APP_EDITOR_EDITOR_MANAGER_H
#define YAZE_APP_EDITOR_EDITOR_MANAGER_H

#define IMGUI_DEFINE_MATH_OPERATORS

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/agent/agent_ui_controller.h"
#include "app/editor/code/project_file_editor.h"
#include "app/editor/editor.h"
#include "app/editor/menu/activity_bar.h"
#include "app/editor/core/content_registry.h"
#include "app/editor/core/editor_context.h"
#include "app/editor/core/event_bus.h"
#include "app/editor/menu/menu_builder.h"
#include "app/editor/menu/menu_orchestrator.h"
#include "app/editor/menu/right_panel_manager.h"
#include "app/editor/menu/status_bar.h"
#include "app/editor/session_types.h"
#include "app/editor/system/panel_manager.h"
#include "app/editor/system/editor_activator.h"
#include "app/editor/system/editor_registry.h"
#include "app/editor/ui/popup_manager.h"
#include "app/editor/system/project_manager.h"
#include "app/editor/system/proposal_drawer.h"
#include "app/editor/system/rom_file_manager.h"
#include "app/editor/system/session_coordinator.h"
#include "app/editor/ui/toast_manager.h"
#include "app/editor/system/user_settings.h"
#include "app/editor/layout/layout_coordinator.h"
#include "app/editor/layout/layout_manager.h"
#include "app/editor/layout/window_delegate.h"
#include "app/editor/ui/dashboard_panel.h"
#include "app/editor/ui/rom_load_options_dialog.h"
#include "app/editor/ui/project_management_panel.h"
#include "app/editor/ui/selection_properties_panel.h"
#include "app/editor/ui/ui_coordinator.h"
#include "app/editor/ui/welcome_screen.h"
#include "app/editor/ui/workspace_manager.h"
#include "app/emu/emulator.h"
#include "app/startup_flags.h"
#include "rom/rom.h"
#include "core/project.h"
#include "core/version_manager.h"
#include "imgui/imgui.h"
#include "util/log.h"
#include "yaze_config.h"
#include "zelda3/overworld/overworld.h"

// Forward declarations for gRPC-dependent types
namespace yaze {
class CanvasAutomationServiceImpl;
}

namespace yaze::editor {
class AgentEditor;
}

namespace yaze {

// Forward declaration for AppConfig
struct AppConfig;

namespace editor {

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
 * EditorManager subscribes to EventBus events for session lifecycle
 * notifications (SessionSwitchedEvent, SessionCreatedEvent, etc.) and
 * updates cross-cutting concerns accordingly.
 */
class EditorManager {
 public:
  enum class PotItemSaveDecision {
    kSaveWithPotItems,
    kSaveWithoutPotItems,
    kCancel
  };

  // Constructor and destructor must be defined in .cc file for std::unique_ptr
  // with forward-declared types
  EditorManager();
  ~EditorManager();

  void Initialize(gfx::IRenderer* renderer, const std::string& filename = "");

  // Processes startup flags to open a specific editor and panels.
  void OpenEditorAndPanelsFromFlags(const std::string& editor_name,
                                    const std::string& panels_str);
                                   
  // Apply startup actions based on AppConfig
  void ProcessStartupActions(const AppConfig& config);
  void ApplyStartupVisibility(const AppConfig& config);

  void SetAssetLoadMode(AssetLoadMode mode);
  AssetLoadMode asset_load_mode() const { return asset_load_mode_; }
                                   
  absl::Status Update();
  void DrawMainMenuBar();

  auto emulator() -> emu::Emulator& { return emulator_; }
  auto quit() const { return quit_; }
  auto version() const { return version_; }
  

  MenuBuilder& menu_builder() { return menu_builder_; }
  WorkspaceManager* workspace_manager() { return &workspace_manager_; }
  RightPanelManager* right_panel_manager() { return right_panel_manager_.get(); }
  StatusBar* status_bar() { return &status_bar_; }
  ToastManager* toast_manager() { return &toast_manager_; }
  PanelManager* GetPanelManager() { return &panel_manager_; }
  PanelManager& panel_manager() { return panel_manager_; }
  const PanelManager& panel_manager() const { return panel_manager_; }
  
  // Deprecated compatibility wrappers
  PanelManager& card_registry() { return panel_manager_; }
  const PanelManager& card_registry() const { return panel_manager_; }

  // Layout offset calculation for dockspace adjustment
  // Delegates to LayoutCoordinator for cleaner separation of concerns
  float GetLeftLayoutOffset() const {
    return layout_coordinator_.GetLeftLayoutOffset();
  }
  float GetRightLayoutOffset() const {
    return layout_coordinator_.GetRightLayoutOffset();
  }
  float GetBottomLayoutOffset() const {
    return layout_coordinator_.GetBottomLayoutOffset();
  }

  absl::Status SetCurrentRom(Rom* rom);
  auto GetCurrentRom() const -> Rom* {
    return session_coordinator_ ? session_coordinator_->GetCurrentRom()
                                : nullptr;
  }
  auto GetCurrentGameData() const -> zelda3::GameData* {
    return session_coordinator_ ? session_coordinator_->GetCurrentGameData()
                                : nullptr;
  }
  auto GetCurrentEditorSet() const -> EditorSet* {
    return session_coordinator_ ? session_coordinator_->GetCurrentEditorSet()
                                : nullptr;
  }
  auto GetCurrentEditor() const -> Editor* { return current_editor_; }
  std::string GetCurrentRomHash() const { return current_rom_hash_; }
  project::RomRole GetProjectRomRole() const {
    return current_project_.rom_metadata.role;
  }
  project::RomWritePolicy GetProjectRomWritePolicy() const {
    return current_project_.rom_metadata.write_policy;
  }
  std::string GetProjectExpectedRomHash() const {
    return current_project_.rom_metadata.expected_hash;
  }
  bool IsRomHashMismatch() const;
  std::vector<editor::RomFileManager::BackupEntry> GetRomBackups() const;
  absl::Status RestoreRomBackup(const std::string& backup_path);
  absl::Status PruneRomBackups();
  void ConfirmRomWrite();
  void CancelRomWriteConfirm();
  bool IsRomWriteConfirmPending() const { return pending_rom_write_confirm_; }

  // Write conflict warning (ASM-owned address protection)
  const std::vector<core::WriteConflict>& pending_write_conflicts() const {
    return pending_write_conflicts_;
  }
  void BypassWriteConflictOnce() { bypass_write_conflict_once_ = true; }
  void ClearPendingWriteConflicts() { pending_write_conflicts_.clear(); }
  void SetCurrentEditor(Editor* editor) {
    current_editor_ = editor;
    // Update ContentRegistry context for panel access
    ContentRegistry::Context::SetCurrentEditor(editor);
    // Update help panel's editor context for context-aware help
    if (right_panel_manager_ && editor) {
      right_panel_manager_->SetActiveEditor(editor->type());
    }
  }
  size_t GetCurrentSessionId() const {
    return session_coordinator_ ? session_coordinator_->GetActiveSessionIndex()
                                : 0;
  }
  UICoordinator* ui_coordinator() { return ui_coordinator_.get(); }
  yaze::zelda3::Overworld* overworld() const;

  // Session management helpers
  size_t GetCurrentSessionIndex() const;

  // Get current session's feature flags (falls back to global if no session)
  core::FeatureFlags::Flags* GetCurrentFeatureFlags() {
    size_t current_index = GetCurrentSessionIndex();
    if (session_coordinator_ && current_index < session_coordinator_->GetTotalSessionCount()) {
      auto* session = static_cast<RomSession*>(session_coordinator_->GetSession(current_index));
      if (session) {
        return &session->feature_flags;
      }
    }
    return &core::FeatureFlags::get();  // Fallback to global
  }

  void SetFontGlobalScale(float scale) {
    user_settings_.prefs().font_global_scale = scale;
    ImGui::GetIO().FontGlobalScale = scale;
    auto status = user_settings_.Save();
    if (!status.ok()) {
      LOG_WARN("EditorManager", "Failed to save user settings: %s",
               status.ToString().c_str());
    }
  }

  // Workspace management (delegates to WorkspaceManager)
  void RefreshWorkspacePresets() { workspace_manager_.RefreshPresets(); }
  void SaveWorkspacePreset(const std::string& name) {
    workspace_manager_.SaveWorkspacePreset(name);
  }
  void LoadWorkspacePreset(const std::string& name) {
    workspace_manager_.LoadWorkspacePreset(name);
  }

  // Jump-to functionality for cross-editor navigation
  void JumpToDungeonRoom(int room_id);
  void JumpToOverworldMap(int map_id);
  void SwitchToEditor(EditorType editor_type, bool force_visible = false,
                      bool from_dialog = false);
  void DismissEditorSelection();

  // Panel-based editor registry
  static bool IsPanelBasedEditor(EditorType type);
  bool IsSidebarVisible() const {
    return ui_coordinator_ ? ui_coordinator_->IsPanelSidebarVisible() : false;
  }
  void SetSidebarVisible(bool visible) {
    if (ui_coordinator_) {
      ui_coordinator_->SetPanelSidebarVisible(visible);
    }
  }

  // Clean up cards when switching editors
  void HideCurrentEditorPanels();

  // Lazy asset loading helpers
  absl::Status EnsureEditorAssetsLoaded(EditorType type);
  absl::Status EnsureGameDataLoaded();

  // Session management
  void CreateNewSession();
  void DuplicateCurrentSession();
  void CloseCurrentSession();
  void RemoveSession(size_t index);
  void SwitchToSession(size_t index);
  size_t GetActiveSessionCount() const;

  // Workspace layout management
  // Window management - inline delegation (reduces EditorManager bloat)
  void SaveWorkspaceLayout() { workspace_manager_.SaveWorkspaceLayout(); }
  void LoadWorkspaceLayout() { workspace_manager_.LoadWorkspaceLayout(); }
  void ResetWorkspaceLayout();
  void ShowAllWindows() {
    if (ui_coordinator_)
      ui_coordinator_->ShowAllWindows();
  }
  void HideAllWindows() {
    if (ui_coordinator_)
      ui_coordinator_->HideAllWindows();
  }

  // Layout presets (inline delegation)
  void LoadDeveloperLayout() { window_delegate_.LoadDeveloperLayout(); }
  void LoadDesignerLayout() { window_delegate_.LoadDesignerLayout(); }
  void LoadModderLayout() { window_delegate_.LoadModderLayout(); }

  // Panel layout presets (command palette accessible)
  void ApplyLayoutPreset(const std::string& preset_name);
  void ResetCurrentEditorLayout();

  // Helper methods
  std::string GenerateUniqueEditorTitle(EditorType type,
                                        size_t session_index) const;
  bool HasDuplicateSession(const std::string& filepath);
  void RenameSession(size_t index, const std::string& new_name);
  void Quit() { quit_ = true; }

  // Deferred action queue - actions executed safely on next frame
  // Use this to avoid modifying ImGui state during menu/popup rendering
  void QueueDeferredAction(std::function<void()> action) {
    deferred_actions_.push_back(std::move(action));
  }

  // Public for SessionCoordinator to configure new sessions
  void ConfigureSession(RomSession* session);

#ifdef YAZE_WITH_GRPC
  void SetCanvasAutomationService(CanvasAutomationServiceImpl* service) {
    canvas_automation_service_ = service;
  }
#endif

  // UI visibility controls (public for MenuOrchestrator)
  // UI visibility controls - inline for performance (single-line wrappers
  // delegating to UICoordinator)
  void ShowImGuiDemo() {
    if (ui_coordinator_)
      ui_coordinator_->SetImGuiDemoVisible(true);
  }
  void ShowImGuiMetrics() {
    if (ui_coordinator_)
      ui_coordinator_->SetImGuiMetricsVisible(true);
  }

#ifdef YAZE_ENABLE_TESTING
  void ShowTestDashboard() { show_test_dashboard_ = true; }
#endif

#ifdef YAZE_BUILD_AGENT_UI
  void ShowAIAgent();
  void ShowChatHistory();
  AgentEditor* GetAgentEditor() { return agent_ui_.GetAgentEditor(); }
  AgentUiController* GetAgentUiController() { return &agent_ui_; }
#else
  AgentEditor* GetAgentEditor() { return nullptr; }
  AgentUiController* GetAgentUiController() { return nullptr; }
#endif
#ifdef YAZE_BUILD_AGENT_UI
  void ShowProposalDrawer() { proposal_drawer_.Show(); }
#endif

  // ROM and Project operations (public for MenuOrchestrator)
  absl::Status LoadRom();
  absl::Status SaveRom();
  absl::Status SaveRomAs(const std::string& filename);
  absl::Status OpenRomOrProject(const std::string& filename);
  absl::Status CreateNewProject(
      const std::string& template_name = "Basic ROM Hack");
  absl::Status OpenProject();
  absl::Status SaveProject();
  absl::Status SaveProjectAs();

  bool HasPendingPotItemSaveConfirmation() const {
    return pending_pot_item_save_confirm_;
  }
  int pending_pot_item_unloaded_rooms() const {
    return pending_pot_item_unloaded_rooms_;
  }
  int pending_pot_item_total_rooms() const {
    return pending_pot_item_total_rooms_;
  }
  void ResolvePotItemSaveConfirmation(PotItemSaveDecision decision);
  absl::Status ImportProject(const std::string& project_path);
  absl::Status RepairCurrentProject();

  // Project management
  absl::Status LoadProjectWithRom();
  project::YazeProject* GetCurrentProject() { return &current_project_; }
  const project::YazeProject* GetCurrentProject() const { return &current_project_; }
  core::VersionManager* GetVersionManager() { return version_manager_.get(); }
  
  // Show project management panel in right sidebar
  void ShowProjectManagement();
  
  // Show project file editor
  void ShowProjectFileEditor();

 private:
  absl::Status DrawRomSelector() = delete;  // Moved to UICoordinator
  // DrawContextSensitivePanelControl removed - card control moved to sidebar

  // Optional loading_handle for WASM progress tracking (0 = create new)
  absl::Status LoadAssets(uint64_t loading_handle = 0);
  absl::Status LoadAssetsForMode(uint64_t loading_handle = 0);
  absl::Status LoadAssetsLazy(uint64_t loading_handle = 0);
  absl::Status InitializeEditorForType(EditorType type, EditorSet* editor_set,
                                       Rom* rom);
  void ResetAssetState(RomSession* session);
  void MarkEditorInitialized(RomSession* session, EditorType type);
  void MarkEditorLoaded(RomSession* session, EditorType type);
  Editor* GetEditorByType(EditorType type, EditorSet* editor_set) const;
  Editor* ResolveEditorForCategory(const std::string& category);
  void SyncEditorContextForCategory(const std::string& category);
  bool EditorRequiresGameData(EditorType type) const;
  bool EditorInitRequiresGameData(EditorType type) const;

  // Testing system
  void InitializeTestSuites();
  void ApplyStartupVisibilityOverrides();

  // Session event handlers (EventBus subscribers)
  void HandleSessionSwitched(size_t new_index, RomSession* session);
  void HandleSessionCreated(size_t index, RomSession* session);
  void HandleSessionClosed(size_t index);
  // Initialization helpers (extracted from constructor for readability)
  void SubscribeToEvents();
  void InitializeSubsystems();
  void RegisterEditors();
  void RegisterEmulatorPanels();
  void InitializeServices();
  void SetupComponentCallbacks();
  void SetupDialogCallbacks();
  void SetupWelcomeScreenCallbacks();
  void SetupSidebarCallbacks();
  void InitializeShortcutSystem();
  void ProcessInput();
  void UpdateEditorState();
  void DrawInterface();
  void DrawSecondaryWindows();
  void UpdateSystemUIs();
  void RunEmulator();

  void HandleSessionRomLoaded(size_t index, Rom* rom);

  // UI action event handler (EventBus subscriber for UIActionRequestEvent)
  void HandleUIActionRequest(UIActionRequestEvent::Action action);

  bool quit_ = false;

  // Note: All show_* flags are being moved to UICoordinator
  // Access via ui_coordinator_->IsXxxVisible() or SetXxxVisible()

  // Workspace dialog flags (managed by EditorManager, not UI)
  bool show_workspace_layout = false;
  size_t session_to_rename_ = 0;
  char session_rename_buffer_[256] = {};

  // Note: Most UI visibility flags have been moved to UICoordinator
  // Access via ui_coordinator_->IsXxxVisible() or SetXxxVisible()

  // Agent proposal drawer
  ProposalDrawer proposal_drawer_;
  bool show_proposal_drawer_ = false;

  // Agent UI (chat + editor), no-op when agent UI is disabled
  AgentUiController agent_ui_;

  // Project file editor
  ProjectFileEditor project_file_editor_;

  // Note: Editor selection dialog and welcome screen are now managed by
  // UICoordinator Kept here for backward compatibility during transition
  std::unique_ptr<DashboardPanel> dashboard_panel_;
  WelcomeScreen welcome_screen_;
  RomLoadOptionsDialog rom_load_options_dialog_;
  bool show_rom_load_options_ = false;
  StartupVisibility welcome_mode_override_ = StartupVisibility::kAuto;
  StartupVisibility dashboard_mode_override_ = StartupVisibility::kAuto;
  StartupVisibility sidebar_mode_override_ = StartupVisibility::kAuto;
  AssetLoadMode asset_load_mode_ = AssetLoadMode::kFull;

  // Properties panel for selection editing
  SelectionPropertiesPanel selection_properties_panel_;

  // Project management panel for version control and ROM management
  std::unique_ptr<ProjectManagementPanel> project_management_panel_;

  std::string version_ = "";
  absl::Status status_;
  emu::Emulator emulator_;

 public:
 private:
  Editor* current_editor_ = nullptr;
  // Tracks which session is currently active so delegators (menus, popups,
  // shortcuts) stay in sync without relying on per-editor context.

  gfx::IRenderer* renderer_ = nullptr;

  project::YazeProject current_project_;
  std::unique_ptr<core::VersionManager> version_manager_;
  EditorDependencies::SharedClipboard shared_clipboard_;
  std::unique_ptr<PopupManager> popup_manager_;
  ToastManager toast_manager_;
  MenuBuilder menu_builder_;
  ShortcutManager shortcut_manager_;
  UserSettings user_settings_;

  // New delegated components (dependency injection architecture)
  PanelManager panel_manager_;  // Panel management with session awareness
  EditorRegistry editor_registry_;
  std::unique_ptr<MenuOrchestrator> menu_orchestrator_;
  ProjectManager project_manager_;
  RomFileManager rom_file_manager_;
  std::unique_ptr<UICoordinator> ui_coordinator_;
  WindowDelegate window_delegate_;
  EditorActivator editor_activator_;
  std::unique_ptr<SessionCoordinator> session_coordinator_;
  std::unique_ptr<LayoutManager>
      layout_manager_;  // DockBuilder layout management
  LayoutCoordinator layout_coordinator_;  // Facade for layout operations
  std::unique_ptr<RightPanelManager>
      right_panel_manager_;  // Right-side panel system
  StatusBar status_bar_;    // Bottom status bar
  std::unique_ptr<ActivityBar> activity_bar_;
  WorkspaceManager workspace_manager_{&toast_manager_};

  emu::input::InputConfig BuildInputConfigFromSettings() const;
  void PersistInputConfig(const emu::input::InputConfig& config);
  void UpdateCurrentRomHash();
  absl::Status CheckRomWritePolicy();
  absl::Status CheckOracleRomSafetyPreSave(Rom* rom);

  float autosave_timer_ = 0.0f;
  bool settings_dirty_ = false;
  float settings_dirty_timestamp_ = 0.0f;

  // Save safety prompt state
  bool pending_pot_item_save_confirm_ = false;
  int pending_pot_item_unloaded_rooms_ = 0;
  int pending_pot_item_total_rooms_ = 0;
  bool bypass_pot_item_confirm_once_ = false;
  bool suppress_pot_item_save_once_ = false;
  bool pending_rom_write_confirm_ = false;
  bool bypass_rom_write_confirm_once_ = false;
  std::string current_rom_hash_;

  // Write conflict warning state (ASM-owned address protection)
  std::vector<core::WriteConflict> pending_write_conflicts_;
  bool bypass_write_conflict_once_ = false;

  // Deferred action queue - executed at the start of each frame
  std::vector<std::function<void()>> deferred_actions_;

  // Core Event Bus and Context
  EventBus event_bus_;
  std::unique_ptr<GlobalEditorContext> editor_context_;

#ifdef YAZE_WITH_GRPC
  CanvasAutomationServiceImpl* canvas_automation_service_ = nullptr;
#endif

  // RAII helper for clean session context switching
  class SessionScope {
   public:
    SessionScope(EditorManager* manager, size_t session_id);
    ~SessionScope();

   private:
    EditorManager* manager_;
    Rom* prev_rom_;
    EditorSet* prev_editor_set_;
    size_t prev_session_id_;
  };

  void ConfigureEditorDependencies(EditorSet* editor_set, Rom* rom,
                                   size_t session_id);
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_EDITOR_MANAGER_H
