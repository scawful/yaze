#include "diagonal_routines.h"

#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {
namespace draw_routines {

namespace {

// USDASM routines 5/6 load size+7, then enter the shared loop at the label
// that decrements once before the first draw. Routines 17/18 load size+6 and
// enter before the draw. Both paths therefore render exactly size+6 columns.
void DrawDiagonalColumns(const DrawContext& ctx, int y_step) {
  if (ctx.tiles.size() < 5) {
    return;
  }

  const int count = (ctx.object.size_ & 0x0F) + 6;
  for (int step = 0; step < count; ++step) {
    const int tile_x = ctx.object.x_ + step;
    const int tile_y = ctx.object.y_ + step * y_step;
    for (int row = 0; row < 5; ++row) {
      DrawRoutineUtils::WriteTile8(ctx.target_bg, tile_x, tile_y + row,
                                   ctx.tiles[row]);
    }
  }
}

}  // namespace

void DrawDiagonalAcute_1to16(const DrawContext& ctx) {
  DrawDiagonalColumns(ctx, /*y_step=*/-1);
}

void DrawDiagonalGrave_1to16(const DrawContext& ctx) {
  DrawDiagonalColumns(ctx, /*y_step=*/1);
}

void DrawDiagonalAcute_1to16_BothBG(const DrawContext& ctx) {
  DrawDiagonalColumns(ctx, /*y_step=*/-1);
}

void DrawDiagonalGrave_1to16_BothBG(const DrawContext& ctx) {
  DrawDiagonalColumns(ctx, /*y_step=*/1);
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
      .min_tiles = 5,  // 2x2 + 1 diagonal step
      .category = DrawRoutineInfo::Category::Diagonal,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 6,  // RoomDraw_DiagonalGrave_1to16
      .name = "DiagonalGrave_1to16",
      .function = DrawDiagonalGrave_1to16,
      .draws_to_both_bgs = false,
      .base_width = 0,  // Variable based on size parameter
      .base_height = 5,
      .min_tiles = 5,  // 2x2 + 1 diagonal step
      .category = DrawRoutineInfo::Category::Diagonal,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 17,  // RoomDraw_DiagonalAcute_1to16_BothBG
      .name = "DiagonalAcute_1to16_BothBG",
      .function = DrawDiagonalAcute_1to16_BothBG,
      .draws_to_both_bgs = true,
      .base_width = 0,  // Variable based on size parameter
      .base_height = 5,
      .min_tiles = 5,  // 2x2 + 1 diagonal step
      .category = DrawRoutineInfo::Category::Diagonal,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 18,  // RoomDraw_DiagonalGrave_1to16_BothBG
      .name = "DiagonalGrave_1to16_BothBG",
      .function = DrawDiagonalGrave_1to16_BothBG,
      .draws_to_both_bgs = true,
      .base_width = 0,  // Variable based on size parameter
      .base_height = 5,
      .min_tiles = 5,  // 2x2 + 1 diagonal step
      .category = DrawRoutineInfo::Category::Diagonal,
  });
}

}  // namespace draw_routines
}  // namespace zelda3
}  // namespace yaze
