#if __APPLE__
#include "app/platform/app_delegate.h"
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include "app/platform/wasm/wasm_collaboration.h"
#include "app/platform/wasm/wasm_bootstrap.h"
#endif

#define IMGUI_DEFINE_MATH_OPERATORS
#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "absl/strings/str_cat.h"
#include "app/controller.h"
#include "app/emu/emulator.h"
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

// Global application state wrapper
class Application {
 public:
  static Application& Instance() {
    static Application instance;
    return instance;
  }

  void Initialize() {
    LOG_INFO("App", "Initializing Application instance...");
    controller_ = std::make_unique<Controller>();
    
    // Process pending ROM load if we have one (from flags or queued web load)
    std::string start_path = "";
    if (!pending_rom_.empty()) {
      start_path = pending_rom_;
      pending_rom_.clear();
      LOG_INFO("App", "Found pending ROM load: %s", start_path.c_str());
    } else {
      LOG_INFO("App", "No pending ROM, starting empty.");
    }
    
    // Always call OnEntry to initialize Window/Renderer, even with empty path
    auto status = controller_->OnEntry(start_path);
    if (!status.ok()) {
       LOG_ERROR("App", "Failed to initialize controller: %s", std::string(status.message()).c_str());
    } else {
       LOG_INFO("App", "Controller initialized successfully. Active: %s", controller_->IsActive() ? "Yes" : "No");
       // Controller is now active
       if (!start_path.empty()) {
           // Apply startup editor flags if set
           if (!FLAGS_editor->Get().empty()) {
             controller_->SetStartupEditor(FLAGS_editor->Get(), FLAGS_cards->Get());
           }
       }
    }
  }

  void Tick() {
    if (!controller_) return;

#ifdef __EMSCRIPTEN__
// ... (keep existing code)
#endif

    controller_->OnInput();
    auto status = controller_->OnLoad();
    if (!status.ok()) {
      LOG_ERROR("Main", "Controller Load Error: %s", std::string(status.message()).c_str());
#ifdef __EMSCRIPTEN__
      emscripten_cancel_main_loop();
#endif
      return;
    }
    
    // Debug check if active state changed
    if (!controller_->IsActive()) {
        LOG_INFO("App", "Controller became inactive during Tick");
    }

#ifdef __EMSCRIPTEN__
    auto& wasm_collab = app::platform::GetWasmCollaborationInstance();
    wasm_collab.ProcessPendingChanges();
#endif
    controller_->DoRender();
  }

  // Unified load function for CLI, Web, and Startup
  void LoadRom(const std::string& path) {
    LOG_INFO("App", "Requesting ROM load: %s", path.c_str());

    if (!controller_) {
      // If controller not initialized, queue it.
      pending_rom_ = path;
      LOG_INFO("App", "Queued ROM load (controller not ready): %s", path.c_str());
      return;
    }

    // Controller exists. 
    // If it's already active (window open), use OpenRomOrProject.
    // If it's not active (unlikely with new Initialize logic, but safe check), use OnEntry.
    
    absl::Status status;
    if (!controller_->IsActive()) {
       status = controller_->OnEntry(path);
    } else {
       status = controller_->editor_manager()->OpenRomOrProject(path);
    }

    if (!status.ok()) {
      std::string error_msg = absl::StrCat("Failed to load ROM: ", status.message());
      LOG_ERROR("App", "%s", error_msg.c_str());
      
#ifdef __EMSCRIPTEN__
      EM_ASM({
        var msg = UTF8ToString($0);
        console.error(msg);
        alert(msg);
      }, error_msg.c_str());
#endif
    } else {
      LOG_INFO("App", "ROM loaded successfully: %s", path.c_str());
#ifdef __EMSCRIPTEN__
      EM_ASM({
        console.log("ROM loaded successfully: " + UTF8ToString($0));
      }, path.c_str());
#endif
    }
  }

  Controller* GetController() { return controller_.get(); }
  bool IsReady() const { return controller_ != nullptr; }

  void Shutdown() {
    if (controller_) {
      controller_->OnExit();
      controller_.reset();
    }
  }

 private:
  std::unique_ptr<Controller> controller_;
  std::string pending_rom_;
};

// Accessor functions for WASM debug inspector / Bridges
namespace yaze::app {
emu::Emulator* GetGlobalEmulator() {
  auto* c = Application::Instance().GetController();
  if (!c || !c->editor_manager()) {
    return nullptr;
  }
  return &c->editor_manager()->emulator();
}

editor::EditorManager* GetGlobalEditorManager() {
  auto* c = Application::Instance().GetController();
  return c ? c->editor_manager() : nullptr;
}
}  // namespace yaze::app

