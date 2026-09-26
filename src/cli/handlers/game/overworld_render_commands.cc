#include "cli/handlers/game/overworld_render_commands.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "app/service/render_service.h"
#include "cli/util/hex_util.h"
#include "rom/rom.h"
#include "util/macro.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace cli {
namespace handlers {

namespace {

namespace ow = app::service::OverworldOverlay;

absl::StatusOr<uint32_t> ParseOverworldOverlays(const std::string& s) {
  uint32_t flags = ow::kNone;
  std::istringstream ss(s);
  std::string tok;
  while (std::getline(ss, tok, ',')) {
    tok.erase(0, tok.find_first_not_of(" \t"));
    if (!tok.empty())
      tok.erase(tok.find_last_not_of(" \t") + 1);
    if (tok.empty())
      continue;
    if (tok == "sprites")
      flags |= ow::kSprites;
    else if (tok == "entrances")
      flags |= ow::kEntrances;
    else if (tok == "exits")
      flags |= ow::kExits;
    else if (tok == "holes")
      flags |= ow::kHoles;
    else if (tok == "items")
      flags |= ow::kItems;
    else if (tok == "grid")
      flags |= ow::kGrid;
    else if (tok == "all")
      flags = ow::kAll & ~ow::kGrid;  // grid stays opt-in
    else
      return absl::InvalidArgumentError(
          absl::StrFormat("Unknown overlay '%s' (expected sprites, entrances, "
                          "exits, holes, items, grid, all)",
                          tok));
  }
  return flags;
}

absl::StatusOr<bool> ParseAreaOverlay(const std::string& value) {
  if (value == "on" || value == "true" || value == "1")
    return true;
  if (value == "off" || value == "false" || value == "0")
    return false;
  return absl::InvalidArgumentError("--area-overlay must be on or off");
}

std::optional<std::string> OutputArg(const resources::ArgumentParser& parser) {
  if (auto out = parser.GetString("out"); out.has_value())
    return out;
  return parser.GetString("output");
}

}  // namespace

absl::Status OverworldRenderCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  RETURN_IF_ERROR(parser.RequireArgs({"screen"}));
  const auto out = OutputArg(parser);
  if (!out.has_value() || out->empty()) {
    return absl::InvalidArgumentError("--out <path.png> is required");
  }
  if (const auto scale = parser.GetString("scale"); scale.has_value()) {
    RETURN_IF_ERROR(app::service::ParseRenderScale(*scale).status());
  }
  if (const auto ov = parser.GetString("overlays"); ov.has_value()) {
    RETURN_IF_ERROR(ParseOverworldOverlays(*ov).status());
  }
  if (const auto ao = parser.GetString("area-overlay"); ao.has_value()) {
    RETURN_IF_ERROR(ParseAreaOverlay(*ao).status());
  }
  if (const auto phase = parser.GetString("phase"); phase.has_value()) {
    int value = -1;
    if (!absl::SimpleAtoi(*phase, &value) || value < 0 || value > 2) {
      return absl::InvalidArgumentError("--phase must be 0, 1, or 2");
    }
  }
  return absl::OkStatus();
}

absl::Status OverworldRenderCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  resources::CommandInvocationContext invocation_context;
  if (rom != nullptr && !rom->filename().empty()) {
    invocation_context.source_rom_path = std::filesystem::path(rom->filename());
    invocation_context.active_rom_path = std::filesystem::path(rom->filename());
  }
  return ExecuteWithContext(rom, parser, formatter, invocation_context);
}

