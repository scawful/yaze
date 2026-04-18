#ifndef YAZE_APP_EDITOR_HACK_WORKFLOW_MANIFEST_ONLY_HACK_WORKFLOW_BACKEND_H_
#define YAZE_APP_EDITOR_HACK_WORKFLOW_MANIFEST_ONLY_HACK_WORKFLOW_BACKEND_H_

#include "app/editor/hack/workflow/hack_workflow_backend.h"

namespace yaze::editor::workflow {

class ManifestOnlyHackWorkflowBackend : public HackWorkflowBackend,
                                        public PlanningCapability {
 public:
  std::string GetBackendId() const override;

  core::HackManifest* ResolveManifest(project::YazeProject* project) const override;

  const core::StoryEventGraph* GetStoryGraph(
      const core::HackManifest& manifest) const override;
};

}  // namespace yaze::editor::workflow

#endif  // YAZE_APP_EDITOR_HACK_WORKFLOW_MANIFEST_ONLY_HACK_WORKFLOW_BACKEND_H_
