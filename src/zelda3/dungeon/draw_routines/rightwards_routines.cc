#include "rightwards_routines.h"

#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {
namespace draw_routines {

void DrawRightwards2x2_1to15or32(const DrawContext& ctx) {
  // Pattern: Draws 2x2 tiles rightward (object 0x00)
  // Size byte determines how many times to repeat (1-15 or 32)
  // ROM tile order is COLUMN-MAJOR: [col0_row0, col0_row1, col1_row0, col1_row1]
  int size = ctx.object.size_;
  if (size == 0)
    size = 32;  // Special case for object 0x00

  for (int s = 0; s < size; s++) {
    if (ctx.tiles.size() >= 4) {
      // Draw 2x2 pattern in COLUMN-MAJOR order (matching assembly)
      // tiles[0] → $BF → (col 0, row 0) = top-left
      // tiles[1] → $CB → (col 0, row 1) = bottom-left
      // tiles[2] → $C2 → (col 1, row 0) = top-right
      // tiles[3] → $CE → (col 1, row 1) = bottom-right
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2),
                                   ctx.object.y_,
                                   ctx.tiles[0]);  // col 0, row 0
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2),
                                   ctx.object.y_ + 1,
                                   ctx.tiles[1]);  // col 0, row 1
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2) + 1,
                                   ctx.object.y_,
                                   ctx.tiles[2]);  // col 1, row 0
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2) + 1,
                                   ctx.object.y_ + 1,
                                   ctx.tiles[3]);  // col 1, row 1
    }
  }
}

void DrawRightwards2x4_1to15or26(const DrawContext& ctx) {
  // Pattern: Draws 2x4 tiles rightward (objects 0x01-0x02)
  // Uses RoomDraw_Nx4 with N=2, tiles are COLUMN-MAJOR:
  // [col0_row0, col0_row1, col0_row2, col0_row3, col1_row0, col1_row1, col1_row2, col1_row3]
  int size = ctx.object.size_;
  if (size == 0)
    size = 26;  // Special case

  for (int s = 0; s < size; s++) {
    if (ctx.tiles.size() >= 8) {
      // Draw 2x4 pattern in COLUMN-MAJOR order (matching RoomDraw_Nx4)
      // Column 0 (tiles 0-3)
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2),
                                   ctx.object.y_,
                                   ctx.tiles[0]);  // col 0, row 0
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2),
                                   ctx.object.y_ + 1,
                                   ctx.tiles[1]);  // col 0, row 1
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2),
                                   ctx.object.y_ + 2,
                                   ctx.tiles[2]);  // col 0, row 2
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2),
                                   ctx.object.y_ + 3,
                                   ctx.tiles[3]);  // col 0, row 3
      // Column 1 (tiles 4-7)
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2) + 1,
                                   ctx.object.y_,
                                   ctx.tiles[4]);  // col 1, row 0
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2) + 1,
                                   ctx.object.y_ + 1,
                                   ctx.tiles[5]);  // col 1, row 1
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2) + 1,
                                   ctx.object.y_ + 2,
                                   ctx.tiles[6]);  // col 1, row 2
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2) + 1,
                                   ctx.object.y_ + 3,
                                   ctx.tiles[7]);  // col 1, row 3
    } else if (ctx.tiles.size() >= 4) {
      // Fallback: with 4 tiles we can only draw 1 column (1x4 pattern)
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2),
                                   ctx.object.y_, ctx.tiles[0]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2),
                                   ctx.object.y_ + 1, ctx.tiles[1]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2),
                                   ctx.object.y_ + 2, ctx.tiles[2]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2),
                                   ctx.object.y_ + 3, ctx.tiles[3]);
    }
  }
}

void DrawRightwards2x4_1to16(const DrawContext& ctx) {
  // Pattern: Draws 2x4 tiles rightward with adjacent spacing (objects 0x03-0x04)
  // Uses RoomDraw_Nx4 with N=2, tiles are COLUMN-MAJOR
  // ASM: GetSize_1to16 means count = size + 1
  int size = ctx.object.size_ & 0x0F;
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (ctx.tiles.size() >= 8) {
      // Draw 2x4 pattern in COLUMN-MAJOR order with adjacent spacing (s * 2)
      // Column 0 (tiles 0-3)
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2),
                                   ctx.object.y_,
                                   ctx.tiles[0]);  // col 0, row 0
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2),
                                   ctx.object.y_ + 1,
                                   ctx.tiles[1]);  // col 0, row 1
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2),
                                   ctx.object.y_ + 2,
                                   ctx.tiles[2]);  // col 0, row 2
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2),
                                   ctx.object.y_ + 3,
                                   ctx.tiles[3]);  // col 0, row 3
      // Column 1 (tiles 4-7)
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2) + 1,
                                   ctx.object.y_,
                                   ctx.tiles[4]);  // col 1, row 0
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2) + 1,
                                   ctx.object.y_ + 1,
                                   ctx.tiles[5]);  // col 1, row 1
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2) + 1,
                                   ctx.object.y_ + 2,
                                   ctx.tiles[6]);  // col 1, row 2
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2) + 1,
                                   ctx.object.y_ + 3,
                                   ctx.tiles[7]);  // col 1, row 3
    } else if (ctx.tiles.size() >= 4) {
      // Fallback: with 4 tiles we can only draw 1 column (1x4 pattern)
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2),
                                   ctx.object.y_, ctx.tiles[0]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2),
                                   ctx.object.y_ + 1, ctx.tiles[1]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2),
                                   ctx.object.y_ + 2, ctx.tiles[2]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2),
                                   ctx.object.y_ + 3, ctx.tiles[3]);
    }
  }
}

