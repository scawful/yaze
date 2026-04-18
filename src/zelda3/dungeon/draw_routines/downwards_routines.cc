#include "downwards_routines.h"

#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {
namespace draw_routines {

void DrawDownwards2x2_1to15or32(const DrawContext& ctx) {
  // Pattern: Draws 2x2 tiles downward (object 0x60)
  // Size byte determines how many times to repeat (1-15 or 32)
  int size = ctx.object.size_;
  if (size == 0)
    size = 32;  // Special case for object 0x60

  for (int s = 0; s < size; s++) {
    if (ctx.tiles.size() >= 4) {
      // Draw 2x2 pattern in COLUMN-MAJOR order (matching assembly)
      // Assembly uses indirect pointers: $BF, $CB, $C2, $CE
      // tiles[0] → $BF → (col 0, row 0) = top-left
      // tiles[1] → $CB → (col 0, row 1) = bottom-left
      // tiles[2] → $C2 → (col 1, row 0) = top-right
      // tiles[3] → $CE → (col 1, row 1) = bottom-right
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_,
                                   ctx.object.y_ + (s * 2),
                                   ctx.tiles[0]);  // col 0, row 0
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_,
                                   ctx.object.y_ + (s * 2) + 1,
                                   ctx.tiles[1]);  // col 0, row 1
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1,
                                   ctx.object.y_ + (s * 2),
                                   ctx.tiles[2]);  // col 1, row 0
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1,
                                   ctx.object.y_ + (s * 2) + 1,
                                   ctx.tiles[3]);  // col 1, row 1
    }
  }
}

void DrawDownwards4x2_1to15or26(const DrawContext& ctx) {
  // Pattern: Draws 4x2 tiles downward (objects 0x61-0x62)
  // This is 4 columns × 2 rows = 8 tiles in ROW-MAJOR order (per ZScream)
  int size = ctx.object.size_;
  if (size == 0)
    size = 26;  // Special case

  for (int s = 0; s < size; s++) {
    if (ctx.tiles.size() >= 8) {
      // Draw 4x2 pattern in ROW-MAJOR order (matching ZScream)
      // Row 0: tiles 0, 1, 2, 3 at x+0, x+1, x+2, x+3
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_,
                                   ctx.object.y_ + (s * 2), ctx.tiles[0]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1,
                                   ctx.object.y_ + (s * 2), ctx.tiles[1]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 2,
                                   ctx.object.y_ + (s * 2), ctx.tiles[2]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 3,
                                   ctx.object.y_ + (s * 2), ctx.tiles[3]);
      // Row 1: tiles 4, 5, 6, 7 at x+0, x+1, x+2, x+3
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_,
                                   ctx.object.y_ + (s * 2) + 1, ctx.tiles[4]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1,
                                   ctx.object.y_ + (s * 2) + 1, ctx.tiles[5]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 2,
                                   ctx.object.y_ + (s * 2) + 1, ctx.tiles[6]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 3,
                                   ctx.object.y_ + (s * 2) + 1, ctx.tiles[7]);
    } else if (ctx.tiles.size() >= 4) {
      // Fallback: with 4 tiles draw 4x1 row pattern
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_,
                                   ctx.object.y_ + (s * 2), ctx.tiles[0]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1,
                                   ctx.object.y_ + (s * 2), ctx.tiles[1]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 2,
                                   ctx.object.y_ + (s * 2), ctx.tiles[2]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 3,
                                   ctx.object.y_ + (s * 2), ctx.tiles[3]);
    }
  }
}

void DrawDownwards4x2_1to16_BothBG(const DrawContext& ctx) {
  // Pattern: Same as above but draws to both BG1 and BG2 (objects 0x63-0x64)
  DrawDownwards4x2_1to15or26(ctx);
  // Note: BothBG would require access to both buffers - simplified for now
}

