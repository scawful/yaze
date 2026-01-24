#include "cli/handlers/game/dungeon_commands.h"

#include "absl/strings/str_format.h"
#include "cli/util/hex_util.h"
#include "zelda3/dungeon/dungeon_editor_system.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_entrance.h"
#include "zelda3/sprite/sprite.h"

namespace yaze {
namespace cli {
namespace handlers {

using util::ParseHexString;

absl::Status DungeonListSpritesCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto room_id_str = parser.GetString("room").value();

  int room_id;
  if (!ParseHexString(room_id_str, &room_id)) {
    return absl::InvalidArgumentError("Invalid room ID format. Must be hex.");
  }

  formatter.BeginObject("Dungeon Room Sprites");
  formatter.AddField("room_id", room_id);

  zelda3::Room room = zelda3::LoadRoomHeaderFromRom(rom, room_id);
  room.LoadSprites();
  const auto& sprites = room.GetSprites();

  formatter.AddField("total_sprites", static_cast<int>(sprites.size()));
  formatter.AddField("status", "success");

  formatter.BeginArray("sprites");
  for (const auto& sprite : sprites) {
    formatter.BeginObject();
    formatter.AddHexField("sprite_id", sprite.id(), 2);
    formatter.AddField("name", zelda3::ResolveSpriteName(sprite.id()));
    formatter.AddField("x", sprite.x());
    formatter.AddField("y", sprite.y());
    formatter.AddField("subtype", sprite.subtype());
    formatter.AddField("layer", sprite.layer());
    if (sprite.key_drop() > 0) {
      formatter.AddField("key_drop", sprite.key_drop());
    }
    formatter.EndObject();
  }
  formatter.EndArray();
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status DungeonDescribeRoomCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto room_id_str = parser.GetString("room").value();

  int room_id;
  if (!ParseHexString(room_id_str, &room_id)) {
    return absl::InvalidArgumentError("Invalid room ID format. Must be hex.");
  }

  formatter.BeginObject("Dungeon Room Description");
  formatter.AddField("room_id", room_id);

  zelda3::Room room = zelda3::LoadRoomHeaderFromRom(rom, room_id);
  room.LoadObjects();

  formatter.AddField("status", "success");
  formatter.AddField("name", absl::StrFormat("Room %d", room.id()));
  formatter.AddField("room_id", room.id());
  formatter.AddField("room_type", "Dungeon Room");

  // Room properties from Room data
  formatter.BeginObject("properties");
  formatter.AddField("blockset", room.blockset);
  formatter.AddField("spriteset", room.spriteset);
  formatter.AddField("palette", room.palette);
  formatter.AddField("layout", room.layout);
  formatter.AddField("floor1", room.floor1());
  formatter.AddField("floor2", room.floor2());
  formatter.AddField("effect", static_cast<int>(room.effect()));
  formatter.AddField("tag1", static_cast<int>(room.tag1()));
  formatter.AddField("tag2", static_cast<int>(room.tag2()));

  // Check object counts for simple heuristics
  formatter.AddField("object_count",
                     static_cast<int>(room.GetTileObjects().size()));

  formatter.EndObject();

  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status DungeonGetEntranceCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto entrance_id_str = parser.GetString("entrance").value();
  bool is_spawn_point = parser.HasFlag("spawn");

  int entrance_id;
  if (!ParseHexString(entrance_id_str, &entrance_id)) {
    return absl::InvalidArgumentError(
        "Invalid entrance ID format. Must be hex.");
  }

  zelda3::RoomEntrance entrance(rom, static_cast<uint8_t>(entrance_id),
                                is_spawn_point);

  formatter.BeginObject("Dungeon Entrance");
  formatter.AddField("entrance_id", absl::StrFormat("0x%02X", entrance_id));
  formatter.AddField("is_spawn_point", is_spawn_point);
  formatter.AddField("room_id", absl::StrFormat("0x%04X", entrance.room_));
  formatter.AddField("exit_id", absl::StrFormat("0x%04X", entrance.exit_));

  formatter.BeginObject("position");
  formatter.AddField("x", entrance.x_position_);
  formatter.AddField("y", entrance.y_position_);
  formatter.EndObject();

  formatter.BeginObject("camera");
  formatter.AddField("x", entrance.camera_x_);
  formatter.AddField("y", entrance.camera_y_);
  formatter.AddField("trigger_x", entrance.camera_trigger_x_);
  formatter.AddField("trigger_y", entrance.camera_trigger_y_);
  formatter.EndObject();

  formatter.BeginObject("properties");
  formatter.AddField("blockset", absl::StrFormat("0x%02X", entrance.blockset_));
  formatter.AddField("floor", absl::StrFormat("0x%02X", entrance.floor_));
  formatter.AddField("dungeon_id",
                     absl::StrFormat("0x%02X", entrance.dungeon_id_));
  formatter.AddField("door", absl::StrFormat("0x%02X", entrance.door_));
  formatter.AddField("ladder_bg",
                     absl::StrFormat("0x%02X", entrance.ladder_bg_));
  formatter.AddField("scrolling",
                     absl::StrFormat("0x%02X", entrance.scrolling_));
  formatter.AddField("scroll_quadrant",
                     absl::StrFormat("0x%02X", entrance.scroll_quadrant_));
  formatter.AddField("music", absl::StrFormat("0x%02X", entrance.music_));
  formatter.EndObject();

