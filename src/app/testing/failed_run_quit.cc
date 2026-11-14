#include "app/testing/failed_run_quit.h"

#include <system_error>
#include <utility>

#include "app/editor/editor_manager.h"
#include "app/editor/registry/content_registry.h"
#include "app/editor/system/session/project_workflow_status.h"
#include "app/editor/system/session/session_coordinator.h"
#include "core/project.h"
#include "util/log.h"

namespace yaze::test {
namespace {

namespace fs = std::filesystem;

// Quit closes every session, so the failed run may request it only while the
// verified fixture session is the only session and still holds the project.
bool FixtureOwnsEverySession(editor::EditorManager& manager,
                             const FixtureOwner& owner) {
  const auto* sessions = manager.session_coordinator();
  const auto* project = manager.GetCurrentProject();
  std::error_code ec;
  return sessions && sessions->GetTotalSessionCount() == 1 &&
         sessions->GetSessionId(0) == owner.session_id &&
         manager.GetCurrentSessionId() == owner.session_id && project &&
         project->project_opened() &&
         fs::equivalent(project->filepath, owner.project, ec) && !ec;
}

bool WorkflowRunning() {
  namespace Context = editor::ContentRegistry::Context;
  return Context::build_workflow_status().state ==
             editor::ProjectWorkflowState::kRunning ||
         Context::run_workflow_status().state ==
             editor::ProjectWorkflowState::kRunning;
}

}  // namespace

FailedRunQuit RequestFailedRunQuit(editor::EditorManager& manager,
                                   const std::optional<FixtureOwner>& owner) {
  if (!owner || !FixtureOwnsEverySession(manager, *owner)) {
    LOG_WARN("GraphicsSourceUITest",
             "Failed run has no verified fixture ownership; Quit left to the "
             "owner");
    return FailedRunQuit::kLeftToOwner;
  }
  // Quit replaces any pending guard, so an existing one is left untouched.
  if (manager.HasPendingUnsavedSessionAction()) {
    LOG_WARN("GraphicsSourceUITest",
             "Failed run found a pending guard; Quit left to the owner");
    return FailedRunQuit::kLeftToOwner;
  }
  if (WorkflowRunning()) {
    return FailedRunQuit::kWaitingForWorkflow;
  }
  manager.Quit();
  if (manager.quit()) {
    return FailedRunQuit::kRequested;
  }
  // A failed run never Saves or Discards session work. Withdraw the guard
  // this Quit raised so the draft stays open for the owner.
  manager.CancelPendingUnsavedSessionAction();
  LOG_WARN("GraphicsSourceUITest",
           "Failed run kept unsaved work; Quit left to the owner");
  return FailedRunQuit::kLeftToOwner;
}

void QueueFailedRunQuit(editor::EditorManager* manager,
                        std::optional<FixtureOwner> owner) {
  manager->QueueDeferredAction([manager, owner = std::move(owner)] {
    if (RequestFailedRunQuit(*manager, owner) ==
        FailedRunQuit::kWaitingForWorkflow) {
      QueueFailedRunQuit(manager, owner);
    }
  });
}

}  // namespace yaze::test
