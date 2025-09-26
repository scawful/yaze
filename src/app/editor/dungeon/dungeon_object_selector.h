#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_OBJECT_SELECTOR_H
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_OBJECT_SELECTOR_H

#include "app/gui/canvas.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/object_renderer.h"
#include "app/zelda3/dungeon/dungeon_object_editor.h"
#include "app/zelda3/dungeon/dungeon_editor_system.h"
#include "app/gfx/snes_palette.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

/**
 * @brief Handles object selection, preview, and editing UI
 */
class DungeonObjectSelector {
 public:
  explicit DungeonObjectSelector(Rom* rom = nullptr) : rom_(rom), object_renderer_(rom) {}

  void DrawTileSelector();
  void DrawObjectRenderer();
  void DrawIntegratedEditingPanels();
  void Draw();
  
  void set_rom(Rom* rom) { 
    rom_ = rom; 
    object_renderer_.SetROM(rom);
  }
  void SetRom(Rom* rom) { 
    rom_ = rom; 
    object_renderer_.SetROM(rom);
  }
  Rom* rom() const { return rom_; }

  // Editor system access
  void set_dungeon_editor_system(std::unique_ptr<zelda3::DungeonEditorSystem>* system) { 
    dungeon_editor_system_ = system; 
  }
  void set_object_editor(std::shared_ptr<zelda3::DungeonObjectEditor>* editor) { 
    object_editor_ = editor; 
  }

  // Room data access
  void set_rooms(std::array<zelda3::Room, 0x128>* rooms) { rooms_ = rooms; }
  void set_current_room_id(int room_id) { current_room_id_ = room_id; }

  // Palette access
  void set_current_palette_group_id(uint64_t id) { current_palette_group_id_ = id; }
  void SetCurrentPaletteGroup(const gfx::PaletteGroup& palette_group) { current_palette_group_ = palette_group; }
  void SetCurrentPaletteId(uint64_t palette_id) { current_palette_id_ = palette_id; }
  
  // Object selection callbacks
  void SetObjectSelectedCallback(std::function<void(const zelda3::RoomObject&)> callback) {
    object_selected_callback_ = callback;
  }
  
  void SetObjectPlacementCallback(std::function<void(const zelda3::RoomObject&)> callback) {
    object_placement_callback_ = callback;
  }
  
  // Get current preview object for placement
  const zelda3::RoomObject& GetPreviewObject() const { return preview_object_; }
  bool IsObjectLoaded() const { return object_loaded_; }

 private:
  void DrawRoomGraphics();
  void DrawObjectBrowser();
  void DrawCompactObjectEditor();
  void DrawCompactSpriteEditor();
  
  // Helper methods for primitive object rendering
  ImU32 GetObjectTypeColor(int object_id);
  std::string GetObjectTypeSymbol(int object_id);
  void RenderObjectPrimitive(const zelda3::RoomObject& object, int x, int y);
  
  // AssetBrowser-style object selection
  void DrawObjectAssetBrowser();
  bool MatchesObjectFilter(int obj_id, int filter_type);
  void CalculateObjectDimensions(const zelda3::RoomObject& object, int& width, int& height);
  void PlaceObjectAtPosition(int x, int y);
  void DrawCompactItemEditor();
  void DrawCompactEntranceEditor();
  void DrawCompactDoorEditor();
  void DrawCompactChestEditor();
  void DrawCompactPropertiesEditor();

  Rom* rom_ = nullptr;
  gui::Canvas room_gfx_canvas_{"##RoomGfxCanvas", ImVec2(0x100 + 1, 0x10 * 0x40 + 1)};
  gui::Canvas object_canvas_;
  zelda3::ObjectRenderer object_renderer_;
  
  // Editor systems
  std::unique_ptr<zelda3::DungeonEditorSystem>* dungeon_editor_system_ = nullptr;
  std::shared_ptr<zelda3::DungeonObjectEditor>* object_editor_ = nullptr;
  
  // Room data
  std::array<zelda3::Room, 0x128>* rooms_ = nullptr;
  int current_room_id_ = 0;
  
  // Palette data
  uint64_t current_palette_group_id_ = 0;
  uint64_t current_palette_id_ = 0;
  gfx::PaletteGroup current_palette_group_;
  
  // Object preview system
  zelda3::RoomObject preview_object_{0, 0, 0, 0, 0};
  gfx::SnesPalette preview_palette_;
  bool object_loaded_ = false;
  
  // Callback for object selection
  std::function<void(const zelda3::RoomObject&)> object_selected_callback_;
  std::function<void(const zelda3::RoomObject&)> object_placement_callback_;
  
  // Object selection state
  int selected_object_id_ = -1;
};

}  // namespace editor
}  // namespace yaze

#endif
