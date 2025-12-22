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
                                   ctx.object.y_, ctx.tiles[0]);  // col 0, row 0
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2),
                                   ctx.object.y_ + 1, ctx.tiles[1]);  // col 0, row 1
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2) + 1,
                                   ctx.object.y_, ctx.tiles[2]);  // col 1, row 0
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2) + 1,
                                   ctx.object.y_ + 1, ctx.tiles[3]);  // col 1, row 1
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
                                   ctx.object.y_, ctx.tiles[0]);  // col 0, row 0
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2),
                                   ctx.object.y_ + 1, ctx.tiles[1]);  // col 0, row 1
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2),
                                   ctx.object.y_ + 2, ctx.tiles[2]);  // col 0, row 2
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2),
                                   ctx.object.y_ + 3, ctx.tiles[3]);  // col 0, row 3
      // Column 1 (tiles 4-7)
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2) + 1,
                                   ctx.object.y_, ctx.tiles[4]);  // col 1, row 0
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2) + 1,
                                   ctx.object.y_ + 1, ctx.tiles[5]);  // col 1, row 1
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2) + 1,
                                   ctx.object.y_ + 2, ctx.tiles[6]);  // col 1, row 2
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2) + 1,
                                   ctx.object.y_ + 3, ctx.tiles[7]);  // col 1, row 3
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
                                   ctx.object.y_, ctx.tiles[0]);  // col 0, row 0
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2),
                                   ctx.object.y_ + 1, ctx.tiles[1]);  // col 0, row 1
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2),
                                   ctx.object.y_ + 2, ctx.tiles[2]);  // col 0, row 2
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2),
                                   ctx.object.y_ + 3, ctx.tiles[3]);  // col 0, row 3
      // Column 1 (tiles 4-7)
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2) + 1,
                                   ctx.object.y_, ctx.tiles[4]);  // col 1, row 0
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2) + 1,
                                   ctx.object.y_ + 1, ctx.tiles[5]);  // col 1, row 1
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2) + 1,
                                   ctx.object.y_ + 2, ctx.tiles[6]);  // col 1, row 2
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2) + 1,
                                   ctx.object.y_ + 3, ctx.tiles[7]);  // col 1, row 3
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
  // Pattern: Same as DrawRightwards2x4_1to16 but draws to both BG1 and BG2 (objects 0x05-0x06)
  DrawRightwards2x4_1to16(ctx);
  // Note: BothBG would require access to both buffers - simplified for now
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
                                   ctx.object.y_, ctx.tiles[0]);  // col 0, row 0
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2),
                                   ctx.object.y_ + 1, ctx.tiles[1]);  // col 0, row 1
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2) + 1,
                                   ctx.object.y_, ctx.tiles[2]);  // col 1, row 0
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 2) + 1,
                                   ctx.object.y_ + 1, ctx.tiles[3]);  // col 1, row 1
    }
  }
}

void DrawRightwards1x2_1to16_plus2(const DrawContext& ctx) {
  // Pattern: 1x2 tiles rightward with +2 offset (object 0x21)
  int size = ctx.object.size_ & 0x0F;

  // Assembly: (size << 1) + 1 = (size * 2) + 1
  int count = (size * 2) + 1;

  for (int s = 0; s < count; s++) {
    if (ctx.tiles.size() >= 2) {
      // Use first tile span for 1x2 pattern
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + s + 2,
                                   ctx.object.y_, ctx.tiles[0]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + s + 2,
                                   ctx.object.y_ + 1, ctx.tiles[1]);
    }
  }
}

void DrawRightwardsHasEdge1x1_1to16_plus3(const DrawContext& ctx) {
  // Pattern: 1x1 tiles with edge detection +3 offset (object 0x22)
  int size = ctx.object.size_ & 0x0F;

  // Assembly: GetSize_1to16_timesA(2), so count = size + 2
  int count = size + 2;

  for (int s = 0; s < count; s++) {
    if (ctx.tiles.size() >= 1) {
      // Use first 8x8 tile from span
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + s + 3,
                                   ctx.object.y_, ctx.tiles[0]);
    }
  }
}

