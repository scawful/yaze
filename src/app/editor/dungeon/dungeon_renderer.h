#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_RENDERER_H
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_RENDERER_H

#include <vector>

#include "absl/status/status.h"
#include "app/gfx/snes_palette.h"
#include "app/gui/canvas.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/object_renderer.h"
#include "app/zelda3/dungeon/room.h"
#include "app/zelda3/dungeon/room_layout.h"
#include "app/zelda3/dungeon/room_object.h"

namespace yaze {
namespace editor {

/**
 * @brief Handles rendering of dungeon objects, layouts, and backgrounds
 * 
 * This component manages all rendering operations for the dungeon editor,
 * including object caching, background layers, and layout visualization.
 */
class DungeonRenderer {
 public:
  explicit DungeonRenderer(gui::Canvas* canvas, Rom* rom) 
      : canvas_(canvas), rom_(rom), object_renderer_(rom) {}
  
  // Object rendering
  void RenderObjectInCanvas(const zelda3::RoomObject& object,
                            const gfx::SnesPalette& palette);
  void DisplayObjectInfo(const zelda3::RoomObject& object, int canvas_x, int canvas_y);
  void RenderSprites(const zelda3::Room& room);
  
  // Background rendering
  void RenderRoomBackgroundLayers(int room_id);
  absl::Status RefreshGraphics(int room_id, uint64_t palette_id,
                               const gfx::PaletteGroup& palette_group);
  
  // Graphics management
  absl::Status LoadAndRenderRoomGraphics(int room_id, 
                                         std::array<zelda3::Room, 0x128>& rooms);
  absl::Status ReloadAllRoomGraphics(std::array<zelda3::Room, 0x128>& rooms);
  
  // Cache management
  void ClearObjectCache() { object_render_cache_.clear(); }
  size_t GetCacheSize() const { return object_render_cache_.size(); }
  
  // Coordinate conversion helpers
  std::pair<int, int> RoomToCanvasCoordinates(int room_x, int room_y) const;
  std::pair<int, int> CanvasToRoomCoordinates(int canvas_x, int canvas_y) const;
  bool IsWithinCanvasBounds(int canvas_x, int canvas_y, int margin = 32) const;

 private:
  gui::Canvas* canvas_;
  Rom* rom_;
  zelda3::ObjectRenderer object_renderer_;
  
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

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_RENDERER_H
