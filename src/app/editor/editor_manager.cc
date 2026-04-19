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
#include "app/editor/system/default_editor_factories.h"
#include "app/editor/system/editor_registry.h"
#include "app/editor/system/project_workflow_status.h"
#include "app/editor/system/shortcut_configurator.h"
#include "app/editor/system/workspace_window_manager.h"
#include "app/editor/ui/dashboard_panel.h"
#include "app/editor/ui/popup_manager.h"
#include "app/editor/ui/project_management_panel.h"
#include "app/editor/ui/toast_manager.h"
#include "app/editor/ui/ui_coordinator.h"
#include "app/emu/emulator.h"
#include "app/gfx/debug/performance/performance_dashboard.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/animation/animator.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/theme_manager.h"
#include "app/platform/ios/ios_platform_state.h"
#include "app/platform/timing.h"
#include "app/test/test_manager.h"
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
#include "startup_flags.h"
#include "util/file_util.h"
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
#include "app/test/core_systems_test_suite.h"
#include "app/test/e2e_test_suite.h"
#include "app/test/integrated_test_suite.h"
#include "app/test/rom_dependent_test_suite.h"
#include "app/test/zscustomoverworld_test_suite.h"
#endif
#ifdef YAZE_ENABLE_GTEST
#include "app/test/unit_test_suite.h"
#endif
#ifdef YAZE_WITH_GRPC
#include "app/test/z3ed_test_suite.h"
#endif

// Conditional agent UI headers
#ifdef YAZE_BUILD_AGENT_UI
#include "app/editor/agent/agent_chat.h"
#endif

