#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_WORKBENCH_PANEL_H
#define YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_WORKBENCH_PANEL_H

#include <deque>
#include <functional>
#include <string>

#include "app/editor/system/editor_panel.h"

namespace yaze {
class Rom;
}  // namespace yaze

namespace yaze::editor {

class DungeonCanvasViewer;
class DungeonRoomSelector;

// Single stable window for dungeon editing. This is step 2 in the Workbench plan.
class DungeonWorkbenchPanel : public EditorPanel {
 public:
  DungeonWorkbenchPanel(DungeonRoomSelector* room_selector,
                        int* current_room_id,
                        int* previous_room_id,
                        bool* split_view_enabled,
                        int* compare_room_id,
                        std::function<void(int)> on_room_selected,
                        std::function<DungeonCanvasViewer*()> get_viewer,
                        std::function<DungeonCanvasViewer*()> get_compare_viewer,
                        std::function<const std::deque<int>&()> get_recent_rooms,
                        std::function<void(int)> forget_recent_room,
                        std::function<void(const std::string&)> show_panel,
                        Rom* rom = nullptr);

  std::string GetId() const override;
  std::string GetDisplayName() const override;
  std::string GetIcon() const override;
  std::string GetEditorCategory() const override;
  int GetPriority() const override;

  void SetRom(Rom* rom);

  void Draw(bool* p_open) override;

 private:
  void DrawRecentRoomTabs();
  void DrawSplitView(DungeonCanvasViewer& primary_viewer);
  void DrawCompareHeader();
  void DrawInspector(DungeonCanvasViewer& viewer);

  DungeonRoomSelector* room_selector_ = nullptr;
  int* current_room_id_ = nullptr;
  int* previous_room_id_ = nullptr;
  bool* split_view_enabled_ = nullptr;
  int* compare_room_id_ = nullptr;
  std::function<void(int)> on_room_selected_;
  std::function<DungeonCanvasViewer*()> get_viewer_;
  std::function<DungeonCanvasViewer*()> get_compare_viewer_;
  std::function<const std::deque<int>&()> get_recent_rooms_;
  std::function<void(int)> forget_recent_room_;
  std::function<void(const std::string&)> show_panel_;
  Rom* rom_ = nullptr;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_WORKBENCH_PANEL_H
