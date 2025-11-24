#if __APPLE__
#include "app/platform/app_delegate.h"
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include "app/platform/wasm/wasm_collaboration.h"
#include "app/platform/wasm/wasm_config.h"
#include "app/platform/wasm/wasm_drop_handler.h"
#include <fstream>
#endif

#define IMGUI_DEFINE_MATH_OPERATORS
#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "app/controller.h"
#include "cli/service/api/http_server.h"
#include "core/features.h"
#include "util/crash_handler.h"
#include "util/flag.h"
#include "util/log.h"
#include "util/platform_paths.h"
#include "yaze.h"  // For YAZE_VERSION_STRING

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
DEFINE_FLAG(
    std::string, log_categories, "",
    "Comma-separated list of log categories to enable. "
    "If empty, all categories are logged. "
    "Example: --log_categories=\"Room,DungeonEditor\" to filter noisy logs.");
DEFINE_FLAG(std::string, editor, "",
            "The editor to open on startup. "
            "Available editors: Assembly, Dungeon, Graphics, Music, Overworld, "
            "Palette, Screen, Sprite, Message, Hex, Agent, Settings. "
            "Example: --editor=Dungeon");
DEFINE_FLAG(
    std::string, cards, "",
    "A comma-separated list of cards to open within the specified editor. "
    "For Dungeon editor: 'Rooms List', 'Room Matrix', 'Entrances List', "
    "'Room Graphics', 'Object Editor', 'Palette Editor', or 'Room N' (where N "
    "is room ID). "
    "Example: --cards=\"Rooms List,Room 0,Room 105\"");

// AI Agent API flags
DEFINE_FLAG(bool, enable_api, false, "Enable the AI Agent API server.");
DEFINE_FLAG(int, api_port, 8080, "Port for the AI Agent API server.");

#ifdef YAZE_WITH_GRPC
// gRPC test harness flags
DEFINE_FLAG(bool, enable_test_harness, false,
            "Start gRPC test harness server for automated GUI testing.");
DEFINE_FLAG(int, test_harness_port, 50051,
            "Port for gRPC test harness server (default: 50051).");
#endif

// Global controller for Emscripten loop
static std::unique_ptr<Controller> g_controller;

#ifdef __EMSCRIPTEN__
static bool g_filesystem_ready = false;
static std::string g_pending_rom_filename;

extern "C" {
EMSCRIPTEN_KEEPALIVE
void SetFileSystemReady() {
  g_filesystem_ready = true;
  LOG_INFO("Main", "Filesystem sync complete, ready to start.");
}

EMSCRIPTEN_KEEPALIVE
void LoadRomFromWeb(const char* filename) {
  if (!filename) {
    LOG_ERROR("Main", "LoadRomFromWeb called with null filename");
    return;
  }
  
  std::string rom_path = filename;
  LOG_INFO("Main", "LoadRomFromWeb called: %s (controller ready: %s)", 
           rom_path.c_str(), g_controller ? "yes" : "no");
  
  // If controller isn't ready yet, queue the ROM load
  if (!g_controller) {
    g_pending_rom_filename = rom_path;
    LOG_INFO("Main", "Controller not ready, queuing ROM load: %s", rom_path.c_str());
    return;
  }
  
  // Controller is ready, load the ROM
  LOG_INFO("Main", "Loading ROM from web: %s", rom_path.c_str());
  auto status = g_controller->OnEntry(rom_path);
  if (!status.ok()) {
    LOG_ERROR("Main", "Failed to load ROM: %s", std::string(status.message()).c_str());
    // Show error in JavaScript console and alert
    EM_ASM({
      console.error("Failed to load ROM: " + UTF8ToString($0));
      alert("Failed to load ROM: " + UTF8ToString($0));
    }, std::string(status.message()).c_str());
  } else {
    LOG_INFO("Main", "ROM loaded successfully: %s", rom_path.c_str());
  }
}
}

