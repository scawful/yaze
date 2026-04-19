#ifndef YAZE_APP_EDITOR_DUNGEON_INSPECTORS_DOOR_EDITOR_CONTENT_H_
#define YAZE_APP_EDITOR_DUNGEON_INSPECTORS_DOOR_EDITOR_CONTENT_H_

#include <functional>
#include <string>

#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_room_store.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "zelda3/dungeon/door_types.h"

namespace yaze::editor {

class DoorEditorContent : public WindowContent {
 public:
  std::string GetId() const override { return "dungeon.door_editor"; }
  std::string GetDisplayName() const override { return "Door Editor"; }
  std::string GetIcon() const override { return ICON_MD_DOOR_FRONT; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  int GetPriority() const override { return 68; }
  float GetPreferredWidth() const override { return 420.0f; }

  void Draw(bool* p_open) override;
  void OnClose() override;

  void SetCurrentRoom(int room_id) { current_room_id_ = room_id; }
  void SetCanvasViewerProvider(std::function<DungeonCanvasViewer*()> provider) {
    canvas_viewer_provider_ = std::move(provider);
  }
  void SetCanvasViewer(DungeonCanvasViewer* viewer) { canvas_viewer_ = viewer; }
  void SetRooms(DungeonRoomStore* rooms) { rooms_ = rooms; }

 private:
  DungeonCanvasViewer* ResolveCanvasViewer();
  void CancelPlacement();

  DungeonCanvasViewer* canvas_viewer_ = nullptr;
  std::function<DungeonCanvasViewer*()> canvas_viewer_provider_;
  DungeonRoomStore* rooms_ = nullptr;
  int current_room_id_ = 0;
  zelda3::DoorType selected_door_type_ = zelda3::DoorType::NormalDoor;
  bool door_placement_mode_ = false;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_INSPECTORS_DOOR_EDITOR_CONTENT_H_
