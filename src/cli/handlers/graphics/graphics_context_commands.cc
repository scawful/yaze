#include "cli/handlers/graphics/graphics_context_commands.h"

#include "absl/strings/str_format.h"
#include "core/project.h"
#include "util/hex.h"
#include "util/macro.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/game_data.h"
#include "zelda3/graphics/resolved_graphics_context.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze::cli {
namespace {

absl::StatusOr<int> ParseId(const resources::ArgumentParser& parser,
                            const char* flag, int limit) {
  const auto text = parser.GetString(flag);
  if (!text.has_value()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("--%s is required", flag));
  }
  uint32_t value = 0;
  if (!util::ParseHexString(*text, &value) ||
      value >= static_cast<uint32_t>(limit)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("--%s must be a hex id below 0x%X", flag, limit));
  }
  return static_cast<int>(value);
}

zelda3::GraphicsContextIdentity IdentityFor(
    const project::YazeProject* project) {
  zelda3::GraphicsContextIdentity identity;
  if (project != nullptr) {
    identity.project_name = project->name;
  }
  return identity;
}

}  // namespace

absl::Status GraphicsContextCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  const bool has_map = parser.GetString("map").has_value();
  const bool has_room = parser.GetString("room").has_value();
  if (has_map == has_room) {
    return absl::InvalidArgumentError("Pass exactly one of --map or --room");
  }
  return absl::OkStatus();
}

absl::Status GraphicsContextCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  if (rom == nullptr || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }
  zelda3::GameData data;
  zelda3::LoadOptions options;
  options.expand_rom = false;
  RETURN_IF_ERROR(zelda3::LoadGameData(*rom, data, options));
  const auto identity = IdentityFor(project_);

  zelda3::ResolvedGraphicsContext context;
  if (parser.GetString("map").has_value()) {
    ASSIGN_OR_RETURN(const int map_id, ParseId(parser, "map", 0xA0));
    zelda3::OverworldMap map(map_id, rom, &data);
    map.LoadAreaGraphics();
    RETURN_IF_ERROR(map.LoadPalette());
    context = zelda3::DescribeOverworldGraphicsContext(map, identity);
  } else {
    ASSIGN_OR_RETURN(const int room_id, ParseId(parser, "room", 0x128));
    zelda3::Room room = zelda3::LoadRoomFromRom(rom, room_id);
    room.SetGameData(&data);
    room.LoadRoomGraphics();
    context = zelda3::DescribeDungeonGraphicsContext(room, identity);
  }
  if (!context.graphics_loaded || !context.loader_agrees) {
    return absl::FailedPreconditionError(
        "Graphics context did not match the loader");
  }
  formatter.AddRawJsonField("graphics_context", context.ToJson());
  return absl::OkStatus();
}

}  // namespace yaze::cli
