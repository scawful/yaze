#ifndef YAZE_TEST_INTEGRATION_TEST_EDITOR_H
#define YAZE_TEST_INTEGRATION_TEST_EDITOR_H

#include "app/editor/editor.h"

#ifdef IMGUI_ENABLE_TEST_ENGINE
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"
#endif

namespace yaze {
namespace test {

class TestEditor : public yaze::editor::Editor {
 public:
  TestEditor() = default;
  ~TestEditor() = default;
  void Initialize() override {}

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

  absl::Status Save() override {
    return absl::UnimplementedError("Not implemented");
  }
  absl::Status Load() override {
    return absl::UnimplementedError("Not implemented");
  }

#ifdef IMGUI_ENABLE_TEST_ENGINE
  void RegisterTests(ImGuiTestEngine* engine);
#endif

 private:
#ifdef IMGUI_ENABLE_TEST_ENGINE
  ImGuiTestEngine* engine_;
#else
  void* engine_; // Placeholder when test engine is disabled
#endif
};

int RunIntegrationTest();

}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_INTEGRATION_TEST_EDITOR_H