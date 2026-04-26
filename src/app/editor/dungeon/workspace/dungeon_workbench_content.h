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
class CustomCollisionPanel;
class MinecartTrackEditorPanel;
class RoomTagEditorPanel;
class WaterFillPanel;
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
      std::function<void(bool)> set_workflow_mode, Rom* rom = nullptr);
  ~DungeonWorkbenchContent() override;

  std::string GetId() const override;
  std::string GetDisplayName() const override;
  std::string GetIcon() const override;
  std::string GetEditorCategory() const override;
  int GetPriority() const override;

  void SetRom(Rom* rom);
  void SetEmbeddedToolPanels(RoomTagEditorPanel* room_tags,
                             CustomCollisionPanel* custom_collision,
                             WaterFillPanel* water_fill,
                             MinecartTrackEditorPanel* minecart_tracks);
  void SetEmbeddedEditorPanels(WindowContent* object_selector,
                               WindowContent* door_editor,
                               WindowContent* sprite_editor,
                               WindowContent* item_editor,
                               WindowContent* room_graphics,
                               WindowContent* palette_editor);
  void FocusRoomInspector();
  void FocusSelectionInspector();
  void FocusEntranceBrowser();
  void ShowConnectedGraph();
  void RequestDungeonMapPopup();
  void OpenObjectSelectorTool();
  void OpenDoorTool();
  void OpenSpriteTool();
  void OpenItemTool();
  void OpenRoomGraphicsTool();
  void OpenPaletteTool();
  void OpenRoomTagsTool();
  void OpenCustomCollisionTool();
  void OpenWaterFillTool();
  void OpenMinecartTool();

  // Mirror toggle: when true, the inspector renders on the LEFT and the
  // sidebar renders on the RIGHT (ZScream-style). Width semantics are
  // preserved (layout_state_.right_width is still the inspector's width),
  // so splitter drags still affect the correct pane regardless of side.
  void SetInspectorOnLeft(bool on_left) { inspector_on_left_ = on_left; }
  bool IsInspectorOnLeft() const { return inspector_on_left_; }
  // Wired by the editor to persist the user's mirror choice in UserSettings.
  // Called when the in-inspector swap-side button is clicked.
  void SetOnInspectorSideChanged(std::function<void(bool)> cb) {
    on_inspector_side_changed_ = std::move(cb);
  }

  // Lightweight state probes for unit tests; production rendering remains
  // driven by the Workbench inspector.
  bool IsToolDrawerActiveForTesting() const;
  const char* GetInspectorModeIdForTesting() const;
  const char* GetActiveToolIdForTesting() const;

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
  enum class WorkbenchTool : uint8_t {
    None,
    RoomTags,
    CustomCollision,
    WaterFill,
    MinecartTracks,
    ObjectSelector,
    DoorEditor,
    SpriteEditor,
    ItemEditor,
    RoomGraphics,
    Palette,
  };

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
  void DrawInspectorToolDrawer(DungeonCanvasViewer& viewer);
  void DrawWorkbenchTool(DungeonCanvasViewer& viewer, WorkbenchTool tool);
  void DrawInspectorToolStrip();
  void OpenTool(WorkbenchTool tool);
  bool IsWorkbenchToolAvailable(WorkbenchTool tool) const;
  const char* GetWorkbenchToolId(WorkbenchTool tool) const;
  const char* GetWorkbenchToolTitle(WorkbenchTool tool) const;
  const char* GetWorkbenchToolIcon(WorkbenchTool tool) const;
  const char* GetWorkbenchToolShortLabel(WorkbenchTool tool) const;
  const char* GetWorkbenchToolUnavailableMessage(WorkbenchTool tool) const;
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
  bool inspector_on_left_ = false;
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
  std::function<void(bool)> set_workflow_mode_;
  std::function<void(bool)> on_inspector_side_changed_;
  Rom* rom_ = nullptr;

  enum class SidebarMode : uint8_t { Rooms, Entrances };
  SidebarMode sidebar_mode_ = SidebarMode::Rooms;
  enum class InspectorMode : uint8_t { Room, Selection, Tools };
  InspectorMode inspector_mode_ = InspectorMode::Room;
  bool inspector_selection_was_active_ = false;
  WorkbenchTool active_tool_ = WorkbenchTool::ObjectSelector;

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
  RoomTagEditorPanel* room_tag_panel_ = nullptr;
  CustomCollisionPanel* custom_collision_panel_ = nullptr;
  WaterFillPanel* water_fill_panel_ = nullptr;
  MinecartTrackEditorPanel* minecart_track_panel_ = nullptr;
  WindowContent* object_selector_content_ = nullptr;
  WindowContent* door_editor_content_ = nullptr;
  WindowContent* sprite_editor_content_ = nullptr;
  WindowContent* item_editor_content_ = nullptr;
  WindowContent* room_graphics_content_ = nullptr;
  WindowContent* palette_editor_content_ = nullptr;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_WORKSPACE_DUNGEON_WORKBENCH_CONTENT_H
