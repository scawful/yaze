#include "app/editor/hack/workflow/manifest_only_hack_workflow_backend.h"

#include "absl/status/status.h"
#include "core/hack_manifest.h"
#include "core/project.h"

namespace yaze::editor::workflow {

std::string ManifestOnlyHackWorkflowBackend::GetBackendId() const {
  return "manifest";
}

core::HackManifest* ManifestOnlyHackWorkflowBackend::ResolveManifest(
    project::YazeProject* project) const {
  if (project != nullptr && project->hack_manifest.loaded()) {
    return &project->hack_manifest;
  }
  return nullptr;
}

const core::StoryEventGraph* ManifestOnlyHackWorkflowBackend::GetStoryGraph(
    const core::HackManifest& manifest) const {
  if (!manifest.HasProjectRegistry() ||
      !manifest.project_registry().story_events.loaded()) {
    return nullptr;
  }
  return &manifest.project_registry().story_events;
}

}  // namespace yaze::editor::workflow
