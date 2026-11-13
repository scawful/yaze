#ifndef YAZE_SRC_CLI_SERVICE_API_HTTP_SERVER_H_
#define YAZE_SRC_CLI_SERVICE_API_HTTP_SERVER_H_

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cli/service/api/bonjour_publisher.h"

namespace httplib {
class Server;
}

namespace yaze {
namespace emu {
namespace debug {
class SymbolProvider;
}
}  // namespace emu

namespace app {
namespace service {
class RenderService;
}
}  // namespace app

class Rom;
}  // namespace yaze

namespace yaze {
namespace cli {
namespace api {

class HttpServer {
 public:
  using SymbolProviderSource = std::function<emu::debug::SymbolProvider*()>;
  using RenderServiceSource = std::function<app::service::RenderService*()>;
  using RomSource = std::function<Rom*()>;
  using ProjectPathSource = std::function<std::string()>;
  using WindowAction = std::function<bool()>;
  using EditorControl =
      std::function<absl::StatusOr<std::string>(const std::string&)>;
  void SetEditorControl(EditorControl control) {
    editor_control_ = std::move(control);
  }

  HttpServer();
  ~HttpServer();

  // Start the server on the specified port in a background thread.
  // `loopback_only` binds 127.0.0.1 and overrides the bind address.
  absl::Status Start(int port, bool loopback_only = false);

  // Address to bind when not loopback-only. Call before Start().
  void SetBindAddress(std::string address) {
    bind_address_ = std::move(address);
  }

  // When disabled, POST /api/v1/command/execute responds 403. Call before
  // Start().
  void SetCommandExecutionEnabled(bool enabled) {
    command_execution_enabled_ = enabled;
  }

  // Stop the server.
  void Stop();

  // Check if server is running
  bool IsRunning() const;

  // Set the source for symbols
  void SetSymbolProviderSource(SymbolProviderSource source) {
    symbol_source_ = std::move(source);
  }

  // Set the source for the render service (headless dungeon rendering).
  void SetRenderServiceSource(RenderServiceSource source) {
    render_source_ = std::move(source);
  }

  // Set the source for ROM context (used by command execution).
  void SetRomSource(RomSource source) { rom_source_ = std::move(source); }

  // Set the source for project path (used by annotation endpoints).
  void SetProjectPathSource(ProjectPathSource source) {
    project_path_source_ = std::move(source);
  }

  // Get current port.
  int port() const { return port_; }

  // Optional window control hooks (GUI builds only)
  void SetWindowActions(WindowAction show, WindowAction hide) {
    window_show_ = std::move(show);
    window_hide_ = std::move(hide);
  }

  // Get current symbol source
  SymbolProviderSource GetSymbolSource() const { return symbol_source_; }

  // Access the Bonjour publisher (e.g. to update ROM title).
  BonjourPublisher& GetBonjourPublisher() { return bonjour_; }

 private:
  void RunServer(int port);
  void RegisterRoutes();

  std::unique_ptr<httplib::Server> server_;
  std::unique_ptr<std::thread> server_thread_;
  std::atomic<bool> is_running_{false};
  int port_ = 0;
  bool editor_control_only_ = false;
  std::string bind_address_ = "0.0.0.0";
  bool command_execution_enabled_ = true;
  BonjourPublisher bonjour_;
  SymbolProviderSource symbol_source_;
  RenderServiceSource render_source_;
  RomSource rom_source_;
  ProjectPathSource project_path_source_;
  WindowAction window_show_;
  WindowAction window_hide_;
  EditorControl editor_control_;
};

}  // namespace api
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_API_HTTP_SERVER_H_
