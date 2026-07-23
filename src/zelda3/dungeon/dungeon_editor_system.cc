#include "dungeon_editor_system.h"

#include <algorithm>
#include <chrono>

#include "absl/strings/str_cat.h"
#include "nlohmann/json.hpp"
#include "zelda3/sprite/sprite.h"

namespace yaze {
namespace zelda3 {

namespace {

absl::Status SaveSingleRoomState(Rom* rom, Room& room) {
  RETURN_IF_ERROR(room.SaveObjects());
  RETURN_IF_ERROR(room.SaveObjectStreamHeader());
  RETURN_IF_ERROR(room.SaveSprites());
  RETURN_IF_ERROR(room.SaveRoomHeader());

  // Room-indexed tables (torches, chests, pot items, custom collision) must
  // preserve every other room's ROM data when saving a single room. Using the
  // full dungeon range keeps the save behavior consistent with the editor-level
  // save path and avoids truncating data for rooms after the edited one.
  const int room_limit = kNumberOfRooms;
  auto room_lookup_const = [&room](int room_id) -> const Room* {
    return room_id == room.id() ? &room : nullptr;
  };
  auto room_lookup_mut = [&room](int room_id) -> Room* {
    return room_id == room.id() ? &room : nullptr;
  };

  RETURN_IF_ERROR(SaveAllTorches(rom, room_limit, room_lookup_const));
  RETURN_IF_ERROR(SaveAllCollision(rom, room_limit, room_lookup_mut));
  RETURN_IF_ERROR(SaveAllChests(rom, room_limit, room_lookup_const));
  RETURN_IF_ERROR(SaveAllPotItems(rom, room_limit, room_lookup_const));
  return absl::OkStatus();
}

}  // namespace

DungeonEditorSystem::DungeonEditorSystem(Rom* rom, GameData* game_data)
    : rom_(rom), game_data_(game_data) {}

DungeonEditorSystem::~DungeonEditorSystem() {
  if (object_editor_) {
    object_editor_.reset();
  }
  external_room_ = nullptr;
}

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

  std::vector<int> room_ids;
  room_ids.reserve(rooms_.size() + 1);
  if (external_room_ != nullptr) {
    room_ids.push_back(external_room_->id());
  }
  for (const auto& [room_id, room] : rooms_) {
    (void)room;
    if (std::find(room_ids.begin(), room_ids.end(), room_id) ==
        room_ids.end()) {
      room_ids.push_back(room_id);
    }
  }

  for (int room_id : room_ids) {
    Room* room = GetManagedRoom(room_id);
    if (room == nullptr) {
      RETURN_IF_ERROR(LoadRoomData(room_id));
      room = GetManagedRoom(room_id);
    }
    if (room == nullptr || !room->IsLoaded()) {
      continue;
    }
    RETURN_IF_ERROR(room->SaveObjects());
    RETURN_IF_ERROR(room->SaveObjectStreamHeader());
    RETURN_IF_ERROR(room->SaveSprites());
    RETURN_IF_ERROR(room->SaveRoomHeader());
  }

  auto room_lookup_const = [this](int room_id) -> const Room* {
    return GetManagedRoom(room_id);
  };
  auto room_lookup_mut = [this](int room_id) -> Room* {
    return GetManagedRoom(room_id);
  };

  RETURN_IF_ERROR(SaveAllTorches(rom_, kNumberOfRooms, room_lookup_const));
  RETURN_IF_ERROR(SaveAllCollision(rom_, kNumberOfRooms, room_lookup_mut));
  RETURN_IF_ERROR(SaveAllChests(rom_, kNumberOfRooms, room_lookup_const));
  RETURN_IF_ERROR(SaveAllPotItems(rom_, kNumberOfRooms, room_lookup_const));

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
  if (room_id < 0 || room_id >= kNumberOfRooms) {
    return absl::InvalidArgumentError("Invalid room ID");
  }

