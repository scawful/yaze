#ifndef YAZE_TEST_UNIT_EDITOR_DUNGEON_WORKBENCH_TEST_PEER_H_
#define YAZE_TEST_UNIT_EDITOR_DUNGEON_WORKBENCH_TEST_PEER_H_

#include "app/editor/dungeon/widgets/dungeon_workbench_toolbar.h"
#include "app/editor/dungeon/workspace/dungeon_workbench_content.h"

namespace yaze::editor {

class DungeonWorkbenchContentTestPeer {
 public:
  static void DrawToolsToolbar(DungeonWorkbenchContent& content) {
    DungeonWorkbenchToolbarParams params;
    params.layout = &content.layout_state_;
    params.current_room_id = content.current_room_id_;
    params.split_view_enabled = &content.split_view_enabled_;
    params.compare_room_id = &content.compare_room_id_;
    params.draw_tools = [&content]() {
      content.DrawInspectorToolPicker();
    };
    DungeonWorkbenchToolbar::Draw(params);
  }
  static void DrawCanvasPane(DungeonWorkbenchContent& content,
                             DungeonCanvasViewer& viewer, float width,
                             float height) {
    content.DrawCanvasPane(width, height, &viewer);
  }
  static void SetCompare(DungeonWorkbenchContent& content, bool enabled,
                         int room_id = 1) {
    content.split_view_enabled_ = enabled;
    content.compare_room_id_ = room_id;
    content.layout_state_.sync_split_view = false;
  }
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
  static void DrawInspectorPane(DungeonWorkbenchContent& content,
                                DungeonCanvasViewer& viewer, float width,
                                float height, bool compact = false) {
    content.DrawInspectorPane(width, height, 22, compact, &viewer);
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
