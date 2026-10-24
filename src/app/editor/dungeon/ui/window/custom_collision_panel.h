#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_CUSTOM_COLLISION_PANEL_H
#define YAZE_APP_EDITOR_DUNGEON_PANELS_CUSTOM_COLLISION_PANEL_H

#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_object_interaction.h"
#include "app/editor/dungeon/dungeon_room_store.h"
#include "app/editor/system/workspace/editor_panel.h"
#include "app/gui/core/icons.h"
#include "zelda3/dungeon/custom_collision.h"

namespace yaze::editor {

class CustomCollisionPanel : public WindowContent {
 public:
  CustomCollisionPanel(DungeonCanvasViewer* viewer,
                       DungeonObjectInteraction* interaction)
      : viewer_(viewer), interaction_(interaction) {}

  std::string GetId() const override { return "dungeon.custom_collision"; }
  std::string GetWorkflowGroup() const override { return "Editors"; }
  std::string GetDisplayName() const override { return "Custom Collision"; }
  std::string GetIcon() const override { return ICON_MD_GRID_ON; }
  std::string GetEditorCategory() const override { return "Dungeon"; }

  void SetCanvasViewer(DungeonCanvasViewer* viewer) { viewer_ = viewer; }
  void SetInteraction(DungeonObjectInteraction* interaction) {
    interaction_ = interaction;
  }

  void Draw(bool* p_open) override;

  static absl::StatusOr<std::vector<zelda3::CustomCollisionRoomEntry>>
  CollectRoomEntries(const DungeonRoomStore& rooms);
  static void ApplyRoomEntry(const zelda3::CustomCollisionRoomEntry& entry,
                             zelda3::Room* room);

  absl::Status ImportCollisionRooms(
      const std::vector<zelda3::CustomCollisionRoomEntry>& entries,
      std::string_view source_label = "import");
  absl::Status ImportCollisionJson(std::string_view json_content,
                                   std::string_view source_label = "import");
  absl::Status ExportCollisionToFile(const DungeonRoomStore& rooms,
                                     const std::string& path);

  const std::string& last_io_status() const { return last_io_status_; }
  const std::string& last_io_error() const { return last_io_error_; }

 private:
  DungeonCanvasViewer* viewer_ = nullptr;
  DungeonObjectInteraction* interaction_ = nullptr;

  std::string last_io_status_;
  std::string last_io_error_;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_CUSTOM_COLLISION_PANEL_H
