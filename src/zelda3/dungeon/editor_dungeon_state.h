#ifndef YAZE_ZELDA3_DUNGEON_EDITOR_DUNGEON_STATE_H
#define YAZE_ZELDA3_DUNGEON_EDITOR_DUNGEON_STATE_H

#include <map>
#include "zelda3/dungeon/dungeon_state.h"

namespace yaze {
class Rom;
namespace zelda3 {
struct GameData;

/**
 * @brief Editor implementation of DungeonState.
 *
 * Stores state in memory to allow the editor to toggle states
 * and visualize the effects.
 */
class EditorDungeonState : public DungeonState {
 public:
  EditorDungeonState(Rom* rom, GameData* game_data) : rom_(rom), game_data_(game_data) {}

  // Chest State
  bool IsChestOpen(int room_id, int chest_index) const override {
    auto it = chest_states_.find({room_id, chest_index});
    if (it != chest_states_.end()) {
      return it->second;
    }
    return false; // Default closed
  }
  
  void SetChestOpen(int room_id, int chest_index, bool open) {
    chest_states_[{room_id, chest_index}] = open;
  }

  bool IsBigChestOpen() const override { return big_chest_open_; }
  void SetBigChestOpen(bool open) { big_chest_open_ = open; }

  // Door State
  bool IsDoorOpen(int room_id, int door_index) const override {
    auto it = door_states_.find({room_id, door_index});
    if (it != door_states_.end()) {
      return it->second;
    }
    return false; // Default closed
  }
  
  void SetDoorOpen(int room_id, int door_index, bool open) {
    door_states_[{room_id, door_index}] = open;
  }

  bool IsDoorSwitchActive(int room_id) const override {
    auto it = door_switch_states_.find(room_id);
    if (it != door_switch_states_.end()) {
      return it->second;
    }
    return false; // Default inactive
  }
  
  void SetDoorSwitchActive(int room_id, bool active) {
    door_switch_states_[room_id] = active;
  }

  // Object State
  bool IsWallMoved(int room_id) const override {
    auto it = wall_moved_states_.find(room_id);
    if (it != wall_moved_states_.end()) {
      return it->second;
    }
    return false; // Default not moved
  }
  
  void SetWallMoved(int room_id, bool moved) {
    wall_moved_states_[room_id] = moved;
  }

  bool IsFloorBombable(int room_id) const override {
    auto it = floor_bombable_states_.find(room_id);
    if (it != floor_bombable_states_.end()) {
      return it->second;
    }
    return false; // Default solid
  }
  
  void SetFloorBombable(int room_id, bool bombed) {
    floor_bombable_states_[room_id] = bombed;
  }

  bool IsRupeeFloorActive(int room_id) const override {
    auto it = rupee_floor_states_.find(room_id);
    if (it != rupee_floor_states_.end()) {
      return it->second;
    }
    return false; // Default hidden/inactive
  }
  
  void SetRupeeFloorActive(int room_id, bool active) {
    rupee_floor_states_[room_id] = active;
  }

  // General Flags
  bool IsCrystalSwitchBlue() const override { return crystal_switch_blue_; }
  void SetCrystalSwitchBlue(bool blue) { crystal_switch_blue_ = blue; }

  // Reset all state
  void Reset() {
    chest_states_.clear();
    big_chest_open_ = false;
    door_states_.clear();
    door_switch_states_.clear();
    wall_moved_states_.clear();
    floor_bombable_states_.clear();
    rupee_floor_states_.clear();
    crystal_switch_blue_ = true; // Default blue
  }

 private:
  Rom* rom_;
  GameData* game_data_;

  // State storage
  std::map<std::pair<int, int>, bool> chest_states_;
  bool big_chest_open_ = false;
  
  std::map<std::pair<int, int>, bool> door_states_;
  std::map<int, bool> door_switch_states_;
  
  std::map<int, bool> wall_moved_states_;
  std::map<int, bool> floor_bombable_states_;
  std::map<int, bool> rupee_floor_states_;
  
  bool crystal_switch_blue_ = true;
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_EDITOR_DUNGEON_STATE_H
