#ifndef YAZE_APP_EDITOR_DUNGEON_WORKSPACE_DUNGEON_WORKBENCH_CONTENT_H
#define YAZE_APP_EDITOR_DUNGEON_WORKSPACE_DUNGEON_WORKBENCH_CONTENT_H

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "app/editor/dungeon/dungeon_workbench_state.h"
#include "app/editor/system/editor_panel.h"

namespace yaze {
class Rom;
}  // namespace yaze

namespace yaze::editor {

class DungeonCanvasViewer;
class DungeonMapPanel;
class DungeonRoomSelector;
enum class RoomSelectionIntent;

struct DungeonWorkbenchResponsiveLayout {
  bool show_left = false;
  bool show_right = false;
  bool compact_left = false;
  bool compact_right = false;
};

struct DungeonWorkbenchPaneLayout {
  DungeonWorkbenchResponsiveLayout responsive{};
  float left_width = 0.0f;
  float center_width = 0.0f;
  float right_width = 0.0f;
  float min_left_width = 0.0f;
  float min_right_width = 0.0f;
};

DungeonWorkbenchResponsiveLayout ResolveDungeonWorkbenchResponsiveLayout(
    float total_width, float min_canvas_width, float min_sidebar_width,
    float splitter_width, bool want_left, bool want_right);

DungeonWorkbenchPaneLayout ResolveDungeonWorkbenchPaneLayout(
    float total_width, float min_canvas_width, float min_sidebar_width,
    float splitter_width, float stored_left_width, float stored_right_width,
    bool want_left, bool want_right);

// Single stable window for dungeon editing. This is step 2 in the Workbench plan.
class DungeonWorkbenchContent : public WindowContent {
 public:
  DungeonWorkbenchContent(
      DungeonRoomSelector* room_selector, int* current_room_id,
      std::function<void(int)> on_room_selected,
      std::function<void(int, RoomSelectionIntent)>
          on_room_selected_with_intent,
      std::function<void(int)> on_save_room,
      std::function<void()> on_save_all_rooms,
      std::function<DungeonCanvasViewer*()> get_viewer,
      std::function<DungeonCanvasViewer*()> get_compare_viewer,
      std::function<const std::deque<int>&()> get_recent_rooms,
      std::function<void(int)> forget_recent_room,
      std::function<void(const std::string&)> show_panel,
      std::function<void(bool)> set_workflow_mode, Rom* rom = nullptr);
  ~DungeonWorkbenchContent() override;

  std::string GetId() const override;
  std::string GetDisplayName() const override;
  std::string GetIcon() const override;
  std::string GetEditorCategory() const override;
  int GetPriority() const override;

  void SetRom(Rom* rom);
  void FocusRoomInspector();
  void FocusSelectionInspector();
  void RequestDungeonMapPopup();

  /// Called by the editor when the current room changes.
  void NotifyRoomChanged(int previous_room_id) {
    previous_room_id_ = previous_room_id;
  }

  // Wire undo/redo state from the editor's UndoManager.
  void SetUndoRedoProvider(std::function<bool()> can_undo,
                           std::function<bool()> can_redo,
                           std::function<void()> on_undo,
                           std::function<void()> on_redo,
                           std::function<std::string()> undo_desc,
                           std::function<std::string()> redo_desc,
                           std::function<int()> undo_depth) {
    can_undo_ = std::move(can_undo);
    can_redo_ = std::move(can_redo);
    on_undo_ = std::move(on_undo);
    on_redo_ = std::move(on_redo);
    undo_desc_ = std::move(undo_desc);
    redo_desc_ = std::move(redo_desc);
    undo_depth_ = std::move(undo_depth);
  }

  // Wire tool mode name provider (e.g., from DungeonToolset::GetToolModeName).
  void SetToolModeProvider(std::function<const char*()> provider) {
    get_tool_mode_ = std::move(provider);
  }

  void Draw(bool* p_open) override;

