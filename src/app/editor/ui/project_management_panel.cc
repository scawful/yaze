#include "app/editor/ui/project_management_panel.h"

#include "absl/strings/str_format.h"
#include "app/editor/ui/toast_manager.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "util/platform_paths.h"
#include "yaze_config.h"

namespace yaze {
namespace editor {

void ProjectManagementPanel::Draw() {
  if (!project_) {
    ImGui::TextDisabled("No project loaded");
    ImGui::Spacing();
    ImGui::TextWrapped(
        "Open a .yaze project file or create a new project to access "
        "project management features.");
    return;
  }

  DrawProjectOverview();
  ImGui::Separator();
  DrawStorageLocations();
  ImGui::Separator();
  DrawRomManagement();
  ImGui::Separator();
  DrawVersionControl();
  ImGui::Separator();
  DrawQuickActions();
}

void ProjectManagementPanel::DrawProjectOverview() {
  // Section header
  ImGui::PushStyleColor(ImGuiCol_Text, gui::GetPrimaryVec4());
  ImGui::Text("%s Project", ICON_MD_FOLDER_SPECIAL);
  ImGui::PopStyleColor();
  ImGui::Spacing();

  ImGui::TextColored(gui::GetTextSecondaryVec4(), "Project Format:");
  ImGui::SameLine();
  ImGui::Text("%s", project_->format == project::ProjectFormat::kYazeNative
                         ? ".yaze"
                         : ".zsproj");

  ImGui::TextColored(gui::GetTextSecondaryVec4(), "Project YAZE Version:");
  ImGui::SameLine();
  const std::string& project_version = project_->metadata.yaze_version;
  if (project_version.empty()) {
    const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
    ImGui::TextColored(gui::ConvertColorToImVec4(theme.warning), "Unknown");
  } else if (project_version != YAZE_VERSION_STRING) {
    const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
    ImGui::TextColored(gui::ConvertColorToImVec4(theme.warning), "%s",
                       project_version.c_str());
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Project saved with v%s; running v%s",
                        project_version.c_str(), YAZE_VERSION_STRING);
    }
  } else {
    ImGui::Text("%s", project_version.c_str());
  }

  ImGui::TextColored(gui::GetTextSecondaryVec4(), "Running YAZE:");
  ImGui::SameLine();
  ImGui::Text("%s", YAZE_VERSION_STRING);

