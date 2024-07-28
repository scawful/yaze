#include "message_editor.h"

namespace yaze {
namespace app {
namespace editor {

void MessageEditor::RegisterTests(ImGuiTestEngine* e) {
  test_engine = e;
  ImGuiTest* t = nullptr;

  t = IM_REGISTER_TEST(e, "message_editor", "read_all_text_data");
  t->TestFunc = [](ImGuiTestContext* ctx) {
    ctx->SetRef("##YazeMain/##TabBar/Message");
    ctx->ItemClick("TestButton");

  };
}

}  // namespace editor
}  // namespace app
}  // namespace yaze
