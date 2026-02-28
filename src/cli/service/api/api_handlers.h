#ifndef YAZE_SRC_CLI_SERVICE_API_API_HANDLERS_H_
#define YAZE_SRC_CLI_SERVICE_API_API_HANDLERS_H_

#include <functional>
#include <string>

// Forward declarations to avoid exposing httplib headers everywhere
namespace httplib {
struct Request;
struct Response;
}  // namespace httplib

namespace yaze {
class Rom;

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

namespace cli {
namespace api {

// Health check endpoint
void HandleHealth(const httplib::Request& req, httplib::Response& res);

// List available models
void HandleListModels(const httplib::Request& req, httplib::Response& res);

// Get current symbol table
void HandleGetSymbols(const httplib::Request& req, httplib::Response& res,
                      yaze::emu::debug::SymbolProvider* symbols);

// Mesen2 bridge endpoints
void HandleNavigate(const httplib::Request& req, httplib::Response& res);
void HandleBreakpointHit(const httplib::Request& req, httplib::Response& res);
void HandleStateUpdate(const httplib::Request& req, httplib::Response& res);

// Window control endpoints
void HandleWindowShow(const httplib::Request& req, httplib::Response& res,
                      const std::function<bool()>& action);
void HandleWindowHide(const httplib::Request& req, httplib::Response& res,
                      const std::function<bool()>& action);

// Apply common CORS headers to a response.
void ApplyCorsHeaders(httplib::Response& res);

// Render dungeon room to PNG image.
// GET /api/v1/render/dungeon?room=<id>[&overlays=collision,sprites,...][&scale=<f>]
void HandleRenderDungeon(const httplib::Request& req, httplib::Response& res,
                         yaze::app::service::RenderService* render_service);

// Return JSON metadata for a dungeon room (no rendering).
// GET /api/v1/render/dungeon/metadata?room=<id>
void HandleRenderDungeonMetadata(
    const httplib::Request& req, httplib::Response& res,
    yaze::app::service::RenderService* render_service);

// CORS preflight handler for OPTIONS requests.
void HandleCorsPreflight(const httplib::Request& req, httplib::Response& res);

// Execute a z3ed CLI command.
// POST /api/v1/command/execute  Body: {"command":"name","args":["--flag=val"]}
void HandleCommandExecute(const httplib::Request& req, httplib::Response& res,
                          yaze::Rom* rom);

// List all registered z3ed commands.
// GET /api/v1/command/list
void HandleCommandList(const httplib::Request& req, httplib::Response& res);

// Annotation CRUD endpoints.
// GET    /api/v1/annotations[?room=<id>]
void HandleAnnotationList(const httplib::Request& req, httplib::Response& res,
                          const std::string& project_path);

// POST   /api/v1/annotations
void HandleAnnotationCreate(const httplib::Request& req, httplib::Response& res,
                            const std::string& project_path);

// PUT    /api/v1/annotations/<id>
void HandleAnnotationUpdate(const httplib::Request& req, httplib::Response& res,
                            const std::string& project_path);

// DELETE /api/v1/annotations/<id>
void HandleAnnotationDelete(const httplib::Request& req, httplib::Response& res,
                            const std::string& project_path);

}  // namespace api
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_API_API_HANDLERS_H_
