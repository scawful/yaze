#ifndef YAZE_APP_EDITOR_SHELL_DIALOGS_NEW_PROJECT_DIALOG_H_
#define YAZE_APP_EDITOR_SHELL_DIALOGS_NEW_PROJECT_DIALOG_H_

#include <functional>
#include <string>

namespace yaze {
namespace editor {

// Guided "New Project" modal. Replaces the old one-click template -> ROM
// dialog chain with a single surface that captures the three inputs project
// creation actually needs: template choice, source ROM, and a human-readable
// project name. Lives inside the welcome-screen surface but is owned by
// UICoordinator so it survives welcome-screen teardown during startup
// transitions.
class NewProjectDialog {
 public:
  // Invoked when the user hits "Create". All three strings are non-empty by
  // the time the callback fires; the dialog enforces it on the UI side.
  using CreateCallback = std::function<void(const std::string& template_name,
                                            const std::string& rom_path,
                                            const std::string& project_name)>;

  void SetCreateCallback(CreateCallback cb) {
    create_callback_ = std::move(cb);
  }

  // Open the modal with a preselected template. `initial_template` should
  // match one of the display names in templates_table_[] (e.g. "Vanilla ROM
  // Hack"); empty defaults to the first entry.
  void Open(const std::string& initial_template = "");

  // Render one frame. Returns true if the modal is currently visible.
  bool Draw();

  bool IsOpen() const { return open_requested_; }

 private:
  void ApplyTemplateSelection(const std::string& name);
  void Reset();

  CreateCallback create_callback_;

  bool open_requested_ = false;
  bool just_opened_ = false;

  int selected_template_ = 0;
  char rom_path_buffer_[1024] = {};
  char project_name_buffer_[128] = {};

  // Status line shown under the Create button — empty when the form is in a
  // clean state, populated when validation fails (missing ROM, bad name).
  std::string status_message_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SHELL_DIALOGS_NEW_PROJECT_DIALOG_H_
