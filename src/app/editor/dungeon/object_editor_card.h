#ifndef YAZE_APP_EDITOR_DUNGEON_OBJECT_EDITOR_CARD_H
#define YAZE_APP_EDITOR_DUNGEON_OBJECT_EDITOR_CARD_H

#include <memory>
#include <unordered_map>

#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/editor.h"
#include "app/editor/dungeon/dungeon_object_selector.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gui/app/editor_layout.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/widgets/dungeon_object_emulator_preview.h"
#include "rom/rom.h"
#include "zelda3/dungeon/room_object.h"

#include "zelda3/dungeon/dungeon_object_editor.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace editor {

/**
 * @brief Unified card combining object selection, emulator preview, and canvas
 * interaction
 *
 * This card replaces three separate components:
 * - Object Selector (choosing which object to place)
 * - Emulator Preview (seeing how objects look in-game)
 * - Object Interaction Controls (placing, selecting, deleting objects)
 *
 * It provides a complete workflow for managing dungeon objects in one place.
 */
class ObjectEditorCard {
 public:
  ObjectEditorCard(
      gfx::IRenderer* renderer, Rom* rom, DungeonCanvasViewer* canvas_viewer,
      std::shared_ptr<zelda3::DungeonObjectEditor> object_editor = nullptr);

  // Main update function
  void Draw(bool* p_open);

  // Access to components
  DungeonObjectSelector& object_selector() { return object_selector_; }
  gui::DungeonObjectEmulatorPreview& emulator_preview() {
    return emulator_preview_;
  }

  // Update current room context
  void SetCurrentRoom(int room_id) { current_room_id_ = room_id; }
  void SetCanvasViewer(DungeonCanvasViewer* viewer) { canvas_viewer_ = viewer; }

  // Unified context setter (preferred)
  void SetContext(EditorContext ctx) {
    object_selector_.SetContext(ctx);
    emulator_preview_.SetGameData(ctx.game_data);
    // Store locally if needed
    rom_ = ctx.rom;
  }

  // GameData propagation (legacy - use SetContext)
  void SetGameData(zelda3::GameData* game_data) {
    object_selector_.SetGameData(game_data);
    emulator_preview_.SetGameData(game_data);
  }

  // Programmatic controls for agents/automation
  void SelectObject(int obj_id);
  void SetAgentOptimizedLayout(bool enabled);

  // Selection callback setup - wires ObjectSelection changes to UI updates
  void SetupSelectionCallbacks();

  // Keyboard shortcuts and interaction modes (public for ShortcutManager)
  void SetInteractionModeSelect() { interaction_mode_ = InteractionMode::Select; }
  void SetInteractionModePlace() { interaction_mode_ = InteractionMode::Place; }
  void SetInteractionModeDelete() { interaction_mode_ = InteractionMode::Delete; }
  void CycleObjectSelection(int direction);
  void SelectAllObjects();
  void DeleteSelectedObjects();
  void CopySelectedObjects();
  void PasteObjects();

 private:
  // Selection change handler
  void OnSelectionChanged();
  void DrawObjectSelector();
  void DrawObjectTemplates();
  void DrawTemplateCreationModal();
  void DrawDeleteConfirmationModal();
  void DrawEmulatorPreview();
  void DrawInteractionControls();
  void DrawSelectedObjectInfo();

  // Keyboard shortcuts
  void HandleKeyboardShortcuts();
  void DeselectAllObjects();
  void PerformDelete(); // Helper for actual deletion
  void DuplicateSelectedObjects();
  void NudgeSelectedObjects(int dx, int dy);
  void ScrollToObject(size_t index);

  Rom* rom_;
  DungeonCanvasViewer* canvas_viewer_;
  int current_room_id_ = 0;

  // Components
  DungeonObjectSelector object_selector_;
  gui::DungeonObjectEmulatorPreview emulator_preview_;

  // Object preview canvases (one per object type)
  std::unordered_map<int, gui::Canvas> object_preview_canvases_;

  // UI state
  int selected_tab_ = 0;
  bool show_emulator_preview_ = false;  // Disabled by default for performance
  bool show_object_list_ = true;
  bool show_interaction_controls_ = true;
  bool show_grid_ = true;
  bool show_object_ids_ = false;
  bool show_template_creation_modal_ = false;
  bool show_delete_confirmation_modal_ = false;

  // Object interaction mode
  enum class InteractionMode { None, Place, Select, Delete };
  InteractionMode interaction_mode_ = InteractionMode::None;

  // Selected object for placement
  zelda3::RoomObject preview_object_{0, 0, 0, 0, 0};
  bool has_preview_object_ = false;
  gfx::IRenderer* renderer_;
  std::shared_ptr<zelda3::DungeonObjectEditor> object_editor_;

  // Selection state cache (updated via callback)
  size_t cached_selection_count_ = 0;
  bool selection_callbacks_setup_ = false;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_OBJECT_EDITOR_CARD_H
