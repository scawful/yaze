#include "special_routines.h"

#include "util/log.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/dungeon_state.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {
namespace draw_routines {

void DrawChest(const DrawContext& ctx, int chest_index) {
  // Determine if chest is open
  bool is_open = false;
  if (ctx.state) {
    is_open = ctx.state->IsChestOpen(ctx.room_id, chest_index);
  }

  // Draw logic
  // Heuristic: If we have extra tiles loaded, assume they are for the open
  // state. Standard chests are 2x2 (4 tiles). Big chests are 4x4 (16 tiles). If
  // we have double the tiles, use the second half for open state.

  if (is_open) {
    if (ctx.tiles.size() >= 32) {
      // Big chest open tiles (indices 16-31)
      // Draw 4x4 pattern
      for (int x = 0; x < 4; ++x) {
        for (int y = 0; y < 4; ++y) {
          DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + x,
                                       ctx.object.y_ + y,
                                       ctx.tiles[16 + x * 4 + y]);
        }
      }
      return;
    }
    if (ctx.tiles.size() >= 8 && ctx.tiles.size() < 16) {
      // Small chest open tiles (indices 4-7)
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, ctx.object.y_,
                                   ctx.tiles[4]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_,
                                   ctx.object.y_ + 1, ctx.tiles[5]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1,
                                   ctx.object.y_, ctx.tiles[6]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1,
                                   ctx.object.y_ + 1, ctx.tiles[7]);
      return;
    }
    // If no extra tiles, fall through to draw closed chest (better than
    // nothing)
  }

  // Fallback to standard 4x4 or 2x2 drawing based on tile count
  if (ctx.tiles.size() >= 16) {
    // Draw 4x4 pattern in COLUMN-MAJOR order
    for (int x = 0; x < 4; ++x) {
      for (int y = 0; y < 4; ++y) {
        DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + x,
                                     ctx.object.y_ + y, ctx.tiles[x * 4 + y]);
      }
    }
  } else if (ctx.tiles.size() >= 4) {
    // Draw 2x2 pattern in COLUMN-MAJOR order
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, ctx.object.y_,
                                 ctx.tiles[0]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_,
                                 ctx.object.y_ + 1, ctx.tiles[1]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1,
                                 ctx.object.y_, ctx.tiles[2]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1,
                                 ctx.object.y_ + 1, ctx.tiles[3]);
  }
}

void DrawNothing([[maybe_unused]] const DrawContext& ctx) {
  // Intentionally empty - represents invisible logic objects or placeholders
  // ASM: RoomDraw_Nothing_A ($0190F2), RoomDraw_Nothing_B ($01932E), etc.
  // These routines typically just RTS.
}

void CustomDraw(const DrawContext& ctx) {
  // Pattern: Custom draw routine (objects 0x31-0x32)
  // For now, fall back to simple 1x1
  if (ctx.tiles.size() >= 1) {
    // Use first 8x8 tile from span
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, ctx.object.y_,
                                 ctx.tiles[0]);
  }
}

void DrawDoorSwitcherer(const DrawContext& ctx) {
  // Pattern: Door switcher (object 0x35)
  // Special door logic
  // Check state to decide graphic
  int tile_index = 0;
  if (ctx.state && ctx.state->IsDoorSwitchActive(ctx.room_id)) {
    // Use active tile if available (assuming 2nd tile is active state)
    if (ctx.tiles.size() >= 2) {
      tile_index = 1;
    }
  }

  if (ctx.tiles.size() > static_cast<size_t>(tile_index)) {
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, ctx.object.y_,
                                 ctx.tiles[tile_index]);
  }
}

void DrawSomariaLine(const DrawContext& ctx) {
  // Pattern: Somaria Line (objects 0x203-0x20F, 0x214)
  // Draws a line of tiles based on direction encoded in object ID
  // Direction mapping based on ZScream reference:
  //   0x03: Horizontal right
  //   0x04: Vertical down
  //   0x05: Diagonal down-right
  //   0x06: Diagonal down-left
  //   0x07-0x09: Variations
  //   0x0A-0x0C: More variations
  //   0x0E-0x0F: Additional patterns
  //   0x14: Another line type

  if (ctx.tiles.empty()) return;

  int length = (ctx.object.size_ & 0x0F) + 1;
  int obj_subid = ctx.object.id_ & 0x0F;  // Low nibble determines direction

  // Determine direction based on object sub-ID
  int dx = 1, dy = 0;  // Default: horizontal right
  switch (obj_subid) {
    case 0x03:
      dx = 1;
      dy = 0;
      break;  // Horizontal right
    case 0x04:
      dx = 0;
      dy = 1;
      break;  // Vertical down
    case 0x05:
      dx = 1;
      dy = 1;
      break;  // Diagonal down-right
    case 0x06:
      dx = -1;
      dy = 1;
      break;  // Diagonal down-left
    case 0x07:
      dx = 1;
      dy = 0;
      break;  // Horizontal (variant)
    case 0x08:
      dx = 0;
      dy = 1;
      break;  // Vertical (variant)
    case 0x09:
      dx = 1;
      dy = 1;
      break;  // Diagonal (variant)
    case 0x0A:
      dx = 1;
      dy = 0;
      break;  // Horizontal
    case 0x0B:
      dx = 0;
      dy = 1;
      break;  // Vertical
    case 0x0C:
      dx = 1;
      dy = 1;
      break;  // Diagonal
    case 0x0E:
      dx = 1;
      dy = 0;
      break;  // Horizontal
    case 0x0F:
      dx = 0;
      dy = 1;
      break;  // Vertical
    default:
      dx = 1;
      dy = 0;
      break;  // Default horizontal
  }

  // Draw tiles along the path using first tile (Somaria uses single tile)
  for (int i = 0; i < length; ++i) {
    size_t tile_idx = i % ctx.tiles.size();  // Cycle through tiles if multiple
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (i * dx),
                                 ctx.object.y_ + (i * dy), ctx.tiles[tile_idx]);
  }
}

