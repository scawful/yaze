#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_ROOM_LOADER_H
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_ROOM_LOADER_H

#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "app/rom.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_entrance.h"

namespace yaze {
namespace editor {

/**
 * @brief Manages loading and saving of dungeon room data
 * 
 * This component handles all ROM-related operations for loading room data,
 * calculating room sizes, and managing room graphics.
 */
class DungeonRoomLoader {
 public:
  explicit DungeonRoomLoader(Rom* rom) : rom_(rom) {}

  // Room loading
  absl::Status LoadRoom(int room_id, zelda3::Room& room);
  absl::Status LoadAllRooms(std::array<zelda3::Room, 0x128>& rooms);
  absl::Status LoadRoomEntrances(
      std::array<zelda3::RoomEntrance, 0x8C>& entrances);

  // Room size management
  void LoadDungeonRoomSize();
  uint64_t GetTotalRoomSize() const { return total_room_size_; }

  // Room graphics
  absl::Status LoadAndRenderRoomGraphics(zelda3::Room& room);
  absl::Status ReloadAllRoomGraphics(std::array<zelda3::Room, 0x128>& rooms);

  // Data access
  const std::vector<int64_t>& GetRoomSizePointers() const {
    return room_size_pointers_;
  }
  const std::vector<int64_t>& GetRoomSizes() const { return room_sizes_; }
  const std::unordered_map<int, int>& GetRoomSizeAddresses() const {
    return room_size_addresses_;
  }
  const std::unordered_map<int, ImVec4>& GetRoomPalette() const {
    return room_palette_;
  }

 private:
  Rom* rom_;

  std::vector<int64_t> room_size_pointers_;
  std::vector<int64_t> room_sizes_;
  std::unordered_map<int, int> room_size_addresses_;
  std::unordered_map<int, ImVec4> room_palette_;
  uint64_t total_room_size_ = 0;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_ROOM_LOADER_H