absl::Status OverworldRenderCommandHandler::ExecuteWithContext(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter,
    const resources::CommandInvocationContext& invocation_context) {
  RETURN_IF_ERROR(ValidateArgs(parser));

  int screen = 0;
  if (!util::ParseHexString(*parser.GetString("screen"), &screen)) {
    return absl::InvalidArgumentError("--screen must be hex (e.g. 0x40)");
  }

  const std::string output_path = *OutputArg(parser);
  ASSIGN_OR_RETURN(const auto resolved_output,
                   resources::ResolveStableArtifactPath(output_path));
  RETURN_IF_ERROR(resources::RejectArtifactRomAliases("--out", resolved_output,
                                                      invocation_context));

  app::service::OverworldRenderRequest req;
  req.screen_id = screen;
  if (auto ov = parser.GetString("overlays"); ov.has_value()) {
    ASSIGN_OR_RETURN(req.overlay_flags, ParseOverworldOverlays(*ov));
  }
  if (auto phase = parser.GetString("phase"); phase.has_value()) {
    absl::SimpleAtoi(*phase, &req.phase);
  }
  if (auto sc = parser.GetString("scale"); sc.has_value()) {
    ASSIGN_OR_RETURN(req.scale, app::service::ParseRenderScale(*sc));
  }
  if (auto ao = parser.GetString("area-overlay"); ao.has_value()) {
    ASSIGN_OR_RETURN(req.area_overlay, ParseAreaOverlay(*ao));
  }

  if (!rom || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  zelda3::GameData game_data;
  auto gd_status = zelda3::LoadGameData(*rom, game_data);
  if (!gd_status.ok()) {
    return absl::InternalError(
        absl::StrFormat("Failed to load game data: %s", gd_status.message()));
  }

  app::service::RenderService render_service(rom, &game_data);
  ASSIGN_OR_RETURN(auto result, render_service.RenderOverworldArea(req));

  RETURN_IF_ERROR(resources::RejectArtifactRomAliases("--out", resolved_output,
                                                      invocation_context));
  std::ofstream out(resolved_output, std::ios::binary);
  if (!out) {
    return absl::InternalError(
        absl::StrFormat("Cannot open output file: %s", output_path));
  }
  out.write(reinterpret_cast<const char*>(result.png_data.data()),
            static_cast<std::streamsize>(result.png_data.size()));
  out.close();
  if (!out) {
    return absl::InternalError(
        absl::StrFormat("Write failed for: %s", output_path));
  }

  formatter.AddField("requested_screen",
                     absl::StrFormat("0x%02X", result.requested_screen));
  formatter.AddField("parent_screen",
                     absl::StrFormat("0x%02X", result.parent_screen));
  formatter.AddField("area_size", result.area_size);
  formatter.AddField("subscreen_overlay",
                     result.subscreen_overlay < 0
                         ? std::string("none")
                         : absl::StrFormat("0x%02X", result.subscreen_overlay));
  formatter.BeginArray("screens");
  for (int s : result.screens)
    formatter.AddArrayItem(absl::StrFormat("0x%02X", s));
  formatter.EndArray();
  formatter.AddField("sprite_phase", req.phase < 0
                                         ? std::string("all")
                                         : absl::StrFormat("%d", req.phase));
  formatter.AddField("output", output_path);
  formatter.AddField("width", result.width);
  formatter.AddField("height", result.height);
  formatter.AddField("scale", absl::StrFormat("%.2f", req.scale));
  formatter.AddField("size_bytes", static_cast<int>(result.png_data.size()));
  formatter.AddField(
      "coordinates",
      "marker x/y are area-local pixels at scale 1; tile = x/16, y/16");
  formatter.BeginArray("markers");
  for (const auto& m : result.markers) {
    formatter.BeginObject();
    formatter.AddField("kind", m.kind);
    formatter.AddField("id", absl::StrFormat("0x%02X", m.id));
    if (m.phase >= 0) {
      formatter.AddField("phase", m.phase);
      formatter.AddField("list_index", m.list_index);
    }
    formatter.AddField("x", m.x);
    formatter.AddField("y", m.y);
    formatter.AddField("tile", absl::StrFormat("(%d,%d)", m.x / 16, m.y / 16));
    formatter.EndObject();
  }
  formatter.EndArray();
  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
