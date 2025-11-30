#include "app/application.h"

#include "absl/strings/str_cat.h"
#include "util/log.h"

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
     
     // If we successfully loaded a ROM at startup, run startup actions
     if (!start_path.empty() && controller_->editor_manager()) {
         RunStartupActions();
     }
  }

#ifdef __EMSCRIPTEN__
  // Register the ROM load handler now that controller is ready.
  yaze::app::wasm::SetRomLoadHandler([](std::string path) {
    Application::Instance().LoadRom(path);
  });
#endif
}

void Application::Tick() {
  if (!controller_) return;

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

void Application::Shutdown() {
#ifdef __EMSCRIPTEN__
  // Sync IDBFS to persist any changes before shutdown
  LOG_INFO("App", "Syncing filesystem before shutdown...");
  extern "C" void SyncFilesystem();
  SyncFilesystem();
#endif

  if (controller_) {
    controller_->OnExit();
    controller_.reset();
  }
}

}  // namespace yaze
