#include "test_editor.h"

#include <SDL.h>

#include "app/core/controller.h"
#include "app/core/window.h"
#include "app/gui/style.h"
#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_sdlrenderer2.h"
#include "imgui/imgui.h"

#ifdef IMGUI_ENABLE_TEST_ENGINE
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"
#include "imgui_test_engine/imgui_te_imconfig.h"
#include "imgui_test_engine/imgui_te_ui.h"
#endif

namespace yaze {
namespace test {

absl::Status TestEditor::Update() {
  ImGui::NewFrame();
  if (ImGui::Begin("My Window")) {
    ImGui::Text("Hello, world!");
    ImGui::Button("My Button");
  }

  static bool show_demo_window = true;

#ifdef IMGUI_ENABLE_TEST_ENGINE
  ImGuiTestEngine_ShowTestEngineWindows(engine_, &show_demo_window);
#else
  ImGui::Text("ImGui Test Engine not available in this build");
  (void)show_demo_window; // Suppress unused variable warning
#endif

  ImGui::End();

  return absl::OkStatus();
}

#ifdef IMGUI_ENABLE_TEST_ENGINE
void TestEditor::RegisterTests(ImGuiTestEngine* engine) {
  engine_ = engine;
  ImGuiTest* test = IM_REGISTER_TEST(engine, "demo_test", "test1");
  test->TestFunc = [](ImGuiTestContext* ctx) {
    ctx->SetRef("My Window");
    ctx->ItemClick("My Button");
    ctx->ItemCheck("Node/Checkbox");
  };
}
#endif

// TODO: Fix the window/controller management
int RunIntegrationTest() {
  yaze::core::Controller controller;
  yaze::core::Window window;
  yaze::core::CreateWindow(window, SDL_WINDOW_RESIZABLE);
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  // Initialize Test Engine (if available)
#ifdef IMGUI_ENABLE_TEST_ENGINE
  ImGuiTestEngine* engine = ImGuiTestEngine_CreateContext();
  ImGuiTestEngineIO& test_io = ImGuiTestEngine_GetIO(engine);
  test_io.ConfigVerboseLevel = ImGuiTestVerboseLevel_Info;
  test_io.ConfigVerboseLevelOnError = ImGuiTestVerboseLevel_Debug;
#else
  void* engine = nullptr; // Placeholder when test engine is disabled
#endif

  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  // Initialize ImGui for SDL
  ImGui_ImplSDL2_InitForSDLRenderer(
      controller.window(), yaze::core::Renderer::Get().renderer());
  ImGui_ImplSDLRenderer2_Init(yaze::core::Renderer::Get().renderer());

  yaze::test::TestEditor test_editor;
#ifdef IMGUI_ENABLE_TEST_ENGINE
  test_editor.RegisterTests(engine);
  ImGuiTestEngine_Start(engine, ImGui::GetCurrentContext());
#endif
  controller.set_active(true);

  // Set the default style
  yaze::gui::ColorsYaze();

  // Build a new ImGui frame
  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();

  while (controller.IsActive()) {
    controller.OnInput();
    auto status = test_editor.Update();
    if (!status.ok()) {
      return EXIT_FAILURE;
    }
    controller.DoRender();
  }

#ifdef IMGUI_ENABLE_TEST_ENGINE
  ImGuiTestEngine_Stop(engine);
#endif
  controller.OnExit();
  return EXIT_SUCCESS;
}

}  // namespace test
}  // namespace yaze
