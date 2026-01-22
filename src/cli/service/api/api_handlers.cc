#include "cli/service/api/api_handlers.h"

#include "app/emu/debug/symbol_provider.h"
#include "cli/service/ai/model_registry.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "util/log.h"

namespace yaze {
namespace cli {
namespace api {

using json = nlohmann::json;

void HandleHealth(const httplib::Request& req, httplib::Response& res) {
  (void)req;
  json j;
  j["status"] = "ok";
  j["version"] = "1.0";
  j["service"] = "yaze-agent-api";

  res.set_content(j.dump(), "application/json");
  res.set_header("Access-Control-Allow-Origin", "*");
}

void HandleListModels(const httplib::Request& req, httplib::Response& res) {
  (void)req;
  auto& registry = ModelRegistry::GetInstance();
  auto models_or = registry.ListAllModels();

  res.set_header("Access-Control-Allow-Origin", "*");

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

  res.set_content(response.dump(), "application/json");
}

void HandleGetSymbols(const httplib::Request& req, httplib::Response& res,
                      yaze::emu::debug::SymbolProvider* symbols) {
  res.set_header("Access-Control-Allow-Origin", "*");

  if (!symbols) {
    json j;
    j["error"] = "Symbol provider not available";
    res.status = 503;
    res.set_content(j.dump(), "application/json");
    return;
  }

  std::string format_str = req.get_param_value("format");
  if (format_str.empty()) format_str = "mesen";

  yaze::emu::debug::SymbolFormat format = yaze::emu::debug::SymbolFormat::kMesen;
  if (format_str == "asar") format = yaze::emu::debug::SymbolFormat::kAsar;
  else if (format_str == "wla") format = yaze::emu::debug::SymbolFormat::kWlaDx;
  else if (format_str == "bsnes") format = yaze::emu::debug::SymbolFormat::kBsnes;

  auto export_or = symbols->ExportSymbols(format);
  if (export_or.ok()) {
    if (req.has_header("Accept") && req.get_header_value("Accept") == "application/json") {
      json j;
      j["symbols"] = *export_or;
      j["format"] = format_str;
      res.set_content(j.dump(), "application/json");
    } else {
      res.set_content(*export_or, "text/plain");
    }
  } else {
    json j;
    j["error"] = export_or.status().message();
    res.status = 500;
    res.set_content(j.dump(), "application/json");
  }
}

}  // namespace api
}  // namespace cli
}  // namespace yaze
