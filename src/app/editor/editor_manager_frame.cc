#include "app/editor/editor_manager.h"

#include <chrono>
#include <string>
#include <unordered_set>
#include <utility>

#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/system/emulator_runtime_policy.h"
#include "app/editor/system/shortcut_manager.h"
#include "app/gfx/debug/performance/performance_dashboard.h"
#include "app/gui/animation/animator.h"
#include "app/gui/core/icons.h"
#include "app/platform/timing.h"
#include "app/testing/test_manager.h"
#include "zelda3/resource_labels.h"

#if defined(YAZE_BUILD_AGENT_UI)
#include "app/editor/agent/agent_chat.h"
#endif

namespace yaze {
namespace editor {
namespace {

void UpdateBuildWorkflowStatus(StatusBar* status_bar,
                               ProjectManagementPanel* project_panel,
                               const ProjectWorkflowStatus& status) {
  ContentRegistry::Context::SetBuildWorkflowStatus(status);
  if (status_bar != nullptr) {
    status_bar->SetBuildStatus(status);
  }
  if (project_panel != nullptr) {
    project_panel->SetBuildStatus(status);
  }
}

void AppendWorkflowHistoryEntry(const std::string& kind,
                                const ProjectWorkflowStatus& status,
                                const std::string& output_log) {
  ContentRegistry::Context::AppendWorkflowHistory(
      {.kind = kind,
       .status = status,
       .output_log = output_log,
       .timestamp = std::chrono::system_clock::now()});
}

}  // namespace

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
 * 7. Draw sidebar (WorkspaceWindowManager) - card-based editor UI
 *
 * Note: EditorManager retains the main loop to coordinate multi-session
 * updates, but delegates specific drawing/state operations to specialized
 * components.
 */
absl::Status EditorManager::Update() {
  ProcessInput();
  UpdateEditorState();
  DrawInterface();

  return status_;
}

void EditorManager::HandleHostVisibilityChanged(bool visible) {
  // Space/focus transitions can leave mid-animation surfaces ghosted.
  // Reset transient animation state to a stable endpoint.
  gui::GetAnimator().ClearWorkspaceTransitionState();
  if (right_drawer_manager_) {
    right_drawer_manager_->OnHostVisibilityChanged(visible);
  }
}

void EditorManager::ProcessInput() {
  // Update timing manager for accurate delta time across the application
  TimingManager::Get().Update();

  // Execute keyboard shortcuts (registered via ShortcutConfigurator)
  ExecuteShortcuts(shortcut_manager_);
}

void EditorManager::UpdateEditorState() {
  status_ = absl::OkStatus();
  PollProjectWorkflowTasks();
  if (ui_coordinator_) {
    ui_coordinator_->RefreshWorkflowActions();
  }
  // Check for layout rebuild requests and execute if needed (delegated to LayoutCoordinator)
  bool is_emulator_visible =
      ui_coordinator_ && ui_coordinator_->IsEmulatorVisible();
  EditorType current_type =
      current_editor_ ? current_editor_->type() : EditorType::kUnknown;
  layout_coordinator_.ProcessLayoutRebuild(current_type, is_emulator_visible);

  // Periodic user settings auto-save
  if (settings_dirty_) {
    const float elapsed = TimingManager::Get().GetElapsedTime();
    if (elapsed - settings_dirty_timestamp_ >= 1.0f) {
      auto save_status = user_settings_.Save();
      if (!save_status.ok()) {
        LOG_WARN("EditorManager", "Failed to save user settings: %s",
                 save_status.ToString().c_str());
      }
      settings_dirty_ = false;
    }
  }

  // Update agent editor dashboard
  status_ = agent_ui_.Update();

  // Ensure TestManager always has the current ROM
  static Rom* last_test_rom = nullptr;
  auto* current_rom = GetCurrentRom();
  if (last_test_rom != current_rom) {
    LOG_DEBUG(
        "EditorManager",
        "EditorManager::Update - ROM changed, updating TestManager: %p -> %p",
        (void*)last_test_rom, (void*)current_rom);
    test::TestManager::Get().SetCurrentRom(current_rom);
    last_test_rom = current_rom;
  }

  // Autosave uses the same guarded serialization, validation, backup, and
  // atomic commit pipeline as an explicit save. Writing Rom::SaveToFile here
  // would skip editor model serialization and could persist a partially
  // failed buffer while clearing its dirty bit.
  const bool current_session_has_unsaved_work =
      current_rom && SessionHasPendingUnsavedWork(GetCurrentSessionIndex());
  const auto* current_session =
      session_coordinator_ ? session_coordinator_->GetActiveRomSession()
                           : nullptr;
  const bool backup_restore_requires_explicit_save =
      current_session != nullptr && current_session->backup_restore_pending;
  if (user_settings_.prefs().autosave_enabled &&
      current_session_has_unsaved_work &&
      !backup_restore_requires_explicit_save) {
    autosave_timer_ += ImGui::GetIO().DeltaTime;
    if (autosave_timer_ >= user_settings_.prefs().autosave_interval) {
      autosave_timer_ = 0.0f;
      auto st = AutosaveActiveSession();
      if (st.ok()) {
        toast_manager_.Show("Autosave completed", editor::ToastType::kSuccess);
      } else if (absl::IsCancelled(st)) {
        toast_manager_.Show("Autosave paused: confirmation required",
                            editor::ToastType::kWarning, 5.0f);
      } else {
        toast_manager_.Show(
            absl::StrFormat("Autosave failed: %s", st.message()),
            editor::ToastType::kError, 5.0f);
      }
    }
  } else {
    autosave_timer_ = 0.0f;
  }

  // Update ROM context for agent UI
  if (current_rom && current_rom->is_loaded()) {
    agent_ui_.SetRomContext(current_rom);
    agent_ui_.SetProjectContext(&current_project_);
    if (auto* editor_set = GetCurrentEditorSet()) {
      agent_ui_.SetAsarWrapperContext(editor_set->GetAsarWrapper());
      // Backend-agnostic symbol feed. Works for ASAR and z3dk alike; tools
      // prefer this pointer over reaching through the wrapper.
      agent_ui_.SetAssemblySymbolTableContext(
          &editor_set->GetAssemblySymbols());
    }
  }

  // Delegate session updates to SessionCoordinator
  if (session_coordinator_) {
    session_coordinator_->UpdateSessions();
  }
}

void EditorManager::PollProjectWorkflowTasks() {
  if (!active_project_build_) {
    return;
  }

  const auto snapshot = active_project_build_->GetSnapshot();
  UpdateBuildWorkflowStatus(
      &status_bar_, project_management_panel_.get(),
      MakeBuildStatus(
          snapshot.running
              ? "Build running"
              : (snapshot.status.ok() ? "Build succeeded" : "Build failed"),
          snapshot.output_tail.empty()
              ? (snapshot.running
                     ? std::string(
                           "Running the configured project build command")
                     : std::string(snapshot.status.message()))
              : snapshot.output_tail,
          snapshot.running
              ? ProjectWorkflowState::kRunning
              : (snapshot.status.ok() ? ProjectWorkflowState::kSuccess
                                      : ProjectWorkflowState::kFailure),
          snapshot.output_tail, snapshot.running));
  if (project_management_panel_) {
    project_management_panel_->SetBuildLogOutput(snapshot.output);
  }
  ContentRegistry::Context::SetBuildWorkflowLog(snapshot.output);

  if (snapshot.running || active_project_build_reported_) {
    return;
  }

  active_project_build_reported_ = true;
  if (!snapshot.status.ok()) {
    AppendWorkflowHistoryEntry(
        "Build",
        MakeBuildStatus("Build failed", std::string(snapshot.status.message()),
                        ProjectWorkflowState::kFailure, snapshot.output_tail,
                        false),
        snapshot.output);
    toast_manager_.Show(
        absl::StrFormat("Build failed: %s", snapshot.status.message()),
        snapshot.status.code() == absl::StatusCode::kCancelled
            ? ToastType::kInfo
            : ToastType::kError);
    return;
  }

  AppendWorkflowHistoryEntry(
      "Build",
      MakeBuildStatus(
          "Build succeeded",
          snapshot.output_tail.empty() ? std::string("Project build completed")
                                       : snapshot.output_tail,
          ProjectWorkflowState::kSuccess, snapshot.output_tail, false),
      snapshot.output);
  toast_manager_.Show(snapshot.output_tail.empty()
                          ? "Project build completed"
                          : absl::StrFormat("Project build completed: %s",
                                            snapshot.output_tail),
                      ToastType::kSuccess);
}

void EditorManager::DrawInterface() {

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

  // Draw window browser (managed by UICoordinator)
  if (ui_coordinator_ && ui_coordinator_->IsWindowBrowserVisible()) {
    bool show = true;
    if (activity_bar_) {
      activity_bar_->DrawWindowBrowser(GetCurrentSessionId(), &show);
    }
    if (!show) {
      ui_coordinator_->SetWindowBrowserVisible(false);
    }
  }

  // Draw background grid effects
  if (ui_coordinator_) {
    ui_coordinator_->DrawBackground();
  }

  // Draw UICoordinator UI components (Welcome Screen, Command Palette, etc.)
  if (ui_coordinator_) {
    ui_coordinator_->DrawAllUI();
  }

  // Handle Welcome screen early-exit for rendering
  if (ui_coordinator_ && ui_coordinator_->ShouldShowWelcome()) {
    if (right_drawer_manager_) {
      // Close drawers left over from an editor only when the Welcome screen
      // replaces the editor surface. Closing them every frame made every
      // drawer opened from the Welcome screen (File > Settings, Ctrl/Cmd+,,
      // the drawers button) close again before it was drawn. The Welcome
      // screen already lays itself out around the drawer width
      // (GetRightLayoutOffset).
      if (!welcome_was_shown_) {
        right_drawer_manager_->CloseDrawer();
      }
      right_drawer_manager_->SetRom(GetCurrentRom());
      right_drawer_manager_->Draw();
    }
    welcome_was_shown_ = true;
    return;
  }
  welcome_was_shown_ = false;

  DrawSecondaryWindows();
  UpdateSystemUIs();
  RunEmulator();

  // Draw sidebar
  if (ui_coordinator_ && ui_coordinator_->IsPanelSidebarVisible()) {
    auto all_categories = EditorRegistry::GetAllEditorCategories();
    std::unordered_set<std::string> active_editor_categories;

    if (auto* current_editor_set = GetCurrentEditorSet()) {
      if (session_coordinator_) {
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

        if (ui_coordinator_->IsEmulatorVisible()) {
          active_editor_categories.insert("Emulator");
        }
      }
    }

    const bool prefer_dashboard_only =
        dashboard_mode_override_ == StartupVisibility::kShow &&
        startup_editor_hint_.empty() && startup_panel_hints_.empty();
    std::string sidebar_category = window_manager_.GetActiveCategory();
    if (!prefer_dashboard_only && sidebar_category.empty() &&
        !all_categories.empty()) {
      sidebar_category = GetPreferredStartupCategory("", all_categories);
      if (!sidebar_category.empty()) {
        window_manager_.SetActiveCategory(sidebar_category, /*notify=*/false);
        SyncEditorContextForCategory(sidebar_category);
        auto it = user_settings_.prefs().panel_visibility_state.find(
            sidebar_category);
        if (it != user_settings_.prefs().panel_visibility_state.end()) {
          window_manager_.RestoreVisibilityState(
              window_manager_.GetActiveSessionId(), it->second);
        }
      }
    }

    auto has_rom_callback = [this]() -> bool {
      auto* rom = GetCurrentRom();
      return rom && rom->is_loaded();
    };

    if (activity_bar_ && ui_coordinator_->ShouldShowActivityBar()) {
      auto is_rom_dirty_callback = [this]() -> bool {
        auto* rom = GetCurrentRom();
        return rom && rom->is_loaded() && rom->dirty();
      };
      auto pending_dungeon_rooms_callback = [this]() -> int {
        if (auto* editor_set = GetCurrentEditorSet()) {
          if (auto* dungeon_editor = editor_set->GetEditorAs<DungeonEditorV2>(
                  EditorType::kDungeon)) {
            return dungeon_editor->PendingRoomCount();
          }
        }
        return 0;
      };
      activity_bar_->Render(GetCurrentSessionId(), sidebar_category,
                            all_categories, active_editor_categories,
                            has_rom_callback, is_rom_dirty_callback,
                            pending_dungeon_rooms_callback);
    }
  }

  // Draw right panel
  if (right_drawer_manager_) {
    right_drawer_manager_->SetRom(GetCurrentRom());
    right_drawer_manager_->Draw();
  }

  // Update and draw status bar
  status_bar_.SetRom(GetCurrentRom());
  if (session_coordinator_) {
    const size_t session_index = GetCurrentSessionIndex();
    const size_t session_count = session_coordinator_->GetActiveSessionCount();
    status_bar_.SetSessionInfo(
        session_index, session_count,
        session_count > 1 ? session_coordinator_->GetActiveSessionDisplayName()
                          : std::string{});
  }

  bool has_agent_info = false;
#if defined(YAZE_BUILD_AGENT_UI)
  if (auto* agent_editor = agent_ui_.GetAgentEditor()) {
    auto* chat = agent_editor->GetAgentChat();
    const auto* ctx = agent_ui_.GetContext();
    if (ctx) {
      const auto& config = ctx->agent_config();
      bool active = chat && *chat->active();
      status_bar_.SetAgentInfo(config.ai_provider, config.ai_model, active);
      has_agent_info = true;
    }
  }
#endif
  if (!has_agent_info) {
    status_bar_.ClearAgentInfo();
  }
  if (!current_project_.project_opened()) {
    status_bar_.ClearProjectWorkflowStatus();
  }

  // Editor-aware context: let the active editor push its own mode/custom
  // segments for this frame without wiping event-driven cursor/selection/zoom
  // state that older editors still publish through the event bus.
  status_bar_.ClearEditorContributions();
  if (current_editor_) {
    current_editor_->ContributeStatus(&status_bar_);

    StatusBarSegmentOptions editor_opts;
    editor_opts.tooltip = "Click to switch editor (Ctrl+E)";
    editor_opts.on_click = [this]() {
      if (ui_coordinator_) {
        ui_coordinator_->ShowEditorSelection();
      }
    };
    status_bar_.SetActiveEditor(
        EditorRegistry::GetEditorCategory(current_editor_->type()),
        std::move(editor_opts));
  } else {
    status_bar_.ClearActiveEditor();
  }

  const size_t session_index = GetCurrentSessionIndex();
  if (SessionHasPendingUnsavedWork(session_index)) {
    StatusBarSegmentOptions dirty_opts;
    dirty_opts.tooltip = absl::StrFormat(
        "%s. Click to save ROM when ROM-buffer work is pending, otherwise open "
        "the Project drawer.",
        DescribePendingUnsavedWork(session_index));
    dirty_opts.on_click = [this, session_index]() {
      if (SessionHasPendingRomWork(session_index)) {
        auto status = SaveRom();
        if (!status.ok()) {
          toast_manager_.Show(std::string(status.message()), ToastType::kError);
        }
      } else if (right_drawer_manager_) {
        right_drawer_manager_->OpenDrawer(
            RightDrawerManager::DrawerType::kProject);
      }
    };
    status_bar_.SetDirtyScope(CompactPendingUnsavedWorkLabel(session_index),
                              std::move(dirty_opts));
  } else {
    status_bar_.ClearDirtyScope();
  }

  if (right_drawer_manager_ && right_drawer_manager_->IsDrawerExpanded()) {
    const auto active = right_drawer_manager_->GetActiveDrawer();
    if (active != RightDrawerManager::DrawerType::kNone) {
      StatusBarSegmentOptions drawer_opts;
      drawer_opts.tooltip =
          "Right sidebar open — Esc closes, View > Right Sidebar toggles";
      status_bar_.SetCustomSegment("Drawer", GetDrawerTypeName(active),
                                   std::move(drawer_opts));
    }
  }

  status_bar_.Draw();

  // Check if ROM is loaded before drawing panels
  auto* current_editor_set = GetCurrentEditorSet();
  if (!current_editor_set || !GetCurrentRom()) {
    if (window_manager_.GetActiveCategory() == "Agent") {
      window_manager_.DrawVisibleWindows();
    }
    return;
  }

  // Central workspace window drawing
  window_manager_.DrawVisibleWindows();

  // The Performance Dashboard is rendered by DrawSecondaryWindows below, which
  // also drives Update() and syncs the visibility flag back. Rendering here as
  // well gave a second Begin/End on the same window name every frame, stacking
  // duplicate metrics, memory blocks and frame plots with colliding plot IDs.

  // Draw SessionCoordinator UI components
  if (session_coordinator_) {
    session_coordinator_->DrawSessionSwitcher();
    session_coordinator_->DrawSessionManager();
    session_coordinator_->DrawSessionRenameDialog();
  }
}

void EditorManager::DrawMainMenuBar() {
  if (ImGui::BeginMenuBar()) {
    const bool sidebar_visible = window_manager_.IsSidebarVisible();
    const char* icon = sidebar_visible ? ICON_MD_MENU_OPEN : ICON_MD_MENU;
    const char* tooltip = sidebar_visible ? "Hide Activity Bar (Ctrl+B)"
                                          : "Show Activity Bar (Ctrl+B)";
    if (ui_coordinator_ && ui_coordinator_->DrawMenuBarIconButton(
                               icon, tooltip, sidebar_visible)) {
      window_manager_.ToggleSidebarVisibility();
    }

    // Delegate menu building to MenuOrchestrator
    if (menu_orchestrator_) {
      menu_orchestrator_->BuildMainMenu();
    }

    // Delegate menu bar extras to UICoordinator
    if (ui_coordinator_) {
      ui_coordinator_->DrawMenuBarExtras();
    }

    ImGui::EndMenuBar();
  }
}

void EditorManager::DrawSecondaryWindows() {

  // ImGui debug windows
  if (ui_coordinator_) {
    if (ui_coordinator_->IsImGuiDemoVisible()) {
      bool visible = true;
      ImGui::ShowDemoWindow(&visible);
      if (!visible)
        ui_coordinator_->SetImGuiDemoVisible(false);
    }

    if (ui_coordinator_->IsImGuiMetricsVisible()) {
      bool visible = true;
      ImGui::ShowMetricsWindow(&visible);
      if (!visible)
        ui_coordinator_->SetImGuiMetricsVisible(false);
    }
  }

  // Legacy window-based editors
  if (auto* editor_set = GetCurrentEditorSet()) {
    bool* hex_visibility =
        window_manager_.GetWindowVisibilityFlag("memory.hex_editor");
    if (hex_visibility) {
      if (auto* editor = editor_set->GetEditor(EditorType::kHex)) {
        // Keep the legacy panel visibility flag in sync with the window close
        // button (ImGui::Begin will toggle Editor::active_).
        editor->set_active(*hex_visibility);
        editor->Update();
        *hex_visibility = *editor->active();
      }
    }

    if (ui_coordinator_ && ui_coordinator_->IsAsmEditorVisible()) {
      if (auto* editor = editor_set->GetEditor(EditorType::kAssembly)) {
        editor->set_active(true);
        editor->Update();
        if (!*editor->active()) {
          ui_coordinator_->SetAsmEditorVisible(false);
        }
      }
    }
  }

  // Project and performance tools
  project_file_editor_.Draw();
  CaptureActiveProjectEditingState();

  if (ui_coordinator_ && ui_coordinator_->IsPerformanceDashboardVisible()) {
    gfx::PerformanceDashboard::Get().SetVisible(true);
    gfx::PerformanceDashboard::Get().Update();
    gfx::PerformanceDashboard::Get().Render();
    if (!gfx::PerformanceDashboard::Get().IsVisible()) {
      ui_coordinator_->SetPerformanceDashboardVisible(false);
    }
  }

#ifdef YAZE_ENABLE_TESTING
  if (show_test_dashboard_) {
    test::TestManager::Get().UpdateResourceStats();
    test::TestManager::Get().DrawTestDashboard(&show_test_dashboard_);
  }
#endif
}

void EditorManager::UpdateSystemUIs() {
  // Update proposal drawer context
  proposal_drawer_.SetRom(GetCurrentRom());

  // Agent UI popups
  agent_ui_.DrawPopups();

  // Resource label management
  if (ui_coordinator_ && ui_coordinator_->IsResourceLabelManagerVisible() &&
      GetCurrentRom()) {
    bool visible = true;
    GetCurrentRom()->resource_label()->DisplayLabels(&visible);
    if (current_project_.project_opened() &&
        !current_project_.labels_filename.empty()) {
      current_project_.labels_filename =
          GetCurrentRom()->resource_label()->filename_;
    }
    if (!visible)
      ui_coordinator_->SetResourceLabelManagerVisible(false);
  }

  // Layout presets
  if (ui_coordinator_) {
    ui_coordinator_->DrawLayoutPresets();
  }
}

void EditorManager::RunEmulator() {
  auto* current_rom = GetCurrentRom();
  if (!current_rom)
    return;

  // Visible emulator: lazy-init + render + tick via Run(rom).
  // Hidden emulator: pause by default so switching editors does not keep game
  // audio/frame state advancing. Opt-in background keep-alive is a user pref.
  // MusicPlayer drives its own RunAudioFrame path when needed.
  if (ui_coordinator_ && ui_coordinator_->IsEmulatorVisible()) {
    emulator_.Run(current_rom);
    return;
  }

  if (!emulator_.running() || !emulator_.is_snes_initialized()) {
    return;
  }
  if (!ShouldTickEmulatorWhenHidden(
          user_settings_.prefs().emulator_keep_running_in_background)) {
    return;
  }

  if (emulator_.is_audio_focus_mode()) {
    emulator_.RunAudioFrame();
  } else {
    emulator_.RunFrameOnly();
  }
}

}  // namespace editor
}  // namespace yaze
