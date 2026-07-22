// Related header
#include "editor_manager.h"

// C system headers
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>

// C++ standard library headers
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
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
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "imgui/imgui.h"

// Project headers
#include "app/application.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/editor.h"
#include "app/editor/hack/workflow/hack_workflow_backend.h"
#include "app/editor/hack/workflow/hack_workflow_backend_factory.h"
#include "app/editor/hack/workflow/project_workflow_output_panel.h"
#include "app/editor/layout/layout_manager.h"
#include "app/editor/layout/layout_presets.h"
#include "app/editor/menu/activity_bar.h"
#include "app/editor/menu/menu_orchestrator.h"
#include "app/editor/session_types.h"
#include "app/editor/shell/coordinator/ui_coordinator.h"
#include "app/editor/shell/feedback/popup_manager.h"
#include "app/editor/shell/feedback/toast_manager.h"
#include "app/editor/shell/windows/dashboard_panel.h"
#include "app/editor/shell/windows/project_management_panel.h"
#include "app/editor/system/editor_registry.h"
#include "app/editor/system/project_workflow_status.h"
#include "app/editor/system/session/default_editor_factories.h"
#include "app/editor/system/shortcut_configurator.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "app/emu/emulator.h"
#include "app/gfx/debug/performance/performance_dashboard.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/util/palette_manager.h"
#include "app/gui/animation/animator.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/theme_manager.h"
#include "app/platform/font_loader.h"
#include "app/platform/ios/ios_platform_state.h"
#include "app/platform/timing.h"
#include "app/testing/test_manager.h"
#include "core/features.h"
#include "core/project.h"
#include "core/rom_settings.h"
#include "editor/core/content_registry.h"
#include "editor/core/editor_context.h"
#include "editor/events/core_events.h"
#include "editor/layout/layout_coordinator.h"
#include "editor/menu/right_drawer_manager.h"
#include "editor/system/editor_activator.h"
#include "editor/system/shortcut_manager.h"
#include "editor/ui/rom_load_options_dialog.h"
#include "rom/rom.h"
#include "rom/rom_diff.h"
#include "rom/transaction.h"
#include "startup_flags.h"
#include "util/file_util.h"
#include "util/i18n/language_manager.h"
#include "util/log.h"
#include "util/macro.h"
#include "util/rom_hash.h"
#include "yaze_config.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/oracle_rom_safety_preflight.h"
#include "zelda3/dungeon/water_fill_zone.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_entrance.h"
#include "zelda3/overworld/overworld_map.h"
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
#include "app/testing/core_systems_test_suite.h"
#include "app/testing/e2e_test_suite.h"
#include "app/testing/integrated_test_suite.h"
#include "app/testing/rom_dependent_test_suite.h"
#include "app/testing/zscustomoverworld_test_suite.h"
#endif
#ifdef YAZE_ENABLE_GTEST
#include "app/testing/unit_test_suite.h"
#endif
#ifdef YAZE_WITH_GRPC
#include "app/testing/z3ed_test_suite.h"
#endif

// Conditional agent UI headers
#ifdef YAZE_BUILD_AGENT_UI
#include "app/editor/agent/agent_chat.h"
#endif

namespace yaze::editor {

namespace {
class ScopedEditorSaveTransactions {
 public:
  ScopedEditorSaveTransactions() = default;
  ScopedEditorSaveTransactions(const ScopedEditorSaveTransactions&) = delete;
  ScopedEditorSaveTransactions& operator=(const ScopedEditorSaveTransactions&) =
      delete;

  ~ScopedEditorSaveTransactions() {
    if (!committed_) {
      for (auto it = editors_.rbegin(); it != editors_.rend(); ++it) {
        (*it)->RollbackSaveTransaction();
      }
    }
  }

  absl::Status Begin(Editor* editor) {
    if (editor == nullptr) {
      return absl::OkStatus();
    }
    RETURN_IF_ERROR(editor->BeginSaveTransaction());
    editors_.push_back(editor);
    return absl::OkStatus();
  }

  void Commit() {
    for (Editor* editor : editors_) {
      editor->CommitSaveTransaction();
    }
    committed_ = true;
  }

