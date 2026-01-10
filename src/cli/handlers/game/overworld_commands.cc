#include "cli/handlers/game/overworld_commands.h"

#include "absl/strings/str_format.h"
#include "cli/handlers/game/overworld_inspect.h"
#include "cli/util/hex_util.h"
#include "zelda3/overworld/overworld.h"

namespace yaze {
namespace cli {
namespace handlers {

using util::ParseHexString;

absl::Status OverworldFindTileCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto tile_id_str = parser.GetString("tile").value();

  int tile_id;
  if (!ParseHexString(tile_id_str, &tile_id)) {
    return absl::InvalidArgumentError("Invalid tile ID format. Must be hex.");
  }

  // Load the Overworld from ROM
  zelda3::Overworld overworld(rom);
  auto ow_status = overworld.Load(rom);
  if (!ow_status.ok()) {
    return ow_status;
  }

  // Call the helper function to find tile matches
  auto matches_or = overworld::FindTileMatches(overworld, tile_id);
  if (!matches_or.ok()) {
    return matches_or.status();
  }
  const auto& matches = matches_or.value();

  // Format the output
  formatter.BeginObject("Overworld Tile Search");
  formatter.AddField("tile_id", absl::StrFormat("0x%03X", tile_id));
  formatter.AddField("matches_found", static_cast<int>(matches.size()));

  formatter.BeginArray("matches");
  for (const auto& match : matches) {
    formatter.BeginObject();
    formatter.AddField("map_id", absl::StrFormat("0x%02X", match.map_id));
    formatter.AddField("world", overworld::WorldName(match.world));
    formatter.AddField("local_x", match.local_x);
    formatter.AddField("local_y", match.local_y);
    formatter.AddField("global_x", match.global_x);
    formatter.AddField("global_y", match.global_y);
    formatter.EndObject();
  }
  formatter.EndArray();
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status OverworldDescribeMapCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto screen_id_str = parser.GetString("screen").value();

  int screen_id;
  if (!ParseHexString(screen_id_str, &screen_id)) {
    return absl::InvalidArgumentError("Invalid screen ID format. Must be hex.");
  }

  // Load the Overworld from ROM
  zelda3::Overworld overworld(rom);
  auto ow_status = overworld.Load(rom);
  if (!ow_status.ok()) {
    return ow_status;
  }

  // Call the helper function to build the map summary
  auto summary_or = overworld::BuildMapSummary(overworld, screen_id);
  if (!summary_or.ok()) {
    return summary_or.status();
  }
  const auto& summary = summary_or.value();

  // Format the output using OutputFormatter
  formatter.BeginObject("Overworld Map Description");
  formatter.AddField("screen_id", absl::StrFormat("0x%02X", summary.map_id));
  formatter.AddField("world", overworld::WorldName(summary.world));

  formatter.BeginObject("grid");
  formatter.AddField("x", summary.map_x);
  formatter.AddField("y", summary.map_y);
  formatter.AddField("local_index", summary.local_index);
  formatter.EndObject();

  formatter.BeginObject("size");
  formatter.AddField("label", summary.area_size);
  formatter.AddField("is_large", summary.is_large_map);
  formatter.AddField("parent", absl::StrFormat("0x%02X", summary.parent_map));
  formatter.AddField("quadrant", summary.large_quadrant);
  formatter.EndObject();

  formatter.AddField("message_id",
                     absl::StrFormat("0x%04X", summary.message_id));
  formatter.AddField("area_graphics",
                     absl::StrFormat("0x%02X", summary.area_graphics));
  formatter.AddField("area_palette",
                     absl::StrFormat("0x%02X", summary.area_palette));
  formatter.AddField("main_palette",
                     absl::StrFormat("0x%02X", summary.main_palette));
  formatter.AddField("animated_gfx",
                     absl::StrFormat("0x%02X", summary.animated_gfx));
  formatter.AddField("subscreen_overlay",
                     absl::StrFormat("0x%04X", summary.subscreen_overlay));
  formatter.AddField("area_specific_bg_color",
                     absl::StrFormat("0x%04X", summary.area_specific_bg_color));

