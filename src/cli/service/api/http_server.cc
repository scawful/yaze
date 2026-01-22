#include "cli/service/api/http_server.h"

#include "cli/service/api/api_handlers.h"
#include "util/log.h"
#include "app/application.h"

#include "httplib.h"

namespace yaze {
namespace cli {
namespace api {

HttpServer::HttpServer() : server_(std::make_unique<httplib::Server>()) {}

HttpServer::~HttpServer() {
  Stop();
}

absl::Status HttpServer::Start(int port) {
  if (is_running_) {
    return absl::AlreadyExistsError("Server is already running");
  }

  if (!server_->is_valid()) {
    return absl::InternalError("HttpServer is not valid");
  }

  RegisterRoutes();

  is_running_ = true;
  server_thread_ = std::make_unique<std::thread>([this, port]() {
    LOG_INFO("HttpServer", "Starting API server on port %d", port);
    bool ret = server_->listen("0.0.0.0", port);
    if (!ret) {
      LOG_ERROR("HttpServer", "Failed to listen on port %d", port);
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

bool HttpServer::IsRunning() const {
  return is_running_;
}

void HttpServer::RegisterRoutes() {
  server_->Get("/api/v1/health", HandleHealth);
  server_->Get("/api/v1/models", HandleListModels);

  server_->Get("/api/v1/symbols", [this](const httplib::Request& req, httplib::Response& res) {
    emu::debug::SymbolProvider* symbols = nullptr;
    if (symbol_source_) {
      symbols = symbol_source_();
    }
    HandleGetSymbols(req, res, symbols);
  });

  server_->Post("/api/v1/window/show",
                [](const httplib::Request&, httplib::Response& res) {
                  auto* ctrl = Application::Instance().GetController();
                  if (ctrl) {
                    ctrl->ShowWindow();
                    res.set_content(R"({"status":"ok", "window":"shown"})",
                                    "application/json");
                  } else {
                    res.status = 500;
                    res.set_content(
                        R"({"status":"error", "message":"controller not ready"})",
                        "application/json");
                  }
                });

  server_->Post("/api/v1/window/hide",
                [](const httplib::Request&, httplib::Response& res) {
                  auto* ctrl = Application::Instance().GetController();
                  if (ctrl) {
                    ctrl->HideWindow();
                    res.set_content(R"({"status":"ok", "window":"hidden"})",
                                    "application/json");
                  } else {
                    res.status = 500;
                    res.set_content(
                        R"({"status":"error", "message":"controller not ready"})",
                        "application/json");
                  }
                });
}

}  // namespace api
}  // namespace cli
}  // namespace yaze
