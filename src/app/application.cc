#include "app/application.h"

#include <ctime>

#ifndef _WIN32
#include <unistd.h>  // getpid()
#endif

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "app/editor/core/content_registry.h"
#include "app/editor/events/core_events.h"
#include "util/log.h"

#ifdef YAZE_WITH_GRPC
#include "app/service/canvas_automation_service.h"
#include "app/service/unified_grpc_server.h"
#include "app/test/test_manager.h"
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include "app/platform/wasm/wasm_bootstrap.h"
#include "app/platform/wasm/wasm_collaboration.h"
#endif

namespace yaze {

Application& Application::Instance() {
  static Application instance;
  return instance;
}

void Application::Initialize(const AppConfig& config) {
  config_ = config;
  LOG_INFO("App", "Initializing Application instance...");

  controller_ = std::make_unique<Controller>();

  // Process pending ROM load if we have one (from flags/config - non-WASM only)
  std::string start_path = config_.rom_file;

#ifndef __EMSCRIPTEN__
  if (!pending_rom_.empty()) {
    // Pending ROM takes precedence over config (e.g. drag-drop before init)
    start_path = pending_rom_;
    pending_rom_.clear();
    LOG_INFO("App", "Found pending ROM load: %s", start_path.c_str());
  } else if (!start_path.empty()) {
    LOG_INFO("App", "Using configured startup ROM: %s", start_path.c_str());
  } else {
    LOG_INFO("App", "No pending ROM, starting empty.");
  }
#else
  LOG_INFO("App", "WASM build - ROM loading handled via wasm_bootstrap queue.");
  // In WASM, start_path from config might be ignored if we rely on web uploads
  // But we can still try to pass it if it's a server-hosted ROM
#endif

  // Always call OnEntry to initialize Window/Renderer, even with empty path
  auto status = controller_->OnEntry(start_path);
  if (!status.ok()) {
     LOG_ERROR("App", "Failed to initialize controller: %s", std::string(status.message()).c_str());
  } else {
     LOG_INFO("App", "Controller initialized successfully. Active: %s", controller_->IsActive() ? "Yes" : "No");

     if (controller_->editor_manager()) {
       controller_->editor_manager()->ApplyStartupVisibility(config_);
     }

     // If we successfully loaded a ROM at startup, run startup actions
     if (!start_path.empty() && controller_->editor_manager()) {
         RunStartupActions();
     }

#ifdef YAZE_WITH_GRPC
     // Initialize gRPC unified server
     if (config_.enable_test_harness) {
       LOG_INFO("App", "Initializing Unified gRPC Server...");
       canvas_automation_service_ = std::make_unique<CanvasAutomationServiceImpl>();
       grpc_server_ = std::make_unique<YazeGRPCServer>();

       auto rom_getter = [this]() { return controller_->GetCurrentRom(); };
       auto rom_loader = [this](const std::string& path) -> bool {
         if (!controller_ || !controller_->editor_manager()) return false;
         auto status = controller_->editor_manager()->OpenRomOrProject(path);
         return status.ok();
       };

       yaze::emu::Emulator* emulator = nullptr;
       if (controller_->editor_manager()) {
           emulator = &controller_->editor_manager()->emulator();
       } else {
           LOG_WARN("App", "EditorManager not ready; emulator gRPC services may be limited");
       }

       // Initialize server with all services
       auto status = grpc_server_->Initialize(
           config_.test_harness_port,
           emulator,
           rom_getter,
           rom_loader,
           &yaze::test::TestManager::Get(),
           nullptr, // Version manager not ready
           nullptr, // Approval manager not ready
           canvas_automation_service_.get()
       );

       if (status.ok()) {
         status = grpc_server_->StartAsync(); // Start in background thread
         if (!status.ok()) {
           LOG_ERROR("App", "Failed to start gRPC server: %s", std::string(status.message()).c_str());
         } else {
           LOG_INFO("App", "Unified gRPC server started on port %d", config_.test_harness_port);
         }
       } else {
         LOG_ERROR("App", "Failed to initialize gRPC server: %s", std::string(status.message()).c_str());
       }

       // Connect services to controller/editor manager
       if (canvas_automation_service_) {
         controller_->SetCanvasAutomationService(canvas_automation_service_.get());
       }
     }
#endif
  }

#ifdef __EMSCRIPTEN__
  // Register the ROM load handler now that controller is ready.
  yaze::app::wasm::SetRomLoadHandler([](std::string path) {
    Application::Instance().LoadRom(path);
  });
#else
  // Create activity file for instance discovery (non-WASM only)
  auto pid = getpid();
  activity_file_ = std::make_unique<app::ActivityFile>(
      absl::StrFormat("/tmp/yaze-%d.status", pid));
  UpdateActivityStatus();
  LOG_INFO("App", "Activity file created: %s", activity_file_->GetPath().c_str());
#endif
}

void Application::Tick() {
  if (!controller_) return;

  // Calculate delta time
  auto now = std::chrono::steady_clock::now();
  if (first_frame_) {
    delta_time_ = 0.016f;  // Assume ~60fps for first frame
    first_frame_ = false;
  } else {
    auto elapsed = std::chrono::duration<float>(now - last_frame_time_);
    delta_time_ = elapsed.count();
  }
  last_frame_time_ = now;

  // Publish FrameBeginEvent for pre-frame (non-ImGui) work
  if (auto* bus = editor::ContentRegistry::Context::event_bus()) {
    bus->Publish(editor::FrameBeginEvent::Create(delta_time_));
  }

#ifdef __EMSCRIPTEN__
  auto& wasm_collab = app::platform::GetWasmCollaborationInstance();
  wasm_collab.ProcessPendingChanges();
#endif

  controller_->OnInput();
  auto status = controller_->OnLoad();
  if (!status.ok()) {
    LOG_ERROR("App", "Controller Load Error: %s", std::string(status.message()).c_str());
#ifdef __EMSCRIPTEN__
    emscripten_cancel_main_loop();
#endif
    return;
  }

  if (!controller_->IsActive()) {
      // Window closed
      // LOG_INFO("App", "Controller became inactive");
  }

  controller_->DoRender();

  // Publish FrameEndEvent for cleanup operations
  if (auto* bus = editor::ContentRegistry::Context::event_bus()) {
    bus->Publish(editor::FrameEndEvent::Create(delta_time_));
  }
}

void Application::LoadRom(const std::string& path) {
  LOG_INFO("App", "Requesting ROM load: %s", path.c_str());

  if (!controller_) {
#ifdef __EMSCRIPTEN__
    yaze::app::wasm::TriggerRomLoad(path);
    LOG_INFO("App", "Forwarded to wasm_bootstrap queue (controller not ready): %s", path.c_str());
#else
    pending_rom_ = path;
    LOG_INFO("App", "Queued ROM load (controller not ready): %s", path.c_str());
#endif
    return;
  }

  // Controller exists.
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

    // Run startup actions whenever a new ROM is loaded IF it matches our startup config
    // (Optional: we might only want to run actions once at startup, but for CLI usage usually
    // you load one ROM and want the actions applied to it).
    // For now, we'll only run actions if this is the first load or if explicitly requested.
    // Actually, simpler: just run them. The user can close cards if they want.
    RunStartupActions();

#ifndef __EMSCRIPTEN__
    // Update activity file with new ROM path
    UpdateActivityStatus();
#endif

#ifdef __EMSCRIPTEN__
    EM_ASM({
      console.log("ROM loaded successfully: " + UTF8ToString($0));
    }, path.c_str());
#endif
  }
}

