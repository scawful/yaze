#ifndef YAZE_APP_EDITOR_DUNGEON_INSPECTORS_DUNGEON_ENTITY_INSPECTOR_H_
#define YAZE_APP_EDITOR_DUNGEON_INSPECTORS_DUNGEON_ENTITY_INSPECTOR_H_

#include <cstddef>
#include <functional>

namespace yaze::editor {

class DungeonCanvasViewer;

// Shared property controls for a single selected door, sprite, or pot item.
// All writes go through the viewer's interaction handlers and mutation hooks.
void DrawDungeonEntityInspector(
    DungeonCanvasViewer& viewer,
    const std::function<void(int, size_t)>& jump_to_reciprocal = {});

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_INSPECTORS_DUNGEON_ENTITY_INSPECTOR_H_
