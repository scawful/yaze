// ROM/project I/O and its callback wiring. Method bodies are kept with the
// qualified editor implementation; editor_manager.cc owns the remaining shell.
#include "app/editor/editor_manager.h"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "imgui/imgui.h"

#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/editor.h"
#include "app/editor/graphics/screen_editor.h"
#include "app/editor/layout/layout_presets.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/session_types.h"
#include "app/editor/shell/coordinator/ui_coordinator.h"
#include "app/editor/shell/feedback/feature_hint_manager.h"
#include "app/editor/shell/feedback/popup_manager.h"
#include "app/editor/shell/feedback/toast_manager.h"
#include "app/editor/shell/windows/project_management_panel.h"
#include "app/editor/system/editor_registry.h"
#include "app/editor/system/project_workflow_status.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "app/gfx/util/palette_manager.h"
#include "app/platform/ios/ios_platform_state.h"
#include "app/platform/timing.h"
#include "core/features.h"
#include "core/native_save_receipt.h"
#include "core/project.h"
#include "core/rom_settings.h"
#include "core/source_artifact_publisher.h"
#include "editor/core/content_registry.h"
#include "rom/rom.h"
#include "rom/rom_diff.h"
#include "rom/transaction.h"
#include "util/file_util.h"
#include "util/json.h"
#include "util/log.h"
#include "util/macro.h"
#include "util/platform_paths.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_entrance.h"
#include "zelda3/overworld/overworld_version_helper.h"

#ifdef YAZE_ENABLE_TESTING
#include "app/testing/test_manager.h"
#endif

namespace yaze::editor {
namespace {
// Declared source files only. Build outputs and the next build request are
// consumers of the saved state, not inputs that a build may refresh in place.
std::map<std::string, std::string> ProjectSaveSources(
    const project::YazeProject& project) {
  std::map<std::string, std::string> sources;
  const auto add = [&](const std::string& role, const std::string& path) {
    if (!path.empty())
      sources.emplace(role, project.GetAbsolutePath(path));
  };
  for (size_t i = 0; i < project.additional_roms.size(); ++i)
    add("dependency.additional_roms." + std::to_string(i),
        project.additional_roms[i]);
  add("sidecar", project.lyra_terrain_file);
  add("dependency.labels_filename", project.labels_filename);
  add("dependency.symbols_filename", project.symbols_filename);
  add("dependency.cutscene_shots", project.cutscene_shots);
  add("dependency.overworld_graphics_preview",
      project.overworld_graphics_preview);
  add("dependency.dungeon_tile_patterns", project.dungeon_tile_patterns);
  add("dependency.custom_collision_json", project.custom_collision_json);
  add("dependency.hack_manifest_file", project.hack_manifest_file);
  add("dependency.sprite_catalog_file", project.sprite_catalog_file);
  add("dependency.terrain_catalog_file", project.terrain_catalog_file);
  add("dependency.palette_catalog_file", project.palette_catalog_file);
  // Planning is a live project reference, not a build input: its own CAS
  // save owns it, and agent status edits must not invalidate a native Save.
  add("dependency.atlas_symbols", project.atlas.symbols);
  for (const auto& [id, files] : project.custom_object_files) {
    for (size_t i = 0; i < files.size(); ++i) {
      const auto path = std::filesystem::path(files[i]);
      const auto resolved = path.is_absolute()
                                ? path
                                : std::filesystem::path(project.GetAbsolutePath(
                                      project.custom_objects_folder)) /
                                      path;
      add("dependency.object_" + std::to_string(id) + "_" + std::to_string(i),
          resolved.string());
    }
  }
  for (size_t i = 0; i < project.atlas.annotations.size(); ++i)
    add("dependency.atlas_" + std::to_string(i), project.atlas.annotations[i]);
  for (size_t i = 0; i < project.sprite_assets.size(); ++i) {
    const auto prefix = "dependency.sprite_" + std::to_string(i);
    const auto& asset = project.sprite_assets[i];
    add(prefix + "_zsm", asset.zsm_path);
    if (!asset.source_path.empty())
      add(prefix + "_source", (std::filesystem::path(project.GetAbsolutePath(
                                   project.sprite_source_root)) /
                               asset.source_path)
                                  .string());
    for (size_t slot = 0; slot < asset.graphics_sources.size(); ++slot) {
      const auto& source = asset.graphics_sources[slot];
      if (source.kind == "bin" || source.kind == "raw_rom")
        add(prefix + "_slot_" + std::to_string(slot), source.path);
    }
  }
  return sources;
}

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

}  // namespace

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
        flags.overworld.kSaveOverworldSprites = options.save_overworld_sprites;
        flags.overworld.kLoadCustomOverworld = options.enable_custom_overworld;
        flags.overworld.kEnableSpecialWorldExpansion =
            options.enable_tail_expansion;
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

