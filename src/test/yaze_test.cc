#define SDL_MAIN_HANDLED

#include <SDL.h>
#include <gtest/gtest.h>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "app/core/controller.h"
#include "app/core/platform/renderer.h"
#include "app/gui/style.h"
#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_sdlrenderer2.h"
#include "imgui/imgui.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"
#include "imgui_test_engine/imgui_te_imconfig.h"
#include "test/integration/test_editor.h"

int main(int argc, char* argv[]) {
  absl::InitializeSymbolizer(argv[0]);

  absl::FailureSignalHandlerOptions options;
  absl::InstallFailureSignalHandler(options);

  // Check if the argument says `integration`
  if (argc > 1 && std::string(argv[1]) == "integration") {
    // Support the ability to launch an integration test window.
    // SDL_SetMainReady();
    yaze::test::integration::TestEditor test_editor;
    yaze::app::core::Controller controller;
    controller.init_test_editor(&test_editor);

    if (!controller.CreateSDL_Window().ok()) {
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
        controller.window(),
        yaze::app::core::Renderer::GetInstance().renderer());
    ImGui_ImplSDLRenderer2_Init(
        yaze::app::core::Renderer::GetInstance().renderer());

    test_editor.RegisterTests(engine);
    ImGuiTestEngine_Start(engine, ImGui::GetCurrentContext());
    controller.set_active(true);

    // Set the default style
    yaze::app::gui::ColorsYaze();

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

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}