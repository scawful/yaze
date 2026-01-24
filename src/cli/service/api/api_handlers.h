#ifndef YAZE_SRC_CLI_SERVICE_API_API_HANDLERS_H_
#define YAZE_SRC_CLI_SERVICE_API_API_HANDLERS_H_

#include <string>

// Forward declarations to avoid exposing httplib headers everywhere
namespace httplib {
struct Request;
struct Response;
}  // namespace httplib

namespace yaze {
namespace emu {
namespace debug {
class SymbolProvider;
}
}

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

}  // namespace api
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_API_API_HANDLERS_H_