  editor_state_.current_room_id = room_id;
  if (external_room_ == nullptr) {
    RETURN_IF_ERROR(LoadRoomData(room_id));
  }
  return BindObjectEditorToCurrentRoom();
}

int DungeonEditorSystem::GetCurrentRoom() const {
  return editor_state_.current_room_id;
}

absl::StatusOr<Room> DungeonEditorSystem::GetRoom(int room_id) {
  if (room_id < 0 || room_id >= kNumberOfRooms) {
    return absl::InvalidArgumentError("Invalid room ID");
  }

  return Room(room_id, rom_, game_data_);
}

std::shared_ptr<DungeonObjectEditor> DungeonEditorSystem::GetObjectEditor() {
  if (!object_editor_) {
    object_editor_ = std::make_shared<DungeonObjectEditor>(rom_);
    (void)object_editor_->InitializeEditor();
    (void)BindObjectEditorToCurrentRoom();
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
    (void)BindObjectEditorToCurrentRoom();
  }
}

void DungeonEditorSystem::SetExternalRoom(Room* room) {
  external_room_ = room;
  if (object_editor_) {
    (void)BindObjectEditorToCurrentRoom();
  }
}

absl::Status DungeonEditorSystem::RefreshRomBackedState(Room* external_room,
                                                        int current_room_id) {
  // Move the shared object editor away from managed Room storage before
  // clearing it. Panels retain this object-editor instance across ROM reloads.
  external_room_ = external_room;
  if (object_editor_) {
    (void)object_editor_->ClearSelection();
    object_editor_->ClearHistory();
    object_editor_->SetExternalRoom(external_room_);
  }

  rooms_.clear();
  ClearHistory();
  editor_state_.is_dirty = false;

  if (current_room_id < 0 || current_room_id >= kNumberOfRooms) {
    return absl::OkStatus();
  }
  editor_state_.current_room_id = current_room_id;
  if (external_room_ != nullptr) {
    return BindObjectEditorToCurrentRoom();
  }
  return LoadRoomData(current_room_id);
}

absl::Status DungeonEditorSystem::BindObjectEditorToCurrentRoom() {
  if (!object_editor_) {
    return absl::OkStatus();
  }
  if (external_room_ != nullptr) {
    object_editor_->SetExternalRoom(external_room_);
    return absl::OkStatus();
  }

  auto* room = GetManagedRoom(editor_state_.current_room_id);
  if (room == nullptr) {
    RETURN_IF_ERROR(LoadRoomData(editor_state_.current_room_id));
    room = GetManagedRoom(editor_state_.current_room_id);
  }
  if (room == nullptr) {
    return absl::NotFoundError("Current room is not available");
  }
  object_editor_->SetExternalRoom(room);
  return absl::OkStatus();
}

Room* DungeonEditorSystem::GetManagedRoom(int room_id) {
  if (external_room_ != nullptr && external_room_->id() == room_id) {
    return external_room_;
  }
  auto it = rooms_.find(room_id);
  if (it == rooms_.end()) {
    return nullptr;
  }
  return &it->second;
}

absl::Status DungeonEditorSystem::SaveManagedRoom(Room& room) {
  return SaveSingleRoomState(rom_, room);
}

absl::Status DungeonEditorSystem::InitializeObjectEditor() {
  if (!object_editor_) {
    object_editor_ = std::make_shared<DungeonObjectEditor>(rom_);
  }
  RETURN_IF_ERROR(object_editor_->InitializeEditor());
  return BindObjectEditorToCurrentRoom();
}

absl::Status DungeonEditorSystem::LoadRoomData(int room_id) {
  if (!rom_)
    return absl::InvalidArgumentError("ROM is null");

  if (room_id < 0 || room_id >= kNumberOfRooms) {
    return absl::InvalidArgumentError("Invalid room ID");
  }

  if (external_room_ != nullptr && external_room_->id() == room_id) {
    *external_room_ = LoadRoomFromRom(rom_, room_id);
    return BindObjectEditorToCurrentRoom();
  }

  rooms_[room_id] = LoadRoomFromRom(rom_, room_id);
  if (editor_state_.current_room_id == room_id) {
    RETURN_IF_ERROR(BindObjectEditorToCurrentRoom());
  }
  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::SaveRoomData(int room_id) {
  if (!rom_)
    return absl::InvalidArgumentError("ROM is null");

  if (room_id < 0 || room_id >= kNumberOfRooms) {
    return absl::InvalidArgumentError("Invalid room ID");
  }

  Room* room = GetManagedRoom(room_id);
  if (room == nullptr) {
    RETURN_IF_ERROR(LoadRoomData(room_id));
    room = GetManagedRoom(room_id);
  }
  if (room == nullptr) {
    return absl::NotFoundError("Room is not available");
  }
  if (!room->IsLoaded()) {
    return absl::OkStatus();
  }

  return SaveManagedRoom(*room);
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
  props["blockset"] = room.blockset();
  props["palette"] = room.palette();
  props["layout"] = room.layout_id();
  props["spriteset"] = room.spriteset();
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
      room.SetBlockset(props["blockset"]);
    if (props.contains("palette"))
      room.SetPalette(props["palette"]);
    if (props.contains("layout"))
      room.SetLayoutId(props["layout"]);
    if (props.contains("spriteset"))
      room.SetSpriteset(props["spriteset"]);
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