  // Project file path (read-only, click to copy)
  ImGui::TextColored(gui::GetTextSecondaryVec4(), "Path:");
  ImGui::SameLine();
  if (ImGui::Selectable(project_->filepath.c_str(), false,
                        ImGuiSelectableFlags_None,
                        ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
    ImGui::SetClipboardText(project_->filepath.c_str());
    if (toast_manager_) {
      toast_manager_->Show("Path copied to clipboard", ToastType::kInfo);
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Click to copy path");
  }

  ImGui::Spacing();

  // Editable Project Name
  ImGui::TextColored(gui::GetTextSecondaryVec4(), "Project Name:");
  static char name_buffer[256] = {};
  if (name_buffer[0] == '\0' && !project_->name.empty()) {
    strncpy(name_buffer, project_->name.c_str(), sizeof(name_buffer) - 1);
  }
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
  if (ImGui::InputText("##project_name", name_buffer, sizeof(name_buffer))) {
    project_->name = name_buffer;
    project_dirty_ = true;
  }

  // Editable Author
  ImGui::TextColored(gui::GetTextSecondaryVec4(), "Author:");
  static char author_buffer[256] = {};
  if (author_buffer[0] == '\0' && !project_->metadata.author.empty()) {
    strncpy(author_buffer, project_->metadata.author.c_str(),
            sizeof(author_buffer) - 1);
  }
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
  if (ImGui::InputText("##author", author_buffer, sizeof(author_buffer))) {
    project_->metadata.author = author_buffer;
    project_dirty_ = true;
  }

  // Editable Description
  ImGui::TextColored(gui::GetTextSecondaryVec4(), "Description:");
  static char desc_buffer[1024] = {};
  if (desc_buffer[0] == '\0' && !project_->metadata.description.empty()) {
    strncpy(desc_buffer, project_->metadata.description.c_str(),
            sizeof(desc_buffer) - 1);
  }
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
  if (ImGui::InputTextMultiline("##description", desc_buffer,
                                sizeof(desc_buffer), ImVec2(0, 60))) {
    project_->metadata.description = desc_buffer;
    project_dirty_ = true;
  }

  ImGui::Spacing();
}

void ProjectManagementPanel::DrawStorageLocations() {
  ImGui::PushStyleColor(ImGuiCol_Text, gui::GetPrimaryVec4());
  ImGui::Text("%s Storage", ICON_MD_STORAGE);
  ImGui::PopStyleColor();
  ImGui::Spacing();

  ImGui::TextWrapped(
      "Primary data lives under the .yaze root. Click any path to copy it.");
  ImGui::Spacing();

  auto app_root = util::PlatformPaths::GetAppDataDirectory();
  if (!app_root.ok()) {
    const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
    ImGui::TextColored(gui::ConvertColorToImVec4(theme.error),
                       "Storage unavailable: %s",
                       std::string(app_root.status().message()).c_str());
    return;
  }

  std::vector<std::pair<const char*, std::filesystem::path>> locations = {
      {"Root", *app_root},
      {"Projects", *app_root / "projects"},
      {"Layouts", *app_root / "layouts"},
      {"Workspaces", *app_root / "workspaces"},
      {"Logs", *app_root / "logs"},
      {"Agent", *app_root / "agent"}};

  auto temp_root = util::PlatformPaths::GetTempDirectory();
  if (temp_root.ok()) {
    locations.emplace_back("Temp", *temp_root);
  }

  if (ImGui::BeginTable("##storage_locations", 2,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                            ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("Location", ImGuiTableColumnFlags_WidthFixed,
                            110.0f);
    ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);
    for (const auto& entry : locations) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextColored(gui::GetTextSecondaryVec4(), "%s", entry.first);
      ImGui::TableNextColumn();
      const std::string display_path =
          util::PlatformPaths::NormalizePathForDisplay(entry.second);
      ImGui::PushID(entry.first);
      if (ImGui::Selectable(display_path.c_str(), false,
                            ImGuiSelectableFlags_SpanAllColumns)) {
        ImGui::SetClipboardText(display_path.c_str());
        if (toast_manager_) {
          toast_manager_->Show("Path copied to clipboard", ToastType::kInfo);
        }
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Click to copy");
      }
      ImGui::PopID();
    }
    ImGui::EndTable();
  }

  ImGui::Spacing();
}

void ProjectManagementPanel::DrawRomManagement() {
  ImGui::PushStyleColor(ImGuiCol_Text, gui::GetPrimaryVec4());
  ImGui::Text("%s ROM File", ICON_MD_MEMORY);
  ImGui::PopStyleColor();
  ImGui::Spacing();

  // Current ROM
  ImGui::TextColored(gui::GetTextSecondaryVec4(), "Current ROM:");
  if (project_->rom_filename.empty()) {
    const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
    ImGui::TextColored(gui::ConvertColorToImVec4(theme.warning),
                       "Not configured");
  } else {
    // Show just the filename, full path on hover
    std::string filename = project_->rom_filename;
    size_t pos = filename.find_last_of("/\\");
    if (pos != std::string::npos) {
      filename = filename.substr(pos + 1);
    }
    ImGui::Text("%s", filename.c_str());
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", project_->rom_filename.c_str());
    }
  }

  // ROM status
  if (rom_ && rom_->is_loaded()) {
    ImGui::TextColored(gui::GetTextSecondaryVec4(), "Title:");
    ImGui::SameLine();
    ImGui::Text("%s", rom_->title().c_str());

    ImGui::TextColored(gui::GetTextSecondaryVec4(), "Size:");
    ImGui::SameLine();
    ImGui::Text("%.2f MB", static_cast<float>(rom_->size()) / (1024 * 1024));

    if (rom_->dirty()) {
      const auto& theme2 = gui::ThemeManager::Get().GetCurrentTheme();
      ImGui::TextColored(gui::ConvertColorToImVec4(theme2.warning),
                         "%s Unsaved changes", ICON_MD_WARNING);
    }
  }

