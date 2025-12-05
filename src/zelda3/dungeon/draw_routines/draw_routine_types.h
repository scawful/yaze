#ifndef YAZE_ZELDA3_DUNGEON_DRAW_ROUTINES_DRAW_ROUTINE_TYPES_H
#define YAZE_ZELDA3_DUNGEON_DRAW_ROUTINES_DRAW_ROUTINE_TYPES_H

#include <cstdint>
#include <functional>
#include <span>
#include <string>

#include "app/gfx/render/background_buffer.h"
#include "app/gfx/types/snes_tile.h"
#include "rom/rom.h"

namespace yaze {
namespace zelda3 {

// Forward declarations
class DungeonState;
struct RoomObject;

/**
 * @brief Context passed to draw routines containing all necessary state
 *
 * This replaces the ObjectDrawer* parameter pattern, making routines
 * pure functions that don't depend on class instance state.
 */
struct DrawContext {
  gfx::BackgroundBuffer& target_bg;          // Primary buffer to draw to
  const RoomObject& object;                   // Object being drawn
  std::span<const gfx::TileInfo> tiles;       // Tile data for the object
  const DungeonState* state;                  // Dungeon state (chest states, etc.)
  Rom* rom;                                   // ROM for additional data lookup
  int room_id;                                // Current room ID
  const uint8_t* room_gfx_buffer;             // Room-specific graphics buffer
  gfx::BackgroundBuffer* secondary_bg;       // Secondary BG for dual-layer routines (nullable)

  // Canvas dimensions
  static constexpr int kMaxTilesX = 64;
  static constexpr int kMaxTilesY = 64;

  // Helper to check if dual-BG drawing is available
  bool HasSecondaryBG() const { return secondary_bg != nullptr; }
};

/**
 * @brief Function signature for a draw routine
 *
 * All draw routines are static functions with this signature.
 * They receive all context via DrawContext and write tiles to target_bg.
 */
using DrawRoutineFn = std::function<void(const DrawContext& ctx)>;

/**
 * @brief Metadata about a draw routine
 */
struct DrawRoutineInfo {
  int id;                           // Routine ID (0-39)
  std::string name;                 // Human-readable name
  DrawRoutineFn function;           // The actual draw function
  bool draws_to_both_bgs;           // If true, draws to BG1 and BG2
  int base_width;                   // Base width in tiles (0 = variable)
  int base_height;                  // Base height in tiles (0 = variable)

  // Category for organization
  enum class Category {
    Special,     // Chest, Nothing, Custom, DoorSwitcher
    Rightwards,  // Horizontal extension patterns
    Downwards,   // Vertical extension patterns
    Diagonal,    // Diagonal patterns
    Corner       // Corner and special shape patterns
  };
  Category category;
};

/**
 * @brief Utility functions for tile writing used by all routines
 */
namespace DrawRoutineUtils {

/**
 * @brief Check if tile position is within canvas bounds
 */
inline bool IsValidTilePosition(int tile_x, int tile_y) {
  return tile_x >= 0 && tile_x < DrawContext::kMaxTilesX && tile_y >= 0 &&
         tile_y < DrawContext::kMaxTilesY;
}

/**
 * @brief Write an 8x8 tile to the background buffer
 *
 * @param bg Background buffer to write to
 * @param tile_x Tile X coordinate (0-63)
 * @param tile_y Tile Y coordinate (0-63)
 * @param tile_info Tile information (ID, palette, flip flags)
 */
void WriteTile8(gfx::BackgroundBuffer& bg, int tile_x, int tile_y,
                const gfx::TileInfo& tile_info);

/**
 * @brief Draw a 2x2 block of tiles (16x16 pixels)
 */
void DrawBlock2x2(gfx::BackgroundBuffer& bg, int tile_x, int tile_y,
                  std::span<const gfx::TileInfo> tiles, int offset = 0);

/**
 * @brief Draw a 2x4 block of tiles (16x32 pixels)
 */
void DrawBlock2x4(gfx::BackgroundBuffer& bg, int tile_x, int tile_y,
                  std::span<const gfx::TileInfo> tiles, int offset = 0);

/**
 * @brief Draw a 4x2 block of tiles (32x16 pixels)
 */
void DrawBlock4x2(gfx::BackgroundBuffer& bg, int tile_x, int tile_y,
                  std::span<const gfx::TileInfo> tiles, int offset = 0);

/**
 * @brief Draw a 4x4 block of tiles (32x32 pixels)
 */
void DrawBlock4x4(gfx::BackgroundBuffer& bg, int tile_x, int tile_y,
                  std::span<const gfx::TileInfo> tiles, int offset = 0);

}  // namespace DrawRoutineUtils

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_DRAW_ROUTINES_DRAW_ROUTINE_TYPES_H
