#if __APPLE__
#include "app/platform/app_delegate.h"
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include "app/platform/wasm/wasm_collaboration.h"
#include "app/platform/wasm/wasm_bootstrap.h"
#endif

#define IMGUI_DEFINE_MATH_OPERATORS
#include <set>

#include "absl/debugging/symbolize.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "app/application.h"
#include "app/startup_flags.h"
#include "app/controller.h"
#include "cli/service/api/http_server.h"
#include "core/features.h"
#include "util/crash_handler.h"
#include "util/flag.h"
#include "util/log.h"
#include "util/platform_paths.h"
#include "yaze.h"

// ============================================================================
// Global Accessors for WASM Integration
DEFINE_FLAG(std::string, log_file, "", "Output log file path for debugging.");
DEFINE_FLAG(std::string, rom_file, "", "ROM file to load on startup.");
DEFINE_FLAG(bool, debug, false, "Enable debug logging and verbose output.");
DEFINE_FLAG(std::string, log_level, "info",
            "Minimum log level: debug, info, warn, error, or fatal.");
DEFINE_FLAG(bool, log_to_console, false,
            "Mirror logs to stderr even when writing to a file.");
DEFINE_FLAG(
    std::string, log_categories, "",
    "Comma-separated list of log categories to enable or disable. "
    "Prefix with '-' to disable a category. "
    "Example: \"Room,DungeonEditor\" (allowlist) or \"-Input,-Graphics\" "
    "(blocklist).");

// Navigation flags
DEFINE_FLAG(std::string, editor, "",
            "The editor to open on startup (e.g., Dungeon, Overworld, Assembly).");

DEFINE_FLAG(std::string, open_panels, "",
            "Comma-separated list of panel IDs to open (e.g. 'dungeon.room_list,emulator.cpu_debugger')");

// UI visibility flags
DEFINE_FLAG(std::string, startup_welcome, "auto",
            "Welcome screen behavior at startup: auto, show, or hide.");
DEFINE_FLAG(std::string, startup_dashboard, "auto",
            "Dashboard panel behavior at startup: auto, show, or hide.");
DEFINE_FLAG(std::string, startup_sidebar, "auto",
            "Panel sidebar visibility at startup: auto, show, or hide.");

DEFINE_FLAG(int, room, -1, "Open Dungeon Editor at specific room ID (0-295).");
DEFINE_FLAG(int, map, -1, "Open Overworld Editor at specific map ID (0-159).");

// AI Agent API flags
DEFINE_FLAG(bool, enable_api, false, "Enable the AI Agent API server.");
DEFINE_FLAG(int, api_port, 8080, "Port for the AI Agent API server.");

#ifdef YAZE_WITH_GRPC
// gRPC test harness flags
DEFINE_FLAG(bool, enable_test_harness, false,
            "Start gRPC test harness server for automated GUI testing.");
DEFINE_FLAG(int, test_harness_port, 50052,
            "Port for gRPC test harness server (default: 50052). AgentControlServer uses 50051.");
#endif

// ============================================================================
// Global Accessors for WASM Integration
// These are used by yaze_debug_inspector.cc and wasm_terminal_bridge.cc
// ============================================================================
namespace yaze {
namespace emu {
class Emulator;
}
namespace editor {
class EditorManager;
}
}

namespace yaze::app {

yaze::emu::Emulator* GetGlobalEmulator() {
  auto* ctrl = yaze::Application::Instance().GetController();
  if (ctrl && ctrl->editor_manager()) {
    return &ctrl->editor_manager()->emulator();
  }
  return nullptr;
}

yaze::editor::EditorManager* GetGlobalEditorManager() {
  auto* ctrl = yaze::Application::Instance().GetController();
  if (ctrl) {
    return ctrl->editor_manager();
  }
  return nullptr;
}

}  // namespace yaze::app

namespace {

yaze::util::LogLevel ParseLogLevelFlag(const std::string& raw_level,
                                       bool debug_flag) {
  if (debug_flag) {
    return yaze::util::LogLevel::YAZE_DEBUG;
  }

  const std::string lower = absl::AsciiStrToLower(raw_level);
  if (lower == "debug") {
    return yaze::util::LogLevel::YAZE_DEBUG;
  }
  if (lower == "warn" || lower == "warning") {
    return yaze::util::LogLevel::WARNING;
  }
  if (lower == "error") {
    return yaze::util::LogLevel::ERROR;
  }
  if (lower == "fatal") {
    return yaze::util::LogLevel::FATAL;
  }
  return yaze::util::LogLevel::INFO;
}

std::set<std::string> ParseLogCategories(const std::string& raw) {
  std::set<std::string> categories;
  for (absl::string_view token :
       absl::StrSplit(raw, ',', absl::SkipWhitespace())) {
    if (!token.empty()) {
      categories.insert(std::string(absl::StripAsciiWhitespace(token)));
    }
  }
  return categories;
}

const char* LogLevelToString(yaze::util::LogLevel level) {
  switch (level) {
    case yaze::util::LogLevel::YAZE_DEBUG:
      return "debug";
    case yaze::util::LogLevel::INFO:
      return "info";
    case yaze::util::LogLevel::WARNING:
      return "warn";
    case yaze::util::LogLevel::ERROR:
      return "error";
    case yaze::util::LogLevel::FATAL:
      return "fatal";
  }
  return "info";
}

std::vector<std::string> ParseCommaList(const std::string& raw) {
  std::vector<std::string> tokens;
  for (absl::string_view token :
       absl::StrSplit(raw, ',', absl::SkipWhitespace())) {
    if (!token.empty()) {
      tokens.emplace_back(absl::StripAsciiWhitespace(token));
    }
  }
  return tokens;
}

}  // namespace

