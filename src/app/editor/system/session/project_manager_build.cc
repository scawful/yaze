#include "project_manager.h"

#include <filesystem>

#include "absl/strings/str_format.h"
#include "app/editor/system/session/rom_lifecycle_manager.h"
#include "core/source_artifact_publisher.h"
#include "util/macro.h"

namespace yaze {
namespace editor {

namespace {

std::string LastNonEmptyLine(const std::string& text) {
  size_t end = text.size();
  while (end > 0) {
    size_t start = text.rfind('\n', end - 1);
    const size_t line_start = start == std::string::npos ? 0 : start + 1;
    std::string line = text.substr(line_start, end - line_start);
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n' ||
                             line.back() == ' ' || line.back() == '\t')) {
      line.pop_back();
    }
    size_t front = 0;
    while (front < line.size() && (line[front] == ' ' || line[front] == '\t')) {
      ++front;
    }
    line = line.substr(front);
    if (!line.empty()) {
      return line;
    }
    if (start == std::string::npos) {
      break;
    }
    end = start;
  }
  return "";
}

}  // namespace

void ProjectManager::SetBuildWorkflowServices(
    RomLifecycleManager* rom_lifecycle) {
  rom_lifecycle_ = rom_lifecycle;
}

bool ProjectManager::HasActiveProjectBuild() const {
  return active_project_build_ != nullptr;
}

absl::Status ProjectManager::ValidatePreparedBuildDoesNotOverlap(
    const ProjectBuildPreparedRequest& request) const {
  if (!active_project_build_ || !active_build_request_.has_value()) {
    return absl::OkStatus();
  }
  const auto snapshot = active_project_build_->GetSnapshot();
  if (!snapshot.running) {
    return absl::OkStatus();
  }
  if (active_build_request_->origin.session_id == request.origin.session_id &&
      active_build_request_->origin.project_descriptor_path ==
          request.origin.project_descriptor_path) {
    return absl::AlreadyExistsError("A project build is already running");
  }
  return absl::AlreadyExistsError(
      "Another project build is already running; wait for it to finish or "
      "cancel it first");
}

absl::StatusOr<ProjectBuildPreparedRequest>
ProjectManager::PrepareProjectBuildRequest(
    const ProjectBuildPrepareInputs& inputs) {
  if (rom_lifecycle_ == nullptr) {
    return absl::FailedPreconditionError("Build workflow is not initialized");
  }
  if (!inputs.origin_session_active) {
    return absl::FailedPreconditionError(
        "Build requires the originating project session to be active");
  }
  if (inputs.project == nullptr) {
    return absl::FailedPreconditionError("No project context for build");
  }
  RETURN_IF_ERROR(
      rom_lifecycle_->ValidateActiveProjectOrigin(inputs.origin_session_id));
  if (inputs.validate_readiness) {
    RETURN_IF_ERROR(inputs.validate_readiness());
  }

  const project::YazeProject& project = *inputs.project;
  ASSIGN_OR_RETURN(const auto command,
                   ResolveConfiguredProjectBuildCommand(project));
  ASSIGN_OR_RETURN(auto environment, BuildProjectCommandEnvironment(
                                         project, inputs.editing_source_path));
  ASSIGN_OR_RETURN(auto saved_input,
                   rom_lifecycle_->CaptureSavedBuildInputIdentity(
                       project, inputs.editing_source_path,
                       inputs.native_save_completion_pending));

  std::string resolved_output;
  if (auto target = ResolveConfiguredProjectRunTargetPath(
          project, inputs.editing_source_path);
      target.ok()) {
    resolved_output = *target;
  }

  ProjectBuildPreparedRequest request;
  request.attempt_id = next_build_attempt_id_++;
  request.origin.session_id = inputs.origin_session_id;
  request.origin.project_descriptor_path = project.filepath;
  request.origin.editing_source_path = inputs.editing_source_path;
  request.origin.rom_baseline = inputs.rom_baseline;
  request.project = project;
  request.command = command;
  request.project_root =
      std::filesystem::path(project.filepath).parent_path().string();
  request.environment = std::move(*environment);
  request.resolved_output_path = std::move(resolved_output);
  request.saved_input = std::move(*saved_input);
  RETURN_IF_ERROR(ValidatePreparedBuildDoesNotOverlap(request));
  return request;
}

