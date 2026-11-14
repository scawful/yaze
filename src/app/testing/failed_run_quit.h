#ifndef YAZE_APP_TESTING_FAILED_RUN_QUIT_H_
#define YAZE_APP_TESTING_FAILED_RUN_QUIT_H_

#include <cstddef>
#include <filesystem>
#include <optional>

namespace yaze::editor {
class EditorManager;
}  // namespace yaze::editor

namespace yaze::test {

// The session and project a gated run verified before it failed.
struct FixtureOwner {
  size_t session_id = 0;
  std::filesystem::path project;
};

enum class FailedRunQuit {
  kRequested,           // Quit is set; the native owner observes the exit.
  kWaitingForWorkflow,  // A Build or Run is still active; ask again later.
  kLeftToOwner,         // The app stays open for the native owner.
};

// Requests the normal Quit after a failed gated run. Never Saves or Discards
// session work, and never quits while a Build or Run is active.
FailedRunQuit RequestFailedRunQuit(editor::EditorManager& manager,
                                   const std::optional<FixtureOwner>& owner);

// Repeats RequestFailedRunQuit on later frames while it is waiting.
void QueueFailedRunQuit(editor::EditorManager* manager,
                        std::optional<FixtureOwner> owner);

}  // namespace yaze::test

#endif  // YAZE_APP_TESTING_FAILED_RUN_QUIT_H_
