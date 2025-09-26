#if __APPLE__
#include "app/core/platform/app_delegate.h"
#endif

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "app/core/controller.h"
#include "util/flag.h"

/**
 * @namespace yaze
 * @brief Main namespace for the application.
 */
using namespace yaze;

DEFINE_FLAG(std::string, rom_file, "", "The ROM file to load.");

int main(int argc, char **argv) {
  absl::InitializeSymbolizer(argv[0]);

  // Configure failure signal handler to be less aggressive
  // This prevents false positives during SDL/graphics cleanup
  absl::FailureSignalHandlerOptions options;
  options.symbolize_stacktrace = true;
  options.use_alternate_stack =
      false;  // Avoid conflicts with normal stack during cleanup
  options.alarm_on_failure_secs =
      false;  // Don't set alarms that can trigger on natural leaks
  options.call_previous_handler = true;  // Allow system handlers to also run
  options.writerfn =
      nullptr;  // Use default writer to avoid custom handling issues
  absl::InstallFailureSignalHandler(options);
  yaze::util::FlagParser parser(yaze::util::global_flag_registry());
  RETURN_IF_EXCEPTION(parser.Parse(argc, argv));
  std::string rom_filename = "";
  if (!FLAGS_rom_file->Get().empty()) {
    rom_filename = FLAGS_rom_file->Get();
  }

#ifdef __APPLE__
  return yaze_run_cocoa_app_delegate(rom_filename.c_str());
#elif defined(_WIN32)
  // We set SDL_MAIN_HANDLED for Win32 to avoid SDL hijacking main()
  SDL_SetMainReady();
#endif

  auto controller = std::make_unique<core::Controller>();
  EXIT_IF_ERROR(controller->OnEntry(rom_filename))

  while (controller->IsActive()) {
    controller->OnInput();
    if (auto status = controller->OnLoad(); !status.ok()) {
      std::cerr << status.message() << std::endl;
      break;
    }
    controller->DoRender();
  }
  controller->OnExit();

  return EXIT_SUCCESS;
}
