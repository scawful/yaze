#include "cli/handlers/game/dungeon_edit_commands.h"

#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "cli/util/hex_util.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "util/macro.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/dungeon/track_collision_generator.h"
#include "zelda3/resource_labels.h"
#include "zelda3/sprite/sprite.h"

namespace yaze {
namespace cli {
namespace handlers {

using util::ParseHexString;

namespace {

absl::StatusOr<std::string> GetRequiredString(
    const resources::ArgumentParser& parser, const char* name) {
  auto value = parser.GetString(name);
  if (!value.has_value()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Missing required argument '--%s'", name));
  }
  return *value;
}

absl::StatusOr<int> GetRequiredInt(const resources::ArgumentParser& parser,
                                   const char* name) {
  auto parsed = parser.GetInt(name);
  if (!parsed.ok()) {
    return parsed.status();
  }
  return parsed.value();
}

absl::StatusOr<int> GetOptionalInt(const resources::ArgumentParser& parser,
                                   const char* name, int default_value) {
  if (!parser.GetString(name).has_value()) {
    return default_value;
  }

  auto parsed = parser.GetInt(name);
  if (!parsed.ok()) {
    return parsed.status();
  }
  return parsed.value();
}

absl::Status ValidateRoomId(int room_id) {
  if (room_id < 0 || room_id >= zelda3::NumberOfRooms) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Room ID out of range: 0x%X (expected 0x00-0x%02X)",
                        room_id, zelda3::NumberOfRooms - 1));
  }
  return absl::OkStatus();
}

absl::Status ValidateSpriteCoord(int value, char axis) {
  if (value < 0 || value > 31) {
    return absl::InvalidArgumentError(
        absl::StrFormat("%c must be 0-31 (5-bit tile coord)", axis));
  }
  return absl::OkStatus();
}

absl::Status SaveRomWithBackup(Rom* rom,
                               resources::OutputFormatter& formatter) {
  Rom::SaveSettings save_settings;
  save_settings.backup = true;
  auto disk_status = rom->SaveToFile(save_settings);
  if (!disk_status.ok()) {
    formatter.AddField("save_error", std::string(disk_status.message()));
    return disk_status;
  }

  formatter.AddField("save_status", "saved");
  return absl::OkStatus();
}

}  // namespace

// ---------------------------------------------------------------------------
// dungeon-place-sprite
// ---------------------------------------------------------------------------

absl::Status DungeonPlaceSpriteCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto room_id_str_or = GetRequiredString(parser, "room");
  if (!room_id_str_or.ok()) {
    return room_id_str_or.status();
  }
  auto sprite_id_str_or = GetRequiredString(parser, "id");
  if (!sprite_id_str_or.ok()) {
    return sprite_id_str_or.status();
  }
  const std::string room_id_str = room_id_str_or.value();
  const std::string sprite_id_str = sprite_id_str_or.value();

  int room_id, sprite_id;
  if (!ParseHexString(room_id_str, &room_id)) {
    return absl::InvalidArgumentError("Invalid room ID. Must be hex.");
  }
  if (!ParseHexString(sprite_id_str, &sprite_id)) {
    return absl::InvalidArgumentError("Invalid sprite ID. Must be hex.");
  }
  auto room_status = ValidateRoomId(room_id);
  if (!room_status.ok()) {
    return room_status;
  }

  auto x_or = GetRequiredInt(parser, "x");
  if (!x_or.ok()) {
    return x_or.status();
  }
  auto y_or = GetRequiredInt(parser, "y");
  if (!y_or.ok()) {
    return y_or.status();
  }
  auto subtype_or = GetOptionalInt(parser, "subtype", 0);
  if (!subtype_or.ok()) {
    return subtype_or.status();
  }
  auto layer_or = GetOptionalInt(parser, "layer", 0);
  if (!layer_or.ok()) {
    return layer_or.status();
  }

  int x = x_or.value();
  int y = y_or.value();
  int subtype = subtype_or.value();
  int layer = layer_or.value();
  bool do_write = parser.HasFlag("write");

  // Validate ranges
  RETURN_IF_ERROR(ValidateSpriteCoord(x, 'X'));
  RETURN_IF_ERROR(ValidateSpriteCoord(y, 'Y'));
  if (sprite_id < 0 || sprite_id > 0xFF) {
    return absl::InvalidArgumentError("Sprite ID must be 0x00-0xFF");
  }
  if (subtype < 0 || subtype > 0x1F) {
    return absl::InvalidArgumentError("Subtype must be 0-31 (5-bit flags)");
  }
  if (layer < 0 || layer > 1) {
    return absl::InvalidArgumentError("Layer must be 0 or 1");
  }

  // Load room and its sprites
  zelda3::Room room = zelda3::LoadRoomHeaderFromRom(rom, room_id);
  room.LoadSprites();

  int count_before = static_cast<int>(room.GetSprites().size());

  // Add the new sprite
  room.GetSprites().emplace_back(
      static_cast<uint8_t>(sprite_id), static_cast<uint8_t>(x),
      static_cast<uint8_t>(y), static_cast<uint8_t>(subtype),
      static_cast<uint8_t>(layer));

  formatter.BeginObject("Place Sprite");
  formatter.AddHexField("room_id", room_id, 2);
  formatter.AddHexField("sprite_id", sprite_id, 2);
  formatter.AddField("sprite_name", zelda3::ResolveSpriteName(sprite_id));
  formatter.AddField("x", x);
  formatter.AddField("y", y);
  formatter.AddField("subtype", subtype);
  formatter.AddField("layer", layer);
  formatter.AddField("sprites_before", count_before);
  formatter.AddField("sprites_after",
                     static_cast<int>(room.GetSprites().size()));
  formatter.AddField("mode", do_write ? "write" : "dry-run");

  if (do_write) {
    auto save_status = room.SaveSprites();
    if (!save_status.ok()) {
      formatter.AddField("write_error", std::string(save_status.message()));
      formatter.EndObject();
      return save_status;
    }
    formatter.AddField("write_status", "success");

    auto disk_status = SaveRomWithBackup(rom, formatter);
    if (!disk_status.ok()) {
      formatter.EndObject();
      return disk_status;
    }
  }

  formatter.EndObject();
  return absl::OkStatus();
}