void DrawRightwards2x4_1to16_BothBG(const DrawContext& ctx) {
  // USDASM: RoomDraw_Rightwards2x4spaced4_1to16_BothBG ($01:8C37)
  //
  // Despite the "_BothBG" suffix in usdasm, this routine does NOT explicitly
  // write to both tilemaps; it uses the current tilemap pointers and is thus
  // single-layer.
  //
  // Behavior: draw a 2x4 block, then advance by an additional 4 tiles before
  // drawing the next block (net step = 2 tile width + 4 tile gap = 6 tiles).
  int size = ctx.object.size_ & 0x0F;
  int count = size + 1;

  constexpr int kStepTiles = 6;

  for (int s = 0; s < count; s++) {
    const int base_x = ctx.object.x_ + (s * kStepTiles);

    if (ctx.tiles.size() >= 8) {
      // Column 0 (tiles 0-3)
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 0, ctx.object.y_,
                                   ctx.tiles[0]);  // col 0, row 0
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 0, ctx.object.y_ + 1,
                                   ctx.tiles[1]);  // col 0, row 1
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 0, ctx.object.y_ + 2,
                                   ctx.tiles[2]);  // col 0, row 2
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 0, ctx.object.y_ + 3,
                                   ctx.tiles[3]);  // col 0, row 3

      // Column 1 (tiles 4-7)
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, ctx.object.y_,
                                   ctx.tiles[4]);  // col 1, row 0
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, ctx.object.y_ + 1,
                                   ctx.tiles[5]);  // col 1, row 1
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, ctx.object.y_ + 2,
                                   ctx.tiles[6]);  // col 1, row 2
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, ctx.object.y_ + 3,
                                   ctx.tiles[7]);  // col 1, row 3
    } else if (ctx.tiles.size() >= 4) {
      // Fallback: with 4 tiles we can only draw 1 column (1x4 pattern)
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 0, ctx.object.y_,
                                   ctx.tiles[0]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 0, ctx.object.y_ + 1,
                                   ctx.tiles[1]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 0, ctx.object.y_ + 2,
                                   ctx.tiles[2]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 0, ctx.object.y_ + 3,
                                   ctx.tiles[3]);
    }
  }
}

void DrawRightwards2x2_1to16(const DrawContext& ctx) {
  // Pattern: Draws 2x2 tiles rightward (objects 0x07-0x08)
  // ROM tile order is COLUMN-MAJOR: [col0_row0, col0_row1, col1_row0, col1_row1]
  int size = ctx.object.size_ & 0x0F;

  // Assembly: JSR RoomDraw_GetSize_1to16
  // GetSize_1to16: count = size + 1
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (ctx.tiles.size() >= 4) {
      // Draw 2x2 pattern in COLUMN-MAJOR order (matching assembly)
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2),
                                   ctx.object.y_,
                                   ctx.tiles[0]);  // col 0, row 0
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2),
                                   ctx.object.y_ + 1,
                                   ctx.tiles[1]);  // col 0, row 1
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2) + 1,
                                   ctx.object.y_,
                                   ctx.tiles[2]);  // col 1, row 0
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2) + 1,
                                   ctx.object.y_ + 1,
                                   ctx.tiles[3]);  // col 1, row 1
    }
  }
}

void DrawRightwards1x2_1to16_plus2(const DrawContext& ctx) {
  // Pattern: 1x3 tiles rightward with caps (object 0x21)
  int size = ctx.object.size_ & 0x0F;

  if (ctx.tiles.size() >= 9) {
    auto draw_column = [&](int x, int base) {
      DrawRoutineUtils::WriteTile8(ctx.target_bg, x, ctx.object.y_,
                                   ctx.tiles[base + 0]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, x, ctx.object.y_ + 1,
                                   ctx.tiles[base + 1]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, x, ctx.object.y_ + 2,
                                   ctx.tiles[base + 2]);
    };

    draw_column(ctx.object.x_, 0);

    int mid_cols = (size + 1) * 2;
    for (int s = 0; s < mid_cols; s++) {
      draw_column(ctx.object.x_ + 1 + s, 3);
    }

    draw_column(ctx.object.x_ + 1 + mid_cols, 6);
    return;
  }

  int count = (size * 2) + 1;
  for (int s = 0; s < count; s++) {
    if (ctx.tiles.size() >= 2) {
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + s + 2,
                                   ctx.object.y_, ctx.tiles[0]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + s + 2,
                                   ctx.object.y_ + 1, ctx.tiles[1]);
    }
  }
}

