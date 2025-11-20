#include "e2e/framework_smoke_test.h"
#include "imgui.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "test_utils.h"

// Smoke test for the E2E testing framework.
// This test is run by the `test-gui` command.
// It opens a window, clicks a button, and verifies that the button was clicked.
// The GUI for this test is rendered in `test/yaze_test.cc`.
void E2ETest_FrameworkSmokeTest(ImGuiTestContext* ctx) {
  yaze::test::gui::LoadRomInTest(ctx, "zelda3.sfc");
  ctx->SetRef("Hello World Window");
  ctx->ItemClick("Button");
  ctx->ItemCheck("Clicked 1 times");
}
