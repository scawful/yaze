#ifndef YAZE_APP_EDITOR_DUNGEON_INSPECTORS_DUNGEON_ROOM_TRANSFER_EDITOR_H_
#define YAZE_APP_EDITOR_DUNGEON_INSPECTORS_DUNGEON_ROOM_TRANSFER_EDITOR_H_

#include <cstdint>
#include <memory>
#include <string>

namespace yaze::editor {

class DungeonCanvasViewer;
struct DungeonRoomTransferPlan;

struct DungeonRoomTransferEditorState {
  int room_id = -1;
  const void* rom = nullptr;
  int source_room_id = 0;
  bool import_json = false;
  uint16_t domains = 31;
  bool copy_destinations = false;
  std::string json;
  std::shared_ptr<DungeonRoomTransferPlan> preview;
  std::string error;
  bool can_retry_without_properties = false;
  std::string status;
  bool request_popup = false;
  bool popup_open = false;
  int popup_room_id = -1;
};

void DrawDungeonRoomTransferEditor(DungeonCanvasViewer& viewer);
void DrawDungeonRoomTransferPopup(DungeonCanvasViewer& viewer);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_INSPECTORS_DUNGEON_ROOM_TRANSFER_EDITOR_H_