void DrawWaterFace(const DrawContext& ctx) {
  // Pattern: Water Face (Type 3 objects 0xF80-0xF82)
  // Draws a 2x2 face in COLUMN-MAJOR order
  // TODO: Implement state check from RoomDraw_EmptyWaterFace ($019D29)
  // Checks Room ID ($AF), Room State ($7EF000), Door Flags ($0403) to switch
  // graphic
  if (ctx.tiles.size() >= 4) {
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, ctx.object.y_,
                                 ctx.tiles[0]);  // col 0, row 0
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_,
                                 ctx.object.y_ + 1,
                                 ctx.tiles[1]);  // col 0, row 1
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1,
                                 ctx.object.y_,
                                 ctx.tiles[2]);  // col 1, row 0
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1,
                                 ctx.object.y_ + 1,
                                 ctx.tiles[3]);  // col 1, row 1
  }
}

void DrawLargeCanvasObject(const DrawContext& ctx, int width, int height) {
  // Generic large object drawer
  if (ctx.tiles.size() >= static_cast<size_t>(width * height)) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + x,
                                     ctx.object.y_ + y,
                                     ctx.tiles[y * width + x]);
      }
    }
  }
}

// ============================================================================
// SuperSquare Routines (Phase 4)
// ============================================================================
// A "SuperSquare" is a 16x16 tile area (4 rows of 4 tiles each).
// These routines draw floor patterns that repeat in super square units.

void Draw4x4BlocksIn4x4SuperSquare(const DrawContext& ctx) {
  // ASM: RoomDraw_4x4BlocksIn4x4SuperSquare ($018B94)
  // Draws solid 4x4 blocks using a single tile, repeated in a grid pattern.
  // Size determines number of super squares in X and Y directions.
  int size_x = ((ctx.object.size_ >> 2) & 0x03) + 1;
  int size_y = (ctx.object.size_ & 0x03) + 1;

  LOG_DEBUG("DrawRoutines",
            "Draw4x4BlocksIn4x4SuperSquare: obj=0x%03X pos=(%d,%d) size=0x%02X "
            "tiles=%zu size_x=%d size_y=%d",
            ctx.object.id_, ctx.object.x_, ctx.object.y_, ctx.object.size_,
            ctx.tiles.size(), size_x, size_y);

  if (ctx.tiles.empty()) {
    LOG_DEBUG("DrawRoutines",
              "Draw4x4BlocksIn4x4SuperSquare: SKIPPING - no tiles loaded!");
    return;
  }

  // Use first tile for all blocks
  const auto& tile = ctx.tiles[0];
  LOG_DEBUG("DrawRoutines",
            "Draw4x4BlocksIn4x4SuperSquare: tile[0] id=%d palette=%d",
            tile.id_, tile.palette_);

  for (int sy = 0; sy < size_y; ++sy) {
    for (int sx = 0; sx < size_x; ++sx) {
      // Each super square is 4x4 tiles
      int base_x = ctx.object.x_ + (sx * 4);
      int base_y = ctx.object.y_ + (sy * 4);

      // Draw 4x4 block with same tile
      for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
          DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x, base_y + y,
                                       tile);
        }
      }
    }
  }
}

void Draw3x3FloorIn4x4SuperSquare(const DrawContext& ctx) {
  // ASM: RoomDraw_3x3FloorIn4x4SuperSquare ($018D8A)
  // Draws 3x3 floor patterns within super square units.
  int size_x = ((ctx.object.size_ >> 2) & 0x03) + 1;
  int size_y = (ctx.object.size_ & 0x03) + 1;

  if (ctx.tiles.empty()) return;

  const auto& tile = ctx.tiles[0];
  for (int sy = 0; sy < size_y; ++sy) {
    for (int sx = 0; sx < size_x; ++sx) {
      int base_x = ctx.object.x_ + (sx * 3);
      int base_y = ctx.object.y_ + (sy * 3);

      for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
          DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x, base_y + y,
                                       tile);
        }
      }
    }
  }
}

void Draw4x4FloorIn4x4SuperSquare(const DrawContext& ctx) {
  // ASM: RoomDraw_4x4FloorIn4x4SuperSquare ($018FA5)
  // Most common floor pattern - draws 4x4 floor tiles in super square units.
  // Uses RoomDraw_A_Many32x32Blocks internally in assembly.
  int size_x = ((ctx.object.size_ >> 2) & 0x03) + 1;
  int size_y = (ctx.object.size_ & 0x03) + 1;

  if (ctx.tiles.size() < 8) return;

  for (int sy = 0; sy < size_y; ++sy) {
    for (int sx = 0; sx < size_x; ++sx) {
      int base_x = ctx.object.x_ + (sx * 4);
      int base_y = ctx.object.y_ + (sy * 4);

      // Draw 4x4 pattern using 8 tiles (rows 0/2 use 0-3, rows 1/3 use 4-7)
      for (int x = 0; x < 4; ++x) {
        DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x, base_y,
                                     ctx.tiles[x]);
        DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x, base_y + 2,
                                     ctx.tiles[x]);
        DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x, base_y + 1,
                                     ctx.tiles[4 + x]);
        DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x, base_y + 3,
                                     ctx.tiles[4 + x]);
      }
    }
  }
}

