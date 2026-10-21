#include "app/editor/editor_manager.h"

#include <memory>
#include <string>

#include "app/gfx/resource/arena.h"
#include "app/gfx/util/palette_manager.h"
#include "core/features.h"
#include "core/rom_settings.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"

#ifdef YAZE_ENABLE_TESTING
#include "app/testing/test_manager.h"
#endif

namespace yaze::editor {
namespace {

zelda3::CustomObjectManager::State BuildCustomObjectRuntimeState(
    const project::YazeProject& project) {
  return {
      .base_path = project.custom_objects_folder.empty()
                       ? ""
                       : project.GetAbsolutePath(project.custom_objects_folder),
      .custom_file_map = project.custom_object_files,
      .dungeon_tile_patterns =
          project.dungeon_tile_patterns.empty()
              ? ""
              : project.GetAbsolutePath(project.dungeon_tile_patterns),
  };
}

}  // namespace

void EditorManager::BindProjectContextToSession(
    RomSession* session, const project::YazeProject& project) {
  if (!session) {
    return;
  }
  const bool needs_version_manager =
      !session->project_context.has_value() || !session->version_manager;
  if (session->project_context.has_value()) {
    *session->project_context = project;
  } else {
    session->project_context.emplace(project);
  }
  if (needs_version_manager) {
    session->version_manager =
        std::make_unique<core::VersionManager>(&*session->project_context);
  }
  session->feature_flags = project.feature_flags;
  // Configure after the stable context and VersionManager exist. EditorSet
  // instances are created before SessionCreatedEvent, so their first
  // dependency pass can legitimately have no project yet.
  ConfigureSession(session);
}

void EditorManager::ApplyCurrentProjectRuntimeContext() {
  core::FeatureFlags::get() = current_project_.feature_flags;
  zelda3::DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();

  user_settings_.prefs().font_global_scale =
      current_project_.workspace_settings.font_global_scale;
  user_settings_.prefs().autosave_enabled =
      current_project_.workspace_settings.autosave_enabled;
  user_settings_.prefs().autosave_interval =
      current_project_.workspace_settings.autosave_interval_secs;
  user_settings_.prefs().backup_before_save =
      current_project_.workspace_settings.backup_on_save;
  if (ImGui::GetCurrentContext() != nullptr) {
    ImGui::GetIO().FontGlobalScale = user_settings_.prefs().font_global_scale;
  }

  core::RomSettings::Get().SetAddressOverrides(
      current_project_.rom_address_overrides);
  auto& custom_object_manager = zelda3::CustomObjectManager::Get();
  const auto custom_object_state =
      BuildCustomObjectRuntimeState(current_project_);
  if (session_coordinator_ != nullptr &&
      session_coordinator_->GetActiveRomSession() != nullptr) {
    custom_object_manager.ActivateRuntimeContext(
        session_coordinator_->GetActiveRomSession()->session_id(),
        custom_object_state);
  } else {
    custom_object_manager.RestoreState(custom_object_state);
  }

  if (current_project_.project_opened()) {
    rom_lifecycle_.ApplyDefaultBackupPolicy(
        current_project_.workspace_settings.backup_on_save,
        current_project_.GetAbsolutePath(current_project_.rom_backup_folder),
        current_project_.workspace_settings.backup_retention_count,
        current_project_.workspace_settings.backup_keep_daily,
        current_project_.workspace_settings.backup_keep_daily_days);
  } else {
    ApplyDefaultBackupPolicy();
  }

  rom_lifecycle_.SetProjectContext(&current_project_);
  ContentRegistry::Context::SetCurrentProject(&current_project_);
  SyncLayoutScopeFromCurrentProject();
  RefreshHackWorkflowBackend();
  if (project_management_panel_) {
    const auto* session = session_coordinator_
                              ? session_coordinator_->GetActiveRomSession()
                              : nullptr;
    project_management_panel_->SetProject(
        &current_project_, session != nullptr && session->project_dirty);
    project_management_panel_->SetVersionManager(version_manager_);
    project_management_panel_->SetRom(GetCurrentRom());
  }
}

bool EditorManager::RestoreProjectContextForSession(RomSession* session) {
  if (!session || !session->project_context.has_value()) {
    if (!session) {
      current_project_ = project::YazeProject();
      RestoreProjectEditingStateForSession(nullptr);
    }
    active_project_context_session_id_.reset();
    runtime_feature_flags_session_id_.reset();
    rom_lifecycle_.SetProjectContext(nullptr);
    version_manager_ = nullptr;
    if (project_management_panel_) {
      project_management_panel_->SetVersionManager(nullptr);
    }
    return false;
  }

  if (!session->version_manager) {
    session->version_manager =
        std::make_unique<core::VersionManager>(&*session->project_context);
    ConfigureSession(session);
  }
  current_project_ = *session->project_context;
  version_manager_ = session->version_manager.get();
  session->feature_flags = current_project_.feature_flags;
  active_project_context_session_id_ = session->session_id();
  runtime_feature_flags_session_id_ = session->session_id();
  ApplyCurrentProjectRuntimeContext();
  RestoreProjectEditingStateForSession(session);
  return true;
}

void EditorManager::RestoreProjectEditingStateForSession(RomSession* session) {
  if (!session) {
    if (project_management_panel_) {
      project_management_panel_->SetProject(nullptr);
    }
    project_file_editor_.ResetForProject(nullptr);
    return;
  }

  if (project_management_panel_) {
    project_management_panel_->SetProject(&current_project_,
                                          session->project_dirty);
  }
  if (session->project_file_editor_state.initialized) {
    project_file_editor_.RestoreState(session->project_file_editor_state,
                                      &current_project_);
  } else {
    project_file_editor_.ResetForProject(&current_project_);
  }
}

void EditorManager::HandleSessionSwitched(size_t new_index, RomSession* session,
                                          bool transient) {
  agent_ui_->SetRomContext(session ? &session->rom : nullptr);
  // Confirmation consent and Save As targets belong to the ROM that started
  // the request. Never carry them across a user session switch. Frame-level
  // context switches are temporary and must not consume the active request.
  if (!transient) {
    ++project_rom_selection_generation_;
    pending_project_rom_selection_.reset();
    pending_project_open_transition_ = false;
    pending_project_open_previous_session_id_.reset();
    CancelPendingRomSave(/*hide_popups=*/true);
    CaptureRuntimeFeatureFlags();
    CaptureActiveProjectContext();
    if (!RestoreProjectContextForSession(session) && session != nullptr) {
      LOG_ERROR("EditorManager",
                "Session %zu has no project context; saves are disabled",
                new_index);
    }
  } else {
    // Frame iteration must not copy full projects or reset project-wide caches.
    // Feature flags are cheap and directly affect per-editor draw/save paths.
    CaptureRuntimeFeatureFlags();
    if (session) {
      core::FeatureFlags::get() = session->feature_flags;
      runtime_feature_flags_session_id_ = session->session_id();
      const auto custom_object_state =
          session->project_context.has_value()
              ? BuildCustomObjectRuntimeState(*session->project_context)
              : zelda3::CustomObjectManager::State{};
      zelda3::CustomObjectManager::Get().ActivateRuntimeContext(
          session->session_id(), custom_object_state);
    } else {
      runtime_feature_flags_session_id_.reset();
      zelda3::CustomObjectManager::Get().ActivateStandaloneContext();
    }
    zelda3::DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
  }

  // User switches and per-frame visits must select the same session's display
  // cache. Its stable bitmap addresses also keep queued uploads/draws valid.
  if (session) {
    session->ActivateGraphicsDisplay();
  } else {
    gfx::Arena::Get().BindGraphicsSheets(nullptr, nullptr);
  }

  // Explicit navigation completes the gesture. Per-frame visits to another
  // session must not split a held color-picker drag into separate undo steps.
  if (!transient) {
    gfx::PaletteManager::Get().CommitColorEdit();
  }
  // Palette edit history and dirty tracking are session-owned. Select the
  // matching state before any session-specific editor or save action runs.
  gfx::PaletteManager::Get().ActivateSession(session ? &session->game_data
                                                     : nullptr);

  // Update RightDrawerManager with the new session's settings editor
  if (right_drawer_manager_) {
    right_drawer_manager_->SetSettingsPanel(
        session ? session->editors.GetSettingsPanel() : nullptr);
  }

  // Update properties panel with new ROM
  if (session) {
    if (!transient) {
      selection_properties_panel_.ClearSelection();
    }
    selection_properties_panel_.SetRom(&session->rom);
  } else {
    selection_properties_panel_.ClearSelection();
    selection_properties_panel_.SetRom(nullptr);
  }

  // Update ContentRegistry context with current session's ROM and GameData
  ContentRegistry::Context::SetRom(session ? &session->rom : nullptr);
  ContentRegistry::Context::SetGameData(session ? &session->game_data
                                                : nullptr);

  // current_editor_ is also session-owned. Resolve it from the newly active
  // EditorSet, or clear it when the current category has no editor backing.
  const std::string active_category = window_manager_.GetActiveCategory();
  Editor* active_editor = ResolveEditorForCategory(active_category);
  SetCurrentEditor(active_editor);
  ContentRegistry::Context::SetEditorWindowContext(active_category,
                                                   active_editor);

  // Keep room/sprite labels in sync with active session context.
  RefreshResourceLabelProvider();

  const std::string category = window_manager_.GetActiveCategory();
  // Per-frame visits must keep the live visibility flags. Replaying saved
  // preferences here resurrects windows immediately after an ImGui close.
  if (!transient && session != nullptr && !category.empty() &&
      category != WorkspaceWindowManager::kDashboardCategory) {
    auto it = user_settings_.prefs().panel_visibility_state.find(category);
    if (it != user_settings_.prefs().panel_visibility_state.end()) {
      const size_t session_id = session ? session->session_id() : new_index;
      window_manager_.RestoreVisibilityState(session_id, it->second);
    }
  }

  // A user-facing ROM-session switch does not reactivate the editor through
  // EditorActivator::SwitchToEditor. Refresh an existing layout context so
  // optional panels opened afterward dock against this session's live tree.
  // First-time layout construction must remain on the normal activation path,
  // after EnsureEditorAssetsLoaded has initialized and registered the panels.
  // Transient switches are frame-iteration implementation details and must not
  // replace the user-facing session context.
  const EditorType active_editor_type =
      active_editor ? active_editor->type() : EditorType::kUnknown;
  const size_t active_editor_index = EditorTypeIndex(active_editor_type);
  const bool active_session_editor_initialized =
      session != nullptr &&
      active_editor_index < session->editor_initialized.size() &&
      session->editor_initialized[active_editor_index];
  if (!transient && active_editor != nullptr &&
      EditorRegistry::IsPanelBasedEditor(active_editor_type) &&
      active_session_editor_initialized &&
      layout_manager_->IsLayoutInitialized(active_editor_type)) {
    editor_activator_.InitializeEditorLayout(active_editor_type);
  }

#ifdef YAZE_ENABLE_TESTING
  test::TestManager::Get().SetCurrentRom(session ? &session->rom : nullptr);
#endif

  // Restore this session's loaded/committed identity, never its pending bytes,
  // before the next save-policy check (including transient activation).
  RestoreCurrentRomIdentity();

  LOG_DEBUG("EditorManager", "Session switched to %zu via EventBus", new_index);
}

void EditorManager::HandleSessionCreated(size_t index, RomSession* session) {
  if (pending_rom_save_.has_value() && !PendingRomSaveMatchesActiveSession()) {
    CancelPendingRomSave(/*hide_popups=*/true);
  }
  CaptureActiveProjectContext();
  if (session && !session->project_context.has_value()) {
    // A parsed project deliberately detaches the prior owner before creating
    // its ROM session. Normal raw-ROM and empty-session opens must instead get
    // a neutral context, not a copy of whichever project happened to be active.
    if (!active_project_context_session_id_.has_value() &&
        current_project_.project_opened()) {
      BindProjectContextToSession(session, current_project_);
    } else {
      project::YazeProject neutral_project;
      BindProjectContextToSession(session, neutral_project);
    }
  }

  const size_t session_id = session ? session->session_id() : index;
  window_manager_.RegisterRegistryWindowContentsForSession(session_id);
  window_manager_.RestorePinnedState(user_settings_.prefs().pinned_panels);
  if (session_coordinator_ &&
      index == session_coordinator_->GetActiveSessionIndex()) {
    RestoreProjectContextForSession(session);
    gfx::PaletteManager::Get().ActivateSession(session ? &session->game_data
                                                       : nullptr);
    RestoreCurrentRomIdentity();
  }
  LOG_INFO("EditorManager", "Session %zu created via EventBus", index);
}

void EditorManager::HandleSessionClosed(size_t index) {
  CancelPendingRomSave(/*hide_popups=*/true);

  // SessionClosedEvent is emitted before the owning RomSession is erased and
  // before the active index is adjusted. Bind the surviving session now so the
  // closing session's destructor cannot leave PaletteManager unbound.
  RomSession* palette_session = nullptr;
  if (session_coordinator_) {
    const size_t session_count = session_coordinator_->GetTotalSessionCount();
    auto* active_session = session_coordinator_->GetActiveRomSession();
    auto* closing_session =
        session_coordinator_->IsValidSessionIndex(index)
            ? static_cast<RomSession*>(session_coordinator_->GetSession(index))
            : nullptr;
    if (closing_session != nullptr) {
      zelda3::CustomObjectManager::Get().RemoveRuntimeContext(
          closing_session->session_id());
    }
    if (closing_session != nullptr &&
        active_project_context_session_id_ == closing_session->session_id()) {
      CaptureActiveProjectContext();
      active_project_context_session_id_.reset();
      if (runtime_feature_flags_session_id_ == closing_session->session_id()) {
        runtime_feature_flags_session_id_.reset();
      }
      version_manager_ = nullptr;
      if (project_management_panel_) {
        project_management_panel_->SetVersionManager(nullptr);
      }
    }
    palette_session = active_session;
    if (closing_session != nullptr && active_session == closing_session &&
        session_count > 1) {
      const size_t replacement_index = index > 0 ? index - 1 : 1;
      palette_session = static_cast<RomSession*>(
          session_coordinator_->GetSession(replacement_index));
    }
  }
  gfx::PaletteManager::Get().ActivateSession(
      palette_session ? &palette_session->game_data : nullptr);

  // Update ContentRegistry - it will be set to new active ROM on next switch
  // If no sessions remain, clear the context
  if (session_coordinator_ &&
      session_coordinator_->GetTotalSessionCount() == 0) {
    ContentRegistry::Context::Clear();
  }

  // Avoid dangling/stale label map pointers after session close.
  RefreshResourceLabelProvider();

#ifdef YAZE_ENABLE_TESTING
  // Update test manager - it will get the new current ROM on next switch
  test::TestManager::Get().SetCurrentRom(GetCurrentRom());
#endif

  LOG_INFO("EditorManager", "Session %zu closed via EventBus", index);
}

void EditorManager::HandleSessionRomLoaded(size_t index, Rom* rom) {
  auto* session =
      static_cast<RomSession*>(session_coordinator_->GetSession(index));
  ResetAssetState(session);

  // Update ContentRegistry when ROM is loaded (if this is the active session)
  if (rom && session_coordinator_ &&
      index == session_coordinator_->GetActiveSessionIndex()) {
    ContentRegistry::Context::SetRom(rom);
    // Also update GameData from the session
    if (session) {
      ContentRegistry::Context::SetGameData(&session->game_data);
    }
    RefreshResourceLabelProvider();
  }

#ifdef YAZE_ENABLE_TESTING
  if (rom) {
    test::TestManager::Get().SetCurrentRom(rom);
  }
#endif

  LOG_INFO("EditorManager", "ROM loaded in session %zu via EventBus", index);
}

void EditorManager::SwitchToSession(size_t index) {
  if (!session_coordinator_) {
    return;
  }

  const size_t current_index = GetCurrentSessionIndex();
  if (index == current_index ||
      !session_coordinator_->IsValidSessionIndex(index)) {
    return;
  }

  const size_t current_session_id = GetCurrentSessionId();
  const size_t target_session_id = session_coordinator_->GetSessionId(index);
  if (!MaybeGuardPendingSessionAction(
          {PendingUnsavedSessionAction::Type::kSwitchSession,
           current_session_id, target_session_id})) {
    return;
  }

  session_coordinator_->SwitchToSession(index);
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
                                          size_t session_index)
    : manager_(manager),
      prev_rom_(manager->GetCurrentRom()),
      prev_editor_set_(manager->GetCurrentEditorSet()),
      prev_session_index_(manager->GetCurrentSessionIndex()) {
  // Set new session context
  manager_->session_coordinator_->SwitchToSession(session_index);
}

EditorManager::SessionScope::~SessionScope() {
  // Restore previous context
  manager_->session_coordinator_->SwitchToSession(prev_session_index_);
}

}  // namespace yaze::editor
