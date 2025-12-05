#include "special_routines.h"

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
  int size_x = (ctx.object.size_ & 0x0F) + 1;
  int size_y = ((ctx.object.size_ >> 4) & 0x0F) + 1;

  if (ctx.tiles.empty()) return;

  // Use first tile for all blocks
  const auto& tile = ctx.tiles[0];

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
  int size_x = (ctx.object.size_ & 0x0F) + 1;
  int size_y = ((ctx.object.size_ >> 4) & 0x0F) + 1;

  if (ctx.tiles.size() < 9) return;  // Need at least 9 tiles for 3x3

  for (int sy = 0; sy < size_y; ++sy) {
    for (int sx = 0; sx < size_x; ++sx) {
      // Each super square positions a 3x3 pattern
      int base_x = ctx.object.x_ + (sx * 4);
      int base_y = ctx.object.y_ + (sy * 4);

      // Draw 3x3 pattern (centered or offset in 4x4 space)
      for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
          size_t tile_idx = static_cast<size_t>(y * 3 + x);
          if (tile_idx < ctx.tiles.size()) {
            DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x, base_y + y,
                                         ctx.tiles[tile_idx]);
          }
        }
      }
    }
  }
}

void Draw4x4FloorIn4x4SuperSquare(const DrawContext& ctx) {
  // ASM: RoomDraw_4x4FloorIn4x4SuperSquare ($018FA5)
  // Most common floor pattern - draws 4x4 floor tiles in super square units.
  // Uses RoomDraw_A_Many32x32Blocks internally in assembly.
  int size_x = (ctx.object.size_ & 0x0F) + 1;
  int size_y = ((ctx.object.size_ >> 4) & 0x0F) + 1;

  if (ctx.tiles.size() < 16) return;  // Need 16 tiles for 4x4

  for (int sy = 0; sy < size_y; ++sy) {
    for (int sx = 0; sx < size_x; ++sx) {
      int base_x = ctx.object.x_ + (sx * 4);
      int base_y = ctx.object.y_ + (sy * 4);

      // Draw 4x4 pattern
      for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
          size_t tile_idx = static_cast<size_t>(y * 4 + x);
          if (tile_idx < ctx.tiles.size()) {
            DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x, base_y + y,
                                         ctx.tiles[tile_idx]);
          }
        }
      }
    }
  }
}

void Draw4x4FloorOneIn4x4SuperSquare(const DrawContext& ctx) {
  // ASM: RoomDraw_4x4FloorOneIn4x4SuperSquare ($018FA2)
  // Single 4x4 floor pattern (starts at different tile offset in assembly).
  // For our purposes, same as 4x4FloorIn4x4SuperSquare with offset tiles.
  int size_x = (ctx.object.size_ & 0x0F) + 1;
  int size_y = ((ctx.object.size_ >> 4) & 0x0F) + 1;

  // Use tiles starting at offset (assembly uses $046A index)
  size_t tile_offset = 0;
  if (ctx.tiles.size() >= 32) tile_offset = 16;  // Use second set if available

  if (ctx.tiles.size() < tile_offset + 16) {
    // Fall back to standard 4x4
    Draw4x4FloorIn4x4SuperSquare(ctx);
    return;
  }

  for (int sy = 0; sy < size_y; ++sy) {
    for (int sx = 0; sx < size_x; ++sx) {
      int base_x = ctx.object.x_ + (sx * 4);
      int base_y = ctx.object.y_ + (sy * 4);

      for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
          size_t tile_idx = tile_offset + static_cast<size_t>(y * 4 + x);
          if (tile_idx < ctx.tiles.size()) {
            DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x, base_y + y,
                                         ctx.tiles[tile_idx]);
          }
        }
      }
    }
  }
}

void Draw4x4FloorTwoIn4x4SuperSquare(const DrawContext& ctx) {
  // ASM: RoomDraw_4x4FloorTwoIn4x4SuperSquare ($018F9D)
  // Two 4x4 floor patterns (uses $0490 offset in assembly).
  int size_x = (ctx.object.size_ & 0x0F) + 1;
  int size_y = ((ctx.object.size_ >> 4) & 0x0F) + 1;

  // Use tiles starting at different offset
  size_t tile_offset = 0;
  if (ctx.tiles.size() >= 48) tile_offset = 32;

  if (ctx.tiles.size() < tile_offset + 16) {
    Draw4x4FloorIn4x4SuperSquare(ctx);
    return;
  }

  for (int sy = 0; sy < size_y; ++sy) {
    for (int sx = 0; sx < size_x; ++sx) {
      int base_x = ctx.object.x_ + (sx * 4);
      int base_y = ctx.object.y_ + (sy * 4);

      for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
          size_t tile_idx = tile_offset + static_cast<size_t>(y * 4 + x);
          if (tile_idx < ctx.tiles.size()) {
            DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x, base_y + y,
                                         ctx.tiles[tile_idx]);
          }
        }
      }
    }
  }
}

