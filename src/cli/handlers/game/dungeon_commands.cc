#include "cli/handlers/game/dungeon_commands.h"

#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "zelda3/dungeon/dungeon_editor_system.h"

namespace yaze {
namespace cli {
namespace handlers {

absl::Status DungeonListSpritesCommandHandler::Execute(Rom* rom, const resources::ArgumentParser& parser,
                      resources::OutputFormatter& formatter) {
  auto room_id_str = parser.GetString("room").value();
  
  int room_id;
  if (!absl::SimpleHexAtoi(room_id_str, &room_id)) {
    return absl::InvalidArgumentError(
        "Invalid room ID format. Must be hex.");
  }
  
  formatter.BeginObject("Dungeon Room Sprites");
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
  
  auto room = room_or.value();
  
  // TODO: Implement sprite listing from room data
  formatter.AddField("total_sprites", 0);
  formatter.AddField("status", "not_implemented");
  formatter.AddField("message", "Sprite listing requires room sprite parsing");
  
  formatter.BeginArray("sprites");
  formatter.EndArray();
  formatter.EndObject();
  
  return absl::OkStatus();
}

absl::Status DungeonDescribeRoomCommandHandler::Execute(Rom* rom, const resources::ArgumentParser& parser,
                      resources::OutputFormatter& formatter) {
  auto room_id_str = parser.GetString("room").value();
  
  int room_id;
  if (!absl::SimpleHexAtoi(room_id_str, &room_id)) {
    return absl::InvalidArgumentError(
        "Invalid room ID format. Must be hex.");
  }
  
  formatter.BeginObject("Dungeon Room Description");
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
  
  auto room = room_or.value();
  
  formatter.AddField("status", "success");
  formatter.AddField("name", absl::StrFormat("Room %d", room.id()));
  formatter.AddField("room_id", room.id());
  formatter.AddField("room_type", "Dungeon Room");
  
  // Add room properties
  formatter.BeginObject("properties");
  formatter.AddField("has_doors", "Unknown");
  formatter.AddField("has_sprites", "Unknown");
  formatter.AddField("has_secrets", "Unknown");
  formatter.EndObject();
  
  formatter.EndObject();
  
  return absl::OkStatus();
}

absl::Status DungeonExportRoomCommandHandler::Execute(Rom* rom, const resources::ArgumentParser& parser,
                      resources::OutputFormatter& formatter) {
  auto room_id_str = parser.GetString("room").value();
  
  int room_id;
  if (!absl::SimpleHexAtoi(room_id_str, &room_id)) {
    return absl::InvalidArgumentError(
        "Invalid room ID format. Must be hex.");
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
  
  auto room = room_or.value();
  
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

absl::Status DungeonListObjectsCommandHandler::Execute(Rom* rom, const resources::ArgumentParser& parser,
                      resources::OutputFormatter& formatter) {
  auto room_id_str = parser.GetString("room").value();
  
  int room_id;
  if (!absl::SimpleHexAtoi(room_id_str, &room_id)) {
    return absl::InvalidArgumentError(
        "Invalid room ID format. Must be hex.");
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
  
  auto room = room_or.value();
  
  // TODO: Implement object listing from room data
  formatter.AddField("total_objects", 0);
  formatter.AddField("status", "not_implemented");
  formatter.AddField("message", "Object listing requires room object parsing");
  
  formatter.BeginArray("objects");
  formatter.EndArray();
  formatter.EndObject();
  
  return absl::OkStatus();
}

absl::Status DungeonGetRoomTilesCommandHandler::Execute(Rom* rom, const resources::ArgumentParser& parser,
                      resources::OutputFormatter& formatter) {
  auto room_id_str = parser.GetString("room").value();
  
  int room_id;
  if (!absl::SimpleHexAtoi(room_id_str, &room_id)) {
    return absl::InvalidArgumentError(
        "Invalid room ID format. Must be hex.");
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
  
  auto room = room_or.value();
  
  // TODO: Implement tile data retrieval from room
  formatter.AddField("room_width", "Unknown");
  formatter.AddField("room_height", "Unknown");
  formatter.AddField("total_tiles", "Unknown");
  formatter.AddField("status", "not_implemented");
  formatter.AddField("message", "Tile data retrieval requires room tile parsing");
  
  formatter.BeginArray("tiles");
  formatter.EndArray();
  formatter.EndObject();
  
  return absl::OkStatus();
}

absl::Status DungeonSetRoomPropertyCommandHandler::Execute(Rom* rom, const resources::ArgumentParser& parser,
                      resources::OutputFormatter& formatter) {
  auto room_id_str = parser.GetString("room").value();
  auto property = parser.GetString("property").value();
  auto value = parser.GetString("value").value();
  
  int room_id;
  if (!absl::SimpleHexAtoi(room_id_str, &room_id)) {
    return absl::InvalidArgumentError(
        "Invalid room ID format. Must be hex.");
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
  formatter.AddField("message", "Property setting requires room property system");
  formatter.EndObject();
  
  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
