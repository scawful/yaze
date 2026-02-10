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
  if (size == 0) size = 32;  // Special case for object 0x60

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
  if (size == 0) size = 26;  // Special case

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
  // This is 4 columns × 2 rows = 8 tiles in COLUMN-MAJOR order with 6-tile Y
  // spacing
  int size = ctx.object.size_ & 0x0F;

  // Assembly: GetSize_1to16, so count = size + 1
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (ctx.tiles.size() >= 8) {
      // Draw 4x2 pattern in COLUMN-MAJOR order
      // Column 0 (tiles 0-1)
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_,
                                    ctx.object.y_ + (s * 6),
                                    ctx.tiles[0]);  // col 0, row 0
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_,
                                    ctx.object.y_ + (s * 6) + 1,
                                    ctx.tiles[1]);  // col 0, row 1
      // Column 1 (tiles 2-3)
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1,
                                    ctx.object.y_ + (s * 6),
                                    ctx.tiles[2]);  // col 1, row 0
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1,
                                    ctx.object.y_ + (s * 6) + 1,
                                    ctx.tiles[3]);  // col 1, row 1
      // Column 2 (tiles 4-5)
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 2,
                                    ctx.object.y_ + (s * 6),
                                    ctx.tiles[4]);  // col 2, row 0
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 2,
                                    ctx.object.y_ + (s * 6) + 1,
                                    ctx.tiles[5]);  // col 2, row 1
      // Column 3 (tiles 6-7)
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 3,
                                    ctx.object.y_ + (s * 6),
                                    ctx.tiles[6]);  // col 3, row 0
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 3,
                                    ctx.object.y_ + (s * 6) + 1,
                                    ctx.tiles[7]);  // col 3, row 1
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

  if (ctx.tiles.size() < 3) return;

  int y = ctx.object.y_;
  DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, y, ctx.tiles[0]);
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
  int size = ctx.object.size_ & 0x0F;

  // Assembly: GetSize_1to16_timesA(0x0A), so count = size + 10
  int count = size + 10;

  for (int s = 0; s < count; s++) {
    if (ctx.tiles.size() >= 2) {
      // Use first tile span for 2x1 pattern
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 12,
                                    ctx.object.y_ + s, ctx.tiles[0]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 12 + 1,
                                    ctx.object.y_ + s, ctx.tiles[1]);
    }
  }
}

void DrawDownwardsRightCorners2x1_1to16_plus12(const DrawContext& ctx) {
  // Pattern: Right corner 2x1 tiles with +12 offset downward (object 0x6D)
  int size = ctx.object.size_ & 0x0F;

  // Assembly: GetSize_1to16_timesA(0x0A), so count = size + 10
  int count = size + 10;

  for (int s = 0; s < count; s++) {
    if (ctx.tiles.size() >= 2) {
      // Use first tile span for 2x1 pattern
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 12,
                                    ctx.object.y_ + s, ctx.tiles[0]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 12 + 1,
                                    ctx.object.y_ + s, ctx.tiles[1]);
    }
  }
}

void RegisterDownwardsRoutines(std::vector<DrawRoutineInfo>& registry) {
  using Category = DrawRoutineInfo::Category;

  registry.push_back(DrawRoutineInfo{
      .id = 7,  // RoomDraw_Downwards2x2_1to15or32
      .name = "Downwards2x2_1to15or32",
      .function = DrawDownwards2x2_1to15or32,
      .draws_to_both_bgs = false,
      .base_width = 2,
      .base_height = 2,
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
      .category = Category::Downwards});

  registry.push_back(DrawRoutineInfo{
      .id = 9,  // RoomDraw_Downwards4x2_1to16_BothBG
      .name = "Downwards4x2_1to16_BothBG",
      .function = DrawDownwards4x2_1to16_BothBG,
      .draws_to_both_bgs = true,
      .base_width = 4,
      .base_height = 2,
      .category = Category::Downwards});

  registry.push_back(DrawRoutineInfo{
      .id = 10,  // RoomDraw_DownwardsDecor4x2spaced4_1to16
      .name = "DownwardsDecor4x2spaced4_1to16",
      .function = DrawDownwardsDecor4x2spaced4_1to16,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 2,
      .category = Category::Downwards});

  registry.push_back(DrawRoutineInfo{
      .id = 11,  // RoomDraw_Downwards2x2_1to16
      .name = "Downwards2x2_1to16",
      .function = DrawDownwards2x2_1to16,
      .draws_to_both_bgs = false,
      .base_width = 2,
      .base_height = 2,
      .category = Category::Downwards});

  registry.push_back(DrawRoutineInfo{
      .id = 12,  // RoomDraw_DownwardsHasEdge1x1_1to16_plus3
      .name = "DownwardsHasEdge1x1_1to16_plus3",
      .function = DrawDownwardsHasEdge1x1_1to16_plus3,
      .draws_to_both_bgs = false,
      .base_width = 1,
      .base_height = 3,
      .category = Category::Downwards});

  registry.push_back(DrawRoutineInfo{
      .id = 13,  // RoomDraw_DownwardsEdge1x1_1to16
      .name = "DownwardsEdge1x1_1to16",
      .function = DrawDownwardsEdge1x1_1to16,
      .draws_to_both_bgs = false,
      .base_width = 1,
      .base_height = 1,
      .category = Category::Downwards});

  registry.push_back(DrawRoutineInfo{
      .id = 14,  // RoomDraw_DownwardsLeftCorners2x1_1to16_plus12
      .name = "DownwardsLeftCorners2x1_1to16_plus12",
      .function = DrawDownwardsLeftCorners2x1_1to16_plus12,
      .draws_to_both_bgs = false,
      .base_width = 2,
      .base_height = 1,
      .category = Category::Downwards});

  registry.push_back(DrawRoutineInfo{
      .id = 15,  // RoomDraw_DownwardsRightCorners2x1_1to16_plus12
      .name = "DownwardsRightCorners2x1_1to16_plus12",
      .function = DrawDownwardsRightCorners2x1_1to16_plus12,
      .draws_to_both_bgs = false,
      .base_width = 2,
      .base_height = 1,
      .category = Category::Downwards});
}

}  // namespace draw_routines
}  // namespace zelda3
}  // namespace yaze
