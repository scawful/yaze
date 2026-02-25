#ifndef YAZE_APP_EDITOR_SYSTEM_EDITOR_MANAGER_INTERFACES_H_
#define YAZE_APP_EDITOR_SYSTEM_EDITOR_MANAGER_INTERFACES_H_

#include "app/editor/editor.h"

namespace yaze {
class Rom;
}

namespace yaze::editor {

class RomSession;

/**
 * @brief Interface for session configuration.
 *
 * SessionCoordinator needs to configure new sessions and set the current
 * editor. This interface decouples it from the full EditorManager.
 */
class ISessionConfigurator {
 public:
  virtual ~ISessionConfigurator() = default;
  virtual void ConfigureSession(RomSession* session) = 0;
  virtual void SetCurrentEditor(Editor* editor) = 0;
};

/**
 * @brief Interface for editor selection and navigation.
 *
 * DashboardPanel needs to check ROM state, switch editors, and dismiss
 * the selection dialog. This interface decouples it from EditorManager.
 */
class IEditorSwitcher {
 public:
  virtual ~IEditorSwitcher() = default;
  virtual Rom* GetCurrentRom() const = 0;
  virtual void SwitchToEditor(EditorType type, bool force_visible = false,
                               bool from_dialog = false) = 0;
  virtual void DismissEditorSelection() = 0;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_SYSTEM_EDITOR_MANAGER_INTERFACES_H_
