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

// A successful load invalidates any preview but does not apply the document.
// A failed load keeps the prior document/preview available for recovery.
void LoadDungeonRoomTransferFile(DungeonRoomTransferEditorState& state,
                                 const std::string& path);

void DrawDungeonRoomTransferEditor(DungeonCanvasViewer& viewer);
void DrawDungeonRoomTransferPopup(DungeonCanvasViewer& viewer);
// Opens the shared Clone / Import dialog for the viewer's current room on its
// next draw. A valid, different `source_room_id` preselects Clone from it.
void RequestDungeonRoomTransferPopup(DungeonCanvasViewer& viewer,
                                     int source_room_id = -1);

// Shared by clone/import and swap. Returns true when a choice changed;
// clearing room properties also clears `copy_destinations`.
bool DrawDungeonRoomTransferDomainChoices(uint16_t* domains,
                                          bool* copy_destinations);
void DrawDungeonRoomTransferCounts(const DungeonRoomTransferPlan& plan);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_INSPECTORS_DUNGEON_ROOM_TRANSFER_EDITOR_H_