void Draw4x4FloorOneIn4x4SuperSquare(const DrawContext& ctx) {
  // ASM: RoomDraw_4x4FloorOneIn4x4SuperSquare ($018FA2)
  // Single 4x4 floor pattern (starts at different tile offset in assembly).
  // For our purposes, same as 4x4FloorIn4x4SuperSquare with offset tiles.
  int size_x = ((ctx.object.size_ >> 2) & 0x03) + 1;
  int size_y = (ctx.object.size_ & 0x03) + 1;

  if (ctx.tiles.size() < 8) {
    Draw4x4FloorIn4x4SuperSquare(ctx);
    return;
  }

  for (int sy = 0; sy < size_y; ++sy) {
    for (int sx = 0; sx < size_x; ++sx) {
      int base_x = ctx.object.x_ + (sx * 4);
      int base_y = ctx.object.y_ + (sy * 4);

      for (int x = 0; x < 4; ++x) {
        DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x, base_y,
                                     ctx.tiles[x]);
        DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x, base_y + 2,
                                     ctx.tiles[x]);
        DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x, base_y + 1,
                                     ctx.tiles[4 + x]);
        DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x, base_y + 3,
                                     ctx.tiles[4 + x]);
      }
    }
  }
}

void Draw4x4FloorTwoIn4x4SuperSquare(const DrawContext& ctx) {
  // ASM: RoomDraw_4x4FloorTwoIn4x4SuperSquare ($018F9D)
  // Two 4x4 floor patterns (uses $0490 offset in assembly).
  int size_x = ((ctx.object.size_ >> 2) & 0x03) + 1;
  int size_y = (ctx.object.size_ & 0x03) + 1;

  if (ctx.tiles.size() < 8) {
    Draw4x4FloorIn4x4SuperSquare(ctx);
    return;
  }

  for (int sy = 0; sy < size_y; ++sy) {
    for (int sx = 0; sx < size_x; ++sx) {
      int base_x = ctx.object.x_ + (sx * 4);
      int base_y = ctx.object.y_ + (sy * 4);

      for (int x = 0; x < 4; ++x) {
        DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x, base_y,
                                     ctx.tiles[x]);
        DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x, base_y + 2,
                                     ctx.tiles[x]);
        DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x, base_y + 1,
                                     ctx.tiles[4 + x]);
        DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x, base_y + 3,
                                     ctx.tiles[4 + x]);
      }
    }
  }
}

void DrawBigHole4x4_1to16(const DrawContext& ctx) {
  // ASM: Object 0xA4 - Big hole pattern
  // Draws a rectangular hole with border tiles using Size as the expansion.
  int size = ctx.object.size_ & 0x0F;

  if (ctx.tiles.size() < 24) return;

  int base_x = ctx.object.x_;
  int base_y = ctx.object.y_;
  int max = size + 3;  // Bottom/right edge offset

  // Corners
  DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x, base_y, ctx.tiles[8]);
  DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + max, base_y,
                               ctx.tiles[14]);
  DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x, base_y + max,
                               ctx.tiles[17]);
  DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + max, base_y + max,
                               ctx.tiles[23]);

  // Edges and interior
  for (int xx = 1; xx < max; ++xx) {
    for (int yy = 1; yy < max; ++yy) {
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + xx, base_y + yy,
                                   ctx.tiles[0]);
    }
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + xx, base_y,
                                 ctx.tiles[10]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + xx, base_y + max,
                                 ctx.tiles[19]);
  }

  for (int yy = 1; yy < max; ++yy) {
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x, base_y + yy,
                                 ctx.tiles[9]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + max, base_y + yy,
                                 ctx.tiles[15]);
  }
}

void DrawSpike2x2In4x4SuperSquare(const DrawContext& ctx) {
  // ASM: RoomDraw_Spike2x2In4x4SuperSquare ($019708)
  // Draws 2x2 spike patterns in a tiled grid
  int size_x = ((ctx.object.size_ >> 2) & 0x03) + 1;
  int size_y = (ctx.object.size_ & 0x03) + 1;

  if (ctx.tiles.size() < 4) return;

  for (int sy = 0; sy < size_y; ++sy) {
    for (int sx = 0; sx < size_x; ++sx) {
      int base_x = ctx.object.x_ + (sx * 2);
      int base_y = ctx.object.y_ + (sy * 2);

      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x, base_y,
                                   ctx.tiles[0]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, base_y,
                                   ctx.tiles[2]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x, base_y + 1,
                                   ctx.tiles[1]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, base_y + 1,
                                   ctx.tiles[3]);
    }
  }
}

