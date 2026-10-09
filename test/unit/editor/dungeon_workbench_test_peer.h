#ifndef YAZE_TEST_UNIT_EDITOR_DUNGEON_WORKBENCH_TEST_PEER_H_
#define YAZE_TEST_UNIT_EDITOR_DUNGEON_WORKBENCH_TEST_PEER_H_

#include "app/editor/dungeon/workspace/dungeon_workbench_content.h"

namespace yaze::editor {

class DungeonWorkbenchContentTestPeer {
 public:
  static void DrawNavigator(DungeonWorkbenchContent& content, float width,
                            float height) {
    content.DrawSidebarPane(width, height, 22, false);
  }
  static const auto& RebuildDungeonBadgeCache(
      DungeonWorkbenchContent& content) {
    content.BuildRoomDungeonCache();
    return content.room_dungeon_cache_;
  }

  static void DrawInspector(DungeonWorkbenchContent& content,
                            DungeonCanvasViewer& viewer, bool compact = false) {
    content.DrawInspectorShelf(viewer, compact);
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