void DrawRightwardsHasEdge1x1_1to16_plus3(const DrawContext& ctx) {
  // Pattern: Rail with corner/middle/end (object 0x22)
  int size = ctx.object.size_ & 0x0F;

  int count = size + 2;
  if (ctx.tiles.size() < 3)
    return;

  int x = ctx.object.x_;
  DrawRoutineUtils::WriteTile8(ctx.target_bg, x, ctx.object.y_, ctx.tiles[0]);
  x++;
  for (int s = 0; s < count; s++) {
    DrawRoutineUtils::WriteTile8(ctx.target_bg, x, ctx.object.y_, ctx.tiles[1]);
    x++;
  }
  DrawRoutineUtils::WriteTile8(ctx.target_bg, x, ctx.object.y_, ctx.tiles[2]);
}

void DrawRightwardsHasEdge1x1_1to16_plus2(const DrawContext& ctx) {
  // Pattern: Rail with corner/middle/end (objects 0x23-0x2E, 0x3F-0x46)
  int size = ctx.object.size_ & 0x0F;

  int count = size + 1;
  if (ctx.tiles.size() < 3)
    return;

  int x = ctx.object.x_;
  DrawRoutineUtils::WriteTile8(ctx.target_bg, x, ctx.object.y_, ctx.tiles[0]);
  x++;
  for (int s = 0; s < count; s++) {
    DrawRoutineUtils::WriteTile8(ctx.target_bg, x, ctx.object.y_, ctx.tiles[1]);
    x++;
  }
  DrawRoutineUtils::WriteTile8(ctx.target_bg, x, ctx.object.y_, ctx.tiles[2]);
}

void DrawRightwardsHasEdge1x1_1to16_plus23(const DrawContext& ctx) {
  // Pattern: Long rail with corner/middle/end (object 0x5F)
  int size = ctx.object.size_ & 0x0F;
  int count = size + 21;
  if (ctx.tiles.size() < 3)
    return;

  int x = ctx.object.x_;
  DrawRoutineUtils::WriteTile8(ctx.target_bg, x, ctx.object.y_, ctx.tiles[0]);
  x++;
  for (int s = 0; s < count; s++) {
    DrawRoutineUtils::WriteTile8(ctx.target_bg, x, ctx.object.y_, ctx.tiles[1]);
    x++;
  }
  DrawRoutineUtils::WriteTile8(ctx.target_bg, x, ctx.object.y_, ctx.tiles[2]);
}

void DrawRightwardsTopCorners1x2_1to16_plus13(const DrawContext& ctx) {
  // Pattern: Top corner 1x2 tiles with +13 offset (object 0x2F)
  const int size = ctx.object.size_ & 0x0F;
  // USDASM $01:8FBD - Object_Size_N_to_N_plus_15 with N=0x0A.
  const int count = size + 10;
  if (count <= 0 || ctx.tiles.empty()) {
    return;
  }

  // Middle columns are (top=tile3, bottom=tile0). Ends use cap tiles.
  const gfx::TileInfo& bottom_fill = ctx.tiles[0];
  const gfx::TileInfo& top_fill =
      ctx.tiles.size() > 3 ? ctx.tiles[3] : ctx.tiles[0];
  const gfx::TileInfo& start_cap_top =
      ctx.tiles.size() > 1 ? ctx.tiles[1] : top_fill;
  const gfx::TileInfo& end_cap_top =
      ctx.tiles.size() > 4 ? ctx.tiles[4] : top_fill;

  for (int s = 0; s < count; ++s) {
    const gfx::TileInfo& top_tile = (s == 0)           ? start_cap_top
                                    : (s == count - 1) ? end_cap_top
                                                       : top_fill;

    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + s + 13,
                                 ctx.object.y_, top_tile);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + s + 13,
                                 ctx.object.y_ + 1, bottom_fill);
  }
}

void DrawRightwardsBottomCorners1x2_1to16_plus13(const DrawContext& ctx) {
  // Pattern: Bottom corner 1x2 tiles with +13 offset (object 0x30)
  const int size = ctx.object.size_ & 0x0F;
  // USDASM $01:9001 - mirrored variant of the top-corner routine.
  const int count = size + 10;
  if (count <= 0 || ctx.tiles.empty()) {
    return;
  }

  const gfx::TileInfo& top_fill = ctx.tiles[0];
  const gfx::TileInfo& bottom_fill =
      ctx.tiles.size() > 3 ? ctx.tiles[3] : ctx.tiles[0];
  const gfx::TileInfo& start_cap_bottom =
      ctx.tiles.size() > 1 ? ctx.tiles[1] : bottom_fill;
  const gfx::TileInfo& end_cap_bottom =
      ctx.tiles.size() > 4 ? ctx.tiles[4] : bottom_fill;

  for (int s = 0; s < count; ++s) {
    const gfx::TileInfo& bottom_tile = (s == 0)           ? start_cap_bottom
                                       : (s == count - 1) ? end_cap_bottom
                                                          : bottom_fill;

    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + s + 13,
                                 ctx.object.y_ + 1, top_fill);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + s + 13,
                                 ctx.object.y_ + 2, bottom_tile);
  }
}

