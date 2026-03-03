#ifndef YAZE_ZELDA3_DUNGEON_DRAW_ROUTINES_RIGHTWARDS_ROUTINES_H
#define YAZE_ZELDA3_DUNGEON_DRAW_ROUTINES_RIGHTWARDS_ROUTINES_H

#include "draw_routine_types.h"

namespace yaze {
namespace zelda3 {
namespace draw_routines {

/**
 * @brief Draw 2x2 tiles rightward (1-15 or 32 repetitions)
 *
 * Pattern: Draws 2x2 tiles rightward (object 0x00)
 * Size byte determines how many times to repeat (1-15 or 32)
 * ROM tile order is COLUMN-MAJOR: [col0_row0, col0_row1, col1_row0, col1_row1]
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawRightwards2x2_1to15or32(const DrawContext& ctx);

/**
 * @brief Draw 2x4 tiles rightward (1-15 or 26 repetitions)
 *
 * Pattern: Draws 2x4 tiles rightward (objects 0x01-0x02)
 * Uses RoomDraw_Nx4 with N=2, tiles are COLUMN-MAJOR:
 * [col0_row0, col0_row1, col0_row2, col0_row3, col1_row0, col1_row1, col1_row2, col1_row3]
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawRightwards2x4_1to15or26(const DrawContext& ctx);

/**
 * @brief Draw 2x4 tiles rightward with adjacent spacing (1-16 repetitions)
 *
 * Pattern: Draws 2x4 tiles rightward with adjacent spacing (objects 0x03-0x04)
 * Uses RoomDraw_Nx4 with N=2, tiles are COLUMN-MAJOR
 * ASM: GetSize_1to16 means count = size + 1
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawRightwards2x4_1to16(const DrawContext& ctx);

/**
 * @brief Draw 2x4 tiles rightward with adjacent spacing to both BG layers
 *
 * Pattern: Same as DrawRightwards2x4_1to16 but draws to both BG1 and BG2 (objects 0x05-0x06)
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawRightwards2x4_1to16_BothBG(const DrawContext& ctx);

/**
 * @brief Draw 2x2 tiles rightward (1-16 repetitions)
 *
 * Pattern: Draws 2x2 tiles rightward (objects 0x07-0x08)
 * ROM tile order is COLUMN-MAJOR: [col0_row0, col0_row1, col1_row0, col1_row1]
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawRightwards2x2_1to16(const DrawContext& ctx);

/**
 * @brief Draw 1x3 tiles rightward with caps
 *
 * Pattern: 1x3 tiles rightward with caps (object 0x21)
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawRightwards1x2_1to16_plus2(const DrawContext& ctx);

/**
 * @brief Draw 1x1 tiles with edge detection +3
 *
 * Pattern: 1x1 tiles with edge detection +3 (object 0x22)
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawRightwardsHasEdge1x1_1to16_plus3(const DrawContext& ctx);

/**
 * @brief Draw 1x1 tiles with edge detection +2
 *
 * Pattern: 1x1 tiles with edge detection +2 (objects 0x23-0x2E, 0x3F-0x46)
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawRightwardsHasEdge1x1_1to16_plus2(const DrawContext& ctx);

/**
 * @brief Draw 1x1 tiles with edge detection +23
 *
 * Pattern: 1x1 tiles with edge detection +23 (object 0x5F)
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawRightwardsHasEdge1x1_1to16_plus23(const DrawContext& ctx);

/**
 * @brief Draw top corner 1x2 tiles with +13 offset
 *
 * Pattern: Top corner 1x2 tiles with +13 offset (object 0x2F)
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawRightwardsTopCorners1x2_1to16_plus13(const DrawContext& ctx);

/**
 * @brief Draw bottom corner 1x2 tiles with +13 offset
 *
 * Pattern: Bottom corner 1x2 tiles with +13 offset (object 0x30)
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawRightwardsBottomCorners1x2_1to16_plus13(const DrawContext& ctx);

/**
 * @brief Draw 4x4 block rightward
 *
 * Pattern: 4x4 block rightward (object 0x33)
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawRightwards4x4_1to16(const DrawContext& ctx);

/**
 * @brief Draw 1x1 solid tiles +3 offset
 *
 * Pattern: 1x1 solid tiles +3 offset (object 0x34)
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawRightwards1x1Solid_1to16_plus3(const DrawContext& ctx);

/**
 * @brief Draw 4x4 decoration with spacing
 *
 * Pattern: 4x4 decoration with spacing (objects 0x36-0x37)
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawRightwardsDecor4x4spaced2_1to16(const DrawContext& ctx);

/**
 * @brief Draw 2x3 statue with spacing
 *
 * Pattern: 2x3 statue with spacing (object 0x38)
 * 2 columns × 3 rows = 6 tiles in COLUMN-MAJOR order
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawRightwardsStatue2x3spaced2_1to16(const DrawContext& ctx);

/**
 * @brief Draw 2x4 pillar with spacing
 *
 * Pattern: 2x4 pillar with spacing (objects 0x39, 0x3D)
 * 2 columns × 4 rows = 8 tiles in COLUMN-MAJOR order
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawRightwardsPillar2x4spaced4_1to16(const DrawContext& ctx);

/**
 * @brief Draw 4x3 decoration with spacing
 *
 * Pattern: 4x3 decoration with spacing (objects 0x3A-0x3B)
 * 4 columns × 3 rows = 12 tiles in COLUMN-MAJOR order
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawRightwardsDecor4x3spaced4_1to16(const DrawContext& ctx);

/**
 * @brief Draw doubled 2x2 with spacing
 *
 * Pattern: Doubled 2x2 with spacing (object 0x3C)
 * 4 columns × 2 rows = 8 tiles in COLUMN-MAJOR order
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawRightwardsDoubled2x2spaced2_1to16(const DrawContext& ctx);

/**
 * @brief Draw 2x2 decoration with large spacing
 *
 * Pattern: 2x2 decoration with large spacing (object 0x3E)
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawRightwardsDecor2x2spaced12_1to16(const DrawContext& ctx);

/**
 * @brief Draw 4x2 floor tiles rightward
 *
 * Pattern: Draws 4x2 tiles in ROW-MAJOR order (objects 0x49-0x4A)
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawRightwards4x2_1to16(const DrawContext& ctx);

/**
 * @brief Draw 1x8 decorative columns with wide horizontal spacing
 *
 * Pattern: Draws one tile-wide, eight tile-high columns (objects 0x55-0x56)
 * with 12-tile horizontal step.
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawRightwardsDecor4x2spaced8_1to16(const DrawContext& ctx);

/**
 * @brief Draw cannon-hole spans with repeated left segment and right cap
 *
 * Pattern: Draws 4x3 cannon-hole tiles (objects 0x51-0x52, 0x5B-0x5C)
 * using the usdasm repeat-and-cap behavior.
 *
 * @param ctx Draw context containing object, tiles, and target buffer
 */
void DrawRightwardsCannonHole4x3_1to16(const DrawContext& ctx);

/**
 * @brief Draw rightwards 1x1 line with +1 extension
 */
void DrawRightwardsLine1x1_1to16plus1(const DrawContext& ctx);

/**
 * @brief Draw rightwards 4x3 bar pattern
 */
void DrawRightwardsBar4x3_1to16(const DrawContext& ctx);

/**
 * @brief Draw rightwards 4x4 shelf pattern
 */
void DrawRightwardsShelf4x4_1to16(const DrawContext& ctx);

/**
 * @brief Draw rightwards big rail pattern (1x3 columns with +5 extension)
 */
void DrawRightwardsBigRail1x3_1to16plus5(const DrawContext& ctx);

/**
 * @brief Draw rightwards 2x2 blocks with 2-tile spacing
 */
void DrawRightwardsBlock2x2spaced2_1to16(const DrawContext& ctx);

/**
 * @brief Draw rightwards 1x1 edge line with +7 extension
 */
void DrawRightwardsEdge1x1_1to16plus7(const DrawContext& ctx);

/**
 * @brief Draw rightwards 2x2 pot groups
 */
void DrawRightwardsPots2x2_1to16(const DrawContext& ctx);

/**
 * @brief Draw rightwards 2x2 hammer-peg groups
 */
void DrawRightwardsHammerPegs2x2_1to16(const DrawContext& ctx);

/**
 * @brief Register all rightwards draw routines to the registry
 *
 * @param registry Vector of DrawRoutineInfo to register routines into
 */
void RegisterRightwardsRoutines(std::vector<DrawRoutineInfo>& registry);

}  // namespace draw_routines
}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_DRAW_ROUTINES_RIGHTWARDS_ROUTINES_H
