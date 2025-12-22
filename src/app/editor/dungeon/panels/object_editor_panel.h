#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_OBJECT_EDITOR_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_PANELS_OBJECT_EDITOR_PANEL_H_

#include <memory>
#include <unordered_map>

#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_object_selector.h"
#include "app/editor/editor.h"
#include "app/editor/system/editor_panel.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gui/app/editor_layout.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/icons.h"
#include "app/gui/widgets/dungeon_object_emulator_preview.h"
#include "rom/rom.h"
#include "zelda3/dungeon/door_types.h"
#include "zelda3/dungeon/dungeon_object_editor.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/object_parser.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace editor {

/**
 * @class ObjectEditorPanel
 * @brief Unified panel for dungeon object editing
 *
 * This panel combines object selection, emulator preview, and canvas
 * interaction into a single EditorPanel component. It provides a complete
 * workflow for managing dungeon objects.
 *
 * Features:
 * - Object browser with graphical previews
 * - Static object editor (opened via double-click)
 * - Emulator-based preview rendering
 * - Object templates for common patterns
 * - Unified canvas context menu integration (Cut/Copy/Paste/Duplicate/Delete)
 * - Keyboard shortcuts for efficient editing
 *
 * @see EditorPanel - Base interface
 * @see DungeonObjectSelector - Object browser component
 * @see DungeonObjectEmulatorPreview - Preview component
 */
class ObjectEditorPanel : public EditorPanel {
 public:
  ObjectEditorPanel(
      gfx::IRenderer* renderer, Rom* rom, DungeonCanvasViewer* canvas_viewer,
      std::shared_ptr<zelda3::DungeonObjectEditor> object_editor = nullptr);

  // ==========================================================================
  // EditorPanel Identity
  // ==========================================================================

  std::string GetId() const override { return "dungeon.object_editor"; }
  std::string GetDisplayName() const override { return "Object Editor"; }
  std::string GetIcon() const override { return ICON_MD_CONSTRUCTION; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  int GetPriority() const override { return 60; }

  // ==========================================================================
  // EditorPanel Drawing
  // ==========================================================================

  void Draw(bool* p_open) override;
  void OnOpen() override {}
  void OnClose() override {}

  // ==========================================================================
  // Component Accessors
  // ==========================================================================

  DungeonObjectSelector& object_selector() { return object_selector_; }
  gui::DungeonObjectEmulatorPreview& emulator_preview() {
    return emulator_preview_;
  }

  // ==========================================================================
  // Context Management
  // ==========================================================================

  void SetCurrentRoom(int room_id) {
    current_room_id_ = room_id;
    object_selector_.set_current_room_id(room_id);
  }
  void SetCanvasViewer(DungeonCanvasViewer* viewer) {
    // Reset callback flag when viewer changes so we rewire to the new viewer
    if (canvas_viewer_ != viewer) {
      selection_callbacks_setup_ = false;
    }
    canvas_viewer_ = viewer;
    SetupSelectionCallbacks();
  }

  void SetContext(EditorContext ctx) {
    object_selector_.SetContext(ctx);
    emulator_preview_.SetGameData(ctx.game_data);
    rom_ = ctx.rom;
  }

  void SetGameData(zelda3::GameData* game_data) {
    object_selector_.SetGameData(game_data);
    emulator_preview_.SetGameData(game_data);
  }

  void SetRooms(std::array<zelda3::Room, 0x128>* rooms) {
    object_selector_.set_rooms(rooms);
  }

  void SetCurrentPaletteGroup(const gfx::PaletteGroup& group) {
    object_selector_.SetCurrentPaletteGroup(group);
  }

  // ==========================================================================
  // Programmatic Controls (for agents/automation)
  // ==========================================================================

  void SelectObject(int obj_id);
  void SetAgentOptimizedLayout(bool enabled);
  void SetupSelectionCallbacks();

  // Object operations
  void CycleObjectSelection(int direction);
  void SelectAllObjects();
  void DeleteSelectedObjects();
  void CopySelectedObjects();
  void PasteObjects();
  void CancelPlacement();  // Cancel current object placement

  // ==========================================================================
  // Static Object Editor (double-click to open)
  // ==========================================================================

  void OpenStaticObjectEditor(int object_id);
  void CloseStaticObjectEditor();
  bool IsStaticEditorOpen() const { return static_editor_open_; }
  int GetStaticEditorObjectId() const { return static_editor_object_id_; }

 private:
  // Selection change handler
  void OnSelectionChanged();

  // Drawing methods
  void DrawObjectSelector();
  void DrawDoorSection();
  void DrawEmulatorPreview();
  void DrawSelectedObjectInfo();
  void DrawStaticObjectEditor();

  // Keyboard shortcuts
  void HandleKeyboardShortcuts();
  void DeselectAllObjects();
  void PerformDelete();
  void DuplicateSelectedObjects();
  void NudgeSelectedObjects(int dx, int dy);
  void ScrollToObject(size_t index);

  // ==========================================================================
  // Member Variables
  // ==========================================================================

  Rom* rom_ = nullptr;
  DungeonCanvasViewer* canvas_viewer_ = nullptr;
  int current_room_id_ = 0;

  // Components
  DungeonObjectSelector object_selector_;
  gui::DungeonObjectEmulatorPreview emulator_preview_;

  // Object preview canvases (one per object type)
  std::unordered_map<int, gui::Canvas> object_preview_canvases_;

  // UI state
  int selected_tab_ = 0;
  bool show_emulator_preview_ = false;
  bool show_object_list_ = true;
  bool show_interaction_controls_ = true;
  bool show_grid_ = true;
  bool show_object_ids_ = false;
  bool show_template_creation_modal_ = false;
  bool show_delete_confirmation_modal_ = false;

  // Selected object for placement
  zelda3::RoomObject preview_object_{0, 0, 0, 0, 0};
  bool has_preview_object_ = false;
  gfx::IRenderer* renderer_;
  std::shared_ptr<zelda3::DungeonObjectEditor> object_editor_;

  // Selection state cache (updated via callback)
  size_t cached_selection_count_ = 0;
  bool selection_callbacks_setup_ = false;

  // Static object editor state (opened via double-click)
  bool static_editor_open_ = false;
  int static_editor_object_id_ = -1;
  gfx::Bitmap static_preview_bitmap_;
  gui::Canvas static_preview_canvas_{"##StaticObjectPreview", ImVec2(128, 128)};
  zelda3::ObjectDrawInfo static_editor_draw_info_;
  std::unique_ptr<zelda3::ObjectParser> object_parser_;
  gfx::BackgroundBuffer static_preview_buffer_{128, 128};
  bool static_preview_rendered_ = false;

  // Door placement state
  zelda3::DoorType selected_door_type_ = zelda3::DoorType::NormalDoor;
  bool door_placement_mode_ = false;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_OBJECT_EDITOR_PANEL_H_