absl::Status ProjectManager::StartPreparedProjectBuild(
    const ProjectBuildPreparedRequest& request, bool synchronous) {
  RETURN_IF_ERROR(ValidatePreparedBuildDoesNotOverlap(request));
  active_build_request_ = request;
  active_build_reported_ = false;
  active_project_build_ = std::make_unique<BackgroundCommandTask>();
  auto start_status = active_project_build_->Start(
      request.command, request.project_root, request.environment);
  if (!start_status.ok()) {
    active_project_build_.reset();
    active_build_request_.reset();
    return start_status;
  }
  if (synchronous) {
    return FinalizeActiveProjectBuild(true);
  }
  return absl::OkStatus();
}

absl::StatusOr<std::string> ProjectManager::WaitForPreparedProjectBuild(
    const ProjectBuildPreparedRequest& request) {
  if (active_build_request_.has_value() &&
      active_build_request_->attempt_id == request.attempt_id) {
    RETURN_IF_ERROR(FinalizeActiveProjectBuild(true));
  }
  const std::string key = ProjectBuildResultKey(
      request.origin.session_id, request.origin.project_descriptor_path);
  const auto it = terminal_build_results_.find(key);
  if (it == terminal_build_results_.end()) {
    return absl::InternalError("Build finished without a terminal record");
  }
  if (it->second.terminal_state != ProjectBuildTerminalState::kSucceeded) {
    return it->second.process_status;
  }
  const std::string summary = LastNonEmptyLine(it->second.output_log);
  return summary.empty() ? request.command : summary;
}

absl::StatusOr<ProjectBuildAttemptResult> ProjectManager::CompleteBuildAttempt(
    const ProjectBuildPreparedRequest& request,
    const BackgroundCommandTask::Snapshot& snapshot) {
  ProjectBuildAttemptResult result;
  result.attempt_id = request.attempt_id;
  result.origin = request.origin;
  result.saved_input = request.saved_input;
  result.output_log = snapshot.output;
  result.output_tail = snapshot.output_tail;
  result.process_status = snapshot.status;

  if (snapshot.status.code() == absl::StatusCode::kCancelled) {
    result.terminal_state = ProjectBuildTerminalState::kCancelled;
    return result;
  }
  if (!snapshot.status.ok()) {
    result.terminal_state = ProjectBuildTerminalState::kFailed;
    return result;
  }

  if (request.resolved_output_path.empty()) {
    result.terminal_state = ProjectBuildTerminalState::kSucceeded;
    return result;
  }

  std::error_code ec;
  if (!std::filesystem::is_regular_file(request.resolved_output_path, ec) ||
      ec) {
    result.terminal_state = ProjectBuildTerminalState::kFailed;
    result.process_status = absl::FailedPreconditionError(
        "Build finished but the configured run target ROM was not produced");
    return result;
  }

  ASSIGN_OR_RETURN(result.output_sha256, core::ComputeSourceArtifactFileSha256(
                                             request.resolved_output_path));
  result.output_size =
      std::filesystem::file_size(request.resolved_output_path, ec);
  result.output_path = request.resolved_output_path;
  result.terminal_state = ProjectBuildTerminalState::kSucceeded;
  result.run_eligible = true;
  return result;
}