void DrawTableRock4x4_1to16(const DrawContext& ctx) {
  // ASM: Object 0xDD - Table rock pattern
  int size_x = ((ctx.object.size_ >> 2) & 0x03);
  int size_y = (ctx.object.size_ & 0x03);

  if (ctx.tiles.size() < 16) return;

  int right_x = ctx.object.x_ + (3 + (size_x * 2));
  int bottom_y = ctx.object.y_ + (3 + (size_y * 2));

  // Interior
  for (int xx = 0; xx < size_x + 1; ++xx) {
    for (int yy = 0; yy < size_y + 1; ++yy) {
      int base_x = ctx.object.x_ + (xx * 2);
      int base_y = ctx.object.y_ + (yy * 2);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, base_y + 1,
                                   ctx.tiles[5]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 2, base_y + 1,
                                   ctx.tiles[6]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, base_y + 2,
                                   ctx.tiles[9]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 2, base_y + 2,
                                   ctx.tiles[10]);
    }
  }

  // Left/right borders
  for (int yy = 0; yy < size_y + 1; ++yy) {
    int base_y = ctx.object.y_ + (yy * 2);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, base_y + 1,
                                 ctx.tiles[4]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, base_y + 2,
                                 ctx.tiles[8]);

    DrawRoutineUtils::WriteTile8(ctx.target_bg, right_x, base_y + 1,
                                 ctx.tiles[7]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, right_x, base_y + 2,
                                 ctx.tiles[11]);
  }

  // Top/bottom borders
  for (int xx = 0; xx < size_x + 1; ++xx) {
    int base_x = ctx.object.x_ + (xx * 2);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, ctx.object.y_,
                                 ctx.tiles[1]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 2, ctx.object.y_,
                                 ctx.tiles[2]);

    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, bottom_y,
                                 ctx.tiles[13]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 2, bottom_y,
                                 ctx.tiles[14]);
  }

  // Corners
  DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, ctx.object.y_,
                               ctx.tiles[0]);
  DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, bottom_y,
                               ctx.tiles[12]);
  DrawRoutineUtils::WriteTile8(ctx.target_bg, right_x, ctx.object.y_,
                               ctx.tiles[3]);
  DrawRoutineUtils::WriteTile8(ctx.target_bg, right_x, bottom_y,
                               ctx.tiles[15]);
}

void DrawWaterOverlay8x8_1to16(const DrawContext& ctx) {
  // ASM: RoomDraw_WaterOverlayA8x8_1to16 ($0195D6) / RoomDraw_WaterOverlayB8x8
  // NOTE: In the original game, this is an HDMA control object that sets up
  // the wavy water distortion effect. It doesn't draw tiles directly.
  // For the editor, we draw the available tile data as a visual indicator.
  
  int size_x = ((ctx.object.size_ >> 2) & 0x03);
  int size_y = (ctx.object.size_ & 0x03);

  if (ctx.tiles.size() < 8) return;

  int count_x = size_x + 2;
  int count_y = size_y + 2;

  for (int yy = 0; yy < count_y; ++yy) {
    for (int xx = 0; xx < count_x; ++xx) {
      int base_x = ctx.object.x_ + (xx * 4);
      int base_y = ctx.object.y_ + (yy * 4);

      for (int x = 0; x < 4; ++x) {
        DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x, base_y,
                                     ctx.tiles[x]);
        DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x, base_y + 2,
                                     ctx.tiles[x]);
        DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x, base_y + 1,
                                     ctx.tiles[4 + x]);
        DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x, base_y + 3,
                                     ctx.tiles[4 + x]);
      }
    }
  }
}

// ============================================================================
// Stair Routines
// ============================================================================

void DrawInterRoomFatStairsUp(const DrawContext& ctx) {
  // ASM: RoomDraw_InterRoomFatStairsUp ($01A41B)
  // Uses tile data at obj1088, draws 4x4 pattern
  // In original game, registers position in $06B0 for transition handling
  // For editor display, we just draw the visual representation

  if (ctx.tiles.size() < 16) return;

  // Draw 4x4 stair pattern
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      size_t tile_idx = static_cast<size_t>(y * 4 + x);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + x,
                                   ctx.object.y_ + y, ctx.tiles[tile_idx]);
    }
  }
}

void DrawInterRoomFatStairsDownA(const DrawContext& ctx) {
  // ASM: RoomDraw_InterRoomFatStairsDownA ($01A458)
  // Uses tile data at obj10A8
  DrawInterRoomFatStairsUp(ctx);  // Same visual structure
}

void DrawInterRoomFatStairsDownB(const DrawContext& ctx) {
  // ASM: RoomDraw_InterRoomFatStairsDownB ($01A486)
  // Uses tile data at obj10A8
  DrawInterRoomFatStairsUp(ctx);  // Same visual structure
}

void DrawSpiralStairs(const DrawContext& ctx, bool going_up, bool is_upper) {
  // ASM: RoomDraw_SpiralStairsGoingUpUpper, etc.
  // Calls RoomDraw_1x3N_rightwards with A=4 -> 4 columns x 3 rows = 12 tiles
  // Tile order is COLUMN-MAJOR (down first, then right)
  (void)going_up;
  (void)is_upper;

  if (ctx.tiles.size() < 12) return;

  // Draw 4x3 pattern in COLUMN-MAJOR order (matching ASM)
  int tid = 0;
  for (int x = 0; x < 4; ++x) {
    for (int y = 0; y < 3; ++y) {
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + x,
                                   ctx.object.y_ + y, ctx.tiles[tid++]);
    }
  }
}

void DrawAutoStairs(const DrawContext& ctx) {
  // ASM: RoomDraw_AutoStairs* routines
  // Multi-layer or merged layer stair patterns
  if (ctx.tiles.size() < 16) return;

  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      size_t tile_idx = static_cast<size_t>(y * 4 + x);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + x,
                                   ctx.object.y_ + y, ctx.tiles[tile_idx]);
    }
  }
}

