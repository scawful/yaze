#include "cli/service/api/api_handlers.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#include <fstream>

#include "absl/status/status.h"
#include "app/emu/debug/symbol_provider.h"
#include "app/service/render_service.h"
#include "cli/service/ai/model_registry.h"
#include "cli/service/command_registry.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
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
  std::transform(
      normalized.begin(), normalized.end(), normalized.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
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
                        const char* window_state, httplib::Response& res) {
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
  const bool force_refresh =
      IsTruthyParam(req.get_param_value("refresh"), req.has_param("refresh"));
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
  if (format_str.empty())
    format_str = "mesen";

  yaze::emu::debug::SymbolFormat format =
      yaze::emu::debug::SymbolFormat::kMesen;
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

// ---------------------------------------------------------------------------
// Render endpoints
// ---------------------------------------------------------------------------

namespace {

// Parse "room" query param — accepts decimal or 0x-prefixed hex.
// Returns false and sets a 400 error response on failure.
bool ParseRoomParam(const httplib::Request& req, httplib::Response& res,
                    int& room_id) {
  if (!req.has_param("room")) {
    json j;
    j["error"] = "Missing required parameter: room";
    res.status = 400;
    res.set_content(j.dump(), "application/json");
    return false;
  }
  const std::string s = req.get_param_value("room");
  try {
    room_id = std::stoi(s, nullptr, 0);
  } catch (...) {
    json j;
    j["error"] = "Invalid room parameter";
    j["value"] = s;
    res.status = 400;
    res.set_content(j.dump(), "application/json");
    return false;
  }
  return true;
}

}  // namespace

void HandleRenderDungeon(const httplib::Request& req, httplib::Response& res,
                         yaze::app::service::RenderService* render_service) {
  ApplyCorsHeaders(res);

  if (!render_service) {
    json j;
    j["error"] = "Render service not available";
    res.status = 503;
    res.set_content(j.dump(), "application/json");
    return;
  }

  int room_id = 0;
  if (!ParseRoomParam(req, res, room_id))
    return;

  // Parse overlays — comma-separated: collision, sprites, objects, track,
  // camera, grid, all.
  uint32_t overlay_flags = yaze::app::service::RenderOverlay::kNone;
  if (req.has_param("overlays")) {
    std::istringstream ss(req.get_param_value("overlays"));
    std::string tok;
    while (std::getline(ss, tok, ',')) {
      tok.erase(0, tok.find_first_not_of(" \t"));
      if (!tok.empty())
        tok.erase(tok.find_last_not_of(" \t") + 1);
      if (tok == "collision")
        overlay_flags |= yaze::app::service::RenderOverlay::kCollision;
      else if (tok == "sprites")
        overlay_flags |= yaze::app::service::RenderOverlay::kSprites;
      else if (tok == "objects")
        overlay_flags |= yaze::app::service::RenderOverlay::kObjects;
      else if (tok == "track")
        overlay_flags |= yaze::app::service::RenderOverlay::kTrack;
      else if (tok == "camera")
        overlay_flags |= yaze::app::service::RenderOverlay::kCameraQuads;
      else if (tok == "grid")
        overlay_flags |= yaze::app::service::RenderOverlay::kGrid;
      else if (tok == "all")
        overlay_flags = yaze::app::service::RenderOverlay::kAll;
    }
  }

  // Parse scale (clamped 0.25–8.0, default 1.0).
  float scale = 1.0f;
  if (req.has_param("scale")) {
    try {
      scale = std::stof(req.get_param_value("scale"));
      if (scale < 0.25f)
        scale = 0.25f;
      if (scale > 8.0f)
        scale = 8.0f;
    } catch (...) {}
  }

  yaze::app::service::RenderRequest render_req;
  render_req.room_id = room_id;
  render_req.overlay_flags = overlay_flags;
  render_req.scale = scale;

  auto result_or = render_service->RenderDungeonRoom(render_req);
  if (!result_or.ok()) {
    const auto& s = result_or.status();
    json j;
    j["error"] = std::string(s.message());
    if (s.code() == absl::StatusCode::kInvalidArgument)
      res.status = 400;
    else if (s.code() == absl::StatusCode::kFailedPrecondition)
      res.status = 503;
    else
      res.status = 500;
    res.set_content(j.dump(), "application/json");
    return;
  }

  const auto& result = *result_or;
  res.status = 200;
  res.set_content(reinterpret_cast<const char*>(result.png_data.data()),
                  result.png_data.size(), "image/png");
  res.set_header("X-Room-Id", std::to_string(room_id));
  res.set_header("X-Room-Width", std::to_string(result.width));
  res.set_header("X-Room-Height", std::to_string(result.height));
}

void HandleRenderDungeonMetadata(
    const httplib::Request& req, httplib::Response& res,
    yaze::app::service::RenderService* render_service) {
  ApplyCorsHeaders(res);

  if (!render_service) {
    json j;
    j["error"] = "Render service not available";
    res.status = 503;
    res.set_content(j.dump(), "application/json");
    return;
  }

  int room_id = 0;
  if (!ParseRoomParam(req, res, room_id))
    return;

  auto meta_or = render_service->GetDungeonRoomMetadata(room_id);
  if (!meta_or.ok()) {
    const auto& s = meta_or.status();
    json j;
    j["error"] = std::string(s.message());
    res.status = (s.code() == absl::StatusCode::kInvalidArgument) ? 400 : 500;
    res.set_content(j.dump(), "application/json");
    return;
  }

  const auto& meta = *meta_or;
  json j;
  j["room_id"] = meta.room_id;
  j["blockset"] = meta.blockset;
  j["spriteset"] = meta.spriteset;
  j["palette"] = meta.palette;
  j["layout_id"] = meta.layout_id;
  j["effect"] = meta.effect;
  j["collision"] = meta.collision;
  j["tag1"] = meta.tag1;
  j["tag2"] = meta.tag2;
  j["message_id"] = meta.message_id;
  j["has_custom_collision"] = meta.has_custom_collision;
  j["object_count"] = meta.object_count;
  j["sprite_count"] = meta.sprite_count;
  res.status = 200;
  res.set_content(j.dump(), "application/json");
}

// ---------------------------------------------------------------------------
// Command execution endpoints (Phase 3)
// ---------------------------------------------------------------------------

void HandleCommandExecute(const httplib::Request& req, httplib::Response& res,
                          yaze::Rom* rom) {
  ApplyCorsHeaders(res);

  try {
    json body = json::parse(req.body);
    std::string command_name = body.value("command", "");
    if (command_name.empty()) {
      json j;
      j["error"] = "Missing 'command' field";
      res.status = 400;
      res.set_content(j.dump(), "application/json");
      return;
    }

    std::vector<std::string> args;
    if (body.contains("args") && body["args"].is_array()) {
      for (const auto& arg : body["args"]) {
        args.push_back(arg.get<std::string>());
      }
    }

    auto& registry = CommandRegistry::Instance();
    if (!registry.HasCommand(command_name)) {
      json j;
      j["error"] = "Unknown command: " + command_name;
      res.status = 404;
      res.set_content(j.dump(), "application/json");
      return;
    }

    std::string captured_output;
    auto status = registry.Execute(command_name, args, rom, &captured_output);

    json response;
    if (status.ok()) {
      response["status"] = "ok";
      response["command"] = command_name;
      // Try to parse output as JSON, fall back to string
      try {
        response["result"] = json::parse(captured_output);
      } catch (...) {
        response["result"] = captured_output;
      }
      res.status = 200;
    } else {
      response["status"] = "error";
      response["command"] = command_name;
      response["error"] = std::string(status.message());
      if (!captured_output.empty()) {
        response["output"] = captured_output;
      }
      res.status = 500;
    }
    res.set_content(response.dump(), "application/json");
  } catch (const std::exception& e) {
    json j;
    j["error"] = e.what();
    res.status = 400;
    res.set_content(j.dump(), "application/json");
  }
}

void HandleCommandList(const httplib::Request& req, httplib::Response& res) {
  (void)req;
  ApplyCorsHeaders(res);

  auto& registry = CommandRegistry::Instance();
  json j_commands = json::array();

  for (const auto& category : registry.GetCategories()) {
    for (const auto& cmd_name : registry.GetCommandsInCategory(category)) {
      const auto* meta = registry.GetMetadata(cmd_name);
      if (!meta)
        continue;

      json j_cmd;
      j_cmd["name"] = meta->name;
      j_cmd["category"] = meta->category;
      j_cmd["description"] = meta->description;
      j_cmd["usage"] = meta->usage;
      j_cmd["requires_rom"] = meta->requires_rom;
      j_commands.push_back(j_cmd);
    }
  }

  json response;
  response["commands"] = j_commands;
  response["count"] = j_commands.size();
  res.status = 200;
  res.set_content(response.dump(), "application/json");
}

// ---------------------------------------------------------------------------
// Annotation CRUD endpoints (Phase 4)
// ---------------------------------------------------------------------------

namespace {

std::string ResolveAnnotationsPath(const std::string& project_path) {
  if (project_path.empty())
    return "";
  // Match the iOS AnnotationStore resolution logic
  return project_path + "/Docs/Dev/Planning/annotations.json";
}

json LoadAnnotationsFile(const std::string& path) {
  if (path.empty())
    return json::object();
  std::ifstream file(path);
  if (!file.is_open())
    return json::object();
  try {
    return json::parse(file);
  } catch (...) {
    return json::object();
  }
}

bool SaveAnnotationsFile(const std::string& path, const json& data) {
  if (path.empty())
    return false;

  // Ensure parent directory exists
  auto last_slash = path.find_last_of('/');
  if (last_slash != std::string::npos) {
    std::string dir = path.substr(0, last_slash);
    // Use mkdir -p equivalent (simple approach)
    std::string cmd = "mkdir -p '" + dir + "'";
    (void)system(cmd.c_str());
  }

  std::ofstream file(path);
  if (!file.is_open())
    return false;
  file << data.dump(2);
  return file.good();
}

}  // namespace

void HandleAnnotationList(const httplib::Request& req, httplib::Response& res,
                          const std::string& project_path) {
  ApplyCorsHeaders(res);

  std::string path = ResolveAnnotationsPath(project_path);
  if (path.empty()) {
    json j;
    j["error"] = "No project path configured";
    res.status = 503;
    res.set_content(j.dump(), "application/json");
    return;
  }

  json file_data = LoadAnnotationsFile(path);
  json annotations = file_data.value("annotations", json::array());

  // Optional room filter
  if (req.has_param("room")) {
    int room_id = 0;
    try {
      room_id = std::stoi(req.get_param_value("room"), nullptr, 0);
    } catch (...) {
      json j;
      j["error"] = "Invalid room parameter";
      res.status = 400;
      res.set_content(j.dump(), "application/json");
      return;
    }

    json filtered = json::array();
    for (const auto& ann : annotations) {
      if (ann.value("room_id", -1) == room_id) {
        filtered.push_back(ann);
      }
    }
    annotations = filtered;
  }

  json response;
  response["annotations"] = annotations;
  res.status = 200;
  res.set_content(response.dump(), "application/json");
}

void HandleAnnotationCreate(const httplib::Request& req, httplib::Response& res,
                            const std::string& project_path) {
  ApplyCorsHeaders(res);

  std::string path = ResolveAnnotationsPath(project_path);
  if (path.empty()) {
    json j;
    j["error"] = "No project path configured";
    res.status = 503;
    res.set_content(j.dump(), "application/json");
    return;
  }

  try {
    json new_annotation = json::parse(req.body);
    json file_data = LoadAnnotationsFile(path);

    if (!file_data.contains("annotations")) {
      file_data["annotations"] = json::array();
    }
    file_data["annotations"].push_back(new_annotation);

    if (SaveAnnotationsFile(path, file_data)) {
      json response;
      response["status"] = "ok";
      response["id"] = new_annotation.value("id", "");
      res.status = 201;
      res.set_content(response.dump(), "application/json");
    } else {
      json j;
      j["error"] = "Failed to write annotations file";
      res.status = 500;
      res.set_content(j.dump(), "application/json");
    }
  } catch (const std::exception& e) {
    json j;
    j["error"] = e.what();
    res.status = 400;
    res.set_content(j.dump(), "application/json");
  }
}

void HandleAnnotationUpdate(const httplib::Request& req, httplib::Response& res,
                            const std::string& project_path) {
  ApplyCorsHeaders(res);

  std::string path = ResolveAnnotationsPath(project_path);
  if (path.empty()) {
    json j;
    j["error"] = "No project path configured";
    res.status = 503;
    res.set_content(j.dump(), "application/json");
    return;
  }

  // Extract annotation ID from URL path
  std::string annotation_id;
  if (!req.matches.empty() && req.matches.size() > 1) {
    annotation_id = req.matches[1].str();
  }
  if (annotation_id.empty()) {
    json j;
    j["error"] = "Missing annotation ID in URL";
    res.status = 400;
    res.set_content(j.dump(), "application/json");
    return;
  }

  try {
    json updated = json::parse(req.body);
    json file_data = LoadAnnotationsFile(path);
    json& annotations = file_data["annotations"];

    bool found = false;
    for (auto& ann : annotations) {
      if (ann.value("id", "") == annotation_id) {
        // Merge updated fields
        for (auto it = updated.begin(); it != updated.end(); ++it) {
          ann[it.key()] = it.value();
        }
        found = true;
        break;
      }
    }

    if (!found) {
      json j;
      j["error"] = "Annotation not found: " + annotation_id;
      res.status = 404;
      res.set_content(j.dump(), "application/json");
      return;
    }

    if (SaveAnnotationsFile(path, file_data)) {
      json response;
      response["status"] = "ok";
      response["id"] = annotation_id;
      res.status = 200;
      res.set_content(response.dump(), "application/json");
    } else {
      json j;
      j["error"] = "Failed to write annotations file";
      res.status = 500;
      res.set_content(j.dump(), "application/json");
    }
  } catch (const std::exception& e) {
    json j;
    j["error"] = e.what();
    res.status = 400;
    res.set_content(j.dump(), "application/json");
  }
}

void HandleAnnotationDelete(const httplib::Request& req, httplib::Response& res,
                            const std::string& project_path) {
  ApplyCorsHeaders(res);
  (void)req;

  std::string path = ResolveAnnotationsPath(project_path);
  if (path.empty()) {
    json j;
    j["error"] = "No project path configured";
    res.status = 503;
    res.set_content(j.dump(), "application/json");
    return;
  }

  std::string annotation_id;
  if (!req.matches.empty() && req.matches.size() > 1) {
    annotation_id = req.matches[1].str();
  }
  if (annotation_id.empty()) {
    json j;
    j["error"] = "Missing annotation ID in URL";
    res.status = 400;
    res.set_content(j.dump(), "application/json");
    return;
  }

  json file_data = LoadAnnotationsFile(path);
  json& annotations = file_data["annotations"];

  size_t original_size = annotations.size();
  json filtered = json::array();
  for (const auto& ann : annotations) {
    if (ann.value("id", "") != annotation_id) {
      filtered.push_back(ann);
    }
  }

  if (filtered.size() == original_size) {
    json j;
    j["error"] = "Annotation not found: " + annotation_id;
    res.status = 404;
    res.set_content(j.dump(), "application/json");
    return;
  }

  file_data["annotations"] = filtered;
  if (SaveAnnotationsFile(path, file_data)) {
    json response;
    response["status"] = "ok";
    response["id"] = annotation_id;
    res.status = 200;
    res.set_content(response.dump(), "application/json");
  } else {
    json j;
    j["error"] = "Failed to write annotations file";
    res.status = 500;
    res.set_content(j.dump(), "application/json");
  }
}

}  // namespace api
}  // namespace cli
}  // namespace yaze
