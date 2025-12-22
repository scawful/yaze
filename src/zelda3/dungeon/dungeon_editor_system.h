#ifndef YAZE_APP_ZELDA3_DUNGEON_DUNGEON_EDITOR_SYSTEM_H
#define YAZE_APP_ZELDA3_DUNGEON_DUNGEON_EDITOR_SYSTEM_H

#include <chrono>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "dungeon_object_editor.h"
#include "rom/rom.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace zelda3 {

/**
 * @brief Core dungeon editing system
 *
 * Provides dungeon editing functionality focused on:
 * - Room loading and saving
 * - Object editing (delegated to DungeonObjectEditor)
 * - Undo/redo support
 *
 * Note: Sprite, item, door, chest, and entrance editing is handled
 * directly by the Room class and UI panels, not by this system.
 */
class DungeonEditorSystem {
 public:
  // Editor state
  struct EditorState {
    int current_room_id = 0;
    bool is_dirty = false;
    std::chrono::steady_clock::time_point last_save_time;
  };

  explicit DungeonEditorSystem(Rom* rom, GameData* game_data = nullptr);
  ~DungeonEditorSystem() = default;

  void SetGameData(GameData* game_data) { game_data_ = game_data; }

  // System initialization and management
  absl::Status Initialize();
  absl::Status LoadDungeon(int dungeon_id);
  absl::Status SaveDungeon();
  absl::Status SaveRoom(int room_id);
  absl::Status ReloadRoom(int room_id);

  // Room management
  absl::Status SetCurrentRoom(int room_id);
  int GetCurrentRoom() const;
  absl::StatusOr<Room> GetRoom(int room_id);

  // Object editing (delegated to DungeonObjectEditor)
  std::shared_ptr<DungeonObjectEditor> GetObjectEditor();

  // Undo/Redo system
  absl::Status Undo();
  absl::Status Redo();
  bool CanUndo() const;
  bool CanRedo() const;
  void ClearHistory();

  // Event callbacks
  using RoomChangedCallback = std::function<void(int room_id)>;
  void SetRoomChangedCallback(RoomChangedCallback callback);

  // Getters
  EditorState GetEditorState() const;
  Rom* GetROM() const;
  bool IsDirty() const;

  // ROM management
  void SetROM(Rom* rom);
  void SetExternalRoom(Room* room);

 private:
  absl::Status InitializeObjectEditor();
  absl::Status LoadRoomData(int room_id);
  absl::Status SaveRoomData(int room_id);

  Rom* rom_;
  GameData* game_data_ = nullptr;
  std::shared_ptr<DungeonObjectEditor> object_editor_;

  EditorState editor_state_;

  // Room data storage
  std::unordered_map<int, Room> rooms_;

  // Event callbacks
  RoomChangedCallback room_changed_callback_;

  // Undo/Redo system - stores room object snapshots
  struct UndoPoint {
    EditorState state;
    std::unordered_map<int, Room> rooms;
    std::chrono::steady_clock::time_point timestamp;
  };

  std::vector<UndoPoint> undo_history_;
  std::vector<UndoPoint> redo_history_;
  static constexpr size_t kMaxUndoHistory = 100;
};

/**
 * @brief Factory function to create dungeon editor system
 */
std::unique_ptr<DungeonEditorSystem> CreateDungeonEditorSystem(
    Rom* rom, GameData* game_data = nullptr);

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_DUNGEON_DUNGEON_EDITOR_SYSTEM_H