namespace yaze::editor {

namespace {
bool HasAnyOverride(const core::RomAddressOverrides& overrides,
                    std::initializer_list<const char*> keys) {
  for (const char* key : keys) {
    if (overrides.addresses.find(key) != overrides.addresses.end()) {
      return true;
    }
  }
  return false;
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

std::optional<EditorType> ParseEditorTypeFromString(absl::string_view name) {
  const std::string lower = absl::AsciiStrToLower(std::string(name));
  for (int i = 0; i < static_cast<int>(EditorType::kSettings) + 1; ++i) {
    const std::string candidate = absl::AsciiStrToLower(
        EditorRegistry::GetEditorName(static_cast<EditorType>(i)));
    if (candidate == lower) {
      return static_cast<EditorType>(i);
    }
  }
  return std::nullopt;
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
  window_manager_.RegisterWindowContent(
      std::make_unique<workflow::ProjectWorkflowOutputPanel>());
  project_management_panel_->SetSwapRomCallback([this]() {
    // Prompt user to select a new ROM for the project
    auto rom_path = util::FileDialogWrapper::ShowOpenFileDialog(
        util::MakeRomFileDialogOptions(false));
    if (!rom_path.empty()) {
      current_project_.rom_filename = rom_path;
      auto status = current_project_.Save();
      if (status.ok()) {
        toast_manager_.Show("Project ROM updated. Reload to apply changes.",
                            ToastType::kSuccess);
      } else {
        toast_manager_.Show("Failed to update project ROM", ToastType::kError);
      }
    }
  });
  project_management_panel_->SetReloadRomCallback([this]() {
    if (current_project_.project_opened() &&
        !current_project_.rom_filename.empty()) {
      auto status = LoadProjectWithRom();
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
        HandleSessionSwitched(e.new_index, e.session);
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
        auto& prefs = user_settings_.prefs();
        prefs.panel_visibility_state[e.category][e.base_panel_id] = e.visible;
        settings_dirty_ = true;
        settings_dirty_timestamp_ = TimingManager::Get().GetElapsedTime();
      });
}

EditorManager::~EditorManager() {
  // ThemeManager is a singleton that outlives us. Clear the theme-changed
  // callback so it stops calling back into a destroyed EditorManager via the
  // captured `this` pointer. Matters for unit tests that construct and destroy
  // multiple EditorManager instances in the same process.
  gui::ThemeManager::Get().SetOnThemeChangedCallback(nullptr);

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

void EditorManager::HandleSessionSwitched(size_t new_index,
                                          RomSession* session) {
  // Update RightDrawerManager with the new session's settings editor
  if (right_drawer_manager_ && session) {
    right_drawer_manager_->SetSettingsPanel(
        session->editors.GetSettingsPanel());
  }

  // Update properties panel with new ROM
  if (session) {
    selection_properties_panel_.SetRom(&session->rom);
  }

  // Update ContentRegistry context with current session's ROM and GameData
  ContentRegistry::Context::SetRom(session ? &session->rom : nullptr);
  ContentRegistry::Context::SetGameData(session ? &session->game_data
                                                : nullptr);

  // Keep room/sprite labels in sync with active session context.
  RefreshResourceLabelProvider();

  const std::string category = window_manager_.GetActiveCategory();
  if (!category.empty() &&
      category != WorkspaceWindowManager::kDashboardCategory) {
    auto it = user_settings_.prefs().panel_visibility_state.find(category);
    if (it != user_settings_.prefs().panel_visibility_state.end()) {
      window_manager_.RestoreVisibilityState(new_index, it->second);
    }
  }

#ifdef YAZE_ENABLE_TESTING
  test::TestManager::Get().SetCurrentRom(session ? &session->rom : nullptr);
#endif

  LOG_DEBUG("EditorManager", "Session switched to %zu via EventBus", new_index);
}

void EditorManager::HandleSessionCreated(size_t index, RomSession* session) {
  window_manager_.RegisterRegistryWindowContentsForSession(index);
  window_manager_.RestorePinnedState(user_settings_.prefs().pinned_panels);
  LOG_INFO("EditorManager", "Session %zu created via EventBus", index);
}

void EditorManager::HandleSessionClosed(size_t index) {
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
        if (status.ok()) {
          toast_manager_.Show("ROM saved successfully", ToastType::kSuccess);
        } else if (!absl::IsCancelled(status)) {
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

  auto& prefs = user_settings_.prefs();
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

        // Create project if requested
        if (options.create_project && !options.project_name.empty()) {
          project_manager_.SetProjectRom(GetCurrentRom()->filename());
          auto status = project_manager_.FinalizeProjectCreation(
              options.project_name, options.project_path);
          if (!status.ok()) {
            toast_manager_.Show("Failed to create project", ToastType::kError);
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

  welcome_screen_.SetNewProjectCallback([this]() {
    status_ = CreateNewProject();
    if (status_.ok() && ui_coordinator_) {
      ui_coordinator_->SetWelcomeScreenVisible(false);
      ui_coordinator_->SetWelcomeScreenManuallyClosed(true);
    }
  });

  welcome_screen_.SetNewProjectWithTemplateCallback(
      [this](const std::string& template_name) {
        status_ = CreateNewProject(template_name);
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
    event_bus_.Publish(JumpToMapRequestEvent::Create(config.jump_to_map,
                                                     GetCurrentSessionId()));
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

  // Autosave timer
  if (user_settings_.prefs().autosave_enabled && current_rom &&
      current_rom->dirty()) {
    autosave_timer_ += ImGui::GetIO().DeltaTime;
    if (autosave_timer_ >= user_settings_.prefs().autosave_interval) {
      autosave_timer_ = 0.0f;
      Rom::SaveSettings s;
      s.backup = true;
      s.save_new = false;
      auto st = current_rom->SaveToFile(s);
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
      activity_bar_->Render(GetCurrentSessionId(), sidebar_category,
                            all_categories, active_editor_categories,
                            has_rom_callback, is_rom_dirty_callback);
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
    status_bar_.SetSessionInfo(GetCurrentSessionId(),
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
  auto load_from_path = [this](const std::string& file_name) -> absl::Status {
    if (file_name.empty()) {
      return absl::OkStatus();
    }

    // Check if this is a project file - route to project loading
    if (absl::EndsWith(file_name, ".yaze") ||
        absl::EndsWith(file_name, ".zsproj") ||
        absl::EndsWith(file_name, ".yazeproj")) {
      return OpenRomOrProject(file_name);
    }

    if (session_coordinator_->HasDuplicateSession(file_name)) {
      toast_manager_.Show("ROM already open in another session",
                          editor::ToastType::kWarning);
      return absl::OkStatus();
    }

    // Delegate ROM loading to RomFileManager
    Rom temp_rom;
    RETURN_IF_ERROR(rom_file_manager_.LoadRom(&temp_rom, file_name));

    auto session_or = session_coordinator_->CreateSessionFromRom(
        std::move(temp_rom), file_name);
    if (!session_or.ok()) {
      return session_or.status();
    }

    ConfigureEditorDependencies(GetCurrentEditorSet(), GetCurrentRom(),
                                GetCurrentSessionId());
    UpdateCurrentRomHash();

    core::RomSettings::Get().ClearOverrides();
    zelda3::CustomObjectManager::Get().ClearObjectFileMap();
    ApplyDefaultBackupPolicy();

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
    manager.AddFile(file_name);
    manager.Save();

    RETURN_IF_ERROR(LoadAssetsForMode());

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
absl::Status EditorManager::CheckRomWritePolicy() {
  return rom_lifecycle_.CheckRomWritePolicy(GetCurrentRom());
}

absl::Status EditorManager::CheckOracleRomSafetyPreSave(Rom* rom) {
  return rom_lifecycle_.CheckOracleRomSafetyPreSave(rom);
}

absl::Status EditorManager::SaveRom() {
  auto* current_rom = GetCurrentRom();
  auto* current_editor_set = GetCurrentEditorSet();
  if (!current_rom || !current_editor_set) {
    return absl::FailedPreconditionError("No ROM or editor set loaded");
  }

  // --- State machine checks (delegated to RomLifecycleManager) ---
  if (rom_lifecycle_.IsRomWriteConfirmPending()) {
    return absl::CancelledError("Save pending confirmation");
  }

  RETURN_IF_ERROR(CheckRomWritePolicy());

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
  if (current_project_.project_opened()) {
    rom_lifecycle_.ApplyDefaultBackupPolicy(
        current_project_.workspace_settings.backup_on_save,
        current_project_.GetAbsolutePath(current_project_.rom_backup_folder),
        current_project_.workspace_settings.backup_retention_count,
        current_project_.workspace_settings.backup_keep_daily,
        current_project_.workspace_settings.backup_keep_daily_days);
  } else {
    rom_lifecycle_.ApplyDefaultBackupPolicy(
        user_settings_.prefs().backup_before_save, "", 20, true, 14);
  }

  // --- Save editor-specific data ---
  if (auto* editor = current_editor_set->GetEditor(EditorType::kScreen)) {
    RETURN_IF_ERROR(editor->Save());
  }

  if (auto* editor = current_editor_set->GetEditor(EditorType::kDungeon)) {
    RETURN_IF_ERROR(editor->Save());
  }

  if (auto* editor = current_editor_set->GetEditor(EditorType::kOverworld)) {
    RETURN_IF_ERROR(editor->Save());
  }

  if (core::FeatureFlags::get().kSaveMessages) {
    RETURN_IF_ERROR(EnsureEditorAssetsLoaded(EditorType::kMessage));
    if (auto* editor = current_editor_set->GetEditor(EditorType::kMessage)) {
      RETURN_IF_ERROR(editor->Save());
    }
  }

  if (core::FeatureFlags::get().kSaveGraphicsSheet) {
    RETURN_IF_ERROR(zelda3::SaveAllGraphicsData(
        *current_rom, gfx::Arena::Get().gfx_sheets()));
  }

  // Oracle guardrails: refuse to write obviously corrupted ROM layouts.
  RETURN_IF_ERROR(CheckOracleRomSafetyPreSave(current_rom));

  // --- Write conflict check (ASM-owned address protection) ---
  if (current_project_.project_opened() &&
      current_project_.hack_manifest.loaded()) {
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
        write_ranges = current_editor_set->CollectDungeonWriteRanges();
      }

      if (!write_ranges.empty()) {
        auto conflicts =
            current_project_.hack_manifest.AnalyzePcWriteRanges(write_ranges);
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

  // Delegate final ROM file writing to RomFileManager
  auto save_status = rom_file_manager_.SaveRom(current_rom);
  if (save_status.ok()) {
    // Write-confirm bypass is single-use. Clear it after a successful save.
    rom_lifecycle_.CancelRomWriteConfirm();
  }
  return save_status;
}

absl::Status EditorManager::SaveRomAs(const std::string& filename) {
  auto* current_rom = GetCurrentRom();
  if (!current_rom) {
    return absl::FailedPreconditionError("No ROM loaded");
  }

  // Reuse SaveRom() logic for editor-specific data saving
  RETURN_IF_ERROR(SaveRom());

  // Now save to the new filename
  auto save_status = rom_file_manager_.SaveRomAs(current_rom, filename);
  if (save_status.ok()) {
    // Update session filepath
    if (session_coordinator_) {
      auto* session = session_coordinator_->GetActiveRomSession();
      if (session) {
        session->filepath = filename;
      }
    }

    // Add to recent files
    auto& manager = project::RecentFilesManager::GetInstance();
    manager.AddFile(filename);
    manager.Save();
  }

  return save_status;
}

absl::Status EditorManager::OpenRomOrProject(const std::string& filename) {
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
    // Open the project file
    RETURN_IF_ERROR(current_project_.Open(filename));
    SyncLayoutScopeFromCurrentProject();
    RefreshHackWorkflowBackend();

    // Initialize VersionManager for the project
    version_manager_ =
        std::make_unique<core::VersionManager>(&current_project_);
    version_manager_->InitializeGit();  // Try to init git if configured

    // Load ROM directly from project - don't prompt user
    return LoadProjectWithRom();
  } else {
#ifdef __EMSCRIPTEN__
    app::platform::WasmLoadingManager::UpdateProgress(loading_handle, 0.05f);
    app::platform::WasmLoadingManager::UpdateMessage(loading_handle,
                                                     "Loading ROM data...");
#endif
    Rom temp_rom;
    RETURN_IF_ERROR(rom_file_manager_.LoadRom(&temp_rom, filename));
    RETURN_IF_ERROR(rom_lifecycle_.CheckRomOpenPolicy(&temp_rom));

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
    RETURN_IF_ERROR(LoadAssetsForMode(loading_handle));
#else
    RETURN_IF_ERROR(LoadAssetsForMode());
#endif

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
  // Delegate to ProjectManager
  auto status = project_manager_.CreateNewProject(template_name);
  if (status.ok()) {
    current_project_ = project_manager_.GetCurrentProject();
    SyncLayoutScopeFromCurrentProject();

    // Trigger ROM selection dialog - projects need a ROM to be useful
    // LoadRom() opens file dialog and shows ROM load options when ROM is loaded
    status = LoadRom();
#if !(defined(__APPLE__) && TARGET_OS_IOS == 1)
    if (status.ok() && ui_coordinator_) {
      ui_coordinator_->SetWelcomeScreenVisible(false);
      ui_coordinator_->SetWelcomeScreenManuallyClosed(true);
    }
#endif
  }
  return status;
}

absl::Status EditorManager::OpenProject() {
  auto open_project_from_path =
      [this](const std::string& file_path) -> absl::Status {
    if (file_path.empty()) {
      return absl::OkStatus();
    }

    project::YazeProject new_project;
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
    SyncLayoutScopeFromCurrentProject();

    // Initialize VersionManager for the project
    version_manager_ =
        std::make_unique<core::VersionManager>(&current_project_);
    version_manager_->InitializeGit();

    return LoadProjectWithRom();
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

absl::Status EditorManager::LoadProjectWithRom() {
  // Check if project has a ROM file specified
  if (current_project_.rom_filename.empty()) {
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
        return absl::OkStatus();
      }
    }
    util::FileDialogWrapper::ShowOpenFileDialogAsync(
        util::MakeRomFileDialogOptions(false),
        [this](const std::string& rom_path) {
          if (rom_path.empty()) {
            return;
          }
          current_project_.rom_filename = rom_path;
          auto save_status = current_project_.Save();
          if (!save_status.ok()) {
            toast_manager_.Show(
                absl::StrFormat("Failed to update project ROM: %s",
                                save_status.message()),
                ToastType::kError);
            return;
          }
          auto status = LoadProjectWithRom();
          if (!status.ok()) {
            toast_manager_.Show(
                absl::StrFormat("Failed to load project ROM: %s",
                                status.message()),
                ToastType::kError);
          }
        });
    return absl::OkStatus();
#else
    auto rom_path = util::FileDialogWrapper::ShowOpenFileDialog(
        util::MakeRomFileDialogOptions(false));
    if (rom_path.empty()) {
      return absl::OkStatus();
    }
    current_project_.rom_filename = rom_path;
    // Save updated project
    RETURN_IF_ERROR(current_project_.Save());
#endif
  }

  // Load ROM from project
  Rom temp_rom;
  auto load_status =
      rom_file_manager_.LoadRom(&temp_rom, current_project_.rom_filename);
  if (!load_status.ok()) {
    // ROM file not found or invalid - prompt user to select new ROM
    toast_manager_.Show(
        absl::StrFormat("Could not load ROM '%s': %s. Please select a new ROM.",
                        current_project_.rom_filename, load_status.message()),
        editor::ToastType::kWarning, 5.0f);
#if defined(__APPLE__) && TARGET_OS_IOS == 1
    // If the ROM is inside a .yazeproj bundle its path may not be readable
    // yet because iCloud hasn't finished downloading it.  Saving any other
    // path here would corrupt the project file with a temporary location.
    // Show guidance and bail without touching current_project_.rom_filename.
    {
      auto rom_parent =
          std::filesystem::path(current_project_.rom_filename).parent_path();
      if (rom_parent.extension() == ".yazeproj") {
        toast_manager_.Show(
            "ROM is still downloading from iCloud. Try reopening the project "
            "in a moment.",
            ToastType::kInfo, 6.0f);
        return absl::OkStatus();
      }
    }
    util::FileDialogWrapper::ShowOpenFileDialogAsync(
        util::MakeRomFileDialogOptions(false),
        [this](const std::string& rom_path) {
          if (rom_path.empty()) {
            return;
          }
          current_project_.rom_filename = rom_path;
          auto save_status = current_project_.Save();
          if (!save_status.ok()) {
            toast_manager_.Show(
                absl::StrFormat("Failed to update project ROM: %s",
                                save_status.message()),
                ToastType::kError);
            return;
          }
          auto status = LoadProjectWithRom();
          if (!status.ok()) {
            toast_manager_.Show(
                absl::StrFormat("Failed to load project ROM: %s",
                                status.message()),
                ToastType::kError);
          }
        });
    return absl::OkStatus();
#else
    auto rom_path = util::FileDialogWrapper::ShowOpenFileDialog(
        util::MakeRomFileDialogOptions(false));
    if (rom_path.empty()) {
      return absl::OkStatus();
    }
    current_project_.rom_filename = rom_path;
    RETURN_IF_ERROR(current_project_.Save());
    RETURN_IF_ERROR(rom_file_manager_.LoadRom(&temp_rom, rom_path));
#endif
  }

  RETURN_IF_ERROR(rom_lifecycle_.CheckRomOpenPolicy(&temp_rom));

  auto session_or = session_coordinator_->CreateSessionFromRom(
      std::move(temp_rom), current_project_.rom_filename);
  if (!session_or.ok()) {
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

  core::RomSettings::Get().SetAddressOverrides(
      current_project_.rom_address_overrides);
  if (!current_project_.custom_object_files.empty()) {
    zelda3::CustomObjectManager::Get().SetObjectFileMap(
        current_project_.custom_object_files);
  } else {
    zelda3::CustomObjectManager::Get().ClearObjectFileMap();
  }
  if (!current_project_.custom_objects_folder.empty()) {
    zelda3::CustomObjectManager::Get().Initialize(
        current_project_.GetAbsolutePath(
            current_project_.custom_objects_folder));
  } else {
    // Avoid inheriting stale singleton state from previous projects.
    zelda3::CustomObjectManager::Get().Initialize("");
  }
  rom_file_manager_.SetBackupBeforeSave(
      current_project_.workspace_settings.backup_on_save);
  rom_file_manager_.SetBackupFolder(
      current_project_.GetAbsolutePath(current_project_.rom_backup_folder));
  rom_file_manager_.SetBackupRetentionCount(
      current_project_.workspace_settings.backup_retention_count);
  rom_file_manager_.SetBackupKeepDaily(
      current_project_.workspace_settings.backup_keep_daily);
  rom_file_manager_.SetBackupKeepDailyDays(
      current_project_.workspace_settings.backup_keep_daily_days);

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

  RETURN_IF_ERROR(LoadAssetsForMode());

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
    project_management_panel_->SetVersionManager(version_manager_.get());
    project_management_panel_->SetRom(GetCurrentRom());
  }

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

  return current_project_.Save();
}

void EditorManager::ResolvePotItemSaveConfirmation(
    PotItemSaveDecision decision) {
  // Map EditorManager enum to RomLifecycleManager enum
  auto lifecycle_decision =
      static_cast<RomLifecycleManager::PotItemSaveDecision>(
          static_cast<int>(decision));
  rom_lifecycle_.ResolvePotItemSaveConfirmation(lifecycle_decision);

  if (decision == PotItemSaveDecision::kCancel) {
    toast_manager_.Show("Save cancelled", ToastType::kInfo);
    return;
  }

  auto status = SaveRom();
  if (status.ok()) {
    toast_manager_.Show("ROM saved successfully", ToastType::kSuccess);
  } else if (!absl::IsCancelled(status)) {
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

  // Ensure a project extension.
  if (!(absl::EndsWith(file_path, ".yaze") ||
        absl::EndsWith(file_path, ".yazeproj"))) {
    file_path += ".yaze";
  }

  // Update project filepath and save
  std::string old_filepath = current_project_.filepath;
  current_project_.filepath = file_path;

  auto save_status = current_project_.Save();
  if (save_status.ok()) {
    SyncLayoutScopeFromCurrentProject();

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
  current_project_ = project_manager_.GetCurrentProject();
  SyncLayoutScopeFromCurrentProject();
  RefreshHackWorkflowBackend();
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
      ResetAssetState(session);
    }
  }
  UpdateCurrentRomHash();
  return LoadAssetsForMode();
}

absl::Status EditorManager::PruneRomBackups() {
  return rom_lifecycle_.PruneRomBackups(GetCurrentRom());
}

// ConfirmRomWrite() and CancelRomWriteConfirm() are now inline in
// editor_manager.h delegating to rom_lifecycle_.

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
  if (session_coordinator_) {
    session_coordinator_->CloseCurrentSession();
  }
}

void EditorManager::RemoveSession(size_t index) {
  if (session_coordinator_) {
    session_coordinator_->RemoveSession(index);
  }
}

void EditorManager::SwitchToSession(size_t index) {
  if (session_coordinator_) {
    // Delegate to SessionCoordinator - cross-cutting concerns
    // are handled by OnSessionSwitched() observer callback
    session_coordinator_->SwitchToSession(index);
  }
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
                                          size_t session_id)
    : manager_(manager),
      prev_rom_(manager->GetCurrentRom()),
      prev_editor_set_(manager->GetCurrentEditorSet()),
      prev_session_id_(manager->GetCurrentSessionId()) {
  // Set new session context
  manager_->session_coordinator_->SwitchToSession(session_id);
}

EditorManager::SessionScope::~SessionScope() {
  // Restore previous context
  manager_->session_coordinator_->SwitchToSession(prev_session_id_);
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
  if (right_drawer_manager_) {
    // Update project panel context before showing
    if (project_management_panel_) {
      project_management_panel_->SetProject(&current_project_);
      project_management_panel_->SetVersionManager(version_manager_.get());
      project_management_panel_->SetRom(GetCurrentRom());
    }
    right_drawer_manager_->ToggleDrawer(
        RightDrawerManager::DrawerType::kProject);
  }
}

void EditorManager::ShowProjectFileEditor() {
  // Load the current project file into the editor
  if (!current_project_.filepath.empty()) {
    auto status = project_file_editor_.LoadFile(current_project_.filepath);
    if (!status.ok()) {
      toast_manager_.Show(
          absl::StrFormat("Failed to load project file: %s", status.message()),
          ToastType::kError);
      return;
    }
  }
  // Set the project pointer for label import functionality
  project_file_editor_.SetProject(&current_project_);
  // Activate the editor window
  project_file_editor_.set_active(true);
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
  deps.project = &current_project_;
  deps.version_manager = version_manager_.get();
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
