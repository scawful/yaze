#include "app/editor/system/session/project_build_workflow.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"

#include "app/editor/system/session/session_coordinator.h"
#include "core/native_save_receipt.h"
#include "core/source_artifact_publisher.h"
#include "util/macro.h"
#include "util/platform_paths.h"

namespace yaze::editor {

std::string FindCompanionZ3edPath(const std::filesystem::path& executable) {
#ifdef _WIN32
  constexpr const char* kZ3edName = "z3ed.exe";
#else
  constexpr const char* kZ3edName = "z3ed";
#endif
  const auto directory = executable.parent_path();
  std::vector<std::filesystem::path> candidates = {directory / kZ3edName};
  // Installs and builds place z3ed beside yaze.app, whose executable is
  // <root>/yaze.app/Contents/MacOS/yaze.
  if (directory.filename() == "MacOS" &&
      directory.parent_path().filename() == "Contents") {
    candidates.push_back(directory.parent_path().parent_path().parent_path() /
                         kZ3edName);
  }
  for (const auto& candidate : candidates) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(candidate, error)) {
      continue;
    }
#ifndef _WIN32
    const auto perms = std::filesystem::status(candidate, error).permissions();
    if (error || (perms & std::filesystem::perms::owner_exec) ==
                     std::filesystem::perms::none) {
      continue;
    }
#endif
    return candidate.lexically_normal().string();
  }
  return {};
}

std::string ProjectBuildResultKey(size_t session_id,
                                  const std::string& descriptor_path) {
  return std::to_string(session_id) + "|" + descriptor_path;
}

absl::StatusOr<std::map<std::string, std::string>>
BuildProjectCommandEnvironment(const project::YazeProject& project,
                               const std::string& editing_source_path,
                               const std::string& project_output_path) {
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
  auto environment = std::map<std::string, std::string>{
      {"YAZE_PROJECT_FILE", project_file.string()},
      {"YAZE_PROJECT_CODE_FOLDER", absolute(project.code_folder)},
      {"YAZE_PROJECT_ROM", absolute(project.rom_filename)},
      {"YAZE_NATIVE_SAVE_RECEIPT", project_file.string() + ".native-save.json"},
      {"YAZE_NATIVE_SAVE_CONTEXT", save_context_path},
      {"YAZE_LYRA_TERRAIN_FILE", absolute(project.lyra_terrain_file)},
      {"OOS_LYRA_PROJECT",
       project.lyra_terrain_file.empty() ? "" : project_file.string()},
      {"OOS_LYRA_REQUEST", absolute(project.lyra_build_request_file)}};
  if (!project_output_path.empty()) {
    environment["YAZE_PROJECT_OUTPUT"] = project_output_path;
  }
  // A GUI launch has no shell PATH, so give build scripts the z3ed that
  // shipped with this yaze unless the caller already chose one.
  const char* inherited_z3ed = std::getenv("OOS_Z3ED_BIN");
  if (inherited_z3ed == nullptr || *inherited_z3ed == '\0') {
    const auto executable = util::PlatformPaths::GetExecutablePath();
    if (executable.ok()) {
      if (auto z3ed = FindCompanionZ3edPath(*executable); !z3ed.empty()) {
        environment["OOS_Z3ED_BIN"] = std::move(z3ed);
      }
    }
  }
  return environment;
}

absl::StatusOr<std::string> ResolveConfiguredProjectRunCommand(
    const project::YazeProject& project) {
  if (!project.project_opened()) {
    return absl::FailedPreconditionError("No project open");
  }
  if (project.run_script.empty()) {
    return absl::NotFoundError(
        "Project does not define a run launcher command");
  }
  return project.run_script;
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
    const auto& pipeline = project.hack_manifest.build_pipeline();
    // A manifest whose development ROM is another file describes another
    // checkout (for example a copied project that still carries the original
    // folder's absolute paths). Its patched ROM is not this project's output,
    // so apply the build command's source rule and skip it.
    const bool manifest_describes_other_source =
        !pipeline.dev_rom.empty() && !project.rom_filename.empty() &&
        !SessionCoordinator::PathsReferToSameBackingFile(
            project.GetAbsolutePath(pipeline.dev_rom),
            project.GetAbsolutePath(project.rom_filename));
    if (!manifest_describes_other_source) {
      target = pipeline.patched_rom;
    } else if (project.build_target.empty()) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Run target mismatch: manifest development ROM %s differs from "
          "project ROM %s, and the project defines no build target.",
          project.GetAbsolutePath(pipeline.dev_rom),
          project.GetAbsolutePath(project.rom_filename)));
    }
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
