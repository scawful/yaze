#include "test_utils.h"

#include "app/controller.h"
#include "app/editor/editor_manager.h"
#include "rom/rom.h"

namespace yaze {
namespace test {
namespace gui {

void LoadRomInTest(ImGuiTestContext* ctx, const std::string& rom_path) {
  yaze::Controller* controller = (yaze::Controller*)ctx->Test->UserData;
  if (!controller) {
    ctx->LogError("LoadRomInTest: Controller is null!");
    return;
  }

  // Check if ROM is already loaded
  Rom* rom = controller->GetCurrentRom();
  if (rom && rom->is_loaded()) {
    ctx->LogInfo("ROM already loaded, skipping...");
    return;
  }

  // Use LoadRomForTesting which performs the full initialization:
  // 1. Load ROM file into session
  // 2. ConfigureEditorDependencies()
  // 3. LoadAssets() - initializes all editors and loads graphics
  // 4. Updates UI state (hides welcome screen, etc.)
  auto status = controller->LoadRomForTesting(rom_path);
  if (!status.ok()) {
    ctx->LogError("LoadRomInTest: Failed to load ROM: %s",
                  std::string(status.message()).c_str());
    return;
  }

  ctx->LogInfo("ROM loaded successfully with full initialization: %s",
               rom_path.c_str());
  ctx->Yield(10);  // Give more time for asset loading and UI updates
}

void OpenEditorInTest(ImGuiTestContext* ctx, const std::string& editor_name) {
  // Editors are under the "View" menu in yaze's menu structure
  // See: src/app/editor/system/menu_orchestrator.cc BuildViewMenu()
  ctx->MenuClick(absl::StrFormat("View/%s", editor_name).c_str());
}

}  // namespace gui
}  // namespace test
}  // namespace yaze