void DrawRightwardsHasEdge1x1_1to16_plus2(const DrawContext& ctx) {
  // Pattern: 1x1 tiles with edge detection +2 offset (objects 0x23-0x2E, 0x3F-0x46)
  int size = ctx.object.size_ & 0x0F;

  // Assembly: GetSize_1to16, so count = size + 1
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (ctx.tiles.size() >= 1) {
      // Use first 8x8 tile from span
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + s + 2,
                                   ctx.object.y_, ctx.tiles[0]);
    }
  }
}

void DrawRightwardsTopCorners1x2_1to16_plus13(const DrawContext& ctx) {
  // Pattern: Top corner 1x2 tiles with +13 offset (object 0x2F)
  int size = ctx.object.size_ & 0x0F;

  // Assembly: GetSize_1to16_timesA(0x0A), so count = size + 10
  int count = size + 10;

  for (int s = 0; s < count; s++) {
    if (ctx.tiles.size() >= 2) {
      // Use first tile span for 1x2 pattern
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + s + 13,
                                   ctx.object.y_, ctx.tiles[0]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + s + 13,
                                   ctx.object.y_ + 1, ctx.tiles[1]);
    }
  }
}

void DrawRightwardsBottomCorners1x2_1to16_plus13(const DrawContext& ctx) {
  // Pattern: Bottom corner 1x2 tiles with +13 offset (object 0x30)
  int size = ctx.object.size_ & 0x0F;

  // Assembly: GetSize_1to16_timesA(0x0A), so count = size + 10
  int count = size + 10;

  for (int s = 0; s < count; s++) {
    if (ctx.tiles.size() >= 2) {
      // Use first tile span for 1x2 pattern
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + s + 13,
                                   ctx.object.y_ + 1, ctx.tiles[0]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + s + 13,
                                   ctx.object.y_ + 2, ctx.tiles[1]);
    }
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
          DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 4) + x,
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
          DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 6) + x,
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
          DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 4) + x,
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
          DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 6) + x,
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
          DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 6) + x,
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
          DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 6) + x,
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
                                   ctx.object.y_, ctx.tiles[0]);  // col 0, row 0
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 14),
                                   ctx.object.y_ + 1, ctx.tiles[1]);  // col 0, row 1
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 14) + 1,
                                   ctx.object.y_, ctx.tiles[2]);  // col 1, row 0
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + (s * 14) + 1,
                                   ctx.object.y_ + 1, ctx.tiles[3]);  // col 1, row 1
    }
  }
}