void DrawRightwards4x4_1to16(const DrawContext& ctx) {
  // Pattern: 4x4 block rightward (object 0x33)
  int size = ctx.object.size_ & 0x0F;

  // Assembly: GetSize_1to16, so count = size + 1
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (ctx.tiles.size() >= 16) {
      // Draw 4x4 pattern in COLUMN-MAJOR order (matching assembly)
      // Iterate columns (x) first, then rows (y) within each column
      for (int x = 0; x < 4; ++x) {
        for (int y = 0; y < 4; ++y) {
          DrawRoutineUtils::WriteTile8(ctx.target_bg,
                                       ctx.object.x_ + (s * 4) + x,
                                       ctx.object.y_ + y, ctx.tiles[x * 4 + y]);
        }
      }
    }
  }
}

void DrawRightwards1x1Solid_1to16_plus3(const DrawContext& ctx) {
  // Pattern: 1x1 solid tiles +3 offset (object 0x34)
  int size = ctx.object.size_ & 0x0F;

  // Assembly: GetSize_1to16_timesA(4), so count = size + 4
  int count = size + 4;

  for (int s = 0; s < count; s++) {
    if (ctx.tiles.size() >= 1) {
      // Use first 8x8 tile from span
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + s + 3,
                                   ctx.object.y_, ctx.tiles[0]);
    }
  }
}

void DrawRightwardsDecor4x4spaced2_1to16(const DrawContext& ctx) {
  // Pattern: 4x4 decoration with spacing (objects 0x36-0x37)
  int size = ctx.object.size_ & 0x0F;

  // Assembly: GetSize_1to16, so count = size + 1
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (ctx.tiles.size() >= 16) {
      // Draw 4x4 pattern with spacing in COLUMN-MAJOR order (matching assembly)
      for (int x = 0; x < 4; ++x) {
        for (int y = 0; y < 4; ++y) {
          DrawRoutineUtils::WriteTile8(ctx.target_bg,
                                       ctx.object.x_ + (s * 6) + x,
                                       ctx.object.y_ + y, ctx.tiles[x * 4 + y]);
        }
      }
    }
  }
}

void DrawRightwardsStatue2x3spaced2_1to16(const DrawContext& ctx) {
  // Pattern: 2x3 statue with spacing (object 0x38)
  // 2 columns × 3 rows = 6 tiles in COLUMN-MAJOR order
  int size = ctx.object.size_ & 0x0F;

  // Assembly: GetSize_1to16, so count = size + 1
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (ctx.tiles.size() >= 6) {
      // Draw 2x3 pattern in COLUMN-MAJOR order (matching assembly)
      for (int x = 0; x < 2; ++x) {
        for (int y = 0; y < 3; ++y) {
          DrawRoutineUtils::WriteTile8(ctx.target_bg,
                                       ctx.object.x_ + (s * 4) + x,
                                       ctx.object.y_ + y, ctx.tiles[x * 3 + y]);
        }
      }
    }
  }
}

void DrawRightwardsPillar2x4spaced4_1to16(const DrawContext& ctx) {
  // Pattern: 2x4 pillar with spacing (objects 0x39, 0x3D)
  // 2 columns × 4 rows = 8 tiles in COLUMN-MAJOR order
  int size = ctx.object.size_ & 0x0F;

  // Assembly: GetSize_1to16, so count = size + 1
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (ctx.tiles.size() >= 8) {
      // Draw 2x4 pattern in COLUMN-MAJOR order (matching assembly)
      for (int x = 0; x < 2; ++x) {
        for (int y = 0; y < 4; ++y) {
          DrawRoutineUtils::WriteTile8(ctx.target_bg,
                                       ctx.object.x_ + (s * 6) + x,
                                       ctx.object.y_ + y, ctx.tiles[x * 4 + y]);
        }
      }
    }
  }
}

void DrawRightwardsDecor4x3spaced4_1to16(const DrawContext& ctx) {
  // Pattern: 4x3 decoration with spacing (objects 0x3A-0x3B)
  // 4 columns × 3 rows = 12 tiles in COLUMN-MAJOR order
  int size = ctx.object.size_ & 0x0F;

  // Assembly: GetSize_1to16, so count = size + 1
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (ctx.tiles.size() >= 12) {
      // Draw 4x3 pattern in COLUMN-MAJOR order (matching assembly)
      for (int x = 0; x < 4; ++x) {
        for (int y = 0; y < 3; ++y) {
          DrawRoutineUtils::WriteTile8(ctx.target_bg,
                                       ctx.object.x_ + (s * 6) + x,
                                       ctx.object.y_ + y, ctx.tiles[x * 3 + y]);
        }
      }
    }
  }
}

