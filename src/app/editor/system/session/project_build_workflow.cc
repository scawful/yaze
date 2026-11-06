#include "app/editor/system/session/project_build_workflow.h"

#include <filesystem>
#include <fstream>

#include "absl/strings/str_format.h"

#include "app/editor/system/session/session_coordinator.h"
#include "core/native_save_receipt.h"
#include "core/source_artifact_publisher.h"
#include "util/macro.h"

namespace yaze::editor {

std::string ProjectBuildResultKey(size_t session_id,
                                  const std::string& descriptor_path) {
  return std::to_string(session_id) + "|" + descriptor_path;
}

absl::StatusOr<std::map<std::string, std::string>>
BuildProjectCommandEnvironment(const project::YazeProject& project,
                               const std::string& editing_source_path) {
  if (project.filepath.empty()) {
    return absl::FailedPreconditionError("Save the project before building");
  }
  std::error_code error;
  const auto project_file =
      std::filesystem::absolute(project.filepath, error).lexically_normal();
  if (error) {
    return absl::FailedPreconditionError(
        "Cannot resolve the project build context: " + error.message());
  }
  const auto absolute = [&project_file](const std::string& value) {
    if (value.empty()) {
      return std::string();
    }
    const std::filesystem::path path(value);
    return (path.is_absolute() ? path : project_file.parent_path() / path)
        .lexically_normal()
        .string();
  };
  std::string save_context_path;
#if defined(YAZE_WITH_JSON) && !defined(__EMSCRIPTEN__)
  if (!editing_source_path.empty()) {
    ASSIGN_OR_RETURN(
        auto save_context,
        core::GetNativeSaveContext(editing_source_path, project_file.string()));
    save_context_path = save_context.path.string();
  }
#endif
  return std::map<std::string, std::string>{
      {"YAZE_PROJECT_FILE", project_file.string()},
      {"YAZE_PROJECT_CODE_FOLDER", absolute(project.code_folder)},
      {"YAZE_PROJECT_ROM", absolute(project.rom_filename)},
      {"YAZE_NATIVE_SAVE_RECEIPT", project_file.string() + ".native-save.json"},
      {"YAZE_NATIVE_SAVE_CONTEXT", save_context_path},
      {"YAZE_LYRA_TERRAIN_FILE", absolute(project.lyra_terrain_file)},
      {"OOS_LYRA_PROJECT",
       project.lyra_terrain_file.empty() ? "" : project_file.string()},
      {"OOS_LYRA_REQUEST", absolute(project.lyra_build_request_file)}};
}

absl::StatusOr<std::string> ResolveConfiguredProjectBuildCommand(
    const project::YazeProject& project) {
  if (!project.project_opened()) {
    return absl::FailedPreconditionError("No project open");
  }
  if (project.rom_metadata.write_policy == project::RomWritePolicy::kReadOnly) {
    return absl::PermissionDeniedError(
        "Build Project is disabled by the project's read-only policy");
  }

  std::string command = project.build_script;
  if (command.empty() && project.hack_manifest.loaded()) {
    const auto& pipeline = project.hack_manifest.build_pipeline();
    if (!pipeline.dev_rom.empty() && !project.rom_filename.empty() &&
        !SessionCoordinator::PathsReferToSameBackingFile(
            project.GetAbsolutePath(pipeline.dev_rom),
            project.GetAbsolutePath(project.rom_filename))) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Build source mismatch: manifest development ROM %s differs from "
          "project ROM %s.",
          project.GetAbsolutePath(pipeline.dev_rom),
          project.GetAbsolutePath(project.rom_filename)));
    }
    command = pipeline.build_script;
    if (!command.empty() &&
        command.find_first_of(" \t\r\n|&;<>()$`'\"\\") == std::string::npos) {
      const std::filesystem::path script_path(command);
      if (script_path.is_absolute() || script_path.has_parent_path() ||
          script_path.extension() == ".sh") {
        const std::string absolute_script = project.GetAbsolutePath(command);
        std::error_code ec;
        if (!std::filesystem::is_regular_file(absolute_script, ec)) {
          return absl::FailedPreconditionError(absl::StrFormat(
              "Manifest build script is unavailable: %s. Configure a build "
              "script for this project before building.",
              absolute_script));
        }
      }
    }
  }
  if (command.empty()) {
    return absl::NotFoundError("Project does not define a build command");
  }
  return command;
}

absl::StatusOr<std::string> ResolveConfiguredProjectRunTargetPath(
    const project::YazeProject& project,
    const std::string& editing_source_path) {
  if (!project.project_opened()) {
    return absl::FailedPreconditionError("No project open");
  }

  ASSIGN_OR_RETURN(const auto expanded,
                   project.ResolveExpandedBuildRunTarget(editing_source_path));
  if (expanded) {
    return *expanded;
  }

  std::string target;
  if (project.hack_manifest.loaded()) {
    target = project.hack_manifest.build_pipeline().patched_rom;
  }
  if (target.empty()) {
    target = project.build_target;
  }
  if (target.empty()) {
    return absl::NotFoundError("Project does not define a run target ROM");
  }
  return project.GetAbsolutePath(target);
}

}  // namespace yaze::editor