void RegisterRightwardsRoutines(std::vector<DrawRoutineInfo>& registry) {
  // Note: Routine IDs are assigned based on the assembly routine table
  // These rightwards routines are part of the core 40 draw routines
  // Uses canonical IDs from DrawRoutineIds namespace

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwards2x2_1to15or32,
      .name = "Rightwards2x2_1to15or32",
      .function = DrawRightwards2x2_1to15or32,
      .draws_to_both_bgs = false,
      .base_width = 2,
      .base_height = 2,
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwards2x4_1to15or26,
      .name = "Rightwards2x4_1to15or26",
      .function = DrawRightwards2x4_1to15or26,
      .draws_to_both_bgs = false,
      .base_width = 2,
      .base_height = 4,
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwards2x4_1to16,
      .name = "Rightwards2x4_1to16",
      .function = DrawRightwards2x4_1to16,
      .draws_to_both_bgs = false,
      .base_width = 2,  // Adjacent spacing (s * 2)
      .base_height = 4,
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwards2x4_1to16_BothBG,
      .name = "Rightwards2x4_1to16_BothBG",
      .function = DrawRightwards2x4_1to16_BothBG,
      .draws_to_both_bgs = true,
      .base_width = 2,  // Adjacent spacing (s * 2)
      .base_height = 4,
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwards2x2_1to16,
      .name = "Rightwards2x2_1to16",
      .function = DrawRightwards2x2_1to16,
      .draws_to_both_bgs = false,
      .base_width = 2,
      .base_height = 2,
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwards1x2_1to16_plus2,
      .name = "Rightwards1x2_1to16_plus2",
      .function = DrawRightwards1x2_1to16_plus2,
      .draws_to_both_bgs = false,
      .base_width = 1,
      .base_height = 2,
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsHasEdge1x1_1to16_plus3,
      .name = "RightwardsHasEdge1x1_1to16_plus3",
      .function = DrawRightwardsHasEdge1x1_1to16_plus3,
      .draws_to_both_bgs = false,
      .base_width = 1,
      .base_height = 1,
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsHasEdge1x1_1to16_plus2,
      .name = "RightwardsHasEdge1x1_1to16_plus2",
      .function = DrawRightwardsHasEdge1x1_1to16_plus2,
      .draws_to_both_bgs = false,
      .base_width = 1,
      .base_height = 1,
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsTopCorners1x2_1to16_plus13,
      .name = "RightwardsTopCorners1x2_1to16_plus13",
      .function = DrawRightwardsTopCorners1x2_1to16_plus13,
      .draws_to_both_bgs = false,
      .base_width = 1,
      .base_height = 2,
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsBottomCorners1x2_1to16_plus13,
      .name = "RightwardsBottomCorners1x2_1to16_plus13",
      .function = DrawRightwardsBottomCorners1x2_1to16_plus13,
      .draws_to_both_bgs = false,
      .base_width = 1,
      .base_height = 2,  // spans y+1 to y+2
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwards4x4_1to16,
      .name = "Rightwards4x4_1to16",
      .function = DrawRightwards4x4_1to16,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 4,
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwards1x1Solid_1to16_plus3,
      .name = "Rightwards1x1Solid_1to16_plus3",
      .function = DrawRightwards1x1Solid_1to16_plus3,
      .draws_to_both_bgs = false,
      .base_width = 1,
      .base_height = 1,
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsDecor4x4spaced2_1to16,
      .name = "RightwardsDecor4x4spaced2_1to16",
      .function = DrawRightwardsDecor4x4spaced2_1to16,
      .draws_to_both_bgs = false,
      .base_width = 6,  // 4 tiles + 2 spacing
      .base_height = 4,
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsStatue2x3spaced2_1to16,
      .name = "RightwardsStatue2x3spaced2_1to16",
      .function = DrawRightwardsStatue2x3spaced2_1to16,
      .draws_to_both_bgs = false,
      .base_width = 4,  // 2 tiles + 2 spacing
      .base_height = 3,
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsPillar2x4spaced4_1to16,
      .name = "RightwardsPillar2x4spaced4_1to16",
      .function = DrawRightwardsPillar2x4spaced4_1to16,
      .draws_to_both_bgs = false,
      .base_width = 6,  // 2 tiles + 4 spacing
      .base_height = 4,
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsDecor4x3spaced4_1to16,
      .name = "RightwardsDecor4x3spaced4_1to16",
      .function = DrawRightwardsDecor4x3spaced4_1to16,
      .draws_to_both_bgs = false,
      .base_width = 6,  // 4 tiles + 2 spacing (actually the calculation seems off, kept as is)
      .base_height = 3,
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsDoubled2x2spaced2_1to16,
      .name = "RightwardsDoubled2x2spaced2_1to16",
      .function = DrawRightwardsDoubled2x2spaced2_1to16,
      .draws_to_both_bgs = false,
      .base_width = 6,  // 4 tiles + 2 spacing
      .base_height = 2,
      .category = DrawRoutineInfo::Category::Rightwards,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwardsDecor2x2spaced12_1to16,
      .name = "RightwardsDecor2x2spaced12_1to16",
      .function = DrawRightwardsDecor2x2spaced12_1to16,
      .draws_to_both_bgs = false,
      .base_width = 14,  // 2 tiles + 12 spacing
      .base_height = 2,
      .category = DrawRoutineInfo::Category::Rightwards,
  });
}

}  // namespace draw_routines
}  // namespace zelda3
}  // namespace yaze