void DrawRightwardsDoubled2x2spaced2_1to16(const DrawContext& ctx) {
  // Pattern: Doubled 2x2 with spacing (object 0x3C)
  // 4 columns × 2 rows = 8 tiles in COLUMN-MAJOR order
  int size = ctx.object.size_ & 0x0F;

  // Assembly: GetSize_1to16, so count = size + 1
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (ctx.tiles.size() >= 8) {
      // Draw doubled 2x2 pattern in COLUMN-MAJOR order (matching assembly)
      for (int x = 0; x < 4; ++x) {
        for (int y = 0; y < 2; ++y) {
          DrawRoutineUtils::WriteTile8(ctx.target_bg,
                                       ctx.object.x_ + (s * 6) + x,
                                       ctx.object.y_ + y, ctx.tiles[x * 2 + y]);
        }
      }
    }
  }
}

void DrawRightwardsDecor2x2spaced12_1to16(const DrawContext& ctx) {
  // Pattern: 2x2 decoration with large spacing (object 0x3E)
  int size = ctx.object.size_ & 0x0F;

  // Assembly: GetSize_1to16, so count = size + 1
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (ctx.tiles.size() >= 4) {
      // Draw 2x2 pattern in COLUMN-MAJOR order (matching assembly)
      // tiles[0] → col 0, row 0 = top-left
      // tiles[1] → col 0, row 1 = bottom-left
      // tiles[2] → col 1, row 0 = top-right
      // tiles[3] → col 1, row 1 = bottom-right
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 14),
                                   ctx.object.y_,
                                   ctx.tiles[0]);  // col 0, row 0
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 14),
                                   ctx.object.y_ + 1,
                                   ctx.tiles[1]);  // col 0, row 1
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 14) + 1,
                                   ctx.object.y_,
                                   ctx.tiles[2]);  // col 1, row 0
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 14) + 1,
                                   ctx.object.y_ + 1,
                                   ctx.tiles[3]);  // col 1, row 1
    }
  }
}

void DrawRightwards4x2_1to16(const DrawContext& ctx) {
  // Pattern: Draws 4x2 tiles rightward (objects 0x49-0x4A: Floor Tile 4x2)
  // Uses ROW-MAJOR tile order:
  // row 0: tiles[0..3], row 1: tiles[4..7]
  const int size = ctx.object.size_ & 0x0F;
  const int count = size + 1;  // GetSize_1to16

  if (ctx.tiles.size() < 8) {
    return;
  }

  for (int s = 0; s < count; ++s) {
    const int base_x = ctx.object.x_ + (s * 4);

    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 0, ctx.object.y_,
                                 ctx.tiles[0]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, ctx.object.y_,
                                 ctx.tiles[1]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 2, ctx.object.y_,
                                 ctx.tiles[2]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 3, ctx.object.y_,
                                 ctx.tiles[3]);

    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 0, ctx.object.y_ + 1,
                                 ctx.tiles[4]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, ctx.object.y_ + 1,
                                 ctx.tiles[5]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 2, ctx.object.y_ + 1,
                                 ctx.tiles[6]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 3, ctx.object.y_ + 1,
                                 ctx.tiles[7]);
  }
}

void DrawRightwardsDecor4x2spaced8_1to16(const DrawContext& ctx) {
  // Pattern: Draws 1x8 column tiles with 12-tile horizontal spacing
  // (objects 0x55-0x56 wall torches).
  const int size = ctx.object.size_ & 0x0F;
  const int count = size + 1;  // GetSize_1to16

  if (ctx.tiles.size() < 8) {
    return;
  }

  for (int s = 0; s < count; ++s) {
    const int base_x = ctx.object.x_ + (s * 12);
    for (int row = 0; row < 8; ++row) {
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x, ctx.object.y_ + row,
                                   ctx.tiles[row]);
    }
  }
}

void DrawRightwardsCannonHole4x3_1to16(const DrawContext& ctx) {
  // USDASM behavior:
  // - Repeat left 2-column 3-row segment count times
  // - Then append right 2-column edge once
  // Tile layout is COLUMN-MAJOR:
  // col0: [0..2], col1: [3..5], col2: [6..8], col3: [9..11]
  const int size = ctx.object.size_ & 0x0F;
  const int count = size + 1;  // GetSize_1to16

  if (ctx.tiles.size() < 12) {
    return;
  }

  auto draw_column = [&](int x, int y, const gfx::TileInfo& t0,
                         const gfx::TileInfo& t1, const gfx::TileInfo& t2) {
    DrawRoutineUtils::WriteTile8(ctx.target_bg, x, y, t0);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, x, y + 1, t1);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, x, y + 2, t2);
  };

  for (int s = 0; s < count; ++s) {
    const int base_x = ctx.object.x_ + (s * 2);
    draw_column(base_x + 0, ctx.object.y_, ctx.tiles[0], ctx.tiles[1],
                ctx.tiles[2]);
    draw_column(base_x + 1, ctx.object.y_, ctx.tiles[3], ctx.tiles[4],
                ctx.tiles[5]);
  }

  const int right_base_x = ctx.object.x_ + (count * 2);
  draw_column(right_base_x + 0, ctx.object.y_, ctx.tiles[6], ctx.tiles[7],
              ctx.tiles[8]);
  draw_column(right_base_x + 1, ctx.object.y_, ctx.tiles[9], ctx.tiles[10],
              ctx.tiles[11]);
}