 private:
  std::vector<Editor*> editors_;
  bool committed_ = false;
};

bool HasAnyOverride(const core::RomAddressOverrides& overrides,
                    std::initializer_list<const char*> keys) {
  for (const char* key : keys) {
    if (overrides.addresses.find(key) != overrides.addresses.end()) {
      return true;
    }
  }
  return false;
}

bool IsTransientPanelVisibilityId(absl::string_view panel_id) {
  constexpr absl::string_view kRoomPanelPrefix = "dungeon.room_";
  if (!absl::StartsWith(panel_id, kRoomPanelPrefix) ||
      panel_id.size() <= kRoomPanelPrefix.size()) {
    return false;
  }
  for (char ch : panel_id.substr(kRoomPanelPrefix.size())) {
    if (ch < '0' || ch > '9') {
      return false;
    }
  }
  return true;
}

std::string LastNonEmptyLine(const std::string& text) {
  std::vector<std::string> lines = absl::StrSplit(text, '\n');
  for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
    std::string line = std::string(*it);
    absl::StripAsciiWhitespace(&line);
    if (!line.empty()) {
      return line;
    }
  }
  return "";
}

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

void UpdateRunWorkflowStatus(StatusBar* status_bar,
                             ProjectManagementPanel* project_panel,
                             const ProjectWorkflowStatus& status) {
  ContentRegistry::Context::SetRunWorkflowStatus(status);
  if (status_bar != nullptr) {
    status_bar->SetRunStatus(status);
  }
  if (project_panel != nullptr) {
    project_panel->SetRunStatus(status);
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

bool ProjectUsesCustomObjects(const project::YazeProject& project) {
  return !project.custom_objects_folder.empty() ||
         !project.custom_object_files.empty();
}

constexpr int kTrackCustomObjectId = 0x31;

bool SeedLegacyTrackObjectMapping(project::YazeProject* project,
                                  std::string* warning) {
  if (project == nullptr) {
    return false;
  }
  if (project->custom_objects_folder.empty()) {
    return false;
  }
  if (project->custom_object_files.find(kTrackCustomObjectId) !=
      project->custom_object_files.end()) {
    return false;
  }

  const auto& defaults =
      zelda3::CustomObjectManager::DefaultSubtypeFilenamesForObject(
          kTrackCustomObjectId);
  if (defaults.empty()) {
    return false;
  }

  project->custom_object_files[kTrackCustomObjectId] = defaults;
  if (warning != nullptr) {
    *warning = absl::StrFormat(
        "Project defines custom_objects_folder but is missing "
        "custom_object_files[0x%02X]. Seeded default mapping (%d entries).",
        kTrackCustomObjectId, static_cast<int>(defaults.size()));
  }
  return true;
}

std::vector<std::string> ValidateRomAddressOverrides(
    const core::RomAddressOverrides& overrides, const Rom& rom) {
  std::vector<std::string> warnings;
  if (overrides.addresses.empty()) {
    return warnings;
  }

  const auto rom_size = rom.size();
  auto warn = [&](const std::string& message) {
    warnings.push_back(message);
  };

  auto check_range = [&](const std::string& label, uint32_t addr, size_t span) {
    const size_t addr_size = static_cast<size_t>(addr);
    if (addr_size >= rom_size || addr_size + span > rom_size) {
      warn(absl::StrFormat("ROM override '%s' out of range: 0x%X (size 0x%X)",
                           label, addr, rom_size));
    }
  };

  for (const auto& [key, value] : overrides.addresses) {
    check_range(key, value, 1);
  }

  if (HasAnyOverride(overrides, {core::RomAddressKey::kExpandedMessageStart,
                                 core::RomAddressKey::kExpandedMessageEnd})) {
    // Defaults match the Oracle expanded message bank ($2F8000-$2FFFFF).
    // Kept local to avoid pulling message editor constants into core validation.
    constexpr uint32_t kExpandedMessageStartDefault = 0x178000;
    constexpr uint32_t kExpandedMessageEndDefault = 0x17FFFF;
    const uint32_t start =
        overrides.GetAddress(core::RomAddressKey::kExpandedMessageStart)
            .value_or(kExpandedMessageStartDefault);
    const uint32_t end =
        overrides.GetAddress(core::RomAddressKey::kExpandedMessageEnd)
            .value_or(kExpandedMessageEndDefault);
    if (start >= rom_size || end >= rom_size) {
      warn(absl::StrFormat(
          "Expanded message range out of ROM bounds: 0x%X-0x%X", start, end));
    } else if (end < start) {
      warn(absl::StrFormat("Expanded message range invalid: 0x%X-0x%X", start,
                           end));
    }
  }

  if (auto hook =
          overrides.GetAddress(core::RomAddressKey::kExpandedMusicHook)) {
    check_range(core::RomAddressKey::kExpandedMusicHook, *hook, 4);
    if (*hook < rom_size) {
      auto opcode = rom.ReadByte(*hook);
      if (opcode.ok() && opcode.value() != 0x22) {
        warn(absl::StrFormat(
            "Expanded music hook at 0x%X is not a JSL opcode (0x%02X)", *hook,
            opcode.value()));
      }
    }
  }

  if (auto main =
          overrides.GetAddress(core::RomAddressKey::kExpandedMusicMain)) {
    check_range(core::RomAddressKey::kExpandedMusicMain, *main, 4);
  }

  if (auto aux = overrides.GetAddress(core::RomAddressKey::kExpandedMusicAux)) {
    check_range(core::RomAddressKey::kExpandedMusicAux, *aux, 4);
  }

  if (HasAnyOverride(overrides,
                     {core::RomAddressKey::kOverworldExpandedPtrMarker,
                      core::RomAddressKey::kOverworldExpandedPtrMagic,
                      core::RomAddressKey::kOverworldExpandedPtrHigh,
                      core::RomAddressKey::kOverworldExpandedPtrLow})) {
    const uint32_t marker =
        overrides.GetAddress(core::RomAddressKey::kOverworldExpandedPtrMarker)
            .value_or(zelda3::kExpandedPtrTableMarker);
    const uint32_t magic =
        overrides.GetAddress(core::RomAddressKey::kOverworldExpandedPtrMagic)
            .value_or(zelda3::kExpandedPtrTableMagic);
    check_range("overworld_ptr_marker", marker, 1);
    if (marker < rom_size) {
      if (rom.data()[marker] != static_cast<uint8_t>(magic)) {
        warn(absl::StrFormat(
            "Overworld expanded pointer marker mismatch at 0x%X: expected "
            "0x%02X, found 0x%02X",
            marker, static_cast<uint8_t>(magic), rom.data()[marker]));
      }
    }
  }

  if (HasAnyOverride(overrides,
                     {core::RomAddressKey::kOverworldEntranceMapExpanded,
                      core::RomAddressKey::kOverworldEntrancePosExpanded,
                      core::RomAddressKey::kOverworldEntranceIdExpanded,
                      core::RomAddressKey::kOverworldEntranceFlagExpanded})) {
    const uint32_t flag_addr =
        overrides
            .GetAddress(core::RomAddressKey::kOverworldEntranceFlagExpanded)
            .value_or(zelda3::kOverworldEntranceExpandedFlagPos);
    check_range("overworld_entrance_flag_expanded", flag_addr, 1);
    if (flag_addr < rom_size && rom.data()[flag_addr] == 0xB8) {
      warn(
          absl::StrFormat("Overworld entrance flag at 0x%X is still 0xB8 "
                          "(vanilla); expanded entrance tables may be ignored",
                          flag_addr));
    }
  }

  return warnings;
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

void SeedOracleProjectInRecents() {
  namespace fs = std::filesystem;

  std::vector<fs::path> roots;
  if (const char* home = std::getenv("HOME");
      home != nullptr && std::strlen(home) > 0) {
    roots.push_back(fs::path(home) / "src" / "hobby" / "oracle-of-secrets");
  }

  std::vector<fs::path> candidates;
  std::unordered_set<std::string> seen;
  auto add_candidate = [&](const fs::path& path) {
    std::error_code ec;
    if (!fs::exists(path, ec) || ec) {
      return;
    }
    std::string ext = path.extension().string();
    absl::AsciiStrToLower(&ext);
    if (ext != ".yaze" && ext != ".yazeproj") {
      return;
    }
    const std::string normalized = fs::weakly_canonical(path, ec).string();
    const std::string key = normalized.empty() ? path.string() : normalized;
    if (key.empty() || seen.count(key) > 0) {
      return;
    }
    seen.insert(key);
    candidates.push_back(path);
  };

  for (const auto& root : roots) {
    std::error_code ec;
    if (!fs::exists(root, ec) || ec) {
      continue;
    }

    // Priority candidates first.
    add_candidate(root / "Oracle-of-Secrets.yaze");
    add_candidate(root / "Oracle of Secrets.yaze");
    add_candidate(root / "Oracle-of-Secrets.yazeproj");
    add_candidate(root / "Oracle of Secrets.yazeproj");
    add_candidate(root / "Roms" / "Oracle of Secrets.yaze");

    // Also detect additional local project files nearby.
    fs::recursive_directory_iterator it(
        root, fs::directory_options::skip_permission_denied, ec);
    fs::recursive_directory_iterator end;
    if (ec) {
      continue;
    }
    for (; it != end; it.increment(ec)) {
      if (ec) {
        ec.clear();
        continue;
      }
      if (it.depth() > 2) {
        it.disable_recursion_pending();
        continue;
      }
      add_candidate(it->path());
    }
  }

  if (candidates.empty()) {
    return;
  }

  auto& manager = project::RecentFilesManager::GetInstance();
  // Add in reverse so the first candidate remains most recent.
  for (auto it = candidates.rbegin(); it != candidates.rend(); ++it) {
    manager.AddFile(it->string());
  }
  manager.Save();
}

}  // namespace

std::optional<EditorType> ParseEditorTypeFromString(absl::string_view name) {
  std::string normalized = std::string(name);
  absl::StripAsciiWhitespace(&normalized);
  const std::string lower = absl::AsciiStrToLower(normalized);
  for (int i = 0; i < static_cast<int>(EditorType::kSettings) + 1; ++i) {
    const auto type = static_cast<EditorType>(i);
    const std::string name_candidate =
        absl::AsciiStrToLower(EditorRegistry::GetEditorName(type));
    const std::string category_candidate =
        absl::AsciiStrToLower(EditorRegistry::GetEditorCategory(type));
    if (name_candidate == lower || category_candidate == lower) {
      return type;
    }
  }
  return std::nullopt;
}

// Static registry of editors that use the card-based layout system
// These editors register their cards with WorkspaceWindowManager and manage their
// own windows They do NOT need the traditional ImGui::Begin/End wrapper - they
// create cards internally
bool EditorManager::IsPanelBasedEditor(EditorType type) {
  return EditorRegistry::IsPanelBasedEditor(type);
}

void EditorManager::ResetWorkspaceLayout() {
  layout_coordinator_.ResetWorkspaceLayout();
}

void EditorManager::ApplyLayoutPreset(const std::string& preset_name) {
  layout_coordinator_.ApplyLayoutPreset(preset_name, GetCurrentSessionId());
}

bool EditorManager::ApplyLayoutProfile(const std::string& profile_id) {
  if (!layout_manager_) {
    return false;
  }

  const size_t session_id = GetCurrentSessionId();
  const EditorType current_type =
      current_editor_ ? current_editor_->type() : EditorType::kUnknown;

  LayoutProfile applied_profile;
  if (!layout_manager_->ApplyBuiltInProfile(profile_id, session_id,
                                            current_type, &applied_profile)) {
    return false;
  }

  if (applied_profile.open_agent_chat && right_drawer_manager_) {
    right_drawer_manager_->OpenDrawer(
        RightDrawerManager::DrawerType::kAgentChat);
    const float default_width = RightDrawerManager::GetDefaultDrawerWidth(
        RightDrawerManager::DrawerType::kAgentChat, current_type);
    right_drawer_manager_->SetDrawerWidth(
        RightDrawerManager::DrawerType::kAgentChat,
        std::max(default_width, 480.0f));
  }

  toast_manager_.Show(
      absl::StrFormat("Layout Profile: %s", applied_profile.label),
      ToastType::kSuccess);
  return true;
}

void EditorManager::CaptureTemporaryLayoutSnapshot() {
  if (!layout_manager_) {
    return;
  }
  layout_manager_->CaptureTemporarySessionLayout(GetCurrentSessionId());
  toast_manager_.Show("Captured temporary layout snapshot", ToastType::kInfo);
}

void EditorManager::RestoreTemporaryLayoutSnapshot(bool clear_after_restore) {
  if (!layout_manager_) {
    return;
  }

  if (layout_manager_->RestoreTemporarySessionLayout(GetCurrentSessionId(),
                                                     clear_after_restore)) {
    toast_manager_.Show("Restored temporary layout snapshot",
                        ToastType::kSuccess);
  } else {
    toast_manager_.Show("No temporary layout snapshot available",
                        ToastType::kWarning);
  }
}

void EditorManager::ClearTemporaryLayoutSnapshot() {
  if (!layout_manager_) {
    return;
  }
  layout_manager_->ClearTemporarySessionLayout();
  toast_manager_.Show("Cleared temporary layout snapshot", ToastType::kInfo);
}

bool EditorManager::SaveLayoutSnapshotAs(const std::string& name) {
  if (!layout_manager_)
    return false;
  if (name.empty()) {
    toast_manager_.Show("Snapshot name cannot be empty", ToastType::kWarning);
    return false;
  }
  const bool ok =
      layout_manager_->SaveNamedSnapshot(name, GetCurrentSessionId());
  if (ok) {
    toast_manager_.Show(absl::StrFormat("Saved snapshot '%s'", name),
                        ToastType::kSuccess);
  } else {
    toast_manager_.Show(absl::StrFormat("Failed to save snapshot '%s'", name),
                        ToastType::kError);
  }
  return ok;
}

bool EditorManager::RestoreLayoutSnapshot(const std::string& name,
                                          bool remove_after_restore) {
  if (!layout_manager_)
    return false;
  const bool ok = layout_manager_->RestoreNamedSnapshot(
      name, GetCurrentSessionId(), remove_after_restore);
  if (ok) {
    toast_manager_.Show(absl::StrFormat("Restored snapshot '%s'", name),
                        ToastType::kSuccess);
  } else {
    toast_manager_.Show(absl::StrFormat("Snapshot '%s' not available", name),
                        ToastType::kWarning);
  }
  return ok;
}

bool EditorManager::DeleteLayoutSnapshot(const std::string& name) {
  if (!layout_manager_)
    return false;
  const bool ok = layout_manager_->DeleteNamedSnapshot(name);
  if (ok) {
    toast_manager_.Show(absl::StrFormat("Deleted snapshot '%s'", name),
                        ToastType::kInfo);
  }
  return ok;
}

std::vector<std::string> EditorManager::ListLayoutSnapshots() const {
  if (!layout_manager_)
    return {};
  return layout_manager_->ListNamedSnapshots(GetCurrentSessionId());
}

void EditorManager::SyncLayoutScopeFromCurrentProject() {
  if (!layout_manager_) {
    return;
  }

  if (current_project_.project_opened()) {
    layout_manager_->SetProjectLayoutKey(
        current_project_.MakeStorageKey("layouts"));
  } else {
    layout_manager_->UseGlobalLayouts();
  }
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
  window_manager_.SetActiveCategory("Agent");
  layout_coordinator_.InitializeEditorLayout(EditorType::kAgent);
  for (const auto& window_id :
       LayoutPresets::GetDefaultWindows(EditorType::kAgent)) {
    window_manager_.OpenWindow(window_id);
  }
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
  ContentRegistry::Context::SetGlobalContext(editor_context_.get());
  status_bar_.Initialize(editor_context_.get());

  InitializeSubsystems();
  RegisterEditors();
  SubscribeToEvents();

  // STEP 5: ShortcutConfigurator created later in Initialize() method
  // It depends on all above coordinators being available
}

void EditorManager::InitializeSubsystems() {
  RefreshHackWorkflowBackend();

  // STEP 1: Initialize PopupManager FIRST
  popup_manager_ = std::make_unique<PopupManager>(this);
  popup_manager_->Initialize();  // Registers all popups with PopupID constants

  // STEP 2: Initialize SessionCoordinator (independent of popups)
  session_coordinator_ = std::make_unique<SessionCoordinator>(
      &window_manager_, &toast_manager_, &user_settings_);
  session_coordinator_->SetEditorRegistry(&editor_registry_);

  // STEP 3: Initialize MenuOrchestrator (depends on popup_manager_,
  // session_coordinator_)
  menu_orchestrator_ = std::make_unique<MenuOrchestrator>(
      this, menu_builder_, rom_file_manager_, project_manager_,
      editor_registry_, *session_coordinator_, toast_manager_, *popup_manager_);

  // Wire up the window manager for the View menu window listing
  menu_orchestrator_->SetWindowManager(&window_manager_);
  menu_orchestrator_->SetStatusBar(&status_bar_);
  menu_orchestrator_->SetUserSettings(&user_settings_);

  session_coordinator_->SetEditorManager(this);
  session_coordinator_->SetEventBus(&event_bus_);  // Enable event publishing
  ContentRegistry::Context::SetEventBus(
      &event_bus_);  // Global event bus access

  // Expose UserSettings to cross-editor panels that don't get constructor
  // injection (e.g. Layout Designer, which reads named_layouts). The
  // LayoutManager setter happens below once the unique_ptr is constructed.
  ContentRegistry::Context::SetUserSettings(&user_settings_);

  // STEP 3.5: Initialize RomLifecycleManager (depends on popup_manager_,
  // session_coordinator_, rom_file_manager_, toast_manager_)
  rom_lifecycle_.Initialize({
      .rom_file_manager = &rom_file_manager_,
      .session_coordinator = session_coordinator_.get(),
      .toast_manager = &toast_manager_,
      .popup_manager = popup_manager_.get(),
      .project = &current_project_,
  });

  // STEP 4: Initialize UICoordinator (depends on popup_manager_,
  // session_coordinator_, window_manager_)
  ui_coordinator_ = std::make_unique<UICoordinator>(
      this, rom_file_manager_, project_manager_, editor_registry_,
      window_manager_, *session_coordinator_, window_delegate_, toast_manager_,
      *popup_manager_, shortcut_manager_);

  // STEP 4.5: Initialize LayoutManager (DockBuilder layouts for editors)
  layout_manager_ = std::make_unique<LayoutManager>();
  layout_manager_->SetWindowManager(&window_manager_);
  layout_manager_->UseGlobalLayouts();
  // Expose to cross-editor panels that drive the live dockspace (Layout
  // Designer). Must land after the unique_ptr is constructed.
  ContentRegistry::Context::SetLayoutManager(layout_manager_.get());
  workspace_manager_.set_layout_manager(layout_manager_.get());
  workspace_manager_.set_apply_preset_callback(
      [this](const std::string& preset_name) {
        ApplyLayoutPreset(preset_name);
      });
  window_delegate_.set_apply_preset_callback(
      [this](const std::string& preset_name) {
        ApplyLayoutPreset(preset_name);
      });

  // STEP 4.6: Initialize RightDrawerManager (right-side sliding drawers)
  right_drawer_manager_ = std::make_unique<RightDrawerManager>();
  right_drawer_manager_->SetToastManager(&toast_manager_);
  right_drawer_manager_->SetProposalDrawer(&proposal_drawer_);
  right_drawer_manager_->SetPropertiesPanel(&selection_properties_panel_);
  right_drawer_manager_->SetShortcutManager(&shortcut_manager_);
  selection_properties_panel_.SetAgentCallbacks(
      [this](const std::string& prompt) {
#if defined(YAZE_BUILD_AGENT_UI)
        auto* agent_editor = agent_ui_.GetAgentEditor();
        if (!agent_editor) {
          return;
        }
        auto* chat = agent_editor->GetAgentChat();
        if (!chat) {
          return;
        }
        chat->set_active(true);
        chat->SendMessage(prompt);
#else
        (void)prompt;
#endif
      },
      [this]() {
#if defined(YAZE_BUILD_AGENT_UI)
        if (right_drawer_manager_) {
          right_drawer_manager_->OpenDrawer(
              RightDrawerManager::DrawerType::kAgentChat);
        }
#endif
      });
  status_bar_.SetAgentToggleCallback([this]() {
    if (right_drawer_manager_) {
      right_drawer_manager_->ToggleDrawer(
          RightDrawerManager::DrawerType::kAgentChat);
    } else {
      agent_ui_.ShowAgent();
    }
  });

  // Initialize ProjectManagementPanel for project/version management
  project_management_panel_ = std::make_unique<ProjectManagementPanel>();
  project_management_panel_->SetToastManager(&toast_manager_);
  project_file_editor_.SetSaveGuardCallback(
      [this](const std::string& filepath, const std::string& contents) {
        return PrepareRawProjectFileSave(filepath, contents);
      });
  project_file_editor_.SetSaveCompleteCallback(
      [this](const std::string& filepath, const std::string& contents) {
        return CommitRawProjectFileSave(filepath, contents);
      });
  window_manager_.RegisterWindowContent(
      std::make_unique<workflow::ProjectWorkflowOutputPanel>());
  project_management_panel_->SetSwapRomCallback([this]() {
    // Prompt user to select a new ROM for the project
    auto rom_path = util::FileDialogWrapper::ShowOpenFileDialog(
        util::MakeRomFileDialogOptions(false));
    if (!rom_path.empty()) {
      auto status = SwapProjectRom(rom_path);
      if (status.ok()) {
        toast_manager_.Show("Project ROM updated and reloaded.",
                            ToastType::kSuccess);
      } else {
        toast_manager_.Show(absl::StrFormat("Failed to update project ROM: %s",
                                            status.message()),
                            ToastType::kError);
      }
    }
  });
  project_management_panel_->SetReloadRomCallback([this]() {
    if (current_project_.project_opened() &&
        !current_project_.rom_filename.empty()) {
      auto status = ReloadProjectRom();
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
    return status;
  });
  project_management_panel_->SetBrowseFolderCallback(
      [this](const std::string& type) {
        auto folder_path = util::FileDialogWrapper::ShowOpenFolderDialog();
        if (!folder_path.empty()) {
          if (type == "code") {
            current_project_.code_folder = folder_path;
            // Update assembly editor path
            if (auto* editor_set = GetCurrentEditorSet()) {
// iOS: avoid blocking folder enumeration on the UI thread.
#if !(defined(__APPLE__) && TARGET_OS_IOS == 1)
              editor_set->OpenAssemblyFolder(folder_path);
#endif
              window_manager_.SetFileBrowserPath("Assembly", folder_path);
            }
          } else if (type == "assets") {
            current_project_.assets_folder = folder_path;
          }
          MarkCurrentProjectDirty();
          if (project_management_panel_) {
            project_management_panel_->SetProject(&current_project_, true);
          }
          toast_manager_.Show(absl::StrFormat("%s folder set: %s", type.c_str(),
                                              folder_path.c_str()),
                              ToastType::kSuccess);
        }
      });
  project_management_panel_->SetBuildProjectCallback(
      [this]() { QueueBuildCurrentProject(); });
  project_management_panel_->SetCancelBuildCallback(
      [this]() { CancelQueuedProjectBuild(); });
  project_management_panel_->SetRunProjectCallback(
      [this]() { (void)RunCurrentProject(); });
  ContentRegistry::Context::SetStartBuildWorkflowCallback(
      [this]() { QueueBuildCurrentProject(); });
  ContentRegistry::Context::SetRunProjectWorkflowCallback(
      [this]() { (void)RunCurrentProject(); });
  ContentRegistry::Context::SetShowWorkflowOutputCallback([this]() {
    window_manager_.OpenWindow("workflow.output");
    window_manager_.MarkWindowRecentlyUsed("workflow.output");
  });
  ContentRegistry::Context::SetCancelBuildWorkflowCallback(
      [this]() { CancelQueuedProjectBuild(); });
  right_drawer_manager_->SetProjectManagementPanel(
      project_management_panel_.get());

  // STEP 4.6.1: Initialize LayoutCoordinator (facade for layout operations)
  LayoutCoordinator::Dependencies layout_deps;
  layout_deps.layout_manager = layout_manager_.get();
  layout_deps.window_manager = &window_manager_;
  layout_deps.ui_coordinator = ui_coordinator_.get();
  layout_deps.toast_manager = &toast_manager_;
  layout_deps.status_bar = &status_bar_;
  layout_deps.right_drawer_manager = right_drawer_manager_.get();
  layout_coordinator_.Initialize(layout_deps);

  // STEP 4.6.2: Initialize EditorActivator (editor switching and jump navigation)
  EditorActivator::Dependencies activator_deps;
  activator_deps.window_manager = &window_manager_;
  activator_deps.layout_manager = layout_manager_.get();
  activator_deps.ui_coordinator = ui_coordinator_.get();
  activator_deps.right_drawer_manager = right_drawer_manager_.get();
  activator_deps.toast_manager = &toast_manager_;
  activator_deps.event_bus = &event_bus_;
  activator_deps.ensure_editor_assets_loaded = [this](EditorType type) {
    return EnsureEditorAssetsLoaded(type);
  };
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
  activity_bar_ = std::make_unique<ActivityBar>(
      window_manager_,
      [this]() -> bool {
        if (auto* editor_set = GetCurrentEditorSet()) {
          if (auto* dungeon_editor = editor_set->GetEditorAs<DungeonEditorV2>(
                  EditorType::kDungeon)) {
            return dungeon_editor->IsWorkbenchWorkflowEnabled();
          }
        }
        return false;
      },
      [this](bool enabled) {
        if (auto* editor_set = GetCurrentEditorSet()) {
          if (auto* dungeon_editor = editor_set->GetEditorAs<DungeonEditorV2>(
                  EditorType::kDungeon)) {
            dungeon_editor->QueueWorkbenchWorkflowMode(enabled);
          }
        }
      });

  // Wire per-user sidebar prefs so right-click / drag mutate persisted state.
  activity_bar_->SetUserSettings(&user_settings_);

  // Populate the MoreActions registry with the default rail entries. External
  // callers can Register/Unregister further entries at runtime.
  auto& registry = activity_bar_->actions_registry();
  registry.Register({"command_palette",
                     "Command Palette",
                     ICON_MD_TERMINAL,
                     [this]() { window_manager_.TriggerShowCommandPalette(); },
                     {}});
  registry.Register({"keyboard_shortcuts",
                     "Keyboard Shortcuts",
                     ICON_MD_KEYBOARD,
                     [this]() { window_manager_.TriggerShowShortcuts(); },
                     {}});
  registry.Register({"open_rom_project",
                     "Open ROM / Project",
                     ICON_MD_FOLDER_OPEN,
                     [this]() { window_manager_.TriggerOpenRom(); },
                     {}});
  registry.Register({"settings",
                     "Settings",
                     ICON_MD_SETTINGS,
                     [this]() { window_manager_.TriggerShowSettings(); },
                     {}});

  // WindowHost is the declarative window registration surface used by
  // editor/runtime systems.
  window_host_ = std::make_unique<WindowHost>(&window_manager_);

  // Wire up EventBus to WorkspaceWindowManager for action event publishing
  window_manager_.SetEventBus(&event_bus_);
}

void EditorManager::RegisterEditors() {
  // Auto-register panels from ContentRegistry (Unified Panel System)
  auto registry_panels = ContentRegistry::Panels::CreateAll();
  for (auto& panel : registry_panels) {
    window_manager_.RegisterRegistryWindowContent(std::move(panel));
  }

  // STEP 4.8: Initialize DashboardPanel
  dashboard_panel_ = std::make_unique<DashboardPanel>(this);

  if (window_host_) {
    WindowDefinition dashboard_definition;
    dashboard_definition.id = "dashboard.main";
    dashboard_definition.display_name = "Dashboard";
    dashboard_definition.icon = ICON_MD_DASHBOARD;
    dashboard_definition.category = "Dashboard";
    dashboard_definition.window_title = " Dashboard";
    dashboard_definition.shortcut_hint = "F1";
    dashboard_definition.priority = 0;
    dashboard_definition.visibility_flag = dashboard_panel_->visibility_flag();
    window_host_->RegisterWindow(dashboard_definition);
  } else {
    window_manager_.RegisterWindow(
        {.card_id = "dashboard.main",
         .display_name = "Dashboard",
         .window_title = " Dashboard",
         .icon = ICON_MD_DASHBOARD,
         .category = "Dashboard",
         .shortcut_hint = "F1",
         .visibility_flag = dashboard_panel_->visibility_flag(),
         .priority = 0});
  }
}

void EditorManager::SubscribeToEvents() {
  // Subscribe to session lifecycle events via EventBus
  // (replaces SessionObserver pattern)
  event_bus_.Subscribe<SessionSwitchedEvent>(
      [this](const SessionSwitchedEvent& e) {
        HandleSessionSwitched(e.new_index, e.session, e.transient);
      });

  event_bus_.Subscribe<SessionCreatedEvent>(
      [this](const SessionCreatedEvent& e) {
        HandleSessionCreated(e.index, e.session);
      });

  event_bus_.Subscribe<SessionClosedEvent>(
      [this](const SessionClosedEvent& e) { HandleSessionClosed(e.index); });

  event_bus_.Subscribe<RomLoadedEvent>([this](const RomLoadedEvent& e) {
    HandleSessionRomLoaded(e.session_id, e.rom);
  });

  // Subscribe to FrameGuiBeginEvent for ImGui-safe deferred action processing
  // This replaces scattered manual processing calls with event-driven execution
  event_bus_.Subscribe<FrameGuiBeginEvent>([this](const FrameGuiBeginEvent&) {
    ui_sync_frame_id_.fetch_add(1, std::memory_order_relaxed);

    // Process LayoutCoordinator's deferred actions
    layout_coordinator_.ProcessDeferredActions();

    // Process EditorManager's deferred actions
    if (!deferred_actions_.empty()) {
      std::vector<std::function<void()>> actions_to_execute;
      actions_to_execute.swap(deferred_actions_);
      const int processed_count = static_cast<int>(actions_to_execute.size());
      for (auto& action : actions_to_execute) {
        action();
      }
      if (processed_count > 0) {
        const int remaining = pending_editor_deferred_actions_.fetch_sub(
                                  processed_count, std::memory_order_relaxed) -
                              processed_count;
        if (remaining < 0) {
          pending_editor_deferred_actions_.store(0, std::memory_order_relaxed);
        }
      }
    }
  });

  // Subscribe to UIActionRequestEvent for activity bar actions
  // This replaces direct callbacks from WorkspaceWindowManager
  event_bus_.Subscribe<UIActionRequestEvent>(
      [this](const UIActionRequestEvent& e) {
        HandleUIActionRequest(e.action);
      });

  event_bus_.Subscribe<PanelVisibilityChangedEvent>(
      [this](const PanelVisibilityChangedEvent& e) {
        if (e.category.empty() ||
            e.category == WorkspaceWindowManager::kDashboardCategory) {
          return;
        }
        if (IsTransientPanelVisibilityId(e.base_panel_id)) {
          return;
        }
        auto& prefs = user_settings_.prefs();
        prefs.panel_visibility_state[e.category][e.base_panel_id] = e.visible;
        settings_dirty_ = true;
        settings_dirty_timestamp_ = TimingManager::Get().GetElapsedTime();
      });
}

EditorManager::~EditorManager() {
  // ContentRegistry::Context holds non-owning pointers into our members
  // (event_bus_, editor_context_, user_settings_, layout_manager_, callbacks).
  // Clear them before our members destruct so widgets that read the singleton
  // (e.g. Canvas::set_global_scale via Context::event_bus()) don't see
  // dangling pointers in tests that instantiate multiple EditorManagers.
  // Clear() resets fallback state but intentionally keeps global_context for
  // session-close semantics, so null it explicitly here.
  ContentRegistry::Context::Clear();
  ContentRegistry::Context::SetGlobalContext(nullptr);

  // GetResourceLabels() is a process-wide singleton. RefreshResourceLabelProvider
  // hands it a pointer to current_project_.hack_manifest, which dangles after
  // we destruct. Clear before our members go out of scope.
  zelda3::GetResourceLabels().SetHackManifest(nullptr);
  zelda3::GetResourceLabels().SetProjectLabels(nullptr);

  // ThemeManager is a singleton that outlives us. Clear the theme-changed
  // callback so it stops calling back into a destroyed EditorManager via the
  // captured `this` pointer. Matters for unit tests that construct and destroy
  // multiple EditorManager instances in the same process.
  gui::ThemeManager::Get().SetOnThemeChangedCallback(nullptr);
  i18n::LanguageManager::Get().SetOnLanguageChangedCallback(nullptr);

  // EventBus subscriptions are automatically cleaned up when event_bus_ is
  // destroyed (owned by this class). No manual unsubscription needed.
}

// ============================================================================
// Session Event Handlers (EventBus subscribers)
// ============================================================================

void EditorManager::RefreshResourceLabelProvider() {
  auto& provider = zelda3::GetResourceLabels();
  static zelda3::ResourceLabelProvider::ProjectLabels empty_labels;
  static zelda3::ResourceLabelProvider::ProjectLabels merged_labels;

  auto merge_labels =
      [](zelda3::ResourceLabelProvider::ProjectLabels* dst,
         const zelda3::ResourceLabelProvider::ProjectLabels& src) {
        for (const auto& [type, labels] : src) {
          auto& out = (*dst)[type];
          for (const auto& [key, value] : labels) {
            out[key] = value;
          }
        }
      };

  merged_labels.clear();
  std::vector<std::string> source_parts;

  // 1) ROM-local labels as baseline for active session.
  if (auto* rom = GetCurrentRom();
      rom && rom->resource_label() && !rom->resource_label()->labels_.empty()) {
    merge_labels(&merged_labels, rom->resource_label()->labels_);
    source_parts.push_back("rom");
  }

  // 2) Project registry labels from the active hack manifest.
  if (current_project_.project_opened() &&
      current_project_.hack_manifest.loaded() &&
      current_project_.hack_manifest.HasProjectRegistry()) {
    merge_labels(
        &merged_labels,
        current_project_.hack_manifest.project_registry().all_resource_labels);
    source_parts.push_back("registry");
  }

  // 3) Explicit project labels override all other sources.
  if (current_project_.project_opened() &&
      !current_project_.resource_labels.empty()) {
    merge_labels(&merged_labels, current_project_.resource_labels);
    source_parts.push_back("project");
  }

  auto* label_source = merged_labels.empty() ? &empty_labels : &merged_labels;
  std::string source =
      source_parts.empty() ? "empty" : absl::StrJoin(source_parts, "+");

  provider.SetProjectLabels(label_source);
  provider.SetHackManifest(current_project_.project_opened() &&
                                   current_project_.hack_manifest.loaded()
                               ? &current_project_.hack_manifest
                               : nullptr);

  const bool prefer_hmagic =
      current_project_.project_opened()
          ? current_project_.workspace_settings.prefer_hmagic_names
          : user_settings_.prefs().prefer_hmagic_sprite_names;
  provider.SetPreferHMagicNames(prefer_hmagic);

  LOG_DEBUG("EditorManager",
            "ResourceLabelProvider refreshed (source=%s, project_open=%s)",
            source.c_str(),
            current_project_.project_opened() ? "true" : "false");
}

void EditorManager::CaptureRuntimeFeatureFlags() {
  if (!runtime_feature_flags_session_id_.has_value() || !session_coordinator_) {
    return;
  }

  const auto index =
      ResolveSessionIndexById(*runtime_feature_flags_session_id_);
  if (!index.has_value()) {
    runtime_feature_flags_session_id_.reset();
    return;
  }

  auto* session =
      static_cast<RomSession*>(session_coordinator_->GetSession(*index));
  if (!session) {
    runtime_feature_flags_session_id_.reset();
    return;
  }

  session->feature_flags = core::FeatureFlags::get();
  if (session->project_context.has_value()) {
    session->project_context->feature_flags = session->feature_flags;
  }
  if (active_project_context_session_id_ == runtime_feature_flags_session_id_) {
    current_project_.feature_flags = session->feature_flags;
  }
}

void EditorManager::CaptureActiveProjectContext() {
  if (!active_project_context_session_id_.has_value() ||
      !session_coordinator_) {
    return;
  }

  CaptureActiveProjectEditingState();
  CaptureRuntimeFeatureFlags();
  const auto index =
      ResolveSessionIndexById(*active_project_context_session_id_);
  if (!index.has_value()) {
    active_project_context_session_id_.reset();
    return;
  }

  auto* session =
      static_cast<RomSession*>(session_coordinator_->GetSession(*index));
  if (!session) {
    active_project_context_session_id_.reset();
    return;
  }

  // The singleton can temporarily belong to another session while all editor
  // windows are ticked. Only use that value when it still belongs to this
  // project owner; otherwise its session-owned copy is authoritative.
  if (runtime_feature_flags_session_id_ == active_project_context_session_id_) {
    session->feature_flags = core::FeatureFlags::get();
  }
  // These preferences are exposed through global runtime settings, but they
  // are project workspace state. Capture them before leaving the session so a
  // later SaveProject cannot copy another session's values into this project.
  current_project_.workspace_settings.font_global_scale =
      user_settings_.prefs().font_global_scale;
  current_project_.workspace_settings.autosave_enabled =
      user_settings_.prefs().autosave_enabled;
  current_project_.workspace_settings.autosave_interval_secs =
      user_settings_.prefs().autosave_interval;
  current_project_.workspace_settings.backup_on_save =
      user_settings_.prefs().backup_before_save;
  current_project_.feature_flags = session->feature_flags;
  BindProjectContextToSession(session, current_project_);
}

void EditorManager::CaptureActiveProjectEditingState() {
  if (!active_project_context_session_id_.has_value() ||
      !session_coordinator_) {
    return;
  }

  const auto index =
      ResolveSessionIndexById(*active_project_context_session_id_);
  if (!index.has_value()) {
    return;
  }
  auto* session =
      static_cast<RomSession*>(session_coordinator_->GetSession(*index));
  if (!session) {
    return;
  }

  if (project_management_panel_) {
    session->project_dirty = project_management_panel_->IsProjectDirty();
  }
  session->project_file_editor_state = project_file_editor_.CaptureState();
}

void EditorManager::DetachActiveProjectContext() {
  CaptureActiveProjectContext();
  active_project_context_session_id_.reset();
  runtime_feature_flags_session_id_.reset();
  version_manager_ = nullptr;
  if (project_management_panel_) {
    project_management_panel_->SetVersionManager(nullptr);
  }
}

void EditorManager::RestoreProjectContextAfterFailedOpen(
    std::optional<size_t> previous_session_id) {
  pending_project_open_transition_ = false;
  pending_project_open_previous_session_id_.reset();
  if (previous_session_id.has_value() && session_coordinator_) {
    const auto index = ResolveSessionIndexById(*previous_session_id);
    if (index.has_value()) {
      auto* previous_session =
          static_cast<RomSession*>(session_coordinator_->GetSession(*index));
      if (previous_session) {
        if (session_coordinator_->GetActiveSessionIndex() != *index) {
          session_coordinator_->SwitchToSession(*index);
        } else {
          RestoreProjectContextForSession(previous_session);
        }
        return;
      }
    }
  }

  current_project_ = project::YazeProject();
  active_project_context_session_id_.reset();
  runtime_feature_flags_session_id_.reset();
  version_manager_ = nullptr;
  ApplyCurrentProjectRuntimeContext();
  project_file_editor_.ResetForProject(&current_project_);
}

absl::Status EditorManager::DiscardProvisionalSessionCreatedSince(
    size_t previous_session_count) {
  if (!session_coordinator_ ||
      session_coordinator_->GetTotalSessionCount() <= previous_session_count) {
    return absl::OkStatus();
  }
  auto* provisional_session = session_coordinator_->GetActiveRomSession();
  if (!provisional_session) {
    return absl::InternalError(
        "A provisional session was created but is not active");
  }
  return session_coordinator_->DiscardProvisionalSession(
      provisional_session->session_id());
}

bool EditorManager::ProjectFileDraftTargetsCurrentProject() const {
  if (current_project_.filepath.empty() ||
      project_file_editor_.filepath().empty()) {
    return false;
  }
  return SessionCoordinator::PathsReferToSameBackingFile(
      project_file_editor_.filepath(), current_project_.filepath);
}

absl::Status EditorManager::PrepareRawProjectFileSave(
    const std::string& filepath, const std::string& contents) {
  CaptureActiveProjectEditingState();
  const bool targets_current = !current_project_.filepath.empty() &&
                               SessionCoordinator::PathsReferToSameBackingFile(
                                   filepath, current_project_.filepath);
  if (!targets_current) {
    // Save As to a separate descriptor is the deterministic escape hatch when
    // both the structured panel and raw document have edits.
    return absl::OkStatus();
  }
  if (IsCurrentProjectDirty()) {
    return absl::FailedPreconditionError(
        "Project settings also have unsaved changes; use raw Save As to "
        "preserve this draft, then save the project settings");
  }

  auto* session = session_coordinator_
                      ? session_coordinator_->GetActiveRomSession()
                      : nullptr;
  if (!session || !session->rom.is_loaded()) {
    return absl::FailedPreconditionError(
        "No active ROM session for this project file");
  }

  project::YazeProject parsed_project;
  RETURN_IF_ERROR(parsed_project.LoadFromString(contents, filepath));
  if (!parsed_project.project_opened()) {
    return absl::InvalidArgumentError(
        "Raw project file must contain a non-empty project name");
  }
  if (parsed_project.rom_filename.empty() ||
      !(SessionCoordinator::PathsReferToSameBackingFile(
            parsed_project.rom_filename, session->filepath) ||
        SessionCoordinator::PathsReferToSameBackingFile(
            parsed_project.rom_filename, session->rom.filename()))) {
    return absl::FailedPreconditionError(
        "Raw project file cannot change the ROM backing file of a loaded "
        "session; use Project Management > Swap ROM");
  }
  return session_coordinator_->CheckBackingFileAvailable(
      parsed_project.rom_filename, session->session_id());
}

absl::Status EditorManager::CommitRawProjectFileSave(
    const std::string& filepath, const std::string& contents) {
  if (current_project_.filepath.empty() ||
      !SessionCoordinator::PathsReferToSameBackingFile(
          filepath, current_project_.filepath)) {
    return absl::OkStatus();
  }

  auto* session = session_coordinator_
                      ? session_coordinator_->GetActiveRomSession()
                      : nullptr;
  if (!session) {
    return absl::FailedPreconditionError("No active ROM session");
  }

  project::YazeProject parsed_project;
  RETURN_IF_ERROR(parsed_project.LoadFromString(contents, filepath));
  current_project_ = std::move(parsed_project);
  BindProjectContextToSession(session, current_project_);
  session->project_dirty = false;
  active_project_context_session_id_ = session->session_id();
  runtime_feature_flags_session_id_ = session->session_id();
  version_manager_ = session->version_manager.get();
  ApplyCurrentProjectRuntimeContext();
  if (project_management_panel_) {
    project_management_panel_->SetProject(&current_project_, false);
  }
  return absl::OkStatus();
}

void EditorManager::RebaseCleanProjectFileDraft(const std::string& filepath) {
  if (!session_coordinator_ || filepath.empty()) {
    return;
  }
  auto* session = session_coordinator_->GetActiveRomSession();
  if (!session || !session->project_file_editor_state.initialized ||
      session->project_file_editor_state.modified) {
    return;
  }

  const bool was_active = project_file_editor_.is_active();
  auto status = project_file_editor_.LoadFile(filepath);
  if (!status.ok()) {
    LOG_WARN("EditorManager", "Failed to rebase raw project editor: %s",
             status.message());
    return;
  }
  project_file_editor_.SetProject(&current_project_);
  project_file_editor_.set_active(was_active);
  session->project_file_editor_state = project_file_editor_.CaptureState();
}

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
  if (current_project_.custom_object_files.empty()) {
    zelda3::CustomObjectManager::Get().ClearObjectFileMap();
  } else {
    zelda3::CustomObjectManager::Get().SetObjectFileMap(
        current_project_.custom_object_files);
  }
  zelda3::CustomObjectManager::Get().Initialize(
      current_project_.custom_objects_folder.empty()
          ? ""
          : current_project_.GetAbsolutePath(
                current_project_.custom_objects_folder));

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

absl::StatusOr<const project::YazeProject*>
EditorManager::PrepareActiveProjectContextForSave() {
  if (!session_coordinator_) {
    return absl::FailedPreconditionError("No session coordinator");
  }
  auto* session = session_coordinator_->GetActiveRomSession();
  if (!session || !session->rom.is_loaded()) {
    return absl::FailedPreconditionError("No active ROM session");
  }

  CaptureRuntimeFeatureFlags();
  if (!active_project_context_session_id_.has_value() ||
      *active_project_context_session_id_ != session->session_id()) {
    return absl::FailedPreconditionError(
        "Active ROM session project context is not restored");
  }
  CaptureActiveProjectContext();
  if (!session->project_context.has_value()) {
    return absl::FailedPreconditionError(
        "Active ROM session has no project/save context");
  }
  return &*session->project_context;
}

void EditorManager::HandleSessionSwitched(size_t new_index, RomSession* session,
                                          bool transient) {
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
    } else {
      runtime_feature_flags_session_id_.reset();
    }
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
  if (session != nullptr && !category.empty() &&
      category != WorkspaceWindowManager::kDashboardCategory) {
    auto it = user_settings_.prefs().panel_visibility_state.find(category);
    if (it != user_settings_.prefs().panel_visibility_state.end()) {
      const size_t session_id = session ? session->session_id() : new_index;
      window_manager_.RestoreVisibilityState(session_id, it->second);
    }
  }

#ifdef YAZE_ENABLE_TESTING
  test::TestManager::Get().SetCurrentRom(session ? &session->rom : nullptr);
#endif

  // RomLifecycleManager caches both identity hash and path; refresh them for
  // the newly active session before its next save-policy check.
  UpdateCurrentRomHash();

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
    UpdateCurrentRomHash();
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

void EditorManager::HandleUIActionRequest(UIActionRequestEvent::Action action) {
  using Action = UIActionRequestEvent::Action;
  switch (action) {
    case Action::kShowEmulator:
      if (ui_coordinator_) {
        ui_coordinator_->SetEmulatorVisible(true);
      }
      break;

    case Action::kShowSettings:
      // Toggle Settings panel in sidebar
      if (right_drawer_manager_) {
        right_drawer_manager_->ToggleDrawer(
            RightDrawerManager::DrawerType::kSettings);
      } else {
        SwitchToEditor(EditorType::kSettings);
      }
      break;

    case Action::kShowPanelBrowser:
      if (ui_coordinator_) {
        ui_coordinator_->ShowWindowBrowser();
      }
      break;

    case Action::kShowSearch:
      if (ui_coordinator_) {
        ui_coordinator_->ShowGlobalSearch();
      }
      break;

    case Action::kShowShortcuts:
      // Shortcut configuration is part of Settings
      SwitchToEditor(EditorType::kSettings);
      break;

    case Action::kShowCommandPalette:
      if (ui_coordinator_) {
        ui_coordinator_->ShowCommandPalette();
      }
      break;

    case Action::kShowHelp:
      if (right_drawer_manager_) {
        // Toggle Help panel in sidebar
        right_drawer_manager_->ToggleDrawer(
            RightDrawerManager::DrawerType::kHelp);
      } else if (popup_manager_) {
        // Fallback to "About" dialog if sidebar not available
        popup_manager_->Show(PopupID::kAbout);
      }
      break;

    case Action::kShowAgentChatSidebar:
      if (right_drawer_manager_) {
        right_drawer_manager_->OpenDrawer(
            RightDrawerManager::DrawerType::kAgentChat);
      } else {
        SwitchToEditor(EditorType::kAgent);
      }
      break;

    case Action::kShowAgentProposalsSidebar:
      if (right_drawer_manager_) {
        right_drawer_manager_->OpenDrawer(
            RightDrawerManager::DrawerType::kProposals);
      }
      break;

    case Action::kOpenRom: {
      auto status = LoadRom();
      if (!status.ok()) {
        toast_manager_.Show(
            std::string("Open failed: ") + std::string(status.message()),
            ToastType::kError);
      }
    } break;

    case Action::kSaveRom:
      if (GetCurrentRom() && GetCurrentRom()->is_loaded()) {
        auto status = SaveRom();
        if (!absl::IsCancelled(status) && !status.ok()) {
          toast_manager_.Show(
              std::string("Save failed: ") + std::string(status.message()),
              ToastType::kError);
        }
      }
      break;

    case Action::kUndo:
      if (auto* current_editor = GetCurrentEditor()) {
        auto status = current_editor->Undo();
        if (!status.ok()) {
          toast_manager_.Show(
              std::string("Undo failed: ") + std::string(status.message()),
              ToastType::kError);
        }
      }
      break;

    case Action::kRedo:
      if (auto* current_editor = GetCurrentEditor()) {
        auto status = current_editor->Redo();
        if (!status.ok()) {
          toast_manager_.Show(
              std::string("Redo failed: ") + std::string(status.message()),
              ToastType::kError);
        }
      }
      break;

    case Action::kResetLayout:
      ResetWorkspaceLayout();
      break;
  }
}

void EditorManager::InitializeTestSuites() {
  auto& test_manager = test::TestManager::Get();

#ifdef YAZE_ENABLE_TESTING
  // Register comprehensive test suites
  test_manager.RegisterTestSuite(
      std::make_unique<test::CoreSystemsTestSuite>());
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
  RegisterDefaultEditorFactories(&editor_registry_);
  SeedOracleProjectInRecents();

  // Inject the window manager into emulator and workspace_manager
  emulator_.set_window_manager(&window_manager_);
  workspace_manager_.set_window_manager(&window_manager_);

  // Point to a blank editor set when no ROM is loaded
  // current_editor_set_ = &blank_editor_set_;

  if (!filename.empty()) {
    PRINT_IF_ERROR(OpenRomOrProject(filename));
  }

  // Note: PopupManager is now initialized in constructor before
  // MenuOrchestrator This ensures all menu callbacks can safely call
  // popup_manager_.Show()

  RegisterEmulatorPanels();
  InitializeServices();
  SetupComponentCallbacks();

  // Apply sidebar state from settings AFTER registering callbacks
  // This triggers the callbacks but they should be safe now
  window_manager_.SetSidebarVisible(user_settings_.prefs().sidebar_visible,
                                    /*notify=*/false);
  window_manager_.SetSidebarExpanded(
      user_settings_.prefs().sidebar_panel_expanded,
      /*notify=*/false);
  window_manager_.SetStoredSidePanelWidth(
      user_settings_.prefs().sidebar_panel_width, /*notify=*/false);
  window_manager_.SetWindowBrowserCategoryWidth(
      user_settings_.prefs().panel_browser_category_width,
      /*notify=*/false);
  {
    const bool prefer_dashboard_only =
        dashboard_mode_override_ == StartupVisibility::kShow &&
        startup_editor_hint_.empty() && startup_panel_hints_.empty();
    if (!prefer_dashboard_only) {
      const std::string category = GetPreferredStartupCategory(
          user_settings_.prefs().sidebar_active_category, {});
      if (!category.empty()) {
        window_manager_.SetActiveCategory(category, /*notify=*/false);
        SyncEditorContextForCategory(category);
        auto it = user_settings_.prefs().panel_visibility_state.find(category);
        if (it != user_settings_.prefs().panel_visibility_state.end()) {
          window_manager_.RestoreVisibilityState(
              window_manager_.GetActiveSessionId(), it->second);
        }
      }
    }
  }

  if (pending_layout_defaults_reset_) {
    ResetWorkspaceLayout();
    pending_layout_defaults_reset_ = false;
  }

  // Initialize testing system only when tests are enabled
#ifdef YAZE_ENABLE_TESTING
  InitializeTestSuites();
#endif

  // TestManager will be updated when ROMs are loaded via SetCurrentRom calls

  InitializeShortcutSystem();
}

void EditorManager::RegisterEmulatorPanels() {
  // Register emulator panels early (emulator Initialize might not be called).
  const std::vector<WindowDefinition> panel_definitions = {
      {.id = "emulator.cpu_debugger",
       .display_name = "CPU Debugger",
       .icon = ICON_MD_BUG_REPORT,
       .category = "Emulator",
       .priority = 10},
      {.id = "emulator.ppu_viewer",
       .display_name = "PPU Viewer",
       .icon = ICON_MD_VIDEOGAME_ASSET,
       .category = "Emulator",
       .priority = 20},
      {.id = "emulator.memory_viewer",
       .display_name = "Memory Viewer",
       .icon = ICON_MD_MEMORY,
       .category = "Emulator",
       .priority = 30},
      {.id = "emulator.breakpoints",
       .display_name = "Breakpoints",
       .icon = ICON_MD_STOP,
       .category = "Emulator",
       .priority = 40},
      {.id = "emulator.performance",
       .display_name = "Performance",
       .icon = ICON_MD_SPEED,
       .category = "Emulator",
       .priority = 50},
      {.id = "emulator.ai_agent",
       .display_name = "AI Agent",
       .icon = ICON_MD_SMART_TOY,
       .category = "Emulator",
       .priority = 60},
      {.id = "emulator.save_states",
       .display_name = "Save States",
       .icon = ICON_MD_SAVE,
       .category = "Emulator",
       .priority = 70},
      {.id = "emulator.keyboard_config",
       .display_name = "Keyboard Config",
       .icon = ICON_MD_KEYBOARD,
       .category = "Emulator",
       .priority = 80},
      {.id = "emulator.virtual_controller",
       .display_name = "Virtual Controller",
       .icon = ICON_MD_SPORTS_ESPORTS,
       .category = "Emulator",
       .priority = 85},
      {.id = "emulator.apu_debugger",
       .display_name = "APU Debugger",
       .icon = ICON_MD_AUDIOTRACK,
       .category = "Emulator",
       .priority = 90},
      {.id = "emulator.audio_mixer",
       .display_name = "Audio Mixer",
       .icon = ICON_MD_AUDIO_FILE,
       .category = "Emulator",
       .priority = 100},
      {.id = "memory.hex_editor",
       .display_name = "Hex Editor",
       .icon = ICON_MD_MEMORY,
       .category = "Memory",
       .window_title = ICON_MD_MEMORY " Hex Editor",
       .priority = 10,
       .legacy_ids = {"Memory Editor"}},
  };

  if (window_host_) {
    window_host_->RegisterPanels(panel_definitions);
    return;
  }

  for (const auto& definition : panel_definitions) {
    WindowDescriptor descriptor;
    descriptor.card_id = definition.id;
    descriptor.display_name = definition.display_name;
    descriptor.window_title = definition.window_title;
    descriptor.icon = definition.icon;
    descriptor.category = definition.category;
    descriptor.shortcut_hint = definition.shortcut_hint;
    descriptor.priority = definition.priority;
    descriptor.scope = definition.scope;
    descriptor.window_lifecycle = definition.window_lifecycle;
    descriptor.context_scope = definition.context_scope;
    descriptor.visibility_flag = definition.visibility_flag;
    descriptor.on_show = definition.on_show;
    descriptor.on_hide = definition.on_hide;

    for (const auto& legacy_id : definition.legacy_ids) {
      window_manager_.RegisterPanelAlias(legacy_id, definition.id);
    }

    window_manager_.RegisterWindow(descriptor);
    if (definition.visible_by_default) {
      window_manager_.OpenWindow(definition.id);
    }
  }
}

void EditorManager::InitializeServices() {
  // Initialize project file editor
  project_file_editor_.SetToastManager(&toast_manager_);

  // Initialize agent UI (no-op when agent UI is disabled)
  agent_ui_.Initialize(&toast_manager_, &proposal_drawer_,
                       right_drawer_manager_.get(), &window_manager_,
                       &user_settings_);

  // Note: Unified gRPC Server is started from Application::Initialize()
  // after gRPC infrastructure is properly set up

  // Load critical user settings first
  status_ = user_settings_.Load();
  if (!status_.ok()) {
    LOG_WARN("EditorManager", "Failed to load user settings: %s",
             status_.ToString().c_str());
  }

  // Wire theme persistence. Any successful theme application (selector click,
  // command-palette switch, programmatic call) stamps the name into prefs and
  // rides the existing debounced-save path. Decouples ThemeManager (singleton
  // in app/gui) from UserSettings (editor-layer) — ThemeManager holds only a
  // std::function.
  gui::ThemeManager::Get().SetOnThemeChangedCallback(
      [this](const std::string& theme_name) {
        user_settings_.prefs().last_theme_name = theme_name;
        settings_dirty_ = true;
        settings_dirty_timestamp_ = TimingManager::Get().GetElapsedTime();
      });
  i18n::LanguageManager::Get().SetOnLanguageChangedCallback(
      [this](const std::string& locale) {
        user_settings_.prefs().language_locale = locale;
        settings_dirty_ = true;
        settings_dirty_timestamp_ = TimingManager::Get().GetElapsedTime();
      });

  auto& prefs = user_settings_.prefs();
  i18n::LanguageManager::Get().SetLanguage(prefs.language_locale);

  // Apply the persisted font selection (defaults to index 0 = Karla).
  // Fonts were loaded by the window backend before EditorManager init, so
  // ImGui::GetIO().Fonts->Fonts is fully populated here.
  ::yaze::SetActiveFontIndex(prefs.font_family_index);

  prefs.switch_motion_profile = std::clamp(prefs.switch_motion_profile, 0, 2);
  gui::GetAnimator().SetMotionPreferences(
      prefs.reduced_motion,
      gui::Animator::ClampMotionProfile(prefs.switch_motion_profile));

  ApplyLayoutDefaultsMigrationIfNeeded();

  if (right_drawer_manager_) {
    if (pending_layout_defaults_reset_) {
      right_drawer_manager_->ResetDrawerWidths();
      user_settings_.prefs().right_panel_widths =
          right_drawer_manager_->SerializeDrawerWidths();
    } else {
      right_drawer_manager_->RestoreDrawerWidths(
          user_settings_.prefs().right_panel_widths);
    }
    right_drawer_manager_->SetDrawerWidthChangedCallback(
        [this](RightDrawerManager::DrawerType, float) {
          if (!right_drawer_manager_) {
            return;
          }
          user_settings_.prefs().right_panel_widths =
              right_drawer_manager_->SerializeDrawerWidths();
          settings_dirty_ = true;
          settings_dirty_timestamp_ = TimingManager::Get().GetElapsedTime();
        });
  }
  agent_ui_.ApplyUserSettingsDefaults();
  // Apply sprite naming preference globally.
  yaze::zelda3::SetPreferHmagicSpriteNames(
      user_settings_.prefs().prefer_hmagic_sprite_names);

  window_manager_.RestorePinnedState(user_settings_.prefs().pinned_panels);

  // Apply font scale after loading (only if ImGui context exists)
  if (ImGui::GetCurrentContext() != nullptr) {
    ImGui::GetIO().FontGlobalScale = user_settings_.prefs().font_global_scale;
  } else {
    LOG_WARN("EditorManager",
             "ImGui context not available; skipping FontGlobalScale update");
  }

  // Restore the user's last theme. Empty (first run) or "Custom Accent"
  // (transient generated themes without a matching discovered preset) both
  // skip restoration and keep the built-in default (Classic YAZE / YAZE Tre).
  const auto& last_theme = user_settings_.prefs().last_theme_name;
  if (!last_theme.empty() && last_theme != "Custom Accent") {
    gui::ThemeManager::Get().ApplyTheme(last_theme);
  }

  // Initialize WASM control and session APIs for browser/agent integration
#ifdef __EMSCRIPTEN__
  app::platform::WasmControlApi::Initialize(this);
  app::platform::WasmSessionBridge::Initialize(this);
  LOG_INFO("EditorManager", "WASM Control and Session APIs initialized");
#endif
}

void EditorManager::SetupComponentCallbacks() {
  SetupDialogCallbacks();
  SetupWelcomeScreenCallbacks();
  SetupSidebarCallbacks();
}

void EditorManager::SetupDialogCallbacks() {
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
        if (session_coordinator_) {
          if (auto* session = session_coordinator_->GetActiveRomSession()) {
            session->feature_flags = flags;
          }
        }

        // Create project if requested
        if (options.create_project && !options.project_name.empty()) {
          auto status =
              FinalizeNewProject(options.project_name, options.project_path);
          if (!status.ok()) {
            toast_manager_.Show(absl::StrFormat("Failed to create project: %s",
                                                status.message()),
                                ToastType::kError);
          } else {
            toast_manager_.Show("Project created: " + options.project_name,
                                ToastType::kSuccess);
          }
        }

        // Close dialog and show editor selection
        show_rom_load_options_ = false;
        if (ui_coordinator_) {
          ui_coordinator_->SetEditorSelectionVisible(true);
        }

        LOG_INFO("EditorManager", "ROM load options applied: preset=%s",
                 options.selected_preset.c_str());
      });
}

void EditorManager::SetupWelcomeScreenCallbacks() {
  // Initialize welcome screen callbacks
  welcome_screen_.SetOpenRomCallback([this]() { status_ = LoadRom(); });

  welcome_screen_.SetNewProjectCallback(
      [this]() { status_ = CreateNewProject(); });

  welcome_screen_.SetNewProjectWithTemplateCallback(
      [this](const std::string& template_name) {
        status_ = CreateNewProject(template_name);
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
  });

  welcome_screen_.SetOpenPrototypeResearchCallback([this]() {
    SwitchToEditor(EditorType::kGraphics, true);
    window_manager_.OpenWindow("graphics.prototype_viewer");
    if (ui_coordinator_) {
      ui_coordinator_->SetWelcomeScreenVisible(false);
      ui_coordinator_->SetWelcomeScreenManuallyClosed(true);
    }
  });

  welcome_screen_.SetOpenAssemblyEditorNoRomCallback([this]() {
    SwitchToEditor(EditorType::kAssembly, true);
    window_manager_.OpenWindow("assembly.code_editor");
    if (ui_coordinator_) {
      ui_coordinator_->SetWelcomeScreenVisible(false);
      ui_coordinator_->SetWelcomeScreenManuallyClosed(true);
    }
  });

  welcome_screen_.SetOpenProjectDialogCallback([this]() {
    status_ = OpenProject();
    if (status_.ok() && ui_coordinator_) {
      ui_coordinator_->SetWelcomeScreenVisible(false);
      ui_coordinator_->SetWelcomeScreenManuallyClosed(true);
    } else if (!status_.ok()) {
      toast_manager_.Show(
          absl::StrFormat("Failed to open project: %s", status_.message()),
          ToastType::kError);
    }
  });

  welcome_screen_.SetOpenProjectManagementCallback(
      [this]() { ShowProjectManagement(); });

  welcome_screen_.SetOpenProjectFileEditorCallback([this]() {
    if (current_project_.filepath.empty()) {
      toast_manager_.Show("No project file to edit", ToastType::kInfo);
      return;
    }
    ShowProjectFileEditor();
  });

  // Apply welcome screen preference
  if (ui_coordinator_ && !user_settings_.prefs().show_welcome_on_startup) {
    ui_coordinator_->SetWelcomeScreenVisible(false);
    ui_coordinator_->SetWelcomeScreenManuallyClosed(true);
  }
}

void EditorManager::SetupSidebarCallbacks() {
  // Utility callbacks removed - now handled via EventBus

  window_manager_.SetSidebarStateChangedCallback(
      [this](bool visible, bool expanded) {
        user_settings_.prefs().sidebar_visible = visible;
        user_settings_.prefs().sidebar_panel_expanded = expanded;
        settings_dirty_ = true;
        settings_dirty_timestamp_ = TimingManager::Get().GetElapsedTime();
      });
  window_manager_.SetSidePanelWidthChangedCallback([this](float width) {
    user_settings_.prefs().sidebar_panel_width = width;
    settings_dirty_ = true;
    settings_dirty_timestamp_ = TimingManager::Get().GetElapsedTime();
  });
  window_manager_.SetPanelBrowserCategoryWidthChangedCallback(
      [this](float width) {
        user_settings_.prefs().panel_browser_category_width = width;
        settings_dirty_ = true;
        settings_dirty_timestamp_ = TimingManager::Get().GetElapsedTime();
      });

  window_manager_.SetCategoryChangedCallback([this](
                                                 const std::string& category) {
    if (category.empty() ||
        category == WorkspaceWindowManager::kDashboardCategory) {
      return;
    }
    SyncEditorContextForCategory(category);
    user_settings_.prefs().sidebar_active_category = category;

    const auto& prefs = user_settings_.prefs();
    auto it = prefs.panel_visibility_state.find(category);
    if (it != prefs.panel_visibility_state.end()) {
      window_manager_.RestoreVisibilityState(
          window_manager_.GetActiveSessionId(), it->second);
    } else {
      // No saved visibility state for this category yet.
      //
      // Apply LayoutPresets defaults only when this category has *no*
      // visible panels (editors may have already shown their own defaults,
      // e.g. Dungeon Workbench).
      const size_t session_id = window_manager_.GetActiveSessionId();
      bool any_visible = false;
      for (const auto& desc :
           window_manager_.GetWindowsInCategory(session_id, category)) {
        if (desc.visibility_flag && *desc.visibility_flag) {
          any_visible = true;
          break;
        }
      }

      if (!any_visible) {
        const EditorType type =
            EditorRegistry::GetEditorTypeFromCategory(category);
        for (const auto& window_id : LayoutPresets::GetDefaultWindows(type)) {
          window_manager_.OpenWindow(session_id, window_id);
        }
      }
    }

    settings_dirty_ = true;
    settings_dirty_timestamp_ = TimingManager::Get().GetElapsedTime();
  });

  window_manager_.SetEditorResolver(
      [this](const std::string& category) -> Editor* {
        Editor* editor = ResolveEditorForCategory(category);
        ContentRegistry::Context::SetEditorWindowContext(category, editor);
        return editor;
      });

  window_manager_.SetOnWindowClickedCallback(
      [this](const std::string& category) {
        EditorType type = EditorRegistry::GetEditorTypeFromCategory(category);
        if (type != EditorType::kSettings && type != EditorType::kUnknown) {
          SwitchToEditor(type, true);
        }
      });

  window_manager_.SetOnWindowCategorySelectedCallback(
      [this](const std::string& category) {
        if (ui_coordinator_) {
          ui_coordinator_->SetStartupSurface(StartupSurface::kEditor);
        }

        if (category == "Agent") {
#ifdef YAZE_BUILD_AGENT_UI
          ShowAIAgent();
#endif
          return;
        }

        EditorType type = EditorRegistry::GetEditorTypeFromCategory(category);
        if (type != EditorType::kSettings) {
          SwitchToEditor(type, true);
        }
      });

  window_manager_.EnableFileBrowser("Assembly");

  window_manager_.SetFileClickedCallback(
      [this](const std::string& category, const std::string& path) {
        if (category == "Assembly") {
          if (auto* editor_set = GetCurrentEditorSet()) {
            editor_set->ChangeActiveAssemblyFile(path);
            SwitchToEditor(EditorType::kAssembly, true);
          }
        }
      });
}

void EditorManager::InitializeShortcutSystem() {
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
  shortcut_deps.window_manager = &window_manager_;
  shortcut_deps.user_settings = &user_settings_;

  ConfigureEditorShortcuts(shortcut_deps, &shortcut_manager_);
  ConfigureMenuShortcuts(shortcut_deps, &shortcut_manager_);
  ConfigurePanelShortcuts(shortcut_deps, &shortcut_manager_);
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

  // Open windows via WorkspaceWindowManager - works for any editor type
  if (!has_panels) {
    return;
  }

  const size_t session_id = GetCurrentSessionId();
  std::string last_known_category = window_manager_.GetActiveCategory();
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
      window_manager_.SetActiveCategory(
          WorkspaceWindowManager::kDashboardCategory,
          /*notify=*/false);
      continue;
    }

    // Special case: "Room <id>" opens a dungeon room
    if (absl::StartsWith(panel_name, "Room ")) {
      if (auto* editor_set = GetCurrentEditorSet()) {
        try {
          int room_id = std::stoi(panel_name.substr(5));
          event_bus_.Publish(
              JumpToRoomRequestEvent::Create(room_id, session_id));
        } catch (const std::exception& e) {
          LOG_WARN("EditorManager", "Invalid room ID format: %s",
                   panel_name.c_str());
        }
      }
      continue;
    }

    std::optional<std::string> resolved_panel;
    if (window_manager_.GetWindowDescriptor(session_id, panel_name)) {
      resolved_panel = panel_name;
    } else {
      for (const auto& [prefixed_id, descriptor] :
           window_manager_.GetAllWindowDescriptors()) {
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
               window_manager_.GetAllWindowDescriptors().size());
      continue;
    }

    if (window_manager_.OpenWindow(session_id, *resolved_panel)) {
      const auto* descriptor =
          window_manager_.GetWindowDescriptor(session_id, *resolved_panel);
      if (descriptor != nullptr) {
        const EditorType type =
            EditorRegistry::GetEditorTypeFromCategory(descriptor->category);
        const auto ensure_status = EnsureEditorAssetsLoaded(type);
        if (!ensure_status.ok()) {
          LOG_WARN("EditorManager", "Failed to load assets for panel '%s': %s",
                   resolved_panel->c_str(), ensure_status.message().data());
        }
      }
      if (descriptor && !applied_category_from_panel &&
          descriptor->category != WorkspaceWindowManager::kDashboardCategory) {
        window_manager_.SetActiveCategory(descriptor->category);
        applied_category_from_panel = true;
      } else if (!applied_category_from_panel && descriptor &&
                 descriptor->category.empty() && !last_known_category.empty()) {
        window_manager_.SetActiveCategory(last_known_category);
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

void EditorManager::SetStartupLoadHints(const AppConfig& config) {
  startup_editor_hint_ = config.startup_editor;
  startup_panel_hints_ = config.open_panels;
  welcome_mode_override_ = config.welcome_mode;
  dashboard_mode_override_ = config.dashboard_mode;
  sidebar_mode_override_ = config.sidebar_mode;
}

void EditorManager::ApplyLayoutDefaultsMigrationIfNeeded() {
  constexpr int kTargetRevision =
      UserSettings::kLatestPanelLayoutDefaultsRevision;
  if (!user_settings_.ApplyPanelLayoutDefaultsRevision(kTargetRevision)) {
    return;
  }

  pending_layout_defaults_reset_ = true;
  settings_dirty_ = true;
  settings_dirty_timestamp_ = TimingManager::Get().GetElapsedTime();

  LOG_INFO("EditorManager",
           "Applied panel layout defaults migration revision %d",
           kTargetRevision);
}

std::string EditorManager::GetPreferredStartupCategory(
    const std::string& saved_category,
    const std::vector<std::string>& available_categories) const {
  // If saved category is valid and not Emulator, use it directly
  if (!saved_category.empty() && saved_category != "Emulator") {
    // Validate it exists in available_categories if the list is provided
    if (available_categories.empty()) {
      return saved_category;
    }
    for (const auto& cat : available_categories) {
      if (cat == saved_category)
        return saved_category;
    }
  }
  // Pick first non-Emulator category from available list
  for (const auto& cat : available_categories) {
    if (cat != "Emulator")
      return cat;
  }
  return {};
}

void EditorManager::SetAssetLoadMode(AssetLoadMode mode) {
  asset_load_mode_ = mode;
}

void EditorManager::ApplyStartupVisibilityOverrides() {
  if (ui_coordinator_) {
    ui_coordinator_->SetWelcomeScreenBehavior(welcome_mode_override_);
    ui_coordinator_->SetDashboardBehavior(dashboard_mode_override_);
  }

  if (sidebar_mode_override_ != StartupVisibility::kAuto) {
    const bool sidebar_visible =
        sidebar_mode_override_ == StartupVisibility::kShow;
    window_manager_.SetSidebarVisible(sidebar_visible, /*notify=*/false);
    if (ui_coordinator_) {
      ui_coordinator_->SetPanelSidebarVisible(sidebar_visible);
    }
  }

  // Force sidebar panel to collapse if Welcome Screen or Dashboard is explicitly shown
  // This prevents visual overlap/clutter on startup
  if (welcome_mode_override_ == StartupVisibility::kShow ||
      dashboard_mode_override_ == StartupVisibility::kShow) {
    window_manager_.SetSidebarExpanded(false, /*notify=*/false);
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
    event_bus_.Publish(JumpToRoomRequestEvent::Create(config.jump_to_room,
                                                      GetCurrentSessionId()));
  }
  if (config.jump_to_map >= 0) {
    if (config.jump_to_map < zelda3::kNumOverworldMaps) {
      event_bus_.Publish(JumpToMapRequestEvent::Create(config.jump_to_map,
                                                       GetCurrentSessionId()));
    } else {
      LOG_WARN("EditorManager",
               "Ignoring invalid startup overworld map target %d",
               config.jump_to_map);
      toast_manager_.Show("Ignored invalid startup overworld map: " +
                              std::to_string(config.jump_to_map),
                          editor::ToastType::kWarning);
    }
  }
}

absl::Status EditorManager::LoadAssetsForMode(uint64_t loading_handle) {
  switch (asset_load_mode_) {
    case AssetLoadMode::kLazy:
      return LoadAssetsLazy(loading_handle);
    case AssetLoadMode::kFull:
    case AssetLoadMode::kAuto:
    default:
      return LoadAssets(loading_handle);
  }
}

void EditorManager::ResetAssetState(RomSession* session) {
  if (!session) {
    return;
  }
  session->game_data_loaded = false;
  session->editor_initialized.fill(false);
  session->editor_assets_loaded.fill(false);
}

void EditorManager::MarkEditorInitialized(RomSession* session,
                                          EditorType type) {
  if (!session) {
    return;
  }
  const size_t index = EditorTypeIndex(type);
  if (index < session->editor_initialized.size()) {
    session->editor_initialized[index] = true;
  }
}

void EditorManager::MarkEditorLoaded(RomSession* session, EditorType type) {
  if (!session) {
    return;
  }
  const size_t index = EditorTypeIndex(type);
  if (index < session->editor_assets_loaded.size()) {
    session->editor_assets_loaded[index] = true;
  }
}

bool EditorManager::EditorRequiresGameData(EditorType type) const {
  switch (type) {
    case EditorType::kOverworld:
    case EditorType::kDungeon:
    case EditorType::kGraphics:
    case EditorType::kPalette:
    case EditorType::kScreen:
    case EditorType::kSprite:
    case EditorType::kMessage:
      return true;
    default:
      return false;
  }
}

bool EditorManager::EditorInitRequiresGameData(EditorType type) const {
  return type == EditorType::kMessage;
}

std::vector<EditorType> EditorManager::CollectEditorsToPreload(
    EditorSet* editor_set) const {
  std::unordered_set<EditorType> types;

  auto add_type = [&types](EditorType type) {
    switch (type) {
      case EditorType::kUnknown:
      case EditorType::kEmulator:
      case EditorType::kSettings:
      case EditorType::kAgent:
        return;
      default:
        types.insert(type);
        return;
    }
  };

  auto add_category = [&](const std::string& category) {
    if (category.empty() ||
        category == WorkspaceWindowManager::kDashboardCategory) {
      return;
    }
    add_type(EditorRegistry::GetEditorTypeFromCategory(category));
  };

  const size_t session_id = GetCurrentSessionId();
  bool used_startup_hints = false;

  if (!startup_editor_hint_.empty()) {
    if (auto startup_type = ParseEditorTypeFromString(startup_editor_hint_)) {
      add_type(*startup_type);
      used_startup_hints = true;
    }
  }

  for (const auto& panel_name : startup_panel_hints_) {
    if (panel_name.empty()) {
      continue;
    }

    if (const auto* descriptor =
            window_manager_.GetWindowDescriptor(session_id, panel_name)) {
      add_category(descriptor->category);
      used_startup_hints = true;
      continue;
    }

    const std::string lower_name = absl::AsciiStrToLower(panel_name);
    for (const auto& [prefixed_id, descriptor] :
         window_manager_.GetAllWindowDescriptors()) {
      const std::string base_id = StripSessionPrefix(prefixed_id);
      const std::string card_lower = absl::AsciiStrToLower(base_id);
      const std::string display_lower =
          absl::AsciiStrToLower(descriptor.display_name);
      if (card_lower == lower_name || display_lower == lower_name) {
        add_category(descriptor.category);
        used_startup_hints = true;
        break;
      }
    }
  }

  if (used_startup_hints) {
    return std::vector<EditorType>(types.begin(), types.end());
  }

  if (dashboard_mode_override_ == StartupVisibility::kShow) {
    return {};
  }

  if (editor_set) {
    for (auto* editor : editor_set->active_editors_) {
      if (editor != nullptr && *editor->active()) {
        add_type(editor->type());
      }
    }
  }

  if (current_editor_ != nullptr) {
    add_type(current_editor_->type());
  }

  add_category(window_manager_.GetActiveCategory());

  for (const auto& window_id :
       window_manager_.GetVisibleWindowIds(session_id)) {
    if (const auto* descriptor =
            window_manager_.GetWindowDescriptor(session_id, window_id)) {
      add_category(descriptor->category);
    }
  }

  return std::vector<EditorType>(types.begin(), types.end());
}

Editor* EditorManager::GetEditorByType(EditorType type,
                                       EditorSet* editor_set) const {
  return editor_set ? editor_set->GetEditor(type) : nullptr;
}

Editor* EditorManager::ResolveEditorForCategory(const std::string& category) {
  if (category.empty() ||
      category == WorkspaceWindowManager::kDashboardCategory) {
    return nullptr;
  }

  auto* editor_set = GetCurrentEditorSet();
  if (!editor_set) {
    return nullptr;
  }

  EditorType type = EditorRegistry::GetEditorTypeFromCategory(category);
  switch (type) {
    case EditorType::kAgent:
#ifdef YAZE_BUILD_AGENT_UI
      return agent_ui_.GetAgentEditor();
#else
      return nullptr;
#endif
    case EditorType::kEmulator:
    case EditorType::kSettings:
    case EditorType::kUnknown:
      return nullptr;
    default:
      return GetEditorByType(type, editor_set);
  }
}

void EditorManager::SyncEditorContextForCategory(const std::string& category) {
  if (Editor* resolved = ResolveEditorForCategory(category)) {
    SetCurrentEditor(resolved);
  } else if (!category.empty() &&
             category != WorkspaceWindowManager::kDashboardCategory) {
    LOG_DEBUG("EditorManager", "No editor context available for category '%s'",
              category.c_str());
  }
}

absl::Status EditorManager::InitializeEditorForType(EditorType type,
                                                    EditorSet* editor_set,
                                                    Rom* rom) {
  if (!editor_set) {
    return absl::FailedPreconditionError("No editor set available");
  }

  auto* editor = GetEditorByType(type, editor_set);
  if (!editor) {
    return absl::OkStatus();
  }
  editor->Initialize();
  return absl::OkStatus();
}

absl::Status EditorManager::EnsureGameDataLoaded() {
  auto* session = session_coordinator_
                      ? session_coordinator_->GetActiveRomSession()
                      : nullptr;
  if (!session) {
    return absl::FailedPreconditionError("No active session");
  }
  if (session->game_data_loaded) {
    return absl::OkStatus();
  }
  if (!session->rom.is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  RETURN_IF_ERROR(zelda3::LoadGameData(session->rom, session->game_data));
  *gfx::Arena::Get().mutable_gfx_sheets() = session->game_data.gfx_bitmaps;

  auto* game_data = &session->game_data;
  auto* editor_set = &session->editors;
  editor_set->SetGameData(game_data);
  gfx::PaletteManager::Get().Initialize(game_data);

  ContentRegistry::Context::SetGameData(game_data);
  session->game_data_loaded = true;

  return absl::OkStatus();
}

absl::Status EditorManager::EnsureEditorAssetsLoaded(EditorType type) {
  if (type == EditorType::kUnknown) {
    return absl::OkStatus();
  }

  auto* session = session_coordinator_
                      ? session_coordinator_->GetActiveRomSession()
                      : nullptr;
  if (!session) {
    return absl::OkStatus();
  }

  if (!session->rom.is_loaded()) {
    if (!EditorRegistry::UpdateAllowedWithoutLoadedRom(type)) {
      return absl::OkStatus();
    }
    const size_t index = EditorTypeIndex(type);
    if (index >= session->editor_initialized.size()) {
      return absl::InvalidArgumentError("Invalid editor type");
    }
    if (EditorInitRequiresGameData(type)) {
      return absl::OkStatus();
    }
    if (!session->editor_initialized[index]) {
      RETURN_IF_ERROR(
          InitializeEditorForType(type, &session->editors, &session->rom));
      MarkEditorInitialized(session, type);
    }
    return absl::OkStatus();
  }

  const size_t index = EditorTypeIndex(type);
  if (index >= session->editor_initialized.size()) {
    return absl::InvalidArgumentError("Invalid editor type");
  }

  if (EditorInitRequiresGameData(type)) {
    RETURN_IF_ERROR(EnsureGameDataLoaded());
  }

  if (!session->editor_initialized[index]) {
    RETURN_IF_ERROR(
        InitializeEditorForType(type, &session->editors, &session->rom));
    MarkEditorInitialized(session, type);
  }

  if (EditorRequiresGameData(type)) {
    RETURN_IF_ERROR(EnsureGameDataLoaded());
  }

  if (!session->editor_assets_loaded[index]) {
    auto* editor = GetEditorByType(type, &session->editors);
    if (editor) {
      RETURN_IF_ERROR(editor->Load());
    }
    MarkEditorLoaded(session, type);
  }

  return absl::OkStatus();
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
  if (user_settings_.prefs().autosave_enabled &&
      current_session_has_unsaved_work) {
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
      right_drawer_manager_->CloseDrawer();
    }
    return;
  }

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
    status_bar_.SetSessionInfo(GetCurrentSessionIndex(),
                               session_coordinator_->GetActiveSessionCount());
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
  }

  if (auto* current_editor_set = GetCurrentEditorSet()) {
    if (auto* dungeon_editor = current_editor_set->GetEditorAs<DungeonEditorV2>(
            EditorType::kDungeon)) {
      const int pending_rooms = dungeon_editor->PendingRoomCount();
      if (pending_rooms > 0) {
        StatusBarSegmentOptions pending_opts;
        pending_opts.tooltip = absl::StrFormat(
            "%d dungeon room%s still ha%s pending editor changes. Apply them "
            "to the ROM buffer before File > Save ROM if needed.",
            pending_rooms, pending_rooms == 1 ? "" : "s",
            pending_rooms == 1 ? "s" : "ve");
        status_bar_.SetCustomSegment(
            "Dungeon", absl::StrFormat("%d pending", pending_rooms),
            std::move(pending_opts));
      }
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

  if (ui_coordinator_ && ui_coordinator_->IsPerformanceDashboardVisible()) {
    gfx::PerformanceDashboard::Get().Render();
  }

  // Draw SessionCoordinator UI components
  if (session_coordinator_) {
    session_coordinator_->DrawSessionSwitcher();
    session_coordinator_->DrawSessionManager();
    session_coordinator_->DrawSessionRenameDialog();
  }
}

void EditorManager::DrawMainMenuBar() {
  if (ImGui::BeginMenuBar()) {
    // Consistent button styling for sidebar toggle
    {
      const bool sidebar_visible = window_manager_.IsSidebarVisible();
      gui::StyleColorGuard sidebar_btn_guard(
          {{ImGuiCol_Button, ImVec4(0, 0, 0, 0)},
           {ImGuiCol_ButtonHovered, gui::GetSurfaceContainerHighVec4()},
           {ImGuiCol_ButtonActive, gui::GetSurfaceContainerHighestVec4()},
           {ImGuiCol_Text, sidebar_visible ? gui::GetPrimaryVec4()
                                           : gui::GetTextSecondaryVec4()}});

      const char* icon = sidebar_visible ? ICON_MD_MENU_OPEN : ICON_MD_MENU;
      if (ImGui::SmallButton(icon)) {
        window_manager_.ToggleSidebarVisibility();
      }
    }

    if (ImGui::IsItemHovered()) {
      const char* tooltip = window_manager_.IsSidebarVisible()
                                ? "Hide Activity Bar (Ctrl+B)"
                                : "Show Activity Bar (Ctrl+B)";
      ImGui::SetTooltip("%s", tooltip);
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

  // Visibility gates *rendering*, not *ticking*. Run(rom) is the lazy-init +
  // render path; the SNES only starts (running_=true, snes_initialized_=true)
  // after Run(rom) fires while the emulator panel is visible. Once running,
  // switching to another editor hides the panel but must NOT freeze the game —
  // the tick-only branch below keeps audio + frame state alive.
  if (ui_coordinator_ && ui_coordinator_->IsEmulatorVisible()) {
    emulator_.Run(current_rom);
  } else if (emulator_.running() && emulator_.is_snes_initialized()) {
    if (emulator_.is_audio_focus_mode()) {
      emulator_.RunAudioFrame();
    } else {
      emulator_.RunFrameOnly();
    }
  }
}

void EditorManager::RefreshHackWorkflowBackend() {
  hack_workflow_backend_ =
      workflow::CreateHackWorkflowBackendForProject(&current_project_);
  ContentRegistry::Context::SetHackWorkflowBackend(
      hack_workflow_backend_.get());
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
 * - Asset loading: LoadAssetsForMode() (calls Initialize/Load based on mode)
 * - UI updates: UICoordinator (hides welcome, shows editor selection)
 *
 * FLOW:
 * 1. Show file dialog and get filename
 * 2. Check for duplicate sessions (prevent opening same ROM twice)
 * 3. Load ROM via RomFileManager into temp_rom
 * 4. Find empty session or create new session
 * 5. Move ROM into session and set current pointers
 * 6. Configure editor dependencies for the session
 * 7. Load editor assets (full or lazy mode)
 * 8. Update UI state and recent files
 */
absl::Status EditorManager::LoadRom() {
  if (!MaybeGuardPendingSessionAction(
          {PendingUnsavedSessionAction::Type::kOpenRomDialog,
           GetCurrentSessionId()})) {
    return absl::OkStatus();
  }

  return LoadRomInternal();
}

absl::Status EditorManager::LoadRomInternal() {
  auto load_from_path = [this](const std::string& file_name) -> absl::Status {
    if (file_name.empty()) {
      return absl::OkStatus();
    }

    // Check if this is a project file - route to project loading
    if (absl::EndsWith(file_name, ".yaze") ||
        absl::EndsWith(file_name, ".zsproj") ||
        absl::EndsWith(file_name, ".yazeproj")) {
      return OpenRomOrProjectInternal(file_name);
    }

    auto path_status =
        session_coordinator_->CheckBackingFileAvailable(file_name);
    if (!path_status.ok()) {
      toast_manager_.Show(std::string(path_status.message()),
                          editor::ToastType::kWarning);
      return path_status;
    }

    // Delegate ROM loading to RomFileManager
    Rom temp_rom;
    RETURN_IF_ERROR(rom_file_manager_.LoadRom(&temp_rom, file_name));

    const std::optional<size_t> previous_session_id =
        active_project_context_session_id_;
    const size_t previous_session_count =
        session_coordinator_->GetTotalSessionCount();
    auto session_or = session_coordinator_->CreateSessionFromRom(
        std::move(temp_rom), file_name);
    if (!session_or.ok()) {
      return session_or.status();
    }

    ConfigureEditorDependencies(GetCurrentEditorSet(), GetCurrentRom(),
                                GetCurrentSessionId());
    UpdateCurrentRomHash();

    // HandleSessionCreated binds the project context that governed this open.
    // Reapply it here rather than partially clearing global runtime state.
    ApplyCurrentProjectRuntimeContext();
    CaptureActiveProjectContext();

    // Keep ResourceLabelProvider in sync with the newly-active ROM session
    // before any editors/assets query room/sprite names.
    RefreshResourceLabelProvider();

#ifdef YAZE_ENABLE_TESTING
    test::TestManager::Get().SetCurrentRom(GetCurrentRom());
#endif

    auto& manager = project::RecentFilesManager::GetInstance();
    const auto& recent_files = manager.GetRecentFiles();
    const bool is_first_time_rom_path =
        std::find(recent_files.begin(), recent_files.end(), file_name) ==
        recent_files.end();
    auto asset_status = LoadAssetsForMode();
    if (!asset_status.ok()) {
      auto rollback_status =
          DiscardProvisionalSessionCreatedSince(previous_session_count);
      RestoreProjectContextAfterFailedOpen(previous_session_id);
      if (!rollback_status.ok()) {
        return absl::InternalError(
            absl::StrFormat("%s; session rollback failed: %s",
                            asset_status.message(), rollback_status.message()));
      }
      return asset_status;
    }

    manager.AddFile(file_name);
    manager.Save();

    if (ui_coordinator_) {
      ui_coordinator_->SetWelcomeScreenVisible(false);

      // Show ROM load options and bias new ROM paths toward project creation.
      rom_load_options_dialog_.Open(GetCurrentRom(), is_first_time_rom_path);
      show_rom_load_options_ = true;
    }

    return absl::OkStatus();
  };

#if defined(__APPLE__) && TARGET_OS_IOS == 1
  // On iOS, route through the SwiftUI overlay document picker to get proper
  // security-scoped access to iCloud Drive and Files app locations. This
  // mirrors how OpenProject() works on iOS and supports cloud ROMs.
  // The SwiftUI picker calls YazeIOSBridge.loadRomAtPath: after importing the
  // ROM to a persistent sandbox directory.
  platform::ios::PostOverlayCommand("open_rom");
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

  auto* current_session = session_coordinator_->GetActiveRomSession();
  if (!current_session) {
    return absl::FailedPreconditionError("No active ROM session");
  }
  ResetAssetState(current_session);

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

  const auto preload_editor_list = CollectEditorsToPreload(current_editor_set);
  const std::unordered_set<EditorType> preload_types(
      preload_editor_list.begin(), preload_editor_list.end());

  // Initialize only the editors needed for the current startup surface. This
  // registers their windows and sets up editor-specific resources before Load().
  struct InitStep {
    EditorType type;
    bool mark_loaded;
  };
  const InitStep init_steps[] = {
      {EditorType::kOverworld, false}, {EditorType::kMessage, false},
      {EditorType::kGraphics, false},  {EditorType::kScreen, false},
      {EditorType::kSprite, false},    {EditorType::kPalette, false},
      {EditorType::kAssembly, true},   {EditorType::kMusic, false},
      {EditorType::kDungeon, false},
  };
  for (const auto& step : init_steps) {
    if (!preload_types.contains(step.type)) {
      continue;
    }
    if (auto* editor = current_editor_set->GetEditor(step.type)) {
      editor->Initialize();
      MarkEditorInitialized(current_session, step.type);
      if (step.mark_loaded) {
        MarkEditorLoaded(current_session, step.type);
      }
    }
  }

#ifdef __EMSCRIPTEN__
  update_progress("Loading graphics sheets...");
#endif
  // Load all Zelda3-specific data (metadata, palettes, gfx groups, graphics)
  gfx::PaletteManager::Get().ReleaseSession(&current_session->game_data);
  RETURN_IF_ERROR(
      zelda3::LoadGameData(*current_rom, current_session->game_data));
  current_session->game_data_loaded = true;

  // Copy loaded graphics to Arena for global access
  *gfx::Arena::Get().mutable_gfx_sheets() =
      current_session->game_data.gfx_bitmaps;

  // Propagate GameData to editors that already exist; future editors inherit it
  // on first construction via EditorSet.
  auto* game_data = &current_session->game_data;
  current_editor_set->SetGameData(game_data);
  gfx::PaletteManager::Get().Initialize(game_data);

  struct LoadStep {
    EditorType type;
    const char* progress_message;
  };
  const LoadStep load_steps[] = {
      {EditorType::kOverworld, "Loading overworld..."},
      {EditorType::kDungeon, "Loading dungeons..."},
      {EditorType::kScreen, "Loading screen editor..."},
      {EditorType::kGraphics, "Loading graphics editor..."},
      {EditorType::kSprite, "Loading sprites..."},
      {EditorType::kMessage, "Loading messages..."},
      {EditorType::kMusic, "Loading music..."},
      {EditorType::kPalette, "Loading palettes..."},
  };
  for (const auto& step : load_steps) {
    if (!preload_types.contains(step.type)) {
      continue;
    }
#ifdef __EMSCRIPTEN__
    update_progress(step.progress_message);
#endif
    if (auto* editor = current_editor_set->GetEditor(step.type)) {
      RETURN_IF_ERROR(editor->Load());
      MarkEditorLoaded(current_session, step.type);
    }
  }

#ifdef __EMSCRIPTEN__
  update_progress("Finishing up...");
#endif

  // Set up RightDrawerManager with session's settings editor
  if (right_drawer_manager_) {
    auto* settings = current_editor_set->GetSettingsPanel();
    right_drawer_manager_->SetSettingsPanel(settings);
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

absl::Status EditorManager::LoadAssetsLazy(uint64_t passed_handle) {
  auto* current_rom = GetCurrentRom();
  auto* current_editor_set = GetCurrentEditorSet();
  if (!current_rom || !current_editor_set) {
    return absl::FailedPreconditionError("No ROM or editor set loaded");
  }

  auto* current_session = session_coordinator_->GetActiveRomSession();
  if (!current_session) {
    return absl::FailedPreconditionError("No active ROM session");
  }
  ResetAssetState(current_session);

#ifdef __EMSCRIPTEN__
  // Use passed handle if provided, otherwise create new one
  auto loading_handle =
      passed_handle != 0
          ? static_cast<app::platform::WasmLoadingManager::LoadingHandle>(
                passed_handle)
          : app::platform::WasmLoadingManager::BeginLoading(
                "Loading ROM (lazy assets)");
  auto cleanup_loading = [&]() {
    app::platform::WasmLoadingManager::EndLoading(loading_handle);
  };
  struct LoadingGuard {
    std::function<void()> cleanup;
    bool dismissed = false;
    ~LoadingGuard() {
      if (!dismissed) {
        cleanup();
      }
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

  // Wire settings panel to right panel manager for the current session.
  if (right_drawer_manager_) {
    auto* settings = current_editor_set->GetSettingsPanel();
    right_drawer_manager_->SetSettingsPanel(settings);
  }

  // Apply user preferences to status bar
  status_bar_.SetEnabled(user_settings_.prefs().show_status_bar);

#ifdef __EMSCRIPTEN__
  loading_guard.dismiss();
  app::platform::WasmLoadingManager::EndLoading(loading_handle);
#endif

  LOG_INFO("EditorManager", "Lazy asset mode: editor assets deferred");
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
absl::Status EditorManager::CheckRomWritePolicy(
    const std::optional<std::string>& target_filename) {
  return rom_lifecycle_.CheckRomWritePolicy(GetCurrentRom(), target_filename);
}

absl::Status EditorManager::CheckOracleRomSafetyPreSave(Rom* rom) {
  return rom_lifecycle_.CheckOracleRomSafetyPreSave(rom);
}

bool EditorManager::HasPendingRomSaveConfirmation() const {
  return rom_lifecycle_.IsRomWriteConfirmPending() ||
         rom_lifecycle_.HasPendingPotItemSaveConfirmation() ||
         !rom_lifecycle_.pending_write_conflicts().empty();
}

bool EditorManager::PendingRomSaveMatchesActiveSession() const {
  if (!pending_rom_save_.has_value() || !session_coordinator_) {
    return false;
  }
  return pending_rom_save_->session_index == GetCurrentSessionIndex() &&
         pending_rom_save_->rom != nullptr &&
         pending_rom_save_->rom == GetCurrentRom();
}

void EditorManager::CancelPendingRomSave(bool hide_popups) {
  pending_rom_save_.reset();
  rom_lifecycle_.ResetPendingSaveState();

  if (!hide_popups || !popup_manager_) {
    return;
  }
  for (const char* popup :
       {PopupID::kRomWriteConfirm, PopupID::kDungeonPotItemSaveConfirm,
        PopupID::kWriteConflictWarning, PopupID::kSaveAs}) {
    if (popup_manager_->IsVisible(popup)) {
      popup_manager_->Hide(popup);
    }
  }
}

absl::Status EditorManager::StartPendingRomSave(
    const std::optional<std::string>& save_as_filename) {
  auto* rom = GetCurrentRom();
  if (!session_coordinator_ || !session_coordinator_->GetActiveRomSession() ||
      !rom) {
    return absl::FailedPreconditionError("No active ROM session");
  }

  CancelPendingRomSave();
  pending_rom_save_ =
      PendingRomSave{save_as_filename, GetCurrentSessionIndex(), rom};
  return absl::OkStatus();
}

void EditorManager::FinishPendingRomSaveAttempt(const absl::Status& status) {
  // A Cancelled status is resumable only while one of this request's explicit
  // confirmation dialogs is pending. Every other outcome is terminal and must
  // consume all one-shot bypasses so a later Ctrl+S cannot inherit consent.
  if (status.ok() || !absl::IsCancelled(status) ||
      !HasPendingRomSaveConfirmation()) {
    CancelPendingRomSave();
  }
}

absl::Status EditorManager::SaveRom() {
  // Do not let an autosave or an unrelated Ctrl+S replace a Save As request
  // while one of its confirmation dialogs is still open.
  if (pending_rom_save_.has_value()) {
    const bool unresolved_confirmation =
        rom_lifecycle_.IsRomWriteConfirmPending() ||
        rom_lifecycle_.HasPendingPotItemSaveConfirmation() ||
        (!rom_lifecycle_.pending_write_conflicts().empty() &&
         !rom_lifecycle_.ShouldBypassWriteConflict());
    if (unresolved_confirmation) {
      return absl::CancelledError("Save pending confirmation");
    }
    // Preserve the established request after programmatic confirmation too.
    // Older callers use ConfirmRomWrite(); SaveRom() rather than the popup's
    // ResumePendingRomSave() helper.
    return ResumePendingRomSave();
  }
  RETURN_IF_ERROR(StartPendingRomSave(std::nullopt));
  auto status = SaveRomInternal(std::nullopt);
  FinishPendingRomSaveAttempt(status);
  return status;
}

absl::Status EditorManager::SaveRomInternal(
    const std::optional<std::string>& save_as_filename) {
  auto* current_rom = GetCurrentRom();
  auto* current_editor_set = GetCurrentEditorSet();
  if (!current_rom || !current_editor_set) {
    return absl::FailedPreconditionError("No ROM or editor set loaded");
  }
  if (save_as_filename.has_value() && save_as_filename->empty()) {
    return absl::InvalidArgumentError("No filename provided for save as");
  }
  if (save_as_filename.has_value()) {
    RETURN_IF_ERROR(session_coordinator_->CheckBackingFileAvailable(
        *save_as_filename, GetCurrentSessionId()));
  }

  ASSIGN_OR_RETURN(const project::YazeProject* save_project,
                   PrepareActiveProjectContextForSave());
  // Hash/write/safety checks must read the same immutable session snapshot as
  // the rest of this save, never a process-global project left by another ROM.
  rom_lifecycle_.SetProjectContext(save_project);
  struct LifecycleProjectContextGuard {
    RomLifecycleManager* lifecycle = nullptr;
    project::YazeProject* restore = nullptr;
    ~LifecycleProjectContextGuard() {
      if (lifecycle) {
        lifecycle->SetProjectContext(restore);
      }
    }
  } lifecycle_project_guard{&rom_lifecycle_, &current_project_};

  // --- State machine checks (delegated to RomLifecycleManager) ---
  if (rom_lifecycle_.IsRomWriteConfirmPending()) {
    return absl::CancelledError("Save pending confirmation");
  }

  RETURN_IF_ERROR(CheckRomWritePolicy(save_as_filename));

  if (rom_lifecycle_.HasPendingPotItemSaveConfirmation()) {
    return absl::CancelledError("Save pending confirmation");
  }

  const bool pot_items_enabled =
      core::FeatureFlags::get().dungeon.kSavePotItems;
  if (!rom_lifecycle_.ShouldBypassPotItemConfirm() &&
      !rom_lifecycle_.ShouldSuppressPotItemSave() && pot_items_enabled) {
    const int loaded_rooms = current_editor_set->LoadedDungeonRoomCount();
    const int total_rooms = current_editor_set->TotalDungeonRoomCount();
    if (loaded_rooms < total_rooms) {
      rom_lifecycle_.SetPotItemConfirmPending(total_rooms - loaded_rooms,
                                              total_rooms);
      if (popup_manager_) {
        popup_manager_->Show(PopupID::kDungeonPotItemSaveConfirm);
      }
      toast_manager_.Show(
          absl::StrFormat(
              "Save paused: pot items enabled with %d unloaded rooms",
              rom_lifecycle_.pending_pot_item_unloaded_rooms()),
          ToastType::kWarning);
      return absl::CancelledError("Pot item save confirmation required");
    }
  }

  const bool bypass_confirm = rom_lifecycle_.ShouldBypassPotItemConfirm();
  const bool suppress_pot_items = rom_lifecycle_.ShouldSuppressPotItemSave();
  rom_lifecycle_.ClearPotItemBypass();

  struct PotItemFlagGuard {
    bool restore = false;
    bool previous = false;
    ~PotItemFlagGuard() {
      if (restore) {
        core::FeatureFlags::get().dungeon.kSavePotItems = previous;
      }
    }
  } pot_item_guard;

  if (suppress_pot_items) {
    pot_item_guard.previous = core::FeatureFlags::get().dungeon.kSavePotItems;
    pot_item_guard.restore = true;
    core::FeatureFlags::get().dungeon.kSavePotItems = false;
  } else if (bypass_confirm) {
    // Explicitly allow pot item save once after confirmation.
  }

  // --- Backup policy setup ---
  if (save_project->project_opened()) {
    rom_lifecycle_.ApplyDefaultBackupPolicy(
        save_project->workspace_settings.backup_on_save,
        save_project->GetAbsolutePath(save_project->rom_backup_folder),
        save_project->workspace_settings.backup_retention_count,
        save_project->workspace_settings.backup_keep_daily,
        save_project->workspace_settings.backup_keep_daily_days);
  } else {
    rom_lifecycle_.ApplyDefaultBackupPolicy(
        user_settings_.prefs().backup_before_save, "", 20, true, 14);
  }

  // Reject an already-invalid Oracle layout before any editor serializer can
  // mutate the ROM buffer or clear its own dirty tracking. The post-save check
  // below remains mandatory because serializers may introduce a violation.
  RETURN_IF_ERROR(CheckOracleRomSafetyPreSave(current_rom));

  // Serializers clear their dirty flags after a successful in-memory write.
  // Capture their conservative write prediction first so conflict protection
  // still has a fail-closed fallback when the on-disk ROM cannot be read.
  const auto predicted_dungeon_write_ranges =
      current_editor_set->CollectDungeonWriteRanges();

  ScopedRomTransaction rom_transaction(*current_rom);
  ScopedEditorSaveTransactions editor_transactions;

  auto save_editor = [&editor_transactions](Editor* editor) -> absl::Status {
    if (editor == nullptr) {
      return absl::OkStatus();
    }
    RETURN_IF_ERROR(editor_transactions.Begin(editor));
    return editor->Save();
  };

  // --- Save editor-specific data ---
  RETURN_IF_ERROR(
      save_editor(current_editor_set->GetEditor(EditorType::kScreen)));
  RETURN_IF_ERROR(
      save_editor(current_editor_set->GetEditor(EditorType::kDungeon)));
  RETURN_IF_ERROR(
      save_editor(current_editor_set->GetEditor(EditorType::kOverworld)));

  if (core::FeatureFlags::get().kSaveMessages) {
    RETURN_IF_ERROR(EnsureEditorAssetsLoaded(EditorType::kMessage));
    RETURN_IF_ERROR(
        save_editor(current_editor_set->GetEditor(EditorType::kMessage)));
  }

  if (core::FeatureFlags::get().kSaveGraphicsSheet) {
    RETURN_IF_ERROR(zelda3::SaveAllGraphicsData(
        *current_rom, gfx::Arena::Get().gfx_sheets()));
  }

  // Oracle guardrails: refuse to write obviously corrupted ROM layouts.
  RETURN_IF_ERROR(CheckOracleRomSafetyPreSave(current_rom));

  // --- Write conflict check (ASM-owned address protection) ---
  if (save_project->project_opened() && save_project->hack_manifest.loaded()) {
    if (!rom_lifecycle_.ShouldBypassWriteConflict()) {
      std::vector<std::pair<uint32_t, uint32_t>> write_ranges;
      bool diff_computed = false;

      if (!current_rom->filename().empty()) {
        std::ifstream file(current_rom->filename(), std::ios::binary);
        if (file.is_open()) {
          file.seekg(0, std::ios::end);
          const std::streampos end = file.tellg();
          if (end >= 0) {
            std::vector<uint8_t> disk_data(static_cast<size_t>(end));
            file.seekg(0, std::ios::beg);
            file.read(reinterpret_cast<char*>(disk_data.data()),
                      static_cast<std::streamsize>(disk_data.size()));
            if (file) {
              diff_computed = true;
              auto diff = yaze::rom::ComputeDiffRanges(disk_data,
                                                       current_rom->vector());
              if (!diff.ranges.empty()) {
                LOG_DEBUG("EditorManager",
                          "ROM save diff: %zu bytes changed in %zu range(s)",
                          diff.total_bytes_changed, diff.ranges.size());
                write_ranges = std::move(diff.ranges);
              }
            }
          }
        }
      }

      if (write_ranges.empty() && !diff_computed) {
        write_ranges = predicted_dungeon_write_ranges;
      }

      if (!write_ranges.empty()) {
        auto conflicts =
            save_project->hack_manifest.AnalyzePcWriteRanges(write_ranges);
        if (!conflicts.empty()) {
          rom_lifecycle_.SetPendingWriteConflicts(std::move(conflicts));
          if (popup_manager_) {
            popup_manager_->Show(PopupID::kWriteConflictWarning);
          }
          toast_manager_.Show(
              absl::StrFormat(
                  "Save paused: %zu write conflict(s) with ASM hooks",
                  rom_lifecycle_.pending_write_conflicts().size()),
              ToastType::kWarning);
          return absl::CancelledError("Write conflict confirmation required");
        }
      }
    } else {
      // Bypass is single-use, set by the warning popup.
      rom_lifecycle_.ConsumeWriteConflictBypass();
    }
  }

  // Delegate the final atomic disk write to RomFileManager. Save As is part of
  // this same transaction and writes only the requested target path.
  auto save_status =
      save_as_filename.has_value()
          ? rom_file_manager_.SaveRomAs(current_rom, *save_as_filename)
          : rom_file_manager_.SaveRom(current_rom);
  if (save_status.ok()) {
    editor_transactions.Commit();
    rom_transaction.Commit();
    UpdateCurrentRomHash();
    // Write-confirm bypass is single-use. Clear it after a successful save.
    rom_lifecycle_.CancelRomWriteConfirm();

    if (save_as_filename.has_value()) {
      if (session_coordinator_) {
        if (auto* session = session_coordinator_->GetActiveRomSession()) {
          session->filepath = *save_as_filename;
        }
      }

      auto& manager = project::RecentFilesManager::GetInstance();
      manager.AddFile(*save_as_filename);
      manager.Save();
      if (popup_manager_) {
        popup_manager_->Hide(PopupID::kSaveAs);
      }
    }
  }
  return save_status;
}

absl::Status EditorManager::SaveRomAs(const std::string& filename) {
  if (filename.empty()) {
    return absl::InvalidArgumentError("No filename provided for save as");
  }
  if (pending_rom_save_.has_value() && HasPendingRomSaveConfirmation()) {
    return absl::CancelledError("Save pending confirmation");
  }
  RETURN_IF_ERROR(StartPendingRomSave(filename));
  auto status = SaveRomInternal(filename);
  FinishPendingRomSaveAttempt(status);
  return status;
}

absl::Status EditorManager::ResumePendingRomSave() {
  if (!pending_rom_save_.has_value()) {
    rom_lifecycle_.ResetPendingSaveState();
    return absl::FailedPreconditionError("No pending ROM save to resume");
  }
  if (!PendingRomSaveMatchesActiveSession()) {
    CancelPendingRomSave(/*hide_popups=*/true);
    return absl::FailedPreconditionError(
        "Pending ROM save belongs to a different session");
  }

  const auto save_as_filename = pending_rom_save_->save_as_filename;
  auto status = SaveRomInternal(save_as_filename);
  FinishPendingRomSaveAttempt(status);
  return status;
}

absl::Status EditorManager::OpenRomOrProject(const std::string& filename) {
  if (!MaybeGuardPendingSessionAction(
          {PendingUnsavedSessionAction::Type::kOpenRomOrProjectPath,
           GetCurrentSessionId(), SIZE_MAX, filename})) {
    return absl::OkStatus();
  }

  return OpenRomOrProjectInternal(filename);
}

absl::Status EditorManager::OpenRomOrProjectInternal(
    const std::string& filename) {
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

  if (absl::EndsWith(filename, ".yaze") ||
      absl::EndsWith(filename, ".zsproj") ||
      absl::EndsWith(filename, ".yazeproj")) {
    // Parse first so a failed project open cannot destroy the active session's
    // working context. Detach only when the incoming project is ready.
    project::YazeProject incoming_project;
    RETURN_IF_ERROR(incoming_project.Open(filename));
    if (!incoming_project.rom_filename.empty()) {
      RETURN_IF_ERROR(session_coordinator_->CheckBackingFileAvailable(
          incoming_project.rom_filename));
    }
    const std::optional<size_t> previous_session_id =
        active_project_context_session_id_;
    pending_project_open_transition_ = true;
    pending_project_open_previous_session_id_ = previous_session_id;
    DetachActiveProjectContext();
    current_project_ = std::move(incoming_project);
    SyncLayoutScopeFromCurrentProject();
    RefreshHackWorkflowBackend();

    // Load ROM directly from project - don't prompt user
    auto project_status = LoadProjectWithRom();
    if (!project_status.ok()) {
      RestoreProjectContextAfterFailedOpen(previous_session_id);
    }
    return project_status;
  } else {
#ifdef __EMSCRIPTEN__
    app::platform::WasmLoadingManager::UpdateProgress(loading_handle, 0.05f);
    app::platform::WasmLoadingManager::UpdateMessage(loading_handle,
                                                     "Loading ROM data...");
#endif
    Rom temp_rom;
    RETURN_IF_ERROR(session_coordinator_->CheckBackingFileAvailable(filename));
    RETURN_IF_ERROR(rom_file_manager_.LoadRom(&temp_rom, filename));
    RETURN_IF_ERROR(rom_lifecycle_.CheckRomOpenPolicy(&temp_rom));

    const std::optional<size_t> previous_session_id =
        active_project_context_session_id_;
    const size_t previous_session_count =
        session_coordinator_->GetTotalSessionCount();
    auto session_or = session_coordinator_->CreateSessionFromRom(
        std::move(temp_rom), filename);
    if (!session_or.ok()) {
      return session_or.status();
    }
    RomSession* session = *session_or;

    ConfigureEditorDependencies(GetCurrentEditorSet(), GetCurrentRom(),
                                GetCurrentSessionId());
    UpdateCurrentRomHash();

    // Apply project feature flags to both session and global singleton
    session->feature_flags = current_project_.feature_flags;
    core::FeatureFlags::get() = current_project_.feature_flags;
    CaptureActiveProjectContext();

    // Keep ResourceLabelProvider in sync with the active ROM session before
    // editors register room/sprite labels.
    RefreshResourceLabelProvider();

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
      const std::string absolute_code_folder =
          current_project_.GetAbsolutePath(current_project_.code_folder);
      // iOS: avoid blocking the main thread during project open / scene updates.
      // Large iCloud-backed projects can trigger watchdog termination if we
      // eagerly enumerate folders here.
#if !(defined(__APPLE__) && TARGET_OS_IOS == 1)
      editor_set->OpenAssemblyFolder(absolute_code_folder);
#endif
      // Also set the sidebar file browser path (refresh happens during UI draw).
      window_manager_.SetFileBrowserPath("Assembly", absolute_code_folder);
    }

#ifdef __EMSCRIPTEN__
    app::platform::WasmLoadingManager::UpdateProgress(loading_handle, 0.10f);
    app::platform::WasmLoadingManager::UpdateMessage(loading_handle,
                                                     "Initializing editors...");
    // Pass the loading handle to LoadAssets and dismiss our guard
    // LoadAssets will manage closing the indicator when done
    loading_guard.dismiss();
    auto asset_status = LoadAssetsForMode(loading_handle);
#else
    auto asset_status = LoadAssetsForMode();
#endif
    if (!asset_status.ok()) {
      auto rollback_status =
          DiscardProvisionalSessionCreatedSince(previous_session_count);
      RestoreProjectContextAfterFailedOpen(previous_session_id);
      if (!rollback_status.ok()) {
        return absl::InternalError(
            absl::StrFormat("%s; session rollback failed: %s",
                            asset_status.message(), rollback_status.message()));
      }
      return asset_status;
    }

    // Hide welcome screen and show editor selection when ROM is loaded
    ui_coordinator_->SetWelcomeScreenVisible(false);
    // dashboard_panel_->ClearRecentEditors();
    ui_coordinator_->SetEditorSelectionVisible(true);

    // Set Dashboard category to suppress panel drawing until user selects an editor
    window_manager_.SetActiveCategory(
        WorkspaceWindowManager::kDashboardCategory,
        /*notify=*/false);
  }
  return absl::OkStatus();
}

absl::Status EditorManager::CreateNewProject(const std::string& template_name) {
  if (HasAnySessionPendingUnsavedWork()) {
    return absl::FailedPreconditionError(
        "Save or discard pending session work before creating a project");
  }

  if (!ui_coordinator_) {
    return absl::FailedPreconditionError(
        "Project creation dialog is not available");
  }
  ui_coordinator_->OpenNewProjectDialog(
      template_name.empty() ? "Vanilla ROM Hack" : template_name);
  return absl::OkStatus();
}

absl::Status EditorManager::CreateNewProjectFromRom(
    const std::string& template_name, const std::string& rom_path,
    const std::string& project_name, const std::string& project_path) {
  if (rom_path.empty() || project_name.empty()) {
    return absl::InvalidArgumentError("ROM path and project name are required");
  }
  if (absl::EndsWith(rom_path, ".yaze") ||
      absl::EndsWith(rom_path, ".yazeproj") ||
      absl::EndsWith(rom_path, ".zsproj")) {
    return absl::InvalidArgumentError(
        "New projects require a ROM file, not a project descriptor");
  }
  if (HasAnySessionPendingUnsavedWork()) {
    return absl::FailedPreconditionError(
        "Save or discard pending session work before creating a project");
  }

  auto* reusable_session = session_coordinator_
                               ? session_coordinator_->GetActiveRomSession()
                               : nullptr;
  const bool reuse_active_raw_session =
      reusable_session && reusable_session->rom.is_loaded() &&
      SessionCoordinator::PathsReferToSameBackingFile(
          rom_path, reusable_session->rom.filename()) &&
      (!current_project_.project_opened() ||
       project_manager_.IsPendingRomSelection());
  const std::optional<size_t> previous_session_id =
      active_project_context_session_id_;
  const size_t previous_session_count =
      session_coordinator_ ? session_coordinator_->GetTotalSessionCount() : 0;
  RETURN_IF_ERROR(project_manager_.CreateNewProject(template_name));
  auto rom_status = project_manager_.SetProjectRom(rom_path);
  if (!rom_status.ok()) {
    project_manager_.CancelPendingProject();
    return rom_status;
  }
  auto target_status = project_manager_.ValidateProjectCreationTarget(
      project_name, project_path);
  if (!target_status.ok()) {
    project_manager_.CancelPendingProject();
    return target_status;
  }

  if (reuse_active_raw_session) {
    return FinalizeNewProject(project_name, project_path);
  }

  DetachActiveProjectContext();
  current_project_ = project_manager_.GetCurrentProject();
  SyncLayoutScopeFromCurrentProject();

  auto open_status = OpenRomOrProjectInternal(rom_path);
  if (!open_status.ok()) {
    project_manager_.CancelPendingProject();
    RestoreProjectContextAfterFailedOpen(previous_session_id);
    return open_status;
  }

  auto finalize_status = FinalizeNewProject(project_name, project_path);
  if (!finalize_status.ok()) {
    auto rollback_status =
        DiscardProvisionalSessionCreatedSince(previous_session_count);
    RestoreProjectContextAfterFailedOpen(previous_session_id);
    if (!rollback_status.ok()) {
      return absl::InternalError(absl::StrFormat(
          "%s; session rollback failed: %s", finalize_status.message(),
          rollback_status.message()));
    }
  }
  return finalize_status;
}

absl::Status EditorManager::FinalizeNewProject(
    const std::string& project_name, const std::string& project_path) {
  if (!session_coordinator_) {
    return absl::FailedPreconditionError("No session coordinator");
  }
  auto* session = session_coordinator_->GetActiveRomSession();
  if (!session || !session->rom.is_loaded()) {
    return absl::FailedPreconditionError(
        "Load a ROM before finalizing the project");
  }

  // The ROM-load-options dialog can create a project directly from an
  // already-open raw ROM. Start from a clean shell in that path rather than
  // reusing ProjectManager state left by an earlier project/import.
  const bool created_project_shell = !project_manager_.IsPendingRomSelection();
  if (created_project_shell) {
    RETURN_IF_ERROR(project_manager_.CreateNewProject());
  }

  auto project_rom_status =
      project_manager_.SetProjectRom(session->rom.filename());
  if (!project_rom_status.ok()) {
    project_manager_.CancelPendingProject();
    return project_rom_status;
  }
  auto& pending_project = project_manager_.GetCurrentProject();
  if (created_project_shell) {
    // ROM-load options apply their selected flags directly to the raw session.
    // Guided creation already configured the pending template, including when
    // it reuses an existing raw-ROM session.
    pending_project.feature_flags = session->feature_flags;
  }
  pending_project.workspace_settings.font_global_scale =
      user_settings_.prefs().font_global_scale;
  pending_project.workspace_settings.autosave_enabled =
      user_settings_.prefs().autosave_enabled;
  pending_project.workspace_settings.autosave_interval_secs =
      user_settings_.prefs().autosave_interval;
  pending_project.workspace_settings.backup_on_save =
      user_settings_.prefs().backup_before_save;

  auto finalize_status =
      project_manager_.FinalizeProjectCreation(project_name, project_path);
  if (!finalize_status.ok()) {
    project_manager_.CancelPendingProject();
    return finalize_status;
  }

  current_project_ = project_manager_.GetCurrentProject();
  BindProjectContextToSession(session, current_project_);
  session->project_dirty = false;
  active_project_context_session_id_ = session->session_id();
  runtime_feature_flags_session_id_ = session->session_id();
  RestoreProjectContextForSession(session);
  if (version_manager_) {
    (void)version_manager_->InitializeGit();
    current_project_.git_repository = session->project_context->git_repository;
    BindProjectContextToSession(session, current_project_);
  }
  RebaseCleanProjectFileDraft(current_project_.filepath);
  ApplyCurrentProjectRuntimeContext();
  CaptureActiveProjectContext();
  return absl::OkStatus();
}

absl::Status EditorManager::OpenProject() {
  if (!MaybeGuardPendingSessionAction(
          {PendingUnsavedSessionAction::Type::kOpenProjectDialog,
           GetCurrentSessionId()})) {
    return absl::OkStatus();
  }

  return OpenProjectInternal();
}

absl::Status EditorManager::OpenProjectInternal() {
  auto open_project_from_path =
      [this](const std::string& file_path) -> absl::Status {
    if (file_path.empty()) {
      return absl::OkStatus();
    }

    project::YazeProject new_project;
    RETURN_IF_ERROR(new_project.Open(file_path));

    if (!new_project.rom_filename.empty()) {
      RETURN_IF_ERROR(session_coordinator_->CheckBackingFileAvailable(
          new_project.rom_filename));
    }

    // Validate project
    auto validation_status = new_project.Validate();
    if (!validation_status.ok()) {
      toast_manager_.Show(absl::StrFormat("Project validation failed: %s",
                                          validation_status.message()),
                          editor::ToastType::kWarning, 5.0f);

      // Ask user if they want to repair
      popup_manager_->Show("Project Repair");
    }

    const std::optional<size_t> previous_session_id =
        active_project_context_session_id_;
    pending_project_open_transition_ = true;
    pending_project_open_previous_session_id_ = previous_session_id;
    DetachActiveProjectContext();
    current_project_ = std::move(new_project);
    SyncLayoutScopeFromCurrentProject();

    auto project_status = LoadProjectWithRom();
    if (!project_status.ok()) {
      RestoreProjectContextAfterFailedOpen(previous_session_id);
    }
    return project_status;
  };

#if defined(__APPLE__) && TARGET_OS_IOS == 1
  // On iOS, route project selection through the SwiftUI overlay document picker
  // so we get open-in-place + security-scoped access for iCloud Drive bundles.
  platform::ios::PostOverlayCommand("open_project");
  return absl::OkStatus();
#else
  auto file_path = util::FileDialogWrapper::ShowOpenFileDialog();
  return open_project_from_path(file_path);
#endif
}

absl::Status EditorManager::ValidateProjectRomSelection(
    const std::string& rom_path) {
  RETURN_IF_ERROR(session_coordinator_->CheckBackingFileAvailable(rom_path));

  Rom candidate_rom;
  RETURN_IF_ERROR(rom_file_manager_.LoadRom(&candidate_rom, rom_path));
  RETURN_IF_ERROR(rom_lifecycle_.CheckRomOpenPolicy(&candidate_rom));

  pending_project_rom_selection_ = PendingProjectRomSelection{
      .previous_path = current_project_.rom_filename,
      .candidate_path = rom_path,
  };
  return absl::OkStatus();
}

absl::Status EditorManager::LoadProjectWithRom() {
  auto pending_selection = std::move(pending_project_rom_selection_);
  pending_project_rom_selection_.reset();
  std::string project_rom_path = pending_selection.has_value()
                                     ? pending_selection->candidate_path
                                     : current_project_.rom_filename;
  const std::string previous_project_rom_path =
      pending_selection.has_value() ? pending_selection->previous_path
                                    : current_project_.rom_filename;
  bool persist_selected_rom = pending_selection.has_value();
  auto abandon_selected_rom = [&]() {
    if (!persist_selected_rom) {
      return;
    }
    current_project_.rom_filename = previous_project_rom_path;
    pending_project_rom_selection_.reset();
  };

  // Check if project has a ROM file specified
  if (project_rom_path.empty()) {
    // No ROM specified - prompt user to select one
    toast_manager_.Show(
        "Project has no ROM file configured. Please select a ROM.",
        editor::ToastType::kInfo);
#if defined(__APPLE__) && TARGET_OS_IOS == 1
    // Guard: if the project lives inside a .yazeproj bundle the ROM path
    // defaults to bundle/rom, which may not exist yet because iCloud hasn't
    // finished the download. Popping the file picker here would let a
    // temporary path overwrite the correct bundle path in project.yaze.
    // Show guidance and let the user reopen the project once the download
    // is complete.
    {
      auto bundle_parent =
          std::filesystem::path(current_project_.filepath).parent_path();
      if (bundle_parent.extension() == ".yazeproj") {
        toast_manager_.Show(
            "ROM is downloading from iCloud. Reopen the project in a few "
            "seconds.",
            ToastType::kInfo, 6.0f);
        return absl::UnavailableError(
            "Project ROM is not available from iCloud yet");
      }
    }
    const std::string target_project_filepath = current_project_.filepath;
    const std::optional<size_t> target_session_id =
        active_project_context_session_id_;
    const uint64_t selection_generation = ++project_rom_selection_generation_;
    util::FileDialogWrapper::ShowOpenFileDialogAsync(
        util::MakeRomFileDialogOptions(false),
        [this, target_project_filepath, target_session_id,
         selection_generation](const std::string& rom_path) {
          if (selection_generation != project_rom_selection_generation_ ||
              current_project_.filepath != target_project_filepath ||
              active_project_context_session_id_ != target_session_id) {
            toast_manager_.Show(
                "ROM selection ignored because the active project changed",
                ToastType::kWarning);
            return;
          }
          const bool restore_pending_transition =
              pending_project_open_transition_;
          const std::optional<size_t> previous_session_id =
              pending_project_open_previous_session_id_;
          auto restore_after_failure = [&]() {
            if (restore_pending_transition) {
              RestoreProjectContextAfterFailedOpen(previous_session_id);
            }
          };
          if (rom_path.empty()) {
            restore_after_failure();
            return;
          }
          auto path_status =
              session_coordinator_->CheckBackingFileAvailable(rom_path);
          if (!path_status.ok()) {
            toast_manager_.Show(std::string(path_status.message()),
                                ToastType::kError);
            restore_after_failure();
            return;
          }
          auto selection_status = ValidateProjectRomSelection(rom_path);
          if (!selection_status.ok()) {
            toast_manager_.Show(
                absl::StrFormat("Failed to update project ROM: %s",
                                selection_status.message()),
                ToastType::kError);
            restore_after_failure();
            return;
          }
          auto status = LoadProjectWithRom();
          if (!status.ok()) {
            toast_manager_.Show(
                absl::StrFormat("Failed to load project ROM: %s",
                                status.message()),
                ToastType::kError);
            restore_after_failure();
          }
        });
    return absl::OkStatus();
#else
    auto rom_path = util::FileDialogWrapper::ShowOpenFileDialog(
        util::MakeRomFileDialogOptions(false));
    if (rom_path.empty()) {
      return absl::CancelledError("Project ROM selection cancelled");
    }
    RETURN_IF_ERROR(session_coordinator_->CheckBackingFileAvailable(rom_path));
    project_rom_path = rom_path;
    persist_selected_rom = true;
#endif
  }

  auto backing_status =
      session_coordinator_->CheckBackingFileAvailable(project_rom_path);
  if (!backing_status.ok()) {
    abandon_selected_rom();
    return backing_status;
  }

  // Load ROM from project
  Rom temp_rom;
  auto load_status = rom_file_manager_.LoadRom(&temp_rom, project_rom_path);
  if (!load_status.ok()) {
    // ROM file not found or invalid - prompt user to select new ROM
    toast_manager_.Show(
        absl::StrFormat("Could not load ROM '%s': %s. Please select a new ROM.",
                        project_rom_path, load_status.message()),
        editor::ToastType::kWarning, 5.0f);
#if defined(__APPLE__) && TARGET_OS_IOS == 1
    abandon_selected_rom();
    // If the ROM is inside a .yazeproj bundle its path may not be readable
    // yet because iCloud hasn't finished downloading it.  Saving any other
    // path here would corrupt the project file with a temporary location.
    // Show guidance and bail without touching current_project_.rom_filename.
    {
      auto rom_parent = std::filesystem::path(project_rom_path).parent_path();
      if (rom_parent.extension() == ".yazeproj") {
        toast_manager_.Show(
            "ROM is still downloading from iCloud. Try reopening the project "
            "in a moment.",
            ToastType::kInfo, 6.0f);
        return absl::UnavailableError(
            "Project ROM is still downloading from iCloud");
      }
    }
    const std::string target_project_filepath = current_project_.filepath;
    const std::optional<size_t> target_session_id =
        active_project_context_session_id_;
    const uint64_t selection_generation = ++project_rom_selection_generation_;
    util::FileDialogWrapper::ShowOpenFileDialogAsync(
        util::MakeRomFileDialogOptions(false),
        [this, target_project_filepath, target_session_id,
         selection_generation](const std::string& rom_path) {
          if (selection_generation != project_rom_selection_generation_ ||
              current_project_.filepath != target_project_filepath ||
              active_project_context_session_id_ != target_session_id) {
            toast_manager_.Show(
                "ROM selection ignored because the active project changed",
                ToastType::kWarning);
            return;
          }
          const bool restore_pending_transition =
              pending_project_open_transition_;
          const std::optional<size_t> previous_session_id =
              pending_project_open_previous_session_id_;
          auto restore_after_failure = [&]() {
            if (restore_pending_transition) {
              RestoreProjectContextAfterFailedOpen(previous_session_id);
            }
          };
          if (rom_path.empty()) {
            restore_after_failure();
            return;
          }
          auto path_status =
              session_coordinator_->CheckBackingFileAvailable(rom_path);
          if (!path_status.ok()) {
            toast_manager_.Show(std::string(path_status.message()),
                                ToastType::kError);
            restore_after_failure();
            return;
          }
          auto selection_status = ValidateProjectRomSelection(rom_path);
          if (!selection_status.ok()) {
            toast_manager_.Show(
                absl::StrFormat("Failed to update project ROM: %s",
                                selection_status.message()),
                ToastType::kError);
            restore_after_failure();
            return;
          }
          auto status = LoadProjectWithRom();
          if (!status.ok()) {
            toast_manager_.Show(
                absl::StrFormat("Failed to load project ROM: %s",
                                status.message()),
                ToastType::kError);
            restore_after_failure();
          }
        });
    return absl::OkStatus();
#else
    auto rom_path = util::FileDialogWrapper::ShowOpenFileDialog(
        util::MakeRomFileDialogOptions(false));
    if (rom_path.empty()) {
      return absl::CancelledError("Project ROM selection cancelled");
    }
    RETURN_IF_ERROR(session_coordinator_->CheckBackingFileAvailable(rom_path));
    project_rom_path = rom_path;
    persist_selected_rom = true;
    RETURN_IF_ERROR(rom_file_manager_.LoadRom(&temp_rom, rom_path));
#endif
  }

  auto policy_status = rom_lifecycle_.CheckRomOpenPolicy(&temp_rom);
  if (!policy_status.ok()) {
    abandon_selected_rom();
    return policy_status;
  }

  if (persist_selected_rom) {
    current_project_.rom_filename = project_rom_path;
  }

  const size_t previous_session_count =
      session_coordinator_->GetTotalSessionCount();
  auto session_or = session_coordinator_->CreateSessionFromRom(
      std::move(temp_rom), project_rom_path);
  if (!session_or.ok()) {
    abandon_selected_rom();
    return session_or.status();
  }
  RomSession* session = *session_or;

  ConfigureEditorDependencies(GetCurrentEditorSet(), GetCurrentRom(),
                              GetCurrentSessionId());
  UpdateCurrentRomHash();

  // Auto-enable custom object rendering when a project defines custom object
  // data but the stale feature flag is off.
  if (ProjectUsesCustomObjects(current_project_) &&
      !current_project_.feature_flags.kEnableCustomObjects) {
    current_project_.feature_flags.kEnableCustomObjects = true;
    LOG_WARN("EditorManager",
             "Project has custom object data but 'enable_custom_objects' was "
             "disabled. Enabling at runtime.");
    toast_manager_.Show("Custom object rendering auto-enabled for this project",
                        ToastType::kInfo);
  }
  std::string legacy_mapping_warning;
  if (SeedLegacyTrackObjectMapping(&current_project_,
                                   &legacy_mapping_warning)) {
    LOG_WARN("EditorManager", "%s", legacy_mapping_warning.c_str());
    toast_manager_.Show(
        "Seeded default custom object mapping for object 0x31 (save project "
        "to persist)",
        ToastType::kWarning);
  }

  // Apply project feature flags to both session and global singleton.
  session->feature_flags = current_project_.feature_flags;
  core::FeatureFlags::get() = current_project_.feature_flags;
  zelda3::DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
#if !defined(NDEBUG)
  LOG_INFO(
      "EditorManager",
      "Feature flags applied: kEnableCustomObjects=%s, "
      "custom_objects_folder='%s', custom_object_files=%zu entries",
      current_project_.feature_flags.kEnableCustomObjects ? "true" : "false",
      current_project_.custom_objects_folder.c_str(),
      current_project_.custom_object_files.size());
#endif

  BindProjectContextToSession(session, current_project_);
  RestoreProjectContextForSession(session);
  if (version_manager_) {
    // Preserve the existing best-effort Git initialization behavior, now
    // against the stable session-owned project object.
    (void)version_manager_->InitializeGit();
    current_project_.git_repository = session->project_context->git_repository;
  }

  if (auto* rom = GetCurrentRom(); rom && rom->is_loaded()) {
    if (IsRomHashMismatch()) {
      toast_manager_.Show(
          "Project ROM hash mismatch detected. Check ROM Identity settings.",
          ToastType::kWarning);
    }
    auto warnings = ValidateRomAddressOverrides(
        current_project_.rom_address_overrides, *rom);
    if (!warnings.empty()) {
      for (const auto& warning : warnings) {
        LOG_WARN("EditorManager", "%s", warning.c_str());
      }
      toast_manager_.Show(absl::StrFormat("ROM override warnings: %d (see log)",
                                          warnings.size()),
                          ToastType::kWarning);
    }
  }

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
    const std::string absolute_code_folder =
        current_project_.GetAbsolutePath(current_project_.code_folder);
    // iOS: avoid blocking the main thread during project open / scene updates.
#if !(defined(__APPLE__) && TARGET_OS_IOS == 1)
    editor_set->OpenAssemblyFolder(absolute_code_folder);
#endif
    // Also set the sidebar file browser path (refresh happens during UI draw).
    window_manager_.SetFileBrowserPath("Assembly", absolute_code_folder);
  }

  // Initialize labels before loading editor assets so room lists / command
  // palette entries resolve project registry labels on first render.
  RefreshResourceLabelProvider();

  auto asset_status = LoadAssetsForMode();
  if (!asset_status.ok()) {
    abandon_selected_rom();
    auto rollback_status =
        DiscardProvisionalSessionCreatedSince(previous_session_count);
    if (!rollback_status.ok()) {
      return absl::InternalError(
          absl::StrFormat("%s; session rollback failed: %s",
                          asset_status.message(), rollback_status.message()));
    }
    return asset_status;
  }

  if (persist_selected_rom) {
    auto save_status = current_project_.Save();
    if (!save_status.ok()) {
      abandon_selected_rom();
      auto rollback_status =
          DiscardProvisionalSessionCreatedSince(previous_session_count);
      if (!rollback_status.ok()) {
        return absl::InternalError(
            absl::StrFormat("%s; session rollback failed: %s",
                            save_status.message(), rollback_status.message()));
      }
      return save_status;
    }
    pending_project_rom_selection_.reset();
  }

  // Hide welcome screen and show editor selection when project ROM is loaded
  if (ui_coordinator_) {
    ui_coordinator_->SetWelcomeScreenVisible(false);
    ui_coordinator_->SetEditorSelectionVisible(true);
  }

  // Set Dashboard category to suppress panel drawing until user selects an editor
  window_manager_.SetActiveCategory(WorkspaceWindowManager::kDashboardCategory,
                                    /*notify=*/false);

  // Apply workspace settings
  user_settings_.prefs().font_global_scale =
      current_project_.workspace_settings.font_global_scale;
  user_settings_.prefs().autosave_enabled =
      current_project_.workspace_settings.autosave_enabled;
  user_settings_.prefs().autosave_interval =
      current_project_.workspace_settings.autosave_interval_secs;
  user_settings_.prefs().backup_before_save =
      current_project_.workspace_settings.backup_on_save;
  ImGui::GetIO().FontGlobalScale = user_settings_.prefs().font_global_scale;

  CaptureActiveProjectContext();
  RefreshHackWorkflowBackend();

  status_bar_.ClearProjectWorkflowStatus();
  ContentRegistry::Context::SetBuildWorkflowStatus(ProjectWorkflowStatus{});
  ContentRegistry::Context::SetRunWorkflowStatus(ProjectWorkflowStatus{});
  ContentRegistry::Context::ClearWorkflowHistory();
  if (project_management_panel_) {
    project_management_panel_->SetBuildStatus(ProjectWorkflowStatus{});
    project_management_panel_->SetRunStatus(ProjectWorkflowStatus{});
    project_management_panel_->SetBuildLogOutput("");
  }
  ContentRegistry::Context::SetBuildWorkflowLog("");
  active_project_build_.reset();
  active_project_build_reported_ = false;

  // Publish project context for hack workflow panels and other consumers
  ContentRegistry::Context::SetCurrentProject(&current_project_);

  // Add to recent files
  auto& manager = project::RecentFilesManager::GetInstance();
  manager.AddFile(current_project_.filepath);
  manager.Save();

  // Update project management panel with loaded project
  if (project_management_panel_) {
    project_management_panel_->SetProject(&current_project_);
    project_management_panel_->SetVersionManager(version_manager_);
    project_management_panel_->SetRom(GetCurrentRom());
  }

  toast_manager_.Show(absl::StrFormat("Project '%s' loaded successfully",
                                      current_project_.GetDisplayName()),
                      editor::ToastType::kSuccess);

  pending_project_open_transition_ = false;
  pending_project_open_previous_session_id_.reset();
  return absl::OkStatus();
}

absl::Status EditorManager::ReplaceActiveSessionRom(
    Rom&& rom, const std::string& filepath) {
  if (!session_coordinator_) {
    return absl::FailedPreconditionError("No session coordinator");
  }
  auto* session = session_coordinator_->GetActiveRomSession();
  if (!session) {
    return absl::FailedPreconditionError("No active ROM session");
  }

  Rom previous_rom = session->rom;
  const std::string previous_filepath = session->filepath;
  gfx::PaletteManager::Get().ReleaseSession(&session->game_data);
  session->rom = std::move(rom);
  session->filepath = filepath;
  session->game_data.Clear();
  session->game_data.set_rom(&session->rom);
  session->editors.SetGameData(&session->game_data);
  gfx::PaletteManager::Get().ActivateSession(&session->game_data);
  ResetAssetState(session);
  ConfigureSession(session);
  HandleSessionRomLoaded(GetCurrentSessionIndex(), &session->rom);
  UpdateCurrentRomHash();
  auto load_status = LoadAssetsForMode();
  if (!load_status.ok()) {
    // Loading editors is fallible. Restore the prior ROM and rebuild its
    // assets so a failed reload never leaves a half-initialized live session.
    gfx::PaletteManager::Get().ReleaseSession(&session->game_data);
    session->rom = std::move(previous_rom);
    session->filepath = previous_filepath;
    session->game_data.Clear();
    session->game_data.set_rom(&session->rom);
    session->editors.SetGameData(&session->game_data);
    gfx::PaletteManager::Get().ActivateSession(&session->game_data);
    ResetAssetState(session);
    ConfigureSession(session);
    HandleSessionRomLoaded(GetCurrentSessionIndex(), &session->rom);
    UpdateCurrentRomHash();
    auto rollback_status = LoadAssetsForMode();
    ApplyCurrentProjectRuntimeContext();
    if (!rollback_status.ok()) {
      return absl::InternalError(absl::StrFormat(
          "ROM reload failed (%s) and restoring prior assets failed (%s)",
          load_status.message(), rollback_status.message()));
    }
    return load_status;
  }
  ApplyCurrentProjectRuntimeContext();
  return absl::OkStatus();
}

absl::Status EditorManager::SwapProjectRom(const std::string& rom_path) {
  if (rom_path.empty()) {
    return absl::InvalidArgumentError("ROM path cannot be empty");
  }
  if (!session_coordinator_) {
    return absl::FailedPreconditionError("No session coordinator");
  }
  auto* session = session_coordinator_->GetActiveRomSession();
  if (!session || !session->rom.is_loaded() ||
      !current_project_.project_opened()) {
    return absl::FailedPreconditionError("No active project ROM session");
  }
  if (SessionHasPendingRomWork(GetCurrentSessionIndex())) {
    return absl::FailedPreconditionError(
        "Save or discard ROM edits before swapping the project ROM");
  }
  CaptureActiveProjectEditingState();
  if (session->project_file_editor_state.initialized &&
      session->project_file_editor_state.modified &&
      ProjectFileDraftTargetsCurrentProject()) {
    return absl::FailedPreconditionError(
        "Preserve the raw project-file draft before swapping ROMs");
  }

  RETURN_IF_ERROR(session_coordinator_->CheckBackingFileAvailable(
      rom_path, session->session_id()));
  Rom replacement_rom;
  RETURN_IF_ERROR(rom_file_manager_.LoadRom(&replacement_rom, rom_path));
  RETURN_IF_ERROR(rom_lifecycle_.CheckRomOpenPolicy(&replacement_rom));

  const std::string old_rom_path = current_project_.rom_filename;
  current_project_.rom_filename = rom_path;
  auto save_status = current_project_.Save();
  if (!save_status.ok()) {
    current_project_.rom_filename = old_rom_path;
    return save_status;
  }

  BindProjectContextToSession(session, current_project_);
  auto replace_status =
      ReplaceActiveSessionRom(std::move(replacement_rom), rom_path);
  if (!replace_status.ok()) {
    current_project_.rom_filename = old_rom_path;
    auto rollback_status = current_project_.Save();
    BindProjectContextToSession(session, current_project_);
    ApplyCurrentProjectRuntimeContext();
    if (!rollback_status.ok()) {
      return absl::InternalError(absl::StrFormat(
          "ROM swap failed (%s) and restoring the project descriptor failed "
          "(%s)",
          replace_status.message(), rollback_status.message()));
    }
    return replace_status;
  }
  session->project_dirty = false;
  if (project_management_panel_) {
    project_management_panel_->SetProjectDirty(false);
  }
  RebaseCleanProjectFileDraft(current_project_.filepath);
  CaptureActiveProjectContext();
  return absl::OkStatus();
}

absl::Status EditorManager::ReloadProjectRom() {
  if (!session_coordinator_) {
    return absl::FailedPreconditionError("No session coordinator");
  }
  auto* session = session_coordinator_->GetActiveRomSession();
  if (!session || !session->rom.is_loaded() ||
      !current_project_.project_opened() ||
      current_project_.rom_filename.empty()) {
    return absl::FailedPreconditionError("No active project ROM to reload");
  }
  if (SessionHasPendingRomWork(GetCurrentSessionIndex())) {
    return absl::FailedPreconditionError(
        "Save or discard ROM edits before reloading from disk");
  }

  RETURN_IF_ERROR(session_coordinator_->CheckBackingFileAvailable(
      current_project_.rom_filename, session->session_id()));
  Rom replacement_rom;
  RETURN_IF_ERROR(rom_file_manager_.LoadRom(&replacement_rom,
                                            current_project_.rom_filename));
  RETURN_IF_ERROR(rom_lifecycle_.CheckRomOpenPolicy(&replacement_rom));
  return ReplaceActiveSessionRom(std::move(replacement_rom),
                                 current_project_.rom_filename);
}

absl::Status EditorManager::SaveProject() {
  CaptureActiveProjectEditingState();
  if (session_coordinator_) {
    if (auto* session = session_coordinator_->GetActiveRomSession();
        session && session->project_file_editor_state.initialized &&
        session->project_file_editor_state.modified &&
        ProjectFileDraftTargetsCurrentProject()) {
      return absl::FailedPreconditionError(
          "Raw project file also has unsaved changes; use raw Save As to "
          "preserve that draft before saving project settings");
    }
  }
  CaptureActiveProjectContext();
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
    current_project_.workspace_settings.backup_on_save =
        user_settings_.prefs().backup_before_save;

    // Save recent files
    auto& manager = project::RecentFilesManager::GetInstance();
    current_project_.workspace_settings.recent_files.clear();
    for (const auto& file : manager.GetRecentFiles()) {
      current_project_.workspace_settings.recent_files.push_back(file);
    }
  }

  auto status = current_project_.Save();
  if (status.ok()) {
    if (project_management_panel_) {
      project_management_panel_->SetProjectDirty(false);
    }
    if (session_coordinator_) {
      if (auto* session = session_coordinator_->GetActiveRomSession()) {
        session->project_dirty = false;
      }
    }
    RebaseCleanProjectFileDraft(current_project_.filepath);
  }
  CaptureActiveProjectContext();
  return status;
}

void EditorManager::MarkCurrentProjectDirty() {
  if (project_management_panel_) {
    project_management_panel_->SetProjectDirty(true);
  }
  if (session_coordinator_) {
    if (auto* session = session_coordinator_->GetActiveRomSession()) {
      session->project_dirty = true;
    }
  }
}

bool EditorManager::IsCurrentProjectDirty() const {
  if (!session_coordinator_) {
    return project_management_panel_ &&
           project_management_panel_->IsProjectDirty();
  }
  const auto* session = session_coordinator_->GetActiveRomSession();
  return session != nullptr && session->project_dirty;
}

absl::Status EditorManager::SaveActiveProjectEditingWork() {
  if (!session_coordinator_) {
    return absl::FailedPreconditionError("No session coordinator");
  }
  auto* session = session_coordinator_->GetActiveRomSession();
  if (!session) {
    return absl::FailedPreconditionError("No active ROM session");
  }

  CaptureActiveProjectEditingState();
  const bool project_dirty = session->project_dirty;
  const bool project_file_dirty =
      session->project_file_editor_state.initialized &&
      session->project_file_editor_state.modified;

  if (project_dirty && project_file_dirty &&
      ProjectFileDraftTargetsCurrentProject()) {
    return absl::FailedPreconditionError(
        "Project settings and raw project file both have unsaved changes; "
        "use raw Save As to preserve the draft, then save project settings");
  }

  if (project_dirty) {
    if (!current_project_.project_opened() ||
        current_project_.filepath.empty()) {
      return absl::FailedPreconditionError(
          "Project settings have no project file to save");
    }
    RETURN_IF_ERROR(SaveProject());
  }

  if (project_file_dirty) {
    if (project_file_editor_.filepath().empty()) {
      return absl::FailedPreconditionError(
          "Project file draft has no destination; use Save As first");
    }
    RETURN_IF_ERROR(project_file_editor_.SaveFile());
    session->project_file_editor_state = project_file_editor_.CaptureState();
  }

  return absl::OkStatus();
}

absl::Status EditorManager::AutosaveActiveSession() {
  if (!session_coordinator_) {
    return absl::FailedPreconditionError("No session coordinator");
  }
  const size_t session_index = GetCurrentSessionIndex();
  RETURN_IF_ERROR(SaveActiveProjectEditingWork());
  if (SessionHasPendingRomWork(session_index)) {
    RETURN_IF_ERROR(SaveRom());
  }
  if (SessionHasPendingUnsavedWork(session_index)) {
    return absl::FailedPreconditionError(
        "Autosave left unsaved project or ROM work");
  }
  return absl::OkStatus();
}

void EditorManager::ResolvePotItemSaveConfirmation(
    PotItemSaveDecision decision) {
  // Map EditorManager enum to RomLifecycleManager enum
  auto lifecycle_decision =
      static_cast<RomLifecycleManager::PotItemSaveDecision>(
          static_cast<int>(decision));
  rom_lifecycle_.ResolvePotItemSaveConfirmation(lifecycle_decision);

  if (decision == PotItemSaveDecision::kCancel) {
    CancelPendingRomSave();
    toast_manager_.Show("Save cancelled", ToastType::kInfo);
    return;
  }

  auto status = ResumePendingRomSave();
  if (!absl::IsCancelled(status) && !status.ok()) {
    toast_manager_.Show(
        absl::StrFormat("Failed to save ROM: %s", status.message()),
        ToastType::kError);
  }
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

  return SaveProjectAs(file_path);
}

absl::Status EditorManager::SaveProjectAs(const std::string& filepath) {
  if (filepath.empty()) {
    return absl::InvalidArgumentError("Project file path cannot be empty");
  }
  CaptureActiveProjectEditingState();
  CaptureActiveProjectContext();
  std::string file_path = filepath;

  // Ensure a project extension.
  if (!(absl::EndsWith(file_path, ".yaze") ||
        absl::EndsWith(file_path, ".yazeproj"))) {
    file_path += ".yaze";
  }

  if (session_coordinator_) {
    if (auto* session = session_coordinator_->GetActiveRomSession();
        session && session->project_file_editor_state.initialized &&
        session->project_file_editor_state.modified &&
        SessionCoordinator::PathsReferToSameBackingFile(
            session->project_file_editor_state.filepath, file_path)) {
      return absl::FailedPreconditionError(
          "Raw project draft targets the selected file; preserve it with "
          "raw Save As before overwriting that destination");
    }
  }

  // Update project filepath and save
  std::string old_filepath = current_project_.filepath;
  current_project_.filepath = file_path;

  auto save_status = current_project_.Save();
  if (save_status.ok()) {
    if (project_management_panel_) {
      project_management_panel_->SetProjectDirty(false);
    }
    if (session_coordinator_) {
      if (auto* session = session_coordinator_->GetActiveRomSession()) {
        session->project_dirty = false;
      }
    }
    SyncLayoutScopeFromCurrentProject();
    RebaseCleanProjectFileDraft(file_path);

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

  CaptureActiveProjectContext();

  return save_status;
}

absl::StatusOr<std::string> EditorManager::ResolveProjectBuildCommand() const {
  if (!current_project_.project_opened()) {
    return absl::FailedPreconditionError("No project open");
  }

  std::string command = current_project_.build_script;
  if (command.empty() && current_project_.hack_manifest.loaded()) {
    command = current_project_.hack_manifest.build_pipeline().build_script;
  }
  if (command.empty()) {
    return absl::NotFoundError("Project does not define a build command");
  }
  return command;
}

absl::StatusOr<std::string> EditorManager::ResolveProjectRunTarget() const {
  if (!current_project_.project_opened()) {
    return absl::FailedPreconditionError("No project open");
  }

  std::string target;
  if (current_project_.hack_manifest.loaded()) {
    target = current_project_.hack_manifest.build_pipeline().patched_rom;
  }
  if (target.empty()) {
    target = current_project_.build_target;
  }
  if (target.empty()) {
    return absl::NotFoundError("Project does not define a run target ROM");
  }
  return current_project_.GetAbsolutePath(target);
}

ProjectWorkflowStatus EditorManager::MakeBuildStatus(
    const std::string& summary, const std::string& detail,
    ProjectWorkflowState state, const std::string& output_tail,
    bool can_cancel) const {
  return {.visible = true,
          .can_cancel = can_cancel,
          .label = "Build",
          .summary = summary,
          .detail = detail,
          .output_tail = output_tail,
          .state = state};
}

ProjectWorkflowStatus EditorManager::MakeRunStatus(
    const std::string& summary, const std::string& detail,
    ProjectWorkflowState state) const {
  return {.visible = true,
          .label = "Run",
          .summary = summary,
          .detail = detail,
          .state = state};
}

absl::StatusOr<std::string> EditorManager::RunProjectBuildCommand() {
  auto command_or = ResolveProjectBuildCommand();
  if (!command_or.ok()) {
    return command_or.status();
  }

  const std::string command = *command_or;
  const std::filesystem::path project_root =
      std::filesystem::path(current_project_.filepath).parent_path();
  BackgroundCommandTask task;
  auto start_status = task.Start(command, project_root.string());
  if (!start_status.ok()) {
    return start_status;
  }
  auto wait_status = task.Wait();
  const auto snapshot = task.GetSnapshot();
  if (!wait_status.ok()) {
    return wait_status;
  }
  const std::string summary = LastNonEmptyLine(snapshot.output);
  return summary.empty() ? command : summary;
}

absl::Status EditorManager::BuildCurrentProject() {
  const std::string running_detail =
      "Running the configured project build command";
  UpdateBuildWorkflowStatus(
      &status_bar_, project_management_panel_.get(),
      MakeBuildStatus("Build running", running_detail,
                      ProjectWorkflowState::kRunning, "", false));

  auto result_or = RunProjectBuildCommand();
  if (!result_or.ok()) {
    UpdateBuildWorkflowStatus(
        &status_bar_, project_management_panel_.get(),
        MakeBuildStatus("Build failed",
                        std::string(result_or.status().message()),
                        ProjectWorkflowState::kFailure));
    toast_manager_.Show(
        absl::StrFormat("Build unavailable: %s", result_or.status().message()),
        ToastType::kWarning);
    return result_or.status();
  }

  const std::string summary = *result_or;
  UpdateBuildWorkflowStatus(&status_bar_, project_management_panel_.get(),
                            MakeBuildStatus("Build succeeded", summary,
                                            ProjectWorkflowState::kSuccess));
  toast_manager_.Show(
      summary.empty() ? "Project build completed"
                      : absl::StrFormat("Project build completed: %s", summary),
      ToastType::kSuccess);
  LOG_INFO("EditorManager", "Project build completed: %s", summary.c_str());
  return absl::OkStatus();
}

void EditorManager::QueueBuildCurrentProject() {
  if (active_project_build_) {
    const auto snapshot = active_project_build_->GetSnapshot();
    if (snapshot.running) {
      toast_manager_.Show("A project build is already running",
                          ToastType::kInfo);
      return;
    }
  }

  auto command_or = ResolveProjectBuildCommand();
  if (!command_or.ok()) {
    UpdateBuildWorkflowStatus(
        &status_bar_, project_management_panel_.get(),
        MakeBuildStatus("Build unavailable",
                        std::string(command_or.status().message()),
                        ProjectWorkflowState::kFailure));
    toast_manager_.Show(
        absl::StrFormat("Build unavailable: %s", command_or.status().message()),
        ToastType::kWarning);
    return;
  }

  const std::filesystem::path project_root =
      std::filesystem::path(current_project_.filepath).parent_path();
  active_project_build_ = std::make_unique<BackgroundCommandTask>();
  auto start_status =
      active_project_build_->Start(*command_or, project_root.string());
  if (!start_status.ok()) {
    UpdateBuildWorkflowStatus(
        &status_bar_, project_management_panel_.get(),
        MakeBuildStatus("Build unavailable",
                        std::string(start_status.message()),
                        ProjectWorkflowState::kFailure));
    toast_manager_.Show(
        absl::StrFormat("Build unavailable: %s", start_status.message()),
        ToastType::kWarning);
    active_project_build_.reset();
    return;
  }

  active_project_build_reported_ = false;
  UpdateBuildWorkflowStatus(
      &status_bar_, project_management_panel_.get(),
      MakeBuildStatus("Build running",
                      "Running the configured project build command",
                      ProjectWorkflowState::kRunning, "", true));
  if (project_management_panel_) {
    project_management_panel_->SetBuildLogOutput("");
  }
  ContentRegistry::Context::SetBuildWorkflowLog("");
}

void EditorManager::CancelQueuedProjectBuild() {
  if (!active_project_build_) {
    return;
  }

  const auto snapshot = active_project_build_->GetSnapshot();
  if (!snapshot.running) {
    return;
  }

  active_project_build_->Cancel();
  UpdateBuildWorkflowStatus(
      &status_bar_, project_management_panel_.get(),
      MakeBuildStatus("Cancelling build", "Stopping the active project build",
                      ProjectWorkflowState::kRunning, snapshot.output_tail,
                      false));
}

absl::Status EditorManager::RunCurrentProject() {
  UpdateRunWorkflowStatus(
      &status_bar_, project_management_panel_.get(),
      MakeRunStatus("Run queued",
                    "Preparing the project output for emulator reload",
                    ProjectWorkflowState::kRunning));

  auto run_target_or = ResolveProjectRunTarget();
  if (!run_target_or.ok()) {
    UpdateRunWorkflowStatus(
        &status_bar_, project_management_panel_.get(),
        MakeRunStatus("Run unavailable",
                      std::string(run_target_or.status().message()),
                      ProjectWorkflowState::kFailure));
    toast_manager_.Show(absl::StrFormat("Run unavailable: %s",
                                        run_target_or.status().message()),
                        ToastType::kWarning);
    return run_target_or.status();
  }
  const std::string run_target = *run_target_or;
  if (!std::filesystem::exists(run_target)) {
    UpdateRunWorkflowStatus(&status_bar_, project_management_panel_.get(),
                            MakeRunStatus("Run target missing", run_target,
                                          ProjectWorkflowState::kFailure));
    toast_manager_.Show("Run target ROM not found. Build the project first.",
                        ToastType::kWarning);
    return absl::NotFoundError("Run target ROM not found");
  }

#ifdef YAZE_WITH_GRPC
  if (auto* emulator_backend = Application::Instance().GetEmulatorBackend()) {
    auto load_status = emulator_backend->LoadRom(run_target);
    if (load_status.ok()) {
      AppendWorkflowHistoryEntry(
          "Run",
          MakeRunStatus("Reloaded in backend", run_target,
                        ProjectWorkflowState::kSuccess),
          "");
      UpdateRunWorkflowStatus(&status_bar_, project_management_panel_.get(),
                              MakeRunStatus("Reloaded in backend", run_target,
                                            ProjectWorkflowState::kSuccess));
      if (ui_coordinator_) {
        ui_coordinator_->SetEmulatorVisible(true);
      }
      toast_manager_.Show(
          absl::StrFormat("Reloaded project output in emulator backend: %s",
                          run_target),
          ToastType::kInfo);
      return absl::OkStatus();
    }
  }
#endif

  Rom temp_rom;
  auto load_rom_status = rom_file_manager_.LoadRom(&temp_rom, run_target);
  if (!load_rom_status.ok()) {
    UpdateRunWorkflowStatus(
        &status_bar_, project_management_panel_.get(),
        MakeRunStatus("Run load failed", std::string(load_rom_status.message()),
                      ProjectWorkflowState::kFailure));
    toast_manager_.Show(absl::StrFormat("Failed to load run target ROM: %s",
                                        load_rom_status.message()),
                        ToastType::kError);
    return load_rom_status;
  }

  auto reload_status = emulator_.ReloadRuntimeRom(temp_rom.vector());
  if (!reload_status.ok()) {
    UpdateRunWorkflowStatus(
        &status_bar_, project_management_panel_.get(),
        MakeRunStatus("Reload failed", std::string(reload_status.message()),
                      ProjectWorkflowState::kFailure));
    toast_manager_.Show(absl::StrFormat("Failed to reload emulator runtime: %s",
                                        reload_status.message()),
                        ToastType::kError);
    return reload_status;
  }

  if (ui_coordinator_) {
    ui_coordinator_->SetEmulatorVisible(true);
  }
  AppendWorkflowHistoryEntry("Run",
                             MakeRunStatus("Reloaded in emulator", run_target,
                                           ProjectWorkflowState::kSuccess),
                             "");
  UpdateRunWorkflowStatus(&status_bar_, project_management_panel_.get(),
                          MakeRunStatus("Reloaded in emulator", run_target,
                                        ProjectWorkflowState::kSuccess));
  toast_manager_.Show(
      absl::StrFormat("Reloaded project output in emulator: %s", run_target),
      ToastType::kInfo);
  return absl::OkStatus();
}

absl::Status EditorManager::ImportProject(const std::string& project_path) {
  // Delegate to ProjectManager for import logic
  RETURN_IF_ERROR(project_manager_.ImportProject(project_path));
  // Sync local project reference
  DetachActiveProjectContext();
  current_project_ = project_manager_.GetCurrentProject();
  if (session_coordinator_) {
    if (auto* session = session_coordinator_->GetActiveRomSession()) {
      BindProjectContextToSession(session, current_project_);
      RestoreProjectContextForSession(session);
    }
  }
  SyncLayoutScopeFromCurrentProject();
  RefreshHackWorkflowBackend();
  return absl::OkStatus();
}

absl::Status EditorManager::RepairCurrentProject() {
  if (!current_project_.project_opened()) {
    return absl::FailedPreconditionError("No project is currently open");
  }

  RETURN_IF_ERROR(current_project_.RepairProject());
  CaptureActiveProjectContext();
  toast_manager_.Show("Project repaired successfully",
                      editor::ToastType::kSuccess);

  return absl::OkStatus();
}

yaze::zelda3::Overworld* EditorManager::overworld() const {
  if (auto* editor_set = GetCurrentEditorSet()) {
    return editor_set->GetOverworldData();
  }
  return nullptr;
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
        RefreshResourceLabelProvider();
        UpdateCurrentRomHash();
        return absl::OkStatus();
      }
    }
  }
  // If ROM wasn't found in existing sessions, treat as new session.
  // Copying an external ROM object is avoided; instead, fail.
  return absl::NotFoundError("ROM not found in existing sessions");
}

void EditorManager::ApplyDefaultBackupPolicy() {
  rom_lifecycle_.ApplyDefaultBackupPolicy(
      user_settings_.prefs().backup_before_save, "", 20, true, 14);
}

void EditorManager::UpdateCurrentRomHash() {
  rom_lifecycle_.UpdateCurrentRomHash(GetCurrentRom());
}

// IsRomHashMismatch() is now inline in editor_manager.h delegating to
// rom_lifecycle_.

std::vector<RomFileManager::BackupEntry> EditorManager::GetRomBackups() const {
  return rom_lifecycle_.GetRomBackups(GetCurrentRom());
}

absl::Status EditorManager::RestoreRomBackup(const std::string& backup_path) {
  auto* rom = GetCurrentRom();
  if (!rom) {
    return absl::FailedPreconditionError("No ROM loaded");
  }
  const std::string original_filename = rom->filename();
  RETURN_IF_ERROR(rom_file_manager_.LoadRom(rom, backup_path));
  if (!original_filename.empty()) {
    rom->set_filename(original_filename);
  }

  if (session_coordinator_) {
    if (auto* session = session_coordinator_->GetActiveRomSession()) {
      gfx::PaletteManager::Get().ReleaseSession(&session->game_data);
      ResetAssetState(session);
    }
  }
  UpdateCurrentRomHash();
  return LoadAssetsForMode();
}

absl::Status EditorManager::PruneRomBackups() {
  return rom_lifecycle_.PruneRomBackups(GetCurrentRom());
}

void EditorManager::ConfirmRomWrite() {
  if (!PendingRomSaveMatchesActiveSession() ||
      !rom_lifecycle_.IsRomWriteConfirmPending()) {
    return;
  }
  rom_lifecycle_.ConfirmRomWrite();
}

void EditorManager::CancelRomWriteConfirm() {
  CancelPendingRomSave();
}

void EditorManager::ClearPendingWriteConflicts() {
  CancelPendingRomSave();
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
  if (!session_coordinator_) {
    return;
  }

  const size_t current_session_id = GetCurrentSessionId();
  if (!MaybeGuardPendingSessionAction(
          {PendingUnsavedSessionAction::Type::kCloseSession, current_session_id,
           current_session_id})) {
    return;
  }

  session_coordinator_->CloseCurrentSession();
  UpdateCurrentRomHash();
}

void EditorManager::RemoveSession(size_t index) {
  if (!session_coordinator_ ||
      !session_coordinator_->IsValidSessionIndex(index)) {
    return;
  }

  const size_t session_id = session_coordinator_->GetSessionId(index);
  if (!MaybeGuardPendingSessionAction(
          {PendingUnsavedSessionAction::Type::kCloseSession, session_id,
           session_id})) {
    return;
  }

  session_coordinator_->RemoveSession(index);
  UpdateCurrentRomHash();
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

void EditorManager::Quit() {
  if (!MaybeGuardPendingSessionAction(
          {PendingUnsavedSessionAction::Type::kQuit, SIZE_MAX})) {
    return;
  }

  quit_ = true;
}

bool EditorManager::HasPendingUnsavedSessionAction() const {
  return pending_unsaved_session_action_.has_value();
}

std::optional<size_t> EditorManager::ResolveSessionIndexById(
    size_t session_id) const {
  if (!session_coordinator_ || session_id == SIZE_MAX) {
    return std::nullopt;
  }

  for (size_t index = 0; index < session_coordinator_->GetTotalSessionCount();
       ++index) {
    if (session_coordinator_->GetSessionId(index) == session_id) {
      return index;
    }
  }
  return std::nullopt;
}

std::string EditorManager::GetPendingUnsavedSessionActionPrompt() const {
  if (!pending_unsaved_session_action_) {
    return "";
  }

  const auto& action = *pending_unsaved_session_action_;
  if (action.type == PendingUnsavedSessionAction::Type::kQuit) {
    return absl::StrFormat("%s\n\nQuitting now will discard them.",
                           DescribeAllPendingUnsavedWork());
  }

  const auto source_index = ResolveSessionIndexById(action.source_session_id);
  const std::string session_name =
      source_index.has_value()
          ? session_coordinator_->GetSessionDisplayName(*source_index)
          : "Closed Session";
  const std::string work = source_index.has_value()
                               ? DescribePendingUnsavedWork(*source_index)
                               : "unsaved work";

  switch (action.type) {
    case PendingUnsavedSessionAction::Type::kOpenRomDialog:
    case PendingUnsavedSessionAction::Type::kOpenRomOrProjectPath:
      return absl::StrFormat(
          "Session '%s' has %s. Opening another ROM or project will not save "
          "them automatically.",
          session_name, work);
    case PendingUnsavedSessionAction::Type::kOpenProjectDialog:
      return absl::StrFormat(
          "Session '%s' has %s. Opening another project will not save them "
          "automatically.",
          session_name, work);
    case PendingUnsavedSessionAction::Type::kSwitchSession:
      return absl::StrFormat(
          "Session '%s' has %s. Switching sessions will leave them behind in "
          "this session.",
          session_name, work);
    case PendingUnsavedSessionAction::Type::kCloseSession:
      return absl::StrFormat(
          "Session '%s' has %s. Closing it now will discard them.",
          session_name, work);
    case PendingUnsavedSessionAction::Type::kQuit:
      break;
  }

  return "";
}

std::string EditorManager::GetPendingUnsavedSessionActionSaveLabel() const {
  if (!pending_unsaved_session_action_) {
    return "Save Work";
  }

  switch (pending_unsaved_session_action_->type) {
    case PendingUnsavedSessionAction::Type::kOpenRomDialog:
    case PendingUnsavedSessionAction::Type::kOpenRomOrProjectPath:
      return "Save Work & Open";
    case PendingUnsavedSessionAction::Type::kOpenProjectDialog:
      return "Save Work & Open Project";
    case PendingUnsavedSessionAction::Type::kSwitchSession:
      return "Save Work & Switch";
    case PendingUnsavedSessionAction::Type::kCloseSession:
      return "Save Work & Close";
    case PendingUnsavedSessionAction::Type::kQuit:
      return ModifiedSessionCount() == 1 ? "Save Work & Quit"
                                         : "Save Modified Work & Quit";
  }

  return "Save Work";
}

std::string EditorManager::GetPendingUnsavedSessionActionContinueLabel() const {
  if (!pending_unsaved_session_action_) {
    return "Continue";
  }

  switch (pending_unsaved_session_action_->type) {
    case PendingUnsavedSessionAction::Type::kOpenRomDialog:
    case PendingUnsavedSessionAction::Type::kOpenRomOrProjectPath:
      return "Open Without Saving";
    case PendingUnsavedSessionAction::Type::kOpenProjectDialog:
      return "Open Project Without Saving";
    case PendingUnsavedSessionAction::Type::kSwitchSession:
      return "Switch Without Saving";
    case PendingUnsavedSessionAction::Type::kCloseSession:
      return "Close Without Saving";
    case PendingUnsavedSessionAction::Type::kQuit:
      return "Quit Without Saving";
  }

  return "Continue";
}

void EditorManager::ConfirmPendingUnsavedSessionActionSaveAndContinue() {
  if (!pending_unsaved_session_action_) {
    return;
  }

  const auto action = *pending_unsaved_session_action_;
  pending_unsaved_session_action_.reset();

  if (popup_manager_) {
    popup_manager_->Hide(PopupID::kUnsavedSessionChanges);
  }

  const size_t original_session_id = GetCurrentSessionId();
  auto save_modified_session = [this](size_t session_id) -> absl::Status {
    const auto session_index = ResolveSessionIndexById(session_id);
    if (!session_index.has_value()) {
      return absl::FailedPreconditionError(
          "The ROM session is no longer available");
    }
    if (!SessionHasPendingUnsavedWork(*session_index)) {
      return absl::OkStatus();
    }

    session_coordinator_->SwitchToSession(*session_index);
    RETURN_IF_ERROR(SaveActiveProjectEditingWork());
    if (SessionHasPendingRomWork(*session_index)) {
      RETURN_IF_ERROR(SaveRom());
    }

    // A successful file write does not necessarily mean every pending editor
    // domain participated in the save. For example, dungeon palette saving is
    // user-configurable. Never execute a destructive follow-up while work that
    // the confirmation dialog promised to save is still pending.
    if (SessionHasPendingUnsavedWork(*session_index)) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Save completed, but session still has %s",
                          DescribePendingUnsavedWork(*session_index)));
    }
    return absl::OkStatus();
  };

  absl::Status save_status = absl::OkStatus();
  if (action.type == PendingUnsavedSessionAction::Type::kQuit) {
    if (session_coordinator_) {
      std::vector<size_t> session_ids;
      session_ids.reserve(session_coordinator_->GetTotalSessionCount());
      for (size_t i = 0; i < session_coordinator_->GetTotalSessionCount();
           ++i) {
        session_ids.push_back(session_coordinator_->GetSessionId(i));
      }
      for (const size_t session_id : session_ids) {
        save_status = save_modified_session(session_id);
        if (!save_status.ok()) {
          break;
        }
      }
    }
  } else {
    save_status = save_modified_session(action.source_session_id);
  }

  if (!save_status.ok()) {
    const bool resumable_confirmation = absl::IsCancelled(save_status) &&
                                        HasPendingRomSaveConfirmation() &&
                                        PendingRomSaveMatchesActiveSession();
    const auto original_session_index =
        ResolveSessionIndexById(original_session_id);
    if (!resumable_confirmation && original_session_index.has_value()) {
      session_coordinator_->SwitchToSession(*original_session_index);
    }

    if (absl::IsCancelled(save_status)) {
      toast_manager_.Show("Save paused. Finish saving, then retry your action.",
                          ToastType::kInfo);
    } else {
      toast_manager_.Show(
          absl::StrFormat("Failed to save before continuing: %s",
                          save_status.message()),
          ToastType::kError);
    }
    return;
  }

  // Saving an inactive close target temporarily activates that session. Put
  // the original session back before compacting the target index so the same
  // stable session remains active after the close.
  if (action.type == PendingUnsavedSessionAction::Type::kCloseSession &&
      original_session_id != action.target_session_id) {
    const auto original_session_index =
        ResolveSessionIndexById(original_session_id);
    if (original_session_index.has_value()) {
      session_coordinator_->SwitchToSession(*original_session_index);
    }
  }

  ExecutePendingUnsavedSessionAction(action);
}

void EditorManager::ConfirmPendingUnsavedSessionActionDiscardAndContinue() {
  if (!pending_unsaved_session_action_) {
    return;
  }

  const auto action = *pending_unsaved_session_action_;
  pending_unsaved_session_action_.reset();
  if (popup_manager_) {
    popup_manager_->Hide(PopupID::kUnsavedSessionChanges);
  }
  ExecutePendingUnsavedSessionAction(action);
}

void EditorManager::CancelPendingUnsavedSessionAction() {
  pending_unsaved_session_action_.reset();
  if (popup_manager_) {
    popup_manager_->Hide(PopupID::kUnsavedSessionChanges);
  }
}

bool EditorManager::MaybeGuardPendingSessionAction(
    PendingUnsavedSessionAction action) {
  CaptureActiveProjectEditingState();
  const auto source_index = ResolveSessionIndexById(action.source_session_id);
  const bool has_pending_work =
      action.type == PendingUnsavedSessionAction::Type::kQuit
          ? HasAnySessionPendingUnsavedWork()
          : source_index.has_value() &&
                SessionHasPendingUnsavedWork(*source_index);
  if (!has_pending_work) {
    return true;
  }

  pending_unsaved_session_action_ = std::move(action);
  if (popup_manager_) {
    popup_manager_->Show(PopupID::kUnsavedSessionChanges);
  }
  return false;
}

void EditorManager::ExecutePendingUnsavedSessionAction(
    const PendingUnsavedSessionAction& action) {
  switch (action.type) {
    case PendingUnsavedSessionAction::Type::kOpenRomDialog: {
      auto status = LoadRomInternal();
      if (!status.ok()) {
        toast_manager_.Show(
            absl::StrFormat("Failed to load ROM: %s", status.message()),
            ToastType::kError);
      }
      break;
    }
    case PendingUnsavedSessionAction::Type::kOpenRomOrProjectPath: {
      auto status = OpenRomOrProjectInternal(action.path);
      if (!status.ok()) {
        toast_manager_.Show(absl::StrFormat("Failed to open ROM / project: %s",
                                            status.message()),
                            ToastType::kError);
      }
      break;
    }
    case PendingUnsavedSessionAction::Type::kOpenProjectDialog: {
      auto status = OpenProjectInternal();
      if (!status.ok()) {
        toast_manager_.Show(
            absl::StrFormat("Failed to open project: %s", status.message()),
            ToastType::kError);
      }
      break;
    }
    case PendingUnsavedSessionAction::Type::kSwitchSession:
      if (session_coordinator_) {
        const auto target_index =
            ResolveSessionIndexById(action.target_session_id);
        if (target_index.has_value()) {
          session_coordinator_->SwitchToSession(*target_index);
        }
      }
      break;
    case PendingUnsavedSessionAction::Type::kCloseSession:
      if (session_coordinator_) {
        const auto target_index =
            ResolveSessionIndexById(action.target_session_id);
        if (target_index.has_value()) {
          session_coordinator_->RemoveSession(*target_index);
          UpdateCurrentRomHash();
        }
      }
      break;
    case PendingUnsavedSessionAction::Type::kQuit:
      quit_ = true;
      break;
  }
}

bool EditorManager::SessionHasPendingUnsavedWork(size_t session_index) const {
  if (!session_coordinator_ ||
      !session_coordinator_->IsValidSessionIndex(session_index)) {
    return false;
  }

  auto* session =
      static_cast<RomSession*>(session_coordinator_->GetSession(session_index));
  return session != nullptr &&
         (SessionHasPendingRomWork(session_index) || session->project_dirty ||
          (session->project_file_editor_state.initialized &&
           session->project_file_editor_state.modified));
}

bool EditorManager::SessionHasPendingRomWork(size_t session_index) const {
  if (!session_coordinator_ ||
      !session_coordinator_->IsValidSessionIndex(session_index)) {
    return false;
  }

  auto* session =
      static_cast<RomSession*>(session_coordinator_->GetSession(session_index));
  return session != nullptr &&
         ((session->rom.is_loaded() && session->rom.dirty()) ||
          PendingDungeonRoomCountForSession(session_index) > 0 ||
          gfx::PaletteManager::Get().HasUnsavedChanges(&session->game_data));
}

bool EditorManager::HasAnySessionPendingUnsavedWork() const {
  return ModifiedSessionCount() > 0;
}

int EditorManager::PendingDungeonRoomCountForSession(
    size_t session_index) const {
  if (!session_coordinator_ ||
      !session_coordinator_->IsValidSessionIndex(session_index)) {
    return 0;
  }

  auto* session =
      static_cast<RomSession*>(session_coordinator_->GetSession(session_index));
  if (!session) {
    return 0;
  }

  if (auto* dungeon_editor =
          session->editors.GetEditorAs<DungeonEditorV2>(EditorType::kDungeon)) {
    return dungeon_editor->PendingRoomCount();
  }
  return 0;
}

size_t EditorManager::PendingPaletteColorCountForSession(
    size_t session_index) const {
  if (!session_coordinator_ ||
      !session_coordinator_->IsValidSessionIndex(session_index)) {
    return 0;
  }

  auto* session =
      static_cast<RomSession*>(session_coordinator_->GetSession(session_index));
  return session != nullptr ? gfx::PaletteManager::Get().GetModifiedColorCount(
                                  &session->game_data)
                            : 0;
}

int EditorManager::ModifiedSessionCount() const {
  if (!session_coordinator_) {
    return 0;
  }

  int count = 0;
  for (size_t i = 0; i < session_coordinator_->GetTotalSessionCount(); ++i) {
    if (SessionHasPendingUnsavedWork(i)) {
      ++count;
    }
  }
  return count;
}

std::string EditorManager::DescribePendingUnsavedWork(
    size_t session_index) const {
  if (!session_coordinator_ ||
      !session_coordinator_->IsValidSessionIndex(session_index)) {
    return "unsaved work";
  }

  auto* session =
      static_cast<RomSession*>(session_coordinator_->GetSession(session_index));
  const bool rom_dirty =
      session != nullptr && session->rom.is_loaded() && session->rom.dirty();
  const int pending_rooms = PendingDungeonRoomCountForSession(session_index);
  const size_t pending_palette_colors =
      PendingPaletteColorCountForSession(session_index);
  const bool project_dirty = session != nullptr && session->project_dirty;
  const bool project_file_dirty =
      session != nullptr && session->project_file_editor_state.initialized &&
      session->project_file_editor_state.modified;

  std::vector<std::string> work;
  if (pending_rooms > 0) {
    work.push_back(absl::StrFormat("%d unapplied dungeon room%s", pending_rooms,
                                   pending_rooms == 1 ? "" : "s"));
  }
  if (pending_palette_colors > 0) {
    work.push_back(absl::StrFormat("%zu unapplied palette color%s",
                                   pending_palette_colors,
                                   pending_palette_colors == 1 ? "" : "s"));
  }
  if (rom_dirty) {
    work.emplace_back("unsaved ROM-buffer changes");
  }
  if (project_dirty) {
    work.emplace_back("unsaved project settings");
  }
  if (project_file_dirty) {
    work.emplace_back("an unsaved project-file draft");
  }
  return work.empty() ? "unsaved work" : absl::StrJoin(work, " and ");
}

std::string EditorManager::DescribeAllPendingUnsavedWork() const {
  const int modified_sessions = ModifiedSessionCount();
  if (modified_sessions <= 0) {
    return "No sessions have unsaved work.";
  }

  if (modified_sessions == 1 && session_coordinator_) {
    for (size_t i = 0; i < session_coordinator_->GetTotalSessionCount(); ++i) {
      if (SessionHasPendingUnsavedWork(i)) {
        return absl::StrFormat("Session '%s' has %s.",
                               session_coordinator_->GetSessionDisplayName(i),
                               DescribePendingUnsavedWork(i));
      }
    }
  }

  return absl::StrFormat(
      "%d sessions have unsaved ROM, dungeon, palette, or project work.",
      modified_sessions);
}

size_t EditorManager::GetCurrentSessionIndex() const {
  return session_coordinator_ ? session_coordinator_->GetActiveSessionIndex()
                              : 0;
}

EditorManager::UiSyncState EditorManager::GetUiSyncStateSnapshot() const {
  UiSyncState state;
  state.frame_id = ui_sync_frame_id_.load(std::memory_order_relaxed);

  int pending_editor =
      pending_editor_deferred_actions_.load(std::memory_order_relaxed);
  if (pending_editor < 0) {
    pending_editor = 0;
  }
  state.pending_editor_actions = pending_editor;

  int pending_layout = layout_coordinator_.PendingDeferredActionCount();
  if (pending_layout < 0) {
    pending_layout = 0;
  }
  state.pending_layout_actions = pending_layout;
  state.layout_rebuild_pending =
      layout_manager_ ? layout_manager_->IsRebuildRequested() : false;
  return state;
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

void EditorManager::SwitchToEditor(EditorType editor_type, bool force_visible,
                                   bool from_dialog) {
  // Special case: Agent editor requires EditorManager-specific handling
#ifdef YAZE_BUILD_AGENT_UI
  if (editor_type == EditorType::kAgent) {
    ShowAIAgent();
    return;
  }
#endif

  // Fresh launch has no ROM session, so GetCurrentEditorSet() is null and
  // EditorActivator::SwitchToEditor would silently no-op (BUG-020) — that was
  // why File->Settings, the "no ROM" welcome buttons, and Tools editors did
  // nothing. Create an empty session on demand so ROM-less editors (Settings,
  // Assembly, ...) have an editor set to live in. Normal ROM loads create their
  // own session, so this only fires when the user reaches an editor first.
  if (session_coordinator_ && GetCurrentEditorSet() == nullptr) {
    session_coordinator_->CreateNewSession();
  }

  auto status = EnsureEditorAssetsLoaded(editor_type);
  if (!status.ok()) {
    toast_manager_.Show(
        absl::StrFormat("Failed to prepare %s: %s",
                        kEditorNames[static_cast<int>(editor_type)],
                        status.message()),
        ToastType::kError);
  }

  // Delegate all other editor switching to EditorActivator
  editor_activator_.SwitchToEditor(editor_type, force_visible, from_dialog);
}

void EditorManager::DismissEditorSelection() {
  if (!ui_coordinator_) {
    return;
  }
  ui_coordinator_->SetEditorSelectionVisible(false);
  ui_coordinator_->SetStartupSurface(StartupSurface::kEditor);
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
  // Menu actions can run before the next Update() frame captures drawer
  // edits. Preserve the panel's current dirty bit before rebinding it.
  CaptureActiveProjectEditingState();
  if (right_drawer_manager_) {
    // Update project panel context before showing
    if (project_management_panel_) {
      project_management_panel_->SetProject(&current_project_,
                                            IsCurrentProjectDirty());
      project_management_panel_->SetVersionManager(version_manager_);
      project_management_panel_->SetRom(GetCurrentRom());
    }
    right_drawer_manager_->ToggleDrawer(
        RightDrawerManager::DrawerType::kProject);
  }
}

void EditorManager::ShowProjectFileEditor() {
  CaptureActiveProjectEditingState();
  auto* session = session_coordinator_
                      ? session_coordinator_->GetActiveRomSession()
                      : nullptr;

  // Preserve an existing draft for this session. Only load from disk the first
  // time this session opens the project-file editor.
  if (session && session->project_file_editor_state.initialized &&
      !session->project_file_editor_state.modified &&
      !current_project_.filepath.empty() &&
      !SessionCoordinator::PathsReferToSameBackingFile(
          session->project_file_editor_state.filepath,
          current_project_.filepath)) {
    RebaseCleanProjectFileDraft(current_project_.filepath);
  }

  if (session && session->project_file_editor_state.initialized) {
    project_file_editor_.RestoreState(session->project_file_editor_state,
                                      &current_project_);
  } else if (!current_project_.filepath.empty()) {
    auto status = project_file_editor_.LoadFile(current_project_.filepath);
    if (!status.ok()) {
      toast_manager_.Show(
          absl::StrFormat("Failed to load project file: %s", status.message()),
          ToastType::kError);
      return;
    }
  } else {
    project_file_editor_.ResetForProject(&current_project_);
  }
  // Set the project pointer for label import functionality
  project_file_editor_.SetProject(&current_project_);
  // Activate the editor window
  project_file_editor_.set_active(true);
  if (session) {
    session->project_file_editor_state = project_file_editor_.CaptureState();
  }
}

void EditorManager::ConfigureEditorDependencies(EditorSet* editor_set, Rom* rom,
                                                size_t session_id) {
  if (!editor_set) {
    return;
  }

  EditorDependencies deps;
  deps.rom = rom;
  deps.session_id = session_id;
  deps.window_manager = &window_manager_;
  deps.toast_manager = &toast_manager_;
  deps.popup_manager = popup_manager_.get();
  deps.shortcut_manager = &shortcut_manager_;
  deps.shared_clipboard = &shared_clipboard_;
  deps.user_settings = &user_settings_;
  if (session_coordinator_) {
    const auto session_index = ResolveSessionIndexById(session_id);
    if (session_index.has_value()) {
      auto* session = static_cast<RomSession*>(
          session_coordinator_->GetSession(*session_index));
      if (session && session->project_context.has_value()) {
        deps.project = &*session->project_context;
        deps.version_manager = session->version_manager.get();
      }
    }
  }
  deps.global_context = editor_context_.get();
  deps.status_bar = &status_bar_;
  deps.renderer = renderer_;
  deps.emulator = &emulator_;
  deps.custom_data = this;
  deps.gfx_group_workspace = editor_set->gfx_group_workspace();

  editor_set->ApplyDependencies(deps);

  // If configuring the active session, update the properties panel
  if (session_id == GetCurrentSessionId()) {
    selection_properties_panel_.SetRom(rom);
  }
}

}  // namespace yaze::editor