 private:
  void DrawRecentRoomTabs();
  void DrawSidebarPane(float width, float height, float button_size,
                       bool compact);
  void DrawSidebarHeader(float button_size, bool compact);
  void DrawSidebarModeTabs(bool stacked, float segment_height);
  void DrawSidebarContent();
  void DrawCanvasPane(float width, float height,
                      DungeonCanvasViewer* primary_viewer,
                      bool left_sidebar_visible);
  void DrawSelectionShelf(DungeonCanvasViewer& viewer);
  void DrawSplitView(DungeonCanvasViewer& primary_viewer);
  void DrawInspectorPane(float width, float height, float button_size,
                         bool compact, DungeonCanvasViewer* viewer);
  void DrawInspectorHeader(float button_size, bool compact);
  void DrawInspector(DungeonCanvasViewer& viewer, bool compact);
  void DrawInspectorPrimarySelector(float segment_height);
  void DrawInspectorCompactSummary(DungeonCanvasViewer& viewer);
  void DrawInspectorShelf(DungeonCanvasViewer& viewer, bool compact);
  void DrawInspectorShelfRoom(DungeonCanvasViewer& viewer);
  void DrawInspectorShelfSelection(DungeonCanvasViewer& viewer);
  void DrawInspectorShelfView(DungeonCanvasViewer& viewer);
  void DrawInspectorShelfTools(DungeonCanvasViewer& viewer);
  void DrawApplyScopeControls(int room_id);
  void DrawLayerCompositingControls(DungeonCanvasViewer& viewer, int room_id);
  void DrawDungeonMapPopup(DungeonCanvasViewer& viewer);
  DungeonMapPanel* GetEmbeddedDungeonMap(DungeonCanvasViewer& viewer);
  void SetAllSaveFlags(bool value);

  // Lazily build a room_id → dungeon_name cache from ROM entrance tables so the
  // room badge can show accurate group context for custom Oracle dungeons.
  void BuildRoomDungeonCache();

  DungeonRoomSelector* room_selector_ = nullptr;
  int* current_room_id_ = nullptr;

  // Workbench-owned UI state (was in DungeonEditorV2, moved here Phase 6.2).
  int previous_room_id_ = -1;
  bool split_view_enabled_ = false;
  int compare_room_id_ = -1;
  DungeonWorkbenchLayoutState layout_state_{};
  std::function<void(int)> on_room_selected_;
  std::function<void(int, RoomSelectionIntent)> on_room_selected_with_intent_;
  std::function<void(int)> on_save_room_;
  std::function<void()> on_save_all_rooms_;
  std::function<DungeonCanvasViewer*()> get_viewer_;
  std::function<DungeonCanvasViewer*()> get_compare_viewer_;
  std::function<const std::deque<int>&()> get_recent_rooms_;
  std::function<void(int)> forget_recent_room_;
  std::function<void(const std::string&)> show_panel_;
  std::function<void(bool)> set_workflow_mode_;
  Rom* rom_ = nullptr;

  enum class SidebarMode : uint8_t { Rooms, Entrances };
  SidebarMode sidebar_mode_ = SidebarMode::Rooms;
  enum class InspectorFocus : uint8_t { Room, Selection };
  InspectorFocus inspector_focus_ = InspectorFocus::Room;
  bool inspector_selection_was_active_ = false;

  char compare_search_buf_[64] = {};

  // ROM-based room→dungeon group label cache (lazy-built on first room render).
  std::unordered_map<int, std::string> room_dungeon_cache_;
  bool room_dungeon_cache_built_ = false;

  // Undo/redo providers (set via SetUndoRedoProvider).
  std::function<bool()> can_undo_;
  std::function<bool()> can_redo_;
  std::function<void()> on_undo_;
  std::function<void()> on_redo_;
  std::function<std::string()> undo_desc_;
  std::function<std::string()> redo_desc_;
  std::function<int()> undo_depth_;

  // Tool mode name provider (set via SetToolModeProvider).
  std::function<const char*()> get_tool_mode_;

  // Shortcut legend toggle.
  bool show_shortcut_legend_ = false;
  bool open_dungeon_map_popup_ = false;
  std::unique_ptr<DungeonMapPanel> embedded_dungeon_map_;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_WORKSPACE_DUNGEON_WORKBENCH_CONTENT_H