EM_JS(void, MountFilesystems, (), {
  // Create directories
  FS.mkdir('/roms');
  FS.mkdir('/saves');

  // Mount filesystems
  FS.mount(MEMFS, {}, '/roms');
  
  // Check if IDBFS is available (try multiple ways to access it)
  var idbfs = null;
  if (typeof IDBFS !== 'undefined') {
    idbfs = IDBFS;
  } else if (typeof Module !== 'undefined' && typeof Module.IDBFS !== 'undefined') {
    idbfs = Module.IDBFS;
  } else if (typeof FS !== 'undefined' && typeof FS.filesystems !== 'undefined' && FS.filesystems.IDBFS) {
    idbfs = FS.filesystems.IDBFS;
  }
  
  if (idbfs !== null) {
    try {
      FS.mount(idbfs, {}, '/saves');
      
      // Sync from IDBFS to memory
      FS.syncfs(true, function(err) {
        if (err) {
          console.error("Failed to sync IDBFS: " + err);
        } else {
          console.log("IDBFS synced successfully");
        }
        // Signal C++ that we are ready regardless of success/fail
        // (worst case we start with empty saves)
        Module._SetFileSystemReady();
      });
    } catch (e) {
      console.error("Error mounting IDBFS: " + e);
      // Fallback to MEMFS
      FS.mount(MEMFS, {}, '/saves');
      Module._SetFileSystemReady();
    }
  } else {
    // Fallback to MEMFS if IDBFS is not available (no persistence, but app will work)
    console.warn("IDBFS not available, using MEMFS for /saves (no persistence)");
    FS.mount(MEMFS, {}, '/saves');
    // Signal ready immediately since there's no async sync needed
    Module._SetFileSystemReady();
  }
});
#endif

void TickFrame() {
#ifdef __EMSCRIPTEN__
  auto& wasm_collab = app::platform::GetWasmCollaborationInstance();
  if (!g_filesystem_ready) {
    // Wait for FS sync
    // We could render a loading screen here if we had ImGui init early
    return;
  }

  // Initialize controller once FS is ready
  if (!g_controller) {
    g_controller = std::make_unique<Controller>();
    
    std::string rom_filename = "";
    if (!FLAGS_rom_file->Get().empty()) {
      rom_filename = FLAGS_rom_file->Get();
    } else if (!g_pending_rom_filename.empty()) {
      // Use pending ROM from LoadRomFromWeb call
      rom_filename = g_pending_rom_filename;
      g_pending_rom_filename.clear();
    }
    
    if (auto status = g_controller->OnEntry(rom_filename); !status.ok()) {
        std::cerr << status.message() << std::endl;
        emscripten_cancel_main_loop();
        return;
    }

    if (!FLAGS_editor->Get().empty()) {
      g_controller->SetStartupEditor(FLAGS_editor->Get(), FLAGS_cards->Get());
    }

    wasm_collab.SetRom(g_controller->GetCurrentRom());
  } else if (!g_pending_rom_filename.empty()) {
    // Controller is already initialized, but we have a pending ROM load
    std::string rom_path = g_pending_rom_filename;
    g_pending_rom_filename.clear();
    LOG_INFO("Main", "Processing queued ROM load: %s", rom_path.c_str());
    auto status = g_controller->OnEntry(rom_path);
    if (!status.ok()) {
      LOG_ERROR("Main", "Failed to load queued ROM: %s", std::string(status.message()).c_str());
      EM_ASM({
        console.error("Failed to load ROM: " + UTF8ToString($0));
        alert("Failed to load ROM: " + UTF8ToString($0));
      }, std::string(status.message()).c_str());
    } else {
      LOG_INFO("Main", "Queued ROM loaded successfully: %s", rom_path.c_str());
    }
  }
#endif

  if (!g_controller || !g_controller->IsActive()) {
#ifdef __EMSCRIPTEN__
    // Trigger sync back to IDBFS on exit (if this ever happens in web)
    EM_ASM(
      FS.syncfs(false, function(err) {
        if (err) console.error("Failed to save IDBFS: " + err);
        else console.log("IDBFS saved successfully");
      });
    );
    emscripten_cancel_main_loop();
#endif
    return;
  }

#ifdef __EMSCRIPTEN__
  if (g_controller) {
    wasm_collab.SetRom(g_controller->GetCurrentRom());
  }
#endif

  g_controller->OnInput();
  if (auto status = g_controller->OnLoad(); !status.ok()) {
    std::cerr << status.message() << std::endl;
#ifdef __EMSCRIPTEN__
    emscripten_cancel_main_loop();
#endif
    return;
  }
#ifdef __EMSCRIPTEN__
  wasm_collab.ProcessPendingChanges();
#endif
  g_controller->DoRender();
}

