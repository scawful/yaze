#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_ROOM_TAG_EDITOR_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_PANELS_ROOM_TAG_EDITOR_PANEL_H_

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "core/project.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace editor {

/**
 * @class RoomTagEditorPanel
 * @brief EditorPanel showing all room tag slots and their usage across rooms
 *
 * Displays a table of tag slots (0x00 through ~0x3A) with columns for:
 * - Slot index (hex)
 * - Vanilla name from resource labels
 * - ASM label from hack manifest (if available)
 * - Feature flag status (enabled/disabled)
 * - Status indicator (Active/Disabled/Available/Vanilla)
 * - Count of rooms using this tag
 *
 * Also provides quick-assign combo boxes for the current room's Tag1 and Tag2.
 *
 * @see EditorPanel - Base interface
 * @see HackManifest - ASM tag metadata source
 */
class RoomTagEditorPanel : public EditorPanel {
 public:
  RoomTagEditorPanel() = default;

  // ==========================================================================
  // EditorPanel Identity
  // ==========================================================================

  std::string GetId() const override { return "dungeon.room_tags"; }
  std::string GetDisplayName() const override { return "Room Tags"; }
  std::string GetIcon() const override { return ICON_MD_LABEL; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  int GetPriority() const override { return 45; }

  // ==========================================================================
  // EditorPanel Drawing
  // ==========================================================================

  void Draw(bool* p_open) override;

  // ==========================================================================
  // Dependencies
  // ==========================================================================

  void SetProject(project::YazeProject* project) { project_ = project; }
  void SetRooms(std::array<zelda3::Room, 0x128>* rooms) {
    rooms_ = rooms;
    cache_dirty_ = true;
  }
  void SetCurrentRoomId(int room_id) { current_room_id_ = room_id; }

 private:
  void DrawTagTable();
  void DrawQuickAssign();
  void RebuildRoomCountCache();

  project::YazeProject* project_ = nullptr;
  std::array<zelda3::Room, 0x128>* rooms_ = nullptr;
  int current_room_id_ = -1;

  // Cache: tag_index -> count of rooms using it
  std::unordered_map<int, int> tag_usage_count_;
  bool cache_dirty_ = true;

  // Filter
  char filter_text_[128] = {};
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_ROOM_TAG_EDITOR_PANEL_H_
