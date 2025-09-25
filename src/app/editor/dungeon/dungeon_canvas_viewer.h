#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_CANVAS_VIEWER_H
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_CANVAS_VIEWER_H

#include "app/gui/canvas.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/object_renderer.h"
#include "app/zelda3/dungeon/room.h"
#include "app/gfx/snes_palette.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

/**
 * @brief Handles the main dungeon canvas rendering and interaction
 */
class DungeonCanvasViewer {
 public:
  explicit DungeonCanvasViewer(Rom* rom = nullptr) : rom_(rom), object_renderer_(rom) {}

  void DrawDungeonTabView();
  void DrawDungeonCanvas(int room_id);
  void Draw(int room_id);
  
  void SetRom(Rom* rom) { 
    rom_ = rom; 
    object_renderer_.SetROM(rom);
  }
  Rom* rom() const { return rom_; }

  // Room data access
  void SetRooms(std::array<zelda3::Room, 0x128>* rooms) { rooms_ = rooms; }
  void set_active_rooms(const ImVector<int>& rooms) { active_rooms_ = rooms; }
  void set_current_active_room_tab(int tab) { current_active_room_tab_ = tab; }

  // Palette access
  void set_current_palette_group_id(uint64_t id) { current_palette_group_id_ = id; }
  void SetCurrentPaletteId(uint64_t id) { current_palette_id_ = id; }
  void SetCurrentPaletteGroup(const gfx::PaletteGroup& group) { current_palette_group_ = group; }

  // Canvas access
  gui::Canvas& canvas() { return canvas_; }
  const gui::Canvas& canvas() const { return canvas_; }

 private:
  void RenderObjectInCanvas(const zelda3::RoomObject &object,
                            const gfx::SnesPalette &palette);
  void DisplayObjectInfo(const zelda3::RoomObject &object, int canvas_x,
                         int canvas_y);
  void RenderLayoutObjects(const zelda3::RoomLayout &layout,
                           const gfx::SnesPalette &palette);
  
  // Coordinate conversion helpers
  std::pair<int, int> RoomToCanvasCoordinates(int room_x, int room_y) const;
  std::pair<int, int> CanvasToRoomCoordinates(int canvas_x, int canvas_y) const;
  bool IsWithinCanvasBounds(int canvas_x, int canvas_y, int margin = 32) const;
  
  // Room graphics management
  absl::Status LoadAndRenderRoomGraphics(int room_id);
  absl::Status UpdateRoomBackgroundLayers(int room_id);
  void RenderRoomBackgroundLayers(int room_id);

  Rom* rom_ = nullptr;
  gui::Canvas canvas_{"##DungeonCanvas", ImVec2(0x200, 0x200)};
  zelda3::ObjectRenderer object_renderer_;
  
  // Room data
  std::array<zelda3::Room, 0x128>* rooms_ = nullptr;
  ImVector<int> active_rooms_;
  int current_active_room_tab_ = 0;
  
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
};

}  // namespace editor
}  // namespace yaze

#endif
