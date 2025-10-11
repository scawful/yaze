#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_ROOM_SELECTOR_H
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_ROOM_SELECTOR_H

#include <functional>
#include "imgui/imgui.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/room_entrance.h"
#include "app/zelda3/dungeon/room.h"

namespace yaze {
namespace editor {

/**
 * @brief Handles room and entrance selection UI
 */
class DungeonRoomSelector {
 public:
  explicit DungeonRoomSelector(Rom* rom = nullptr) : rom_(rom) {}

  void Draw();
  void DrawRoomSelector();
  void DrawEntranceSelector();
  
  void set_rom(Rom* rom) { rom_ = rom; }
  Rom* rom() const { return rom_; }

  // Room selection
  void set_current_room_id(uint16_t room_id) { current_room_id_ = room_id; }
  int current_room_id() const { return current_room_id_; }
  
  void set_active_rooms(const ImVector<int>& rooms) { active_rooms_ = rooms; }
  const ImVector<int>& active_rooms() const { return active_rooms_; }
  ImVector<int>& mutable_active_rooms() { return active_rooms_; }

  // Entrance selection
  void set_current_entrance_id(int entrance_id) { current_entrance_id_ = entrance_id; }
  int current_entrance_id() const { return current_entrance_id_; }

  // Room data access
  void set_rooms(std::array<zelda3::Room, 0x128>* rooms) { rooms_ = rooms; }
  void set_entrances(std::array<zelda3::RoomEntrance, 0x8C>* entrances) { entrances_ = entrances; }

  // Callback for room selection events
  void set_room_selected_callback(std::function<void(int)> callback) { 
    room_selected_callback_ = callback; 
  }

 private:
  Rom* rom_ = nullptr;
  uint16_t current_room_id_ = 0;
  int current_entrance_id_ = 0;
  ImVector<int> active_rooms_;
  
  std::array<zelda3::Room, 0x128>* rooms_ = nullptr;
  std::array<zelda3::RoomEntrance, 0x8C>* entrances_ = nullptr;
  
  // Callback for room selection events
  std::function<void(int)> room_selected_callback_;
};

}  // namespace editor
}  // namespace yaze

#endif
