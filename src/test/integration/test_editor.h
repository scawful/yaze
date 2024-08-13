#ifndef YAZE_TEST_INTEGRATION_TEST_EDITOR_H
#define YAZE_TEST_INTEGRATION_TEST_EDITOR_H

#include "app/editor/utils/editor.h"

namespace yaze_test {
namespace integration {

class TestEditor : public yaze::app::editor::Editor {
 public:
  TestEditor() = default;
  ~TestEditor() = default;

  absl::Status Cut() override {
    return absl::UnimplementedError("Not implemented");
  }
  absl::Status Copy() override {
    return absl::UnimplementedError("Not implemented");
  }
  absl::Status Paste() override {
    return absl::UnimplementedError("Not implemented");
  }

  absl::Status Undo() override {
    return absl::UnimplementedError("Not implemented");
  }
  absl::Status Redo() override {
    return absl::UnimplementedError("Not implemented");
  }

  absl::Status Find() override {
    return absl::UnimplementedError("Not implemented");
  }

  absl::Status Update() override {
    return absl::UnimplementedError("Not implemented");
  }
};

}  // namespace integration
}  // namespace yaze_test

#endif  // YAZE_TEST_INTEGRATION_TEST_EDITOR_H