void DrawRightwardsLine1x1_1to16plus1(const DrawContext& ctx) {
  const int size = ctx.object.size_ & 0x0F;
  const int count = size + 2;
  if (ctx.tiles.empty()) {
    return;
  }

  for (int s = 0; s < count; ++s) {
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + s,
                                 ctx.object.y_, ctx.tiles[0]);
  }
}

void DrawRightwardsBar4x3_1to16(const DrawContext& ctx) {
  const int size = ctx.object.size_ & 0x0F;
  const int count = size + 1;
  if (ctx.tiles.size() < 12) {
    return;
  }

  for (int s = 0; s < count; ++s) {
    const int base_x = ctx.object.x_ + (s * 4);
    for (int x = 0; x < 4; ++x) {
      for (int y = 0; y < 3; ++y) {
        DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x,
                                     ctx.object.y_ + y, ctx.tiles[x * 3 + y]);
      }
    }
  }
}

void DrawRightwardsShelf4x4_1to16(const DrawContext& ctx) {
  const int size = ctx.object.size_ & 0x0F;
  const int count = size + 1;
  if (ctx.tiles.size() < 16) {
    return;
  }

  for (int s = 0; s < count; ++s) {
    const int base_x = ctx.object.x_ + (s * 4);
    for (int x = 0; x < 4; ++x) {
      for (int y = 0; y < 4; ++y) {
        DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x,
                                     ctx.object.y_ + y, ctx.tiles[x * 4 + y]);
      }
    }
  }
}

void DrawRightwardsBigRail1x3_1to16plus5(const DrawContext& ctx) {
  const int size = ctx.object.size_ & 0x0F;
  const int middle_columns = size + 2;
  const int total_columns = size + 6;

  if (ctx.tiles.size() >= 15) {
    auto draw_column = [&](int x, int base_idx) {
      DrawRoutineUtils::WriteTile8(ctx.target_bg, x, ctx.object.y_ + 0,
                                   ctx.tiles[base_idx + 0]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, x, ctx.object.y_ + 1,
                                   ctx.tiles[base_idx + 1]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, x, ctx.object.y_ + 2,
                                   ctx.tiles[base_idx + 2]);
    };

    // USDASM $01:8F36:
    // - 2-column start cap (tiles 0..5)
    // - repeated middle columns (tiles 6..8), repeated size+2 times
    // - 2-column end cap (tiles 9..14)
    draw_column(ctx.object.x_ + 0, 0);
    draw_column(ctx.object.x_ + 1, 3);
    for (int s = 0; s < middle_columns; ++s) {
      draw_column(ctx.object.x_ + 2 + s, 6);
    }
    draw_column(ctx.object.x_ + 2 + middle_columns, 9);
    draw_column(ctx.object.x_ + 3 + middle_columns, 12);
    return;
  }

  if (ctx.tiles.empty()) {
    return;
  }
  for (int x = 0; x < total_columns; ++x) {
    for (int y = 0; y < 3; ++y) {
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + x,
                                   ctx.object.y_ + y, ctx.tiles[0]);
    }
  }
}

void DrawRightwardsBlock2x2spaced2_1to16(const DrawContext& ctx) {
  const int size = ctx.object.size_ & 0x0F;
  const int count = size + 1;
  if (ctx.tiles.size() < 4) {
    return;
  }

  for (int s = 0; s < count; ++s) {
    const int base_x = ctx.object.x_ + (s * 4);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 0, ctx.object.y_ + 0,
                                 ctx.tiles[0]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 0, ctx.object.y_ + 1,
                                 ctx.tiles[1]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, ctx.object.y_ + 0,
                                 ctx.tiles[2]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, ctx.object.y_ + 1,
                                 ctx.tiles[3]);
  }
}

void DrawRightwardsEdge1x1_1to16plus7(const DrawContext& ctx) {
  const int size = ctx.object.size_ & 0x0F;
  const int count = size + 8;
  if (ctx.tiles.empty()) {
    return;
  }

  for (int s = 0; s < count; ++s) {
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + s,
                                 ctx.object.y_, ctx.tiles[0]);
  }
}

void DrawRightwardsPots2x2_1to16(const DrawContext& ctx) {
  const int size = ctx.object.size_ & 0x0F;
  const int count = size + 1;
  if (ctx.tiles.size() < 4) {
    return;
  }

  for (int s = 0; s < count; ++s) {
    const int base_x = ctx.object.x_ + (s * 2);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 0, ctx.object.y_ + 0,
                                 ctx.tiles[0]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 0, ctx.object.y_ + 1,
                                 ctx.tiles[1]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, ctx.object.y_ + 0,
                                 ctx.tiles[2]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, ctx.object.y_ + 1,
                                 ctx.tiles[3]);
  }
}

void DrawRightwardsHammerPegs2x2_1to16(const DrawContext& ctx) {
  DrawRightwardsPots2x2_1to16(ctx);
}

