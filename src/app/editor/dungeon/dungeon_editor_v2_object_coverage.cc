// Object Coverage wiring for DungeonEditorV2: creating the panel, opening a
// room on a placed object, and the canvas "Check in Object Coverage" action.

#include <algorithm>
#include <memory>

#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/dungeon/ui/window/object_coverage_panel.h"
#include "app/editor/dungeon/workspace/dungeon_workbench_content.h"

namespace yaze::editor {

std::unique_ptr<ObjectCoveragePanel>
DungeonEditorV2::CreateObjectCoveragePanel() {
  auto panel = std::make_unique<ObjectCoveragePanel>();
  panel->SetProject(dependencies_.project);
  panel->SetRooms(&rooms_);
  panel->SetNavigateCallback(
      [this](int room_id, size_t object_index, int object_id) {
        NavigateToPlacedObject(room_id, object_index, object_id);
      });
  return panel;
}

void DungeonEditorV2::FocusObjectCoverage(int room_id,
                                          const zelda3::RoomObject& object) {
  if (object_coverage_panel_ == nullptr) {
    return;
  }
  object_coverage_panel_->FocusObject(object.id_, room_id);
  if (IsWorkbenchWorkflowEnabled() && workbench_panel_) {
    workbench_panel_->OpenObjectCoverageTool();
    OpenWindow("dungeon.workbench");
    return;
  }
  OpenWindow("dungeon.object_coverage");
}

void DungeonEditorV2::NavigateToPlacedObject(int room_id, size_t object_index,
                                             int object_id) {
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size())) {
    return;
  }
  OnRoomSelected(room_id, /*request_focus=*/true);
  auto* viewer = GetViewerForRoom(room_id);
  auto* room = rooms_.GetIfMaterialized(room_id);
  if (viewer == nullptr || room == nullptr) {
    return;
  }
  const auto& objects = room->GetTileObjects();
  size_t target = object_index;
  if (target >= objects.size() || objects[target].id_ != object_id) {
    auto it = std::find_if(
        objects.begin(), objects.end(),
        [object_id](const auto& object) { return object.id_ == object_id; });
    if (it == objects.end()) {
      return;
    }
    target = static_cast<size_t>(it - objects.begin());
  }
  viewer->object_interaction().SetSelectedObjects({target});
  viewer->ScrollToTile(objects[target].x(), objects[target].y());
}

}  // namespace yaze::editor