void DrawStraightInterRoomStairs(const DrawContext& ctx) {
  // ASM: RoomDraw_StraightInterroomStairs* routines
  // North/South, Up/Down variants
  if (ctx.tiles.size() < 16) return;

  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      size_t tile_idx = static_cast<size_t>(y * 4 + x);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + x,
                                   ctx.object.y_ + y, ctx.tiles[tile_idx]);
    }
  }
}

// ============================================================================
// Interactive Object Routines
// ============================================================================

void DrawPrisonCell(const DrawContext& ctx) {
  // ASM: RoomDraw_PrisonCell ($019C44)
  // Draws prison cell bars to BOTH BG layers with horizontal flip for symmetry
  // The ASM writes to $7E2xxx (BG1) and also uses ORA #$4000 for horizontal flip
  // Pattern: 5 iterations drawing a complex bar pattern

  if (ctx.tiles.size() < 6) return;

  // Prison cell layout based on ASM analysis:
  // The routine draws 5 columns of bars, each with specific tile patterns
  // Tiles at positions: (x, y), (x+7, y) for outer bars
  // Middle bars with horizontal flip on one side

  int base_x = ctx.object.x_;
  int base_y = ctx.object.y_;

  // Draw the prison cell pattern - 5 vertical bar segments
  for (int col = 0; col < 5; ++col) {
    int x_offset = col;

    // Each column has 4 rows of tiles
    for (int row = 0; row < 4; ++row) {
      size_t tile_idx = (row < static_cast<int>(ctx.tiles.size())) ? row : 0;

      // Left side bar
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x_offset,
                                   base_y + row, ctx.tiles[tile_idx]);

      // Right side bar (mirrored horizontally)
      auto mirrored_tile = ctx.tiles[tile_idx];
      mirrored_tile.horizontal_mirror_ = !mirrored_tile.horizontal_mirror_;
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 9 - x_offset,
                                   base_y + row, mirrored_tile);
    }
  }

  // If we have a secondary BG buffer, draw the same pattern there
  // This ensures the prison bars appear on both background layers
  if (ctx.HasSecondaryBG()) {
    for (int col = 0; col < 5; ++col) {
      int x_offset = col;

      for (int row = 0; row < 4; ++row) {
        size_t tile_idx = (row < static_cast<int>(ctx.tiles.size())) ? row : 0;

        // Left side bar
        DrawRoutineUtils::WriteTile8(*ctx.secondary_bg, base_x + x_offset,
                                     base_y + row, ctx.tiles[tile_idx]);

        // Right side bar (mirrored)
        auto mirrored_tile = ctx.tiles[tile_idx];
        mirrored_tile.horizontal_mirror_ = !mirrored_tile.horizontal_mirror_;
        DrawRoutineUtils::WriteTile8(*ctx.secondary_bg, base_x + 9 - x_offset,
                                     base_y + row, mirrored_tile);
      }
    }
  }
}

void DrawBigKeyLock(const DrawContext& ctx) {
  // ASM: RoomDraw_BigKeyLock ($0198AE)
  // Checks room flags via RoomFlagMask to see if lock is already opened
  // For editor, we draw the closed state by default

  bool is_opened = false;
  if (ctx.state) {
    // Check if this specific lock has been opened via door state
    is_opened = ctx.state->IsDoorOpen(ctx.room_id, 0);  // Lock uses door slot 0
  }

  if (is_opened) {
    // Draw open lock (if different tiles available)
    if (ctx.tiles.size() >= 8) {
      // Use second set of tiles for open state
      for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
          DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + x,
                                       ctx.object.y_ + y,
                                       ctx.tiles[4 + y * 2 + x]);
        }
      }
      return;
    }
  }

  // Draw closed lock (2x2 pattern)
  if (ctx.tiles.size() >= 4) {
    for (int y = 0; y < 2; ++y) {
      for (int x = 0; x < 2; ++x) {
        DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + x,
                                     ctx.object.y_ + y, ctx.tiles[y * 2 + x]);
      }
    }
  }
}

void DrawBombableFloor(const DrawContext& ctx) {
  // ASM: RoomDraw_BombableFloor ($019B7A)
  // Checks room flags to see if floor has been bombed

  bool is_bombed = false;
  if (ctx.state) {
    is_bombed = ctx.state->IsFloorBombable(ctx.room_id);
  }

  if (is_bombed) {
    // Draw hole (use second tile set if available)
    if (ctx.tiles.size() >= 8) {
      for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
          DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + x,
                                       ctx.object.y_ + y,
                                       ctx.tiles[4 + y * 2 + x]);
        }
      }
      return;
    }
  }

  // Draw intact floor (2x2 pattern)
  if (ctx.tiles.size() >= 4) {
    for (int y = 0; y < 2; ++y) {
      for (int x = 0; x < 2; ++x) {
        DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + x,
                                     ctx.object.y_ + y, ctx.tiles[y * 2 + x]);
      }
    }
  }
}

void DrawMovingWall(const DrawContext& ctx, bool is_west) {
  // ASM: RoomDraw_MovingWallWest ($019316), RoomDraw_MovingWallEast ($01935C)
  // Checks if wall has moved based on game state
  (void)is_west;  // Direction affects which way wall moves

  bool has_moved = false;
  if (ctx.state) {
    has_moved = ctx.state->IsWallMoved(ctx.room_id);
  }

  // Draw wall in current position
  // Size determines wall length
  int size = (ctx.object.size_ & 0x0F) + 1;

  if (ctx.tiles.size() < 4) return;

  for (int s = 0; s < size; ++s) {
    int offset = has_moved ? 2 : 0;  // Offset position if wall has moved
    int x = ctx.object.x_ + offset;
    int y = ctx.object.y_ + (s * 2);

    // Draw 2x2 wall segment
    for (int dy = 0; dy < 2; ++dy) {
      for (int dx = 0; dx < 2; ++dx) {
        DrawRoutineUtils::WriteTile8(ctx.target_bg, x + dx, y + dy,
                                     ctx.tiles[dy * 2 + dx]);
      }
    }
  }
}

