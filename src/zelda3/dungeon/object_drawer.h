#ifndef YAZE_APP_ZELDA3_DUNGEON_OBJECT_DRAWER_H
#define YAZE_APP_ZELDA3_DUNGEON_OBJECT_DRAWER_H

#include <functional>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "app/gfx/render/background_buffer.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/types/snes_tile.h"
#include "app/rom.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {

/**
 * @brief Draws dungeon objects to background buffers using game patterns
 *
 * This class interprets object IDs and draws them to BG1/BG2 buffers
 * using the patterns extracted from the game's drawing routines.
 * Based on ZScream's DungeonObjectData.cs and Subtype1_Draw.cs patterns.
 *
 * Architecture:
 * 1. Load tile data from ROM for the object
 * 2. Look up draw routine ID from object ID mapping
 * 3. Execute appropriate draw routine with size/orientation
 * 4. Write tiles to BackgroundBuffer according to pattern
 * 5. Handle palette coordination and graphics sheet access
 */
class ObjectDrawer {
 public:
  explicit ObjectDrawer(Rom* rom, const uint8_t* room_gfx_buffer = nullptr);

  /**
   * @brief Draw a room object to background buffers
   * @param object The object to draw
   * @param bg1 Background layer 1 buffer
   * @param bg2 Background layer 2 buffer
   * @param palette_group Current palette group for color mapping
   * @return Status of the drawing operation
   */
  absl::Status DrawObject(const RoomObject& object, gfx::BackgroundBuffer& bg1,
                          gfx::BackgroundBuffer& bg2,
                          const gfx::PaletteGroup& palette_group);

  struct DoorDef {
      uint8_t type;
      uint8_t direction;
      uint8_t position;
  };

  /**
   * @brief Draw a door to background buffers
   * @param door Door definition (type, direction, position)
   * @param bg1 Background layer 1 buffer
   * @param bg2 Background layer 2 buffer
   */
  void DrawDoor(const DoorDef& door, gfx::BackgroundBuffer& bg1,
                gfx::BackgroundBuffer& bg2);

  /**
   * @brief Draw a pot item visualization
   * @param item_id Item ID
   * @param x X coordinate (pixels)
   * @param y Y coordinate (pixels)
   * @param bg Background buffer
   */
  void DrawPotItem(uint8_t item_id, int x, int y, gfx::BackgroundBuffer& bg);

  /**
   * @brief Draw all objects in a room
   * @param objects Vector of room objects
   * @param bg1 Background layer 1 buffer
   * @param bg2 Background layer 2 buffer
   * @param palette_group Current palette group for color mapping
   * @return Status of the drawing operation
   */
  absl::Status DrawObjectList(const std::vector<RoomObject>& objects,
                              gfx::BackgroundBuffer& bg1,
                              gfx::BackgroundBuffer& bg2,
                              const gfx::PaletteGroup& palette_group);

  /**
   * @brief Get draw routine ID for an object
   * @param object_id The object ID to look up
   * @return Draw routine ID (0-24) based on ZScream mapping
   */
  int GetDrawRoutineId(int16_t object_id) const;

  /**
   * @brief Initialize draw routine registry
   * Must be called before drawing objects
   */
  void InitializeDrawRoutines();

  /**
   * @brief Draw a single tile directly to bitmap
   * @param bitmap Target bitmap to draw to
   * @param tile_info Tile information (ID, palette, mirroring)
   * @param pixel_x X pixel coordinate in bitmap
   * @param pixel_y Y pixel coordinate in bitmap
   * @param tiledata Source graphics data from ROM
   */
  void DrawTileToBitmap(gfx::Bitmap& bitmap, const gfx::TileInfo& tile_info,
                        int pixel_x, int pixel_y, const uint8_t* tiledata);

  /**
   * @brief Calculate the dimensions (width, height) of an object in pixels
   * @param object The object to calculate dimensions for
   * @return Pair of (width, height) in pixels
   */
  std::pair<int, int> CalculateObjectDimensions(const RoomObject& object);

