#ifndef YAZE_ZELDA3_DUNGEON_DRAW_ROUTINES_DIAGONAL_ROUTINES_H
#define YAZE_ZELDA3_DUNGEON_DRAW_ROUTINES_DIAGONAL_ROUTINES_H

#include "draw_routine_types.h"

namespace yaze {
namespace zelda3 {
namespace draw_routines {

/**
 * @brief Draw diagonal acute (/) pattern - draws 5 tiles vertically, moves
 * up-right
 *
 * Based on bank_01.asm RoomDraw_DiagonalAcute_1to16 at $018C58
 * Uses RoomDraw_2x2and1 to draw 5 tiles at rows 0-4, then moves Y -= $7E
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawDiagonalAcute_1to16(const DrawContext& ctx);

/**
 * @brief Draw diagonal grave (\) pattern - draws 5 tiles vertically, moves
 * down-right
 *
 * Based on bank_01.asm RoomDraw_DiagonalGrave_1to16 at $018C61
 * Uses RoomDraw_2x2and1 to draw 5 tiles at rows 0-4, then moves Y += $82
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawDiagonalGrave_1to16(const DrawContext& ctx);

/**
 * @brief Draw diagonal acute (/) pattern for both BG layers
 *
 * Used for objects 0x15, 0x18-0x1D, 0x20
 * Based on bank_01.asm RoomDraw_DiagonalAcute_1to16_BothBG at $018C6A
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawDiagonalAcute_1to16_BothBG(const DrawContext& ctx);

/**
 * @brief Draw diagonal grave (\) pattern for both BG layers
 *
 * Used for objects 0x16-0x17, 0x1A-0x1B, 0x1E-0x1F
 * Based on bank_01.asm RoomDraw_DiagonalGrave_1to16_BothBG at $018CB9
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawDiagonalGrave_1to16_BothBG(const DrawContext& ctx);

/**
 * @brief Register all diagonal draw routines to the registry
 *
 * @param registry Vector of DrawRoutineInfo to register routines into
 */
void RegisterDiagonalRoutines(std::vector<DrawRoutineInfo>& registry);

}  // namespace draw_routines
}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_DRAW_ROUTINES_DIAGONAL_ROUTINES_H
