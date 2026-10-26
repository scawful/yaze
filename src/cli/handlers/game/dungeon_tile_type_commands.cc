#include "cli/handlers/game/dungeon_tile_type_commands.h"

#include <map>
#include <string>

#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "cli/util/hex_util.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "util/macro.h"
#include "zelda3/dungeon/game_tilemap_comparison.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_collision.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace cli {
namespace handlers {

namespace {

constexpr uint8_t kPitType = 0x20;

// Names for the attribute values the editor and agents ask about most. The
// groups follow the object and door passes in room_collision.h; anything else
// prints as its hex value.
std::string TileTypeName(uint8_t value) {
  switch (value) {
    case 0x00:
      return "floor";
    case 0x01:
      return "wall";
    case kPitType:
      return "pit";
    case 0x66:
    case 0x67:
      return "peg";
    default:
      break;
  }
  if ((value >= 0x30 && value <= 0x39) || value == 0x26 || value == 0x5E ||
      value == 0x5F)
    return "stairs";
  if (value >= 0x1D && value <= 0x1F)
    return "north_layer_stairs";
  if (value >= 0x3D && value <= 0x3F)
    return "south_layer_stairs";
  if (value >= 0x58 && value <= 0x5D)
    return "chest_or_lock";
  if (value >= 0x70 && value <= 0x7F)
    return "pot_or_block";
  if (value >= 0xC0 && value <= 0xCF)
    return "torch";
  if (value >= 0x80 && value <= 0x8F)
    return "door";
  if (value >= 0x90 && value <= 0xAF)
    return "toggle_door";
  if (value >= 0xF0)
    return "locked_door";
  return absl::StrFormat("type_%02X", value);
}

struct LayerView {
  const char* name;  // upper / lower
  const char* map;   // WRAM map the game builds
  const char* bg;    // tilemap it reads
  size_t base;       // offset into RoomCollisionMaps::attributes
  const std::vector<uint16_t>* tilemap;
};

}  // namespace

absl::Status DungeonTileTypesCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  RETURN_IF_ERROR(parser.RequireArgs({"room"}));
  if (const auto layer = parser.GetString("layer"); layer.has_value()) {
    const std::string value = absl::AsciiStrToLower(*layer);
    if (value != "upper" && value != "lower" && value != "both") {
      return absl::InvalidArgumentError("--layer must be upper, lower or both");
    }
  }
  if (const auto type = parser.GetString("type"); type.has_value()) {
    int parsed = 0;
    if (!util::ParseHexString(*type, &parsed) || parsed < 0 || parsed > 0xFF) {
      return absl::InvalidArgumentError(
          "--type must be a hex byte (e.g. 0x20)");
    }
  }
  return absl::OkStatus();
}

