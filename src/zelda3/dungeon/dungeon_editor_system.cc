#include "dungeon_editor_system.h"

#include <chrono>

#include "absl/strings/str_cat.h"
#include "nlohmann/json.hpp"
#include "zelda3/sprite/sprite.h"

namespace yaze {
namespace zelda3 {

DungeonEditorSystem::DungeonEditorSystem(Rom* rom, GameData* game_data)
    : rom_(rom), game_data_(game_data) {}

absl::Status DungeonEditorSystem::Initialize() {
  if (rom_ == nullptr) {
    return absl::InvalidArgumentError("ROM is null");
  }

  // Initialize object editor
  object_editor_ = std::make_shared<DungeonObjectEditor>(rom_);
  RETURN_IF_ERROR(object_editor_->InitializeEditor());

  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::LoadDungeon(int dungeon_id) {
  editor_state_.current_room_id = 0;
  editor_state_.is_dirty = false;
  editor_state_.last_save_time = std::chrono::steady_clock::now();
  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::SaveDungeon() {
  if (!rom_)
    return absl::InvalidArgumentError("ROM is null");

  for (int i = 0; i < NumberOfRooms; ++i) {
    auto status = SaveRoomData(i);
    if (!status.ok()) {
      return status;
    }
  }

  editor_state_.is_dirty = false;
  editor_state_.last_save_time = std::chrono::steady_clock::now();

  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::SaveRoom(int room_id) {
  return SaveRoomData(room_id);
}

absl::Status DungeonEditorSystem::ReloadRoom(int room_id) {
  return LoadRoomData(room_id);
}

absl::Status DungeonEditorSystem::SetCurrentRoom(int room_id) {
  if (room_id < 0 || room_id >= NumberOfRooms) {
    return absl::InvalidArgumentError("Invalid room ID");
  }

  editor_state_.current_room_id = room_id;
  return absl::OkStatus();
}

int DungeonEditorSystem::GetCurrentRoom() const {
  return editor_state_.current_room_id;
}

absl::StatusOr<Room> DungeonEditorSystem::GetRoom(int room_id) {
  if (room_id < 0 || room_id >= NumberOfRooms) {
    return absl::InvalidArgumentError("Invalid room ID");
  }

  return Room(room_id, rom_, game_data_);
}

std::shared_ptr<DungeonObjectEditor> DungeonEditorSystem::GetObjectEditor() {
  if (!object_editor_) {
    object_editor_ = std::make_shared<DungeonObjectEditor>(rom_);
  }
  return object_editor_;
}

absl::Status DungeonEditorSystem::Undo() {
  if (object_editor_) {
    return object_editor_->Undo();
  }

  if (!CanUndo()) {
    return absl::FailedPreconditionError("Nothing to undo");
  }

  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::Redo() {
  if (object_editor_) {
    return object_editor_->Redo();
  }

  if (!CanRedo()) {
    return absl::FailedPreconditionError("Nothing to redo");
  }

  return absl::OkStatus();
}

bool DungeonEditorSystem::CanUndo() const {
  return !undo_history_.empty();
}

bool DungeonEditorSystem::CanRedo() const {
  return !redo_history_.empty();
}

void DungeonEditorSystem::ClearHistory() {
  undo_history_.clear();
  redo_history_.clear();
}

void DungeonEditorSystem::SetRoomChangedCallback(RoomChangedCallback callback) {
  room_changed_callback_ = std::move(callback);
}

DungeonEditorSystem::EditorState DungeonEditorSystem::GetEditorState() const {
  return editor_state_;
}

Rom* DungeonEditorSystem::GetROM() const {
  return rom_;
}

bool DungeonEditorSystem::IsDirty() const {
  return editor_state_.is_dirty;
}

void DungeonEditorSystem::SetROM(Rom* rom) {
  rom_ = rom;
  if (object_editor_) {
    object_editor_->SetROM(rom);
  }
}

void DungeonEditorSystem::SetExternalRoom(Room* room) {
  if (object_editor_) {
    object_editor_->SetExternalRoom(room);
  }
}

absl::Status DungeonEditorSystem::InitializeObjectEditor() {
  if (!object_editor_) {
    object_editor_ = std::make_shared<DungeonObjectEditor>(rom_);
  }
  return object_editor_->InitializeEditor();
}

absl::Status DungeonEditorSystem::LoadRoomData(int room_id) {
  if (!rom_)
    return absl::InvalidArgumentError("ROM is null");

  // Room loading is handled by the Room class itself
  // This method exists for consistency with the API
  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::SaveRoomData(int room_id) {
  if (!rom_)
    return absl::InvalidArgumentError("ROM is null");

  // Check if this is the currently edited room in the ObjectEditor
  if (object_editor_ && object_editor_->GetRoom().id() == room_id) {
    Room* room = object_editor_->GetMutableRoom();
    if (room) {
      return room->SaveObjects();
    }
  }

  return absl::OkStatus();
}

std::unique_ptr<DungeonEditorSystem> CreateDungeonEditorSystem(
    Rom* rom, GameData* game_data) {
  return std::make_unique<DungeonEditorSystem>(rom, game_data);
}

absl::StatusOr<std::string> ExportRoomLayoutTemplate(const Room& room) {
  using json = nlohmann::json;

  json tmpl;
  tmpl["version"] = 1;
  tmpl["room_id"] = room.id();

  // Room properties
  json props;
  props["blockset"] = room.blockset;
  props["palette"] = room.palette;
  props["layout"] = room.layout;
  props["spriteset"] = room.spriteset;
  tmpl["properties"] = props;

  // Tile objects
  json objects_arr = json::array();
  for (const auto& obj : room.GetTileObjects()) {
    json obj_json;
    obj_json["id"] = obj.id_;
    obj_json["x"] = obj.x_;
    obj_json["y"] = obj.y_;
    obj_json["size"] = obj.size_;
    obj_json["layer"] = obj.GetLayerValue();
    objects_arr.push_back(obj_json);
  }
  tmpl["objects"] = objects_arr;

  // Sprites
  json sprites_arr = json::array();
  for (const auto& sprite : room.GetSprites()) {
    json spr_json;
    spr_json["id"] = sprite.id();
    spr_json["x"] = sprite.x();
    spr_json["y"] = sprite.y();
    spr_json["subtype"] = sprite.subtype();
    spr_json["layer"] = sprite.layer();
    sprites_arr.push_back(spr_json);
  }
  tmpl["sprites"] = sprites_arr;

  // Pot items
  json items_arr = json::array();
  for (const auto& item : room.GetPotItems()) {
    json item_json;
    item_json["position"] = item.position;
    item_json["item"] = item.item;
    items_arr.push_back(item_json);
  }
  tmpl["pot_items"] = items_arr;

  return tmpl.dump(2);
}

absl::Status ApplyRoomLayoutTemplate(Room& room, const std::string& json_str,
                                     bool apply_properties) {
  using json = nlohmann::json;

  json tmpl;
  try {
    tmpl = json::parse(json_str);
  } catch (const json::parse_error& err) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid JSON: ", err.what()));
  }

  if (!tmpl.contains("version") || tmpl["version"].get<int>() != 1) {
    return absl::InvalidArgumentError("Unsupported template version");
  }

  // Apply room properties if requested
  if (apply_properties && tmpl.contains("properties")) {
    const auto& props = tmpl["properties"];
    if (props.contains("blockset"))
      room.blockset = props["blockset"];
    if (props.contains("palette"))
      room.palette = props["palette"];
    if (props.contains("layout"))
      room.layout = props["layout"];
    if (props.contains("spriteset"))
      room.spriteset = props["spriteset"];
  }

  // Apply objects
  if (tmpl.contains("objects")) {
    auto& objects = room.GetTileObjects();
    objects.clear();
    for (const auto& obj_json : tmpl["objects"]) {
      int16_t obj_id = obj_json.value("id", static_cast<int16_t>(0));
      uint8_t obj_x = obj_json.value("x", static_cast<uint8_t>(0));
      uint8_t obj_y = obj_json.value("y", static_cast<uint8_t>(0));
      uint8_t obj_size = obj_json.value("size", static_cast<uint8_t>(0));
      uint8_t obj_layer = obj_json.value("layer", static_cast<uint8_t>(0));
      objects.emplace_back(obj_id, obj_x, obj_y, obj_size, obj_layer);
    }
  }

  // Apply sprites
  if (tmpl.contains("sprites")) {
    auto& sprites = room.GetSprites();
    sprites.clear();
    for (const auto& spr_json : tmpl["sprites"]) {
      uint8_t spr_id = spr_json.value("id", static_cast<uint8_t>(0));
      uint8_t spr_x = spr_json.value("x", static_cast<uint8_t>(0));
      uint8_t spr_y = spr_json.value("y", static_cast<uint8_t>(0));
      uint8_t spr_sub = spr_json.value("subtype", static_cast<uint8_t>(0));
      uint8_t spr_layer = spr_json.value("layer", static_cast<uint8_t>(0));
      sprites.emplace_back(spr_id, spr_x, spr_y, spr_sub, spr_layer);
    }
  }

  // Apply pot items
  if (tmpl.contains("pot_items")) {
    auto& items = room.GetPotItems();
    items.clear();
    for (const auto& item_json : tmpl["pot_items"]) {
      PotItem item;
      item.position = item_json.value("position", static_cast<uint16_t>(0));
      item.item = item_json.value("item", static_cast<uint8_t>(0));
      items.push_back(item);
    }
  }

  return absl::OkStatus();
}

}  // namespace zelda3
}  // namespace yaze