  ImGui::Spacing();

  // Action buttons
  float button_width = (ImGui::GetContentRegionAvail().x - 8) / 2;

  if (ImGui::Button(ICON_MD_SWAP_HORIZ " Swap ROM", ImVec2(button_width, 0))) {
    if (swap_rom_callback_) {
      swap_rom_callback_();
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Replace the ROM file for this project");
  }

  ImGui::SameLine();

  if (ImGui::Button(ICON_MD_REFRESH " Reload", ImVec2(button_width, 0))) {
    if (reload_rom_callback_) {
      reload_rom_callback_();
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Reload ROM from disk");
  }

  ImGui::Spacing();
}

void ProjectManagementPanel::DrawVersionControl() {
  ImGui::PushStyleColor(ImGuiCol_Text, gui::GetPrimaryVec4());
  ImGui::Text("%s Version Control", ICON_MD_HISTORY);
  ImGui::PopStyleColor();
  ImGui::Spacing();

  if (!version_manager_) {
    ImGui::TextDisabled("Version manager not available");
    return;
  }

  bool git_initialized = version_manager_->IsGitInitialized();

  if (!git_initialized) {
    ImGui::TextWrapped(
        "Git is not initialized for this project. Initialize Git to enable "
        "version control and snapshots.");
    ImGui::Spacing();

    if (ImGui::Button(ICON_MD_ADD " Initialize Git",
                      ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
      auto status = version_manager_->InitializeGit();
      if (status.ok()) {
        if (toast_manager_) {
          toast_manager_->Show("Git repository initialized",
                               ToastType::kSuccess);
        }
      } else {
        if (toast_manager_) {
          toast_manager_->Show(
              absl::StrFormat("Failed to initialize Git: %s", status.message()),
              ToastType::kError);
        }
      }
    }
    return;
  }

  // Show current commit
  std::string current_hash = version_manager_->GetCurrentHash();
  if (!current_hash.empty()) {
    ImGui::TextColored(gui::GetTextSecondaryVec4(), "Current:");
    ImGui::SameLine();
    ImGui::Text("%s", current_hash.substr(0, 7).c_str());
  }

  ImGui::Spacing();

  // Create snapshot section
  ImGui::Text("Create Snapshot:");
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
  ImGui::InputTextWithHint("##snapshot_msg", "Snapshot message...",
                           snapshot_message_, sizeof(snapshot_message_));

  if (ImGui::Button(ICON_MD_CAMERA_ALT " Create Snapshot",
                    ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
    std::string msg =
        snapshot_message_[0] ? snapshot_message_ : "Manual snapshot";
    auto result = version_manager_->CreateSnapshot(msg);
    if (result.ok()) {
      if (toast_manager_) {
        toast_manager_->Show(
            absl::StrFormat("Snapshot created: %s", result->commit_hash),
            ToastType::kSuccess);
      }
      snapshot_message_[0] = '\0';
      history_dirty_ = true;
    } else {
      if (toast_manager_) {
        toast_manager_->Show(
            absl::StrFormat("Snapshot failed: %s", result.status().message()),
            ToastType::kError);
      }
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Create a snapshot of your project (Git commit + ROM backup)");
  }

  // Show recent history
  DrawSnapshotHistory();
}

void ProjectManagementPanel::DrawSnapshotHistory() {
  if (!version_manager_ || !version_manager_->IsGitInitialized()) {
    return;
  }

  ImGui::Spacing();
  if (ImGui::CollapsingHeader(ICON_MD_LIST " Recent Snapshots",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    // Refresh history if needed
    if (history_dirty_) {
      history_cache_ = version_manager_->GetHistory(5);
      history_dirty_ = false;
    }

    if (history_cache_.empty()) {
      ImGui::TextDisabled("No snapshots yet");
    } else {
      for (const auto& entry : history_cache_) {
        // Format: "hash message"
        size_t space_pos = entry.find(' ');
        std::string hash =
            space_pos != std::string::npos ? entry.substr(0, 7) : entry;
        std::string message =
            space_pos != std::string::npos ? entry.substr(space_pos + 1) : "";

        ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextSecondaryVec4());
        ImGui::Text("%s", hash.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextWrapped("%s", message.c_str());
      }
    }
  }
}

void ProjectManagementPanel::DrawQuickActions() {
  ImGui::PushStyleColor(ImGuiCol_Text, gui::GetPrimaryVec4());
  ImGui::Text("%s Quick Actions", ICON_MD_BOLT);
  ImGui::PopStyleColor();
  ImGui::Spacing();

  float button_width = ImGui::GetContentRegionAvail().x;

  // Show unsaved indicator
  if (project_dirty_) {
    const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
    ImGui::TextColored(gui::ConvertColorToImVec4(theme.warning),
                       "%s Project has unsaved changes", ICON_MD_EDIT);
    ImGui::Spacing();
  }

  if (ImGui::Button(ICON_MD_SAVE " Save Project", ImVec2(button_width, 0))) {
    if (save_project_callback_) {
      save_project_callback_();
      project_dirty_ = false;
    }
  }

  ImGui::Spacing();

  // Editable Code folder
  ImGui::TextColored(gui::GetTextSecondaryVec4(), "Code Folder:");
  static char code_buffer[512] = {};
  if (code_buffer[0] == '\0' && !project_->code_folder.empty()) {
    strncpy(code_buffer, project_->code_folder.c_str(),
            sizeof(code_buffer) - 1);
  }
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 32);
  if (ImGui::InputText("##code_folder", code_buffer, sizeof(code_buffer))) {
    project_->code_folder = code_buffer;
    project_dirty_ = true;
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_FOLDER_OPEN "##browse_code")) {
    if (browse_folder_callback_) {
      browse_folder_callback_("code");
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Browse for code folder");
  }

  // Editable Assets folder
  ImGui::TextColored(gui::GetTextSecondaryVec4(), "Assets Folder:");
  static char assets_buffer[512] = {};
  if (assets_buffer[0] == '\0' && !project_->assets_folder.empty()) {
    strncpy(assets_buffer, project_->assets_folder.c_str(),
            sizeof(assets_buffer) - 1);
  }
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 32);
  if (ImGui::InputText("##assets_folder", assets_buffer,
                       sizeof(assets_buffer))) {
    project_->assets_folder = assets_buffer;
    project_dirty_ = true;
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_FOLDER_OPEN "##browse_assets")) {
    if (browse_folder_callback_) {
      browse_folder_callback_("assets");
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Browse for assets folder");
  }

  // Editable Build target
  ImGui::TextColored(gui::GetTextSecondaryVec4(), "Build Target:");
  static char build_buffer[256] = {};
  if (build_buffer[0] == '\0' && !project_->build_target.empty()) {
    strncpy(build_buffer, project_->build_target.c_str(),
            sizeof(build_buffer) - 1);
  }
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
  if (ImGui::InputText("##build_target", build_buffer, sizeof(build_buffer))) {
    project_->build_target = build_buffer;
    project_dirty_ = true;
  }

  // Build script
  ImGui::TextColored(gui::GetTextSecondaryVec4(), "Build Script:");
  static char script_buffer[512] = {};
  if (script_buffer[0] == '\0' && !project_->build_script.empty()) {
    strncpy(script_buffer, project_->build_script.c_str(),
            sizeof(script_buffer) - 1);
  }
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
  if (ImGui::InputText("##build_script", script_buffer,
                       sizeof(script_buffer))) {
    project_->build_script = script_buffer;
    project_dirty_ = true;
  }
}

}  // namespace editor
}  // namespace yaze