void DrawDownwardsDecor4x2spaced4_1to16(const DrawContext& ctx) {
  // Pattern: Draws 4x2 decoration downward with spacing (objects 0x65-0x66)
  // This is 4 columns × 2 rows = 8 tiles in ROW-MAJOR order with 6-tile Y
  // spacing.
  int size = ctx.object.size_ & 0x0F;

  // Assembly: GetSize_1to16, so count = size + 1
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (ctx.tiles.size() >= 8) {
      // Draw 4x2 pattern in ROW-MAJOR order:
      // Row 0: tiles[0..3], Row 1: tiles[4..7].
      const int base_y = ctx.object.y_ + (s * 6);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, base_y,
                                   ctx.tiles[0]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1, base_y,
                                   ctx.tiles[1]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 2, base_y,
                                   ctx.tiles[2]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 3, base_y,
                                   ctx.tiles[3]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, base_y + 1,
                                   ctx.tiles[4]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1, base_y + 1,
                                   ctx.tiles[5]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 2, base_y + 1,
                                   ctx.tiles[6]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 3, base_y + 1,
                                   ctx.tiles[7]);
    }
  }
}

void DrawDownwards2x2_1to16(const DrawContext& ctx) {
  // Pattern: Draws 2x2 tiles downward (objects 0x67-0x68)
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
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_,
                                   ctx.object.y_ + (s * 2),
                                   ctx.tiles[0]);  // col 0, row 0
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_,
                                   ctx.object.y_ + (s * 2) + 1,
                                   ctx.tiles[1]);  // col 0, row 1
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1,
                                   ctx.object.y_ + (s * 2),
                                   ctx.tiles[2]);  // col 1, row 0
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1,
                                   ctx.object.y_ + (s * 2) + 1,
                                   ctx.tiles[3]);  // col 1, row 1
    }
  }
}

void DrawDownwardsHasEdge1x1_1to16_plus3(const DrawContext& ctx) {
  // Pattern: Vertical rail with corner/middle/end (object 0x69)
  int size = ctx.object.size_ & 0x0F;
  int count = size + 1;

  if (ctx.tiles.size() < 3)
    return;

  int y = ctx.object.y_;
  // USDASM $01:8EC9-$01:8ED4 suppresses the leading corner when the existing
  // tile already contains the small vertical rail corner.
  if (!DrawRoutineUtils::ExistingTileMatchesAny(ctx.target_bg, ctx.object.x_, y,
                                                {0x00E3})) {
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, y, ctx.tiles[0]);
  }
  y++;
  for (int s = 0; s < count; s++) {
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, y, ctx.tiles[1]);
    y++;
  }
  DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, y, ctx.tiles[2]);
}

void DrawDownwardsEdge1x1_1to16(const DrawContext& ctx) {
  // Pattern: 1x1 edge tiles downward (objects 0x6A-0x6B)
  int size = ctx.object.size_ & 0x0F;

  // Assembly: GetSize_1to16, so count = size + 1
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (ctx.tiles.size() >= 1) {
      // Use first 8x8 tile from span
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_,
                                   ctx.object.y_ + s, ctx.tiles[0]);
    }
  }
}

void DrawDownwardsLeftCorners2x1_1to16_plus12(const DrawContext& ctx) {
  // Pattern: Left corner 2x1 tiles with +12 offset downward (object 0x6C)
  const int size = ctx.object.size_ & 0x0F;
  // USDASM $01:9045 - Object_Size_N_to_N_plus_15 with N=0x0A.
  const int count = size + 10;
  if (count <= 0 || ctx.tiles.empty()) {
    return;
  }

  const int base_x = ctx.object.x_ + 12;
  int current_y = ctx.object.y_;

  const gfx::TileInfo& fill = ctx.tiles[0];
  const gfx::TileInfo& start_top_left =
      ctx.tiles.size() > 1 ? ctx.tiles[1] : fill;
  const gfx::TileInfo& start_bottom_left =
      ctx.tiles.size() > 2 ? ctx.tiles[2] : fill;
  const gfx::TileInfo& body_left =
      ctx.tiles.size() > 3 ? ctx.tiles[3] : fill;
  const gfx::TileInfo& end_top_left =
      ctx.tiles.size() > 4 ? ctx.tiles[4] : body_left;
  const gfx::TileInfo& end_bottom_left =
      ctx.tiles.size() > 5 ? ctx.tiles[5] : fill;

  if (!DrawRoutineUtils::ExistingTileMatchesAny(ctx.target_bg, base_x, current_y,
                                                {0x00E3})) {
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x, current_y,
                                 start_top_left);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, current_y, fill);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x, current_y + 1,
                                 start_bottom_left);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, current_y + 1,
                                 fill);
    current_y += 2;
  }

  for (int s = 0; s < count; ++s) {
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x, current_y + s,
                                 body_left);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, current_y + s,
                                 fill);
  }

  current_y += count;
  DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x, current_y, end_top_left);
  DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, current_y, fill);
  DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x, current_y + 1,
                               end_bottom_left);
  DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, current_y + 1, fill);
}