        // Close dialog, then pick the post-load surface again: the options
        // may have created a project, but an open editor still wins.
        show_rom_load_options_ = false;
        ShowPostLoadSurface();

        LOG_INFO("EditorManager", "ROM load options applied: preset=%s",
                 options.selected_preset.c_str());
      });
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

absl::Status EditorManager::LoadRomInternal() {
  auto load_from_path = [this](const std::string& file_name) -> absl::Status {
    if (file_name.empty()) {
      return absl::OkStatus();
    }
    if (ActivateExistingSessionForOpen(file_name)) {
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
    RestoreCurrentRomIdentity();

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

absl::Status EditorManager::SaveRomInternal(
    const std::optional<std::string>& save_as_filename) {
  auto* current_rom = GetCurrentRom();
  auto* current_editor_set = GetCurrentEditorSet();
  if (!current_rom || !current_editor_set) {
    return absl::FailedPreconditionError("No ROM or editor set loaded");
  }
  if (auto* overworld = static_cast<OverworldEditor*>(
          current_editor_set->GetExistingEditor(EditorType::kOverworld));
      overworld && overworld->HasUnpublishedProjectGraphics()) {
    return absl::FailedPreconditionError(
        "Publish project graphics before Save ROM (Edit / Import Project "
        "Graphics)");
  }
  if (current_project_.rom_metadata.write_policy ==
      project::RomWritePolicy::kReadOnly) {
    RETURN_IF_ERROR(CheckRomWritePolicy(save_as_filename));
  }
  const auto lyra_layout = current_rom->InspectLyraLayout();
  const bool lyra_terrain_save =
      lyra_layout.presence == rom::LyraLayoutPresence::kValid &&
      lyra_layout.a0_terrain_write_supported;
  if (!lyra_terrain_save) {
    RETURN_IF_ERROR(current_rom->CheckLayoutWritable());
  } else if (!save_as_filename.has_value()) {
    return absl::FailedPreconditionError("Lyra A0 terrain requires Save As");
  }
  if (save_as_filename.has_value() && save_as_filename->empty()) {
    return absl::InvalidArgumentError("No filename provided for save as");
  }
  if (save_as_filename.has_value()) {
    RETURN_IF_ERROR(session_coordinator_->CheckBackingFileAvailable(
        *save_as_filename, GetCurrentSessionId()));
  }

  // Refuse excluded model edits before serializers, project rebinding, or disk
  // writes. Reuse the same preflight as direct Save / Apply Room.
  if (auto* dungeon = static_cast<DungeonEditorV2*>(
          current_editor_set->GetExistingEditor(EditorType::kDungeon))) {
    RETURN_IF_ERROR(dungeon->ValidateSaveScope());
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

  // GraphicsEditor tracks pixel edits in its model, not in the ROM buffer.
  // With Save Graphics Sheets enabled it joins the editor saves below; with
  // it disabled the edits cannot be written, so block before any serializer
  // can mutate the ROM rather than report a save that omitted the sheets.
  // Gfx group tables (blocksets, spritesets, palettesets) are edited in
  // GameData. Without Save Gfx Groups they cannot be written, so block before
  // any serializer runs instead of saving a ROM that silently drops them.
  auto* save_session = session_coordinator_->GetActiveRomSession();
  auto& palette_manager = gfx::PaletteManager::Get();
  const bool pending_palettes =
      save_session != nullptr &&
      palette_manager.HasUnsavedChanges(&save_session->game_data);
  // Both existing save settings authorize PaletteManager's modified colors.
  // Do not make palette-only persistence depend on opening Dungeon first.
  const bool palette_save_enabled =
      core::FeatureFlags::get().kSaveAllPalettes ||
      core::FeatureFlags::get().dungeon.kSavePalettes;
  if (pending_palettes && !palette_save_enabled) {
    return absl::FailedPreconditionError(
        "Save blocked: palette edits are pending, but palette saving is "
        "disabled. Enable Save All Palettes or Save Dungeon Palettes, or undo "
        "the palette edits before saving the ROM.");
  }
  const bool pending_gfx_groups =
      save_session != nullptr && save_session->HasPendingGfxGroupChanges();
  if (pending_gfx_groups && !core::FeatureFlags::get().kSaveGfxGroups) {
    return absl::FailedPreconditionError(
        "Save blocked: blockset/spriteset (gfx group) edits are pending, but "
        "Save Gfx Groups is disabled. Enable it, or undo the gfx group edits "
        "before saving the ROM.");
  }

  if (current_editor_set->HasPendingGraphicsChanges() &&
      !core::FeatureFlags::get().kSaveGraphicsSheet) {
    return absl::FailedPreconditionError(
        "Save blocked: graphics sheet edits are pending, but Save Graphics "
        "Sheets is disabled. Enable it, or discard the graphics sheet edits "
        "before saving the ROM.");
  }

  // Preflight map-only Screen edits before any serializer can mutate the ROM.
  // Retain the exact planned extents for manifest protection if disk diffing
  // is unavailable. Never materialize an unopened Screen editor here.
  auto* screen_editor = static_cast<ScreenEditor*>(
      current_editor_set->GetExistingEditor(EditorType::kScreen));
  std::vector<std::pair<uint32_t, uint32_t>> predicted_screen_write_ranges;
  if (screen_editor != nullptr && (screen_editor->IsRomBackedStateValid() ||
                                   screen_editor->HasPendingScreenChanges())) {
    ASSIGN_OR_RETURN(auto plan, screen_editor->PrepareDungeonMapSave());
    for (const auto& write : plan.writes) {
      predicted_screen_write_ranges.emplace_back(
          write.pc_address, write.pc_address + write.data.size());
    }
  }

  // --- State machine checks (delegated to RomLifecycleManager) ---
  if (rom_lifecycle_.IsRomWriteConfirmPending()) {
    return absl::CancelledError("Save pending confirmation");
  }

  RETURN_IF_ERROR(CheckRomWritePolicy(save_as_filename));

  if (lyra_terrain_save) {
    // This capability never enlists generic editor serializers. Refuse known
    // pending model domains rather than silently omit their edits.
    auto* dungeon = static_cast<DungeonEditorV2*>(
        current_editor_set->GetExistingEditor(EditorType::kDungeon));
    if (pending_palettes || pending_gfx_groups ||
        current_editor_set->HasPendingGraphicsChanges() ||
        current_editor_set->HasPendingScreenChanges() ||
        (dungeon && dungeon->HasPendingDungeonChanges())) {
      return absl::FailedPreconditionError(
          "Lyra Save As supports only A0 terrain; unrelated edits are pending");
    }
    auto* overworld = static_cast<OverworldEditor*>(
        current_editor_set->GetExistingEditor(EditorType::kOverworld));
    if (!overworld || !overworld->lyra_a0_terrain_writable()) {
      return absl::FailedPreconditionError(
          "Open Lyra A0 terrain before Save As");
    }
    RETURN_IF_ERROR(RetireNativeSaveTransfer("rebound"));
    RETURN_IF_ERROR(overworld->SaveLyraA0TerrainAs(*save_as_filename));
    CaptureCurrentRomIdentity(/*preserve_editing_source=*/true);
    if (auto* session = session_coordinator_->GetActiveRomSession()) {
      session->filepath = *save_as_filename;
      session->backup_restore_pending = false;
    }
    rom_lifecycle_.CancelRomWriteConfirm();
    auto& recent = project::RecentFilesManager::GetInstance();
    recent.AddFile(*save_as_filename);
    recent.Save();
    if (popup_manager_)
      popup_manager_->Hide(PopupID::kSaveAs);
    return RecordCompletedNativeSave(*save_project, *save_as_filename,
                                     /*save_as=*/true);
  }

  // Pot streams are saved sparsely: unchanged/unloaded rooms are inventoried
  // from ROM by the serializer. No load-all confirmation is necessary.

  // --- Backup policy setup ---
  const auto experiment_policy =
      current_editor_ != nullptr
          ? EditorRegistry::GetExperimentPolicy(current_editor_->type())
          : EditorExperimentPolicy{};
  const bool force_experimental_backup =
      !user_settings_.prefs().show_experimental_editors &&
      experiment_policy.experimental &&
      experiment_policy.disabled_save_posture ==
          ExperimentalSavePosture::kDefensiveBackup;
  if (force_experimental_backup) {
    toast_manager_.Show(
        "Experimental editor save: backup forced before writing the ROM",
        ToastType::kWarning);
  }
  if (save_project->project_opened()) {
    rom_lifecycle_.ApplyDefaultBackupPolicy(
        save_project->workspace_settings.backup_on_save ||
            force_experimental_backup,
        save_project->GetAbsolutePath(save_project->rom_backup_folder),
        save_project->workspace_settings.backup_retention_count,
        save_project->workspace_settings.backup_keep_daily,
        save_project->workspace_settings.backup_keep_daily_days);
  } else {
    rom_lifecycle_.ApplyDefaultBackupPolicy(
        user_settings_.prefs().backup_before_save || force_experimental_backup,
        "", 20, true, 14);
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
  // A failed lazy Screen load leaves an invalid, clean editor behind. It has
  // nothing to serialize and must not poison unrelated ROM saves.
  if (screen_editor != nullptr && (screen_editor->IsRomBackedStateValid() ||
                                   screen_editor->HasPendingScreenChanges())) {
    RETURN_IF_ERROR(save_editor(screen_editor));
  }
  // A coordinated save must not materialize an unrelated lazy editor. Its
  // serializer can project broad writes (and correctly trip manifest guards)
  // even though the user never opened or edited that domain.
  RETURN_IF_ERROR(
      save_editor(current_editor_set->GetExistingEditor(EditorType::kDungeon)));
  // Project context binding creates an Overworld editor before its model is
  // loaded. Its empty caches are not ROM edits and must not be serialized.
  if (auto* overworld = static_cast<OverworldEditor*>(
          current_editor_set->GetExistingEditor(EditorType::kOverworld));
      overworld && overworld->overworld().is_loaded()) {
    RETURN_IF_ERROR(save_editor(overworld));
  }

  // Dungeon may already have serialized palettes under its own transaction.
  // Otherwise enlist the Palette editor in the same file-save transaction.
  if (pending_palettes &&
      palette_manager.HasUnsavedChanges(&save_session->game_data)) {
    RETURN_IF_ERROR(EnsureEditorAssetsLoaded(EditorType::kPalette));
    RETURN_IF_ERROR(save_editor(
        current_editor_set->GetExistingEditor(EditorType::kPalette)));
  }

  if (core::FeatureFlags::get().kSaveMessages) {
    RETURN_IF_ERROR(EnsureEditorAssetsLoaded(EditorType::kMessage));
    RETURN_IF_ERROR(
        save_editor(current_editor_set->GetEditor(EditorType::kMessage)));
  }

  // Graphics sheets are written through zelda3::WriteGfxSheet (in place, or
  // relocated into manifest-registered free space) before the write-conflict
  // check below diffs the ROM against disk.
  if (core::FeatureFlags::get().kSaveGraphicsSheet) {
    RETURN_IF_ERROR(save_editor(
        current_editor_set->GetExistingEditor(EditorType::kGraphics)));
  }

  // Gfx group tables are written byte-for-byte where they differ, then read
  // back. The ROM transaction restores them if a later step fails.
  if (pending_gfx_groups && core::FeatureFlags::get().kSaveGfxGroups) {
    RETURN_IF_ERROR(
        zelda3::SaveGfxGroups(*current_rom, save_session->game_data));
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
        write_ranges.insert(write_ranges.end(),
                            predicted_screen_write_ranges.begin(),
                            predicted_screen_write_ranges.end());
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
  // A project's tracked custom_collision.json follows only the project's own
  // ROM file, never Save As targets or practice copies opened under it.
  std::filesystem::path collision_source;
  if (!save_as_filename.has_value() && save_project->project_opened() &&
      !save_project->custom_collision_json.empty() &&
      !save_project->rom_filename.empty()) {
    std::error_code same_ec;
    if (std::filesystem::equivalent(
            save_project->GetAbsolutePath(save_project->rom_filename),
            current_rom->filename(), same_ec)) {
      collision_source =
          save_project->GetAbsolutePath(save_project->custom_collision_json);
    }
  }
  // A same-byte save still supersedes the prior native operation. Retire its
  // external context before committing disk/baseline state, so publication or
  // write failure cannot leave the previous operation consumable as current.
  // Pending confirmation and serializer/conflict checks above do not retire it.
  RETURN_IF_ERROR(RetireNativeSaveTransfer("rebound"));
  auto save_status =
      save_as_filename.has_value()
          ? rom_file_manager_.SaveRomAs(current_rom, *save_as_filename)
          : rom_file_manager_.SaveRom(current_rom, collision_source);
  if (save_status.ok()) {
    editor_transactions.Commit();
    rom_transaction.Commit();
    CaptureCurrentRomIdentity(/*preserve_editing_source=*/true);
    if (session_coordinator_) {
      if (auto* session = session_coordinator_->GetActiveRomSession()) {
        session->backup_restore_pending = false;
      }
    }
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
    return RecordCompletedNativeSave(
        *save_project, save_as_filename.value_or(current_rom->filename()),
        save_as_filename.has_value());
  }
  return save_status;
}

absl::Status EditorManager::RecordCompletedNativeSave(
    const project::YazeProject& project, const std::string& saved_rom,
    bool save_as, bool rom_written) {
#if defined(__EMSCRIPTEN__) || !defined(YAZE_WITH_JSON)
  // Browser persistence has no native producer or durable publication lock.
  return absl::OkStatus();
#else
  core::NativeSaveReceiptRequest request;
  const auto finish = [&]() -> absl::Status {
    std::error_code error;
    const auto owner = std::filesystem::weakly_canonical(
        project.project_opened() ? project.filepath : saved_rom, error);
    if (error)
      return absl::FailedPreconditionError("Cannot resolve save receipt owner");
    request.receipt_path = owner.string() + ".native-save.json";
    request.operation = save_as ? "save_as" : "save";
    std::random_device random;
    for (int i = 0; i < 4; ++i)
      request.operation_id += absl::StrFormat("%08x", random());
    const auto* rom = GetCurrentRom();
    if (!rom)
      return absl::FailedPreconditionError("Saved ROM session is unavailable");
    const auto rom_path = std::filesystem::canonical(saved_rom, error);
    if (error)
      return absl::FailedPreconditionError("Cannot resolve saved ROM");
    if (rom_written) {
      request.artifacts.push_back(
          {"rom", rom_path, ComputeRomFileBytesSha256(*rom)});
    } else {
      // A project-only Save binds the ROM file only as this session loaded or
      // last wrote it; bytes changed by another writer are never blessed.
      const auto* session = session_coordinator_
                                ? session_coordinator_->GetActiveRomSession()
                                : nullptr;
      if (session == nullptr || !session->rom_baseline ||
          session->rom_baseline->backing_sha256.empty()) {
        return absl::FailedPreconditionError(
            "The ROM has no verified on-disk identity in this session; save "
            "the ROM first");
      }
      ASSIGN_OR_RETURN(const auto on_disk,
                       core::ComputeSourceArtifactFileSha256(rom_path));
      if (on_disk != session->rom_baseline->backing_sha256) {
        return absl::FailedPreconditionError(
            "The ROM file changed on disk since yaze loaded or last saved it; "
            "use Revert to Saved to load it, or save the ROM from yaze to "
            "replace it");
      }
      request.artifacts.push_back({"rom", rom_path, on_disk});
    }
    // Pin the process's expected producer once; the publisher still rehashes
    // it before publication and on readback for every completed save.
    static const auto producer =
        []() -> absl::StatusOr<std::pair<std::filesystem::path, std::string>> {
      ASSIGN_OR_RETURN(auto executable,
                       util::PlatformPaths::GetExecutablePath());
      ASSIGN_OR_RETURN(auto hash,
                       core::ComputeSourceArtifactFileSha256(executable));
      return std::make_pair(std::move(executable), std::move(hash));
    }();
    RETURN_IF_ERROR(producer.status());
    request.artifacts.push_back(
        {"producer", producer->first, producer->second});

    const auto bind = [&](const std::string& role, const std::string& path) {
      const auto digest = core::ComputeSourceArtifactFileSha256(path);
      if (!digest.ok()) {
        request.binding_errors.push_back(
            role + ": " + std::string(digest.status().message()));
      } else {
        request.artifacts.push_back({role, path, *digest});
      }
    };
    if (project.project_opened()) {
      bind("project", project.filepath);
      const auto live_sources = ProjectSaveSources(project);
      std::ifstream descriptor(project.filepath, std::ios::binary);
      if (descriptor) {
        const std::string bytes{std::istreambuf_iterator<char>(descriptor), {}};
        const auto binding_status = project.ValidateSavedSourceBindings(bytes);
        if (!binding_status.ok())
          request.binding_errors.push_back(
              std::string(binding_status.message()));
      }
      for (const auto& [role, path] : live_sources)
        bind(role, path);
      for (size_t i = 0; i < project.sprite_assets.size(); ++i) {
        const auto& asset = project.sprite_assets[i];
        if (asset.source_path.empty())
          continue;
        const auto role = "dependency.sprite_" + std::to_string(i) + "_source";
        const auto binding = std::find_if(
            request.artifacts.begin(), request.artifacts.end(),
            [&](const auto& artifact) { return artifact.role == role; });
        if (project.sprite_source_root.empty() ||
            (binding != request.artifacts.end() &&
             binding->expected_sha256 != asset.source_sha256))
          request.binding_errors.push_back(role + ": imported source is stale");
      }

#ifdef YAZE_WITH_JSON
      // Preview transfers are explicit external dependencies of the manifest.
      if (!project.overworld_graphics_preview.empty()) {
        try {
          const auto path =
              project.GetAbsolutePath(project.overworld_graphics_preview);
          std::ifstream input(path);
          const auto preview = Json::parse(input);
          size_t index = 0;
          for (const auto& area : preview.at("areas"))
            for (const auto& transfer : area.at("transfers"))
              bind("dependency.preview_transfer_" + std::to_string(index++),
                   (std::filesystem::path(path).parent_path() /
                    transfer.at("path").get<std::string>())
                       .string());
        } catch (const std::exception& exception) {
          request.binding_errors.push_back(
              std::string("Cannot bind graphics preview transfers: ") +
              exception.what());
        }
      }
#else
      if (!project.overworld_graphics_preview.empty())
        request.binding_errors.push_back(
            "Graphics preview dependency binding requires JSON support");
#endif
      if (!project.lyra_terrain_file.empty()) {
        auto* overworld = static_cast<OverworldEditor*>(
            GetCurrentEditorSet()->GetExistingEditor(EditorType::kOverworld));
        if (!overworld || !overworld->lyra_canvas_.loaded()) {
          request.binding_errors.push_back("Terrain document is not loaded");
        } else {
          auto context = project;
          const auto valid = overworld->lyra_canvas_.ValidateSave(
              &context, overworld->overworld());
          if (!valid.ok())
            request.binding_errors.push_back(std::string(valid.message()));
          std::ifstream input(
              project.GetAbsolutePath(project.lyra_terrain_file));
          const std::string bytes{std::istreambuf_iterator<char>(input), {}};
          const auto* live = overworld->lyra_canvas_.document();
          const auto saved =
              core::LyraTerrainDocument::Parse(bytes, live->profile());
          if (!saved.ok() || saved->Serialize() != live->Serialize())
            request.binding_errors.push_back(
                "Saved terrain differs from the live document");
        }
      }
    }
    // Keep completion recordable when a large or malformed project has many
    // unavailable optional inputs; never turn truncated evidence into complete.
    if (request.artifacts.size() > 256) {
      request.binding_errors.push_back(
          "More than 256 source bindings; incomplete");
      request.artifacts.resize(256);
    }
    if (request.binding_errors.size() > 256) {
      const auto omitted = request.binding_errors.size() - 255;
      request.binding_errors.resize(255);
      request.binding_errors.push_back(std::to_string(omitted) +
                                       " additional binding errors");
    }
    for (auto& diagnostic : request.binding_errors) {
      if (diagnostic.size() > 1024) {
        size_t end = 1000;
        while (end > 0 &&
               (static_cast<unsigned char>(diagnostic[end]) & 0xC0) == 0x80)
          --end;
        diagnostic = diagnostic.substr(0, end) + " [truncated]";
      }
    }
    const auto descriptor = std::find_if(
        request.artifacts.begin(), request.artifacts.end(),
        [](const auto& artifact) { return artifact.role == "project"; });
    const std::string project_path =
        descriptor == request.artifacts.end()
            ? ""
            : std::filesystem::absolute(descriptor->path)
                  .lexically_normal()
                  .string();
    ASSIGN_OR_RETURN(request.context,
                     core::GetNativeSaveContext(NativeSaveEditingSourcePath(),
                                                project_path));
    ASSIGN_OR_RETURN(auto receipt, core::PublishNativeSaveReceipt(request));
    if (!request.binding_errors.empty()) {
      toast_manager_.Show(std::string(rom_written ? "ROM" : "Project") +
                              " saved; build inputs are incomplete: " +
                              request.binding_errors.front(),
                          ToastType::kWarning);
    }
    return absl::OkStatus();
  };
  absl::Status status;
  try {
    status = finish();
  } catch (const std::exception& exception) {
    status = absl::InternalError(exception.what());
  }
  if (!status.ok()) {
    return absl::FailedPreconditionError(
        rom_written
            ? "ROM saved, but native-save receipt is unavailable: " +
                  std::string(status.message())
            : "Project saved, but its native-save receipt is unavailable; "
              "Save again to retry: " +
                  std::string(status.message()));
  }
  if (session_coordinator_) {
    if (auto* session = session_coordinator_->GetActiveRomSession()) {
      session->native_save_completion_pending = false;
      session->native_save_completion_save_as = false;
    }
  }
  return status;
#endif
}

std::string EditorManager::NativeSaveEditingSourcePath() const {
  if (session_coordinator_) {
    if (const auto* session = session_coordinator_->GetActiveRomSession();
        session && session->rom_baseline &&
        !session->rom_baseline->editing_source_path.empty()) {
      return session->rom_baseline->editing_source_path;
    }
  }
  const auto* rom = GetCurrentRom();
  return rom ? rom->filename() : "";
}

absl::Status EditorManager::RetireNativeSaveTransfer(
    const std::string& state, const std::string& editing_source_path) {
#if defined(__EMSCRIPTEN__) || !defined(YAZE_WITH_JSON)
  return absl::OkStatus();
#else
  const auto origin = editing_source_path.empty()
                          ? NativeSaveEditingSourcePath()
                          : editing_source_path;
  if (origin.empty())
    return absl::OkStatus();
  std::string project_path;
  std::error_code error;
  if (current_project_.project_opened() &&
      std::filesystem::is_regular_file(current_project_.filepath, error)) {
    project_path = std::filesystem::absolute(current_project_.filepath, error)
                       .lexically_normal()
                       .string();
  }
  if (error == std::errc::no_such_file_or_directory) {
    error.clear();
  }
  if (error) {
    return absl::FailedPreconditionError(
        "Cannot resolve native-save project context: " + error.message());
  }
  ASSIGN_OR_RETURN(auto context,
                   core::GetNativeSaveContext(origin, project_path));
  return core::RetireNativeSaveContext(context, state);
#endif
}

absl::Status EditorManager::LoadProjectWithRom(bool allow_dialogs) {
  const auto replaced_project_session_id =
      pending_project_open_transition_
          ? pending_project_open_previous_session_id_
          : std::nullopt;
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
    if (!allow_dialogs) {
      return absl::FailedPreconditionError("Project has no ROM configured");
    }
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
    if (!allow_dialogs) {
      abandon_selected_rom();
      return load_status;
    }
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
  RestoreCurrentRomIdentity();

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
    // Opening a project never creates a Git repository; the Initialize Git
    // action in Project Management is the opt-in. Adopt one that exists.
    version_manager_->AdoptExistingRepository();
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

  ShowPostLoadSurface();

  MaybeEmitZScreamRomLoadHints();

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

  // Async ROM selection can return before a project opens. Discard only at
  // completed adoption, and preserve drafts belonging to another session.
  if (replaced_project_session_id.has_value() &&
      GetCurrentSessionId() == *replaced_project_session_id) {
    if (auto* editors = GetCurrentEditorSet()) {
      editors->DiscardPlanningProjectDraft();
    }
  }
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

  // Lazy asset loading can defer OverworldEditor::Load after replacement.
  // Refuse a changed image while its read-only canvas still owns ROM-backed
  // maps and textures, before changing bytes, dirty state or undo history.
  if (auto* overworld_editor = static_cast<OverworldEditor*>(
          session->editors.GetExistingEditor(EditorType::kOverworld))) {
    RETURN_IF_ERROR(overworld_editor->CheckRomReplacement(rom));
  }

  // Reject invalid shared assets before invalidating live editor models and
  // their Undo histories. Rebuilding the old ROM on failure cannot recover
  // model-only edits. Keep this preflight CPU-only and leave lazy, unloaded
  // sessions lazy; editor-specific refresh failures still use rollback below.
  if (session->game_data_loaded || asset_load_mode_ != AssetLoadMode::kLazy) {
    zelda3::GameData replacement_data;
    zelda3::LoadOptions options;
    options.load_graphics = false;
    options.expand_rom = false;
    RETURN_IF_ERROR(zelda3::LoadGameData(rom, replacement_data, options));
  }

  Rom previous_rom = session->rom;
  const std::string previous_filepath = session->filepath;
  const bool game_data_was_loaded = session->game_data_loaded;
  auto* dungeon_editor = static_cast<DungeonEditorV2*>(
      session->editors.GetExistingEditor(EditorType::kDungeon));
  const size_t dungeon_index = EditorTypeIndex(EditorType::kDungeon);
  const bool dungeon_was_initialized =
      session->editor_initialized[dungeon_index];
  const bool dungeon_assets_were_loaded =
      session->editor_assets_loaded[dungeon_index];
  auto* screen_editor = static_cast<ScreenEditor*>(
      session->editors.GetExistingEditor(EditorType::kScreen));
  const size_t screen_index = EditorTypeIndex(EditorType::kScreen);
  const bool screen_was_initialized = session->editor_initialized[screen_index];
  const bool screen_assets_were_loaded =
      session->editor_assets_loaded[screen_index];
  auto restore_dungeon_asset_state = [&]() {
    if (dungeon_editor) {
      session->editor_initialized[dungeon_index] =
          session->editor_initialized[dungeon_index] || dungeon_was_initialized;
      session->editor_assets_loaded[dungeon_index] =
          session->editor_assets_loaded[dungeon_index] ||
          dungeon_assets_were_loaded;
    }
  };
  auto restore_screen_asset_state = [&]() {
    if (screen_editor) {
      session->editor_initialized[screen_index] =
          session->editor_initialized[screen_index] || screen_was_initialized;
      session->editor_assets_loaded[screen_index] =
          session->editor_assets_loaded[screen_index] ||
          screen_assets_were_loaded;
    }
  };

  session->editors.InvalidateScreenRomBackedState();
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
  // Retain the prior baseline until the replacement and its assets succeed.
  auto load_status = LoadAssetsForMode();
  if (load_status.ok() && game_data_was_loaded && !session->game_data_loaded) {
    load_status = EnsureGameDataLoaded();
  }
  if (load_status.ok() && screen_editor && screen_assets_were_loaded) {
    load_status = screen_editor->RefreshRomBackedState();
  }
  if (load_status.ok()) {
    restore_screen_asset_state();
  }
  if (load_status.ok() && dungeon_editor) {
    load_status = dungeon_editor->RefreshRomBackedState();
    if (load_status.ok()) {
      restore_dungeon_asset_state();
    }
  }
  if (!load_status.ok()) {
    // Loading editors is fallible. Restore the prior ROM and rebuild its
    // assets so a failed reload never leaves a half-initialized live session.
    session->editors.InvalidateScreenRomBackedState();
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
    RestoreCurrentRomIdentity();
    auto rollback_status = LoadAssetsForMode();
    if (game_data_was_loaded && !session->game_data_loaded) {
      auto game_data_status = EnsureGameDataLoaded();
      if (rollback_status.ok()) {
        rollback_status = game_data_status;
      }
    }
    if (screen_editor && screen_assets_were_loaded) {
      auto screen_status = screen_editor->RefreshRomBackedState();
      if (screen_status.ok()) {
        restore_screen_asset_state();
      }
      if (rollback_status.ok()) {
        rollback_status = screen_status;
      }
    } else {
      restore_screen_asset_state();
    }
    if (dungeon_editor) {
      auto dungeon_status = dungeon_editor->RefreshRomBackedState();
      if (dungeon_status.ok()) {
        restore_dungeon_asset_state();
      }
      if (rollback_status.ok()) {
        rollback_status = dungeon_status;
      }
    }
    ApplyCurrentProjectRuntimeContext();
    if (!rollback_status.ok()) {
      return absl::InternalError(absl::StrFormat(
          "ROM reload failed (%s) and restoring prior assets failed (%s)",
          load_status.message(), rollback_status.message()));
    }
    return load_status;
  }
  CaptureCurrentRomIdentity();
  ApplyCurrentProjectRuntimeContext();
  return absl::OkStatus();
}

void EditorManager::MaybeEmitZScreamRomLoadHints() {
  // Avoid a wall of onboarding toasts after load. Each subsequent load
  // advances to the next unseen, context-relevant tip.
  if (feature_hint_manager_.MaybeShow(FeatureHintId::kZScreamWelcome)) {
    return;
  }
  Rom* rom = GetCurrentRom();
  if (rom && rom->is_loaded()) {
    const auto version = zelda3::OverworldVersionHelper::GetVersion(*rom);
    if (version != zelda3::OverworldVersion::kVanilla &&
        feature_hint_manager_.MaybeShow(FeatureHintId::kZScreamZso)) {
      return;
    }
  }
  feature_hint_manager_.MaybeShow(FeatureHintId::kZScreamProjects);
}

absl::Status EditorManager::SaveCurrentSessionAs() {
  if (current_project_.project_opened()) {
    return SaveProjectAs();
  }
  if (!GetCurrentRom() || !GetCurrentRom()->is_loaded()) {
    return absl::FailedPreconditionError("No active ROM session");
  }
  popup_manager_->Show(PopupID::kSaveAs);
  return absl::OkStatus();
}

}  // namespace yaze::editor
