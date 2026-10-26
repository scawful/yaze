#include "app/editor/hack/workflow/hack_workflow_backend_factory.h"

#include <memory>

#include "app/editor/hack/workflow/manifest_only_hack_workflow_backend.h"
#include "app/editor/oracle/oracle_hack_workflow_backend.h"
#include "core/project.h"

namespace yaze::editor::workflow {
std::unique_ptr<HackWorkflowBackend> CreateHackWorkflowBackendForProject(
    const project::YazeProject* project) {
  if (project && project->IsOracleCompatible()) {
    return std::make_unique<OracleHackWorkflowBackend>();
  }
  return std::make_unique<ManifestOnlyHackWorkflowBackend>();
}

}  // namespace yaze::editor::workflow
