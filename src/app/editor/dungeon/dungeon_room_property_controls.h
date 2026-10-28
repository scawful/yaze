#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_ROOM_PROPERTY_CONTROLS_H_
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_ROOM_PROPERTY_CONTROLS_H_

#include <functional>

namespace yaze::zelda3 {
class Room;
}

namespace yaze::editor {
class DungeonCanvasViewer;

enum class RoomPropertyLayout {
  kCompact,
  kFull,
  kCoreGraphics,
  kCoreGraphicsRow,
  kRoomDetails,
  kTags,
  kMechanics,
};

// Both hosts edit through the viewer's validated, undoable metadata callback.
// Navigation and surrounding layout remain owned by each host.
// kCoreGraphicsRow appends Message and the optional pit-damage checkbox. The
// callback draws only the value widget; this renderer owns its label and cell.
void DrawDungeonRoomPropertyFields(
    DungeonCanvasViewer& viewer, zelda3::Room& room, int room_id,
    RoomPropertyLayout layout,
    const std::function<void()>& draw_pit_damage = {});

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_ROOM_PROPERTY_CONTROLS_H_
