#ifndef YAZE_APP_EDITOR_DUNGEON_OBJECT_EDITOR_CARD_H
#define YAZE_APP_EDITOR_DUNGEON_OBJECT_EDITOR_CARD_H

#include <memory>
#include <unordered_map>

#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gui/canvas.h"
#include "app/editor/dungeon/dungeon_object_selector.h"
#include "app/gui/editor_layout.h"
#include "app/gui/widgets/dungeon_object_emulator_preview.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/room_object.h"

namespace yaze {
namespace editor {

/**
 * @brief Unified card combining object selection, emulator preview, and canvas interaction
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
  ObjectEditorCard(gfx::IRenderer* renderer, Rom* rom, DungeonCanvasViewer* canvas_viewer);
  
  // Main update function
  void Draw(bool* p_open);
  
  // Access to components
  DungeonObjectSelector& object_selector() { return object_selector_; }
  gui::DungeonObjectEmulatorPreview& emulator_preview() { return emulator_preview_; }
  
  // Update current room context
  void SetCurrentRoom(int room_id) { current_room_id_ = room_id; }
  
 private:
  void DrawObjectSelector();
  void DrawEmulatorPreview();
  void DrawInteractionControls();
  void DrawSelectedObjectInfo();
  void DrawObjectPreviewIcon(int object_id, const ImVec2& size);
  
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
  
  // Object interaction mode
  enum class InteractionMode {
    None,
    Place,
    Select,
    Delete
  };
  InteractionMode interaction_mode_ = InteractionMode::None;
  
  // Selected object for placement
  zelda3::RoomObject preview_object_{0, 0, 0, 0, 0};
  bool has_preview_object_ = false;
  gfx::IRenderer* renderer_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_OBJECT_EDITOR_CARD_H