absl::Status ProjectManager::FinalizeActiveProjectBuild(bool synchronous_wait) {
  if (!active_project_build_ || !active_build_request_.has_value()) {
    return absl::FailedPreconditionError("No active project build");
  }
  if (synchronous_wait) {
    RETURN_IF_ERROR(active_project_build_->Wait());
  }
  const auto snapshot = active_project_build_->GetSnapshot();
  if (snapshot.running) {
    return absl::OkStatus();
  }
  ASSIGN_OR_RETURN(auto result,
                   CompleteBuildAttempt(*active_build_request_, snapshot));
  const std::string key = ProjectBuildResultKey(
      result.origin.session_id, result.origin.project_descriptor_path);
  terminal_build_results_[key] = result;
  active_project_build_.reset();
  active_build_request_.reset();
  active_build_reported_ = true;
  return result.process_status;
}

ProjectBuildPollUpdate ProjectManager::PollProjectBuildForViewer(
    size_t viewer_session_id, const std::string& viewer_descriptor_path) {
  ProjectBuildPollUpdate update;
  const std::string viewer_key =
      ProjectBuildResultKey(viewer_session_id, viewer_descriptor_path);
  if (const auto it = terminal_build_results_.find(viewer_key);
      it != terminal_build_results_.end()) {
    update.owner_last_result = it->second;
  }

  if (!active_project_build_ || !active_build_request_.has_value()) {
    return update;
  }

  update.has_active_operation = true;
  update.owner_session_id = active_build_request_->origin.session_id;
  update.attempt_id = active_build_request_->attempt_id;
  update.task_snapshot = active_project_build_->GetSnapshot();

  if (update.task_snapshot.running) {
    return update;
  }
  if (active_build_reported_) {
    return update;
  }

  active_build_reported_ = true;
  auto finished_or =
      CompleteBuildAttempt(*active_build_request_, update.task_snapshot);
  if (finished_or.ok()) {
    update.finished = *finished_or;
    update.newly_finished = true;
    const std::string key =
        ProjectBuildResultKey(update.finished.origin.session_id,
                              update.finished.origin.project_descriptor_path);
    terminal_build_results_[key] = update.finished;
    if (viewer_key == key) {
      update.owner_last_result = update.finished;
    }
  }
  active_project_build_.reset();
  active_build_request_.reset();
  return update;
}

absl::Status ProjectManager::CancelProjectBuildForOwner(
    size_t owner_session_id, const std::string& descriptor_path) {
  if (!active_project_build_ || !active_build_request_.has_value()) {
    return absl::OkStatus();
  }
  if (active_build_request_->origin.session_id != owner_session_id ||
      active_build_request_->origin.project_descriptor_path !=
          descriptor_path) {
    return absl::FailedPreconditionError(
        "No running build belongs to this project session");
  }
  const auto snapshot = active_project_build_->GetSnapshot();
  if (!snapshot.running) {
    return absl::OkStatus();
  }
  active_project_build_->Cancel();
  RETURN_IF_ERROR(FinalizeActiveProjectBuild(false));
  return absl::OkStatus();
}

void ProjectManager::ResetProjectBuildPresentationForDescriptor(
    const std::string& descriptor_path) {
  for (auto it = terminal_build_results_.begin();
       it != terminal_build_results_.end();) {
    if (it->second.origin.project_descriptor_path == descriptor_path) {
      it = terminal_build_results_.erase(it);
    } else {
      ++it;
    }
  }
  if (active_build_request_.has_value() &&
      active_build_request_->origin.project_descriptor_path ==
          descriptor_path) {
    if (active_project_build_) {
      active_project_build_->Cancel();
    }
    active_project_build_.reset();
    active_build_request_.reset();
    active_build_reported_ = false;
  }
}

std::optional<ProjectBuildAttemptResult>
ProjectManager::GetLastBuildResultForOwner(
    size_t owner_session_id, const std::string& descriptor_path) const {
  const auto it = terminal_build_results_.find(
      ProjectBuildResultKey(owner_session_id, descriptor_path));
  if (it == terminal_build_results_.end()) {
    return std::nullopt;
  }
  return it->second;
}