void DrawDownwardsRightCorners2x1_1to16_plus12(const DrawContext& ctx) {
  // Pattern: Right corner 2x1 tiles with +12 offset downward (object 0x6D)
  const int size = ctx.object.size_ & 0x0F;
  // USDASM $01:908F - mirrored variant of the left-corner routine.
  const int count = size + 10;
  if (count <= 0 || ctx.tiles.empty()) {
    return;
  }

  const int base_x = ctx.object.x_ + 12;
  int current_y = ctx.object.y_;

  const gfx::TileInfo& fill = ctx.tiles[0];
  const gfx::TileInfo& start_top_right =
      ctx.tiles.size() > 1 ? ctx.tiles[1] : fill;
  const gfx::TileInfo& start_bottom_right =
      ctx.tiles.size() > 2 ? ctx.tiles[2] : fill;
  const gfx::TileInfo& body_right =
      ctx.tiles.size() > 3 ? ctx.tiles[3] : fill;
  const gfx::TileInfo& end_top_right =
      ctx.tiles.size() > 4 ? ctx.tiles[4] : body_right;
  const gfx::TileInfo& end_bottom_right =
      ctx.tiles.size() > 5 ? ctx.tiles[5] : fill;

  if (!DrawRoutineUtils::ExistingTileMatchesAny(ctx.target_bg, base_x + 1,
                                                current_y, {0x00E3})) {
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x, current_y, fill);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, current_y,
                                 start_top_right);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x, current_y + 1, fill);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, current_y + 1,
                                 start_bottom_right);
    current_y += 2;
  }

  for (int s = 0; s < count; ++s) {
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x, current_y + s, fill);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, current_y + s,
                                 body_right);
  }

  current_y += count;
  DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x, current_y, fill);
  DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, current_y,
                               end_top_right);
  DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x, current_y + 1, fill);
  DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, current_y + 1,
                               end_bottom_right);
}

void DrawDownwardsFloor4x4_1to16(const DrawContext& ctx) {
  const int size = ctx.object.size_ & 0x0F;
  const int count = size + 1;
  if (ctx.tiles.size() < 16) {
    return;
  }

  for (int s = 0; s < count; ++s) {
    const int base_y = ctx.object.y_ + (s * 4);
    for (int x = 0; x < 4; ++x) {
      for (int y = 0; y < 4; ++y) {
        DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + x,
                                     base_y + y, ctx.tiles[x * 4 + y]);
      }
    }
  }
}

void DrawDownwards1x1Solid_1to16_plus3(const DrawContext& ctx) {
  const int size = ctx.object.size_ & 0x0F;
  const int count = size + 4;
  if (ctx.tiles.empty()) {
    return;
  }

  for (int s = 0; s < count; ++s) {
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_,
                                 ctx.object.y_ + s + 3, ctx.tiles[0]);
  }
}

void DrawDownwardsDecor4x4spaced2_1to16(const DrawContext& ctx) {
  const int size = ctx.object.size_ & 0x0F;
  const int count = size + 1;
  if (ctx.tiles.size() < 16) {
    return;
  }

  for (int s = 0; s < count; ++s) {
    const int base_y = ctx.object.y_ + (s * 6);
    for (int x = 0; x < 4; ++x) {
      for (int y = 0; y < 4; ++y) {
        DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + x,
                                     base_y + y, ctx.tiles[x * 4 + y]);
      }
    }
  }
}

