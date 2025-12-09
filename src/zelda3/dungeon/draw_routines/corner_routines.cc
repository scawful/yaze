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
  // Pattern: 4x4 Corner for Both BG (objects 0x108-0x10F for Type 2)
  // Type 3 objects (0xF9B-0xF9D) only have 8 tiles, draw 2x4 pattern
  if (ctx.tiles.size() >= 16) {
    DrawCorner4x4(ctx);
  } else if (ctx.tiles.size() >= 8) {
    // Draw 2x4 corner pattern (column-major)
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
  // Pattern: Weird Corner Bottom (objects 0x110-0x113 for Type 2)
  // Type 3 objects (0xF9E-0xFA1) use 8 tiles in 4x2 bottom corner layout
  if (ctx.tiles.size() >= 16) {
    DrawCorner4x4(ctx);
  } else if (ctx.tiles.size() >= 8) {
    // Draw 4x2 bottom corner pattern (row-major for bottom corners)
    int tid = 0;
    for (int yy = 0; yy < 2; yy++) {
      for (int xx = 0; xx < 4; xx++) {
        DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + xx,
                                     ctx.object.y_ + yy, ctx.tiles[tid++]);
      }
    }
  } else if (ctx.tiles.size() >= 4) {
    DrawWaterFace(ctx);
  }
}

void DrawWeirdCornerTop_BothBG(const DrawContext& ctx) {
  // Pattern: Weird Corner Top (objects 0x114-0x117 for Type 2)
  // Type 3 objects (0xFA2-0xFA5) use 8 tiles in 4x2 top corner layout
  if (ctx.tiles.size() >= 16) {
    DrawCorner4x4(ctx);
  } else if (ctx.tiles.size() >= 8) {
    // Draw 4x2 top corner pattern (row-major for top corners)
    int tid = 0;
    for (int yy = 0; yy < 2; yy++) {
      for (int xx = 0; xx < 4; xx++) {
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
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 4,
      .category = DrawRoutineInfo::Category::Corner,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 35,  // Draw4x4Corner_BothBG
      .name = "4x4Corner_BothBG",
      .function = Draw4x4Corner_BothBG,
      .draws_to_both_bgs = true,
      .base_width = 4,
      .base_height = 4,
      .category = DrawRoutineInfo::Category::Corner,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 36,  // DrawWeirdCornerBottom_BothBG
      .name = "WeirdCornerBottom_BothBG",
      .function = DrawWeirdCornerBottom_BothBG,
      .draws_to_both_bgs = true,
      .base_width = 4,
      .base_height = 2,
      .category = DrawRoutineInfo::Category::Corner,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 37,  // DrawWeirdCornerTop_BothBG
      .name = "WeirdCornerTop_BothBG",
      .function = DrawWeirdCornerTop_BothBG,
      .draws_to_both_bgs = true,
      .base_width = 4,
      .base_height = 2,
      .category = DrawRoutineInfo::Category::Corner,
  });
}

}  // namespace draw_routines
}  // namespace zelda3
}  // namespace yaze
