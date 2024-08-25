#ifndef YAZE_TEST_INTEGRATION_TEST_EDITOR_H
#define YAZE_TEST_INTEGRATION_TEST_EDITOR_H

#include "app/editor/utils/editor.h"
#include "imgui/imgui.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"

namespace yaze {
namespace test {
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

  absl::Status Update() override;

  void RegisterTests(ImGuiTestEngine* engine);

 private:
  ImGuiTestEngine* engine_;
};

int RunIntegrationTest();

}  // namespace integration
}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_INTEGRATION_TEST_EDITOR_H