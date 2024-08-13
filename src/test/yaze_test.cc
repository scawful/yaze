#define SDL_MAIN_HANDLED

#include <SDL.h>
#include <gtest/gtest.h>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "app/core/controller.h"
#include "imgui/imgui.h"
#include "imgui_test_engine/imgui_te_engine.h"
#include "test/integration/test_editor.h"

int main(int argc, char* argv[]) {
  absl::InitializeSymbolizer(argv[0]);

  absl::FailureSignalHandlerOptions options;
  absl::InstallFailureSignalHandler(options);

  // Support the ability to launch an integration test window.
  SDL_SetMainReady();

  // Check if the argument says `integration`
  if (argc > 1 && std::string(argv[1]) == "integration") {
    yaze_test::integration::TestEditor test_editor;
    yaze::app::core::Controller controller;
    controller.init_test_editor(&test_editor);

    auto entry = controller.OnEntry();
    if (!entry.ok()) {
      return EXIT_FAILURE;
    }

    while (controller.IsActive()) {
      controller.OnInput();
      if (!controller.OnLoad().ok()) {
        return EXIT_FAILURE;
      }
      controller.DoRender();
    }

    controller.OnExit();

    return EXIT_SUCCESS;
  }

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}