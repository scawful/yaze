#include "corner_routines.h"

#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/draw_routines/special_routines.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {
namespace draw_routines {

// Note: DrawWaterFace is defined in special_routines.cc along with other
// water face variants (Empty, Spitting, Drenching)

void DrawCorner4x4(const DrawContext& ctx) {
  // Pattern: 4x4 grid corner (Type 2 corners 0x40-0x4F, 0x108-0x10F)
  // Type 2 objects only have 8 tiles, so we need to handle both 16 and 8 tile
  // cases

  if (ctx.tiles.size() >= 16) {
    // Full 4x4 pattern - Column-major ordering per ZScream
    int tid = 0;
    for (int xx = 0; xx < 4; xx++) {
      for (int yy = 0; yy < 4; yy++) {
        DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + xx,
                                     ctx.object.y_ + yy, ctx.tiles[tid++]);
      }
    }
  } else if (ctx.tiles.size() >= 8) {
    // Type 2 objects: 8 tiles arranged in 2x4 column-major pattern
    // This is the standard Type 2 tile layout
    int tid = 0;
    for (int xx = 0; xx < 2; xx++) {
      for (int yy = 0; yy < 4; yy++) {
        DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + xx,
                                     ctx.object.y_ + yy, ctx.tiles[tid++]);
      }
    }
  } else if (ctx.tiles.size() >= 4) {
    // Fallback: 2x2 pattern
    int tid = 0;
    for (int xx = 0; xx < 2; xx++) {
      for (int yy = 0; yy < 2; yy++) {
        DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + xx,
                                     ctx.object.y_ + yy, ctx.tiles[tid++]);
      }
    }
  }
}

void Draw4x4Corner_BothBG(const DrawContext& ctx) {
  // USDASM: RoomDraw_4x4Corner_BothBG ($01:9813)
  // Canonical shape is 4 columns x 4 rows (column-major). We retain smaller
  // fallback shapes for abbreviated hack-ROM payloads.
  if (ctx.tiles.size() >= 16) {
    DrawCorner4x4(ctx);
  } else if (ctx.tiles.size() >= 8) {
    // Fallback: 2x4 corner pattern (column-major)
    int tid = 0;
    for (int xx = 0; xx < 2; xx++) {
      for (int yy = 0; yy < 4; yy++) {
        DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + xx,
                                     ctx.object.y_ + yy, ctx.tiles[tid++]);
      }
    }
  } else if (ctx.tiles.size() >= 4) {
    // Fallback: 2x2 pattern
    DrawWaterFace(ctx);
  }
}

void DrawWeirdCornerBottom_BothBG(const DrawContext& ctx) {
  // USDASM: RoomDraw_WeirdCornerBottom_BothBG ($01:9854)
  // ASM sets count=3 and reuses RoomDraw_4x4Corner_BothBG_set_count:
  // 3 columns x 4 rows, column-major (12 tiles).
  if (ctx.tiles.size() >= 12) {
    int tid = 0;
    for (int xx = 0; xx < 3; xx++) {
      for (int yy = 0; yy < 4; yy++) {
        DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + xx,
                                     ctx.object.y_ + yy, ctx.tiles[tid++]);
      }
    }
  } else if (ctx.tiles.size() >= 8) {
    // Fallback for truncated payloads: 2x4 column-major.
    int tid = 0;
    for (int xx = 0; xx < 2; xx++) {
      for (int yy = 0; yy < 4; yy++) {
        DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + xx,
                                     ctx.object.y_ + yy, ctx.tiles[tid++]);
      }
    }
  } else if (ctx.tiles.size() >= 4) {
    DrawWaterFace(ctx);
  }
}

void DrawWeirdCornerTop_BothBG(const DrawContext& ctx) {
  // USDASM: RoomDraw_WeirdCornerTop_BothBG ($01:985C)
  // ASM writes 3 rows per column and advances source by 6 bytes each loop:
  // 4 columns x 3 rows, column-major (12 tiles).
  if (ctx.tiles.size() >= 12) {
    int tid = 0;
    for (int xx = 0; xx < 4; xx++) {
      for (int yy = 0; yy < 3; yy++) {
        DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + xx,
                                     ctx.object.y_ + yy, ctx.tiles[tid++]);
      }
    }
  } else if (ctx.tiles.size() >= 8) {
    // Fallback for truncated payloads: 2x4 column-major.
    int tid = 0;
    for (int xx = 0; xx < 2; xx++) {
      for (int yy = 0; yy < 4; yy++) {
        DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + xx,
                                     ctx.object.y_ + yy, ctx.tiles[tid++]);
      }
    }
  } else if (ctx.tiles.size() >= 4) {
    DrawWaterFace(ctx);
  }
}

void RegisterCornerRoutines(std::vector<DrawRoutineInfo>& registry) {
  // Note: Routine IDs are assigned based on the assembly routine table
  // These corner routines are part of the core 40 draw routines

  registry.push_back(DrawRoutineInfo{
      .id = 19,  // DrawCorner4x4
      .name = "Corner4x4",
      .function = DrawCorner4x4,
      // Structural layout routine: writes to both BG1 and BG2 in the engine.
      .draws_to_both_bgs = true,
      .base_width = 4,
      .base_height = 4,
      .min_tiles = 4,  // handles 4, 8, or 16 tile variants
      .category = DrawRoutineInfo::Category::Corner,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 35,  // Draw4x4Corner_BothBG
      .name = "4x4Corner_BothBG",
      .function = Draw4x4Corner_BothBG,
      .draws_to_both_bgs = true,
      .base_width = 4,
      .base_height = 4,
      .min_tiles = 4,  // handles 4, 8, or 16 tile variants
      .category = DrawRoutineInfo::Category::Corner,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 36,  // DrawWeirdCornerBottom_BothBG
      .name = "WeirdCornerBottom_BothBG",
      .function = DrawWeirdCornerBottom_BothBG,
      .draws_to_both_bgs = true,
      .base_width = 3,
      .base_height = 4,
      .min_tiles = 12,  // Enforce canonical USDASM payload size.
      .category = DrawRoutineInfo::Category::Corner,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 37,  // DrawWeirdCornerTop_BothBG
      .name = "WeirdCornerTop_BothBG",
      .function = DrawWeirdCornerTop_BothBG,
      .draws_to_both_bgs = true,
      .base_width = 4,
      .base_height = 3,
      .min_tiles = 12,  // Enforce canonical USDASM payload size.
      .category = DrawRoutineInfo::Category::Corner,
  });
}

}  // namespace draw_routines
}  // namespace zelda3
}  // namespace yaze