void TickFrame() {
#ifdef __EMSCRIPTEN__
  if (!yaze::app::wasm::IsFileSystemReady()) {
    return;
  }

  // Lazy Init for WASM (waiting for FS)
  if (!Application::Instance().IsReady()) {
    Application::Instance().Initialize();
    
    // Check flags for startup ROM if no web-load occurred
    // Note: If Initialize() processed a pending_rom, this flag check might be redundant 
    // or conflict. However, flags are static.
    // If pending_rom_ was set via LoadRom (from Web) before Init, it takes precedence.
    // If not, we check flags.
    // But Initialize() already checks pending_rom_.
    // So we just need to ensure flags are put into pending_rom_ if no other rom is there?
    // Actually, in main() we can seed pending_rom_ from flags.
  }
#endif

  if (!Application::Instance().IsReady() || !Application::Instance().GetController()->IsActive()) {
#ifdef __EMSCRIPTEN__
    LOG_INFO("App", "Shutdown triggered. Ready: %s, Active: %s", 
             Application::Instance().IsReady() ? "Yes" : "No",
             (Application::Instance().IsReady() && Application::Instance().GetController()->IsActive()) ? "Yes" : "No");
             
    // Sync back to IDBFS on exit
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

  Application::Instance().Tick();
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
  yaze::util::LogLevel log_level = FLAGS_debug->Get() 
                                       ? yaze::util::LogLevel::YAZE_DEBUG 
                                       : yaze::util::LogLevel::INFO;

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

  std::string log_path = FLAGS_log_file->Get();
  if (log_path.empty()) {
    auto logs_dir = yaze::util::PlatformPaths::GetUserDocumentsSubdirectory("logs");
    if (logs_dir.ok()) {
      log_path = (*logs_dir / "yaze.log").string();
    }
  }

  yaze::util::LogManager::instance().configure(log_level, log_path,
                                               log_categories);

  if (FLAGS_debug->Get()) {
    yaze::core::FeatureFlags::get().kLogToConsole = true;
    LOG_INFO("Main", "🚀 YAZE started in debug mode");
  }

  // Pre-load ROM path into Application if flag is set
  if (!FLAGS_rom_file->Get().empty()) {
    Application::Instance().LoadRom(FLAGS_rom_file->Get());
  }

#ifdef YAZE_WITH_GRPC
#ifndef __EMSCRIPTEN__
  if (FLAGS_enable_test_harness->Get()) {
    auto& test_manager = yaze::test::TestManager::Get();
    auto& server = yaze::test::ImGuiTestHarnessServer::Instance();
    int port = FLAGS_test_harness_port->Get();

    std::cout << "\n🚀 Starting ImGui Test Harness on port " << port << "..." << std::endl;
    auto status = server.Start(port, &test_manager);
    if (!status.ok()) {
      std::cerr << "❌ ERROR: Failed to start test harness server: " << status.message() << std::endl;
      return 1;
    }
    std::cout << "✅ Test harness ready on 127.0.0.1:" << port << std::endl;
  }
#endif
#endif

#ifdef __APPLE__
  // On macOS, we delegate to Cocoa app delegate which will eventually call main loop or TickFrame?
  // Existing code was: return yaze_run_cocoa_app_delegate(rom_filename.c_str());
  // The app delegate likely manages the loop.
  // We should pass the rom filename if we have it.
  // Application singleton is now ready to receive calls.
  return yaze_run_cocoa_app_delegate(FLAGS_rom_file->Get().c_str());
#elif defined(_WIN32)
  SDL_SetMainReady();
#endif

#ifdef __EMSCRIPTEN__
  // Register the ROM load handler with the bootstrap layer
  yaze::app::wasm::SetRomLoadHandler([](std::string path) {
    Application::Instance().LoadRom(path);
  });

  yaze::app::wasm::InitializeWasmPlatform();
  emscripten_set_main_loop(TickFrame, 0, 1);
#else
  // Desktop Main Loop
  
  // API Server
  std::unique_ptr<yaze::cli::api::HttpServer> api_server;
  if (FLAGS_enable_api->Get()) {
    api_server = std::make_unique<yaze::cli::api::HttpServer>();
    auto status = api_server->Start(FLAGS_api_port->Get());
    if (!status.ok()) {
      LOG_ERROR("Main", "Failed to start API server: %s", std::string(status.message()).c_str());
    } else {
      LOG_INFO("Main", "API Server started on port %d", FLAGS_api_port->Get());
    }
  }

  Application::Instance().Initialize();
  
  while (Application::Instance().GetController()->IsActive()) {
      TickFrame();
  }
  
  Application::Instance().Shutdown();

  if (api_server) {
    api_server->Stop();
  }

#ifdef YAZE_WITH_GRPC
  yaze::test::ImGuiTestHarnessServer::Instance().Shutdown();
#endif

#endif // __EMSCRIPTEN__

  return EXIT_SUCCESS;
}