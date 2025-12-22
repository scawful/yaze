#ifndef YAZE_ZELDA3_DUNGEON_DRAW_ROUTINES_DOWNWARDS_ROUTINES_H
#define YAZE_ZELDA3_DUNGEON_DRAW_ROUTINES_DOWNWARDS_ROUTINES_H

#include "draw_routine_types.h"

namespace yaze {
namespace zelda3 {
namespace draw_routines {

/**
 * @brief Draw 2x2 tiles downward pattern (1-15 or 32 iterations)
 *
 * Pattern: Draws 2x2 tiles downward (object 0x60)
 * Size byte determines how many times to repeat (1-15 or 32)
 * Based on bank_01.asm RoomDraw_Downwards2x2_1to15or32
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawDownwards2x2_1to15or32(const DrawContext& ctx);

/**
 * @brief Draw 4x2 tiles downward pattern (1-15 or 26 iterations)
 *
 * Pattern: Draws 4x2 tiles downward (objects 0x61-0x62)
 * This is 4 columns × 2 rows = 8 tiles in ROW-MAJOR order (per ZScream)
 * Based on bank_01.asm RoomDraw_Downwards4x2_1to15or26
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawDownwards4x2_1to15or26(const DrawContext& ctx);

/**
 * @brief Draw 4x2 tiles downward pattern for both BG layers
 *
 * Pattern: Same as above but draws to both BG1 and BG2 (objects 0x63-0x64)
 * Based on bank_01.asm RoomDraw_Downwards4x2_1to16_BothBG
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawDownwards4x2_1to16_BothBG(const DrawContext& ctx);

/**
 * @brief Draw 4x2 decoration downward with spacing (1-16 iterations)
 *
 * Pattern: Draws 4x2 decoration downward with spacing (objects 0x65-0x66)
 * This is 4 columns × 2 rows = 8 tiles in COLUMN-MAJOR order with 6-tile Y
 * spacing Based on bank_01.asm RoomDraw_DownwardsDecor4x2spaced4_1to16
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawDownwardsDecor4x2spaced4_1to16(const DrawContext& ctx);

/**
 * @brief Draw 2x2 tiles downward pattern (1-16 iterations)
 *
 * Pattern: Draws 2x2 tiles downward (objects 0x67-0x68)
 * Based on bank_01.asm RoomDraw_Downwards2x2_1to16
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawDownwards2x2_1to16(const DrawContext& ctx);

/**
 * @brief Draw 1x1 tiles with edge detection +3 offset downward
 *
 * Pattern: 1x1 tiles with edge detection +3 offset downward (object 0x69)
 * Based on bank_01.asm RoomDraw_DownwardsHasEdge1x1_1to16_plus3
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawDownwardsHasEdge1x1_1to16_plus3(const DrawContext& ctx);

/**
 * @brief Draw 1x1 edge tiles downward (1-16 iterations)
 *
 * Pattern: 1x1 edge tiles downward (objects 0x6A-0x6B)
 * Based on bank_01.asm RoomDraw_DownwardsEdge1x1_1to16
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawDownwardsEdge1x1_1to16(const DrawContext& ctx);

/**
 * @brief Draw left corner 2x1 tiles with +12 offset downward
 *
 * Pattern: Left corner 2x1 tiles with +12 offset downward (object 0x6C)
 * Based on bank_01.asm RoomDraw_DownwardsLeftCorners2x1_1to16_plus12
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawDownwardsLeftCorners2x1_1to16_plus12(const DrawContext& ctx);

/**
 * @brief Draw right corner 2x1 tiles with +12 offset downward
 *
 * Pattern: Right corner 2x1 tiles with +12 offset downward (object 0x6D)
 * Based on bank_01.asm RoomDraw_DownwardsRightCorners2x1_1to16_plus12
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawDownwardsRightCorners2x1_1to16_plus12(const DrawContext& ctx);

/**
 * @brief Register all downwards draw routines to the registry
 *
 * @param registry Vector of DrawRoutineInfo to register routines into
 */
void RegisterDownwardsRoutines(std::vector<DrawRoutineInfo>& registry);

}  // namespace draw_routines
}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_DRAW_ROUTINES_DOWNWARDS_ROUTINES_H
