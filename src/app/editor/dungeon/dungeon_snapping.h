#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_SNAPPING_H
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_SNAPPING_H

#include <algorithm>
#include <cmath>
#include "imgui/imgui.h"

namespace yaze::editor::snapping {

/**
 * @brief Snapping grids for different dungeon elements
 */
enum class GridType {
    Object,   // 8px (1 tile)
    Sprite,   // 16px (2x2 tiles)
    Item,     // 4px horizontal, 16px vertical
    Door      // Snap to walls (handled by DoorPositionManager)
};

/**
 * @brief Snap a pixel coordinate to the 8px tile grid
 */
inline int SnapToTile(int pixel) {
    return (pixel / 8) * 8;
}

/**
 * @brief Snap a pixel coordinate to a specific grid size
 */
inline int SnapToGrid(int pixel, int grid_size) {
    if (grid_size <= 0) return pixel;
    return (pixel / grid_size) * grid_size;
}

/**
 * @brief Snap a point to the 8px tile grid
 */
inline ImVec2 SnapToTileGrid(const ImVec2& point) {
    return ImVec2(static_cast<float>(SnapToTile(static_cast<int>(point.x))),
                  static_cast<float>(SnapToTile(static_cast<int>(point.y))));
}

/**
 * @brief Snap a point for Pot Items (X: 4px, Y: 16px)
 */
inline ImVec2 SnapToItemGrid(const ImVec2& point) {
    return ImVec2(static_cast<float>(SnapToGrid(static_cast<int>(point.x), 4)),
                  static_cast<float>(SnapToGrid(static_cast<int>(point.y), 16)));
}

/**
 * @brief Snap a point for Sprites (16x16px)
 */
inline ImVec2 SnapToSpriteGrid(const ImVec2& point) {
    return ImVec2(static_cast<float>(SnapToGrid(static_cast<int>(point.x), 16)),
                  static_cast<float>(SnapToGrid(static_cast<int>(point.y), 16)));
}

} // namespace yaze::editor::snapping

#endif // YAZE_APP_EDITOR_DUNGEON_DUNGEON_SNAPPING_H
