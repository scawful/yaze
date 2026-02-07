#ifndef YAZE_APP_EDITOR_DUNGEON_WIDGETS_DUNGEON_ROOM_NAV_WIDGET_H
#define YAZE_APP_EDITOR_DUNGEON_WIDGETS_DUNGEON_ROOM_NAV_WIDGET_H

#include <functional>
#include <optional>

namespace yaze::editor {

// Small reusable widget that renders 4-way dungeon room navigation arrows.
// Intended for Workbench/toolbars (not tied to DungeonCanvasViewer).
class DungeonRoomNavWidget {
 public:
  struct Neighbors {
    std::optional<int> west;
    std::optional<int> north;
    std::optional<int> south;
    std::optional<int> east;
  };

  static Neighbors GetNeighbors(int room_id);

  // Draws a single-row navigation strip (W/N/S/E). Returns true if navigation
  // occurred (callback invoked).
  static bool Draw(const char* id, int room_id,
                   const std::function<void(int)>& on_navigate);
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_WIDGETS_DUNGEON_ROOM_NAV_WIDGET_H

