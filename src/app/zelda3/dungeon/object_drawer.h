#ifndef YAZE_APP_ZELDA3_DUNGEON_OBJECT_DRAWER_H
#define YAZE_APP_ZELDA3_DUNGEON_OBJECT_DRAWER_H

#include <vector>

#include "absl/status/status.h"
#include "app/gfx/background_buffer.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {

/**
 * @brief Draws dungeon objects to background buffers using game patterns
 * 
 * This class interprets object IDs and draws them to BG1/BG2 buffers
 * using the patterns extracted from the game's drawing routines.
 * 
 * Architecture:
 * 1. Load tile data from ROM for the object
 * 2. Determine drawing pattern (rightward, downward, diagonal, special)
 * 3. Write tiles to BackgroundBuffer according to pattern
 * 4. Handle size bytes for repeating patterns
 */
class ObjectDrawer {
 public:
  explicit ObjectDrawer(Rom* rom);
  
  /**
   * @brief Draw a room object to background buffers
   * @param object The object to draw
   * @param bg1 Background layer 1 buffer
   * @param bg2 Background layer 2 buffer
   * @return Status of the drawing operation
   */
  absl::Status DrawObject(const RoomObject& object,
                         gfx::BackgroundBuffer& bg1,
                         gfx::BackgroundBuffer& bg2);
  
  /**
   * @brief Draw all objects in a room
   * @param objects Vector of room objects
   * @param bg1 Background layer 1 buffer
   * @param bg2 Background layer 2 buffer
   * @return Status of the drawing operation
   */
  absl::Status DrawObjectList(const std::vector<RoomObject>& objects,
                              gfx::BackgroundBuffer& bg1,
                              gfx::BackgroundBuffer& bg2);

 private:
  // Pattern-specific drawing methods
  void DrawRightwards2x2(const RoomObject& obj, gfx::BackgroundBuffer& bg, 
                         const gfx::Tile16& tile);
  void DrawDownwards2x2(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                        const gfx::Tile16& tile);
  void DrawDiagonalAcute(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                         const gfx::Tile16& tile);
  void DrawDiagonalGrave(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                         const gfx::Tile16& tile);
  void Draw1x1Solid(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                    const gfx::Tile16& tile);
  void Draw4x4Block(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                    const gfx::Tile16& tile);
  
  // Utility methods
  void WriteTile16(gfx::BackgroundBuffer& bg, int tile_x, int tile_y,
                   const gfx::Tile16& tile);
  bool IsValidTilePosition(int tile_x, int tile_y) const;
  
  Rom* rom_;
  
  // Canvas dimensions in tiles (64x64 = 512x512 pixels)
  static constexpr int kMaxTilesX = 64;
  static constexpr int kMaxTilesY = 64;
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_DUNGEON_OBJECT_DRAWER_H

