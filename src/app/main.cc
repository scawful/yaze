#if __APPLE__
#include "app/platform/app_delegate.h"
#endif

#define IMGUI_DEFINE_MATH_OPERATORS
#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "app/controller.h"
#include "core/features.h"
#include "util/flag.h"
#include "util/log.h"

#ifdef YAZE_WITH_GRPC
#include "app/service/imgui_test_harness_service.h"
#include "app/test/test_manager.h"
#endif

/**
 * @namespace yaze
 * @brief Main namespace for the application.
 */
using namespace yaze;

// Enhanced flags for debugging
DEFINE_FLAG(std::string, rom_file, "", "The ROM file to load.");
DEFINE_FLAG(std::string, log_file, "", "Output log file path for debugging.");
DEFINE_FLAG(bool, debug, false, "Enable debug logging and verbose output.");
DEFINE_FLAG(std::string, log_categories, "", 
            "Comma-separated list of log categories to enable. "
            "If empty, all categories are logged. "
            "Example: --log_categories=\"Room,DungeonEditor\" to filter noisy logs.");
DEFINE_FLAG(std::string, editor, "", 
            "The editor to open on startup. "
            "Available editors: Assembly, Dungeon, Graphics, Music, Overworld, "
            "Palette, Screen, Sprite, Message, Hex, Agent, Settings. "
            "Example: --editor=Dungeon");
DEFINE_FLAG(std::string, cards, "",
            "A comma-separated list of cards to open within the specified editor. "
            "For Dungeon editor: 'Rooms List', 'Room Matrix', 'Entrances List', "
            "'Room Graphics', 'Object Editor', 'Palette Editor', or 'Room N' (where N is room ID). "
            "Example: --cards=\"Rooms List,Room 0,Room 105\"");

#ifdef YAZE_WITH_GRPC
// gRPC test harness flags
DEFINE_FLAG(bool, enable_test_harness, false,
            "Start gRPC test harness server for automated GUI testing.");
DEFINE_FLAG(int, test_harness_port, 50051,
            "Port for gRPC test harness server (default: 50051).");
#endif

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
  
  // Parse command line flags with custom parser
  yaze::util::FlagParser parser(yaze::util::global_flag_registry());
  RETURN_IF_EXCEPTION(parser.Parse(argc, argv));
  
  // Set up logging
  yaze::util::LogLevel log_level = FLAGS_debug->Get()
                                     ? yaze::util::LogLevel::YAZE_DEBUG
                                     : yaze::util::LogLevel::INFO;
  
  // Parse log categories from comma-separated string
  std::set<std::string> log_categories;
  std::string categories_str = FLAGS_log_categories->Get();
  if (!categories_str.empty()) {
    size_t start = 0;
    size_t end = categories_str.find(',');
    while (end != std::string::npos) {
      log_categories.insert(categories_str.substr(start, end - start));
      start = end + 1;
      end = categories_str.find(',', start);
    }
    log_categories.insert(categories_str.substr(start));
  }
  
  yaze::util::LogManager::instance().configure(log_level, FLAGS_log_file->Get(),
                                             log_categories);

  // Enable console logging via feature flag if debug is enabled.
  if (FLAGS_debug->Get()) {
    yaze::core::FeatureFlags::get().kLogToConsole = true;
    LOG_INFO("Main", "🚀 YAZE started in debug mode");
  }
  
  std::string rom_filename = "";
  if (!FLAGS_rom_file->Get().empty()) {
    rom_filename = FLAGS_rom_file->Get();
  }

#ifdef YAZE_WITH_GRPC
  // Start gRPC test harness server if requested
  if (FLAGS_enable_test_harness->Get()) {
    // Get TestManager instance (initializes UI testing if available)
    auto& test_manager = yaze::test::TestManager::Get();
    
    auto& server = yaze::test::ImGuiTestHarnessServer::Instance();
    int port = FLAGS_test_harness_port->Get();
    
    std::cout << "\n🚀 Starting ImGui Test Harness on port " << port << "..." << std::endl;
    auto status = server.Start(port, &test_manager);
    if (!status.ok()) {
      std::cerr << "❌ ERROR: Failed to start test harness server on port " << port << std::endl;
      std::cerr << "   " << status.message() << std::endl;
      return 1;
    }
    std::cout << "✅ Test harness ready on 127.0.0.1:" << port << std::endl;
    std::cout << "   Available RPCs: Ping, Click, Type, Wait, Assert, Screenshot\n" << std::endl;
  }
#endif

#ifdef __APPLE__
  return yaze_run_cocoa_app_delegate(rom_filename.c_str());
#elif defined(_WIN32)
  // We set SDL_MAIN_HANDLED for Win32 to avoid SDL hijacking main()
  SDL_SetMainReady();
#endif

  auto controller = std::make_unique<Controller>();
  EXIT_IF_ERROR(controller->OnEntry(rom_filename))
  
  // Set startup editor and cards from flags (after OnEntry initializes editor manager)
  if (!FLAGS_editor->Get().empty()) {
    controller->SetStartupEditor(FLAGS_editor->Get(), FLAGS_cards->Get());
  }

  while (controller->IsActive()) {
    controller->OnInput();
    if (auto status = controller->OnLoad(); !status.ok()) {
      std::cerr << status.message() << std::endl;
      break;
    }
    controller->DoRender();
  }
  controller->OnExit();

#ifdef YAZE_WITH_GRPC
  // Shutdown gRPC server if running
  yaze::test::ImGuiTestHarnessServer::Instance().Shutdown();
#endif

  return EXIT_SUCCESS;
}
