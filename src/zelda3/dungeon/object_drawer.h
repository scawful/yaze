#ifndef YAZE_APP_ZELDA3_DUNGEON_OBJECT_DRAWER_H
#define YAZE_APP_ZELDA3_DUNGEON_OBJECT_DRAWER_H

#include <functional>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "app/gfx/render/background_buffer.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/types/snes_tile.h"
#include "rom/rom.h"
#include "zelda3/dungeon/door_types.h"
#include "zelda3/dungeon/door_position.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/dungeon/dungeon_state.h"
#include "zelda3/dungeon/custom_object.h"

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
  explicit ObjectDrawer(Rom* rom, int room_id, const uint8_t* room_gfx_buffer = nullptr);

  /**
   * @brief Draw a room object to background buffers
   * @param object The object to draw
   * @param bg1 Background layer 1 buffer (object buffer)
   * @param bg2 Background layer 2 buffer (object buffer)
   * @param palette_group Current palette group for color mapping
   * @param layout_bg1 Optional layout buffer to mask for BG2 object transparency
   * @return Status of the drawing operation
   */
  absl::Status DrawObject(const RoomObject& object, gfx::BackgroundBuffer& bg1,
                          gfx::BackgroundBuffer& bg2,
                          const gfx::PaletteGroup& palette_group,
                          const DungeonState* state = nullptr,
                          gfx::BackgroundBuffer* layout_bg1 = nullptr);

  struct DoorDef {
      DoorType type;
      DoorDirection direction;
      uint8_t position;

      // Helper to get position coordinates using DoorPositionManager
      std::pair<int, int> GetTileCoords() const {
        return DoorPositionManager::PositionToTileCoords(position, direction);
      }

      // Helper to get door dimensions
      DoorDimensions GetDimensions() const {
        return GetDoorDimensions(direction);
      }
  };

  // Chest index tracking for state queries
  void ResetChestIndex() { current_chest_index_ = 0; }

  /**
   * @brief Draw a door to background buffers
   * @param door Door definition (type, direction, position)
   * @param bg1 Background layer 1 buffer
   * @param bg2 Background layer 2 buffer
   */
  void DrawDoor(const DoorDef& door, int door_index, gfx::BackgroundBuffer& bg1,
                gfx::BackgroundBuffer& bg2, const DungeonState* state = nullptr);

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
   * @param bg1 Background layer 1 buffer (object buffer)
   * @param bg2 Background layer 2 buffer (object buffer)
   * @param palette_group Current palette group for color mapping
   * @param layout_bg1 Optional layout buffer to mask for BG2 object transparency
   * @return Status of the drawing operation
   */
  absl::Status DrawObjectList(const std::vector<RoomObject>& objects,
                              gfx::BackgroundBuffer& bg1,
                              gfx::BackgroundBuffer& bg2,
                              const gfx::PaletteGroup& palette_group,
                              const DungeonState* state = nullptr,
                              gfx::BackgroundBuffer* layout_bg1 = nullptr);

  /**
   * @brief Get draw routine ID for an object
   * @param object_id The object ID to look up
   * @return Draw routine ID (0-24) based on ZScream mapping
   */
  int GetDrawRoutineId(int16_t object_id) const;

  /**
   * @brief Get the total number of registered draw routines
   */
  int GetDrawRoutineCount() const {
    return static_cast<int>(draw_routines_.size());
  }

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
   * @brief Draw a fixed 2x2 (16x16) tile pattern from RoomDrawObjectData
   *
   * USDASM uses hardcoded offsets into RoomDrawObjectData for certain special
   * objects that are NOT part of the room object stream (e.g. pushable blocks
   * and lightable torches). See bank_01.asm: RoomDraw_PushableBlock and
   * RoomDraw_LightableTorch.
   *
   * @param object_id Identifier used only for tracing/debugging (not used for routine lookup)
   * @param tile_x Top-left X position in tiles
   * @param tile_y Top-left Y position in tiles
   * @param layer Target layer (BG1/BG2)
   * @param room_draw_object_data_offset Offset from RoomDrawObjectData base (not an absolute PC address)
   */
  absl::Status DrawRoomDrawObjectData2x2(uint16_t object_id, int tile_x,
                                        int tile_y, RoomObject::LayerType layer,
                                        uint16_t room_draw_object_data_offset,
                                        gfx::BackgroundBuffer& bg1,
                                        gfx::BackgroundBuffer& bg2);

  /**
   * @brief Calculate the dimensions (width, height) of an object in pixels
   * @param object The object to calculate dimensions for
   * @return Pair of (width, height) in pixels
   */
  std::pair<int, int> CalculateObjectDimensions(const RoomObject& object);

  struct TileTrace {
    uint16_t object_id = 0;
    uint8_t size = 0;
    uint8_t layer = 0;
    int16_t x_tile = 0;
    int16_t y_tile = 0;
    uint16_t tile_id = 0;
    uint8_t flags = 0;
  };

  void SetTraceCollector(std::vector<TileTrace>* collector,
                         bool trace_only = false);
  void ClearTraceCollector();
  bool TraceOnly() const { return trace_only_; }

 protected:
  // Draw routine function type
  using DrawRoutine = std::function<void(ObjectDrawer*, const RoomObject&,
                                         gfx::BackgroundBuffer&,
                                         std::span<const gfx::TileInfo>,
                                         const DungeonState*)>;

  // Core draw routines (based on ZScream's subtype1_routines table)
  void DrawChest(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                 std::span<const gfx::TileInfo> tiles,
                 const DungeonState* state = nullptr);

  void DrawNothing(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                   std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRightwards2x2_1to15or32(const RoomObject& obj,
                                   gfx::BackgroundBuffer& bg,
                                   std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRightwards2x4_1to15or26(const RoomObject& obj,
                                   gfx::BackgroundBuffer& bg,
                                   std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRightwards2x4_1to16(const RoomObject& obj,
                               gfx::BackgroundBuffer& bg,
                               std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRightwards2x4_1to16_BothBG(
      const RoomObject& obj, gfx::BackgroundBuffer& bg,
      std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRightwards2x2_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                               std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawDiagonalAcute_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                               std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawDiagonalGrave_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                               std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawDiagonalAcute_1to16_BothBG(const RoomObject& obj,
                                      gfx::BackgroundBuffer& bg,
                                      std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawDiagonalGrave_1to16_BothBG(const RoomObject& obj,
                                      gfx::BackgroundBuffer& bg,
                                      std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRightwards1x2_1to16_plus2(const RoomObject& obj,
                                     gfx::BackgroundBuffer& bg,
                                     std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRightwardsHasEdge1x1_1to16_plus3(
      const RoomObject& obj, gfx::BackgroundBuffer& bg,
      std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRightwardsHasEdge1x1_1to16_plus2(
      const RoomObject& obj, gfx::BackgroundBuffer& bg,
      std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRightwardsHasEdge1x1_1to16_plus23(
      const RoomObject& obj, gfx::BackgroundBuffer& bg,
      std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRightwardsTopCorners1x2_1to16_plus13(
      const RoomObject& obj, gfx::BackgroundBuffer& bg,
      std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRightwardsBottomCorners1x2_1to16_plus13(
      const RoomObject& obj, gfx::BackgroundBuffer& bg,
      std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void CustomDraw(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                  std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRightwards4x4_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                               std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRightwards1x1Solid_1to16_plus3(const RoomObject& obj,
                                          gfx::BackgroundBuffer& bg,
                                          std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawDoorSwitcherer(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                          std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRightwardsDecor4x4spaced2_1to16(
      const RoomObject& obj, gfx::BackgroundBuffer& bg,
      std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRightwardsStatue2x3spaced2_1to16(
      const RoomObject& obj, gfx::BackgroundBuffer& bg,
      std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRightwardsPillar2x4spaced4_1to16(
      const RoomObject& obj, gfx::BackgroundBuffer& bg,
      std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRightwardsDecor4x3spaced4_1to16(
      const RoomObject& obj, gfx::BackgroundBuffer& bg,
      std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRightwardsDoubled2x2spaced2_1to16(
      const RoomObject& obj, gfx::BackgroundBuffer& bg,
      std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRightwardsDecor2x2spaced12_1to16(
      const RoomObject& obj, gfx::BackgroundBuffer& bg,
      std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRightwards4x2_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                               std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRightwardsDecor4x2spaced8_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                            std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRightwardsCannonHole4x3_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                          std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);

  // Additional Rightwards draw routines (0x47-0x5E range)
  void DrawRightwardsLine1x1_1to16plus1(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                         std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRightwardsBar4x3_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                   std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRightwardsShelf4x4_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                     std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRightwardsBigRail1x3_1to16plus5(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                            std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRightwardsBlock2x2spaced2_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                            std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);

  // Corner draw routines
  void DrawCorner4x4(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                     std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);

  // Downwards draw routines
  void DrawDownwards2x2_1to15or32(const RoomObject& obj,
                                  gfx::BackgroundBuffer& bg,
                                  std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawDownwards4x2_1to15or26(const RoomObject& obj,
                                  gfx::BackgroundBuffer& bg,
                                  std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawDownwards4x2_1to16_BothBG(const RoomObject& obj,
                                     gfx::BackgroundBuffer& bg,
                                     std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawDownwardsDecor4x2spaced4_1to16(const RoomObject& obj,
                                          gfx::BackgroundBuffer& bg,
                                          std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawDownwards2x2_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawDownwardsHasEdge1x1_1to16_plus3(
      const RoomObject& obj, gfx::BackgroundBuffer& bg,
      std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawDownwardsHasEdge1x1_1to16_plus23(
      const RoomObject& obj, gfx::BackgroundBuffer& bg,
      std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawDownwardsEdge1x1_1to16(const RoomObject& obj,
                                  gfx::BackgroundBuffer& bg,
                                  std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawDownwardsLeftCorners2x1_1to16_plus12(
      const RoomObject& obj, gfx::BackgroundBuffer& bg,
      std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawDownwardsRightCorners2x1_1to16_plus12(
      const RoomObject& obj, gfx::BackgroundBuffer& bg,
      std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);

  // Additional Downwards draw routines (0x70-0x7F range)
  void DrawDownwardsFloor4x4_1to16(const RoomObject& obj,
                                    gfx::BackgroundBuffer& bg,
                                    std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawDownwards1x1Solid_1to16_plus3(const RoomObject& obj,
                                          gfx::BackgroundBuffer& bg,
                                          std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawDownwardsDecor4x4spaced2_1to16(const RoomObject& obj,
                                           gfx::BackgroundBuffer& bg,
                                           std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawDownwardsPillar2x4spaced2_1to16(const RoomObject& obj,
                                            gfx::BackgroundBuffer& bg,
                                            std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawDownwardsDecor3x4spaced4_1to16(const RoomObject& obj,
                                           gfx::BackgroundBuffer& bg,
                                           std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawDownwardsDecor2x2spaced12_1to16(const RoomObject& obj,
                                            gfx::BackgroundBuffer& bg,
                                            std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawDownwardsLine1x1_1to16plus1(const RoomObject& obj,
                                        gfx::BackgroundBuffer& bg,
                                        std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawDownwardsDecor2x4spaced8_1to16(const RoomObject& obj,
                                           gfx::BackgroundBuffer& bg,
                                           std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);

  // Phase 4 Step 2: Simple Variant Routines (0x80-0x96, 0xB0-0xBD range)
  void DrawDownwardsDecor3x4spaced2_1to16(const RoomObject& obj,
                                           gfx::BackgroundBuffer& bg,
                                           std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawDownwardsBigRail3x1_1to16plus5(const RoomObject& obj,
                                           gfx::BackgroundBuffer& bg,
                                           std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawDownwardsBlock2x2spaced2_1to16(const RoomObject& obj,
                                           gfx::BackgroundBuffer& bg,
                                           std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawDownwardsCannonHole3x6_1to16(const RoomObject& obj,
                                         gfx::BackgroundBuffer& bg,
                                         std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawDownwardsBar2x3_1to16(const RoomObject& obj,
                                  gfx::BackgroundBuffer& bg,
                                  std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawDownwardsPots2x2_1to16(const RoomObject& obj,
                                   gfx::BackgroundBuffer& bg,
                                   std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawDownwardsHammerPegs2x2_1to16(const RoomObject& obj,
                                         gfx::BackgroundBuffer& bg,
                                         std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRightwardsEdge1x1_1to16plus7(const RoomObject& obj,
                                         gfx::BackgroundBuffer& bg,
                                         std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRightwardsPots2x2_1to16(const RoomObject& obj,
                                    gfx::BackgroundBuffer& bg,
                                    std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRightwardsHammerPegs2x2_1to16(const RoomObject& obj,
                                          gfx::BackgroundBuffer& bg,
                                          std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);

  // Phase 4 Step 3: Diagonal Ceiling Routines (0xA0-0xAC range)
  void DrawDiagonalCeilingTopLeft(const RoomObject& obj,
                                   gfx::BackgroundBuffer& bg,
                                   std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawDiagonalCeilingBottomLeft(const RoomObject& obj,
                                      gfx::BackgroundBuffer& bg,
                                      std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawDiagonalCeilingTopRight(const RoomObject& obj,
                                    gfx::BackgroundBuffer& bg,
                                    std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawDiagonalCeilingBottomRight(const RoomObject& obj,
                                       gfx::BackgroundBuffer& bg,
                                       std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);

  // Phase 4 Step 5: Special Routines (0xC1, 0xCD, 0xCE, 0xDC)
  void DrawClosedChestPlatform(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                               std::span<const gfx::TileInfo> tiles);
  void DrawMovingWallWest(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                          std::span<const gfx::TileInfo> tiles);
  void DrawMovingWallEast(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                          std::span<const gfx::TileInfo> tiles);
  void DrawOpenChestPlatform(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                             std::span<const gfx::TileInfo> tiles);

  // Type 3 / Special Routines
  void DrawSomariaLine(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                       std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawWaterFace(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                     std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void Draw4x4Corner_BothBG(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                            std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawWeirdCornerBottom_BothBG(const RoomObject& obj,
                                    gfx::BackgroundBuffer& bg,
                                    std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawWeirdCornerTop_BothBG(const RoomObject& obj,
                                 gfx::BackgroundBuffer& bg,
                                 std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawLargeCanvasObject(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                             std::span<const gfx::TileInfo> tiles, int width,
                             int height);

  // Type 2 Special Object Routines (0x122, 0x12C, 0x13E, etc.)
  void DrawBed4x5(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                  std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRightwards3x6(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                         std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawUtility6x3(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                      std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawUtility3x5(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                      std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);

  // Type 3 Special Object Routines (pipes, shells, lighting, etc.)
  void DrawVerticalTurtleRockPipe(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                  std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawHorizontalTurtleRockPipe(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                    std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawLightBeam(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                     std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawBigLightBeam(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                        std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawBossShell4x4(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                        std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawSolidWallDecor3x4(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                             std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawArcheryGameTargetDoor(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                 std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawGanonTriforceFloorDecor(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                   std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawSingle2x2(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                     std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawSingle4x4(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                     std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawSingle4x3(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                     std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawRupeeFloor(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                      std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
  void DrawActual4x4(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                     std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);
                     
  // Custom Object Routine
  void DrawCustomObject(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                        std::span<const gfx::TileInfo> tiles, const DungeonState* state = nullptr);

  // Utility methods
  // Execute a DrawRoutineRegistry routine (pure DrawContext function) but render
  // via ObjectDrawer::WriteTile8 so output hits the bitmap-backed buffers used
  // by RoomLayerManager compositing.
  void DrawUsingRegistryRoutine(int routine_id, const RoomObject& obj,
                                gfx::BackgroundBuffer& bg,
                                std::span<const gfx::TileInfo> tiles,
                                const DungeonState* state);
  void WriteTile8(gfx::BackgroundBuffer& bg, int tile_x, int tile_y,
                  const gfx::TileInfo& tile_info);
  bool IsValidTilePosition(int tile_x, int tile_y) const;

  // Check if a draw routine should render to both BG1 and BG2
  // Uses metadata instead of hardcoded routine IDs
  static bool RoutineDrawsToBothBGs(int routine_id);

  /**
   * @brief Mark BG1 pixels as transparent where BG2 overlay objects are drawn
   *
   * This creates "holes" in BG1 that allow BG2 content to show through,
   * matching SNES behavior where Layer 1 objects only write to BG2 tilemap.
   *
   * @param bg1 Background layer 1 buffer to mark transparent
   * @param tile_x Object X position in tiles
   * @param tile_y Object Y position in tiles
   * @param pixel_width Width of the object in pixels
   * @param pixel_height Height of the object in pixels
   */
  void MarkBG1Transparent(gfx::BackgroundBuffer& bg1, int tile_x, int tile_y,
                          int pixel_width, int pixel_height);

  // Door indicator fallback when graphics unavailable
  void DrawDoorIndicator(gfx::Bitmap& bitmap, int tile_x, int tile_y,
                         int width, int height, DoorType type, DoorDirection direction);

  // Draw routine function array (indexed by routine ID from DrawRoutineRegistry)
  std::vector<DrawRoutine> draw_routines_;
  bool routines_initialized_ = false;

  struct TraceContext {
    uint16_t object_id = 0;
    uint8_t size = 0;
    uint8_t layer = 0;
  };

  void SetTraceContext(const RoomObject& object, RoomObject::LayerType layer);
  void PushTrace(int tile_x, int tile_y, const gfx::TileInfo& tile_info);
  static void TraceHookThunk(int tile_x, int tile_y,
                             const gfx::TileInfo& tile_info,
                             void* user_data);

  TraceContext trace_context_{};
  std::vector<TileTrace>* trace_collector_ = nullptr;
  bool trace_only_ = false;

  Rom* rom_;
  int room_id_;
  mutable int current_chest_index_ = 0;
  const uint8_t*
      room_gfx_buffer_;  // Room-specific graphics buffer (current_gfx16_)

  // Canvas dimensions in tiles (64x64 = 512x512 pixels)
  static constexpr int kMaxTilesX = 64;
  static constexpr int kMaxTilesY = 64;
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_DUNGEON_OBJECT_DRAWER_H