  // Format array fields
  formatter.BeginArray("sprite_graphics");
  for (uint8_t gfx : summary.sprite_graphics) {
    formatter.AddArrayItem(absl::StrFormat("0x%02X", gfx));
  }
  formatter.EndArray();

  formatter.BeginArray("sprite_palettes");
  for (uint8_t pal : summary.sprite_palettes) {
    formatter.AddArrayItem(absl::StrFormat("0x%02X", pal));
  }
  formatter.EndArray();

  formatter.BeginArray("area_music");
  for (uint8_t music : summary.area_music) {
    formatter.AddArrayItem(absl::StrFormat("0x%02X", music));
  }
  formatter.EndArray();

  formatter.BeginArray("static_graphics");
  for (uint8_t sgfx : summary.static_graphics) {
    formatter.AddArrayItem(absl::StrFormat("0x%02X", sgfx));
  }
  formatter.EndArray();

  formatter.BeginObject("overlay");
  formatter.AddField("enabled", summary.has_overlay);
  formatter.AddField("id", absl::StrFormat("0x%04X", summary.overlay_id));
  formatter.EndObject();

  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status OverworldListWarpsCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto screen_id_str = parser.GetString("screen").value_or("all");

  // Load the Overworld from ROM
  zelda3::Overworld overworld(rom);
  auto ow_status = overworld.Load(rom);
  if (!ow_status.ok()) {
    return ow_status;
  }

  // Build the query
  overworld::WarpQuery query;
  if (screen_id_str != "all") {
    int map_id;
    if (!ParseHexString(screen_id_str, &map_id)) {
      return absl::InvalidArgumentError(
          "Invalid screen ID format. Must be hex.");
    }
    query.map_id = map_id;
  }

  // Call the helper function to collect warp entries
  auto warps_or = overworld::CollectWarpEntries(overworld, query);
  if (!warps_or.ok()) {
    return warps_or.status();
  }
  const auto& warps = warps_or.value();

  // Format the output
  formatter.BeginObject("Overworld Warps");
  formatter.AddField("screen_filter", screen_id_str);
  formatter.AddField("total_warps", static_cast<int>(warps.size()));

  formatter.BeginArray("warps");
  for (const auto& warp : warps) {
    formatter.BeginObject();
    formatter.AddField("type", overworld::WarpTypeName(warp.type));
    formatter.AddField("map_id", absl::StrFormat("0x%02X", warp.map_id));
    formatter.AddField("world", overworld::WorldName(warp.world));
    formatter.AddField("position",
                       absl::StrFormat("(%d,%d)", warp.pixel_x, warp.pixel_y));
    formatter.AddField("map_pos", absl::StrFormat("0x%04X", warp.map_pos));

    if (warp.entrance_id.has_value()) {
      formatter.AddField("entrance_id",
                         absl::StrFormat("0x%02X", warp.entrance_id.value()));
    }
    if (warp.entrance_name.has_value()) {
      formatter.AddField("entrance_name", warp.entrance_name.value());
    }
    if (warp.room_id.has_value()) {
      formatter.AddField("room_id",
                         absl::StrFormat("0x%04X", warp.room_id.value()));
    }

    formatter.AddField("deleted", warp.deleted);
    formatter.AddField("is_hole", warp.is_hole);
    formatter.EndObject();
  }
  formatter.EndArray();
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status OverworldListSpritesCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto screen_id_str = parser.GetString("screen").value_or("all");

  // Load the Overworld from ROM
  zelda3::Overworld overworld(rom);
  auto ow_status = overworld.Load(rom);
  if (!ow_status.ok()) {
    return ow_status;
  }

  // Build the query
  overworld::SpriteQuery query;
  if (screen_id_str != "all") {
    int map_id;
    if (!ParseHexString(screen_id_str, &map_id)) {
      return absl::InvalidArgumentError(
          "Invalid screen ID format. Must be hex.");
    }
    query.map_id = map_id;
  }

