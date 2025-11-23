#include "app/editor/code/project_file_editor.h"

#include <fstream>
#include <sstream>

#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "app/editor/system/toast_manager.h"
#include "app/gui/core/icons.h"
#include "core/project.h"
#include "imgui/imgui.h"
#include "util/file_util.h"

namespace yaze {
namespace editor {

ProjectFileEditor::ProjectFileEditor() {
  text_editor_.SetLanguageDefinition(TextEditor::LanguageDefinition::C());
  text_editor_.SetTabSize(2);
  text_editor_.SetShowWhitespaces(false);
}

void ProjectFileEditor::Draw() {
  if (!active_)
    return;

  ImGui::SetNextWindowSize(ImVec2(900, 700), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin(absl::StrFormat("%s Project Editor###ProjectFileEditor",
                                    ICON_MD_DESCRIPTION)
                        .c_str(),
                    &active_)) {
    ImGui::End();
    return;
  }

  // Toolbar
  if (ImGui::BeginTable("ProjectEditorToolbar", 8,
                        ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableNextColumn();
    if (ImGui::Button(absl::StrFormat("%s New", ICON_MD_NOTE_ADD).c_str())) {
      NewFile();
    }

    ImGui::TableNextColumn();
    if (ImGui::Button(
            absl::StrFormat("%s Open", ICON_MD_FOLDER_OPEN).c_str())) {
      auto file = util::FileDialogWrapper::ShowOpenFileDialog();
      if (!file.empty()) {
        auto status = LoadFile(file);
        if (!status.ok() && toast_manager_) {
          toast_manager_->Show(std::string(status.message().data(), status.message().size()),
                               ToastType::kError);
        }
      }
    }

    ImGui::TableNextColumn();
    bool can_save = !filepath_.empty() && IsModified();
    if (!can_save)
      ImGui::BeginDisabled();
    if (ImGui::Button(absl::StrFormat("%s Save", ICON_MD_SAVE).c_str())) {
      auto status = SaveFile();
      if (status.ok() && toast_manager_) {
        toast_manager_->Show("Project file saved", ToastType::kSuccess);
      } else if (!status.ok() && toast_manager_) {
        toast_manager_->Show(std::string(status.message().data(), status.message().size()), ToastType::kError);
      }
    }
    if (!can_save)
      ImGui::EndDisabled();

    ImGui::TableNextColumn();
    if (ImGui::Button(absl::StrFormat("%s Save As", ICON_MD_SAVE_AS).c_str())) {
      auto file = util::FileDialogWrapper::ShowSaveFileDialog(
          filepath_.empty() ? "project" : filepath_, "yaze");
      if (!file.empty()) {
        auto status = SaveFileAs(file);
        if (status.ok() && toast_manager_) {
          toast_manager_->Show("Project file saved", ToastType::kSuccess);
        } else if (!status.ok() && toast_manager_) {
          toast_manager_->Show(std::string(status.message().data(), status.message().size()),
                               ToastType::kError);
        }
      }
    }

    ImGui::TableNextColumn();
    ImGui::Text("|");

    ImGui::TableNextColumn();
    if (ImGui::Button(
            absl::StrFormat("%s Validate", ICON_MD_CHECK_CIRCLE).c_str())) {
      ValidateContent();
      show_validation_ = true;
    }

    ImGui::TableNextColumn();
    ImGui::Checkbox("Show Validation", &show_validation_);

    ImGui::TableNextColumn();
    if (!filepath_.empty()) {
      ImGui::TextDisabled("%s", filepath_.c_str());
    } else {
      ImGui::TextDisabled("No file loaded");
    }

    ImGui::EndTable();
  }

  ImGui::Separator();

  // Validation errors panel
  if (show_validation_ && !validation_errors_.empty()) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.3f, 0.2f, 0.2f, 0.5f));
    if (ImGui::BeginChild("ValidationErrors", ImVec2(0, 100), true)) {
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                         "%s Validation Errors:", ICON_MD_ERROR);
      for (const auto& error : validation_errors_) {
        ImGui::BulletText("%s", error.c_str());
      }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
  }

