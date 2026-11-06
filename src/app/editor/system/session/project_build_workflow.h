#ifndef YAZE_APP_EDITOR_SYSTEM_PROJECT_BUILD_WORKFLOW_H_
#define YAZE_APP_EDITOR_SYSTEM_PROJECT_BUILD_WORKFLOW_H_

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/session_types.h"
#include "app/editor/system/session/background_command_task.h"
#include "app/editor/system/session/project_workflow_status.h"
#include "core/project.h"

namespace yaze::editor {

struct ProjectSavedInputIdentity {
  std::string fingerprint_sha256;
  bool uses_native_receipt = false;
  uint64_t native_generation = 0;
  std::string native_operation_id;
};

struct ProjectBuildOriginBinding {
  size_t session_id = SIZE_MAX;
  std::string project_descriptor_path;
  std::string editing_source_path;
  std::optional<RomBaselineIdentity> rom_baseline;
};

struct ProjectBuildPreparedRequest {
  uint64_t attempt_id = 0;
  ProjectBuildOriginBinding origin;
  project::YazeProject project;
  std::string command;
  std::string project_root;
  std::map<std::string, std::string> environment;
  std::string resolved_output_path;
  ProjectSavedInputIdentity saved_input;
};

enum class ProjectBuildTerminalState {
  kRunning,
  kSucceeded,
  kFailed,
  kCancelled,
};

struct ProjectBuildAttemptResult {
  uint64_t attempt_id = 0;
  ProjectBuildOriginBinding origin;
  ProjectBuildTerminalState terminal_state = ProjectBuildTerminalState::kFailed;
  ProjectSavedInputIdentity saved_input;
  std::string output_path;
  std::string output_sha256;
  uint64_t output_size = 0;
  bool run_eligible = false;
  absl::Status process_status = absl::UnknownError("Not finished");
  std::string output_log;
  std::string output_tail;
};

struct ProjectValidatedRunSelection {
  std::string absolute_path;
  std::string sha256;
  uint64_t size_bytes = 0;
  ProjectBuildOriginBinding origin;
  ProjectSavedInputIdentity saved_input;
  uint64_t build_attempt_id = 0;
};

struct ProjectBuildPollUpdate {
  bool has_active_operation = false;
  size_t owner_session_id = SIZE_MAX;
  uint64_t attempt_id = 0;
  BackgroundCommandTask::Snapshot task_snapshot;
  bool newly_finished = false;
  ProjectBuildAttemptResult finished;
  std::optional<ProjectBuildAttemptResult> owner_last_result;
};

struct ProjectBuildPrepareInputs {
  size_t origin_session_id = SIZE_MAX;
  bool origin_session_active = false;
  bool native_save_completion_pending = false;
  const project::YazeProject* project = nullptr;
  std::string editing_source_path;
  std::optional<RomBaselineIdentity> rom_baseline;
  std::function<absl::Status()> validate_readiness;
};

struct ProjectRunPrepareInputs {
  size_t origin_session_id = SIZE_MAX;
  bool origin_session_active = false;
  bool native_save_completion_pending = false;
  bool build_running_for_origin = false;
  const project::YazeProject* project = nullptr;
  std::string editing_source_path;
};

std::string ProjectBuildResultKey(size_t session_id,
                                  const std::string& descriptor_path);

absl::StatusOr<std::map<std::string, std::string>>
BuildProjectCommandEnvironment(const project::YazeProject& project,
                               const std::string& editing_source_path);

absl::StatusOr<std::string> ResolveConfiguredProjectBuildCommand(
    const project::YazeProject& project);

absl::StatusOr<std::string> ResolveConfiguredProjectRunTargetPath(
    const project::YazeProject& project,
    const std::string& editing_source_path);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_SYSTEM_PROJECT_BUILD_WORKFLOW_H_