void Application::RunStartupActions() {
  if (!controller_ || !controller_->editor_manager()) return;

  auto* manager = controller_->editor_manager();
  manager->ProcessStartupActions(config_);
}

#ifndef __EMSCRIPTEN__
void Application::UpdateActivityStatus() {
  if (!activity_file_) return;

  app::ActivityStatus status;
  status.pid = getpid();
  status.version = YAZE_VERSION_STRING;
  status.start_timestamp = std::time(nullptr);

  if (controller_ && controller_->editor_manager()) {
    auto* rom = controller_->GetCurrentRom();
    status.active_rom = rom ? rom->filename() : "";
  }

#ifdef YAZE_WITH_GRPC
  if (config_.enable_test_harness && config_.test_harness_port > 0) {
    status.socket_path = absl::StrFormat("localhost:%d", config_.test_harness_port);
  }
#endif

  activity_file_->Update(status);
}
#endif

#ifdef __EMSCRIPTEN__
extern "C" void SyncFilesystem();
#endif

void Application::Shutdown() {
#ifdef __EMSCRIPTEN__
  // Sync IDBFS to persist any changes before shutdown
  LOG_INFO("App", "Syncing filesystem before shutdown...");
  SyncFilesystem();
#endif

#ifdef YAZE_WITH_GRPC
  if (grpc_server_) {
    LOG_INFO("App", "Shutting down Unified gRPC Server...");
    grpc_server_->Shutdown();
    grpc_server_.reset();
  }
  canvas_automation_service_.reset();
#endif

#ifndef __EMSCRIPTEN__
  // Clean up activity file for instance discovery
  if (activity_file_) {
    LOG_INFO("App", "Removing activity file: %s", activity_file_->GetPath().c_str());
    activity_file_.reset();  // Destructor deletes the file
  }
#endif

  if (controller_) {
    controller_->OnExit();
    controller_.reset();
  }
}

}  // namespace yaze
