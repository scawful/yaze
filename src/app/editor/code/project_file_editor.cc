#include "app/editor/code/project_file_editor.h"
#include "util/i18n/tr.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "app/editor/shell/feedback/toast_manager.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style_guard.h"
#include "core/project.h"
#include "imgui/imgui.h"
#include "util/file_util.h"
#include "util/macro.h"
#include "yaze_config.h"

#ifdef __EMSCRIPTEN__
#include "app/platform/wasm/wasm_storage.h"
#elif defined(_WIN32)
#include <windows.h>
#endif
namespace yaze {
namespace editor {

namespace {

std::filesystem::path MakeRawProjectSaveTempPath(
    const std::filesystem::path& target_path) {
  static std::atomic<uint64_t> next_temp_id{0};
  const auto timestamp =
      std::chrono::steady_clock::now().time_since_epoch().count();
  std::filesystem::path temp_path = target_path;
  temp_path +=
      ".tmp." + std::to_string(timestamp) + "." +
      std::to_string(next_temp_id.fetch_add(1, std::memory_order_relaxed));
  return temp_path;
}

#ifdef __EMSCRIPTEN__
std::string ProjectStorageKey(const project::YazeProject* project,
                              const std::string& filepath) {
  if (project != nullptr && project->filepath == filepath) {
    return project->MakeStorageKey("project");
  }
  std::string key = std::filesystem::path(filepath).stem().string();
  return key.empty() ? "project" : key;
}
#endif

}  // namespace

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
  if (ImGui::BeginTable("ProjectEditorToolbar", 10,
                        ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableNextColumn();
    if (ImGui::Button(absl::StrFormat("%s New", ICON_MD_NOTE_ADD).c_str())) {
      auto status = NewFile();
      if (!status.ok() && toast_manager_) {
        toast_manager_->Show(std::string(status.message()), ToastType::kError);
      }
    }

    ImGui::TableNextColumn();
    if (ImGui::Button(
            absl::StrFormat("%s Open", ICON_MD_FOLDER_OPEN).c_str())) {
      auto file = util::FileDialogWrapper::ShowOpenFileDialog();
      if (!file.empty()) {
        auto status = LoadFile(file);
        if (!status.ok() && toast_manager_) {
          toast_manager_->Show(
              std::string(status.message().data(), status.message().size()),
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
        toast_manager_->Show(
            std::string(status.message().data(), status.message().size()),
            ToastType::kError);
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
          toast_manager_->Show(
              std::string(status.message().data(), status.message().size()),
              ToastType::kError);
        }
      }
    }

    ImGui::TableNextColumn();
    ImGui::Text("|");

    ImGui::TableNextColumn();
    // Import ZScream Labels button
    if (ImGui::Button(
            absl::StrFormat("%s Import Labels", ICON_MD_LABEL).c_str())) {
      auto status = ImportLabelsFromZScream();
      if (status.ok() && toast_manager_) {
        toast_manager_->Show("Labels imported successfully",
                             ToastType::kSuccess);
      } else if (!status.ok() && toast_manager_) {
        toast_manager_->Show(
            std::string(status.message().data(), status.message().size()),
            ToastType::kError);
      }
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(tr("Import labels from ZScream DefaultNames.txt"));
    }

    ImGui::TableNextColumn();
    if (ImGui::Button(
            absl::StrFormat("%s Validate", ICON_MD_CHECK_CIRCLE).c_str())) {
      ValidateContent();
      show_validation_ = true;
    }

    ImGui::TableNextColumn();
    ImGui::Checkbox(tr("Show Validation"), &show_validation_);

    ImGui::TableNextColumn();
    if (!filepath_.empty()) {
      ImGui::TextDisabled("%s", filepath_.c_str());
    } else {
      ImGui::TextDisabled(tr("No file loaded"));
    }

    ImGui::EndTable();
  }

  ImGui::Separator();

  // Validation errors panel
  if (show_validation_ && !validation_errors_.empty()) {
    gui::StyledChild errors_child("ValidationErrors", ImVec2(0, 100),
                                  {.bg = ImVec4(0.3f, 0.2f, 0.2f, 0.5f)}, true);
    if (errors_child) {
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                         tr("%s Validation Errors:"), ICON_MD_ERROR);
      for (const auto& error : validation_errors_) {
        ImGui::BulletText("%s", error.c_str());
      }
    }
  }

  // Main editor
  ImVec2 editor_size = ImGui::GetContentRegionAvail();
  text_editor_.Render("##ProjectEditor", editor_size);
  if (text_editor_.IsTextChanged()) {
    modified_ = true;
  }

  ImGui::End();
}

ProjectFileEditorState ProjectFileEditor::CaptureState() const {
  ProjectFileEditorState state;
  state.filepath = filepath_;
  state.text = GetDocumentText();
  state.initialized = initialized_;
  state.active = active_;
  state.modified = modified_;
  state.show_validation = show_validation_;
  state.validation_errors = validation_errors_;
  return state;
}

void ProjectFileEditor::RestoreState(const ProjectFileEditorState& state,
                                     project::YazeProject* project) {
  project_ = project;
  filepath_ = state.filepath;
  text_editor_.SetText(state.text);
  initialized_ = state.initialized;
  active_ = state.active;
  modified_ = state.modified;
  show_validation_ = state.show_validation;
  validation_errors_ = state.validation_errors;
}

void ProjectFileEditor::ResetForProject(project::YazeProject* project) {
  project_ = project;
  filepath_.clear();
  text_editor_.SetText("");
  initialized_ = false;
  active_ = false;
  modified_ = false;
  show_validation_ = true;
  validation_errors_.clear();
}

absl::Status ProjectFileEditor::LoadFile(const std::string& filepath) {
  RETURN_IF_ERROR(CanReplaceDocument());
#ifdef __EMSCRIPTEN__
  const std::string key = ProjectStorageKey(project_, filepath);
  auto storage_or = platform::WasmStorage::LoadProject(key);
  if (storage_or.ok()) {
    text_editor_.SetText(storage_or.value());
    filepath_ = filepath;
    initialized_ = true;
    modified_ = false;
    ValidateContent();
    return absl::OkStatus();
  }
#endif

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
  initialized_ = true;
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
  const std::string contents = GetDocumentText();

  if (save_guard_callback_) {
    RETURN_IF_ERROR(save_guard_callback_(final_path, contents));
  }

#ifdef __EMSCRIPTEN__
  const std::string key = ProjectStorageKey(project_, final_path);
  auto storage_status = platform::WasmStorage::SaveProject(key, contents);
  if (!storage_status.ok()) {
    return storage_status;
  }
  if (save_complete_callback_) {
    RETURN_IF_ERROR(save_complete_callback_(final_path, contents));
  }
  filepath_ = final_path;
  initialized_ = true;
  modified_ = false;
  auto& recent_mgr = project::RecentFilesManager::GetInstance();
  recent_mgr.AddFile(filepath_);
  recent_mgr.Save();
  return absl::OkStatus();
#else
  const std::filesystem::path target_path(final_path);
  const std::filesystem::path temp_path =
      MakeRawProjectSaveTempPath(target_path);
  std::ofstream file(temp_path, std::ios::binary | std::ios::trunc);
  if (!file.is_open()) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Cannot create temporary project file: %s", temp_path.string()));
  }

