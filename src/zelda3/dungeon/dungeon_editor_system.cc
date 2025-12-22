#include "dungeon_editor_system.h"

#include <chrono>

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
  if (!rom_) return absl::InvalidArgumentError("ROM is null");

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

bool DungeonEditorSystem::CanUndo() const { return !undo_history_.empty(); }

bool DungeonEditorSystem::CanRedo() const { return !redo_history_.empty(); }

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

Rom* DungeonEditorSystem::GetROM() const { return rom_; }

bool DungeonEditorSystem::IsDirty() const { return editor_state_.is_dirty; }

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
  if (!rom_) return absl::InvalidArgumentError("ROM is null");

  // Room loading is handled by the Room class itself
  // This method exists for consistency with the API
  return absl::OkStatus();
}

absl::Status DungeonEditorSystem::SaveRoomData(int room_id) {
  if (!rom_) return absl::InvalidArgumentError("ROM is null");

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

}  // namespace zelda3
}  // namespace yaze