absl::Status DungeonTileTypesCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  int room_id = 0;
  if (!util::ParseHexString(parser.GetString("room").value(), &room_id) ||
      room_id < 0 || room_id >= zelda3::kNumberOfRooms) {
    return absl::InvalidArgumentError(
        "Invalid room ID. Must be hex in range 0x000-0x127.");
  }
  if (rom == nullptr || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  int match_type = kPitType;
  if (const auto type = parser.GetString("type"); type.has_value()) {
    util::ParseHexString(*type, &match_type);
  }
  const std::string layer_arg =
      absl::AsciiStrToLower(parser.GetString("layer").value_or("both"));
  const bool rows = !parser.HasFlag("no-rows");

  // The maps come from the room's drawn tilemaps, so the room is rendered the
  // way dungeon-render and the game-state parity test render it.
  zelda3::GameData game_data;
  RETURN_IF_ERROR(zelda3::LoadGameData(*rom, game_data));
  zelda3::Room room = zelda3::LoadRoomFromRom(rom, room_id);
  room.SetGameData(&game_data);
  room.LoadRoomGraphics();
  room.RenderRoomGraphics();

  const zelda3::RoomTilemaps tilemaps = zelda3::ComposeYazeRoomTilemaps(room);
  const zelda3::RoomCollisionMaps maps = zelda3::ComputeRoomCollisionMaps(
      *rom, zelda3::MakeRoomCollisionInput(room, tilemaps));

  int custom_cells = 0;
  if (room.has_custom_collision()) {
    const auto& custom_collision = room.custom_collision();
    for (size_t offset = 0; offset < custom_collision.tiles.size(); ++offset) {
      custom_cells += custom_collision.HasOverride(offset);
    }
  }

  formatter.AddHexField("room_id", room_id, 3);
  formatter.AddField("blockset", static_cast<int>(room.blockset()));
  formatter.AddField("layout_id", static_cast<int>(room.layout_id()));
  formatter.AddField("model", "Underworld_LoadAttributeTable replay");
  // Slot 5-6 types (chars 0x140-0x1BF) follow the blockset's group, read from
  // the table the $0E:942A routine (or its JSL hook) uses.
  const zelda3::CustomTileTypeTable custom_types =
      zelda3::ResolveCustomTileTypeTable(*rom);
  formatter.AddField("custom_tile_types",
                     zelda3::CustomTileTypeSourceName(custom_types.source));
  formatter.AddHexField("custom_tile_type_offsets",
                        PcToSnes(custom_types.offsets_pc), 6);
  formatter.AddHexField("custom_tile_type_data", PcToSnes(custom_types.data_pc),
                        6);
  formatter.AddField("load_state",
                     "new file: no room flags, pegs not swapped, shutters "
                     "closed");
  formatter.AddField("has_custom_collision", room.has_custom_collision());
  formatter.AddField("custom_collision_cells", custom_cells);
  formatter.AddHexField("match_type", static_cast<uint64_t>(match_type), 2);
  formatter.AddField("match_type_name",
                     TileTypeName(static_cast<uint8_t>(match_type)));

  const LayerView kLayers[] = {
      {"upper", "COLMAPA $7F2000", "BG1", 0, &tilemaps.bg1},
      {"lower", "COLMAPB $7F3000", "BG2", zelda3::kCollisionMapTiles,
       &tilemaps.bg2},
  };

  formatter.BeginArray("layers");
  for (const auto& layer : kLayers) {
    if (layer_arg != "both" && layer_arg != layer.name) {
      continue;
    }
    std::map<uint8_t, int> counts;
    for (size_t i = 0; i < zelda3::kCollisionMapTiles; ++i) {
      ++counts[maps.attributes[layer.base + i]];
    }

    formatter.BeginObject();
    formatter.AddField("layer", layer.name);
    formatter.AddField("map", layer.map);
    formatter.AddField("tilemap", layer.bg);
    auto cells_of = [&counts](int value) {
      const auto it = counts.find(static_cast<uint8_t>(value));
      return it == counts.end() ? 0 : it->second;
    };
    formatter.AddField("match_cells", cells_of(match_type));
    formatter.AddField("pit_cells", cells_of(kPitType));

    formatter.BeginArray("type_counts");
    for (const auto& [value, count] : counts) {
      formatter.BeginObject();
      formatter.AddHexField("type", value, 2);
      formatter.AddField("name", TileTypeName(value));
      formatter.AddField("cells", count);
      formatter.EndObject();
    }
    formatter.EndArray();

    // Each matching cell with the tile word it came from: the word's tile
    // number indexes TILEATTR ($7EFE00); bits 14/15 are its flips.
    formatter.BeginArray("matches");
    for (size_t i = 0; i < zelda3::kCollisionMapTiles; ++i) {
      const size_t index = layer.base + i;
      if (maps.attributes[index] != match_type) {
        continue;
      }
      const uint16_t word = i < layer.tilemap->size() ? (*layer.tilemap)[i] : 0;
      formatter.BeginObject();
      formatter.AddField("x", static_cast<int>(i % 64));
      formatter.AddField("y", static_cast<int>(i / 64));
      formatter.AddHexField("tile_word", word, 4);
      formatter.AddHexField("tile", word & 0x03FF, 3);
      formatter.AddField("source",
                         zelda3::CollisionSourceName(maps.sources[index]));
      formatter.EndObject();
    }
    formatter.EndArray();

    if (rows) {
      formatter.BeginArray("rows");
      for (int y = 0; y < 64; ++y) {
        std::string row;
        row.reserve(64 * 3);
        for (int x = 0; x < 64; ++x) {
          if (x > 0) {
            row.push_back(' ');
          }
          absl::StrAppend(
              &row, absl::StrFormat("%02X",
                                    maps.attributes[layer.base + y * 64 + x]));
        }
        formatter.AddArrayItem(row);
      }
      formatter.EndArray();
    }
    formatter.EndObject();
  }
  formatter.EndArray();
  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
