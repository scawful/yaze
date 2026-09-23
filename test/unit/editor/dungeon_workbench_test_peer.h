#ifndef YAZE_TEST_UNIT_EDITOR_DUNGEON_WORKBENCH_TEST_PEER_H_
#define YAZE_TEST_UNIT_EDITOR_DUNGEON_WORKBENCH_TEST_PEER_H_

#include "app/editor/dungeon/workspace/dungeon_workbench_content.h"

namespace yaze::editor {

class DungeonWorkbenchContentTestPeer {
 public:
  static void DrawInspector(DungeonWorkbenchContent& content,
                            DungeonCanvasViewer& viewer) {
    content.DrawInspectorShelf(viewer, false);
  }
  static void DrawRoomInspector(DungeonWorkbenchContent& content,
                                DungeonCanvasViewer& viewer) {
    content.DrawInspectorShelfRoom(viewer);
  }
  static void DrawSelectedObjectActions(DungeonWorkbenchContent& content,
                                        DungeonCanvasViewer& viewer,
                                        size_t index) {
    content.DrawSelectedObjectActions(viewer, index);
  }
};

}  // namespace yaze::editor

#endif  // YAZE_TEST_UNIT_EDITOR_DUNGEON_WORKBENCH_TEST_PEER_H_
