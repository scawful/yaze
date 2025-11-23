#include "test_utils.h"

#include "app/controller.h"
#include "app/editor/editor_manager.h"
#include "app/rom.h"

namespace yaze {
namespace test {
namespace gui {

void LoadRomInTest(ImGuiTestContext* ctx, const std::string& rom_path) {
  yaze::Controller* controller = (yaze::Controller*)ctx->Test->UserData;
  if (!controller) {
    ctx->LogError("LoadRomInTest: Controller is null!");
    return;
  }

  // Get the ROM from the editor manager and load it directly
  Rom* rom = controller->GetCurrentRom();
  if (!rom) {
    ctx->LogError("LoadRomInTest: ROM object is null!");
    return;
  }

  // Check if ROM is already loaded
  if (rom->is_loaded()) {
    ctx->LogInfo("ROM already loaded, skipping...");
    return;
  }

  // Load the ROM file directly
  auto status = rom->LoadFromFile(rom_path);
  if (!status.ok()) {
    ctx->LogError("LoadRomInTest: Failed to load ROM: %s",
                  std::string(status.message()).c_str());
    return;
  }

  ctx->LogInfo("ROM loaded successfully: %s", rom_path.c_str());
  ctx->Yield(5);  // Give time for UI to update
}

void OpenEditorInTest(ImGuiTestContext* ctx, const std::string& editor_name) {
  ctx->MenuClick(absl::StrFormat("Editors/%s", editor_name).c_str());
}

}  // namespace gui
}  // namespace test
}  // namespace yaze
