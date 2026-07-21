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
  // Legacy editor-wide big-chest preview override.
  virtual bool IsBigChestOpen() const = 0;
  // Runtime big chests use the same room+slot flag domain as small chests.
  // Preserve the legacy global preview toggle as a compatibility override.
  virtual bool IsBigChestOpen(int room_id, int chest_index) const {
    return IsChestOpen(room_id, chest_index) || IsBigChestOpen();
  }

  // Door State
  virtual bool IsDoorOpen(int room_id, int door_index) const = 0;
  virtual bool IsDoorSwitchActive(int room_id) const = 0;

  // Big-key locks share the room-event slot counter used by chests in the
  // original engine ($0498 / RoomFlagMask). Keep this separate from door
  // state so rooms with multiple locks or a chest before a lock can be
  // previewed correctly. Existing state implementations inherit chest-slot
  // behavior until they need a distinct lock preview control.
  virtual bool IsBigKeyLockOpen(int room_id, int room_event_index) const {
    return IsChestOpen(room_id, room_event_index);
  }

  // Water Face State (Type 3 object 0xF80 active variant)
  // Default false so implementations can opt in without breaking callers.
  virtual bool IsWaterFaceActive(int room_id) const {
    (void)room_id;
    return false;
  }

  // Dam Floodgate State (Type 2 object 0x137 alternate water-open variant)
  // Default false so implementations can opt in without breaking callers.
  virtual bool IsDamFloodgateOpen(int room_id) const {
    (void)room_id;
    return false;
  }

  // Object State
  virtual bool IsWallMoved(int room_id) const = 0;
  virtual bool IsFloorBombable(int room_id) const = 0;
  // True once the room's rupee-floor reward has been collected. USDASM's
  // RoomDraw_RupeeFloor suppresses the object while this room flag is set.
  virtual bool IsRupeeFloorCleared(int room_id) const = 0;

  // General Flags
  virtual bool IsCrystalSwitchBlue() const = 0;
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_DUNGEON_STATE_H