  // Call the helper function to collect sprites
  auto sprites_or = overworld::CollectOverworldSprites(overworld, query);
  if (!sprites_or.ok()) {
    return sprites_or.status();
  }
  const auto& sprites = sprites_or.value();

  // Format the output
  formatter.BeginObject("Overworld Sprites");
  formatter.AddField("screen_filter", screen_id_str);
  formatter.AddField("total_sprites", static_cast<int>(sprites.size()));

  formatter.BeginArray("sprites");
  for (const auto& sprite : sprites) {
    formatter.BeginObject();
    formatter.AddField("sprite_id",
                       absl::StrFormat("0x%02X", sprite.sprite_id));
    formatter.AddField("map_id", absl::StrFormat("0x%02X", sprite.map_id));
    formatter.AddField("world", overworld::WorldName(sprite.world));
    formatter.AddField("position",
                       absl::StrFormat("(%d,%d)", sprite.x, sprite.y));

    if (sprite.sprite_name.has_value()) {
      formatter.AddField("name", sprite.sprite_name.value());
    }

    formatter.EndObject();
  }
  formatter.EndArray();
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status OverworldGetEntranceCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto entrance_id_str = parser.GetString("entrance").value();

  int entrance_id;
  if (!ParseHexString(entrance_id_str, &entrance_id)) {
    return absl::InvalidArgumentError(
        "Invalid entrance ID format. Must be hex.");
  }

  // Load the Overworld from ROM
  zelda3::Overworld overworld(rom);
  auto ow_status = overworld.Load(rom);
  if (!ow_status.ok()) {
    return ow_status;
  }

  // Call the helper function to get entrance details
  auto details_or = overworld::GetEntranceDetails(overworld, entrance_id);
  if (!details_or.ok()) {
    return details_or.status();
  }
  const auto& details = details_or.value();

  // Format the output
  formatter.BeginObject("Overworld Entrance");
  formatter.AddField("entrance_id",
                     absl::StrFormat("0x%02X", details.entrance_id));
  formatter.AddField("map_id", absl::StrFormat("0x%02X", details.map_id));
  formatter.AddField("world", overworld::WorldName(details.world));
  formatter.AddField("position",
                     absl::StrFormat("(%d,%d)", details.x, details.y));
  formatter.AddField("area_position", absl::StrFormat("(%d,%d)", details.area_x,
                                                      details.area_y));
  formatter.AddField("map_pos", absl::StrFormat("0x%04X", details.map_pos));
  formatter.AddField("is_hole", details.is_hole);

  if (details.entrance_name.has_value()) {
    formatter.AddField("name", details.entrance_name.value());
  }

  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status OverworldTileStatsCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto screen_id_str = parser.GetString("screen").value_or("all");

  // Load the Overworld from ROM
  zelda3::Overworld overworld(rom);
  auto ow_status = overworld.Load(rom);
  if (!ow_status.ok()) {
    return ow_status;
  }

  // TODO: Implement comprehensive tile statistics
  // The AnalyzeTileUsage helper requires a specific tile_id,
  // so we need a different approach to gather overall tile statistics.
  // This could involve:
  // 1. Iterating through all tiles in the overworld maps
  // 2. Building a frequency map of tile usage
  // 3. Computing statistics like unique tiles, most common tiles, etc.

  formatter.BeginObject("Overworld Tile Statistics");
  formatter.AddField("screen_filter", screen_id_str);
  formatter.AddField("status", "partial_implementation");
  formatter.AddField("message",
                     "Comprehensive tile statistics not yet implemented. "
                     "Use overworld-find-tile for specific tile analysis.");
  formatter.AddField("total_tiles", 0);
  formatter.AddField("unique_tiles", 0);

  formatter.BeginArray("tile_counts");
  formatter.EndArray();
  formatter.EndObject();

  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
