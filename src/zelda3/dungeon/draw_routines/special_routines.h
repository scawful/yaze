#ifndef YAZE_ZELDA3_DUNGEON_DRAW_ROUTINES_SPECIAL_ROUTINES_H
#define YAZE_ZELDA3_DUNGEON_DRAW_ROUTINES_SPECIAL_ROUTINES_H

#include "draw_routine_types.h"

namespace yaze {
namespace zelda3 {
namespace draw_routines {

/**
 * @brief Draw chest object (big or small) with open/closed state support
 *
 * Pattern: Chest drawing (objects 0x140-0x14F big chests, 0x150-0x15F small
 * chests) Determines chest state from DungeonState and draws appropriate
 * graphic. Standard chests are 2x2 (4 tiles), big chests are 4x4 (16 tiles).
 * If we have double the tiles, use the second half for open state.
 *
 * @param ctx Draw context containing object, tiles, state, and target buffer
 * @param chest_index Index of this chest in the room (0-based)
 */
void DrawChest(const DrawContext& ctx, int chest_index);

/**
 * @brief Draw nothing - represents invisible logic objects or placeholders
 *
 * Pattern: Logic/invisible objects that have no visual representation
 * ASM: RoomDraw_Nothing_A ($0190F2), RoomDraw_Nothing_B ($01932E), etc.
 * These routines typically just RTS (return from subroutine).
 *
 * @param ctx Draw context containing object information
 */
void DrawNothing(const DrawContext& ctx);

/**
 * @brief Custom draw routine for special objects
 *
 * Pattern: Custom draw routine (objects 0x31-0x32)
 * For now, falls back to simple 1x1 tile placement.
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void CustomDraw(const DrawContext& ctx);

/**
 * @brief Draw door switcher object with state-based graphics
 *
 * Pattern: Door switcher (object 0x35)
 * Special door logic that checks dungeon state to decide which graphic to use.
 * Uses active tile (2nd tile) if door switch is active, otherwise uses default.
 *
 * @param ctx Draw context containing object, tiles, state, and target buffer
 */
void DrawDoorSwitcherer(const DrawContext& ctx);

/**
 * @brief Draw Somaria line in various directions
 *
 * Pattern: Somaria Line (objects 0x203-0x20F, 0x214)
 * Draws a line of tiles based on direction encoded in object ID.
 * Direction mapping:
 *   0x03: Horizontal right
 *   0x04: Vertical down
 *   0x05: Diagonal down-right
 *   0x06: Diagonal down-left
 *   0x07-0x0F: Various other patterns
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawSomariaLine(const DrawContext& ctx);

/**
 * @brief Draw water face pattern (2x2)
 *
 * Pattern: Water Face (Type 3 objects 0xF80-0xF82)
 * Draws a 2x2 face in COLUMN-MAJOR order.
 * TODO: Implement state check from RoomDraw_EmptyWaterFace ($019D29)
 * Should check Room ID, Room State, Door Flags to switch graphic.
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawWaterFace(const DrawContext& ctx);

/**
 * @brief Draw large canvas object with arbitrary dimensions
 *
 * Generic large object drawer for custom-sized objects.
 * Draws tiles in row-major order (left-to-right, top-to-bottom).
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 * @param width Width in tiles
 * @param height Height in tiles
 */
void DrawLargeCanvasObject(const DrawContext& ctx, int width, int height);

// ============================================================================
// SuperSquare Routines (Phase 4)
// ============================================================================

/**
 * @brief Draw 4x4 solid blocks in a super square grid
 *
 * ASM: RoomDraw_4x4BlocksIn4x4SuperSquare ($018B94)
 * Objects: 0xC0, 0xC2
 * Draws solid 4x4 blocks using a single tile, repeated in a grid pattern.
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void Draw4x4BlocksIn4x4SuperSquare(const DrawContext& ctx);

/**
 * @brief Draw 3x3 floor pattern in super square units
 *
 * ASM: RoomDraw_3x3FloorIn4x4SuperSquare ($018D8A)
 * Objects: 0xC3, 0xD7
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void Draw3x3FloorIn4x4SuperSquare(const DrawContext& ctx);

/**
 * @brief Draw 4x4 floor pattern in super square units
 *
 * ASM: RoomDraw_4x4FloorIn4x4SuperSquare ($018FA5)
 * Objects: 0xC5-0xCA, 0xD1-0xD2, 0xD9, 0xDF-0xE8
 * Most common floor pattern in dungeons.
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void Draw4x4FloorIn4x4SuperSquare(const DrawContext& ctx);

/**
 * @brief Draw single 4x4 floor pattern variant
 *
 * ASM: RoomDraw_4x4FloorOneIn4x4SuperSquare ($018FA2)
 * Object: 0xC4
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void Draw4x4FloorOneIn4x4SuperSquare(const DrawContext& ctx);

/**
 * @brief Draw two 4x4 floor pattern variant
 *
 * ASM: RoomDraw_4x4FloorTwoIn4x4SuperSquare ($018F9D)
 * Object: 0xDB
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void Draw4x4FloorTwoIn4x4SuperSquare(const DrawContext& ctx);

/**
 * @brief Draw 4x4 big hole pattern
 *
 * Object: 0xA4
 * Draws a 4x4 hole pattern that repeats based on size.
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawBigHole4x4_1to16(const DrawContext& ctx);

/**
 * @brief Draw 2x2 spike pattern in super square units
 *
 * ASM: RoomDraw_Spike2x2In4x4SuperSquare ($019708)
 * Object: 0xDE
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawSpike2x2In4x4SuperSquare(const DrawContext& ctx);

/**
 * @brief Draw 4x4 table rock pattern
 *
 * Object: 0xDD
 * Draws a 4x4 rock pattern that repeats horizontally.
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawTableRock4x4_1to16(const DrawContext& ctx);

/**
 * @brief Draw water overlay 8x8 pattern
 *
 * ASM: RoomDraw_WaterOverlayA8x8_1to16 ($0195D6)
 * Objects: 0xD8, 0xDA
 * Semi-transparent water overlay.
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawWaterOverlay8x8_1to16(const DrawContext& ctx);

// ============================================================================
// Stair Routines (matching assembly)
// ============================================================================

/**
 * @brief Draw inter-room fat stairs going up (Type 2 object 0x12D)
 *
 * ASM: RoomDraw_InterRoomFatStairsUp ($01A41B)
 * Registers stair position in game state $06B0.
 * Draws 4x4 stair pattern.
 *
 * @param ctx Draw context
 */
void DrawInterRoomFatStairsUp(const DrawContext& ctx);

/**
 * @brief Draw inter-room fat stairs going down A (Type 2 object 0x12E)
 *
 * ASM: RoomDraw_InterRoomFatStairsDownA ($01A458)
 * Registers stair position and draws 4x4 pattern.
 *
 * @param ctx Draw context
 */
void DrawInterRoomFatStairsDownA(const DrawContext& ctx);

/**
 * @brief Draw inter-room fat stairs going down B (Type 2 object 0x12F)
 *
 * ASM: RoomDraw_InterRoomFatStairsDownB ($01A486)
 *
 * @param ctx Draw context
 */
void DrawInterRoomFatStairsDownB(const DrawContext& ctx);

/**
 * @brief Draw spiral stairs (Type 2 objects 0x138-0x13B)
 *
 * ASM: RoomDraw_SpiralStairs* routines
 * Handles upper/lower variants for going up/down.
 *
 * @param ctx Draw context
 * @param going_up True if stairs go up, false if down
 * @param is_upper True if upper portion, false if lower
 */
void DrawSpiralStairs(const DrawContext& ctx, bool going_up, bool is_upper);

/**
 * @brief Draw auto stairs (Type 2/3 objects 0x130-0x133, 0x21B-0x21D, 0x233)
 *
 * ASM: RoomDraw_AutoStairs* routines
 * Multi-layer or merged layer variants.
 *
 * @param ctx Draw context
 */
void DrawAutoStairs(const DrawContext& ctx);

/**
 * @brief Draw straight inter-room stairs (Type 3 objects 0x21E-0x229)
 *
 * ASM: RoomDraw_StraightInterroomStairs* routines
 * North/South, Up/Down, Upper/Lower variants.
 *
 * @param ctx Draw context
 */
void DrawStraightInterRoomStairs(const DrawContext& ctx);

// ============================================================================
// Interactive Object Routines
// ============================================================================

/**
 * @brief Draw prison cell with bars (Type 3 objects 0x20D, 0x217)
 *
 * ASM: RoomDraw_PrisonCell ($019C44)
 * Draws to BOTH BG1 and BG2 with horizontal flip for symmetry.
 * Uses ctx.secondary_bg for dual-layer drawing when available.
 *
 * @param ctx Draw context with optional secondary_bg for dual-BG drawing
 */
void DrawPrisonCell(const DrawContext& ctx);

/**
 * @brief Draw big key lock (Type 3 object 0x218)
 *
 * ASM: RoomDraw_BigKeyLock ($0198AE)
 * Registers lock position in $06E0.
 * Checks room flags to determine if already opened.
 *
 * @param ctx Draw context
 */
void DrawBigKeyLock(const DrawContext& ctx);

/**
 * @brief Draw bombable floor (Type 3 object 0x247)
 *
 * ASM: RoomDraw_BombableFloor ($019B7A)
 * Registers floor position for bombing interaction.
 * Checks if already bombed via room flags.
 *
 * @param ctx Draw context
 */
void DrawBombableFloor(const DrawContext& ctx);

/**
 * @brief Draw moving wall (Type 1 objects 0xCD, 0xCE)
 *
 * ASM: RoomDraw_MovingWallWest ($019316), RoomDraw_MovingWallEast ($01935C)
 * Checks game state to determine if wall has moved.
 *
 * @param ctx Draw context
 * @param is_west True for west wall (0xCD), false for east wall (0xCE)
 */
void DrawMovingWall(const DrawContext& ctx, bool is_west);

// ============================================================================
// Water Face Variants (based on room state)
// ============================================================================

/**
 * @brief Draw empty water face (Type 3 object 0x200)
 *
 * ASM: RoomDraw_EmptyWaterFace ($019D29)
 *
 * @param ctx Draw context
 */
void DrawEmptyWaterFace(const DrawContext& ctx);

/**
 * @brief Draw spitting water face (Type 3 object 0x201)
 *
 * ASM: RoomDraw_SpittingWaterFace ($019D64)
 *
 * @param ctx Draw context
 */
void DrawSpittingWaterFace(const DrawContext& ctx);

/**
 * @brief Draw drenching water face (Type 3 object 0x202)
 *
 * ASM: RoomDraw_DrenchingWaterFace ($019D83)
 *
 * @param ctx Draw context
 */
void DrawDrenchingWaterFace(const DrawContext& ctx);

// ============================================================================
// Chest Platform Multi-Part Routines
// ============================================================================

/**
 * @brief Draw closed chest platform (Type 1 object 0xC1)
 *
 * ASM: RoomDraw_ClosedChestPlatform ($018CC7)
 * Complex multi-part structure with horizontal wall, vertical walls, corners.
 *
 * @param ctx Draw context
 */
void DrawClosedChestPlatform(const DrawContext& ctx);

/**
 * @brief Draw chest platform horizontal wall section
 *
 * ASM: RoomDraw_ChestPlatformHorizontalWallWithCorners ($018D0D)
 *
 * @param ctx Draw context
 */
void DrawChestPlatformHorizontalWall(const DrawContext& ctx);

/**
 * @brief Draw chest platform vertical wall section
 *
 * ASM: RoomDraw_ChestPlatformVerticalWall ($019E70)
 *
 * @param ctx Draw context
 */
void DrawChestPlatformVerticalWall(const DrawContext& ctx);

/**
 * @brief Register all special/miscellaneous draw routines to the registry
 *
 * @param registry Vector of DrawRoutineInfo to register routines into
 */
void RegisterSpecialRoutines(std::vector<DrawRoutineInfo>& registry);

}  // namespace draw_routines
}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_DRAW_ROUTINES_SPECIAL_ROUTINES_H
