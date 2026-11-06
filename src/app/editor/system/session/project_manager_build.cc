#include "project_manager.h"

#include <filesystem>

#include "absl/strings/str_format.h"
#include "app/editor/system/session/rom_lifecycle_manager.h"
#include "core/source_artifact_publisher.h"
#include "util/macro.h"

namespace yaze {
namespace editor {

namespace {

bool RomBaselinesMatch(const std::optional<RomBaselineIdentity>& left,
                       const std::optional<RomBaselineIdentity>& right) {
  if (!left.has_value() != !right.has_value()) {
    return false;
  }
  if (!left.has_value()) {
    return true;
  }
  return left->editing_source_path == right->editing_source_path &&
         left->backing_sha256 == right->backing_sha256 &&
         left->normalized_path == right->normalized_path;
}

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

bool ProjectManager::HasActiveProjectBuildForOrigin(
    size_t owner_session_id, const std::string& descriptor_path) const {
  if (!active_project_build_ || !active_build_request_.has_value()) {
    return false;
  }
  const auto snapshot = active_project_build_->GetSnapshot();
  if (!snapshot.running) {
    return false;
  }
  return active_build_request_->origin.session_id == owner_session_id &&
         active_build_request_->origin.project_descriptor_path ==
             descriptor_path;
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

  const bool requires_configured_output =
      !project.lyra_build_request_file.empty() ||
      !project.build_target.empty() ||
      (project.hack_manifest.loaded() &&
       !project.hack_manifest.build_pipeline().patched_rom.empty());
  std::string resolved_output;
  if (!project.lyra_build_request_file.empty()) {
    if (project.build_target.empty()) {
      return absl::FailedPreconditionError(
          "Expanded Build requires a configured build target");
    }
    // Capture intent before the child exists. Completed expanded proof belongs
    // to completion/Run and can legitimately be absent for this generation.
    std::error_code error;
    const auto target = std::filesystem::weakly_canonical(
        std::filesystem::u8path(project.GetAbsolutePath(project.build_target)),
        error);
    if (error) {
      return absl::FailedPreconditionError(
          "Cannot resolve expanded build target");
    }
    const auto utf8 = target.generic_u8string();
    resolved_output.assign(utf8.begin(), utf8.end());
  } else if (requires_configured_output) {
    ASSIGN_OR_RETURN(resolved_output, ResolveConfiguredProjectRunTargetPath(
                                          project, inputs.editing_source_path));
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
  request.environment = std::move(environment);
  request.resolved_output_path = std::move(resolved_output);
  request.saved_input = std::move(saved_input);
  if (!request.resolved_output_path.empty()) {
    std::error_code ec;
    if (std::filesystem::is_regular_file(request.resolved_output_path, ec) &&
        !ec) {
      request.output_existed_at_prepare = true;
      request.output_preimage_size =
          std::filesystem::file_size(request.resolved_output_path, ec);
      ASSIGN_OR_RETURN(
          request.output_preimage_sha256,
          core::ComputeSourceArtifactFileSha256(request.resolved_output_path));
    }
  }
  RETURN_IF_ERROR(ValidatePreparedBuildDoesNotOverlap(request));
  return request;
}

absl::Status ProjectManager::ValidatePreparedBuildOriginBinding(
    const ProjectBuildPreparedRequest& request,
    bool require_active_origin) const {
  if (rom_lifecycle_ == nullptr) {
    return absl::FailedPreconditionError("Build workflow is not initialized");
  }
  if (require_active_origin) {
    RETURN_IF_ERROR(
        rom_lifecycle_->ValidateActiveProjectOrigin(request.origin.session_id));
  }
  ASSIGN_OR_RETURN(
      const auto origin_snapshot,
      rom_lifecycle_->ResolveProjectOriginSnapshot(request.origin.session_id));
  if (!origin_snapshot.session_open) {
    return absl::FailedPreconditionError(
        "Build origin session is no longer available");
  }
  if (origin_snapshot.project_descriptor_path !=
      request.origin.project_descriptor_path) {
    return absl::FailedPreconditionError(
        "Project binding changed since the build was prepared");
  }
  if (origin_snapshot.editing_source_path !=
      request.origin.editing_source_path) {
    return absl::FailedPreconditionError(
        "Editing source changed since the build was prepared");
  }
  if (!RomBaselinesMatch(origin_snapshot.rom_baseline,
                         request.origin.rom_baseline)) {
    return absl::FailedPreconditionError(
        "ROM baseline changed since the build was prepared");
  }
  project::YazeProject live_project;
  RETURN_IF_ERROR(live_project.Open(request.origin.project_descriptor_path));
  return rom_lifecycle_->SavedBuildInputIdentityMatchesLive(
      request.saved_input, live_project, request.origin.editing_source_path,
      /*native_save_completion_pending=*/false);
}

void ProjectManager::RevokeRunEligibilityForOwner(
    size_t owner_session_id, const std::string& descriptor) {
  const std::string key = ProjectBuildResultKey(owner_session_id, descriptor);
  const auto it = terminal_build_results_.find(key);
  if (it != terminal_build_results_.end()) {
    it->second.run_eligible = false;
  }
}

ProjectBuildAttemptResult ProjectManager::MakeTerminalBuildFailure(
    const ProjectBuildPreparedRequest& request,
    const BackgroundCommandTask::Snapshot& snapshot,
    absl::Status status) const {
  ProjectBuildAttemptResult result;
  result.attempt_id = request.attempt_id;
  result.origin = request.origin;
  result.saved_input = request.saved_input;
  result.output_log = snapshot.output;
  result.output_tail = snapshot.output_tail;
  result.process_status = std::move(status);
  result.terminal_state = snapshot.status.code() == absl::StatusCode::kCancelled
                              ? ProjectBuildTerminalState::kCancelled
                              : ProjectBuildTerminalState::kFailed;
  result.run_eligible = false;
  return result;
}

absl::Status ProjectManager::StartPreparedProjectBuild(
    const ProjectBuildPreparedRequest& request, bool synchronous) {
  RETURN_IF_ERROR(ValidatePreparedBuildDoesNotOverlap(request));
  RETURN_IF_ERROR(ValidatePreparedBuildOriginBinding(request, true));
  if (active_project_build_) {
    // A child can finish between UI polls. Preserve its result before replacing
    // the task; its failure must not prevent a later valid attempt.
    (void)FinalizeActiveProjectBuild(false);
  }
  RevokeRunEligibilityForOwner(request.origin.session_id,
                               request.origin.project_descriptor_path);
  active_build_request_ = request;
  active_project_build_ = std::make_unique<BackgroundCommandTask>();
  auto start_status = active_project_build_->Start(
      request.command, request.project_root, request.environment);
  if (!start_status.ok()) {
    const auto snapshot = active_project_build_->GetSnapshot();
    const auto failure =
        MakeTerminalBuildFailure(request, snapshot, start_status);
    terminal_build_results_[ProjectBuildResultKey(
        request.origin.session_id, request.origin.project_descriptor_path)] =
        failure;
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
  result.run_eligible = false;

  if (snapshot.status.code() == absl::StatusCode::kCancelled) {
    result.terminal_state = ProjectBuildTerminalState::kCancelled;
    return result;
  }
  if (!snapshot.status.ok()) {
    result.terminal_state = ProjectBuildTerminalState::kFailed;
    return result;
  }

  if (const auto origin_status =
          ValidatePreparedBuildOriginBinding(request, false);
      !origin_status.ok()) {
    result.terminal_state = ProjectBuildTerminalState::kFailed;
    result.process_status = origin_status;
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

  project::YazeProject live_project;
  if (const auto load_status =
          live_project.Open(request.origin.project_descriptor_path);
      !load_status.ok()) {
    result.terminal_state = ProjectBuildTerminalState::kFailed;
    result.process_status = load_status;
    return result;
  }
  if (!live_project.lyra_build_request_file.empty()) {
    ASSIGN_OR_RETURN(const auto expanded,
                     live_project.ResolveExpandedBuildRunTarget(
                         request.origin.editing_source_path));
    if (!expanded || *expanded != request.resolved_output_path) {
      result.terminal_state = ProjectBuildTerminalState::kFailed;
      result.process_status = absl::FailedPreconditionError(
          "Build output does not match the expanded run target contract");
      return result;
    }
  }

  if (request.output_existed_at_prepare &&
      result.output_sha256 == request.output_preimage_sha256 &&
      result.output_size == request.output_preimage_size) {
    const std::string key = ProjectBuildResultKey(
        request.origin.session_id, request.origin.project_descriptor_path);
    const auto prior = verified_build_provenance_.find(key);
    const bool deterministic_rebuild =
        prior != verified_build_provenance_.end() &&
        prior->second.saved_input.fingerprint_sha256 ==
            request.saved_input.fingerprint_sha256 &&
        prior->second.output_path == request.resolved_output_path &&
        prior->second.output_sha256 == result.output_sha256 &&
        prior->second.output_size == result.output_size;
    if (!deterministic_rebuild &&
        live_project.lyra_build_request_file.empty()) {
      result.terminal_state = ProjectBuildTerminalState::kFailed;
      result.process_status = absl::FailedPreconditionError(
          "Build completed, but unchanged output cannot be verified for these "
          "saved inputs");
      return result;
    }
  }

  result.terminal_state = ProjectBuildTerminalState::kSucceeded;
  result.run_eligible = true;
  verified_build_provenance_[ProjectBuildResultKey(
      request.origin.session_id, request.origin.project_descriptor_path)] =
      result;
  return result;
}

absl::Status ProjectManager::FinalizeActiveProjectBuild(bool synchronous_wait) {
  if (!active_project_build_ || !active_build_request_.has_value()) {
    return absl::FailedPreconditionError("No active project build");
  }
  absl::Status wait_status = absl::OkStatus();
  if (synchronous_wait) {
    wait_status = active_project_build_->Wait();
  }
  const auto snapshot = active_project_build_->GetSnapshot();
  if (snapshot.running) {
    return wait_status;
  }
  const ProjectBuildPreparedRequest request = *active_build_request_;
  const std::string key = ProjectBuildResultKey(
      request.origin.session_id, request.origin.project_descriptor_path);
  ProjectBuildAttemptResult result;
  if (!wait_status.ok()) {
    result = MakeTerminalBuildFailure(request, snapshot, wait_status);
  } else {
    auto finished_or = CompleteBuildAttempt(request, snapshot);
    if (!finished_or.ok()) {
      result =
          MakeTerminalBuildFailure(request, snapshot, finished_or.status());
    } else {
      result = *finished_or;
    }
  }
  terminal_build_results_[key] = result;
  pending_owner_completions_[key].push_back(result);
  active_project_build_.reset();
  active_build_request_.reset();
  return result.process_status;
}

ProjectBuildPollUpdate ProjectManager::PollProjectBuildForViewer(
    size_t viewer_session_id, const std::string& viewer_descriptor_path) {
  ProjectBuildPollUpdate update;
  const std::string viewer_key =
      ProjectBuildResultKey(viewer_session_id, viewer_descriptor_path);
  if (const auto pending = pending_owner_completions_.find(viewer_key);
      pending != pending_owner_completions_.end()) {
    update.newly_finished = true;
    update.finished = pending->second.front();
    update.owner_session_id = update.finished.origin.session_id;
    update.owner_last_result = update.finished;
    pending->second.pop_front();
    if (pending->second.empty()) {
      pending_owner_completions_.erase(pending);
    }
    return update;
  }
  if (const auto it = terminal_build_results_.find(viewer_key);
      it != terminal_build_results_.end()) {
    update.owner_last_result = it->second;
  }
  if (!active_project_build_ || !active_build_request_) {
    return update;
  }
  update.task_snapshot = active_project_build_->GetSnapshot();
  if (!update.task_snapshot.running) {
    // Finalization queues a terminal event for its owner. A second observation
    // cannot recurse again because finalization releases the active task.
    (void)FinalizeActiveProjectBuild(false);
    return PollProjectBuildForViewer(viewer_session_id, viewer_descriptor_path);
  }
  update.has_active_operation = true;
  update.owner_session_id = active_build_request_->origin.session_id;
  update.attempt_id = active_build_request_->attempt_id;
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
  for (auto it = verified_build_provenance_.begin();
       it != verified_build_provenance_.end();) {
    if (it->second.origin.project_descriptor_path == descriptor_path) {
      it = verified_build_provenance_.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = pending_owner_completions_.begin();
       it != pending_owner_completions_.end();) {
    if (it->second.front().origin.project_descriptor_path == descriptor_path) {
      it = pending_owner_completions_.erase(it);
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

std::optional<ProjectBuildAttemptResult> ProjectManager::GetBuildStatusForOwner(
    size_t owner_session_id, const std::string& descriptor_path) const {
  if (active_build_request_ &&
      active_build_request_->origin.session_id == owner_session_id &&
      ProjectBuildResultKey(owner_session_id, descriptor_path) ==
          ProjectBuildResultKey(
              owner_session_id,
              active_build_request_->origin.project_descriptor_path)) {
    ProjectBuildAttemptResult result;
    result.attempt_id = active_build_request_->attempt_id;
    result.origin = active_build_request_->origin;
    result.terminal_state = ProjectBuildTerminalState::kRunning;
    return result;
  }
  return GetLastBuildResultForOwner(owner_session_id, descriptor_path);
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
  if (inputs.validate_readiness) {
    RETURN_IF_ERROR(inputs.validate_readiness());
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

  if (!last_result.has_value() && !project.lyra_build_request_file.empty()) {
    // The resolver above has authenticated the existing expanded receipt and
    // its current saved generation. A local failed/cancelled attempt prevents
    // this external-adoption path until a new successful build.
    authorized = true;
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