  file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
  file.flush();
  if (!file.good()) {
    file.close();
    std::error_code remove_error;
    std::filesystem::remove(temp_path, remove_error);
    return absl::InternalError(
        absl::StrFormat("Failed to write project file: %s", final_path));
  }
  file.close();
  if (file.fail()) {
    std::error_code remove_error;
    std::filesystem::remove(temp_path, remove_error);
    return absl::InternalError(
        absl::StrFormat("Failed to close project file: %s", final_path));
  }

  std::error_code rename_error;
#if defined(_WIN32)
  if (!::MoveFileExW(temp_path.c_str(), target_path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    rename_error = std::error_code(static_cast<int>(::GetLastError()),
                                   std::system_category());
  }
#else
  std::filesystem::rename(temp_path, target_path, rename_error);
#endif
  if (rename_error) {
    std::error_code remove_error;
    std::filesystem::remove(temp_path, remove_error);
    return absl::InternalError(absl::StrFormat(
        "Failed to replace project file: %s", rename_error.message()));
  }

  if (save_complete_callback_) {
    RETURN_IF_ERROR(save_complete_callback_(final_path, contents));
  }

  filepath_ = final_path;
  initialized_ = true;
  modified_ = false;

  // Add to recent files
  auto& recent_mgr = project::RecentFilesManager::GetInstance();
  recent_mgr.AddFile(filepath_);
  recent_mgr.Save();

  return absl::OkStatus();
#endif
}

absl::Status ProjectFileEditor::CanReplaceDocument() const {
  if (initialized_ && modified_) {
    return absl::FailedPreconditionError(
        "Project-file draft has unsaved changes; use Save or Save As first");
  }
  return absl::OkStatus();
}

absl::Status ProjectFileEditor::NewFile() {
  RETURN_IF_ERROR(CanReplaceDocument());
  // Create a template project file
  const std::string template_content = absl::StrFormat(
      R"(# yaze Project File
# Format Version: 2.0

[project]
name=New Project
project_id=
description=
author=
license=
version=1.0
created_date=
last_modified=
yaze_version=%s
created_by=YAZE
tags=

[agent_settings]
ai_provider=auto
ai_model=
ollama_host=http://localhost:11434
use_custom_prompt=false

[files]
rom_filename=
rom_backup_folder=backups
code_folder=asm
assets_folder=assets
patches_folder=patches
labels_filename=labels.txt
symbols_filename=symbols.txt
output_folder=build
custom_objects_folder=
# Optional: ASM integration (e.g. Oracle-of-Secrets hack_manifest.json)
hack_manifest_file=
additional_roms=

[rom]
role=dev
expected_hash=
write_policy=warn

[feature_flags]
# REMOVED: kLogInstructions - DisassemblyViewer is always active
kSaveOverworldMaps=true
kSaveOverworldEntrances=true
kSaveOverworldExits=true
kSaveOverworldItems=true
kSaveOverworldProperties=true
kSaveDungeonMaps=true
kSaveDungeonObjects=true
kSaveDungeonSprites=true
kSaveDungeonRoomHeaders=true
kSaveDungeonTorches=true
kSaveDungeonPits=true
kSaveDungeonBlocks=true
kSaveDungeonCollision=true
kSaveDungeonChests=true
kSaveDungeonPotItems=true
kSaveDungeonPalettes=true
kSaveGraphicsSheet=true
kSaveAllPalettes=true
kSaveGfxGroups=true
kSaveMessages=true
kLoadCustomOverworld=false

[workspace]
font_global_scale=1.0
autosave_enabled=true
autosave_interval_secs=300
backup_on_save=true
backup_retention_count=20
backup_keep_daily=true
backup_keep_daily_days=14
theme=dark

[rom_addresses]
# expanded_message_start=0x178000
# expanded_message_end=0x17FFFF
# expanded_music_hook=0x008919
# expanded_music_main=0x1A9EF5
# expanded_music_aux=0x1ACCA7
# overworld_messages_expanded=0x1417F8
# overworld_map16_expanded=0x1E8000
# overworld_map32_tr_expanded=0x020000
# overworld_map32_bl_expanded=0x1F0000
# overworld_map32_br_expanded=0x1F8000
# overworld_entrance_map_expanded=0x0DB55F
# overworld_entrance_pos_expanded=0x0DB35F
# overworld_entrance_id_expanded=0x0DB75F
# overworld_entrance_flag_expanded=0x0DB895
# overworld_ptr_marker_expanded=0x1423FF
# overworld_ptr_magic_expanded=0xEA
# overworld_gfx_ptr1=0x004F80
# overworld_gfx_ptr2=0x00505F
# overworld_gfx_ptr3=0x00513E

[custom_objects]
# object_0x31=track_LR.bin,track_UD.bin,track_corner_TL.bin
# object_0x32=furnace.bin,firewood.bin,ice_chair.bin

[build]
build_script=
output_folder=build
build_target=
asm_entry_point=asm/main.asm
asm_sources=asm
build_number=0
last_build_hash=

[music]
persist_custom_music=true
storage_key=
last_saved_at=
)",
      YAZE_VERSION_STRING);