void DrawDownwardsPillar2x4spaced2_1to16(const DrawContext& ctx) {
  const int size = ctx.object.size_ & 0x0F;
  const int count = size + 1;
  if (ctx.tiles.size() < 8) {
    return;
  }

  for (int s = 0; s < count; ++s) {
    const int base_y = ctx.object.y_ + (s * 6);
    for (int x = 0; x < 2; ++x) {
      for (int y = 0; y < 4; ++y) {
        DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + x,
                                     base_y + y, ctx.tiles[x * 4 + y]);
      }
    }
  }
}

void DrawDownwardsDecor3x4spaced4_1to16(const DrawContext& ctx) {
  const int size = ctx.object.size_ & 0x0F;
  const int count = size + 1;
  if (ctx.tiles.size() < 12) {
    return;
  }

  for (int s = 0; s < count; ++s) {
    const int base_y = ctx.object.y_ + (s * 8);
    for (int x = 0; x < 3; ++x) {
      for (int y = 0; y < 4; ++y) {
        DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + x,
                                     base_y + y, ctx.tiles[x * 4 + y]);
      }
    }
  }
}

void DrawDownwardsDecor2x2spaced12_1to16(const DrawContext& ctx) {
  const int size = ctx.object.size_ & 0x0F;
  const int count = size + 1;
  if (ctx.tiles.size() < 4) {
    return;
  }

  for (int s = 0; s < count; ++s) {
    const int base_y = ctx.object.y_ + (s * 14);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 0, base_y + 0,
                                 ctx.tiles[0]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 0, base_y + 1,
                                 ctx.tiles[1]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1, base_y + 0,
                                 ctx.tiles[2]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1, base_y + 1,
                                 ctx.tiles[3]);
  }
}

void DrawDownwardsLine1x1_1to16plus1(const DrawContext& ctx) {
  const int size = ctx.object.size_ & 0x0F;
  const int count = size + 2;
  if (ctx.tiles.empty()) {
    return;
  }

  for (int s = 0; s < count; ++s) {
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_,
                                 ctx.object.y_ + s, ctx.tiles[0]);
  }
}

void DrawDownwardsDecor2x4spaced8_1to16(const DrawContext& ctx) {
  const int size = ctx.object.size_ & 0x0F;
  const int count = size + 1;
  if (ctx.tiles.size() < 8) {
    return;
  }

  for (int s = 0; s < count; ++s) {
    const int base_y = ctx.object.y_ + (s * 12);
    for (int x = 0; x < 2; ++x) {
      for (int y = 0; y < 4; ++y) {
        DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + x,
                                     base_y + y, ctx.tiles[x * 4 + y]);
      }
    }
  }
}

void DrawDownwardsDecor3x4spaced2_1to16(const DrawContext& ctx) {
  const int size = ctx.object.size_ & 0x0F;
  const int count = size + 1;
  if (ctx.tiles.size() < 12) {
    return;
  }

  for (int s = 0; s < count; ++s) {
    const int base_y = ctx.object.y_ + (s * 6);
    for (int x = 0; x < 3; ++x) {
      for (int y = 0; y < 4; ++y) {
        DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + x,
                                     base_y + y, ctx.tiles[x * 4 + y]);
      }
    }
  }
}

