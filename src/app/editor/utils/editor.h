#ifndef YAZE_APP_CORE_EDITOR_H
#define YAZE_APP_CORE_EDITOR_H

#include "absl/status/status.h"

namespace yaze {
namespace app {

/**
 * @namespace yaze::app::editor
 * @brief Editors are the view controllers for the application.
 */
namespace editor {

/**
 * @class Editor
 * @brief Interface for editor classes.
 *
 * Provides basic editing operations that each editor should implement.
 */
class Editor {
 public:
  Editor() = default;
  virtual ~Editor() = default;

  virtual absl::Status Cut() = 0;
  virtual absl::Status Copy() = 0;
  virtual absl::Status Paste() = 0;

  virtual absl::Status Undo() = 0;
  virtual absl::Status Redo() = 0;

  virtual absl::Status Update() = 0;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_CORE_EDITOR_H