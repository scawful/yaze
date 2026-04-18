#ifndef YAZE_APP_EDITOR_ORACLE_ORACLE_HACK_WORKFLOW_BACKEND_H_
#define YAZE_APP_EDITOR_ORACLE_ORACLE_HACK_WORKFLOW_BACKEND_H_

#include "app/editor/hack/workflow/hack_workflow_backend.h"

namespace yaze::editor {

class OracleHackWorkflowBackend : public workflow::HackWorkflowBackend,
                                  public workflow::ValidationCapability,
                                  public workflow::ProgressionCapability,
                                  public workflow::PlanningCapability {
 public:
  std::string GetBackendId() const override;

  oracle_validation::OracleRunResult RunValidation(
      oracle_validation::RunMode mode,
      const oracle_validation::SmokeOptions& smoke_options,
      const oracle_validation::PreflightOptions& preflight_options,
      Rom* rom_context) const override;

  core::HackManifest* ResolveManifest(project::YazeProject* project) const override;

  std::optional<core::OracleProgressionState> GetProgressionState(
      const core::HackManifest& manifest) const override;
  void SetProgressionState(
      core::HackManifest& manifest,
      const core::OracleProgressionState& state) const override;
  void ClearProgressionState(core::HackManifest& manifest) const override;

  absl::StatusOr<core::OracleProgressionState> LoadProgressionStateFromFile(
      const std::string& filepath) const override;
  absl::StatusOr<core::OracleProgressionState> ReadProgressionStateFromLiveSram(
      emu::mesen::MesenSocketClient& client) const override;

  const core::StoryEventGraph* GetStoryGraph(
      const core::HackManifest& manifest) const override;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_ORACLE_ORACLE_HACK_WORKFLOW_BACKEND_H_
