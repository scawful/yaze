#include "dungeon_editor_system.h"

#include <algorithm>
#include <chrono>

#include "absl/strings/str_format.h"

namespace yaze {
namespace zelda3 {

DungeonEditorSystem::DungeonEditorSystem(Rom* rom) : rom_(rom) {}

absl::Status DungeonEditorSystem::Initialize() {
  if (rom_ == nullptr) {
    return absl::InvalidArgumentError("ROM is null");
  }

  // Initialize default dungeon settings
  dungeon_settings_.dungeon_id = 0;
  dungeon_settings_.name = "Default Dungeon";
  dungeon_settings_.description = "A dungeon created with the editor";
  dungeon_settings_.total_rooms = 0;
  dungeon_settings_.starting_room_id = 0;
  dungeon_settings_.boss_room_id = 0;
  dungeon_settings_.music_theme_id = 0;
  dungeon_settings_.color_palette_id = 0;
  dungeon_settings_.has_map = true;
  dungeon_settings_.has_compass = true;
  dungeon_settings_.has_big_key = true;

  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::LoadDungeon(int dungeon_id) {
  // TODO: Implement actual dungeon loading from ROM
  editor_state_.current_room_id = 0;
  editor_state_.is_dirty = false;
  editor_state_.auto_save_enabled = true;
  editor_state_.last_save_time = std::chrono::steady_clock::now();

  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::SaveDungeon() {
  // TODO: Implement actual dungeon saving to ROM
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

void DungeonEditorSystem::SetEditorMode(EditorMode mode) {
  editor_state_.current_mode = mode;
}

DungeonEditorSystem::EditorMode DungeonEditorSystem::GetEditorMode() const {
  return editor_state_.current_mode;
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

  // TODO: Load room from ROM or return cached room
  return Room(room_id, rom_);
}

absl::Status DungeonEditorSystem::CreateRoom(int room_id,
                                             const std::string& name) {
  // TODO: Implement room creation
  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::DeleteRoom(int room_id) {
  // TODO: Implement room deletion
  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::DuplicateRoom(int source_room_id,
                                                int target_room_id) {
  // TODO: Implement room duplication
  return absl::OkStatus();
}

std::shared_ptr<DungeonObjectEditor> DungeonEditorSystem::GetObjectEditor() {
  if (!object_editor_) {
    object_editor_ = std::make_shared<DungeonObjectEditor>(rom_);
  }
  return object_editor_;
}

absl::Status DungeonEditorSystem::SetObjectEditorMode() {
  editor_state_.current_mode = EditorMode::kObjects;
  return absl::OkStatus();
}

// Sprite management
absl::Status DungeonEditorSystem::AddSprite(const SpriteData& sprite_data) {
  int sprite_id = GenerateSpriteId();
  sprites_[sprite_id] = sprite_data;
  sprites_[sprite_id].sprite_id = sprite_id;

  if (sprite_changed_callback_) {
    sprite_changed_callback_(sprite_id);
  }

  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::RemoveSprite(int sprite_id) {
  auto it = sprites_.find(sprite_id);
  if (it == sprites_.end()) {
    return absl::NotFoundError("Sprite not found");
  }

  sprites_.erase(it);
  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::UpdateSprite(int sprite_id,
                                               const SpriteData& sprite_data) {
  auto it = sprites_.find(sprite_id);
  if (it == sprites_.end()) {
    return absl::NotFoundError("Sprite not found");
  }

  it->second = sprite_data;
  it->second.sprite_id = sprite_id;

  if (sprite_changed_callback_) {
    sprite_changed_callback_(sprite_id);
  }

  return absl::OkStatus();
}

absl::StatusOr<DungeonEditorSystem::SpriteData> DungeonEditorSystem::GetSprite(
    int sprite_id) {
  auto it = sprites_.find(sprite_id);
  if (it == sprites_.end()) {
    return absl::NotFoundError("Sprite not found");
  }

  return it->second;
}

absl::StatusOr<std::vector<DungeonEditorSystem::SpriteData>>
DungeonEditorSystem::GetSpritesByRoom(int room_id) {
  std::vector<SpriteData> room_sprites;

  for (const auto& [id, sprite] : sprites_) {
    if (sprite.x >= 0 && sprite.y >= 0) {  // Simple room assignment logic
      room_sprites.push_back(sprite);
    }
  }

  return room_sprites;
}

absl::StatusOr<std::vector<DungeonEditorSystem::SpriteData>>
DungeonEditorSystem::GetSpritesByType(SpriteType type) {
  std::vector<SpriteData> typed_sprites;

  for (const auto& [id, sprite] : sprites_) {
    if (sprite.type == type) {
      typed_sprites.push_back(sprite);
    }
  }

  return typed_sprites;
}

absl::Status DungeonEditorSystem::MoveSprite(int sprite_id, int new_x,
                                             int new_y) {
  auto it = sprites_.find(sprite_id);
  if (it == sprites_.end()) {
    return absl::NotFoundError("Sprite not found");
  }

  it->second.x = new_x;
  it->second.y = new_y;

  if (sprite_changed_callback_) {
    sprite_changed_callback_(sprite_id);
  }

  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::SetSpriteActive(int sprite_id, bool active) {
  auto it = sprites_.find(sprite_id);
  if (it == sprites_.end()) {
    return absl::NotFoundError("Sprite not found");
  }

  it->second.is_active = active;

  if (sprite_changed_callback_) {
    sprite_changed_callback_(sprite_id);
  }

  return absl::OkStatus();
}

// Item management
absl::Status DungeonEditorSystem::AddItem(const ItemData& item_data) {
  int item_id = GenerateItemId();
  items_[item_id] = item_data;
  items_[item_id].item_id = item_id;

  if (item_changed_callback_) {
    item_changed_callback_(item_id);
  }

  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::RemoveItem(int item_id) {
  auto it = items_.find(item_id);
  if (it == items_.end()) {
    return absl::NotFoundError("Item not found");
  }

  items_.erase(it);
  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::UpdateItem(int item_id,
                                             const ItemData& item_data) {
  auto it = items_.find(item_id);
  if (it == items_.end()) {
    return absl::NotFoundError("Item not found");
  }

  it->second = item_data;
  it->second.item_id = item_id;

  if (item_changed_callback_) {
    item_changed_callback_(item_id);
  }

  return absl::OkStatus();
}

absl::StatusOr<DungeonEditorSystem::ItemData> DungeonEditorSystem::GetItem(
    int item_id) {
  auto it = items_.find(item_id);
  if (it == items_.end()) {
    return absl::NotFoundError("Item not found");
  }

  return it->second;
}

absl::StatusOr<std::vector<DungeonEditorSystem::ItemData>>
DungeonEditorSystem::GetItemsByRoom(int room_id) {
  std::vector<ItemData> room_items;

  for (const auto& [id, item] : items_) {
    if (item.room_id == room_id) {
      room_items.push_back(item);
    }
  }

  return room_items;
}

absl::StatusOr<std::vector<DungeonEditorSystem::ItemData>>
DungeonEditorSystem::GetItemsByType(ItemType type) {
  std::vector<ItemData> typed_items;

  for (const auto& [id, item] : items_) {
    if (item.type == type) {
      typed_items.push_back(item);
    }
  }

  return typed_items;
}

absl::Status DungeonEditorSystem::MoveItem(int item_id, int new_x, int new_y) {
  auto it = items_.find(item_id);
  if (it == items_.end()) {
    return absl::NotFoundError("Item not found");
  }

  it->second.x = new_x;
  it->second.y = new_y;

  if (item_changed_callback_) {
    item_changed_callback_(item_id);
  }

  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::SetItemHidden(int item_id, bool hidden) {
  auto it = items_.find(item_id);
  if (it == items_.end()) {
    return absl::NotFoundError("Item not found");
  }

  it->second.is_hidden = hidden;

  if (item_changed_callback_) {
    item_changed_callback_(item_id);
  }

  return absl::OkStatus();
}

// Entrance/exit management
absl::Status DungeonEditorSystem::AddEntrance(
    const EntranceData& entrance_data) {
  int entrance_id = GenerateEntranceId();
  entrances_[entrance_id] = entrance_data;
  entrances_[entrance_id].entrance_id = entrance_id;

  if (entrance_changed_callback_) {
    entrance_changed_callback_(entrance_id);
  }

  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::RemoveEntrance(int entrance_id) {
  auto it = entrances_.find(entrance_id);
  if (it == entrances_.end()) {
    return absl::NotFoundError("Entrance not found");
  }

  entrances_.erase(it);
  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::UpdateEntrance(
    int entrance_id, const EntranceData& entrance_data) {
  auto it = entrances_.find(entrance_id);
  if (it == entrances_.end()) {
    return absl::NotFoundError("Entrance not found");
  }

  it->second = entrance_data;
  it->second.entrance_id = entrance_id;

  if (entrance_changed_callback_) {
    entrance_changed_callback_(entrance_id);
  }

  return absl::OkStatus();
}

absl::StatusOr<DungeonEditorSystem::EntranceData>
DungeonEditorSystem::GetEntrance(int entrance_id) {
  auto it = entrances_.find(entrance_id);
  if (it == entrances_.end()) {
    return absl::NotFoundError("Entrance not found");
  }

  return it->second;
}

absl::StatusOr<std::vector<DungeonEditorSystem::EntranceData>>
DungeonEditorSystem::GetEntrancesByRoom(int room_id) {
  std::vector<EntranceData> room_entrances;

  for (const auto& [id, entrance] : entrances_) {
    if (entrance.source_room_id == room_id ||
        entrance.target_room_id == room_id) {
      room_entrances.push_back(entrance);
    }
  }

  return room_entrances;
}

absl::StatusOr<std::vector<DungeonEditorSystem::EntranceData>>
DungeonEditorSystem::GetEntrancesByType(EntranceType type) {
  std::vector<EntranceData> typed_entrances;

  for (const auto& [id, entrance] : entrances_) {
    if (entrance.type == type) {
      typed_entrances.push_back(entrance);
    }
  }

  return typed_entrances;
}

absl::Status DungeonEditorSystem::ConnectRooms(int room1_id, int room2_id,
                                               int x1, int y1, int x2, int y2) {
  EntranceData entrance_data;
  entrance_data.source_room_id = room1_id;
  entrance_data.target_room_id = room2_id;
  entrance_data.source_x = x1;
  entrance_data.source_y = y1;
  entrance_data.target_x = x2;
  entrance_data.target_y = y2;
  entrance_data.type = EntranceType::kNormal;
  entrance_data.is_bidirectional = true;

  return AddEntrance(entrance_data);
}

absl::Status DungeonEditorSystem::DisconnectRooms(int room1_id, int room2_id) {
  // Find and remove entrance between rooms
  for (auto it = entrances_.begin(); it != entrances_.end();) {
    const auto& entrance = it->second;
    if ((entrance.source_room_id == room1_id &&
         entrance.target_room_id == room2_id) ||
        (entrance.source_room_id == room2_id &&
         entrance.target_room_id == room1_id)) {
      it = entrances_.erase(it);
    } else {
      ++it;
    }
  }

  return absl::OkStatus();
}

// Door management
absl::Status DungeonEditorSystem::AddDoor(const DoorData& door_data) {
  int door_id = GenerateDoorId();
  doors_[door_id] = door_data;
  doors_[door_id].door_id = door_id;

  if (door_changed_callback_) {
    door_changed_callback_(door_id);
  }

  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::RemoveDoor(int door_id) {
  auto it = doors_.find(door_id);
  if (it == doors_.end()) {
    return absl::NotFoundError("Door not found");
  }

  doors_.erase(it);
  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::UpdateDoor(int door_id,
                                             const DoorData& door_data) {
  auto it = doors_.find(door_id);
  if (it == doors_.end()) {
    return absl::NotFoundError("Door not found");
  }

  it->second = door_data;
  it->second.door_id = door_id;

  if (door_changed_callback_) {
    door_changed_callback_(door_id);
  }

  return absl::OkStatus();
}

absl::StatusOr<DungeonEditorSystem::DoorData> DungeonEditorSystem::GetDoor(
    int door_id) {
  auto it = doors_.find(door_id);
  if (it == doors_.end()) {
    return absl::NotFoundError("Door not found");
  }

  return it->second;
}

absl::StatusOr<std::vector<DungeonEditorSystem::DoorData>>
DungeonEditorSystem::GetDoorsByRoom(int room_id) {
  std::vector<DoorData> room_doors;

  for (const auto& [id, door] : doors_) {
    if (door.room_id == room_id) {
      room_doors.push_back(door);
    }
  }

  return room_doors;
}

absl::Status DungeonEditorSystem::SetDoorLocked(int door_id, bool locked) {
  auto it = doors_.find(door_id);
  if (it == doors_.end()) {
    return absl::NotFoundError("Door not found");
  }

  it->second.is_locked = locked;

  if (door_changed_callback_) {
    door_changed_callback_(door_id);
  }

  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::SetDoorKeyRequirement(int door_id,
                                                        bool requires_key,
                                                        int key_type) {
  auto it = doors_.find(door_id);
  if (it == doors_.end()) {
    return absl::NotFoundError("Door not found");
  }

  it->second.requires_key = requires_key;
  it->second.key_type = key_type;

  if (door_changed_callback_) {
    door_changed_callback_(door_id);
  }

  return absl::OkStatus();
}

// Chest management
absl::Status DungeonEditorSystem::AddChest(const ChestData& chest_data) {
  int chest_id = GenerateChestId();
  chests_[chest_id] = chest_data;
  chests_[chest_id].chest_id = chest_id;

  if (chest_changed_callback_) {
    chest_changed_callback_(chest_id);
  }

  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::RemoveChest(int chest_id) {
  auto it = chests_.find(chest_id);
  if (it == chests_.end()) {
    return absl::NotFoundError("Chest not found");
  }

  chests_.erase(it);
  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::UpdateChest(int chest_id,
                                              const ChestData& chest_data) {
  auto it = chests_.find(chest_id);
  if (it == chests_.end()) {
    return absl::NotFoundError("Chest not found");
  }

  it->second = chest_data;
  it->second.chest_id = chest_id;

  if (chest_changed_callback_) {
    chest_changed_callback_(chest_id);
  }

  return absl::OkStatus();
}

absl::StatusOr<DungeonEditorSystem::ChestData> DungeonEditorSystem::GetChest(
    int chest_id) {
  auto it = chests_.find(chest_id);
  if (it == chests_.end()) {
    return absl::NotFoundError("Chest not found");
  }

  return it->second;
}

absl::StatusOr<std::vector<DungeonEditorSystem::ChestData>>
DungeonEditorSystem::GetChestsByRoom(int room_id) {
  std::vector<ChestData> room_chests;

  for (const auto& [id, chest] : chests_) {
    if (chest.room_id == room_id) {
      room_chests.push_back(chest);
    }
  }

  return room_chests;
}

absl::Status DungeonEditorSystem::SetChestItem(int chest_id, int item_id,
                                               int quantity) {
  auto it = chests_.find(chest_id);
  if (it == chests_.end()) {
    return absl::NotFoundError("Chest not found");
  }

  it->second.item_id = item_id;
  it->second.item_quantity = quantity;

  if (chest_changed_callback_) {
    chest_changed_callback_(chest_id);
  }

  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::SetChestOpened(int chest_id, bool opened) {
  auto it = chests_.find(chest_id);
  if (it == chests_.end()) {
    return absl::NotFoundError("Chest not found");
  }

  it->second.is_opened = opened;

  if (chest_changed_callback_) {
    chest_changed_callback_(chest_id);
  }

  return absl::OkStatus();
}

// Room properties and metadata
absl::Status DungeonEditorSystem::SetRoomProperties(
    int room_id, const RoomProperties& properties) {
  room_properties_[room_id] = properties;

  if (room_changed_callback_) {
    room_changed_callback_(room_id);
  }

  return absl::OkStatus();
}

absl::StatusOr<DungeonEditorSystem::RoomProperties>
DungeonEditorSystem::GetRoomProperties(int room_id) {
  auto it = room_properties_.find(room_id);
  if (it == room_properties_.end()) {
    // Return default properties
    RoomProperties default_properties;
    default_properties.room_id = room_id;
    default_properties.name = absl::StrFormat("Room %d", room_id);
    default_properties.description = "";
    default_properties.dungeon_id = 0;
    default_properties.floor_level = 0;
    default_properties.is_boss_room = false;
    default_properties.is_save_room = false;
    default_properties.is_shop_room = false;
    default_properties.music_id = 0;
    default_properties.ambient_sound_id = 0;
    return default_properties;
  }

  return it->second;
}

// Dungeon-wide settings
absl::Status DungeonEditorSystem::SetDungeonSettings(
    const DungeonSettings& settings) {
  dungeon_settings_ = settings;
  return absl::OkStatus();
}

absl::StatusOr<DungeonEditorSystem::DungeonSettings>
DungeonEditorSystem::GetDungeonSettings() {
  return dungeon_settings_;
}

// Validation and error checking
absl::Status DungeonEditorSystem::ValidateRoom(int room_id) {
  // TODO: Implement room validation
  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::ValidateDungeon() {
  // TODO: Implement dungeon validation
  return absl::OkStatus();
}

std::vector<std::string> DungeonEditorSystem::GetValidationErrors(int room_id) {
  // TODO: Implement validation error collection
  return {};
}

std::vector<std::string> DungeonEditorSystem::GetDungeonValidationErrors() {
  // TODO: Implement dungeon validation error collection
  return {};
}

// Rendering and preview
absl::StatusOr<gfx::Bitmap> DungeonEditorSystem::RenderRoom(int room_id) {
  // TODO: Implement room rendering
  return gfx::Bitmap();
}

absl::StatusOr<gfx::Bitmap> DungeonEditorSystem::RenderRoomPreview(
    int room_id, EditorMode mode) {
  // TODO: Implement room preview rendering
  return gfx::Bitmap();
}

absl::StatusOr<gfx::Bitmap> DungeonEditorSystem::RenderDungeonMap() {
  // TODO: Implement dungeon map rendering
  return gfx::Bitmap();
}

// Import/Export functionality
absl::Status DungeonEditorSystem::ImportRoomFromFile(
    const std::string& file_path, int room_id) {
  // TODO: Implement room import
  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::ExportRoomToFile(
    int room_id, const std::string& file_path) {
  // TODO: Implement room export
  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::ImportDungeonFromFile(
    const std::string& file_path) {
  // TODO: Implement dungeon import
  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::ExportDungeonToFile(
    const std::string& file_path) {
  // TODO: Implement dungeon export
  return absl::OkStatus();
}

// Undo/Redo system
absl::Status DungeonEditorSystem::Undo() {
  if (!CanUndo()) {
    return absl::FailedPreconditionError("Nothing to undo");
  }

  // TODO: Implement undo functionality
  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::Redo() {
  if (!CanRedo()) {
    return absl::FailedPreconditionError("Nothing to redo");
  }

  // TODO: Implement redo functionality
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

// Event callbacks
void DungeonEditorSystem::SetRoomChangedCallback(RoomChangedCallback callback) {
  room_changed_callback_ = callback;
}

void DungeonEditorSystem::SetSpriteChangedCallback(
    SpriteChangedCallback callback) {
  sprite_changed_callback_ = callback;
}

void DungeonEditorSystem::SetItemChangedCallback(ItemChangedCallback callback) {
  item_changed_callback_ = callback;
}

void DungeonEditorSystem::SetEntranceChangedCallback(
    EntranceChangedCallback callback) {
  entrance_changed_callback_ = callback;
}

void DungeonEditorSystem::SetDoorChangedCallback(DoorChangedCallback callback) {
  door_changed_callback_ = callback;
}

void DungeonEditorSystem::SetChestChangedCallback(
    ChestChangedCallback callback) {
  chest_changed_callback_ = callback;
}

void DungeonEditorSystem::SetModeChangedCallback(ModeChangedCallback callback) {
  mode_changed_callback_ = callback;
}

void DungeonEditorSystem::SetValidationCallback(ValidationCallback callback) {
  validation_callback_ = callback;
}

// Helper methods
int DungeonEditorSystem::GenerateSpriteId() {
  return next_sprite_id_++;
}

int DungeonEditorSystem::GenerateItemId() {
  return next_item_id_++;
}

int DungeonEditorSystem::GenerateEntranceId() {
  return next_entrance_id_++;
}

int DungeonEditorSystem::GenerateDoorId() {
  return next_door_id_++;
}

int DungeonEditorSystem::GenerateChestId() {
  return next_chest_id_++;
}

Rom* DungeonEditorSystem::GetROM() const {
  return rom_;
}

bool DungeonEditorSystem::IsDirty() const {
  return editor_state_.is_dirty;
}

void DungeonEditorSystem::SetROM(Rom* rom) {
  rom_ = rom;
  // Update object editor with new ROM if it exists
  if (object_editor_) {
    object_editor_->SetROM(rom);
  }
}

// Data management
absl::Status DungeonEditorSystem::LoadRoomData(int room_id) {
  if (!rom_) return absl::InvalidArgumentError("ROM is null");

  // Load the room from ROM to get current data
  Room room = LoadRoomFromRom(rom_, room_id);
  
  // 1. Load Sprites
  // Clear existing sprites for this room to avoid duplicates on reload
  for (auto it = sprites_.begin(); it != sprites_.end();) {
    if (it->second.properties.count("room_id") && std::stoi(it->second.properties.at("room_id")) == room_id) {
      it = sprites_.erase(it);
    } else {
      ++it;
    }
  }
  
  const auto& room_sprites = room.GetSprites();
  for (const auto& spr : room_sprites) {
    SpriteData data;
    data.sprite_id = GenerateSpriteId();
    data.x = spr.x();
    data.y = spr.y();
    data.layer = spr.layer();
    data.type = SpriteType::kEnemy; // Default, should map from spr.id()
    data.name = absl::StrFormat("Sprite %02X", spr.id());
    data.properties["id"] = absl::StrFormat("%d", spr.id());
    data.properties["subtype"] = absl::StrFormat("%d", spr.subtype());
    data.properties["room_id"] = absl::StrFormat("%d", room_id);
    
    sprites_[data.sprite_id] = data;
  }

  // 2. Load Chests
  // Clear existing chests for this room
  for (auto it = chests_.begin(); it != chests_.end();) {
    if (it->second.room_id == room_id) {
      it = chests_.erase(it);
    } else {
      ++it;
    }
  }

  const auto& room_chests = room.GetChests();
  for (const auto& chest : room_chests) {
    ChestData data;
    data.chest_id = GenerateChestId();
    data.room_id = room_id;
    data.item_id = chest.id; // Raw item ID
    data.is_big_chest = chest.size;
    chests_[data.chest_id] = data;
  }

  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::SaveRoomData(int room_id) {
  if (!rom_) return absl::InvalidArgumentError("ROM is null");

  // Load room first to get pointers/metadata correct
  Room room = LoadRoomFromRom(rom_, room_id);
  
  // 1. Save Sprites
  room.GetSprites().clear();
  for (const auto& [id, sprite_data] : sprites_) {
    auto room_id_it = sprite_data.properties.find("room_id");
    if (room_id_it != sprite_data.properties.end()) {
      if (std::stoi(room_id_it->second) != room_id) continue;
    } else {
      continue; 
    }

    int raw_id = 0;
    int subtype = 0;
    if (sprite_data.properties.count("id")) raw_id = std::stoi(sprite_data.properties.at("id"));
    if (sprite_data.properties.count("subtype")) subtype = std::stoi(sprite_data.properties.at("subtype"));

    zelda3::Sprite z3_sprite(raw_id, sprite_data.x, sprite_data.y, subtype, sprite_data.layer);
    room.GetSprites().push_back(z3_sprite);
  }
  
  auto status = room.SaveSprites();
  if (!status.ok()) return status;

  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::LoadSpriteData() {
  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::SaveSpriteData() {
  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::LoadItemData() {
  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::SaveItemData() {
  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::LoadEntranceData() {
  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::SaveEntranceData() {
  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::LoadDoorData() {
  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::SaveDoorData() {
  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::LoadChestData() {
  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::SaveChestData() {
  return absl::OkStatus();
}

// Factory function
std::unique_ptr<DungeonEditorSystem> CreateDungeonEditorSystem(Rom* rom) {
  return std::make_unique<DungeonEditorSystem>(rom);
}

}  // namespace zelda3
}  // namespace yaze
