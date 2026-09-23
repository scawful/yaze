#ifndef YAZE_APP_EDITOR_DUNGEON_INSPECTORS_DUNGEON_CONNECTION_EDITOR_H_
#define YAZE_APP_EDITOR_DUNGEON_INSPECTORS_DUNGEON_CONNECTION_EDITOR_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace yaze::editor {

class DungeonCanvasViewer;
enum class DungeonConnectionLayer;

struct DungeonConnectionEditorState {
  int room_id = -1;
  size_t door_index = 0;
  const void* rom = nullptr;
  std::optional<std::array<uint8_t, 3>> source;
  DungeonConnectionLayer layer{};
  std::string error;
};

// Shared connection controls for the selected door. A preview is rebuilt from
// the current room data each frame; only the explicit action publishes it.
void DrawDungeonConnectionEditor(
    DungeonCanvasViewer& viewer, size_t door_index,
    const std::function<void(int, size_t)>& jump_to_reciprocal = {});

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_INSPECTORS_DUNGEON_CONNECTION_EDITOR_H_
