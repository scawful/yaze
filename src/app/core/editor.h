#ifndef YAZE_APP_CORE_EDITOR_H
#define YAZE_APP_CORE_EDITOR_H

#include "absl/status/status.h"

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

#endif  // YAZE_APP_CORE_EDITOR_H