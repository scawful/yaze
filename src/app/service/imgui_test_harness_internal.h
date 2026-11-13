#ifndef YAZE_APP_SERVICE_IMGUI_TEST_HARNESS_INTERNAL_H_
#define YAZE_APP_SERVICE_IMGUI_TEST_HARNESS_INTERNAL_H_

#ifdef YAZE_WITH_GRPC

#include <cstddef>
#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {
class WindowHost;
}
namespace test {

enum class HarnessTestStatus;
struct HarnessTestExecution;

namespace internal {

// Empty selects Cancel. Nonempty destinations must be new absolute .yaze paths.
absl::Status ValidateProjectSaveAsDestination(const char* destination);

// Use a registered base ID in the active session, never a guessed ImGui label.
absl::StatusOr<std::string> FocusHarnessWindow(editor::WindowHost* host,
                                               size_t session_id,
                                               absl::string_view window_id);
// Call after a UI frame: dispatching focus alone does not prove tab selection.
absl::Status VerifyHarnessWindowFocus(absl::string_view window_name);

struct ParsedTarget {
  std::string type;
  std::string label;
};

struct ResolvedWidgetSelector {
  ParsedTarget target;
  std::string resolved_widget_key;
  std::string resolved_path;
  ImGuiID imgui_id = 0;
};

// Resolve a stable registry key to both its descriptive metadata and exact
// ImGui identifier. Keeping the identifier is what makes duplicate labels
// safe for harness actions.
absl::StatusOr<ResolvedWidgetSelector> ResolveWidgetSelector(
    absl::string_view target, absl::string_view widget_key);

// Replay RPC responses describe queue acceptance, while the execution record
// describes the terminal action result. Always let the latter win.
void ApplyTerminalHarnessExecution(const HarnessTestExecution& execution,
                                   bool* step_success,
                                   std::string* step_message);

}  // namespace internal
}  // namespace test
}  // namespace yaze

#endif  // YAZE_WITH_GRPC
#endif  // YAZE_APP_SERVICE_IMGUI_TEST_HARNESS_INTERNAL_H_
