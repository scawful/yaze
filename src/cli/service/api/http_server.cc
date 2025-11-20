#include "cli/service/api/http_server.h"

#include "cli/service/api/api_handlers.h"
#include "util/log.h"

// Include httplib implementation in one file or just use the header if
// header-only usually cpp-httplib is header only, so just including is enough.
// However, we need to be careful about multiple definitions if we include it in
// multiple .cc files without precautions? cpp-httplib is header only.
#include "httplib.h"

namespace yaze {
namespace cli {
namespace api {

HttpServer::HttpServer() : server_(std::make_unique<httplib::Server>()) {}

HttpServer::~HttpServer() { Stop(); }

absl::Status HttpServer::Start(int port) {
  if (is_running_) {
    return absl::AlreadyExistsError("Server is already running");
  }

  if (!server_->is_valid()) {
    return absl::InternalError("HttpServer is not valid");
  }

  RegisterRoutes();

  // Start server in a separate thread
  is_running_ = true;

  // Capture server pointer to avoid race conditions if 'this' is destroyed
  // (though HttpServer shouldn't be)
  server_thread_ = std::make_unique<std::thread>([this, port]() {
    LOG_INFO("HttpServer", "Starting API server on port %d", port);
    bool ret = server_->listen("0.0.0.0", port);
    if (!ret) {
      LOG_ERROR("HttpServer",
                "Failed to listen on port %d. Port might be in use.", port);
    }
    is_running_ = false;
  });

  return absl::OkStatus();
}

void HttpServer::Stop() {
  if (is_running_) {
    LOG_INFO("HttpServer", "Stopping API server...");
    server_->stop();
    if (server_thread_ && server_thread_->joinable()) {
      server_thread_->join();
    }
    is_running_ = false;
    LOG_INFO("HttpServer", "API server stopped");
  }
}

bool HttpServer::IsRunning() const { return is_running_; }

void HttpServer::RegisterRoutes() {
  server_->Get("/api/v1/health", HandleHealth);
  server_->Get("/api/v1/models", HandleListModels);

  // Handle CORS options for all routes?
  // For now, we set CORS headers in individual handlers or via a middleware if
  // httplib supported it easily.
}

}  // namespace api
}  // namespace cli
}  // namespace yaze
