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
  options.alarm_on_failure_secs = true;
  absl::InstallFailureSignalHandler(options);

  std::string rom_filename;
  if (argc > 1) {
    rom_filename = argv[1];
  }

  core::Controller controller;
  EXIT_IF_ERROR(controller.OnEntry(rom_filename))

#ifdef __APPLE__
  InitializeCocoa();
#endif
  try {
    while (controller.IsActive()) {
      controller.OnInput();
      controller.OnLoad();
      controller.DoRender();
    }
    controller.OnExit();
  } catch (const std::bad_alloc& e) {
    std::cerr << "Memory allocation failed: " << e.what() << std::endl;
    return EXIT_FAILURE;
  } catch (const std::exception& e) {
    std::cerr << "Exception: " << e.what() << std::endl;
    return EXIT_FAILURE;
  } catch (...) {
    std::cerr << "Unknown exception" << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}