// ============================================================================
// Water Face Variants
// ============================================================================

void DrawEmptyWaterFace(const DrawContext& ctx) {
  // ASM: RoomDraw_EmptyWaterFace ($019D29)
  // No water spout, just the face
  DrawWaterFace(ctx);
}

void DrawSpittingWaterFace(const DrawContext& ctx) {
  // ASM: RoomDraw_SpittingWaterFace ($019D64)
  // Face with periodic water spout
  DrawWaterFace(ctx);
}

void DrawDrenchingWaterFace(const DrawContext& ctx) {
  // ASM: RoomDraw_DrenchingWaterFace ($019D83)
  // Face with continuous water stream
  DrawWaterFace(ctx);
}

// ============================================================================
// Chest Platform Multi-Part Routines
// ============================================================================

void DrawClosedChestPlatform(const DrawContext& ctx) {
  // ASM: RoomDraw_ClosedChestPlatform ($018CC7)
  // Complex structure: horizontal wall top, vertical walls sides

  int size_x = (ctx.object.size_ & 0x0F) + 4;  // Width is size + 4
  int size_y = ((ctx.object.size_ >> 4) & 0x0F) + 1;

  if (ctx.tiles.size() < 16) return;

  // Draw top horizontal wall with corners
  for (int x = 0; x < size_x; ++x) {
    // Top row
    size_t tile_idx = (x == 0) ? 0 : ((x == size_x - 1) ? 2 : 1);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + x,
                                 ctx.object.y_, ctx.tiles[tile_idx]);
  }

  // Draw vertical walls on sides
  for (int y = 1; y < size_y + 1; ++y) {
    // Left wall
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_,
                                 ctx.object.y_ + y, ctx.tiles[3]);
    // Right wall
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + size_x - 1,
                                 ctx.object.y_ + y, ctx.tiles[4]);
  }

  // Draw bottom horizontal wall with corners
  int bottom_y = ctx.object.y_ + size_y + 1;
  for (int x = 0; x < size_x; ++x) {
    size_t tile_idx = (x == 0) ? 5 : ((x == size_x - 1) ? 7 : 6);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + x, bottom_y,
                                 ctx.tiles[tile_idx]);
  }
}

void DrawChestPlatformHorizontalWall(const DrawContext& ctx) {
  // ASM: RoomDraw_ChestPlatformHorizontalWallWithCorners ($018D0D)
  int width = (ctx.object.size_ & 0x0F) + 1;

  if (ctx.tiles.size() < 3) return;

  for (int x = 0; x < width; ++x) {
    size_t tile_idx = (x == 0) ? 0 : ((x == width - 1) ? 2 : 1);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + x,
                                 ctx.object.y_, ctx.tiles[tile_idx]);
  }
}

void DrawChestPlatformVerticalWall(const DrawContext& ctx) {
  // ASM: RoomDraw_ChestPlatformVerticalWall ($019E70)
  int height = (ctx.object.size_ & 0x0F) + 1;

  if (ctx.tiles.empty()) return;

  for (int y = 0; y < height; ++y) {
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_,
                                 ctx.object.y_ + y, ctx.tiles[0]);
  }
}

