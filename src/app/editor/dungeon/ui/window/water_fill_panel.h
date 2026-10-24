#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_WATER_FILL_PANEL_H
#define YAZE_APP_EDITOR_DUNGEON_PANELS_WATER_FILL_PANEL_H

#include <string>
#include <vector>

#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_object_interaction.h"
#include "app/editor/dungeon/dungeon_room_store.h"
#include "app/editor/system/workspace/editor_panel.h"
#include "app/gui/core/icons.h"
#include "zelda3/dungeon/water_fill_zone.h"

namespace yaze::editor {

class WaterFillPanel : public WindowContent {
 public:
  WaterFillPanel(DungeonCanvasViewer* viewer,
                 DungeonObjectInteraction* interaction)
      : viewer_(viewer), interaction_(interaction) {}

  std::string GetId() const override { return "dungeon.water_fill"; }
  std::string GetWorkflowGroup() const override { return "Editors"; }
  std::string GetDisplayName() const override { return "Water Fill"; }
  std::string GetIcon() const override { return ICON_MD_WATER_DROP; }
  std::string GetEditorCategory() const override { return "Dungeon"; }

  void SetCanvasViewer(DungeonCanvasViewer* viewer) { viewer_ = viewer; }
  void SetInteraction(DungeonObjectInteraction* interaction) {
    interaction_ = interaction;
  }

  void Draw(bool* p_open) override;

  static std::vector<zelda3::WaterFillZoneEntry> CollectZones(
      const DungeonRoomStore& rooms);
  static void ApplyZoneToRoom(const zelda3::WaterFillZoneEntry& z,
                              zelda3::Room* room);

 private:
  const InteractionContext* RoomEditContext(int room_id) const {
    if (!viewer_ || !interaction_ || !viewer_->rooms())
      return nullptr;
    const auto* context =
        interaction_->entity_coordinator().tile_handler().context();
    if (!context || context->rooms != viewer_->rooms() ||
        context->rom != viewer_->rom() || context->current_room_id != room_id ||
        !context->on_mutation || !context->on_invalidate_cache) {
      return nullptr;
    }
    return context;
  }

  template <typename Edit>
  void EditRoomWaterFill(int room_id, Edit edit) {
    const auto* context = RoomEditContext(room_id);
    if (!context)
      return;
    // A panel command is separate from any pending canvas paint stroke.
    interaction_->HandleMouseRelease();
    context->NotifyMutation(MutationDomain::kWaterFill);
    edit((*viewer_->rooms())[room_id]);
    context->NotifyInvalidateCache(MutationDomain::kWaterFill);
  }

  std::string last_io_status_;
  std::string last_io_error_;

  DungeonCanvasViewer* viewer_ = nullptr;
  DungeonObjectInteraction* interaction_ = nullptr;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_WATER_FILL_PANEL_H
