#ifndef YAZE_SRC_CLI_SERVICE_API_HTTP_SERVER_H_
#define YAZE_SRC_CLI_SERVICE_API_HTTP_SERVER_H_

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include "absl/status/status.h"

// Forward declaration
namespace httplib {
class Server;
}

namespace yaze {
namespace cli {
namespace api {

class HttpServer {
 public:
  HttpServer();
  ~HttpServer();

  // Start the server on the specified port in a background thread.
  absl::Status Start(int port);

  // Stop the server.
  void Stop();

  // Check if server is running
  bool IsRunning() const;

 private:
  void RunServer(int port);
  void RegisterRoutes();

  std::unique_ptr<httplib::Server> server_;
  std::unique_ptr<std::thread> server_thread_;
  std::atomic<bool> is_running_{false};
};

}  // namespace api
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_API_HTTP_SERVER_H_