  formatter.BeginObject("camera_boundaries");
  formatter.AddField("qn",
                     absl::StrFormat("0x%02X", entrance.camera_boundary_qn_));
  formatter.AddField("fn",
                     absl::StrFormat("0x%02X", entrance.camera_boundary_fn_));
  formatter.AddField("qs",
                     absl::StrFormat("0x%02X", entrance.camera_boundary_qs_));
  formatter.AddField("fs",
                     absl::StrFormat("0x%02X", entrance.camera_boundary_fs_));
  formatter.AddField("qw",
                     absl::StrFormat("0x%02X", entrance.camera_boundary_qw_));
  formatter.AddField("fw",
                     absl::StrFormat("0x%02X", entrance.camera_boundary_fw_));
  formatter.AddField("qe",
                     absl::StrFormat("0x%02X", entrance.camera_boundary_qe_));
  formatter.AddField("fe",
                     absl::StrFormat("0x%02X", entrance.camera_boundary_fe_));
  formatter.EndObject();

  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status DungeonExportRoomCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto room_id_str = parser.GetString("room").value();

  int room_id;
  if (!ParseHexString(room_id_str, &room_id)) {
    return absl::InvalidArgumentError("Invalid room ID format. Must be hex.");
  }

  formatter.BeginObject("Dungeon Export");
  formatter.AddField("room_id", room_id);

  // Use existing dungeon system
  zelda3::DungeonEditorSystem dungeon_editor(rom);
  auto room_or = dungeon_editor.GetRoom(room_id);
  if (!room_or.ok()) {
    formatter.AddField("status", "error");
    formatter.AddField("error", room_or.status().ToString());
    formatter.EndObject();
    return room_or.status();
  }

  auto& room = room_or.value();

  // Export room data
  formatter.AddField("status", "success");
  formatter.AddField("room_width", "Unknown");
  formatter.AddField("room_height", "Unknown");
  formatter.AddField("room_name", absl::StrFormat("Room %d", room.id()));

  // Add room data as JSON
  formatter.BeginObject("room_data");
  formatter.AddField("tiles", "Room tile data would be exported here");
  formatter.AddField("sprites", "Room sprite data would be exported here");
  formatter.AddField("doors", "Room door data would be exported here");
  formatter.EndObject();

  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status DungeonListObjectsCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto room_id_str = parser.GetString("room").value();

  int room_id;
  if (!ParseHexString(room_id_str, &room_id)) {
    return absl::InvalidArgumentError("Invalid room ID format. Must be hex.");
  }

  formatter.BeginObject("Dungeon Room Objects");
  formatter.AddField("room_id", room_id);

  // Use existing dungeon system
  zelda3::DungeonEditorSystem dungeon_editor(rom);
  auto room_or = dungeon_editor.GetRoom(room_id);
  if (!room_or.ok()) {
    formatter.AddField("status", "error");
    formatter.AddField("error", room_or.status().ToString());
    formatter.EndObject();
    return room_or.status();
  }

  auto& room = room_or.value();

  // Load objects if not already loaded (GetTileObjects might be empty otherwise)
  room.LoadObjects();

  const auto& objects = room.GetTileObjects();
  formatter.AddField("total_objects", static_cast<int>(objects.size()));
  formatter.AddField("status", "success");

  formatter.BeginArray("objects");
  for (const auto& obj : objects) {
    formatter.BeginObject("");
    formatter.AddField("id", obj.id_);
    formatter.AddField("id_hex", absl::StrFormat("0x%04X", obj.id_));
    formatter.AddField("x", obj.x_);
    formatter.AddField("y", obj.y_);
    formatter.AddField("size", obj.size_);
    formatter.AddField("layer", static_cast<int>(obj.layer_));
    // Add decoded type info if available
    int type = zelda3::RoomObject::DetermineObjectType((obj.id_ & 0xFF),
                                                       (obj.id_ >> 8));
    formatter.AddField("type", type);
    formatter.EndObject();
  }
  formatter.EndArray();
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status DungeonGetRoomTilesCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto room_id_str = parser.GetString("room").value();

  int room_id;
  if (!ParseHexString(room_id_str, &room_id)) {
    return absl::InvalidArgumentError("Invalid room ID format. Must be hex.");
  }

  formatter.BeginObject("Dungeon Room Tiles");
  formatter.AddField("room_id", room_id);

  // Use existing dungeon system
  zelda3::DungeonEditorSystem dungeon_editor(rom);
  auto room_or = dungeon_editor.GetRoom(room_id);
  if (!room_or.ok()) {
    formatter.AddField("status", "error");
    formatter.AddField("error", room_or.status().ToString());
    formatter.EndObject();
    return room_or.status();
  }

  auto& room = room_or.value();

  // TODO: Implement tile data retrieval from room
  formatter.AddField("room_width", "Unknown");
  formatter.AddField("room_height", "Unknown");
  formatter.AddField("total_tiles", "Unknown");
  formatter.AddField("status", "not_implemented");
  formatter.AddField("message",
                     "Tile data retrieval requires room tile parsing");

  formatter.BeginArray("tiles");
  formatter.EndArray();
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status DungeonSetRoomPropertyCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto room_id_str = parser.GetString("room").value();
  auto property = parser.GetString("property").value();
  auto value = parser.GetString("value").value();

  int room_id;
  if (!ParseHexString(room_id_str, &room_id)) {
    return absl::InvalidArgumentError("Invalid room ID format. Must be hex.");
  }

  formatter.BeginObject("Dungeon Room Property Set");
  formatter.AddField("room_id", room_id);
  formatter.AddField("property", property);
  formatter.AddField("value", value);

  // Use existing dungeon system
  zelda3::DungeonEditorSystem dungeon_editor(rom);
  auto room_or = dungeon_editor.GetRoom(room_id);
  if (!room_or.ok()) {
    formatter.AddField("status", "error");
    formatter.AddField("error", room_or.status().ToString());
    formatter.EndObject();
    return room_or.status();
  }

  // TODO: Implement property setting
  formatter.AddField("status", "not_implemented");
  formatter.AddField("message",
                     "Property setting requires room property system");
  formatter.EndObject();

  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
