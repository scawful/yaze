#if defined(_WIN32)
#define main SDL_main
#endif

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "app/core/controller.h"

int main(int argc, char** argv) {
  absl::InitializeSymbolizer(argv[0]);

  absl::FailureSignalHandlerOptions options;
  options.symbolize_stacktrace = true;
  options.alarm_on_failure_secs = true;
  absl::InstallFailureSignalHandler(options);

  yaze::app::core::Controller controller;

  EXIT_IF_ERROR(controller.OnEntry())
  while (controller.IsActive()) {
    controller.OnInput();
    controller.OnLoad();
    controller.DoRender();
  }
  controller.OnExit();

  return EXIT_SUCCESS;
}