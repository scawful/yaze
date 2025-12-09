#include "diagonal_routines.h"

#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {
namespace draw_routines {

void DrawDiagonalAcute_1to16(const DrawContext& ctx) {
  // Pattern: Diagonal acute (/) - draws 5 tiles vertically, moves up-right
  // Based on bank_01.asm RoomDraw_DiagonalAcute_1to16 at $018C58
  // Uses RoomDraw_2x2and1 to draw 5 tiles at rows 0-4, then moves Y -= $7E
  int size = ctx.object.size_ & 0x0F;

  // Assembly: LDA #$0007; JSR RoomDraw_GetSize_1to16_timesA
  // GetSize_1to16_timesA formula: ((B2 << 2) | B4) + A
  // Since size_ already contains ((B2 << 2) | B4), count = size + 7
  int count = size + 7;

  if (ctx.tiles.size() < 5) return;

  for (int s = 0; s < count; s++) {
    // Draw 5 tiles in a vertical column (RoomDraw_2x2and1 pattern)
    // Assembly stores to [$BF],Y, [$CB],Y, [$D7],Y, [$DA],Y, [$DD],Y
    // These are rows 0, 1, 2, 3, 4 at the same X position
    int tile_x = ctx.object.x_ + s;  // Move right each iteration
    int tile_y = ctx.object.y_ - s;  // Move up each iteration (acute = /)

    DrawRoutineUtils::WriteTile8(ctx.target_bg, tile_x, tile_y + 0,
                                 ctx.tiles[0]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, tile_x, tile_y + 1,
                                 ctx.tiles[1]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, tile_x, tile_y + 2,
                                 ctx.tiles[2]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, tile_x, tile_y + 3,
                                 ctx.tiles[3]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, tile_x, tile_y + 4,
                                 ctx.tiles[4]);
  }
}

void DrawDiagonalGrave_1to16(const DrawContext& ctx) {
  // Pattern: Diagonal grave (\) - draws 5 tiles vertically, moves down-right
  // Based on bank_01.asm RoomDraw_DiagonalGrave_1to16 at $018C61
  // Uses RoomDraw_2x2and1 to draw 5 tiles at rows 0-4, then moves Y += $82
  int size = ctx.object.size_ & 0x0F;

  // Assembly: LDA #$0007; JSR RoomDraw_GetSize_1to16_timesA
  // GetSize_1to16_timesA formula: ((B2 << 2) | B4) + A
  // Since size_ already contains ((B2 << 2) | B4), count = size + 7
  int count = size + 7;

  if (ctx.tiles.size() < 5) return;

  for (int s = 0; s < count; s++) {
    // Draw 5 tiles in a vertical column (RoomDraw_2x2and1 pattern)
    // Assembly stores to [$BF],Y, [$CB],Y, [$D7],Y, [$DA],Y, [$DD],Y
    // These are rows 0, 1, 2, 3, 4 at the same X position
    int tile_x = ctx.object.x_ + s;  // Move right each iteration
    int tile_y = ctx.object.y_ + s;  // Move down each iteration (grave = \)

    DrawRoutineUtils::WriteTile8(ctx.target_bg, tile_x, tile_y + 0,
                                 ctx.tiles[0]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, tile_x, tile_y + 1,
                                 ctx.tiles[1]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, tile_x, tile_y + 2,
                                 ctx.tiles[2]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, tile_x, tile_y + 3,
                                 ctx.tiles[3]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, tile_x, tile_y + 4,
                                 ctx.tiles[4]);
  }
}

void DrawDiagonalAcute_1to16_BothBG(const DrawContext& ctx) {
  // Pattern: Diagonal acute (/) for both BG layers (objects 0x15, 0x18-0x1D,
  // 0x20) Based on bank_01.asm RoomDraw_DiagonalAcute_1to16_BothBG at $018C6A
  // Assembly: LDA #$0006; JSR RoomDraw_GetSize_1to16_timesA
  // GetSize_1to16_timesA formula: ((B2 << 2) | B4) + A
  // Since size_ already contains ((B2 << 2) | B4), count = size + 6
  int size = ctx.object.size_ & 0x0F;
  int count = size + 6;

  if (ctx.tiles.size() < 5) return;

  for (int s = 0; s < count; s++) {
    int tile_x = ctx.object.x_ + s;
    int tile_y = ctx.object.y_ - s;

    DrawRoutineUtils::WriteTile8(ctx.target_bg, tile_x, tile_y + 0,
                                 ctx.tiles[0]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, tile_x, tile_y + 1,
                                 ctx.tiles[1]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, tile_x, tile_y + 2,
                                 ctx.tiles[2]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, tile_x, tile_y + 3,
                                 ctx.tiles[3]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, tile_x, tile_y + 4,
                                 ctx.tiles[4]);
  }
  // Note: BothBG should write to both BG1 and BG2 - handled by DrawObject
  // caller
}

void DrawDiagonalGrave_1to16_BothBG(const DrawContext& ctx) {
  // Pattern: Diagonal grave (\) for both BG layers (objects 0x16-0x17,
  // 0x1A-0x1B, 0x1E-0x1F) Based on bank_01.asm RoomDraw_DiagonalGrave_1to16_BothBG
  // at $018CB9 Assembly: LDA #$0006; JSR RoomDraw_GetSize_1to16_timesA
  // GetSize_1to16_timesA formula: ((B2 << 2) | B4) + A
  // Since size_ already contains ((B2 << 2) | B4), count = size + 6
  int size = ctx.object.size_ & 0x0F;
  int count = size + 6;

  if (ctx.tiles.size() < 5) return;

  for (int s = 0; s < count; s++) {
    int tile_x = ctx.object.x_ + s;
    int tile_y = ctx.object.y_ + s;

    DrawRoutineUtils::WriteTile8(ctx.target_bg, tile_x, tile_y + 0,
                                 ctx.tiles[0]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, tile_x, tile_y + 1,
                                 ctx.tiles[1]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, tile_x, tile_y + 2,
                                 ctx.tiles[2]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, tile_x, tile_y + 3,
                                 ctx.tiles[3]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, tile_x, tile_y + 4,
                                 ctx.tiles[4]);
  }
  // Note: BothBG should write to both BG1 and BG2 - handled by DrawObject
  // caller
}

void RegisterDiagonalRoutines(std::vector<DrawRoutineInfo>& registry) {
  // Note: Routine IDs are assigned based on the assembly routine table
  // These diagonal routines are part of the core 40 draw routines

  registry.push_back(DrawRoutineInfo{
      .id = 5,  // RoomDraw_DiagonalAcute_1to16
      .name = "DiagonalAcute_1to16",
      .function = DrawDiagonalAcute_1to16,
      .draws_to_both_bgs = false,
      .base_width = 0,  // Variable based on size parameter
      .base_height = 5,
      .category = DrawRoutineInfo::Category::Diagonal,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 6,  // RoomDraw_DiagonalGrave_1to16
      .name = "DiagonalGrave_1to16",
      .function = DrawDiagonalGrave_1to16,
      .draws_to_both_bgs = false,
      .base_width = 0,  // Variable based on size parameter
      .base_height = 5,
      .category = DrawRoutineInfo::Category::Diagonal,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 17,  // RoomDraw_DiagonalAcute_1to16_BothBG
      .name = "DiagonalAcute_1to16_BothBG",
      .function = DrawDiagonalAcute_1to16_BothBG,
      .draws_to_both_bgs = true,
      .base_width = 0,  // Variable based on size parameter
      .base_height = 5,
      .category = DrawRoutineInfo::Category::Diagonal,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 18,  // RoomDraw_DiagonalGrave_1to16_BothBG
      .name = "DiagonalGrave_1to16_BothBG",
      .function = DrawDiagonalGrave_1to16_BothBG,
      .draws_to_both_bgs = true,
      .base_width = 0,  // Variable based on size parameter
      .base_height = 5,
      .category = DrawRoutineInfo::Category::Diagonal,
  });
}

}  // namespace draw_routines
}  // namespace zelda3
}  // namespace yaze