void RegisterSpecialRoutines(std::vector<DrawRoutineInfo>& registry) {
  // Note: Routine IDs are assigned based on the assembly routine table
  // These special routines handle chests, doors, and other non-standard objects

  // Chest routine - uses a wrapper since it needs chest_index
  registry.push_back(DrawRoutineInfo{
      .id = 39,  // DrawChest (special index)
      .name = "Chest",
      .function =
          [](const DrawContext& ctx) {
            // Default chest index 0 - actual index tracked externally
            DrawChest(ctx, 0);
          },
      .draws_to_both_bgs = false,
      .base_width = 2,
      .base_height = 2,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 38,  // DrawNothing
      .name = "Nothing",
      .function = DrawNothing,
      .draws_to_both_bgs = false,
      .base_width = 0,
      .base_height = 0,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 26,  // DrawDoorSwitcherer
      .name = "DoorSwitcherer",
      .function = DrawDoorSwitcherer,
      .draws_to_both_bgs = false,
      .base_width = 1,
      .base_height = 1,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 33,  // DrawSomariaLine
      .name = "SomariaLine",
      .function = DrawSomariaLine,
      .draws_to_both_bgs = false,
      .base_width = 0,  // Variable
      .base_height = 0,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 34,  // DrawWaterFace
      .name = "WaterFace",
      .function = DrawWaterFace,
      .draws_to_both_bgs = false,
      .base_width = 2,
      .base_height = 2,
      .category = DrawRoutineInfo::Category::Special,
  });

  // ============================================================================
  // SuperSquare Routines (Phase 4) - IDs 56-64
  // ============================================================================

  registry.push_back(DrawRoutineInfo{
      .id = 56,  // Draw4x4BlocksIn4x4SuperSquare
      .name = "4x4BlocksIn4x4SuperSquare",
      .function = Draw4x4BlocksIn4x4SuperSquare,
      .draws_to_both_bgs = false,
      .base_width = 0,   // Variable: width = (((size >> 2) & 3) + 1) * 4
      .base_height = 0,  // Variable: height = ((size & 3) + 1) * 4
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 57,  // Draw3x3FloorIn4x4SuperSquare
      .name = "3x3FloorIn4x4SuperSquare",
      .function = Draw3x3FloorIn4x4SuperSquare,
      .draws_to_both_bgs = false,
      .base_width = 0,   // Variable
      .base_height = 0,  // Variable
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 58,  // Draw4x4FloorIn4x4SuperSquare
      .name = "4x4FloorIn4x4SuperSquare",
      .function = Draw4x4FloorIn4x4SuperSquare,
      .draws_to_both_bgs = false,
      .base_width = 0,   // Variable
      .base_height = 0,  // Variable
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 59,  // Draw4x4FloorOneIn4x4SuperSquare
      .name = "4x4FloorOneIn4x4SuperSquare",
      .function = Draw4x4FloorOneIn4x4SuperSquare,
      .draws_to_both_bgs = false,
      .base_width = 0,   // Variable
      .base_height = 0,  // Variable
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 60,  // Draw4x4FloorTwoIn4x4SuperSquare
      .name = "4x4FloorTwoIn4x4SuperSquare",
      .function = Draw4x4FloorTwoIn4x4SuperSquare,
      .draws_to_both_bgs = false,
      .base_width = 0,   // Variable
      .base_height = 0,  // Variable
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 61,  // DrawBigHole4x4_1to16
      .name = "BigHole4x4_1to16",
      .function = DrawBigHole4x4_1to16,
      .draws_to_both_bgs = false,
      .base_width = 0,   // Variable
      .base_height = 0,  // Variable
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 62,  // DrawSpike2x2In4x4SuperSquare
      .name = "Spike2x2In4x4SuperSquare",
      .function = DrawSpike2x2In4x4SuperSquare,
      .draws_to_both_bgs = false,
      .base_width = 0,   // Variable
      .base_height = 0,  // Variable
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 63,  // DrawTableRock4x4_1to16
      .name = "TableRock4x4_1to16",
      .function = DrawTableRock4x4_1to16,
      .draws_to_both_bgs = false,
      .base_width = 0,   // Variable
      .base_height = 0,  // Variable
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 64,  // DrawWaterOverlay8x8_1to16
      .name = "WaterOverlay8x8_1to16",
      .function = DrawWaterOverlay8x8_1to16,
      .draws_to_both_bgs = false,
      .base_width = 0,   // Variable
      .base_height = 0,  // Variable
      .category = DrawRoutineInfo::Category::Special,
  });

  // Stair routines (IDs 83-88)
  registry.push_back(DrawRoutineInfo{
      .id = 83,  // DrawInterRoomFatStairsUp
      .name = "InterRoomFatStairsUp",
      .function = DrawInterRoomFatStairsUp,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 4,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 84,  // DrawInterRoomFatStairsDownA
      .name = "InterRoomFatStairsDownA",
      .function = DrawInterRoomFatStairsDownA,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 4,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 85,  // DrawInterRoomFatStairsDownB
      .name = "InterRoomFatStairsDownB",
      .function = DrawInterRoomFatStairsDownB,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 4,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 86,  // DrawAutoStairs
      .name = "AutoStairs",
      .function = DrawAutoStairs,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 4,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 87,  // DrawStraightInterRoomStairs
      .name = "StraightInterRoomStairs",
      .function = DrawStraightInterRoomStairs,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 4,
      .category = DrawRoutineInfo::Category::Special,
  });

  // Spiral stairs variants (IDs 88-91)
  registry.push_back(DrawRoutineInfo{
      .id = 88,  // DrawSpiralStairsGoingUpUpper
      .name = "SpiralStairsGoingUpUpper",
      .function =
          [](const DrawContext& ctx) { DrawSpiralStairs(ctx, true, true); },
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 3,  // 4x3 pattern
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 89,  // DrawSpiralStairsGoingDownUpper
      .name = "SpiralStairsGoingDownUpper",
      .function =
          [](const DrawContext& ctx) { DrawSpiralStairs(ctx, false, true); },
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 3,  // 4x3 pattern
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 90,  // DrawSpiralStairsGoingUpLower
      .name = "SpiralStairsGoingUpLower",
      .function =
          [](const DrawContext& ctx) { DrawSpiralStairs(ctx, true, false); },
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 3,  // 4x3 pattern
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 91,  // DrawSpiralStairsGoingDownLower
      .name = "SpiralStairsGoingDownLower",
      .function =
          [](const DrawContext& ctx) { DrawSpiralStairs(ctx, false, false); },
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 3,  // 4x3 pattern
      .category = DrawRoutineInfo::Category::Special,
  });

  // Interactive object routines (IDs 92-95)
  registry.push_back(DrawRoutineInfo{
      .id = 92,  // DrawBigKeyLock
      .name = "BigKeyLock",
      .function = DrawBigKeyLock,
      .draws_to_both_bgs = false,
      .base_width = 2,
      .base_height = 2,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 93,  // DrawBombableFloor
      .name = "BombableFloor",
      .function = DrawBombableFloor,
      .draws_to_both_bgs = false,
      .base_width = 2,
      .base_height = 2,
      .category = DrawRoutineInfo::Category::Special,
  });

  // Water face variants (IDs 94-96)
  registry.push_back(DrawRoutineInfo{
      .id = 94,  // DrawEmptyWaterFace
      .name = "EmptyWaterFace",
      .function = DrawEmptyWaterFace,
      .draws_to_both_bgs = false,
      .base_width = 2,
      .base_height = 2,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 95,  // DrawSpittingWaterFace
      .name = "SpittingWaterFace",
      .function = DrawSpittingWaterFace,
      .draws_to_both_bgs = false,
      .base_width = 2,
      .base_height = 2,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 96,  // DrawDrenchingWaterFace
      .name = "DrenchingWaterFace",
      .function = DrawDrenchingWaterFace,
      .draws_to_both_bgs = false,
      .base_width = 2,
      .base_height = 2,
      .category = DrawRoutineInfo::Category::Special,
  });

  // Prison cell (Type 3 objects 0x20D, 0x217) draws to both BG layers.
  registry.push_back(DrawRoutineInfo{
      .id = 97,  // DrawPrisonCell
      .name = "PrisonCell",
      .function = DrawPrisonCell,
      .draws_to_both_bgs = true,
      .base_width = 10,  // Columns x..x+9
      .base_height = 4,
      .category = DrawRoutineInfo::Category::Special,
  });

  // Chest platform routines - use canonical IDs from DrawRoutineIds
  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kClosedChestPlatform,  // 79
      .name = "ClosedChestPlatform",
      .function = DrawClosedChestPlatform,
      .draws_to_both_bgs = false,
      .base_width = 0,  // Variable: width = (size & 0x0F) + 4
      .base_height = 0, // Variable: height = ((size >> 4) & 0x0F) + 1
      .category = DrawRoutineInfo::Category::Special,
  });

  // Moving wall routines
  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kMovingWallWest,  // 80
      .name = "MovingWallWest",
      .function = [](const DrawContext& ctx) {
        // Placeholder - actual logic in ObjectDrawer
        DrawNothing(ctx);
      },
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 8,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kMovingWallEast,  // 81
      .name = "MovingWallEast",
      .function = [](const DrawContext& ctx) {
        // Placeholder - actual logic in ObjectDrawer
        DrawNothing(ctx);
      },
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 8,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kOpenChestPlatform,  // 82
      .name = "OpenChestPlatform",
      .function = [](const DrawContext& ctx) {
        // Open chest platform - draws multi-segment pattern
        // Size: width = (size & 0x0F) + 1, segments = ((size >> 4) & 0x0F) * 2 + 5
        int width = (ctx.object.size_ & 0x0F) + 1;
        int segments = ((ctx.object.size_ >> 4) & 0x0F) * 2 + 5;
        // For geometry purposes, just set reasonable bounds
        for (int s = 0; s < segments && s < 8; ++s) {
          for (int x = 0; x < width && x < 8; ++x) {
            if (ctx.tiles.size() > 0) {
              size_t idx = (s * width + x) % ctx.tiles.size();
              DrawRoutineUtils::WriteTile8(ctx.target_bg, 
                  ctx.object.x_ + x, ctx.object.y_ + s, ctx.tiles[idx]);
            }
          }
        }
      },
      .draws_to_both_bgs = false,
      .base_width = 0,  // Variable
      .base_height = 0, // Variable
      .category = DrawRoutineInfo::Category::Special,
  });

  // Vertical rails with CORNER+MIDDLE+END pattern (ID 117) - objects 0x8A-0x8C
  // Matches horizontal rail 0x22 but in vertical orientation
  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kDownwardsHasEdge1x1_1to16_plus23,  // 117
      .name = "DownwardsHasEdge1x1_1to16_plus23",
      .function = [](const DrawContext& ctx) {
        // CORNER+MIDDLE+END pattern vertically
        int size = ctx.object.size_ & 0x0F;
        int count = size + 21;
        if (ctx.tiles.size() < 3) return;
        
        int tile_y = ctx.object.y_;
        // Corner
        DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, tile_y, ctx.tiles[0]);
        tile_y++;
        // Middle tiles
        for (int s = 0; s < count; s++) {
          DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, tile_y, ctx.tiles[1]);
          tile_y++;
        }
        // End tile
        DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, tile_y, ctx.tiles[2]);
      },
      .draws_to_both_bgs = false,
      .base_width = 1,
      .base_height = 23,  // size + 23
      .category = DrawRoutineInfo::Category::Special,
  });

  // Custom Object routine (ID 130) - Oracle of Secrets objects 0x31, 0x32
  // These use external binary files instead of ROM tile data.
  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kCustomObject,  // 130
      .name = "CustomObject",
      .function = [](const DrawContext& ctx) {
        // Custom objects use external binary files
        // Geometry: dimensions depend on the binary file content
        // For now, assume a 4x4 tile pattern as reasonable default
        for (int row = 0; row < 4 && row < 4; ++row) {
          for (int col = 0; col < 4 && col < 4; ++col) {
            if (static_cast<size_t>(row * 4 + col) < ctx.tiles.size()) {
              DrawRoutineUtils::WriteTile8(ctx.target_bg,
                  ctx.object.x_ + col, ctx.object.y_ + row,
                  ctx.tiles[row * 4 + col]);
            }
          }
        }
      },
      .draws_to_both_bgs = false,
      .base_width = 4,   // Default: 4x4 tiles
      .base_height = 4,
      .category = DrawRoutineInfo::Category::Special,
  });
}

}  // namespace draw_routines
}  // namespace zelda3
}  // namespace yaze
