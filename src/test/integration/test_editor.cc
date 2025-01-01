#include "test/integration/test_editor.h"

#include <SDL.h>

#include "app/core/controller.h"
#include "app/core/platform/renderer.h"
#include "app/gui/style.h"
#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_sdlrenderer2.h"
#include "imgui/imgui.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"
#include "imgui_test_engine/imgui_te_imconfig.h"
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

int RunIntegrationTest() {
  yaze::test::integration::TestEditor test_editor;
  yaze::core::Controller controller;
  controller.init_test_editor(&test_editor);

  if (!controller.CreateWindow().ok()) {
    return EXIT_FAILURE;
  }
  if (!controller.CreateRenderer().ok()) {
    return EXIT_FAILURE;
  }
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  // Initialize Test Engine
  ImGuiTestEngine* engine = ImGuiTestEngine_CreateContext();
  ImGuiTestEngineIO& test_io = ImGuiTestEngine_GetIO(engine);
  test_io.ConfigVerboseLevel = ImGuiTestVerboseLevel_Info;
  test_io.ConfigVerboseLevelOnError = ImGuiTestVerboseLevel_Debug;

  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  // Initialize ImGui for SDL
  ImGui_ImplSDL2_InitForSDLRenderer(
      controller.window(), yaze::core::Renderer::GetInstance().renderer());
  ImGui_ImplSDLRenderer2_Init(yaze::core::Renderer::GetInstance().renderer());

  test_editor.RegisterTests(engine);
  ImGuiTestEngine_Start(engine, ImGui::GetCurrentContext());
  controller.set_active(true);

  // Set the default style
  yaze::gui::ColorsYaze();

  // Build a new ImGui frame
  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();

  while (controller.IsActive()) {
    controller.OnInput();
    if (const auto status = controller.OnTestLoad(); !status.ok()) {
      return EXIT_FAILURE;
    }
    controller.DoRender();
  }

  ImGuiTestEngine_Stop(engine);
  controller.OnExit();
  return EXIT_SUCCESS;
}

}  // namespace integration
}  // namespace test
}  // namespace yaze