  text_editor_.SetText(template_content);
  filepath_.clear();
  initialized_ = true;
  modified_ = true;
  validation_errors_.clear();
  return absl::OkStatus();
}

void ProjectFileEditor::ApplySyntaxHighlighting() {
  // TODO: Implement custom syntax highlighting for INI format
  // For now, use C language definition which provides some basic highlighting
}

void ProjectFileEditor::ValidateContent() {
  validation_errors_.clear();

  std::string content = GetDocumentText();
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
          current_section != "workspace" &&
          current_section != "workspace_settings" && current_section != "rom" &&
          current_section != "rom_addresses" &&
          current_section != "custom_objects" &&
          current_section != "dungeon_overlay" && current_section != "build" &&
          current_section != "agent_settings" && current_section != "music" &&
          current_section != "keybindings" &&
          current_section != "editor_visibility") {
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

std::string ProjectFileEditor::GetDocumentText() const {
  std::string text = text_editor_.GetText();
  // TextEditor emits one synthetic trailing newline for its final line.
  // Exclude that sentinel so capture/restore and load/save are byte-stable.
  if (!text.empty() && text.back() == '\n') {
    text.pop_back();
  }
  return text;
}

void ProjectFileEditor::ShowValidationErrors() {
  if (validation_errors_.empty())
    return;

  ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), tr("Validation Errors:"));
  for (const auto& error : validation_errors_) {
    ImGui::BulletText("%s", error.c_str());
  }
}

absl::Status ProjectFileEditor::ImportLabelsFromZScream() {
#ifdef __EMSCRIPTEN__
  return absl::UnimplementedError(
      "File-based label import is not supported in the web build");
#else
  if (!project_) {
    return absl::FailedPreconditionError(
        "No project loaded. Open a project first.");
  }

  // Show file dialog for DefaultNames.txt
  auto file = util::FileDialogWrapper::ShowOpenFileDialog();
  if (file.empty()) {
    return absl::CancelledError("No file selected");
  }

  // Read the file contents
  std::ifstream input_file(file);
  if (!input_file.is_open()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Cannot open file: %s", file));
  }

  std::stringstream buffer;
  buffer << input_file.rdbuf();
  input_file.close();

  // Import using the project's method
  auto status = project_->ImportLabelsFromZScreamContent(buffer.str());
  if (!status.ok()) {
    return status;
  }

  // Save the project to persist the imported labels
  return project_->Save();
#endif
}

}  // namespace editor
}  // namespace yaze
