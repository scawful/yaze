#include "cli/service/api/api_handlers.h"

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

}  // namespace api
}  // namespace cli
}  // namespace yaze
