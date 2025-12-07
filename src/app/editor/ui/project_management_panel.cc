#include "app/editor/ui/project_management_panel.h"

#include "absl/strings/str_format.h"
#include "app/editor/ui/toast_manager.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"
#include "rom/rom.h"

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

  // Project name
  ImGui::TextColored(gui::GetTextSecondaryVec4(), "Name:");
  ImGui::SameLine();
  ImGui::Text("%s", project_->GetDisplayName().c_str());

  // Project file path
  ImGui::TextColored(gui::GetTextSecondaryVec4(), "Path:");
  ImGui::SameLine();
  if (ImGui::Selectable(project_->filepath.c_str(), false,
                        ImGuiSelectableFlags_None,
                        ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
    // Could copy to clipboard or open in file manager
    ImGui::SetClipboardText(project_->filepath.c_str());
    if (toast_manager_) {
      toast_manager_->Show("Path copied to clipboard", ToastType::kInfo);
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Click to copy path");
  }

  // Metadata
  if (!project_->metadata.author.empty()) {
    ImGui::TextColored(gui::GetTextSecondaryVec4(), "Author:");
    ImGui::SameLine();
    ImGui::Text("%s", project_->metadata.author.c_str());
  }

  if (!project_->metadata.description.empty()) {
    ImGui::TextColored(gui::GetTextSecondaryVec4(), "Description:");
    ImGui::TextWrapped("%s", project_->metadata.description.c_str());
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
    ImGui::TextColored(gui::ConvertColorToImVec4(theme.warning), "Not configured");
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

  if (ImGui::Button(ICON_MD_SAVE " Save Project", ImVec2(button_width, 0))) {
    if (save_project_callback_) {
      save_project_callback_();
    }
  }

  // Code folder
  if (!project_->code_folder.empty()) {
    ImGui::Spacing();
    ImGui::TextColored(gui::GetTextSecondaryVec4(), "Code Folder:");
    ImGui::Text("%s", project_->code_folder.c_str());
  }

  // Assets folder
  if (!project_->assets_folder.empty()) {
    ImGui::TextColored(gui::GetTextSecondaryVec4(), "Assets:");
    ImGui::Text("%s", project_->assets_folder.c_str());
  }

  // Build configuration
  if (!project_->build_target.empty()) {
    ImGui::Spacing();
    ImGui::TextColored(gui::GetTextSecondaryVec4(), "Build Target:");
    ImGui::Text("%s", project_->build_target.c_str());
  }
}

}  // namespace editor
}  // namespace yaze

