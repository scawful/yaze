#include "app/editor/hack/workflow/hack_workflow_backend_factory.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>

#include "app/editor/hack/workflow/manifest_only_hack_workflow_backend.h"
#include "app/editor/oracle/oracle_hack_workflow_backend.h"
#include "core/project.h"

namespace yaze::editor::workflow {
namespace {

bool ContainsIgnoreCase(const std::string& haystack,
                        const std::string& needle) {
  if (needle.empty()) {
    return true;
  }
  auto it = std::search(haystack.begin(), haystack.end(), needle.begin(),
                        needle.end(), [](unsigned char a, unsigned char b) {
                          return std::tolower(a) == std::tolower(b);
                        });
  return it != haystack.end();
}

bool LooksLikeOracleProject(const project::YazeProject* project) {
  if (project == nullptr || !project->hack_manifest.loaded()) {
    return false;
  }

  // Capability-based first: any project carrying Oracle progression data is
  // by definition Oracle-compatible, regardless of what it's named. This
  // covers forks / renames that would previously silently drop to the
  // manifest-only backend.
  if (project->hack_manifest.oracle_progression_state().has_value()) {
    return true;
  }

  // Name-based fallback for projects that don't yet have progression state
  // populated (fresh manifest, early-stage scaffolding). Kept as a local
  // helper instead of absl::StrContainsIgnoreCase so older Abseil versions
  // on Linux CI can still compile this translation unit.
  return ContainsIgnoreCase(project->hack_manifest.hack_name(), "oracle");
}

}  // namespace

std::unique_ptr<HackWorkflowBackend> CreateHackWorkflowBackendForProject(
    const project::YazeProject* project) {
  if (LooksLikeOracleProject(project)) {
    return std::make_unique<OracleHackWorkflowBackend>();
  }
  return std::make_unique<ManifestOnlyHackWorkflowBackend>();
}

}  // namespace yaze::editor::workflow
