#include "cli/service/api/http_server.h"

#include <string>

#include "cli/service/api/api_handlers.h"
#include "util/log.h"

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

  port_ = port;
  is_running_ = true;
  server_thread_ = std::make_unique<std::thread>([this, port]() {
    LOG_INFO("HttpServer", "Starting API server on port %d", port);
    bool ret = server_->listen("0.0.0.0", port);
    if (!ret) {
      LOG_ERROR("HttpServer", "Failed to listen on port %d", port);
    }
    is_running_ = false;
  });

  // Publish Bonjour service for LAN discovery by iOS clients.
  bonjour_.Publish(port);
  if (bonjour_.IsAvailable()) {
    if (bonjour_.IsPublished()) {
      LOG_INFO("HttpServer", "Bonjour discovery published on port %d", port);
    } else {
      LOG_ERROR("HttpServer", "Bonjour discovery failed to publish on port %d",
                port);
    }
  } else {
    LOG_INFO("HttpServer",
             "Bonjour discovery not available on this platform, skipping");
  }

  return absl::OkStatus();
}

void HttpServer::Stop() {
  if (is_running_) {
    LOG_INFO("HttpServer", "Stopping API server...");
    bonjour_.Unpublish();
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
  server_->set_error_handler(
      [](const httplib::Request& req, httplib::Response& res) {
        (void)req;
        ApplyCorsHeaders(res);
        if (!res.body.empty()) {
          return;
        }
        const char* message = "Request failed";
        switch (res.status) {
          case 400:
            message = "Bad request";
            break;
          case 404:
            message = "Not found";
            break;
          case 405:
            message = "Method not allowed";
            break;
          case 501:
            message = "Not implemented";
            break;
          case 503:
            message = "Service unavailable";
            break;
          case 500:
            message = "Internal server error";
            break;
          default:
            break;
        }
        std::string body = std::string("{\"error\":\"") + message + "\"}";
        res.set_content(body, "application/json");
      });

  server_->Get("/api/v1/health",
               [this](const httplib::Request& req, httplib::Response& res) {
                 HandleHealth(req, res, &bonjour_);
               });
  server_->Get("/api/v1/models", HandleListModels);
  server_->Options("/api/v1/health", HandleCorsPreflight);
  server_->Options("/api/v1/models", HandleCorsPreflight);

  server_->Get("/api/v1/symbols",
               [this](const httplib::Request& req, httplib::Response& res) {
                 emu::debug::SymbolProvider* symbols = nullptr;
                 if (symbol_source_) {
                   symbols = symbol_source_();
                 }
                 HandleGetSymbols(req, res, symbols);
               });
  server_->Options("/api/v1/symbols", HandleCorsPreflight);

  server_->Post("/api/v1/navigate", HandleNavigate);
  server_->Post("/api/v1/breakpoint/hit", HandleBreakpointHit);
  server_->Post("/api/v1/state/update", HandleStateUpdate);
  server_->Options("/api/v1/navigate", HandleCorsPreflight);
  server_->Options("/api/v1/breakpoint/hit", HandleCorsPreflight);
  server_->Options("/api/v1/state/update", HandleCorsPreflight);

  server_->Post("/api/v1/window/show",
                [this](const httplib::Request& req, httplib::Response& res) {
                  HandleWindowShow(req, res, window_show_);
                });
  server_->Options("/api/v1/window/show", HandleCorsPreflight);

  server_->Post("/api/v1/window/hide",
                [this](const httplib::Request& req, httplib::Response& res) {
                  HandleWindowHide(req, res, window_hide_);
                });
  server_->Options("/api/v1/window/hide", HandleCorsPreflight);

  server_->Get("/api/v1/render/dungeon",
               [this](const httplib::Request& req, httplib::Response& res) {
                 app::service::RenderService* rs =
                     render_source_ ? render_source_() : nullptr;
                 HandleRenderDungeon(req, res, rs);
               });
  server_->Options("/api/v1/render/dungeon", HandleCorsPreflight);

  server_->Get("/api/v1/render/dungeon/metadata",
               [this](const httplib::Request& req, httplib::Response& res) {
                 app::service::RenderService* rs =
                     render_source_ ? render_source_() : nullptr;
                 HandleRenderDungeonMetadata(req, res, rs);
               });
  server_->Options("/api/v1/render/dungeon/metadata", HandleCorsPreflight);

  // Command execution endpoints (Phase 3)
  server_->Post("/api/v1/command/execute",
                [this](const httplib::Request& req, httplib::Response& res) {
                  Rom* rom = rom_source_ ? rom_source_() : nullptr;
                  HandleCommandExecute(req, res, rom);
                });
  server_->Get("/api/v1/command/list", HandleCommandList);
  server_->Options("/api/v1/command/execute", HandleCorsPreflight);
  server_->Options("/api/v1/command/list", HandleCorsPreflight);

  // Annotation CRUD endpoints (Phase 4)
  auto project_path_lambda = [this]() -> std::string {
    return project_path_source_ ? project_path_source_() : "";
  };
  server_->Get("/api/v1/annotations",
               [project_path_lambda](const httplib::Request& req,
                                     httplib::Response& res) {
                 HandleAnnotationList(req, res, project_path_lambda());
               });
  server_->Post("/api/v1/annotations",
                [project_path_lambda](const httplib::Request& req,
                                      httplib::Response& res) {
                  HandleAnnotationCreate(req, res, project_path_lambda());
                });
  server_->Put(R"(/api/v1/annotations/(.+))",
               [project_path_lambda](const httplib::Request& req,
                                     httplib::Response& res) {
                 HandleAnnotationUpdate(req, res, project_path_lambda());
               });
  server_->Delete(R"(/api/v1/annotations/(.+))",
                  [project_path_lambda](const httplib::Request& req,
                                        httplib::Response& res) {
                    HandleAnnotationDelete(req, res, project_path_lambda());
                  });
  server_->Options("/api/v1/annotations", HandleCorsPreflight);
  server_->Options(R"(/api/v1/annotations/(.+))", HandleCorsPreflight);
}

}  // namespace api
}  // namespace cli
}  // namespace yaze
