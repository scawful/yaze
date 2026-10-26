#ifndef YAZE_APP_SERVICE_EDITOR_HTTP_SERVER_H_
#define YAZE_APP_SERVICE_EDITOR_HTTP_SERVER_H_

#if !defined(__EMSCRIPTEN__) && defined(YAZE_HTTP_API_ENABLED)

#include <memory>

#include "app/application.h"
#include "cli/service/api/http_server.h"
#include "util/log.h"

namespace yaze::app {

// Both Cocoa and the ordinary desktop loop own this server locally. Create it
// after Initialize succeeds, and Stop it before Application::Shutdown destroys
// the controller captured by its callbacks.
inline std::unique_ptr<cli::api::HttpServer> SetupEditorHttpServer(
    const AppConfig& config) {
  auto* controller = Application::Instance().GetController();
  if (!config.enable_api || !controller || !controller->IsActive()) {
    return nullptr;
  }

  auto server = std::make_unique<cli::api::HttpServer>();
  server->SetSymbolProviderSource(
      [controller]() -> emu::debug::SymbolProvider* {
        auto* manager = controller->editor_manager();
        return manager ? &manager->emulator().symbol_provider() : nullptr;
      });
  server->SetWindowActions(
      [controller]() {
        controller->ShowWindow();
        return true;
      },
      [controller]() {
        controller->HideWindow();
        return false;
      });

  if (config.editor_control) {
    server->SetEditorControl([controller](const std::string& request) {
      return controller->RequestEditorControl(request);
    });
  }

  auto status = server->Start(config.api_port, config.editor_control);
  if (!status.ok()) {
    LOG_ERROR("App", "Failed to start API server: %s",
              std::string(status.message()).c_str());
    return nullptr;
  }
  LOG_INFO("App", "API Server started on port %d", config.api_port);
  return server;
}

}  // namespace yaze::app
#endif
#endif