// ---------------------------------------------------------------------------
// dungeon-remove-sprite
// ---------------------------------------------------------------------------

absl::Status DungeonRemoveSpriteCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto room_id_str_or = GetRequiredString(parser, "room");
  if (!room_id_str_or.ok()) {
    return room_id_str_or.status();
  }
  const std::string room_id_str = room_id_str_or.value();

  int room_id;
  if (!ParseHexString(room_id_str, &room_id)) {
    return absl::InvalidArgumentError("Invalid room ID. Must be hex.");
  }
  auto room_status = ValidateRoomId(room_id);
  if (!room_status.ok()) {
    return room_status;
  }

  bool do_write = parser.HasFlag("write");

  // Load room and its sprites
  zelda3::Room room = zelda3::LoadRoomHeaderFromRom(rom, room_id);
  room.LoadSprites();

  auto& sprites = room.GetSprites();
  int count_before = static_cast<int>(sprites.size());

  // Find sprite to remove: by --index or by --x/--y position.
  const bool has_index = parser.GetString("index").has_value();
  const bool has_x = parser.GetString("x").has_value();
  const bool has_y = parser.GetString("y").has_value();
  if (has_index && (has_x || has_y)) {
    return absl::InvalidArgumentError(
        "Use either --index or --x/--y, not both");
  }
  if (!has_index && has_x != has_y) {
    return absl::InvalidArgumentError(
        "Both --x and --y are required when removing by position");
  }
  if (!has_index && !has_x) {
    return absl::InvalidArgumentError(
        "Either --index or both --x and --y are required");
  }

  int remove_index = -1;
  if (has_index) {
    auto index_or = GetRequiredInt(parser, "index");
    if (!index_or.ok()) {
      return index_or.status();
    }
    remove_index = index_or.value();
  } else {
    auto x_or = GetRequiredInt(parser, "x");
    if (!x_or.ok()) {
      return x_or.status();
    }
    auto y_or = GetRequiredInt(parser, "y");
    if (!y_or.ok()) {
      return y_or.status();
    }

    const int x = x_or.value();
    const int y = y_or.value();
    RETURN_IF_ERROR(ValidateSpriteCoord(x, 'X'));
    RETURN_IF_ERROR(ValidateSpriteCoord(y, 'Y'));

    for (int i = 0; i < static_cast<int>(sprites.size()); ++i) {
      if (sprites[i].x() == x && sprites[i].y() == y) {
        remove_index = i;
        break;
      }
    }
    if (remove_index < 0) {
      return absl::NotFoundError(absl::StrFormat(
          "No sprite at (%d, %d) in room 0x%02X", x, y, room_id));
    }
  }

  if (remove_index < 0 || remove_index >= static_cast<int>(sprites.size())) {
    return absl::OutOfRangeError(
        absl::StrFormat("Sprite index %d out of range (room has %d sprites)",
                        remove_index, count_before));
  }

  // Report which sprite we're removing
  const auto& target = sprites[remove_index];
  formatter.BeginObject("Remove Sprite");
  formatter.AddHexField("room_id", room_id, 2);
  formatter.AddField("removed_index", remove_index);
  formatter.AddHexField("sprite_id", target.id(), 2);
  formatter.AddField("sprite_name", zelda3::ResolveSpriteName(target.id()));
  formatter.AddField("x", target.x());
  formatter.AddField("y", target.y());
  formatter.AddField("sprites_before", count_before);

  // Remove
  sprites.erase(sprites.begin() + remove_index);
  formatter.AddField("sprites_after", static_cast<int>(sprites.size()));
  formatter.AddField("mode", do_write ? "write" : "dry-run");

  if (do_write) {
    auto save_status = room.SaveSprites();
    if (!save_status.ok()) {
      formatter.AddField("write_error", std::string(save_status.message()));
      formatter.EndObject();
      return save_status;
    }
    formatter.AddField("write_status", "success");

    auto disk_status = SaveRomWithBackup(rom, formatter);
    if (!disk_status.ok()) {
      formatter.EndObject();
      return disk_status;
    }
  }

  formatter.EndObject();
  return absl::OkStatus();
}

