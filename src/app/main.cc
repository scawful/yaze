#if defined(_WIN32)
#define main SDL_main
#elif __APPLE__
#include "app/core/platform/app_delegate.h"
#endif

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "app/core/controller.h"

/**
 * @namespace yaze::app
 * @brief Main namespace for the ImGui application.
 */
using namespace yaze::app;

/**
 * @brief Main entry point for the application.
 */
int main(int argc, char** argv) {
  absl::InitializeSymbolizer(argv[0]);

  absl::FailureSignalHandlerOptions options;
  options.symbolize_stacktrace = true;
  options.use_alternate_stack = true;
  options.alarm_on_failure_secs = true;
  options.call_previous_handler = true;
  absl::InstallFailureSignalHandler(options);

  std::string rom_filename;
  if (argc > 1) {
    rom_filename = argv[1];
  }

#ifdef __APPLE__
  yaze_run_cocoa_app_delegate(rom_filename.c_str());
  return EXIT_SUCCESS;
#endif

  core::Controller controller;
  EXIT_IF_ERROR(controller.OnEntry(rom_filename))

  while (controller.IsActive()) {
    controller.OnInput();
    if (auto status = controller.OnLoad(); !status.ok()) {
      std::cerr << status.message() << std::endl;
      break;
    }
    controller.DoRender();
  }
  controller.OnExit();

  return EXIT_SUCCESS;
}
