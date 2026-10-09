#ifndef YAZE_APP_EDITOR_DUNGEON_INSPECTORS_DUNGEON_CHEST_EDITOR_H_
#define YAZE_APP_EDITOR_DUNGEON_INSPECTORS_DUNGEON_CHEST_EDITOR_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace yaze::zelda3 {
class Room;
}

namespace yaze::project {
struct YazeProject;
}

namespace yaze::editor {

class DungeonCanvasViewer;

struct DungeonChestEditorState {
  int room_id = -1;
  int selected_index = 0;
  std::optional<size_t> last_canvas_selection;
  std::array<char, 64> search{};
  std::string error;
};

// Chest item IDs are receipt-table indices, not inventory slot values. Unknown
// bytes retain their numeric identity; project item_receipt labels override defaults.
std::string GetDungeonChestItemLabel(uint8_t item_id);
std::string GetDungeonChestItemLabel(uint8_t item_id,
                                     const project::YazeProject* project);

// Shared chest authoring controls. Placement uses the canvas tool, and structural
// edits keep the visual chest and its contents in one undoable operation.
void DrawDungeonChestEditor(int room_id, zelda3::Room& room,
                            DungeonCanvasViewer& viewer);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_INSPECTORS_DUNGEON_CHEST_EDITOR_H_
