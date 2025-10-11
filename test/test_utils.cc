#include "test_utils.h"
#include "app/core/controller.h"

namespace yaze {
namespace test {
namespace gui {

void LoadRomInTest(ImGuiTestContext* ctx, const std::string& rom_path) {
#ifdef YAZE_ENABLE_IMGUI_TEST_ENGINE
    yaze::core::Controller* controller = (yaze::core::Controller*)ctx->Test->UserData;
    controller->OnEntry(rom_path);
#endif
}

void OpenEditorInTest(ImGuiTestContext* ctx, const std::string& editor_name) {
#ifdef YAZE_ENABLE_IMGUI_TEST_ENGINE
    ctx->MenuClick(absl::StrFormat("Editors/%s", editor_name).c_str());
#endif
}

} // namespace gui
} // namespace test
} // namespace yaze