void DrawBigHole4x4_1to16(const DrawContext& ctx) {
  // ASM: Object 0xA4 - Big hole pattern
  // Draws a 4x4 hole pattern that repeats based on size
  int count = (ctx.object.size_ & 0x0F) + 1;

  if (ctx.tiles.size() < 16) return;

  for (int s = 0; s < count; ++s) {
    int base_x = ctx.object.x_;
    int base_y = ctx.object.y_ + (s * 4);

    // Draw 4x4 pattern
    for (int y = 0; y < 4; ++y) {
      for (int x = 0; x < 4; ++x) {
        size_t tile_idx = static_cast<size_t>(y * 4 + x);
        DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x, base_y + y,
                                     ctx.tiles[tile_idx]);
      }
    }
  }
}

void DrawSpike2x2In4x4SuperSquare(const DrawContext& ctx) {
  // ASM: RoomDraw_Spike2x2In4x4SuperSquare ($019708)
  // Draws 2x2 spike patterns within super square units
  int size_x = (ctx.object.size_ & 0x0F) + 1;
  int size_y = ((ctx.object.size_ >> 4) & 0x0F) + 1;

  if (ctx.tiles.size() < 4) return;

  for (int sy = 0; sy < size_y; ++sy) {
    for (int sx = 0; sx < size_x; ++sx) {
      int base_x = ctx.object.x_ + (sx * 4);
      int base_y = ctx.object.y_ + (sy * 4);

      // Draw 2x2 spike pattern (centered in 4x4 area)
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, base_y + 1,
                                   ctx.tiles[0]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 2, base_y + 1,
                                   ctx.tiles[1]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, base_y + 2,
                                   ctx.tiles[2]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 2, base_y + 2,
                                   ctx.tiles[3]);
    }
  }
}

void DrawTableRock4x4_1to16(const DrawContext& ctx) {
  // ASM: Object 0xDD - Table rock pattern
  // 4x4 rock pattern that repeats
  int count = (ctx.object.size_ & 0x0F) + 1;

  if (ctx.tiles.size() < 16) return;

  for (int s = 0; s < count; ++s) {
    int base_x = ctx.object.x_ + (s * 4);
    int base_y = ctx.object.y_;

    for (int y = 0; y < 4; ++y) {
      for (int x = 0; x < 4; ++x) {
        size_t tile_idx = static_cast<size_t>(y * 4 + x);
        DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x, base_y + y,
                                     ctx.tiles[tile_idx]);
      }
    }
  }
}

void DrawWaterOverlay8x8_1to16(const DrawContext& ctx) {
  // ASM: RoomDraw_WaterOverlayA8x8_1to16 ($0195D6) / RoomDraw_WaterOverlayB8x8
  // Water overlay pattern that draws 8x8 tile areas
  int size_x = (ctx.object.size_ & 0x0F) + 1;
  int size_y = ((ctx.object.size_ >> 4) & 0x0F) + 1;

  if (ctx.tiles.size() < 16) return;

  for (int sy = 0; sy < size_y; ++sy) {
    for (int sx = 0; sx < size_x; ++sx) {
      int base_x = ctx.object.x_ + (sx * 8);
      int base_y = ctx.object.y_ + (sy * 8);

      // Draw repeating 4x4 pattern to fill 8x8 area
      for (int by = 0; by < 2; ++by) {
        for (int bx = 0; bx < 2; ++bx) {
          for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
              size_t tile_idx = static_cast<size_t>(y * 4 + x);
              DrawRoutineUtils::WriteTile8(ctx.target_bg,
                                           base_x + (bx * 4) + x,
                                           base_y + (by * 4) + y,
                                           ctx.tiles[tile_idx]);
            }
          }
        }
      }
    }
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
}

}  // namespace draw_routines
}  // namespace zelda3
}  // namespace yaze
