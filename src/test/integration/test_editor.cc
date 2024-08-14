#include "test/integration/test_editor.h"

#include "imgui/imgui.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"
#include "imgui_test_engine/imgui_te_ui.h"

namespace yaze {
namespace test {
namespace integration {

absl::Status TestEditor::Update() {
  ImGui::NewFrame();
  if (ImGui::Begin("My Window")) {
    ImGui::Text("Hello, world!");
    ImGui::Button("My Button");
  }

  static bool show_demo_window = true;

  ImGuiTestEngine_ShowTestEngineWindows(engine_, &show_demo_window);

  ImGui::End();

  return absl::OkStatus();
}

void TestEditor::RegisterTests(ImGuiTestEngine* engine) {
  engine_ = engine;
  ImGuiTest* test = IM_REGISTER_TEST(engine, "demo_test", "test1");
  test->TestFunc = [](ImGuiTestContext* ctx) {
    ctx->SetRef("My Window");
    ctx->ItemClick("My Button");
    ctx->ItemCheck("Node/Checkbox");
  };
}

}  // namespace integration
}  // namespace test
}  // namespace yaze
