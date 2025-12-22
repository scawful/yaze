#ifndef YAZE_ZELDA3_DUNGEON_DUNGEON_STATE_H
#define YAZE_ZELDA3_DUNGEON_DUNGEON_STATE_H

#include <cstdint>

namespace yaze {
namespace zelda3 {

/**
 * @brief Interface for accessing dungeon game state.
 *
 * This interface abstracts the access to game state variables (SRAM/RAM)
 * that affect object rendering and logic. This allows the editor to
 * simulate game state without a full emulator running.
 */
class DungeonState {
 public:
  virtual ~DungeonState() = default;

  // Chest State
  virtual bool IsChestOpen(int room_id, int chest_index) const = 0;
  virtual bool IsBigChestOpen() const = 0;

  // Door State
  virtual bool IsDoorOpen(int room_id, int door_index) const = 0;
  virtual bool IsDoorSwitchActive(int room_id) const = 0;

  // Object State
  virtual bool IsWallMoved(int room_id) const = 0;
  virtual bool IsFloorBombable(int room_id) const = 0;
  virtual bool IsRupeeFloorActive(int room_id) const = 0;
  
  // General Flags
  virtual bool IsCrystalSwitchBlue() const = 0;
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_DUNGEON_STATE_H
