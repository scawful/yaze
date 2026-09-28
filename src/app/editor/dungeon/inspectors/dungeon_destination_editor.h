#ifndef YAZE_APP_EDITOR_DUNGEON_INSPECTORS_DUNGEON_DESTINATION_EDITOR_H_
#define YAZE_APP_EDITOR_DUNGEON_INSPECTORS_DUNGEON_DESTINATION_EDITOR_H_

namespace yaze::editor {
class DungeonCanvasViewer;

// Shared header-slot controls. Does not infer which placed stair consumes a slot.
void DrawDungeonDestinationEditor(DungeonCanvasViewer& viewer);

}  // namespace yaze::editor
#endif  // YAZE_APP_EDITOR_DUNGEON_INSPECTORS_DUNGEON_DESTINATION_EDITOR_H_