void RegisterRightwardsRoutines(std::vector<DrawRoutineInfo>& registry) {
  // Note: Routine IDs are assigned based on the assembly routine table
  // These rightwards routines are part of the core 40 draw routines
  // Uses canonical IDs from DrawRoutineIds namespace

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwards2x2_1to15or32,
      .name = "Rightwards2x2_1to15or32",
      .function = DrawRightwards2x2_1to15or32,
      // USDASM: RoomDraw_Rightwards2x2_1to15or32 ($01:8B89) calls
      // RoomDraw_Rightwards2x2 ($01:9895), which writes through the current
      // tilemap pointer set (single-layer).
      .draws_to_both_bgs = false,
      .base_width = 2,
      .base_height = 2,
      .min_tiles = 4,  // 2x2 block
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwards2x4_1to15or26,
      .name = "Rightwards2x4_1to15or26",
      .function = DrawRightwards2x4_1to15or26,
      // USDASM: RoomDraw_Rightwards2x4_1to15or26 ($01:8A92) uses RoomDraw_Nx4
      // ($01:97F0) via pointers; single-layer.
      .draws_to_both_bgs = false,
      .base_width = 2,
      .base_height = 4,
      .min_tiles = 8,  // 2x4 block
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwards2x4_1to16,
      .name = "Rightwards2x4_1to16",
      .function = DrawRightwards2x4_1to16,
      // USDASM: RoomDraw_Rightwards2x4spaced4_1to16 ($01:8B0D) explicitly
      // writes to both tilemaps ($7E2000 and $7E4000).
      .draws_to_both_bgs = true,
      .base_width = 2,  // Adjacent spacing (s * 2)
      .base_height = 4,
      .min_tiles = 8,  // 2x4 block
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwards2x4_1to16_BothBG,
      .name = "Rightwards2x4_1to16_BothBG",
      .function = DrawRightwards2x4_1to16_BothBG,
      // USDASM: RoomDraw_Rightwards2x4spaced4_1to16_BothBG ($01:8C37) uses
      // RoomDraw_Nx4 through the current tilemap pointers (single-layer).
      .draws_to_both_bgs = false,
      .base_width = 2,  // 2x4 blocks; repeat step is 6 tiles (2 wide + 4 gap)
      .base_height = 4,
      .min_tiles = 8,  // 2x4 block
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwards2x2_1to16,
      .name = "Rightwards2x2_1to16",
      .function = DrawRightwards2x2_1to16,
      .draws_to_both_bgs = false,
      .base_width = 2,
      .base_height = 2,
      .min_tiles = 4,  // 2x2 block
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwards1x2_1to16_plus2,
      .name = "Rightwards1x2_1to16_plus2",
      .function = DrawRightwards1x2_1to16_plus2,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 3,
      .min_tiles = 2,  // cap + middle tiles
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsHasEdge1x1_1to16_plus3,
      .name = "RightwardsHasEdge1x1_1to16_plus3",
      .function = DrawRightwardsHasEdge1x1_1to16_plus3,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 1,
      .min_tiles = 3,  // left edge + middle + right edge
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsHasEdge1x1_1to16_plus2,
      .name = "RightwardsHasEdge1x1_1to16_plus2",
      .function = DrawRightwardsHasEdge1x1_1to16_plus2,
      .draws_to_both_bgs = false,
      .base_width = 3,
      .base_height = 1,
      .min_tiles = 3,  // left edge + middle + right edge
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsHasEdge1x1_1to16_plus23,
      .name = "RightwardsHasEdge1x1_1to16_plus23",
      .function = DrawRightwardsHasEdge1x1_1to16_plus23,
      .draws_to_both_bgs = false,
      .base_width = 23,
      .base_height = 1,
      .min_tiles = 3,  // left edge + middle + right edge
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsTopCorners1x2_1to16_plus13,
      .name = "RightwardsTopCorners1x2_1to16_plus13",
      .function = DrawRightwardsTopCorners1x2_1to16_plus13,
      .draws_to_both_bgs = false,
      .base_width = 1,
      .base_height = 2,
      .min_tiles = 6,  // cap + body + endpoint tile spans from subtype-1 table
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsBottomCorners1x2_1to16_plus13,
      .name = "RightwardsBottomCorners1x2_1to16_plus13",
      .function = DrawRightwardsBottomCorners1x2_1to16_plus13,
      .draws_to_both_bgs = false,
      .base_width = 1,
      .base_height = 2,  // spans y+1 to y+2
      .min_tiles = 6,  // cap + body + endpoint tile spans from subtype-1 table
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwards4x4_1to16,
      .name = "Rightwards4x4_1to16",
      .function = DrawRightwards4x4_1to16,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 4,
      .min_tiles = 16,  // 4x4 block
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwards1x1Solid_1to16_plus3,
      .name = "Rightwards1x1Solid_1to16_plus3",
      .function = DrawRightwards1x1Solid_1to16_plus3,
      .draws_to_both_bgs = false,
      .base_width = 1,
      .base_height = 1,
      .min_tiles = 4,  // solid fill uses 4 directional tiles
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsDecor4x4spaced2_1to16,
      .name = "RightwardsDecor4x4spaced2_1to16",
      .function = DrawRightwardsDecor4x4spaced2_1to16,
      .draws_to_both_bgs = false,
      .base_width = 6,  // 4 tiles + 2 spacing
      .base_height = 4,
      .min_tiles = 16,  // 4x4 block
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsStatue2x3spaced2_1to16,
      .name = "RightwardsStatue2x3spaced2_1to16",
      .function = DrawRightwardsStatue2x3spaced2_1to16,
      .draws_to_both_bgs = false,
      .base_width = 4,  // 2 tiles + 2 spacing
      .base_height = 3,
      .min_tiles = 6,  // 2x3 block
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsPillar2x4spaced4_1to16,
      .name = "RightwardsPillar2x4spaced4_1to16",
      .function = DrawRightwardsPillar2x4spaced4_1to16,
      .draws_to_both_bgs = false,
      .base_width = 6,  // 2 tiles + 4 spacing
      .base_height = 4,
      .min_tiles = 8,  // 2x4 block
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsDecor4x3spaced4_1to16,
      .name = "RightwardsDecor4x3spaced4_1to16",
      .function = DrawRightwardsDecor4x3spaced4_1to16,
      .draws_to_both_bgs = false,
      .base_width =
          6,  // 4 tiles + 2 spacing (actually the calculation seems off, kept as is)
      .base_height = 3,
      .min_tiles = 12,  // 4x3 block
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsDoubled2x2spaced2_1to16,
      .name = "RightwardsDoubled2x2spaced2_1to16",
      .function = DrawRightwardsDoubled2x2spaced2_1to16,
      .draws_to_both_bgs = false,
      .base_width = 6,  // 4 tiles + 2 spacing
      .base_height = 2,
      .min_tiles = 8,  // 4x2 block
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsDecor2x2spaced12_1to16,
      .name = "RightwardsDecor2x2spaced12_1to16",
      .function = DrawRightwardsDecor2x2spaced12_1to16,
      .draws_to_both_bgs = false,
      .base_width = 14,  // 2 tiles + 12 spacing
      .base_height = 2,
      .min_tiles = 4,  // 2x2 block
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwards4x2_1to16,
      .name = "Rightwards4x2_1to16",
      .function = DrawRightwards4x2_1to16,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 2,
      .min_tiles = 8,  // 4x2 block
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsDecor4x2spaced8_1to16,
      .name = "RightwardsDecor4x2spaced8_1to16",
      .function = DrawRightwardsDecor4x2spaced8_1to16,
      .draws_to_both_bgs = false,
      .base_width = 12,  // 1 tile + 11-tile gap (12-tile step)
      .base_height = 8,
      .min_tiles = 8,  // 1x8 column
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsCannonHole4x3_1to16,
      .name = "RightwardsCannonHole4x3_1to16",
      .function = DrawRightwardsCannonHole4x3_1to16,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 3,
      .min_tiles = 12,  // 4x3 block
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsLine1x1_1to16plus1,
      .name = "RightwardsLine1x1_1to16plus1",
      .function = DrawRightwardsLine1x1_1to16plus1,
      .draws_to_both_bgs = false,
      .base_width = 1,
      .base_height = 1,
      .min_tiles = 1,
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsBar4x3_1to16,
      .name = "RightwardsBar4x3_1to16",
      .function = DrawRightwardsBar4x3_1to16,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 3,
      .min_tiles = 12,
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsShelf4x4_1to16,
      .name = "RightwardsShelf4x4_1to16",
      .function = DrawRightwardsShelf4x4_1to16,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 4,
      .min_tiles = 16,
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsBigRail1x3_1to16plus5,
      .name = "RightwardsBigRail1x3_1to16plus5",
      .function = DrawRightwardsBigRail1x3_1to16plus5,
      .draws_to_both_bgs = false,
      .base_width = 6,
      .base_height = 3,
      .min_tiles = 15,
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsBlock2x2spaced2_1to16,
      .name = "RightwardsBlock2x2spaced2_1to16",
      .function = DrawRightwardsBlock2x2spaced2_1to16,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 2,
      .min_tiles = 4,
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsEdge1x1_1to16plus7,
      .name = "RightwardsEdge1x1_1to16plus7",
      .function = DrawRightwardsEdge1x1_1to16plus7,
      .draws_to_both_bgs = false,
      .base_width = 8,
      .base_height = 1,
      .min_tiles = 1,
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsPots2x2_1to16,
      .name = "RightwardsPots2x2_1to16",
      .function = DrawRightwardsPots2x2_1to16,
      .draws_to_both_bgs = false,
      .base_width = 2,
      .base_height = 2,
      .min_tiles = 4,
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsHammerPegs2x2_1to16,
      .name = "RightwardsHammerPegs2x2_1to16",
      .function = DrawRightwardsHammerPegs2x2_1to16,
      .draws_to_both_bgs = false,
      .base_width = 2,
      .base_height = 2,
      .min_tiles = 4,
      .category = DrawRoutineInfo::Category::Rightwards,
  });
}

}  // namespace draw_routines
}  // namespace zelda3
}  // namespace yaze