 private:
  // Draw routine function type
  using DrawRoutine = std::function<void(ObjectDrawer*, const RoomObject&,
                                         gfx::BackgroundBuffer&,
                                         std::span<const gfx::TileInfo>)>;

  // Core draw routines (based on ZScream's subtype1_routines table)
  void DrawRightwards2x2_1to15or32(const RoomObject& obj,
                                   gfx::BackgroundBuffer& bg,
                                   std::span<const gfx::TileInfo> tiles);
  void DrawRightwards2x4_1to15or26(const RoomObject& obj,
                                   gfx::BackgroundBuffer& bg,
                                   std::span<const gfx::TileInfo> tiles);
  void DrawRightwards2x4spaced4_1to16(const RoomObject& obj,
                                      gfx::BackgroundBuffer& bg,
                                      std::span<const gfx::TileInfo> tiles);
  void DrawRightwards2x4spaced4_1to16_BothBG(
      const RoomObject& obj, gfx::BackgroundBuffer& bg,
      std::span<const gfx::TileInfo> tiles);
  void DrawRightwards2x2_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                               std::span<const gfx::TileInfo> tiles);
  void DrawDiagonalAcute_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                               std::span<const gfx::TileInfo> tiles);
  void DrawDiagonalGrave_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                               std::span<const gfx::TileInfo> tiles);
  void DrawDiagonalAcute_1to16_BothBG(const RoomObject& obj,
                                      gfx::BackgroundBuffer& bg,
                                      std::span<const gfx::TileInfo> tiles);
  void DrawDiagonalGrave_1to16_BothBG(const RoomObject& obj,
                                      gfx::BackgroundBuffer& bg,
                                      std::span<const gfx::TileInfo> tiles);
  void DrawRightwards1x2_1to16_plus2(const RoomObject& obj,
                                     gfx::BackgroundBuffer& bg,
                                     std::span<const gfx::TileInfo> tiles);
  void DrawRightwardsHasEdge1x1_1to16_plus3(
      const RoomObject& obj, gfx::BackgroundBuffer& bg,
      std::span<const gfx::TileInfo> tiles);
  void DrawRightwardsHasEdge1x1_1to16_plus2(
      const RoomObject& obj, gfx::BackgroundBuffer& bg,
      std::span<const gfx::TileInfo> tiles);
  void DrawRightwardsTopCorners1x2_1to16_plus13(
      const RoomObject& obj, gfx::BackgroundBuffer& bg,
      std::span<const gfx::TileInfo> tiles);
  void DrawRightwardsBottomCorners1x2_1to16_plus13(
      const RoomObject& obj, gfx::BackgroundBuffer& bg,
      std::span<const gfx::TileInfo> tiles);
  void CustomDraw(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                  std::span<const gfx::TileInfo> tiles);
  void DrawRightwards4x4_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                               std::span<const gfx::TileInfo> tiles);
  void DrawRightwards1x1Solid_1to16_plus3(const RoomObject& obj,
                                          gfx::BackgroundBuffer& bg,
                                          std::span<const gfx::TileInfo> tiles);
  void DrawDoorSwitcherer(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                          std::span<const gfx::TileInfo> tiles);
  void DrawRightwardsDecor4x4spaced2_1to16(
      const RoomObject& obj, gfx::BackgroundBuffer& bg,
      std::span<const gfx::TileInfo> tiles);
  void DrawRightwardsStatue2x3spaced2_1to16(
      const RoomObject& obj, gfx::BackgroundBuffer& bg,
      std::span<const gfx::TileInfo> tiles);
  void DrawRightwardsPillar2x4spaced4_1to16(
      const RoomObject& obj, gfx::BackgroundBuffer& bg,
      std::span<const gfx::TileInfo> tiles);
  void DrawRightwardsDecor4x3spaced4_1to16(
      const RoomObject& obj, gfx::BackgroundBuffer& bg,
      std::span<const gfx::TileInfo> tiles);
  void DrawRightwardsDoubled2x2spaced2_1to16(
      const RoomObject& obj, gfx::BackgroundBuffer& bg,
      std::span<const gfx::TileInfo> tiles);
  void DrawRightwardsDecor2x2spaced12_1to16(
      const RoomObject& obj, gfx::BackgroundBuffer& bg,
      std::span<const gfx::TileInfo> tiles);

  // Corner draw routines
  void DrawCorner4x4(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                     std::span<const gfx::TileInfo> tiles);

  // Downwards draw routines
  void DrawDownwards2x2_1to15or32(const RoomObject& obj,
                                  gfx::BackgroundBuffer& bg,
                                  std::span<const gfx::TileInfo> tiles);
  void DrawDownwards4x2_1to15or26(const RoomObject& obj,
                                  gfx::BackgroundBuffer& bg,
                                  std::span<const gfx::TileInfo> tiles);
  void DrawDownwards4x2_1to16_BothBG(const RoomObject& obj,
                                     gfx::BackgroundBuffer& bg,
                                     std::span<const gfx::TileInfo> tiles);
  void DrawDownwardsDecor4x2spaced4_1to16(const RoomObject& obj,
                                          gfx::BackgroundBuffer& bg,
                                          std::span<const gfx::TileInfo> tiles);
  void DrawDownwards2x2_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles);
  void DrawDownwardsHasEdge1x1_1to16_plus3(
      const RoomObject& obj, gfx::BackgroundBuffer& bg,
      std::span<const gfx::TileInfo> tiles);
  void DrawDownwardsEdge1x1_1to16(const RoomObject& obj,
                                  gfx::BackgroundBuffer& bg,
                                  std::span<const gfx::TileInfo> tiles);
  void DrawDownwardsLeftCorners2x1_1to16_plus12(
      const RoomObject& obj, gfx::BackgroundBuffer& bg,
      std::span<const gfx::TileInfo> tiles);
  void DrawDownwardsRightCorners2x1_1to16_plus12(
      const RoomObject& obj, gfx::BackgroundBuffer& bg,
      std::span<const gfx::TileInfo> tiles);

  // Type 3 / Special Routines
  void DrawSomariaLine(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                       std::span<const gfx::TileInfo> tiles);
  void DrawWaterFace(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                     std::span<const gfx::TileInfo> tiles);
  void Draw4x4Corner_BothBG(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                            std::span<const gfx::TileInfo> tiles);
  void DrawWeirdCornerBottom_BothBG(const RoomObject& obj,
                                    gfx::BackgroundBuffer& bg,
                                    std::span<const gfx::TileInfo> tiles);
  void DrawWeirdCornerTop_BothBG(const RoomObject& obj,
                                 gfx::BackgroundBuffer& bg,
                                 std::span<const gfx::TileInfo> tiles);
  void DrawLargeCanvasObject(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                             std::span<const gfx::TileInfo> tiles, int width,
                             int height);

  // Utility methods
  void WriteTile8(gfx::BackgroundBuffer& bg, int tile_x, int tile_y,
                  const gfx::TileInfo& tile_info);
  bool IsValidTilePosition(int tile_x, int tile_y) const;

  // Draw routine registry
  std::unordered_map<int16_t, int> object_to_routine_map_;
  std::vector<DrawRoutine> draw_routines_;
  bool routines_initialized_ = false;

  Rom* rom_;
  const uint8_t*
      room_gfx_buffer_;  // Room-specific graphics buffer (current_gfx16_)

  // Canvas dimensions in tiles (64x64 = 512x512 pixels)
  static constexpr int kMaxTilesX = 64;
  static constexpr int kMaxTilesY = 64;
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_DUNGEON_OBJECT_DRAWER_H
