#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_CANVAS_VIEWER_H
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_CANVAS_VIEWER_H

#include <map>

#include "app/gui/canvas.h"
#include "app/rom.h"
#include "zelda3/dungeon/room.h"
#include "app/gfx/snes_palette.h"
#include "dungeon_object_interaction.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

/**
 * @brief Handles the main dungeon canvas rendering and interaction
 * 
 * In Link to the Past, dungeon "layers" are not separate visual layers
 * but a game concept where objects exist on different logical levels.
 * Players move between these levels using stair objects that act as
 * transitions between the different object planes.
 */
class DungeonCanvasViewer {
 public:
  explicit DungeonCanvasViewer(Rom* rom = nullptr)
      : rom_(rom), object_interaction_(&canvas_) {}

  // DrawDungeonTabView() removed - using EditorCard system instead
  void DrawDungeonCanvas(int room_id);
  void Draw(int room_id);
  
  void SetRom(Rom* rom) { 
    rom_ = rom; 
  }
  Rom* rom() const { return rom_; }

  // Room data access
  void SetRooms(std::array<zelda3::Room, 0x128>* rooms) { rooms_ = rooms; }
  // Used by overworld editor when double-clicking entrances
  void set_active_rooms(const ImVector<int>& rooms) { active_rooms_ = rooms; }
  void set_current_active_room_tab(int tab) { current_active_room_tab_ = tab; }

  // Palette access
  void set_current_palette_group_id(uint64_t id) { current_palette_group_id_ = id; }
  void SetCurrentPaletteId(uint64_t id) { current_palette_id_ = id; }
  void SetCurrentPaletteGroup(const gfx::PaletteGroup& group) { current_palette_group_ = group; }

  // Canvas access
  gui::Canvas& canvas() { return canvas_; }
  const gui::Canvas& canvas() const { return canvas_; }
  
  // Object interaction access
  DungeonObjectInteraction& object_interaction() { return object_interaction_; }
  
  // Enable/disable object interaction mode
  void SetObjectInteractionEnabled(bool enabled) { object_interaction_enabled_ = enabled; }
  bool IsObjectInteractionEnabled() const { return object_interaction_enabled_; }
  
  // Layer visibility controls (per-room)
  void SetBG1Visible(int room_id, bool visible) { 
    GetRoomLayerSettings(room_id).bg1_visible = visible; 
  }
  void SetBG2Visible(int room_id, bool visible) { 
    GetRoomLayerSettings(room_id).bg2_visible = visible; 
  }
  bool IsBG1Visible(int room_id) const { 
    auto it = room_layer_settings_.find(room_id);
    return it != room_layer_settings_.end() ? it->second.bg1_visible : true;
  }
  bool IsBG2Visible(int room_id) const { 
    auto it = room_layer_settings_.find(room_id);
    return it != room_layer_settings_.end() ? it->second.bg2_visible : true;
  }
  
  // BG2 layer type controls (per-room)
  void SetBG2LayerType(int room_id, int type) { 
    GetRoomLayerSettings(room_id).bg2_layer_type = type; 
  }
  int GetBG2LayerType(int room_id) const { 
    auto it = room_layer_settings_.find(room_id);
    return it != room_layer_settings_.end() ? it->second.bg2_layer_type : 0;
  }
  
  // Set the object to be placed
  void SetPreviewObject(const zelda3::RoomObject& object) {
    object_interaction_.SetPreviewObject(object, true);
  }
  void ClearPreviewObject() {
    object_interaction_.SetPreviewObject(zelda3::RoomObject{0, 0, 0, 0, 0}, false);
  }

 private:
  void DisplayObjectInfo(const zelda3::RoomObject &object, int canvas_x,
                         int canvas_y);
  void RenderSprites(const zelda3::Room& room);
  
  // Coordinate conversion helpers
  std::pair<int, int> RoomToCanvasCoordinates(int room_x, int room_y) const;
  std::pair<int, int> CanvasToRoomCoordinates(int canvas_x, int canvas_y) const;
  bool IsWithinCanvasBounds(int canvas_x, int canvas_y, int margin = 32) const;
  
  // Object dimension calculation
  void CalculateWallDimensions(const zelda3::RoomObject& object, int& width, int& height);
  
  // Visualization
  void DrawObjectPositionOutlines(const zelda3::Room& room);
  
  // Room graphics management
  // Load: Read from ROM, Render: Process pixels, Draw: Display on canvas
  absl::Status LoadAndRenderRoomGraphics(int room_id);
  void DrawRoomBackgroundLayers(int room_id);  // Draw room buffers to canvas

  Rom* rom_ = nullptr;
  gui::Canvas canvas_{"##DungeonCanvas", ImVec2(0x200, 0x200)};
  // ObjectRenderer removed - use ObjectDrawer for rendering (production system)
  DungeonObjectInteraction object_interaction_;
  
  // Room data
  std::array<zelda3::Room, 0x128>* rooms_ = nullptr;
  // Used by overworld editor for double-click entrance → open dungeon room
  ImVector<int> active_rooms_;
  int current_active_room_tab_ = 0;
  
  // Object interaction state
  bool object_interaction_enabled_ = true;
  
  // Per-room layer visibility settings
  struct RoomLayerSettings {
    bool bg1_visible = true;
    bool bg2_visible = true;
    int bg2_layer_type = 0;  // 0=Normal, 1=Translucent, 2=Addition, etc.
  };
  std::map<int, RoomLayerSettings> room_layer_settings_;
  
  // Helper to get settings for a room (creates default if not exists)
  RoomLayerSettings& GetRoomLayerSettings(int room_id) {
    return room_layer_settings_[room_id];
  }
  
  // Palette data
  uint64_t current_palette_group_id_ = 0;
  uint64_t current_palette_id_ = 0;
  gfx::PaletteGroup current_palette_group_;

  // Object rendering cache
  struct ObjectRenderCache {
    int object_id;
    int object_x, object_y, object_size;
    uint64_t palette_hash;
    gfx::Bitmap rendered_bitmap;
    bool is_valid;
  };
  std::vector<ObjectRenderCache> object_render_cache_;
  uint64_t last_palette_hash_ = 0;
  
  // Debug state flags
  bool show_room_debug_info_ = false;
  bool show_texture_debug_ = false;
  bool show_object_bounds_ = false;
  bool show_layer_info_ = false;
  int layout_override_ = -1; // -1 for no override

  // Object outline filtering toggles
  struct ObjectOutlineToggles {
    bool show_type1_objects = true;   // Standard objects (0x00-0xFF)
    bool show_type2_objects = true;   // Extended objects (0x100-0x1FF)
    bool show_type3_objects = true;   // Special objects (0xF00-0xFFF)
    bool show_layer0_objects = true;  // Layer 0 (BG1)
    bool show_layer1_objects = true;  // Layer 1 (BG2)
    bool show_layer2_objects = true;  // Layer 2 (BG3)
  };
  ObjectOutlineToggles object_outline_toggles_;
};

}  // namespace editor
}  // namespace yaze

#endif
