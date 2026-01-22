#ifndef YAZE_APP_APPLICATION_H_
#define YAZE_APP_APPLICATION_H_

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/controller.h"
#include "app/startup_flags.h"
#include "yaze_config.h"

#ifdef YAZE_WITH_GRPC
#include "app/service/unified_grpc_server.h"
#include "app/service/canvas_automation_service.h"
#include "cli/service/agent/agent_control_server.h"
#endif

namespace yaze {

/**
 * @brief Configuration options for the application startup
 */
struct AppConfig {
  // File loading
  std::string rom_file;
  std::string log_file;
  bool debug = false;
  std::string log_categories;
  StartupVisibility welcome_mode = StartupVisibility::kAuto;
  StartupVisibility dashboard_mode = StartupVisibility::kAuto;
  StartupVisibility sidebar_mode = StartupVisibility::kAuto;
  AssetLoadMode asset_load_mode = AssetLoadMode::kAuto;

  // Startup navigation
  std::string startup_editor;            // Editor to open (e.g., "Dungeon")
  std::vector<std::string> open_panels;  // Panel IDs to show (e.g., "dungeon.room_list")
  
  // Jump targets
  int jump_to_room = -1;  // Dungeon room ID (-1 to ignore)
  int jump_to_map = -1;   // Overworld map ID (-1 to ignore)
  
  // Services
  bool headless = false;   // Run without GUI (uses NullWindowBackend)
  bool enable_api = false;
  int api_port = 8080;
  bool enable_test_harness = false;
  int test_harness_port = 50052;  // GUI automation (50052), AgentControlServer uses 50051
};

/**
 * @class Application
 * @brief Main application singleton managing lifecycle and global state
 */
class Application {
 public:
  static Application& Instance();

  // Initialize the application with configuration
  void Initialize(const AppConfig& config);
  
  // Default initialization (empty config)
  void Initialize() { Initialize(AppConfig{}); }

  // Main loop tick
  void Tick();
  
  // Shutdown application
  void Shutdown();

  // Unified ROM loading
  void LoadRom(const std::string& path);
  
  // Accessors
  Controller* GetController() { return controller_.get(); }
  bool IsReady() const { return controller_ != nullptr; }
  const AppConfig& GetConfig() const { return config_; }

 private:
  Application() = default;
  ~Application() = default;
  
  // Non-copyable
  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;

  std::unique_ptr<Controller> controller_;
  AppConfig config_;

  // Frame timing for delta_time calculation
  std::chrono::steady_clock::time_point last_frame_time_;
  float delta_time_ = 0.0f;
  bool first_frame_ = true;

#ifndef __EMSCRIPTEN__
  // For non-WASM builds, we need a local queue for ROMs requested before
  // the controller is initialized.
  std::string pending_rom_;
#endif

  // Helper to run startup actions (jumps, card opening) after ROM load
  void RunStartupActions();

#ifdef YAZE_WITH_GRPC
  std::unique_ptr<YazeGRPCServer> grpc_server_;
  std::unique_ptr<CanvasAutomationServiceImpl> canvas_automation_service_;
  std::unique_ptr<yaze::agent::AgentControlServer> agent_control_server_;
#endif
};

}  // namespace yaze

#endif  // YAZE_APP_APPLICATION_H_