// ---------------------------------------------------------------------------
// dungeon-place-object
// ---------------------------------------------------------------------------

absl::Status DungeonPlaceObjectCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto room_id_str_or = GetRequiredString(parser, "room");
  if (!room_id_str_or.ok()) {
    return room_id_str_or.status();
  }
  auto object_id_str_or = GetRequiredString(parser, "id");
  if (!object_id_str_or.ok()) {
    return object_id_str_or.status();
  }
  const std::string room_id_str = room_id_str_or.value();
  const std::string object_id_str = object_id_str_or.value();

  int room_id, object_id;
  if (!ParseHexString(room_id_str, &room_id)) {
    return absl::InvalidArgumentError("Invalid room ID. Must be hex.");
  }
  if (!ParseHexString(object_id_str, &object_id)) {
    return absl::InvalidArgumentError("Invalid object ID. Must be hex.");
  }
  auto room_status = ValidateRoomId(room_id);
  if (!room_status.ok()) {
    return room_status;
  }
  if (object_id < 0 || object_id > 0xFFFF) {
    return absl::InvalidArgumentError("Object ID must be 0x0000-0xFFFF");
  }

  auto x_or = GetRequiredInt(parser, "x");
  if (!x_or.ok()) {
    return x_or.status();
  }
  auto y_or = GetRequiredInt(parser, "y");
  if (!y_or.ok()) {
    return y_or.status();
  }
  auto size_or = GetOptionalInt(parser, "size", 0);
  if (!size_or.ok()) {
    return size_or.status();
  }
  auto layer_or = GetOptionalInt(parser, "layer", 0);
  if (!layer_or.ok()) {
    return layer_or.status();
  }

  int x = x_or.value();
  int y = y_or.value();
  int size = size_or.value();
  int layer = layer_or.value();
  bool do_write = parser.HasFlag("write");

  // Validate ranges
  if (x < 0 || x > 63) {
    return absl::InvalidArgumentError("X must be 0-63");
  }
  if (y < 0 || y > 63) {
    return absl::InvalidArgumentError("Y must be 0-63");
  }
  if (layer < 0 || layer > 2) {
    return absl::InvalidArgumentError("Layer must be 0, 1, or 2");
  }
  if (size < 0 || size > 0xFF) {
    return absl::InvalidArgumentError("Size must be 0-255");
  }

  // Load room with full objects
  zelda3::Room room = zelda3::LoadRoomFromRom(rom, room_id);

  int count_before = static_cast<int>(room.GetTileObjects().size());

  // Create the new object
  zelda3::RoomObject obj(static_cast<int16_t>(object_id),
                         static_cast<uint8_t>(x), static_cast<uint8_t>(y),
                         static_cast<uint8_t>(size),
                         static_cast<uint8_t>(layer));

  // Determine type for reporting
  int type = zelda3::RoomObject::DetermineObjectType((object_id & 0xFF),
                                                     (object_id >> 8));

  formatter.BeginObject("Place Object");
  formatter.AddHexField("room_id", room_id, 2);
  formatter.AddHexField("object_id", object_id, 4);
  formatter.AddField("object_name", zelda3::GetObjectName(object_id));
  formatter.AddField("object_type", type);
  formatter.AddField("x", x);
  formatter.AddField("y", y);
  formatter.AddField("size", size);
  formatter.AddField("layer", layer);
  formatter.AddField("objects_before", count_before);

  // Add the object
  auto add_status = room.AddObject(obj);
  if (!add_status.ok()) {
    formatter.AddField("error", std::string(add_status.message()));
    formatter.EndObject();
    return add_status;
  }

  formatter.AddField("objects_after",
                     static_cast<int>(room.GetTileObjects().size()));
  formatter.AddField("mode", do_write ? "write" : "dry-run");

  if (do_write) {
    auto save_status = room.SaveObjects();
    if (!save_status.ok()) {
      formatter.AddField("write_error", std::string(save_status.message()));
      formatter.EndObject();
      return save_status;
    }
    formatter.AddField("write_status", "success");

    auto disk_status = SaveRomWithBackup(rom, formatter);
    if (!disk_status.ok()) {
      formatter.EndObject();
      return disk_status;
    }
  }

  formatter.EndObject();
  return absl::OkStatus();
}

