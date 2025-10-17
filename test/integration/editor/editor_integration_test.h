#ifndef YAZE_TEST_EDITOR_INTEGRATION_TEST_H
#define YAZE_TEST_EDITOR_INTEGRATION_TEST_H

#define IMGUI_DEFINE_MATH_OPERATORS

#include "imgui/imgui.h"
#include "app/editor/editor.h"
#include "app/rom.h"
#include "app/controller.h"
#include "app/platform/window.h"
#include "app/gfx/backend/sdl2_renderer.h"

#ifdef YAZE_ENABLE_IMGUI_TEST_ENGINE
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"
#endif

namespace yaze {
namespace test {

/**
 * @class EditorIntegrationTest
 * @brief Base class for editor integration tests
 * 
 * This class provides common functionality for testing editors in the application.
 * It sets up the test environment and provides helper methods for ROM operations.
 * 
 * For UI interaction testing, use the ImGui test engine API directly within your test functions:
 * 
 * ImGuiTest* test = IM_REGISTER_TEST(engine, "test_suite", "test_name");
 * test->TestFunc = [](ImGuiTestContext* ctx) {
 *   ctx->SetRef("Window Name");
 *   ctx->ItemClick("Button Name");
 * };
 */
class EditorIntegrationTest {
 public:
  EditorIntegrationTest();
  ~EditorIntegrationTest();

  // Initialize the test environment
  absl::Status Initialize();
  
  // Run the test
  int RunTest();
  
#ifdef YAZE_ENABLE_IMGUI_TEST_ENGINE
  // Register tests for a specific editor
  virtual void RegisterTests(ImGuiTestEngine* engine) = 0;
#else
  // Default implementation when ImGui Test Engine is disabled
  virtual void RegisterTests(void* engine) {}
#endif
  
  // Update the test environment
  virtual absl::Status Update();

 protected:
  
  // Helper methods for testing with a ROM
  absl::Status LoadTestRom(const std::string& filename);
  absl::Status SaveTestRom(const std::string& filename);
  
  // Helper methods for testing with a specific editor
  absl::Status TestEditorInitialize(editor::Editor* editor);
  absl::Status TestEditorLoad(editor::Editor* editor);
  absl::Status TestEditorSave(editor::Editor* editor);
  absl::Status TestEditorUpdate(editor::Editor* editor);
  absl::Status TestEditorCut(editor::Editor* editor);
  absl::Status TestEditorCopy(editor::Editor* editor);
  absl::Status TestEditorPaste(editor::Editor* editor);
  absl::Status TestEditorUndo(editor::Editor* editor);
  absl::Status TestEditorRedo(editor::Editor* editor);
  absl::Status TestEditorFind(editor::Editor* editor);
  absl::Status TestEditorClear(editor::Editor* editor);

 private:
  Controller controller_;
#ifdef YAZE_ENABLE_IMGUI_TEST_ENGINE
  ImGuiTestEngine* engine_;
  bool show_demo_window_;
#endif
  std::unique_ptr<Rom> test_rom_;
  core::Window window_;
  std::unique_ptr<gfx::SDL2Renderer> test_renderer_;
};

}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_EDITOR_INTEGRATION_TEST_H 