void DrawDownwardsBigRail3x1_1to16plus5(const DrawContext& ctx) {
  const int size = ctx.object.size_ & 0x0F;
  const int middle_rows = size + 1;
  const int total_rows = size + 6;  // 2 top + (size+1) middle + 3 bottom.

  if (ctx.tiles.size() >= 12) {
    // USDASM $01:8F0C:
    // - Top cap: 2x2 (tiles 0..3, column-major)
    // - Middle: repeated 2x1 rows (tiles 4..5)
    // - Bottom cap: 2 columns x 3 rows (tiles 6..11)
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 0,
                                 ctx.object.y_ + 0, ctx.tiles[0]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 0,
                                 ctx.object.y_ + 1, ctx.tiles[1]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1,
                                 ctx.object.y_ + 0, ctx.tiles[2]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1,
                                 ctx.object.y_ + 1, ctx.tiles[3]);

    int base_y = ctx.object.y_ + 2;
    for (int row = 0; row < middle_rows; ++row) {
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 0,
                                   base_y + row, ctx.tiles[4]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1,
                                   base_y + row, ctx.tiles[5]);
    }

    const int cap_y = base_y + middle_rows;
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 0, cap_y + 0,
                                 ctx.tiles[6]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 0, cap_y + 1,
                                 ctx.tiles[7]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 0, cap_y + 2,
                                 ctx.tiles[8]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1, cap_y + 0,
                                 ctx.tiles[9]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1, cap_y + 1,
                                 ctx.tiles[10]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1, cap_y + 2,
                                 ctx.tiles[11]);
    return;
  }

  if (ctx.tiles.empty() || total_rows <= 0) {
    return;
  }
  const gfx::TileInfo& left = ctx.tiles[0];
  const gfx::TileInfo& right =
      ctx.tiles.size() > 1 ? ctx.tiles[1] : ctx.tiles[0];
  for (int row = 0; row < total_rows; ++row) {
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 0,
                                 ctx.object.y_ + row, left);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1,
                                 ctx.object.y_ + row, right);
  }
}

void DrawDownwardsBlock2x2spaced2_1to16(const DrawContext& ctx) {
  const int size = ctx.object.size_ & 0x0F;
  const int count = size + 1;
  if (ctx.tiles.size() < 4) {
    return;
  }

  for (int s = 0; s < count; ++s) {
    const int base_y = ctx.object.y_ + (s * 4);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 0, base_y + 0,
                                 ctx.tiles[0]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 0, base_y + 1,
                                 ctx.tiles[1]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1, base_y + 0,
                                 ctx.tiles[2]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1, base_y + 1,
                                 ctx.tiles[3]);
  }
}

void DrawDownwardsCannonHole3x4_1to16(const DrawContext& ctx) {
  const int size = ctx.object.size_ & 0x0F;
  const int repeat_count = size + 1;
  if (ctx.tiles.size() < 12) {
    return;
  }

  auto draw_segment = [&](int base_y, int tile_base) {
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 0, base_y + 0,
                                 ctx.tiles[tile_base + 0]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1, base_y + 0,
                                 ctx.tiles[tile_base + 1]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 2, base_y + 0,
                                 ctx.tiles[tile_base + 2]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 0, base_y + 1,
                                 ctx.tiles[tile_base + 3]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1, base_y + 1,
                                 ctx.tiles[tile_base + 4]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 2, base_y + 1,
                                 ctx.tiles[tile_base + 5]);
  };

  // USDASM $01:9CEB:
  // - Repeat the left 3x2 segment (tiles 0..5) size+1 times
  // - Append one 3x2 edge segment (tiles 6..11)
  for (int s = 0; s < repeat_count; ++s) {
    draw_segment(ctx.object.y_ + (s * 2), /*tile_base=*/0);
  }
  draw_segment(ctx.object.y_ + (repeat_count * 2), /*tile_base=*/6);
}

void DrawDownwardsBar2x5_1to16(const DrawContext& ctx) {
  const int size = ctx.object.size_ & 0x0F;
  const int middle_rows = (size + 2) * 2;
  if (ctx.tiles.size() < 4) {
    return;
  }

  // USDASM $01:97B5:
  // - top row uses tiles 0..1
  // - remaining rows use tiles 2..3, repeated 2*(size+2) times
  DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 0,
                               ctx.object.y_ + 0, ctx.tiles[0]);
  DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1,
                               ctx.object.y_ + 0, ctx.tiles[1]);
  for (int row = 0; row < middle_rows; ++row) {
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 0,
                                 ctx.object.y_ + 1 + row, ctx.tiles[2]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1,
                                 ctx.object.y_ + 1 + row, ctx.tiles[3]);
  }
}

