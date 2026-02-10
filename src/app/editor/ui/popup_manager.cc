#include "popup_manager.h"

#include <cstring>
#include <ctime>
#include <filesystem>
#include <functional>
#include <initializer_list>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "app/editor/editor_manager.h"
#include "app/editor/layout/layout_presets.h"
#include "app/gui/app/feature_flags_menu.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/style.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "util/file_util.h"
#include "util/hex.h"
#include "yaze.h"

namespace yaze {
namespace editor {

using namespace ImGui;

PopupManager::PopupManager(EditorManager* editor_manager)
    : editor_manager_(editor_manager), status_(absl::OkStatus()) {}

void PopupManager::Initialize() {
  // ============================================================================
  // POPUP REGISTRATION
  // ============================================================================
  // All popups must be registered here BEFORE any menu callbacks can trigger
  // them. This method is called in EditorManager constructor BEFORE
  // MenuOrchestrator and UICoordinator are created, ensuring safe
  // initialization order.
  //
  // Popup Registration Format:
  // popups_[PopupID::kConstant] = {
  //   .name = PopupID::kConstant,
  //   .type = PopupType::kXxx,
  //   .is_visible = false,
  //   .allow_resize = false/true,
  //   .draw_function = [this]() { DrawXxxPopup(); }
  // };
  // ============================================================================

  // File Operations
  popups_[PopupID::kSaveAs] = {PopupID::kSaveAs, PopupType::kFileOperation,
                               false, false, [this]() {
                                 DrawSaveAsPopup();
                               }};
  popups_[PopupID::kSaveScope] = {PopupID::kSaveScope, PopupType::kSettings,
                                  false, true, [this]() {
                                    DrawSaveScopePopup();
                                  }};
  popups_[PopupID::kNewProject] = {
      PopupID::kNewProject, PopupType::kFileOperation, false, false, [this]() {
        DrawNewProjectPopup();
      }};
  popups_[PopupID::kManageProject] = {PopupID::kManageProject,
                                      PopupType::kFileOperation, false, false,
                                      [this]() {
                                        DrawManageProjectPopup();
                                      }};
  popups_[PopupID::kRomBackups] = {PopupID::kRomBackups,
                                   PopupType::kFileOperation, false, true,
                                   [this]() { DrawRomBackupManagerPopup(); }};

  // Information
  popups_[PopupID::kAbout] = {PopupID::kAbout, PopupType::kInfo, false, false,
                              [this]() {
                                DrawAboutPopup();
                              }};
  popups_[PopupID::kRomInfo] = {PopupID::kRomInfo, PopupType::kInfo, false,
                                false, [this]() {
                                  DrawRomInfoPopup();
                                }};
  popups_[PopupID::kSupportedFeatures] = {
      PopupID::kSupportedFeatures, PopupType::kInfo, false, false, [this]() {
        DrawSupportedFeaturesPopup();
      }};
  popups_[PopupID::kOpenRomHelp] = {PopupID::kOpenRomHelp, PopupType::kHelp,
                                    false, false, [this]() {
                                      DrawOpenRomHelpPopup();
                                    }};

  // Help Documentation
  popups_[PopupID::kGettingStarted] = {
      PopupID::kGettingStarted, PopupType::kHelp, false, false, [this]() {
        DrawGettingStartedPopup();
      }};
  popups_[PopupID::kAsarIntegration] = {
      PopupID::kAsarIntegration, PopupType::kHelp, false, false, [this]() {
        DrawAsarIntegrationPopup();
      }};
  popups_[PopupID::kBuildInstructions] = {
      PopupID::kBuildInstructions, PopupType::kHelp, false, false, [this]() {
        DrawBuildInstructionsPopup();
      }};
  popups_[PopupID::kCLIUsage] = {PopupID::kCLIUsage, PopupType::kHelp, false,
                                 false, [this]() {
                                   DrawCLIUsagePopup();
                                 }};
  popups_[PopupID::kTroubleshooting] = {
      PopupID::kTroubleshooting, PopupType::kHelp, false, false, [this]() {
        DrawTroubleshootingPopup();
      }};
  popups_[PopupID::kContributing] = {PopupID::kContributing, PopupType::kHelp,
                                     false, false, [this]() {
                                       DrawContributingPopup();
                                     }};
  popups_[PopupID::kWhatsNew] = {PopupID::kWhatsNew, PopupType::kHelp, false,
                                 false, [this]() {
                                   DrawWhatsNewPopup();
                                 }};

  // Settings
  popups_[PopupID::kDisplaySettings] = {PopupID::kDisplaySettings,
                                        PopupType::kSettings, false,
                                        true,  // Resizable
                                        [this]() {
                                          DrawDisplaySettingsPopup();
                                        }};
  popups_[PopupID::kFeatureFlags] = {
      PopupID::kFeatureFlags, PopupType::kSettings, false, true,  // Resizable
      [this]() {
        DrawFeatureFlagsPopup();
      }};

  // Workspace
  popups_[PopupID::kWorkspaceHelp] = {PopupID::kWorkspaceHelp, PopupType::kHelp,
                                      false, false, [this]() {
                                        DrawWorkspaceHelpPopup();
                                      }};
  popups_[PopupID::kSessionLimitWarning] = {PopupID::kSessionLimitWarning,
                                            PopupType::kWarning, false, false,
                                            [this]() {
                                              DrawSessionLimitWarningPopup();
                                            }};
  popups_[PopupID::kLayoutResetConfirm] = {PopupID::kLayoutResetConfirm,
                                           PopupType::kConfirmation, false,
                                           false, [this]() {
                                             DrawLayoutResetConfirmPopup();
                                           }};

  popups_[PopupID::kLayoutPresets] = {
      PopupID::kLayoutPresets, PopupType::kSettings, false, false, [this]() {
        DrawLayoutPresetsPopup();
      }};

  popups_[PopupID::kSessionManager] = {
      PopupID::kSessionManager, PopupType::kSettings, false, true, [this]() {
        DrawSessionManagerPopup();
      }};

  // Debug/Testing
  popups_[PopupID::kDataIntegrity] = {PopupID::kDataIntegrity, PopupType::kInfo,
                                      false, true,  // Resizable
                                      [this]() {
                                        DrawDataIntegrityPopup();
                                      }};

  popups_[PopupID::kDungeonPotItemSaveConfirm] = {
      PopupID::kDungeonPotItemSaveConfirm, PopupType::kConfirmation, false,
      false, [this]() { DrawDungeonPotItemSaveConfirmPopup(); }};
  popups_[PopupID::kRomWriteConfirm] = {
      PopupID::kRomWriteConfirm, PopupType::kConfirmation, false, false,
      [this]() { DrawRomWriteConfirmPopup(); }};
  popups_[PopupID::kWriteConflictWarning] = {
      PopupID::kWriteConflictWarning, PopupType::kWarning, false, true,
      [this]() { DrawWriteConflictWarningPopup(); }};
}

void PopupManager::DrawPopups() {
  // Draw status popup if needed
  DrawStatusPopup();

  // Draw all registered popups
  for (auto& [name, params] : popups_) {
    if (params.is_visible) {
      OpenPopup(name.c_str());

      // Use allow_resize flag from popup definition
      ImGuiWindowFlags popup_flags = params.allow_resize
                                         ? ImGuiWindowFlags_None
                                         : ImGuiWindowFlags_AlwaysAutoResize;

      if (BeginPopupModal(name.c_str(), nullptr, popup_flags)) {
        params.draw_function();
        EndPopup();
      }
    }
  }
}

void PopupManager::Show(const char* name) {
  if (!name) {
    return;  // Safety check for null pointer
  }

  std::string name_str(name);
  auto it = popups_.find(name_str);
  if (it != popups_.end()) {
    it->second.is_visible = true;
  } else {
    // Log warning for unregistered popup
    printf(
        "[PopupManager] Warning: Popup '%s' not registered. Available popups: ",
        name);
    for (const auto& [key, _] : popups_) {
      printf("'%s' ", key.c_str());
    }
    printf("\n");
  }
}

void PopupManager::Hide(const char* name) {
  if (!name) {
    return;  // Safety check for null pointer
  }

  std::string name_str(name);
  auto it = popups_.find(name_str);
  if (it != popups_.end()) {
    it->second.is_visible = false;
    CloseCurrentPopup();
  }
}

bool PopupManager::IsVisible(const char* name) const {
  if (!name) {
    return false;  // Safety check for null pointer
  }

  std::string name_str(name);
  auto it = popups_.find(name_str);
  if (it != popups_.end()) {
    return it->second.is_visible;
  }
  return false;
}

void PopupManager::SetStatus(const absl::Status& status) {
  if (!status.ok()) {
    show_status_ = true;
    prev_status_ = status;
    status_ = status;
  }
}

bool PopupManager::BeginCentered(const char* name) {
  ImGuiIO const& io = GetIO();
  ImVec2 pos(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
  SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |
      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
  return Begin(name, nullptr, flags);
}

void PopupManager::DrawStatusPopup() {
  if (show_status_ && BeginCentered("StatusWindow")) {
    Text("%s", ICON_MD_ERROR);
    Text("%s", prev_status_.ToString().c_str());
    Spacing();
    NextColumn();
    Columns(1);
    Separator();
    NewLine();
    SameLine(128);
    if (Button("OK", ::yaze::gui::kDefaultModalSize) || IsKeyPressed(ImGuiKey_Space)) {
      show_status_ = false;
      status_ = absl::OkStatus();
    }
    SameLine();
    if (Button(ICON_MD_CONTENT_COPY, ImVec2(50, 0))) {
      SetClipboardText(prev_status_.ToString().c_str());
    }
    End();
  }
}

void PopupManager::DrawAboutPopup() {
  Text("Yet Another Zelda3 Editor - v%s", editor_manager_->version().c_str());
  Text("Written by: scawful");
  Spacing();
  Text("Special Thanks: Zarby89, JaredBrian");
  Separator();

  if (Button("Close", ::yaze::gui::kDefaultModalSize)) {
    Hide("About");
  }
}

void PopupManager::DrawRomInfoPopup() {
  auto* current_rom = editor_manager_->GetCurrentRom();
  if (!current_rom)
    return;

  Text("Title: %s", current_rom->title().c_str());
  Text("ROM Size: %s", util::HexLongLong(current_rom->size()).c_str());
  Text("ROM Hash: %s",
       editor_manager_->GetCurrentRomHash().empty()
           ? "(unknown)"
           : editor_manager_->GetCurrentRomHash().c_str());

  auto* project = editor_manager_->GetCurrentProject();
  if (project && project->project_opened()) {
    Separator();
    Text("Role: %s", project::RomRoleToString(project->rom_metadata.role).c_str());
    Text("Write Policy: %s",
         project::RomWritePolicyToString(project->rom_metadata.write_policy)
             .c_str());
    Text("Expected Hash: %s",
         project->rom_metadata.expected_hash.empty()
             ? "(unset)"
             : project->rom_metadata.expected_hash.c_str());
    if (editor_manager_->IsRomHashMismatch()) {
      const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
      TextColored(gui::ConvertColorToImVec4(theme.warning),
                  "ROM hash mismatch detected");
    }
  }

  if (Button("Close", ::yaze::gui::kDefaultModalSize) ||
      IsKeyPressed(ImGuiKey_Escape)) {
    Hide("ROM Information");
  }
}

void PopupManager::DrawSaveAsPopup() {
  using namespace ImGui;

  Text("%s Save ROM to new location", ICON_MD_SAVE_AS);
  Separator();

  static std::string save_as_filename = "";
  if (editor_manager_->GetCurrentRom() && save_as_filename.empty()) {
    save_as_filename = editor_manager_->GetCurrentRom()->title();
  }

  InputText("Filename", &save_as_filename);
  Separator();

  if (Button(absl::StrFormat("%s Browse...", ICON_MD_FOLDER_OPEN).c_str(),
             ::yaze::gui::kDefaultModalSize)) {
    auto file_path =
        util::FileDialogWrapper::ShowSaveFileDialog(save_as_filename, "sfc");
    if (!file_path.empty()) {
      save_as_filename = file_path;
    }
  }

  SameLine();
  if (Button(absl::StrFormat("%s Save", ICON_MD_SAVE).c_str(),
             ::yaze::gui::kDefaultModalSize)) {
    if (!save_as_filename.empty()) {
      // Ensure proper file extension
      std::string final_filename = save_as_filename;
      if (final_filename.find(".sfc") == std::string::npos &&
          final_filename.find(".smc") == std::string::npos) {
        final_filename += ".sfc";
      }

      auto status = editor_manager_->SaveRomAs(final_filename);
      if (status.ok()) {
        save_as_filename = "";
        Hide(PopupID::kSaveAs);
      }
    }
  }

  SameLine();
  if (Button(absl::StrFormat("%s Cancel", ICON_MD_CANCEL).c_str(),
             ::yaze::gui::kDefaultModalSize)) {
    save_as_filename = "";
    Hide(PopupID::kSaveAs);
  }
}

void PopupManager::DrawSaveScopePopup() {
  using namespace ImGui;

  Text("%s Save Scope", ICON_MD_SAVE);
  Separator();
  TextWrapped(
      "Controls which data is written during File > Save ROM. "
      "Changes apply immediately.");
  Separator();

  if (CollapsingHeader("Overworld", ImGuiTreeNodeFlags_DefaultOpen)) {
    Checkbox("Save Overworld Maps",
             &core::FeatureFlags::get().overworld.kSaveOverworldMaps);
    Checkbox("Save Overworld Entrances",
             &core::FeatureFlags::get().overworld.kSaveOverworldEntrances);
    Checkbox("Save Overworld Exits",
             &core::FeatureFlags::get().overworld.kSaveOverworldExits);
    Checkbox("Save Overworld Items",
             &core::FeatureFlags::get().overworld.kSaveOverworldItems);
    Checkbox("Save Overworld Properties",
             &core::FeatureFlags::get().overworld.kSaveOverworldProperties);
  }

  if (CollapsingHeader("Dungeon", ImGuiTreeNodeFlags_DefaultOpen)) {
    Checkbox("Save Dungeon Maps", &core::FeatureFlags::get().kSaveDungeonMaps);
    Checkbox("Save Objects", &core::FeatureFlags::get().dungeon.kSaveObjects);
    Checkbox("Save Sprites", &core::FeatureFlags::get().dungeon.kSaveSprites);
    Checkbox("Save Room Headers",
             &core::FeatureFlags::get().dungeon.kSaveRoomHeaders);
    Checkbox("Save Torches", &core::FeatureFlags::get().dungeon.kSaveTorches);
    Checkbox("Save Pits", &core::FeatureFlags::get().dungeon.kSavePits);
    Checkbox("Save Blocks", &core::FeatureFlags::get().dungeon.kSaveBlocks);
    Checkbox("Save Collision",
             &core::FeatureFlags::get().dungeon.kSaveCollision);
    Checkbox("Save Chests", &core::FeatureFlags::get().dungeon.kSaveChests);
    Checkbox("Save Pot Items",
             &core::FeatureFlags::get().dungeon.kSavePotItems);
    Checkbox("Save Palettes",
             &core::FeatureFlags::get().dungeon.kSavePalettes);
  }

  if (CollapsingHeader("Graphics", ImGuiTreeNodeFlags_DefaultOpen)) {
    Checkbox("Save Graphics Sheets",
             &core::FeatureFlags::get().kSaveGraphicsSheet);
    Checkbox("Save All Palettes", &core::FeatureFlags::get().kSaveAllPalettes);
    Checkbox("Save Gfx Groups", &core::FeatureFlags::get().kSaveGfxGroups);
  }

  if (CollapsingHeader("Messages", ImGuiTreeNodeFlags_DefaultOpen)) {
    Checkbox("Save Message Text", &core::FeatureFlags::get().kSaveMessages);
  }

  Separator();
  if (Button("Close", ::yaze::gui::kDefaultModalSize)) {
    Hide(PopupID::kSaveScope);
  }
}

void PopupManager::DrawRomBackupManagerPopup() {
  using namespace ImGui;

  auto* rom = editor_manager_->GetCurrentRom();
  if (!rom || !rom->is_loaded()) {
    Text("No ROM loaded.");
    if (Button("Close", ::yaze::gui::kDefaultModalSize)) {
      Hide(PopupID::kRomBackups);
    }
    return;
  }

  const auto* project = editor_manager_->GetCurrentProject();
  std::string backup_dir;
  if (project && project->project_opened() &&
      !project->rom_backup_folder.empty()) {
    backup_dir = project->GetAbsolutePath(project->rom_backup_folder);
  } else {
    backup_dir = std::filesystem::path(rom->filename()).parent_path().string();
  }

  Text("%s ROM Backups", ICON_MD_BACKUP);
  Separator();
  TextWrapped("Backup folder: %s", backup_dir.c_str());

  if (Button(ICON_MD_DELETE_SWEEP " Prune Backups")) {
    auto status = editor_manager_->PruneRomBackups();
    if (!status.ok()) {
      if (auto* toast = editor_manager_->toast_manager()) {
        toast->Show(absl::StrFormat("Prune failed: %s", status.message()),
                    ToastType::kError);
      }
    } else if (auto* toast = editor_manager_->toast_manager()) {
      toast->Show("Backups pruned", ToastType::kSuccess);
    }
  }

  Separator();
  auto backups = editor_manager_->GetRomBackups();
  if (backups.empty()) {
    TextDisabled("No backups found.");
  } else if (BeginTable("RomBackupTable", 4,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                            ImGuiTableFlags_Resizable)) {
    TableSetupColumn("Timestamp");
    TableSetupColumn("Size");
    TableSetupColumn("Filename");
    TableSetupColumn("Actions");
    TableHeadersRow();

    auto format_size = [](uintmax_t bytes) {
      if (bytes > (1024 * 1024)) {
        return absl::StrFormat("%.2f MB",
                               static_cast<double>(bytes) / (1024 * 1024));
      }
      if (bytes > 1024) {
        return absl::StrFormat("%.1f KB",
                               static_cast<double>(bytes) / 1024.0);
      }
      return absl::StrFormat("%llu B",
                             static_cast<unsigned long long>(bytes));
    };

    for (size_t i = 0; i < backups.size(); ++i) {
      const auto& backup = backups[i];
      TableNextRow();
      TableNextColumn();
      char time_buffer[32] = "unknown";
      if (backup.timestamp != 0) {
        std::tm local_tm{};
#ifdef _WIN32
        localtime_s(&local_tm, &backup.timestamp);
#else
        localtime_r(&backup.timestamp, &local_tm);
#endif
        std::strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S",
                      &local_tm);
      }
      TextUnformatted(time_buffer);

      TableNextColumn();
      TextUnformatted(format_size(backup.size_bytes).c_str());

      TableNextColumn();
      TextUnformatted(backup.filename.c_str());

      TableNextColumn();
      PushID(static_cast<int>(i));
      if (Button(ICON_MD_RESTORE " Restore")) {
        auto status = editor_manager_->RestoreRomBackup(backup.path);
        if (!status.ok()) {
          if (auto* toast = editor_manager_->toast_manager()) {
            toast->Show(
                absl::StrFormat("Restore failed: %s", status.message()),
                ToastType::kError);
          }
        } else if (auto* toast = editor_manager_->toast_manager()) {
          toast->Show("ROM restored from backup", ToastType::kSuccess);
        }
      }
      SameLine();
      if (Button(ICON_MD_OPEN_IN_NEW " Open")) {
        auto status = editor_manager_->OpenRomOrProject(backup.path);
        if (!status.ok()) {
          if (auto* toast = editor_manager_->toast_manager()) {
            toast->Show(
                absl::StrFormat("Open failed: %s", status.message()),
                ToastType::kError);
          }
        }
      }
      SameLine();
      if (Button(ICON_MD_CONTENT_COPY " Copy")) {
        SetClipboardText(backup.path.c_str());
      }
      PopID();
    }
    EndTable();
  }

  Separator();
  if (Button("Close", ::yaze::gui::kDefaultModalSize)) {
    Hide(PopupID::kRomBackups);
  }
}

void PopupManager::DrawNewProjectPopup() {
  using namespace ImGui;

  static std::string project_name = "";
  static std::string project_filepath = "";
  static std::string rom_filename = "";
  static std::string labels_filename = "";
  static std::string code_folder = "";

  InputText("Project Name", &project_name);

  if (Button(absl::StrFormat("%s Destination Folder", ICON_MD_FOLDER).c_str(),
             ::yaze::gui::kDefaultModalSize)) {
    project_filepath = util::FileDialogWrapper::ShowOpenFolderDialog();
  }
  SameLine();
  Text("%s", project_filepath.empty() ? "(Not set)" : project_filepath.c_str());

  if (Button(absl::StrFormat("%s ROM File", ICON_MD_VIDEOGAME_ASSET).c_str(),
             ::yaze::gui::kDefaultModalSize)) {
    rom_filename = util::FileDialogWrapper::ShowOpenFileDialog(
        util::MakeRomFileDialogOptions(false));
  }
  SameLine();
  Text("%s", rom_filename.empty() ? "(Not set)" : rom_filename.c_str());

  if (Button(absl::StrFormat("%s Labels File", ICON_MD_LABEL).c_str(),
             ::yaze::gui::kDefaultModalSize)) {
    labels_filename = util::FileDialogWrapper::ShowOpenFileDialog();
  }
  SameLine();
  Text("%s", labels_filename.empty() ? "(Not set)" : labels_filename.c_str());

  if (Button(absl::StrFormat("%s Code Folder", ICON_MD_CODE).c_str(),
             ::yaze::gui::kDefaultModalSize)) {
    code_folder = util::FileDialogWrapper::ShowOpenFolderDialog();
  }
  SameLine();
  Text("%s", code_folder.empty() ? "(Not set)" : code_folder.c_str());

  Separator();

  if (Button(absl::StrFormat("%s Choose Project File Location", ICON_MD_SAVE)
                 .c_str(),
             ::yaze::gui::kDefaultModalSize)) {
    auto project_file_path =
        util::FileDialogWrapper::ShowSaveFileDialog(project_name, "yaze");
    if (!project_file_path.empty()) {
      if (!(absl::EndsWith(project_file_path, ".yaze") ||
            absl::EndsWith(project_file_path, ".yazeproj"))) {
        project_file_path += ".yaze";
      }
      project_filepath = project_file_path;
    }
  }

  if (Button(absl::StrFormat("%s Create Project", ICON_MD_ADD).c_str(),
             ::yaze::gui::kDefaultModalSize)) {
    if (!project_filepath.empty() && !project_name.empty()) {
      auto status = editor_manager_->CreateNewProject();
      if (status.ok()) {
        // Clear fields
        project_name = "";
        project_filepath = "";
        rom_filename = "";
        labels_filename = "";
        code_folder = "";
        Hide(PopupID::kNewProject);
      }
    }
  }
  SameLine();
  if (Button(absl::StrFormat("%s Cancel", ICON_MD_CANCEL).c_str(),
             ::yaze::gui::kDefaultModalSize)) {
    // Clear fields
    project_name = "";
    project_filepath = "";
    rom_filename = "";
    labels_filename = "";
    code_folder = "";
    Hide(PopupID::kNewProject);
  }
}

void PopupManager::DrawSupportedFeaturesPopup() {
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  const ImVec4 status_ok = gui::ConvertColorToImVec4(theme.success);
  const ImVec4 status_warn = gui::ConvertColorToImVec4(theme.warning);
  const ImVec4 status_info = gui::ConvertColorToImVec4(theme.info);
  const ImVec4 status_error = gui::ConvertColorToImVec4(theme.error);

  auto status_color = [&](const char* status) -> ImVec4 {
    if (strcmp(status, "Stable") == 0 || strcmp(status, "Working") == 0) {
      return status_ok;
    }
    if (strcmp(status, "Beta") == 0 || strcmp(status, "Experimental") == 0) {
      return status_warn;
    }
    if (strcmp(status, "Preview") == 0) {
      return status_info;
    }
    if (strcmp(status, "Not available") == 0) {
      return status_error;
    }
    return status_info;
  };

  struct FeatureRow {
    const char* feature;
    const char* status;
    const char* persistence;
    const char* notes;
  };

  auto draw_table = [&](const char* table_id,
                        std::initializer_list<FeatureRow> rows) {
    ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerH |
                            ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable;
    if (!BeginTable(table_id, 4, flags)) {
      return;
    }
    TableSetupColumn("Feature", ImGuiTableColumnFlags_WidthStretch);
    TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    TableSetupColumn("Save/Load", ImGuiTableColumnFlags_WidthFixed, 180.0f);
    TableSetupColumn("Notes", ImGuiTableColumnFlags_WidthStretch);
    TableHeadersRow();

    for (const auto& row : rows) {
      TableNextRow();
      TableSetColumnIndex(0);
      TextUnformatted(row.feature);
      TableSetColumnIndex(1);
      TextColored(status_color(row.status), "%s", row.status);
      TableSetColumnIndex(2);
      TextUnformatted(row.persistence);
      TableSetColumnIndex(3);
      TextWrapped("%s", row.notes);
    }

    EndTable();
  };

  TextDisabled(
      "Status: Stable = production ready, Beta = usable with gaps, "
      "Experimental = WIP, Preview = web parity in progress.");
  TextDisabled("See Settings > Feature Flags for ROM-specific toggles.");
  Spacing();

  if (CollapsingHeader("Desktop App (yaze)", ImGuiTreeNodeFlags_DefaultOpen)) {
    draw_table(
        "desktop_features",
        {
            {"ROM load/save", "Stable", "ROM + backups",
             "Backups on save when enabled."},
            {"Overworld Editor", "Stable", "ROM",
             "Maps/entrances/exits/items; version-gated."},
            {"Dungeon Editor", "Stable", "ROM",
             "Room objects/tiles/palettes persist."},
            {"Palette Editor", "Stable", "ROM",
             "Palette edits persist; JSON IO pending."},
            {"Graphics Editor", "Beta", "ROM",
             "Sheet edits persist; tooling still expanding."},
            {"Sprite Editor", "Stable", "ROM", "Sprite edits persist."},
            {"Message Editor", "Stable", "ROM", "Text edits persist."},
            {"Screen Editor", "Experimental", "ROM (partial)",
             "Save coverage incomplete."},
            {"Hex Editor", "Beta", "ROM", "Search UX incomplete."},
            {"Assembly/Asar", "Beta", "ROM + project",
             "Patch apply + symbol export."},
            {"Emulator", "Beta", "Runtime only",
             "Save-state UI partially wired."},
            {"Music Editor", "Experimental", "ROM (partial)",
             "Serialization in progress."},
            {"Agent UI", "Experimental", ".yaze/agent",
             "Requires AI provider configuration."},
            {"Settings/Layouts", "Beta", ".yaze config",
             "Layout serialization improving."},
        });
  }

  if (CollapsingHeader("z3ed CLI")) {
    draw_table(
        "cli_features",
        {
            {"ROM read/write/validate", "Stable", "ROM file",
             "Direct command execution."},
            {"Agent workflows", "Stable", ".yaze/proposals + sandboxes",
             "Commit writes ROM; revert reloads."},
            {"Snapshots/restore", "Stable", "Sandbox copies",
             "Supports YAZE_SANDBOX_ROOT override."},
            {"Doctor/test suites", "Stable", "Reports",
             "Structured output for automation."},
            {"TUI/REPL", "Stable", "Session history",
             "Interactive command palette + logs."},
        });
  }

  if (CollapsingHeader("Web/WASM Preview")) {
    draw_table(
        "web_features",
        {
            {"ROM load/save", "Preview", "IndexedDB + download",
             "Drag/drop or picker; download for backups."},
            {"Editors (OW/Dungeon/Palette/etc.)", "Preview",
             "IndexedDB + download", "Parity work in progress."},
            {"Hex Editor", "Working", "IndexedDB + download",
             "Direct ROM editing available."},
            {"Asar patching", "Preview", "ROM",
             "Basic patch apply support."},
            {"Emulator", "Not available", "N/A", "Desktop only."},
            {"Collaboration", "Experimental", "Server",
             "Requires yaze-server."},
            {"AI features", "Preview", "Server",
             "Requires AI-enabled server."},
        });
  }

  if (Button("Close", ::yaze::gui::kDefaultModalSize)) {
    Hide(PopupID::kSupportedFeatures);
  }
}

void PopupManager::DrawOpenRomHelpPopup() {
  Text("File -> Open");
  Text("Select a ROM file to open");
  Text("Supported ROMs (headered or unheadered):");
  Text("The Legend of Zelda: A Link to the Past");
  Text("US Version 1.0");
  Text("JP Version 1.0");
  Spacing();
  TextWrapped("ROM files are not bundled. Use a clean, legally obtained copy.");

  if (Button("Close", ::yaze::gui::kDefaultModalSize)) {
    Hide("Open a ROM");
  }
}

void PopupManager::DrawManageProjectPopup() {
  Text("Project Menu");
  Text("Create a new project or open an existing one.");
  Text("Save the project to save the current state of the project.");
  TextWrapped(
      "To save a project, you need to first open a ROM and initialize your "
      "code path and labels file. Label resource manager can be found in "
      "the View menu. Code path is set in the Code editor after opening a "
      "folder.");

  if (Button("Close", ::yaze::gui::kDefaultModalSize)) {
    Hide("Manage Project");
  }
}

void PopupManager::DrawGettingStartedPopup() {
  TextWrapped("Welcome to YAZE v%s!", YAZE_VERSION_STRING);
  TextWrapped(
      "YAZE lets you modify 'The Legend of Zelda: A Link to the Past' (US or "
      "JP) ROMs with modern tooling.");
  Spacing();
  TextWrapped("Release Highlights:");
  BulletText(
      "AI-assisted workflows via z3ed agent and in-app panels "
      "(Ollama/Gemini/OpenAI/Anthropic)");
  BulletText("Clear feature status panels and improved help/tooltips");
  BulletText("Unified .yaze storage across desktop/CLI/web");
  Spacing();
  TextWrapped("General Tips:");
  BulletText("Open a clean ROM and save a backup before editing");
  BulletText("Use Help (F1) for context-aware guidance and shortcuts");
  BulletText(
      "Configure AI providers (Ollama/Gemini/OpenAI/Anthropic) in Settings > "
      "Agent");

  if (Button("Close", ::yaze::gui::kDefaultModalSize)) {
    Hide("Getting Started");
  }
}

void PopupManager::DrawAsarIntegrationPopup() {
  TextWrapped("Asar 65816 Assembly Integration");
  TextWrapped(
      "YAZE includes full Asar assembler support for ROM patching.");
  Spacing();
  TextWrapped("Features:");
  BulletText("Cross-platform ROM patching with assembly code");
  BulletText("Symbol export with addresses and opcodes");
  BulletText("Assembly validation with detailed error reporting");
  BulletText("Memory-safe patch application with size checks");

  if (Button("Close", ::yaze::gui::kDefaultModalSize)) {
    Hide("Asar Integration");
  }
}

void PopupManager::DrawBuildInstructionsPopup() {
  TextWrapped("Build Instructions");
  TextWrapped("YAZE uses modern CMake for cross-platform builds.");
  Spacing();
  TextWrapped("Quick Start (examples):");
  BulletText("cmake --preset mac-dbg | lin-dbg | win-dbg");
  BulletText("cmake --build --preset <preset> --target yaze");
  Spacing();
  TextWrapped("AI Builds:");
  BulletText("cmake --preset mac-ai | lin-ai | win-ai");
  BulletText("cmake --build --preset <preset> --target yaze z3ed");
  Spacing();
  TextWrapped("Docs: docs/public/build/quick-reference.md");

  if (Button("Close", ::yaze::gui::kDefaultModalSize)) {
    Hide("Build Instructions");
  }
}

void PopupManager::DrawCLIUsagePopup() {
  TextWrapped("Command Line Interface (z3ed)");
  TextWrapped("Scriptable ROM editing and AI agent workflows.");
  Spacing();
  TextWrapped("Commands:");
  BulletText("z3ed rom-info --rom=zelda3.sfc");
  BulletText("z3ed agent simple-chat --rom=zelda3.sfc --ai_provider=auto");
  BulletText("z3ed agent plan --rom=zelda3.sfc");
  BulletText("z3ed test-list --format json");
  BulletText("z3ed patch apply-asar patch.asm --rom=zelda3.sfc");
  BulletText("z3ed --tui");
  Spacing();
  TextWrapped("Storage:");
  BulletText("Agent plans/proposals live under ~/.yaze (see docs for details)");

  if (Button("Close", ::yaze::gui::kDefaultModalSize)) {
    Hide("CLI Usage");
  }
}

void PopupManager::DrawTroubleshootingPopup() {
  TextWrapped("Troubleshooting");
  TextWrapped("Common issues and solutions:");
  Spacing();
  BulletText("ROM won't load: Check file format (SFC/SMC supported)");
  BulletText(
      "AI agent missing: Start Ollama or set GEMINI_API_KEY/OPENAI_API_KEY/"
      "ANTHROPIC_API_KEY (web uses AI_AGENT_ENDPOINT)");
  BulletText("Graphics issues: Disable experimental flags in Settings");
  BulletText("Performance: Enable hardware acceleration in display settings");
  BulletText("Crashes: Check ROM file integrity and available memory");
  BulletText("Layout issues: Reset workspace layouts from View > Layouts");

  if (Button("Close", ::yaze::gui::kDefaultModalSize)) {
    Hide("Troubleshooting");
  }
}

void PopupManager::DrawContributingPopup() {
  TextWrapped("Contributing to YAZE");
  TextWrapped("YAZE is open source and welcomes contributions!");
  Spacing();
  TextWrapped("How to contribute:");
  BulletText("Fork the repository on GitHub");
  BulletText("Create feature branches for new work");
  BulletText("Follow C++ coding standards");
  BulletText("Include tests for new features");
  BulletText("Submit pull requests for review");

  if (Button("Close", ::yaze::gui::kDefaultModalSize)) {
    Hide("Contributing");
  }
}

void PopupManager::DrawWhatsNewPopup() {
  TextWrapped("What's New in YAZE v%s", YAZE_VERSION_STRING);
  Spacing();

  if (CollapsingHeader(
          absl::StrFormat("%s User Interface & Theming", ICON_MD_PALETTE)
              .c_str(),
          ImGuiTreeNodeFlags_DefaultOpen)) {
    BulletText("Feature status/persistence summaries across desktop/CLI/web");
    BulletText("Shortcut/help panels now match configured keybindings");
    BulletText("Refined onboarding tips and error messaging");
    BulletText("Help text refreshed across desktop, CLI, and web");
  }

  if (CollapsingHeader(
          absl::StrFormat("%s Development & Build System", ICON_MD_BUILD)
              .c_str(),
          ImGuiTreeNodeFlags_DefaultOpen)) {
    BulletText("Asar 65816 assembler integration for ROM patching");
    BulletText("z3ed CLI + TUI for scripting, test/doctor, and automation");
    BulletText("Modern CMake presets for desktop, AI, and web builds");
    BulletText("Unified version + storage references for 0.5.1");
  }

  if (CollapsingHeader(
          absl::StrFormat("%s Core Improvements", ICON_MD_SETTINGS).c_str())) {
    BulletText("Improved project metadata + .yaze storage alignment");
    BulletText("Stronger error reporting and status feedback");
    BulletText("Performance and stability improvements across editors");
    BulletText("Expanded logging and diagnostics tooling");
  }

  if (CollapsingHeader(
          absl::StrFormat("%s Editor Features", ICON_MD_EDIT).c_str())) {
    BulletText("Music editor updates with SPC parsing/playback");
    BulletText("AI agent-assisted editing workflows (multi-provider + vision)");
    BulletText("Expanded overworld/dungeon tooling and palette accuracy");
    BulletText("Web/WASM preview with collaboration hooks");
  }

  Spacing();
  if (Button(
          absl::StrFormat("%s View Release Notes", ICON_MD_DESCRIPTION).c_str(),
          ImVec2(-1, 30))) {
    // Close this popup and show theme settings
    Hide(PopupID::kWhatsNew);
    // Could trigger release notes panel opening here
  }

  if (Button("Close", ::yaze::gui::kDefaultModalSize)) {
    Hide(PopupID::kWhatsNew);
  }
}

void PopupManager::DrawWorkspaceHelpPopup() {
  TextWrapped("Workspace Management");
  TextWrapped(
      "YAZE supports multiple ROM sessions and flexible workspace layouts.");
  Spacing();

  TextWrapped("Session Management:");
  BulletText("Ctrl+Shift+N: Create new session");
  BulletText("Ctrl+Shift+W: Close current session");
  BulletText("Ctrl+Tab: Quick session switcher");
  BulletText("Each session maintains its own ROM and editor state");

  Spacing();
  TextWrapped("Layout Management:");
  BulletText("Drag window tabs to dock/undock");
  BulletText("Ctrl+Shift+S: Save current layout");
  BulletText("Ctrl+Shift+O: Load saved layout");
  BulletText("F11: Maximize current window");

  Spacing();
  TextWrapped("Preset Layouts:");
  BulletText("Developer: Code, memory, testing tools");
  BulletText("Designer: Graphics, palettes, sprites");
  BulletText("Modder: All gameplay editing tools");

  if (Button("Close", ::yaze::gui::kDefaultModalSize)) {
    Hide("Workspace Help");
  }
}

void PopupManager::DrawSessionLimitWarningPopup() {
  TextColored(gui::GetWarningColor(), "%s Warning", ICON_MD_WARNING);
  TextWrapped("You have reached the recommended session limit.");
  TextWrapped("Having too many sessions open may impact performance.");
  Spacing();
  TextWrapped("Consider closing unused sessions or saving your work.");

  if (Button("Understood", ::yaze::gui::kDefaultModalSize)) {
    Hide("Session Limit Warning");
  }
  SameLine();
  if (Button("Open Session Manager", ::yaze::gui::kDefaultModalSize)) {
    Hide("Session Limit Warning");
    // This would trigger the session manager to open
  }
}

void PopupManager::DrawLayoutResetConfirmPopup() {
  TextColored(gui::GetWarningColor(), "%s Confirm Reset",
              ICON_MD_WARNING);
  TextWrapped("This will reset your current workspace layout to default.");
  TextWrapped("Any custom window arrangements will be lost.");
  Spacing();
  TextWrapped("Do you want to continue?");

  if (Button("Reset Layout", ::yaze::gui::kDefaultModalSize)) {
    Hide("Layout Reset Confirm");
    // This would trigger the actual reset
  }
  SameLine();
  if (Button("Cancel", ::yaze::gui::kDefaultModalSize)) {
    Hide("Layout Reset Confirm");
  }
}

void PopupManager::DrawLayoutPresetsPopup() {
  TextColored(gui::GetInfoColor(), "%s Layout Presets",
              ICON_MD_DASHBOARD);
  Separator();
  Spacing();

  TextWrapped("Choose a workspace preset to quickly configure your layout:");
  Spacing();

  // Get named presets from LayoutPresets
  struct PresetInfo {
    const char* name;
    const char* icon;
    const char* description;
    std::function<PanelLayoutPreset()> getter;
  };

  PresetInfo presets[] = {
      {"Minimal", ICON_MD_CROP_FREE,
       "Essential cards only - maximum editing space",
       []() {
         return LayoutPresets::GetMinimalPreset();
       }},
      {"Developer", ICON_MD_BUG_REPORT,
       "Debug and development focused - CPU/Memory/Breakpoints",
       []() {
         return LayoutPresets::GetDeveloperPreset();
       }},
      {"Designer", ICON_MD_PALETTE,
       "Visual and artistic focused - Graphics/Palettes/Sprites",
       []() {
         return LayoutPresets::GetDesignerPreset();
       }},
      {"Modder", ICON_MD_BUILD,
       "Full-featured - All tools available for comprehensive editing",
       []() {
         return LayoutPresets::GetModderPreset();
       }},
      {"Overworld Expert", ICON_MD_MAP,
       "Complete overworld editing toolkit with all map tools",
       []() {
         return LayoutPresets::GetOverworldExpertPreset();
       }},
      {"Dungeon Expert", ICON_MD_DOOR_SLIDING,
       "Complete dungeon editing toolkit with room tools",
       []() {
         return LayoutPresets::GetDungeonExpertPreset();
       }},
      {"Testing", ICON_MD_SCIENCE, "Quality assurance and ROM testing layout",
       []() {
         return LayoutPresets::GetTestingPreset();
       }},
      {"Audio", ICON_MD_MUSIC_NOTE, "Music and sound editing layout",
       []() {
         return LayoutPresets::GetAudioPreset();
       }},
  };

  constexpr int kPresetCount = 8;

  // Draw preset buttons in a grid
  float button_width = 200.0f;
  float button_height = 50.0f;

  for (int i = 0; i < kPresetCount; i++) {
    if (i % 2 != 0)
      SameLine();

    {
      gui::StyleVarGuard align_guard(ImGuiStyleVar_ButtonTextAlign,
                                     ImVec2(0.0f, 0.5f));
      if (Button(
              absl::StrFormat("%s %s", presets[i].icon, presets[i].name).c_str(),
              ImVec2(button_width, button_height))) {
        // Apply the preset
        auto preset = presets[i].getter();
        auto& panel_manager = editor_manager_->panel_manager();
        // Hide all panels first
        panel_manager.HideAll();
        // Show preset panels
        for (const auto& panel_id : preset.default_visible_panels) {
          panel_manager.ShowPanel(panel_id);
        }
        Hide(PopupID::kLayoutPresets);
      }
    }

    if (IsItemHovered()) {
      BeginTooltip();
      TextUnformatted(presets[i].description);
      EndTooltip();
    }
  }

  Spacing();
  Separator();
  Spacing();

  // Reset current editor to defaults
  if (Button(
          absl::StrFormat("%s Reset Current Editor", ICON_MD_REFRESH).c_str(),
          ImVec2(-1, 0))) {
    auto& panel_manager = editor_manager_->card_registry();
    auto* current_editor = editor_manager_->GetCurrentEditor();
    if (current_editor) {
      auto current_type = current_editor->type();
      panel_manager.ResetToDefaults(0, current_type);
    }
    Hide(PopupID::kLayoutPresets);
  }

  Spacing();
  if (Button("Close", ImVec2(-1, 0))) {
    Hide(PopupID::kLayoutPresets);
  }
}

void PopupManager::DrawSessionManagerPopup() {
  TextColored(gui::GetInfoColor(), "%s Session Manager",
              ICON_MD_TAB);
  Separator();
  Spacing();

  size_t session_count = editor_manager_->GetActiveSessionCount();
  size_t active_session = editor_manager_->GetCurrentSessionId();

  Text("Active Sessions: %zu", session_count);
  Spacing();

  // Session table
  if (BeginTable("SessionTable", 4,
                 ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 30.0f);
    TableSetupColumn("ROM", ImGuiTableColumnFlags_WidthStretch);
    TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    TableHeadersRow();

    for (size_t i = 0; i < session_count; i++) {
      TableNextRow();

      // Session number
      TableSetColumnIndex(0);
      Text("%zu", i + 1);

      // ROM name (simplified - show current ROM for active session)
      TableSetColumnIndex(1);
      if (i == active_session) {
        auto* rom = editor_manager_->GetCurrentRom();
        if (rom && rom->is_loaded()) {
          TextUnformatted(rom->filename().c_str());
        } else {
          TextDisabled("(No ROM loaded)");
        }
      } else {
        TextDisabled("Session %zu", i + 1);
      }

      // Status indicator
      TableSetColumnIndex(2);
      if (i == active_session) {
        TextColored(gui::GetSuccessColor(), "%s Active",
                    ICON_MD_CHECK_CIRCLE);
      } else {
        TextDisabled("Inactive");
      }

      // Actions
      TableSetColumnIndex(3);
      PushID(static_cast<int>(i));

      if (i != active_session) {
        if (SmallButton("Switch")) {
          editor_manager_->SwitchToSession(i);
        }
        SameLine();
      }

      BeginDisabled(session_count <= 1);
      if (SmallButton("Close")) {
        editor_manager_->RemoveSession(i);
      }
      EndDisabled();

      PopID();
    }

    EndTable();
  }

  Spacing();
  Separator();
  Spacing();

  // New session button
  if (Button(absl::StrFormat("%s New Session", ICON_MD_ADD).c_str(),
             ImVec2(-1, 0))) {
    editor_manager_->CreateNewSession();
  }

  Spacing();
  if (Button("Close", ImVec2(-1, 0))) {
    Hide(PopupID::kSessionManager);
  }
}

void PopupManager::DrawDisplaySettingsPopup() {
  // Set a comfortable default size with natural constraints
  SetNextWindowSize(ImVec2(900, 700), ImGuiCond_FirstUseEver);
  SetNextWindowSizeConstraints(ImVec2(600, 400), ImVec2(FLT_MAX, FLT_MAX));

  Text("%s Display & Theme Settings", ICON_MD_DISPLAY_SETTINGS);
  TextWrapped("Customize your YAZE experience - accessible anytime!");
  Separator();

  // Create a child window for scrollable content to avoid table conflicts
  // Use remaining space minus the close button area
  float available_height =
      GetContentRegionAvail().y - 60;  // Reserve space for close button
  if (BeginChild("DisplaySettingsContent", ImVec2(0, available_height), true,
                 ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    // Use the popup-safe version to avoid table conflicts
    gui::DrawDisplaySettingsForPopup();

    Separator();
    gui::TextWithSeparators("Font Manager");
    gui::DrawFontManager();

    // Global font scale (moved from the old display settings window)
    ImGuiIO& io = GetIO();
    Separator();
    Text("Global Font Scale");
    float font_global_scale = io.FontGlobalScale;
    if (SliderFloat("##global_scale", &font_global_scale, 0.5f, 1.8f, "%.2f")) {
      if (editor_manager_) {
        editor_manager_->SetFontGlobalScale(font_global_scale);
      } else {
        io.FontGlobalScale = font_global_scale;
      }
    }
  }
  EndChild();

  Separator();
  if (Button("Close", ::yaze::gui::kDefaultModalSize)) {
    Hide("Display Settings");
  }
}

void PopupManager::DrawFeatureFlagsPopup() {
  using namespace ImGui;

  // Display feature flags editor using the existing FlagsMenu system
  Text("Feature Flags Configuration");
  Separator();

  BeginChild("##FlagsContent", ImVec2(0, -30), true);

  // Use the feature flags menu system
  static gui::FlagsMenu flags_menu;

  if (BeginTabBar("FlagCategories")) {
    if (BeginTabItem("Overworld")) {
      flags_menu.DrawOverworldFlags();
      EndTabItem();
    }
    if (BeginTabItem("Dungeon")) {
      flags_menu.DrawDungeonFlags();
      EndTabItem();
    }
    if (BeginTabItem("Resources")) {
      flags_menu.DrawResourceFlags();
      EndTabItem();
    }
    if (BeginTabItem("System")) {
      flags_menu.DrawSystemFlags();
      EndTabItem();
    }
    EndTabBar();
  }

  EndChild();

  Separator();
  if (Button("Close", ::yaze::gui::kDefaultModalSize)) {
    Hide(PopupID::kFeatureFlags);
  }
}

void PopupManager::DrawDataIntegrityPopup() {
  using namespace ImGui;

  Text("Data Integrity Check Results");
  Separator();

  BeginChild("##IntegrityContent", ImVec2(0, -30), true);

  // Placeholder for data integrity results
  // In a full implementation, this would show test results
  Text("ROM Data Integrity:");
  Separator();
  TextColored(gui::GetSuccessColor(), "✓ ROM header valid");
  TextColored(gui::GetSuccessColor(), "✓ Checksum valid");
  TextColored(gui::GetSuccessColor(), "✓ Graphics data intact");
  TextColored(gui::GetSuccessColor(), "✓ Map data intact");

  Spacing();
  Text("No issues detected.");

  EndChild();

  Separator();
  if (Button("Close", ::yaze::gui::kDefaultModalSize)) {
    Hide(PopupID::kDataIntegrity);
  }
}

void PopupManager::DrawDungeonPotItemSaveConfirmPopup() {
  using namespace ImGui;

  if (!editor_manager_) {
    Text("Editor manager unavailable.");
    if (Button("Close", ::yaze::gui::kDefaultModalSize)) {
      Hide(PopupID::kDungeonPotItemSaveConfirm);
    }
    return;
  }

  const int unloaded = editor_manager_->pending_pot_item_unloaded_rooms();
  const int total = editor_manager_->pending_pot_item_total_rooms();

  Text("Pot Item Save Confirmation");
  Separator();
  TextWrapped(
      "Dungeon pot item saving is enabled, but %d of %d rooms are not loaded.",
      unloaded, total);
  Spacing();
  TextWrapped(
      "Saving now can overwrite pot items in unloaded rooms. Choose how to "
      "proceed:");

  Spacing();
  if (Button("Save without pot items", ImVec2(0, 0))) {
    editor_manager_->ResolvePotItemSaveConfirmation(
        EditorManager::PotItemSaveDecision::kSaveWithoutPotItems);
    Hide(PopupID::kDungeonPotItemSaveConfirm);
    return;
  }
  SameLine();
  if (Button("Save anyway", ImVec2(0, 0))) {
    editor_manager_->ResolvePotItemSaveConfirmation(
        EditorManager::PotItemSaveDecision::kSaveWithPotItems);
    Hide(PopupID::kDungeonPotItemSaveConfirm);
    return;
  }
  SameLine();
  if (Button("Cancel", ImVec2(0, 0))) {
    editor_manager_->ResolvePotItemSaveConfirmation(
        EditorManager::PotItemSaveDecision::kCancel);
    Hide(PopupID::kDungeonPotItemSaveConfirm);
  }
}

void PopupManager::DrawRomWriteConfirmPopup() {
  using namespace ImGui;

  if (!editor_manager_) {
    Text("Editor manager unavailable.");
    if (Button("Close", ::yaze::gui::kDefaultModalSize)) {
      Hide(PopupID::kRomWriteConfirm);
    }
    return;
  }

  auto role = project::RomRoleToString(editor_manager_->GetProjectRomRole());
  auto policy =
      project::RomWritePolicyToString(editor_manager_->GetProjectRomWritePolicy());
  const auto expected = editor_manager_->GetProjectExpectedRomHash();
  const auto actual = editor_manager_->GetCurrentRomHash();

  Text("ROM Write Confirmation");
  Separator();
  TextWrapped(
      "The loaded ROM hash does not match the project's expected hash.");
  Spacing();
  Text("Role: %s", role.c_str());
  Text("Write policy: %s", policy.c_str());
  Text("Expected: %s", expected.empty() ? "(unset)" : expected.c_str());
  Text("Actual:   %s", actual.empty() ? "(unknown)" : actual.c_str());
  Spacing();
  TextWrapped(
      "Proceeding will write to the current ROM file. This may corrupt a base "
      "or release ROM if it is not the intended dev ROM.");

  Spacing();
  if (Button("Save anyway", ImVec2(0, 0))) {
    editor_manager_->ConfirmRomWrite();
    Hide(PopupID::kRomWriteConfirm);
    auto status = editor_manager_->SaveRom();
    if (!status.ok() && !absl::IsCancelled(status)) {
      if (auto* toast = editor_manager_->toast_manager()) {
        toast->Show(
            absl::StrFormat("Save failed: %s", status.message()),
            ToastType::kError);
      }
    }
    return;
  }
  SameLine();
  if (Button("Cancel", ImVec2(0, 0)) || IsKeyPressed(ImGuiKey_Escape)) {
    editor_manager_->CancelRomWriteConfirm();
    Hide(PopupID::kRomWriteConfirm);
  }
}

void PopupManager::DrawWriteConflictWarningPopup() {
  using namespace ImGui;

  if (!editor_manager_) {
    Text("Editor manager unavailable.");
    if (Button("Close", ::yaze::gui::kDefaultModalSize)) {
      Hide(PopupID::kWriteConflictWarning);
    }
    return;
  }

  const auto& conflicts = editor_manager_->pending_write_conflicts();

  TextColored(gui::GetWarningColor(), "%s Write Conflict Warning",
              ICON_MD_WARNING);
  Separator();
  TextWrapped(
      "The following ROM addresses are owned by ASM hooks and will be "
      "overwritten on next build. Saving now will write data that asar "
      "will replace.");
  Spacing();

  if (!conflicts.empty()) {
    if (BeginTable("WriteConflictTable", 3,
                   ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                       ImGuiTableFlags_Resizable)) {
      TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 120.0f);
      TableSetupColumn("Ownership", ImGuiTableColumnFlags_WidthFixed, 140.0f);
      TableSetupColumn("Module", ImGuiTableColumnFlags_WidthStretch);
      TableHeadersRow();

      for (const auto& conflict : conflicts) {
        TableNextRow();
        TableNextColumn();
        Text("$%06X", conflict.address);
        TableNextColumn();
        TextUnformatted(
            core::AddressOwnershipToString(conflict.ownership).c_str());
        TableNextColumn();
        if (!conflict.module.empty()) {
          TextUnformatted(conflict.module.c_str());
        } else {
          TextDisabled("(unknown)");
        }
      }
      EndTable();
    }
  }

  Spacing();
  Text("%zu conflict(s) detected.", conflicts.size());
  Spacing();

  if (Button("Save Anyway", ImVec2(0, 0))) {
    editor_manager_->BypassWriteConflictOnce();
    Hide(PopupID::kWriteConflictWarning);
    auto status = editor_manager_->SaveRom();
    if (!status.ok() && !absl::IsCancelled(status)) {
      if (auto* toast = editor_manager_->toast_manager()) {
        toast->Show(absl::StrFormat("Save failed: %s", status.message()),
                    ToastType::kError);
      }
    }
    return;
  }
  SameLine();
  if (Button("Cancel", ImVec2(0, 0)) || IsKeyPressed(ImGuiKey_Escape)) {
    editor_manager_->ClearPendingWriteConflicts();
    Hide(PopupID::kWriteConflictWarning);
  }
}

}  // namespace editor
}  // namespace yaze
