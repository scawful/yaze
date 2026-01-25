#include "cli/service/api/api_handlers.h"

#include <algorithm>
#include <cctype>

#include "app/emu/debug/symbol_provider.h"
#include "cli/service/ai/model_registry.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "util/log.h"

namespace yaze {
namespace cli {
namespace api {

using json = nlohmann::json;

namespace {

constexpr const char* kCorsAllowOrigin = "*";
constexpr const char* kCorsAllowHeaders = "Content-Type, Authorization, Accept";
constexpr const char* kCorsAllowMethods = "GET, POST, OPTIONS";
constexpr const char* kCorsMaxAge = "86400";

bool IsTruthyParam(const std::string& value, bool param_present) {
  if (value.empty()) {
    return param_present;
  }
  std::string normalized = value;
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char c) {
                   return static_cast<char>(std::tolower(c));
                 });
  return normalized == "1" || normalized == "true" || normalized == "yes" ||
         normalized == "on";
}

}  // namespace

void ApplyCorsHeaders(httplib::Response& res) {
  res.set_header("Access-Control-Allow-Origin", kCorsAllowOrigin);
  res.set_header("Access-Control-Allow-Headers", kCorsAllowHeaders);
  res.set_header("Access-Control-Allow-Methods", kCorsAllowMethods);
  res.set_header("Access-Control-Max-Age", kCorsMaxAge);
}

namespace {

void HandleWindowAction(const std::function<bool()>& action,
                        const char* window_state,
                        httplib::Response& res) {
  ApplyCorsHeaders(res);

  if (action) {
    if (action()) {
      json response;
      response["status"] = "ok";
      response["window"] = window_state;
      res.status = 200;
      res.set_content(response.dump(), "application/json");
      return;
    }
    res.status = 500;
    json response;
    response["status"] = "error";
    response["message"] = "window action failed";
    res.set_content(response.dump(), "application/json");
    return;
  }

  res.status = 501;
  json response;
  response["status"] = "error";
  response["message"] = "window control unavailable";
  res.set_content(response.dump(), "application/json");
}

}  // namespace

void HandleHealth(const httplib::Request& req, httplib::Response& res) {
  (void)req;
  json j;
  j["status"] = "ok";
  j["version"] = "1.0";
  j["service"] = "yaze-agent-api";

  res.status = 200;
  res.set_content(j.dump(), "application/json");
  ApplyCorsHeaders(res);
}

void HandleListModels(const httplib::Request& req, httplib::Response& res) {
  auto& registry = ModelRegistry::GetInstance();
  const bool force_refresh = IsTruthyParam(req.get_param_value("refresh"),
                                           req.has_param("refresh"));
  auto models_or = registry.ListAllModels(force_refresh);

  ApplyCorsHeaders(res);

  if (!models_or.ok()) {
    json j;
    j["error"] = models_or.status().message();
    res.status = 500;
    res.set_content(j.dump(), "application/json");
    return;
  }

  json j_models = json::array();
  for (const auto& info : *models_or) {
    json j_model;
    j_model["name"] = info.name;
    j_model["display_name"] = info.display_name;
    j_model["provider"] = info.provider;
    j_model["description"] = info.description;
    j_model["family"] = info.family;
    j_model["parameter_size"] = info.parameter_size;
    j_model["quantization"] = info.quantization;
    j_model["size_bytes"] = info.size_bytes;
    j_model["is_local"] = info.is_local;
    j_models.push_back(j_model);
  }

  json response;
  response["models"] = j_models;
  response["count"] = j_models.size();

  res.status = 200;
  res.set_content(response.dump(), "application/json");
}

void HandleGetSymbols(const httplib::Request& req, httplib::Response& res,
                      yaze::emu::debug::SymbolProvider* symbols) {
  ApplyCorsHeaders(res);

  std::string format_str = req.get_param_value("format");
  if (format_str.empty()) format_str = "mesen";

  yaze::emu::debug::SymbolFormat format = yaze::emu::debug::SymbolFormat::kMesen;
  if (format_str == "mesen") {
    format = yaze::emu::debug::SymbolFormat::kMesen;
  } else if (format_str == "asar") {
    format = yaze::emu::debug::SymbolFormat::kAsar;
  } else if (format_str == "wla") {
    format = yaze::emu::debug::SymbolFormat::kWlaDx;
  } else if (format_str == "bsnes") {
    format = yaze::emu::debug::SymbolFormat::kBsnes;
  } else {
    json j;
    j["error"] = "Unsupported symbol format";
    j["format"] = format_str;
    j["supported"] = {"mesen", "asar", "wla", "bsnes"};
    res.status = 400;
    res.set_content(j.dump(), "application/json");
    return;
  }

  if (!symbols) {
    json j;
    j["error"] = "Symbol provider not available";
    res.status = 503;
    res.set_content(j.dump(), "application/json");
    return;
  }

  auto export_or = symbols->ExportSymbols(format);
  if (export_or.ok()) {
    const std::string accept = req.has_header("Accept")
                                   ? req.get_header_value("Accept")
                                   : std::string();
    const bool wants_json =
        accept.find("application/json") != std::string::npos;
    if (wants_json) {
      json j;
      j["symbols"] = *export_or;
      j["format"] = format_str;
      res.status = 200;
      res.set_content(j.dump(), "application/json");
    } else {
      res.status = 200;
      res.set_content(*export_or, "text/plain");
    }
  } else {
    json j;
    j["error"] = export_or.status().message();
    res.status = 500;
    res.set_content(j.dump(), "application/json");
  }
}

void HandleNavigate(const httplib::Request& req, httplib::Response& res) {
  ApplyCorsHeaders(res);

  try {
    json body = json::parse(req.body);
    uint32_t address = body.value("address", 0);
    std::string source = body.value("source", "unknown");

    (void)source;  // Used in response
    // Navigate request logged via JSON response

    // TODO: Integrate with yaze's disassembly viewer to jump to address
    // For now, just acknowledge the request
    json response;
    response["status"] = "ok";
    response["address"] = address;
    response["message"] = "Navigation request received";
    res.status = 200;
    res.set_content(response.dump(), "application/json");
  } catch (const std::exception& e) {
    json j;
    j["error"] = e.what();
    res.status = 400;
    res.set_content(j.dump(), "application/json");
  }
}

void HandleBreakpointHit(const httplib::Request& req, httplib::Response& res) {
  ApplyCorsHeaders(res);

  try {
    json body = json::parse(req.body);
    uint32_t address = body.value("address", 0);
    std::string source = body.value("source", "unknown");

    (void)source;  // Used in response

    // Log CPU state if provided (for debugging, stored in response)
    json cpu_info;
    if (body.contains("cpu_state")) {
      cpu_info = body["cpu_state"];
    }

    // TODO: Integrate with RomDebugAgent for analysis
    json response;
    response["status"] = "ok";
    response["address"] = address;
    res.status = 200;
    res.set_content(response.dump(), "application/json");
  } catch (const std::exception& e) {
    json j;
    j["error"] = e.what();
    res.status = 400;
    res.set_content(j.dump(), "application/json");
  }
}

void HandleStateUpdate(const httplib::Request& req, httplib::Response& res) {
  ApplyCorsHeaders(res);

  try {
    json body = json::parse(req.body);

    // State update received; store for panel use
    (void)body;  // Will be used when state storage is implemented

    // TODO: Store state for use by MesenDebugPanel and RomDebugAgent
    json response;
    response["status"] = "ok";
    res.status = 200;
    res.set_content(response.dump(), "application/json");
  } catch (const std::exception& e) {
    json j;
    j["error"] = e.what();
    res.status = 400;
    res.set_content(j.dump(), "application/json");
  }
}

void HandleWindowShow(const httplib::Request& req, httplib::Response& res,
                      const std::function<bool()>& action) {
  (void)req;
  HandleWindowAction(action, "shown", res);
}

void HandleWindowHide(const httplib::Request& req, httplib::Response& res,
                      const std::function<bool()>& action) {
  (void)req;
  HandleWindowAction(action, "hidden", res);
}

void HandleCorsPreflight(const httplib::Request& req, httplib::Response& res) {
  (void)req;
  ApplyCorsHeaders(res);
  res.status = 204;
}

}  // namespace api
}  // namespace cli
}  // namespace yaze