int main(int argc, char** argv) {
  absl::InitializeSymbolizer(argv[0]);

#ifndef __EMSCRIPTEN__
  // Initialize crash handler for release builds
  // This writes crash reports to ~/.yaze/crash_logs/ (or equivalent)
  // In debug builds, crashes are also printed to stderr
  yaze::util::CrashHandler::Initialize(YAZE_VERSION_STRING);

  // Clean up old crash logs (keep last 5)
  yaze::util::CrashHandler::CleanupOldLogs(5);
#endif

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

  // Determine log file path
  std::string log_path = FLAGS_log_file->Get();
  if (log_path.empty()) {
    // Default to ~/Documents/Yaze/logs/yaze.log if not specified
    auto logs_dir = yaze::util::PlatformPaths::GetUserDocumentsSubdirectory("logs");
    if (logs_dir.ok()) {
      log_path = (*logs_dir / "yaze.log").string();
    }
  }

  yaze::util::LogManager::instance().configure(log_level, log_path,
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
#ifndef __EMSCRIPTEN__
  // Start gRPC test harness server if requested
  if (FLAGS_enable_test_harness->Get()) {
    // Get TestManager instance (initializes UI testing if available)
    auto& test_manager = yaze::test::TestManager::Get();

    auto& server = yaze::test::ImGuiTestHarnessServer::Instance();
    int port = FLAGS_test_harness_port->Get();

    std::cout << "\n🚀 Starting ImGui Test Harness on port " << port << "..."
              << std::endl;
    auto status = server.Start(port, &test_manager);
    if (!status.ok()) {
      std::cerr << "❌ ERROR: Failed to start test harness server on port "
                << port << std::endl;
      std::cerr << "   " << status.message() << std::endl;
      return 1;
    }
    std::cout << "✅ Test harness ready on 127.0.0.1:" << port << std::endl;
    std::cout
        << "   Available RPCs: Ping, Click, Type, Wait, Assert, Screenshot\n"
        << std::endl;
  }
#endif
#endif

#ifdef __APPLE__
  return yaze_run_cocoa_app_delegate(rom_filename.c_str());
#elif defined(_WIN32)
  // We set SDL_MAIN_HANDLED for Win32 to avoid SDL hijacking main()
  SDL_SetMainReady();
#endif

#ifdef __EMSCRIPTEN__
  // Load WASM configuration from JavaScript
  app::platform::WasmConfig::Get().LoadFromJavaScript();

  // Initialize drop handler for Drag & Drop support
  auto& drop_handler = yaze::platform::WasmDropHandler::GetInstance();
  drop_handler.Initialize("", 
    [](const std::string& filename, const std::vector<uint8_t>& data) {
        // Determine file type from extension
        std::string ext = filename.substr(filename.find_last_of(".") + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == "sfc" || ext == "smc" || ext == "zip") {
            // Write to MEMFS and load
            std::string path = "/roms/" + filename;
            std::ofstream file(path, std::ios::binary);
            file.write(reinterpret_cast<const char*>(data.data()), data.size());
            file.close();
            
            LOG_INFO("Main", "Wrote dropped ROM to %s (%zu bytes)", path.c_str(), data.size());
            LoadRomFromWeb(path.c_str());
        } 
        else if (ext == "pal" || ext == "tpl") {
            // Placeholder for palette logic
            LOG_INFO("Main", "Palette drop detected: %s. Feature pending UI integration.", filename.c_str());
            // Future: Decode and apply to current palette editor via Controller/EditorManager
        }
    },
    [](const std::string& error) {
        LOG_ERROR("Main", "Drop Handler Error: %s", error.c_str());
    }
  );

  // Initialize filesystems asynchronously
  MountFilesystems();

  // Emscripten main loop - waits for g_filesystem_ready in TickFrame
  emscripten_set_main_loop(TickFrame, 0, 1);
#else
  auto controller = std::make_unique<Controller>();
  EXIT_IF_ERROR(controller->OnEntry(rom_filename))

  // Set startup editor and cards from flags (after OnEntry initializes editor
  // manager)
  if (!FLAGS_editor->Get().empty()) {
    controller->SetStartupEditor(FLAGS_editor->Get(), FLAGS_cards->Get());
  }

  // Start API server if requested
  std::unique_ptr<yaze::cli::api::HttpServer> api_server;
  if (FLAGS_enable_api->Get()) {
    api_server = std::make_unique<yaze::cli::api::HttpServer>();
    auto status = api_server->Start(FLAGS_api_port->Get());
    if (!status.ok()) {
      LOG_ERROR("Main", "Failed to start API server: %s",
                std::string(status.message().data(), status.message().size()).c_str());
    } else {
      LOG_INFO("Main", "API Server started on port %d", FLAGS_api_port->Get());
    }
  }

  // Assign to global for shared TickFrame if we wanted to reuse it, but 
  // for desktop we can keep the loop local or use TickFrame logic.
  // To avoid duplication, let's wire it up to use TickFrame-like logic 
  // or just keep the loop as is for minimal risk.
  
  // Using local loop for desktop to match previous behavior exactly
  g_controller = std::move(controller);
  
  while (g_controller->IsActive()) {
      TickFrame();
  }
  
  g_controller->OnExit();

  if (api_server) {
    api_server->Stop();
  }

#ifdef YAZE_WITH_GRPC
  // Shutdown gRPC server if running
  yaze::test::ImGuiTestHarnessServer::Instance().Shutdown();
#endif

#endif // __EMSCRIPTEN__

  return EXIT_SUCCESS;
}