void DrawDownwardsPots2x2_1to16(const DrawContext& ctx) {
  const int size = ctx.object.size_ & 0x0F;
  const int count = size + 1;
  if (ctx.tiles.size() < 4) {
    return;
  }

  for (int s = 0; s < count; ++s) {
    const int base_y = ctx.object.y_ + (s * 2);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 0, base_y + 0,
                                 ctx.tiles[0]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 0, base_y + 1,
                                 ctx.tiles[1]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1, base_y + 0,
                                 ctx.tiles[2]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1, base_y + 1,
                                 ctx.tiles[3]);
  }
}

void DrawDownwardsHammerPegs2x2_1to16(const DrawContext& ctx) {
  DrawDownwardsPots2x2_1to16(ctx);
}

void RegisterDownwardsRoutines(std::vector<DrawRoutineInfo>& registry) {
  using Category = DrawRoutineInfo::Category;

  registry.push_back(
      DrawRoutineInfo{.id = 7,  // RoomDraw_Downwards2x2_1to15or32
                      .name = "Downwards2x2_1to15or32",
                      .function = DrawDownwards2x2_1to15or32,
                      .draws_to_both_bgs = false,
                      .base_width = 2,
                      .base_height = 2,
                      .min_tiles = 4,  // 2x2 block
                      .category = Category::Downwards});

  registry.push_back(DrawRoutineInfo{
      .id = 8,  // RoomDraw_Downwards4x2_1to15or26
      .name = "Downwards4x2_1to15or26",
      .function = DrawDownwards4x2_1to15or26,
      // USDASM: RoomDraw_Downwards4x2_1to15or26 ($01:8A89) jumps to
      // RoomDraw_Downwards4x2VariableSpacing ($01:B220) which writes through
      // the current tilemap pointers (single-layer).
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 2,
      .min_tiles = 8,  // 4x2 block
      .category = Category::Downwards});

  registry.push_back(
      DrawRoutineInfo{.id = 9,  // RoomDraw_Downwards4x2_1to16_BothBG
                      .name = "Downwards4x2_1to16_BothBG",
                      .function = DrawDownwards4x2_1to16_BothBG,
                      .draws_to_both_bgs = true,
                      .base_width = 4,
                      .base_height = 2,
                      .min_tiles = 8,  // 4x2 block
                      .category = Category::Downwards});

  registry.push_back(
      DrawRoutineInfo{.id = 10,  // RoomDraw_DownwardsDecor4x2spaced4_1to16
                      .name = "DownwardsDecor4x2spaced4_1to16",
                      .function = DrawDownwardsDecor4x2spaced4_1to16,
                      .draws_to_both_bgs = false,
                      .base_width = 4,
                      .base_height = 2,
                      .min_tiles = 8,  // 4x2 block
                      .category = Category::Downwards});

  registry.push_back(DrawRoutineInfo{.id = 11,  // RoomDraw_Downwards2x2_1to16
                                     .name = "Downwards2x2_1to16",
                                     .function = DrawDownwards2x2_1to16,
                                     .draws_to_both_bgs = false,
                                     .base_width = 2,
                                     .base_height = 2,
                                     .min_tiles = 4,  // 2x2 block
                                     .category = Category::Downwards});

  registry.push_back(
      DrawRoutineInfo{.id = 12,  // RoomDraw_DownwardsHasEdge1x1_1to16_plus3
                      .name = "DownwardsHasEdge1x1_1to16_plus3",
                      .function = DrawDownwardsHasEdge1x1_1to16_plus3,
                      .draws_to_both_bgs = false,
                      .base_width = 1,
                      .base_height = 3,
                      .min_tiles = 3,  // top edge + middle + bottom edge
                      .category = Category::Downwards});

  registry.push_back(
      DrawRoutineInfo{.id = 13,  // RoomDraw_DownwardsEdge1x1_1to16
                      .name = "DownwardsEdge1x1_1to16",
                      .function = DrawDownwardsEdge1x1_1to16,
                      .draws_to_both_bgs = false,
                      .base_width = 1,
                      .base_height = 1,
                      .min_tiles = 1,  // single repeated tile
                      .category = Category::Downwards});

  registry.push_back(DrawRoutineInfo{
      .id = 14,  // RoomDraw_DownwardsLeftCorners2x1_1to16_plus12
      .name = "DownwardsLeftCorners2x1_1to16_plus12",
      .function = DrawDownwardsLeftCorners2x1_1to16_plus12,
      .draws_to_both_bgs = false,
      .base_width = 2,
      .base_height = 1,
      .min_tiles = 6,  // cap + body + endpoint tile spans from subtype-1 table
      .category = Category::Downwards});

  registry.push_back(DrawRoutineInfo{
      .id = 15,  // RoomDraw_DownwardsRightCorners2x1_1to16_plus12
      .name = "DownwardsRightCorners2x1_1to16_plus12",
      .function = DrawDownwardsRightCorners2x1_1to16_plus12,
      .draws_to_both_bgs = false,
      .base_width = 2,
      .base_height = 1,
      .min_tiles = 6,  // cap + body + endpoint tile spans from subtype-1 table
      .category = Category::Downwards});

  registry.push_back(
      DrawRoutineInfo{.id = DrawRoutineIds::kDownwardsFloor4x4_1to16,
                      .name = "DownwardsFloor4x4_1to16",
                      .function = DrawDownwardsFloor4x4_1to16,
                      .draws_to_both_bgs = false,
                      .base_width = 4,
                      .base_height = 4,
                      .min_tiles = 16,
                      .category = Category::Downwards});

  registry.push_back(
      DrawRoutineInfo{.id = DrawRoutineIds::kDownwards1x1Solid_1to16_plus3,
                      .name = "Downwards1x1Solid_1to16_plus3",
                      .function = DrawDownwards1x1Solid_1to16_plus3,
                      .draws_to_both_bgs = false,
                      .base_width = 1,
                      .base_height = 1,
                      .min_tiles = 1,
                      .category = Category::Downwards});

  registry.push_back(
      DrawRoutineInfo{.id = DrawRoutineIds::kDownwardsDecor4x4spaced2_1to16,
                      .name = "DownwardsDecor4x4spaced2_1to16",
                      .function = DrawDownwardsDecor4x4spaced2_1to16,
                      .draws_to_both_bgs = false,
                      .base_width = 4,
                      .base_height = 4,
                      .min_tiles = 16,
                      .category = Category::Downwards});

  registry.push_back(
      DrawRoutineInfo{.id = DrawRoutineIds::kDownwardsPillar2x4spaced2_1to16,
                      .name = "DownwardsPillar2x4spaced2_1to16",
                      .function = DrawDownwardsPillar2x4spaced2_1to16,
                      .draws_to_both_bgs = false,
                      .base_width = 2,
                      .base_height = 4,
                      .min_tiles = 8,
                      .category = Category::Downwards});

  registry.push_back(
      DrawRoutineInfo{.id = DrawRoutineIds::kDownwardsDecor3x4spaced4_1to16,
                      .name = "DownwardsDecor3x4spaced4_1to16",
                      .function = DrawDownwardsDecor3x4spaced4_1to16,
                      .draws_to_both_bgs = false,
                      .base_width = 3,
                      .base_height = 4,
                      .min_tiles = 12,
                      .category = Category::Downwards});

  registry.push_back(
      DrawRoutineInfo{.id = DrawRoutineIds::kDownwardsDecor2x2spaced12_1to16,
                      .name = "DownwardsDecor2x2spaced12_1to16",
                      .function = DrawDownwardsDecor2x2spaced12_1to16,
                      .draws_to_both_bgs = false,
                      .base_width = 2,
                      .base_height = 2,
                      .min_tiles = 4,
                      .category = Category::Downwards});

  registry.push_back(
      DrawRoutineInfo{.id = DrawRoutineIds::kDownwardsLine1x1_1to16plus1,
                      .name = "DownwardsLine1x1_1to16plus1",
                      .function = DrawDownwardsLine1x1_1to16plus1,
                      .draws_to_both_bgs = false,
                      .base_width = 1,
                      .base_height = 1,
                      .min_tiles = 1,
                      .category = Category::Downwards});

  registry.push_back(
      DrawRoutineInfo{.id = DrawRoutineIds::kDownwardsDecor2x4spaced8_1to16,
                      .name = "DownwardsDecor2x4spaced8_1to16",
                      .function = DrawDownwardsDecor2x4spaced8_1to16,
                      .draws_to_both_bgs = false,
                      .base_width = 2,
                      .base_height = 4,
                      .min_tiles = 8,
                      .category = Category::Downwards});

  registry.push_back(
      DrawRoutineInfo{.id = DrawRoutineIds::kDownwardsDecor3x4spaced2_1to16,
                      .name = "DownwardsDecor3x4spaced2_1to16",
                      .function = DrawDownwardsDecor3x4spaced2_1to16,
                      .draws_to_both_bgs = false,
                      .base_width = 3,
                      .base_height = 4,
                      .min_tiles = 12,
                      .category = Category::Downwards});

  registry.push_back(
      DrawRoutineInfo{.id = DrawRoutineIds::kDownwardsBigRail3x1_1to16plus5,
                      .name = "DownwardsBigRail3x1_1to16plus5",
                      .function = DrawDownwardsBigRail3x1_1to16plus5,
                      .draws_to_both_bgs = false,
                      .base_width = 2,
                      .base_height = 6,
                      .min_tiles = 12,
                      .category = Category::Downwards});

  registry.push_back(
      DrawRoutineInfo{.id = DrawRoutineIds::kDownwardsBlock2x2spaced2_1to16,
                      .name = "DownwardsBlock2x2spaced2_1to16",
                      .function = DrawDownwardsBlock2x2spaced2_1to16,
                      .draws_to_both_bgs = false,
                      .base_width = 2,
                      .base_height = 2,
                      .min_tiles = 4,
                      .category = Category::Downwards});

  registry.push_back(
      DrawRoutineInfo{.id = DrawRoutineIds::kDownwardsCannonHole3x4_1to16,
                      .name = "DownwardsCannonHole3x4_1to16",
                      .function = DrawDownwardsCannonHole3x4_1to16,
                      .draws_to_both_bgs = false,
                      .base_width = 3,
                      .base_height = 4,
                      .min_tiles = 12,
                      .category = Category::Downwards});

  registry.push_back(
      DrawRoutineInfo{.id = DrawRoutineIds::kDownwardsBar2x5_1to16,
                      .name = "DownwardsBar2x5_1to16",
                      .function = DrawDownwardsBar2x5_1to16,
                      .draws_to_both_bgs = false,
                      .base_width = 2,
                      .base_height = 5,
                      .min_tiles = 4,
                      .category = Category::Downwards});

  registry.push_back(
      DrawRoutineInfo{.id = DrawRoutineIds::kDownwardsPots2x2_1to16,
                      .name = "DownwardsPots2x2_1to16",
                      .function = DrawDownwardsPots2x2_1to16,
                      .draws_to_both_bgs = false,
                      .base_width = 2,
                      .base_height = 2,
                      .min_tiles = 4,
                      .category = Category::Downwards});

  registry.push_back(
      DrawRoutineInfo{.id = DrawRoutineIds::kDownwardsHammerPegs2x2_1to16,
                      .name = "DownwardsHammerPegs2x2_1to16",
                      .function = DrawDownwardsHammerPegs2x2_1to16,
                      .draws_to_both_bgs = false,
                      .base_width = 2,
                      .base_height = 2,
                      .min_tiles = 4,
                      .category = Category::Downwards});
}

}  // namespace draw_routines
}  // namespace zelda3
}  // namespace yaze
