#ifndef YAZE_APP_ZELDA3_DUNGEON_ROOM_DIAGNOSTIC_H
#define YAZE_APP_ZELDA3_DUNGEON_ROOM_DIAGNOSTIC_H

namespace yaze {
namespace zelda3 {

class Room;

// Comprehensive diagnostic function to trace room rendering pipeline
void DiagnoseRoomRendering(Room& room, int room_id);

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_DUNGEON_ROOM_DIAGNOSTIC_H

