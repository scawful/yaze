#ifndef YAZE_SRC_CLI_SERVICE_API_HTTP_SERVER_H_
#define YAZE_SRC_CLI_SERVICE_API_HTTP_SERVER_H_

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "absl/status/status.h"

namespace httplib {
class Server;
}

namespace yaze {
namespace emu {
namespace debug {
class SymbolProvider;
}
}
}

namespace yaze {
namespace cli {
namespace api {

class HttpServer {
 public:
  using SymbolProviderSource = std::function<emu::debug::SymbolProvider*()>;
  using WindowAction = std::function<bool()>;

  HttpServer();
  ~HttpServer();

  // Start the server on the specified port in a background thread.
  absl::Status Start(int port);

  // Stop the server.
  void Stop();

  // Check if server is running
  bool IsRunning() const;

  // Set the source for symbols
  void SetSymbolProviderSource(SymbolProviderSource source) {
    symbol_source_ = std::move(source);
  }

  // Optional window control hooks (GUI builds only)
  void SetWindowActions(WindowAction show, WindowAction hide) {
    window_show_ = std::move(show);
    window_hide_ = std::move(hide);
  }

  // Get current symbol source
  SymbolProviderSource GetSymbolSource() const { return symbol_source_; }

 private:
  void RunServer(int port);
  void RegisterRoutes();

  std::unique_ptr<httplib::Server> server_;
  std::unique_ptr<std::thread> server_thread_;
  std::atomic<bool> is_running_{false};
  SymbolProviderSource symbol_source_;
  WindowAction window_show_;
  WindowAction window_hide_;
};

}  // namespace api
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_API_HTTP_SERVER_H_
