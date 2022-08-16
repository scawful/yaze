#if defined(_WIN32)
#define main SDL_main
#endif

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "app/core/controller.h"

int main(int argc, char** argv) {
  absl::InitializeSymbolizer(argv[0]);

  absl::FailureSignalHandlerOptions options;
  absl::InstallFailureSignalHandler(options);

  yaze::app::core::Controller controller;

  auto entry_status = controller.onEntry();
  if (!entry_status.ok()) {
    // TODO(@scawful): log the specific error
    return EXIT_FAILURE;
  }

  while (controller.isActive()) {
    controller.onInput();
    controller.onLoad();
    controller.doRender();
  }
  controller.onExit();

  return EXIT_SUCCESS;
}