// ---------------------------------------------------------------------------
// dungeon-set-collision-tile
// ---------------------------------------------------------------------------

absl::Status DungeonSetCollisionTileCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto room_id_str_or = GetRequiredString(parser, "room");
  if (!room_id_str_or.ok()) {
    return room_id_str_or.status();
  }
  auto tiles_str_or = GetRequiredString(parser, "tiles");
  if (!tiles_str_or.ok()) {
    return tiles_str_or.status();
  }
  const std::string room_id_str = room_id_str_or.value();
  const std::string tiles_str = tiles_str_or.value();

  int room_id;
  if (!ParseHexString(room_id_str, &room_id)) {
    return absl::InvalidArgumentError("Invalid room ID. Must be hex.");
  }
  auto room_status = ValidateRoomId(room_id);
  if (!room_status.ok()) {
    return room_status;
  }

  bool do_write = parser.HasFlag("write");

  // Parse tile specifications: "x,y,tile;x,y,tile;..."
  struct TileSpec {
    int x, y, tile;
  };
  std::vector<TileSpec> specs;

  for (absl::string_view entry :
       absl::StrSplit(tiles_str, ';', absl::SkipEmpty())) {
    std::vector<std::string> parts =
        absl::StrSplit(entry, ',', absl::SkipEmpty());
    if (parts.size() != 3) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Invalid tile spec '%s'. Expected x,y,tile (e.g. 10,5,0xB7)", entry));
    }
    TileSpec spec;

    // x and y are decimal tile coords
    if (!absl::SimpleAtoi(parts[0], &spec.x)) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid X coord '%s'", parts[0]));
    }
    if (!absl::SimpleAtoi(parts[1], &spec.y)) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid Y coord '%s'", parts[1]));
    }
    if (!ParseHexString(parts[2], &spec.tile)) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid tile value '%s'. Must be hex.", parts[2]));
    }

    // Validate ranges
    if (spec.x < 0 || spec.x > 63 || spec.y < 0 || spec.y > 63) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Tile coords (%d,%d) out of range (0-63)", spec.x, spec.y));
    }
    if (spec.tile < 0 || spec.tile > 0xFF) {
      return absl::InvalidArgumentError("Tile value must be 0x00-0xFF");
    }

    specs.push_back(spec);
  }

  if (specs.empty()) {
    return absl::InvalidArgumentError("No tile specs provided");
  }

  // Load room with custom collision data
  zelda3::Room room = zelda3::LoadRoomFromRom(rom, room_id);

  formatter.BeginObject("Set Collision Tiles");
  formatter.AddHexField("room_id", room_id, 2);
  formatter.AddField("had_custom_collision", room.has_custom_collision());
  formatter.AddField("tile_count", static_cast<int>(specs.size()));
  formatter.AddField("mode", do_write ? "write" : "dry-run");

  // Apply each tile change
  formatter.BeginArray("changes");
  for (const auto& spec : specs) {
    uint8_t old_value = room.GetCollisionTile(spec.x, spec.y);
    room.SetCollisionTile(spec.x, spec.y, static_cast<uint8_t>(spec.tile));

    formatter.BeginObject();
    formatter.AddField("x", spec.x);
    formatter.AddField("y", spec.y);
    formatter.AddHexField("old_tile", old_value, 2);
    formatter.AddHexField("new_tile", spec.tile, 2);
    formatter.EndObject();
  }
  formatter.EndArray();

  if (do_write) {
    // Flush collision changes to ROM
    std::array<zelda3::Room, 1> rooms_arr = {std::move(room)};
    auto save_status = zelda3::SaveAllCollision(rom, absl::MakeSpan(rooms_arr));
    if (!save_status.ok()) {
      formatter.AddField("write_error", std::string(save_status.message()));
      formatter.EndObject();
      return save_status;
    }
    formatter.AddField("write_status", "success");

    auto disk_status = SaveRomWithBackup(rom, formatter);
    if (!disk_status.ok()) {
      formatter.EndObject();
      return disk_status;
    }
  }

  formatter.EndObject();
  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