void TickFrame() {
  yaze::Application::Instance().Tick();
}

int main(int argc, char** argv) {
  absl::InitializeSymbolizer(argv[0]);

#ifndef __EMSCRIPTEN__
  yaze::util::CrashHandler::Initialize(YAZE_VERSION_STRING);
  yaze::util::CrashHandler::CleanupOldLogs(5);
#endif

  yaze::util::FlagParser parser(yaze::util::global_flag_registry());
  RETURN_IF_EXCEPTION(parser.Parse(argc, argv));

  // Set up logging
  const bool debug_flag = FLAGS_debug->Get();
  const bool log_to_console_flag = FLAGS_log_to_console->Get();
  yaze::util::LogLevel log_level =
      ParseLogLevelFlag(FLAGS_log_level->Get(), debug_flag);
  std::set<std::string> log_categories =
      ParseLogCategories(FLAGS_log_categories->Get());

  std::string log_path = FLAGS_log_file->Get();
  if (log_path.empty()) {
    auto logs_dir = yaze::util::PlatformPaths::GetAppDataSubdirectory("logs");
    if (logs_dir.ok()) {
      log_path = (*logs_dir / "yaze.log").string();
    }
  }

  yaze::util::LogManager::instance().configure(log_level, log_path,
                                               log_categories);

  if (debug_flag || log_to_console_flag) {
    yaze::core::FeatureFlags::get().kLogToConsole = true;
  }
  if (debug_flag) {
    LOG_INFO("Main", "🚀 YAZE started in debug mode");
  }
  LOG_INFO("Main",
           "Logging configured (level=%s, file=%s, console=%s, categories=%zu)",
           LogLevelToString(log_level),
           log_path.empty() ? "<stderr>" : log_path.c_str(),
           (debug_flag || log_to_console_flag) ? "on" : "off",
           log_categories.size());

  // Build AppConfig from flags
  yaze::AppConfig config;
  config.rom_file = FLAGS_rom_file->Get();
  config.log_file = log_path;
  config.debug = (log_level == yaze::util::LogLevel::YAZE_DEBUG);
  config.log_categories = FLAGS_log_categories->Get();
  config.startup_editor = FLAGS_editor->Get();
  config.jump_to_room = FLAGS_room->Get();
  config.jump_to_map = FLAGS_map->Get();
  config.enable_api = FLAGS_enable_api->Get();
  config.api_port = FLAGS_api_port->Get();
  
  config.welcome_mode =
      yaze::StartupVisibilityFromString(FLAGS_startup_welcome->Get());
  config.dashboard_mode =
      yaze::StartupVisibilityFromString(FLAGS_startup_dashboard->Get());
  config.sidebar_mode =
      yaze::StartupVisibilityFromString(FLAGS_startup_sidebar->Get());

  if (!FLAGS_open_panels->Get().empty()) {
    config.open_panels = ParseCommaList(FLAGS_open_panels->Get());
  }

#ifdef YAZE_WITH_GRPC
  config.enable_test_harness = FLAGS_enable_test_harness->Get();
  config.test_harness_port = FLAGS_test_harness_port->Get();
#endif

#ifdef __APPLE__
  return yaze_run_cocoa_app_delegate(config);
#elif defined(_WIN32)
  SDL_SetMainReady();
#endif

#ifdef __EMSCRIPTEN__
  yaze::app::wasm::InitializeWasmPlatform();
  
  // Store config for deferred initialization
  static yaze::AppConfig s_wasm_config = config;
  static bool s_wasm_initialized = false;
  
  // Main loop that handles deferred initialization for filesystem readiness
  auto WasmMainLoop = []() {
    // Wait for filesystem to be ready before initializing application
    if (!s_wasm_initialized) {
      if (yaze::app::wasm::IsFileSystemReady()) {
        LOG_INFO("Main", "Filesystem ready, initializing application...");
        yaze::Application::Instance().Initialize(s_wasm_config);
        s_wasm_initialized = true;
      } else {
        // Still waiting for filesystem - do nothing this frame
        return;
      }
    }
    
    // Normal tick once initialized
    TickFrame();
  };
  
  // Use 0 for frame rate to enable requestAnimationFrame (better performance)
  // The third parameter (1) simulates infinite loop
  emscripten_set_main_loop(WasmMainLoop, 0, 1);
#else
  // Desktop Main Loop (Linux/Windows)
  
  // API Server
  std::unique_ptr<yaze::cli::api::HttpServer> api_server;
  if (config.enable_api) {
    api_server = std::make_unique<yaze::cli::api::HttpServer>();
    auto status = api_server->Start(config.api_port);
    if (!status.ok()) {
      LOG_ERROR("Main", "Failed to start API server: %s", std::string(status.message()).c_str());
    } else {
      LOG_INFO("Main", "API Server started on port %d", config.api_port);
    }
  }

  yaze::Application::Instance().Initialize(config);
  
  while (yaze::Application::Instance().GetController()->IsActive()) {
      TickFrame();
  }
  
  yaze::Application::Instance().Shutdown();

  if (api_server) {
    api_server->Stop();
  }

#endif // __EMSCRIPTEN__

  return EXIT_SUCCESS;
}
