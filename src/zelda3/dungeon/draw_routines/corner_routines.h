#ifndef YAZE_ZELDA3_DUNGEON_DRAW_ROUTINES_CORNER_ROUTINES_H
#define YAZE_ZELDA3_DUNGEON_DRAW_ROUTINES_CORNER_ROUTINES_H

#include "draw_routine_types.h"

namespace yaze {
namespace zelda3 {
namespace draw_routines {

// Note: DrawWaterFace is declared in special_routines.h

/**
 * @brief Draw a 4x4 grid corner pattern
 *
 * Type 2 corners 0x40-0x4F, 0x108-0x10F
 * Supports multiple tile counts:
 * - 16 tiles: Full 4x4 pattern (column-major)
 * - 8 tiles: 2x4 pattern (column-major, Type 2 standard layout)
 * - 4 tiles: 2x2 fallback pattern
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawCorner4x4(const DrawContext& ctx);

/**
 * @brief Draw a 4x4 corner for both BG layers
 *
 * Used by objects 0x108-0x10F for Type 2, 0xF9B-0xF9D for Type 3
 * Delegates to DrawCorner4x4 for 16 tiles, or draws:
 * - 8 tiles: 2x4 corner pattern (column-major)
 * - 4 tiles: 2x2 pattern via DrawWaterFace
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void Draw4x4Corner_BothBG(const DrawContext& ctx);

/**
 * @brief Draw a weird corner bottom pattern for both BG layers
 *
 * Used by objects 0x110-0x113 for Type 2, 0xF9E-0xFA1 for Type 3
 * Type 3 objects use 8 tiles in 4x2 bottom corner layout
 * Draws in row-major order for bottom corners
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawWeirdCornerBottom_BothBG(const DrawContext& ctx);

/**
 * @brief Draw a weird corner top pattern for both BG layers
 *
 * Used by objects 0x114-0x117 for Type 2, 0xFA2-0xFA5 for Type 3
 * Type 3 objects use 8 tiles in 4x2 top corner layout
 * Draws in row-major order for top corners
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawWeirdCornerTop_BothBG(const DrawContext& ctx);

/**
 * @brief Register all corner draw routines to the registry
 *
 * @param registry Vector of DrawRoutineInfo to register routines into
 */
void RegisterCornerRoutines(std::vector<DrawRoutineInfo>& registry);

}  // namespace draw_routines
}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_DRAW_ROUTINES_CORNER_ROUTINES_H