absl::StatusOr<ProjectValidatedRunSelection>
ProjectManager::ValidateProjectRunSelection(
    const ProjectRunPrepareInputs& inputs) {
  if (rom_lifecycle_ == nullptr) {
    return absl::FailedPreconditionError("Run workflow is not initialized");
  }
  if (!inputs.origin_session_active) {
    return absl::FailedPreconditionError(
        "Run requires the originating project session to be active");
  }
  if (inputs.project == nullptr) {
    return absl::FailedPreconditionError("No project context for run");
  }
  if (inputs.build_running_for_origin) {
    return absl::FailedPreconditionError(
        "Wait for the active project build to finish before running");
  }
  RETURN_IF_ERROR(
      rom_lifecycle_->ValidateActiveProjectOrigin(inputs.origin_session_id));

  const project::YazeProject& project = *inputs.project;
  ASSIGN_OR_RETURN(auto saved_input,
                   rom_lifecycle_->CaptureSavedBuildInputIdentity(
                       project, inputs.editing_source_path,
                       inputs.native_save_completion_pending));

  ASSIGN_OR_RETURN(const std::string run_target,
                   ResolveConfiguredProjectRunTargetPath(
                       project, inputs.editing_source_path));

  const std::string result_key =
      ProjectBuildResultKey(inputs.origin_session_id, project.filepath);
  const auto last_result =
      GetLastBuildResultForOwner(inputs.origin_session_id, project.filepath);

  bool authorized = false;
  uint64_t attempt_id = 0;
  std::string expected_hash;
  uint64_t expected_size = 0;

  if (last_result.has_value()) {
    if (last_result->terminal_state == ProjectBuildTerminalState::kRunning) {
      return absl::FailedPreconditionError(
          "Wait for the active project build to finish before running");
    }
    if (last_result->terminal_state == ProjectBuildTerminalState::kSucceeded &&
        last_result->run_eligible &&
        last_result->saved_input.fingerprint_sha256 ==
            saved_input.fingerprint_sha256 &&
        last_result->output_path == run_target) {
      authorized = true;
      attempt_id = last_result->attempt_id;
      expected_hash = last_result->output_sha256;
      expected_size = last_result->output_size;
    }
  }

  if (!authorized && !project.lyra_build_request_file.empty()) {
    ASSIGN_OR_RETURN(const auto expanded, project.ResolveExpandedBuildRunTarget(
                                              inputs.editing_source_path));
    if (expanded && *expanded == run_target) {
      authorized = true;
    } else {
      return absl::FailedPreconditionError(
          "Build the project before running this expanded output");
    }
  }

  if (!authorized) {
    return absl::FailedPreconditionError(
        "Build the project before running its output");
  }

  if (expected_hash.empty()) {
    ASSIGN_OR_RETURN(expected_hash,
                     core::ComputeSourceArtifactFileSha256(run_target));
    std::error_code ec;
    expected_size = std::filesystem::file_size(run_target, ec);
    if (ec) {
      return absl::NotFoundError("Run target ROM not found");
    }
  }

  std::error_code ec;
  if (!std::filesystem::is_regular_file(run_target, ec) || ec) {
    return absl::NotFoundError("Run target ROM not found");
  }
  const uint64_t size = std::filesystem::file_size(run_target, ec);
  if (ec || size != expected_size) {
    return absl::FailedPreconditionError(
        "Run target changed since the last successful build");
  }
  ASSIGN_OR_RETURN(const auto live_hash,
                   core::ComputeSourceArtifactFileSha256(run_target));
  if (live_hash != expected_hash) {
    return absl::FailedPreconditionError(
        "Run target bytes changed since the last successful build");
  }

  ProjectValidatedRunSelection selection;
  selection.absolute_path = run_target;
  selection.sha256 = live_hash;
  selection.size_bytes = size;
  selection.origin.session_id = inputs.origin_session_id;
  selection.origin.project_descriptor_path = project.filepath;
  selection.origin.editing_source_path = inputs.editing_source_path;
  selection.saved_input = std::move(saved_input);
  selection.build_attempt_id = attempt_id;
  return selection;
}

}  // namespace editor
}  // namespace yaze
