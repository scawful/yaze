#include "master_editor.h"

namespace yaze {
namespace app {
namespace editor {

void MasterEditor::RegisterTests(ImGuiTestEngine* e) {
  test_engine = e;
  ImGuiTest* t = nullptr;

  t = IM_REGISTER_TEST(e, "master_editor", "open_rom");
  t->GuiFunc = [](ImGuiTestContext* ctx) {
    IM_UNUSED(ctx);
    ImGui::Begin("Test Window", nullptr, ImGuiWindowFlags_NoSavedSettings);
    ImGui::Text("Hello, automation world");
    ImGui::Button("Click Me");
    if (ImGui::TreeNode("Node")) {
      static bool b = false;
      ImGui::Checkbox("Checkbox", &b);
      ImGui::TreePop();
    }
    ImGui::End();
  };
  t->TestFunc = [](ImGuiTestContext* ctx) {
    ctx->SetRef("Test Window");
    ctx->ItemClick("Click Me");
    ctx->ItemOpen("Node");  // Optional as ItemCheck("Node/Checkbox") can do it
    ctx->ItemCheck("Node/Checkbox");
    ctx->ItemUncheck("Node/Checkbox");
  };

  t = IM_REGISTER_TEST(e, "master_editor", "open_metrics");
  t->GuiFunc = [](ImGuiTestContext* ctx) {
    IM_UNUSED(ctx);
    ImGui::ShowMetricsWindow();
  };
  t->TestFunc = [](ImGuiTestContext* ctx) {
    ctx->SetRef("Dear ImGui Metrics");
  };
}

}  // namespace editor
}  // namespace app
}  // namespace yaze