#include "test_utils.h"
#include "app/core/controller.h"

namespace yaze {
namespace test {
namespace gui {

void LoadRomInTest(ImGuiTestContext* ctx, const std::string& rom_path) {
    yaze::core::Controller* controller = (yaze::core::Controller*)ctx->Test->UserData;
    controller->OnEntry(rom_path);
}

void OpenEditorInTest(ImGuiTestContext* ctx, const std::string& editor_name) {
    ctx->MenuClick(absl::StrFormat("Editors/%s", editor_name).c_str());
}

} // namespace gui
} // namespace test
} // namespace yaze
