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
  kTags,
  kMechanics,
};

// A host-owned control placed in the core grid after the message field, for
// state that lives outside the room header (e.g. pit damage). `draw` renders
// one frame-height widget in the value cell.
struct RoomPropertyExtraCell {
  const char* label = nullptr;
  std::function<void()> draw;
};

// Both hosts edit through the viewer's validated, undoable metadata callback.
// Navigation and surrounding layout remain owned by each host.
void DrawDungeonRoomPropertyFields(
    DungeonCanvasViewer& viewer, zelda3::Room& room, int room_id,
    RoomPropertyLayout layout, const RoomPropertyExtraCell* extra = nullptr);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_ROOM_PROPERTY_CONTROLS_H_
