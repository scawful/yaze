#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_ROOM_PROPERTY_CONTROLS_H_
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_ROOM_PROPERTY_CONTROLS_H_

namespace yaze::zelda3 {
class Room;
}

namespace yaze::editor {
class DungeonCanvasViewer;

enum class RoomPropertyLayout {
  kCompact,
  kFull,
  kCoreGraphics,
  kTags,
  kMechanics,
};

// Both hosts edit through the viewer's validated, undoable metadata callback.
// Navigation and surrounding layout remain owned by each host.
void DrawDungeonRoomPropertyFields(DungeonCanvasViewer& viewer,
                                   zelda3::Room& room, int room_id,
                                   RoomPropertyLayout layout);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_ROOM_PROPERTY_CONTROLS_H_