  // Main editor
  ImVec2 editor_size = ImGui::GetContentRegionAvail();
  text_editor_.Render("##ProjectEditor", editor_size);

  ImGui::End();
}

absl::Status ProjectFileEditor::LoadFile(const std::string& filepath) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Cannot open file: %s", filepath));
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  file.close();

  text_editor_.SetText(buffer.str());
  filepath_ = filepath;
  modified_ = false;

  ValidateContent();

  return absl::OkStatus();
}

absl::Status ProjectFileEditor::SaveFile() {
  if (filepath_.empty()) {
    return absl::InvalidArgumentError("No file path specified");
  }

  return SaveFileAs(filepath_);
}

absl::Status ProjectFileEditor::SaveFileAs(const std::string& filepath) {
  // Ensure .yaze extension
  std::string final_path = filepath;
  if (!absl::EndsWith(final_path, ".yaze")) {
    final_path += ".yaze";
  }

  std::ofstream file(final_path);
  if (!file.is_open()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Cannot create file: %s", final_path));
  }

  file << text_editor_.GetText();
  file.close();

  filepath_ = final_path;
  modified_ = false;

  // Add to recent files
  auto& recent_mgr = project::RecentFilesManager::GetInstance();
  recent_mgr.AddFile(filepath_);
  recent_mgr.Save();

  return absl::OkStatus();
}

void ProjectFileEditor::NewFile() {
  // Create a template project file
  const char* template_content = R"(# yaze Project File
# Format Version: 2.0

[project]
name=New Project
description=
author=
license=
version=1.0
created_date=
last_modified=
yaze_version=0.4.0
tags=

[files]
rom_filename=
rom_backup_folder=backups
code_folder=asm
assets_folder=assets
patches_folder=patches
labels_filename=labels.txt
symbols_filename=symbols.txt
output_folder=build
additional_roms=

[feature_flags]
# REMOVED: kLogInstructions - DisassemblyViewer is always active
kSaveDungeonMaps=true
kSaveGraphicsSheet=true
kLoadCustomOverworld=false

[workspace_settings]
font_global_scale=1.0
autosave_enabled=true
autosave_interval_secs=300
theme=dark
)";

  text_editor_.SetText(template_content);
  filepath_.clear();
  modified_ = true;
  validation_errors_.clear();
}

void ProjectFileEditor::ApplySyntaxHighlighting() {
  // TODO: Implement custom syntax highlighting for INI format
  // For now, use C language definition which provides some basic highlighting
}

void ProjectFileEditor::ValidateContent() {
  validation_errors_.clear();

  std::string content = text_editor_.GetText();
  std::vector<std::string> lines = absl::StrSplit(content, '\n');

  std::string current_section;
  int line_num = 0;

  for (const auto& line : lines) {
    line_num++;
    std::string trimmed = std::string(absl::StripAsciiWhitespace(line));

    // Skip empty lines and comments
    if (trimmed.empty() || trimmed[0] == '#')
      continue;

    // Check for section headers
    if (trimmed[0] == '[' && trimmed[trimmed.size() - 1] == ']') {
      current_section = trimmed.substr(1, trimmed.size() - 2);

      // Validate known sections
      if (current_section != "project" && current_section != "files" &&
          current_section != "feature_flags" &&
          current_section != "workspace_settings" &&
          current_section != "build_settings") {
        validation_errors_.push_back(absl::StrFormat(
            "Line %d: Unknown section [%s]", line_num, current_section));
      }
      continue;
    }

    // Check for key=value pairs
    size_t equals_pos = trimmed.find('=');
    if (equals_pos == std::string::npos) {
      validation_errors_.push_back(absl::StrFormat(
          "Line %d: Invalid format, expected key=value", line_num));
      continue;
    }
  }

  if (validation_errors_.empty() && show_validation_ && toast_manager_) {
    toast_manager_->Show("Project file validation passed", ToastType::kSuccess);
  }
}

void ProjectFileEditor::ShowValidationErrors() {
  if (validation_errors_.empty())
    return;

  ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Validation Errors:");
  for (const auto& error : validation_errors_) {
    ImGui::BulletText("%s", error.c_str());
  }
}

}  // namespace editor
}  // namespace yaze
