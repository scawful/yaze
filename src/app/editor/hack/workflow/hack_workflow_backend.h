#ifndef YAZE_APP_EDITOR_HACK_WORKFLOW_HACK_WORKFLOW_BACKEND_H_
#define YAZE_APP_EDITOR_HACK_WORKFLOW_HACK_WORKFLOW_BACKEND_H_

#include <optional>
#include <string>

#include "absl/status/statusor.h"
#include "app/editor/oracle/panels/oracle_validation_view_model.h"
#include "core/oracle_progression.h"

namespace yaze {
class Rom;

namespace core {
class HackManifest;
class StoryEventGraph;
}  // namespace core

namespace emu::mesen {
class MesenSocketClient;
}  // namespace emu::mesen

namespace project {
struct YazeProject;
}  // namespace project

namespace editor::workflow {

class ValidationCapability {
 public:
  virtual ~ValidationCapability() = default;

  virtual oracle_validation::OracleRunResult RunValidation(
      oracle_validation::RunMode mode,
      const oracle_validation::SmokeOptions& smoke_options,
      const oracle_validation::PreflightOptions& preflight_options,
      Rom* rom_context) const = 0;
};

class ProgressionCapability {
 public:
  virtual ~ProgressionCapability() = default;

  virtual std::optional<core::OracleProgressionState> GetProgressionState(
      const core::HackManifest& manifest) const = 0;
  virtual void SetProgressionState(
      core::HackManifest& manifest,
      const core::OracleProgressionState& state) const = 0;
  virtual void ClearProgressionState(core::HackManifest& manifest) const = 0;

  virtual absl::StatusOr<core::OracleProgressionState>
  LoadProgressionStateFromFile(const std::string& filepath) const = 0;
  virtual absl::StatusOr<core::OracleProgressionState>
  ReadProgressionStateFromLiveSram(
      emu::mesen::MesenSocketClient& client) const = 0;
};

class PlanningCapability {
 public:
  virtual ~PlanningCapability() = default;

  virtual const core::StoryEventGraph* GetStoryGraph(
      const core::HackManifest& manifest) const = 0;
};

class HackWorkflowBackend {
 public:
  virtual ~HackWorkflowBackend() = default;

  virtual std::string GetBackendId() const = 0;

  virtual core::HackManifest* ResolveManifest(
      project::YazeProject* project) const = 0;
};

}  // namespace editor::workflow
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_HACK_WORKFLOW_HACK_WORKFLOW_BACKEND_H_
