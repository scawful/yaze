#include "corner_routines.h"

#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/draw_routines/special_routines.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {
namespace draw_routines {

namespace {

void DrawSanctuaryWall(const DrawContext& ctx) {
  // RoomDraw_SanctuaryWall ($019B56): twenty facade columns write directly
  // to $7E2000. The final 4x3 center uses the active tilemap pointers instead.
  if (ctx.tiles.size() < 24) {
    return;
  }
  auto& facade_bg = ctx.object.layer_ == RoomObject::LayerType::BG2 &&
                            ctx.secondary_bg != nullptr
                        ? *ctx.secondary_bg
                        : ctx.target_bg;

  for (int row = 0; row < 6; ++row) {
    const auto& first = ctx.tiles[row];
    auto first_mirrored = first;
    first_mirrored.horizontal_mirror_ = true;  // OR $4000, not XOR.
    for (int column : {0, 4, 8, 14, 18, 22}) {
      DrawRoutineUtils::WriteTile8(facade_bg, ctx.object.x_ + column,
                                   ctx.object.y_ + row, first);
    }
    for (int column : {1, 5, 9, 15, 19, 23}) {
      DrawRoutineUtils::WriteTile8(facade_bg, ctx.object.x_ + column,
                                   ctx.object.y_ + row, first_mirrored);
    }
    const auto& second = ctx.tiles[6 + row];
    auto second_mirrored = second;
    second_mirrored.horizontal_mirror_ = true;
    for (int column : {2, 6, 16, 20}) {
      DrawRoutineUtils::WriteTile8(facade_bg, ctx.object.x_ + column,
                                   ctx.object.y_ + row, second);
    }
    for (int column : {3, 7, 17, 21}) {
      DrawRoutineUtils::WriteTile8(facade_bg, ctx.object.x_ + column,
                                   ctx.object.y_ + row, second_mirrored);
    }
  }

  // $019BC6 advances past both facade columns, then $019BD6 tail-calls
  // RoomDraw_1x3N_rightwards with A=4 at the original anchor plus ten tiles.
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 3; ++row) {
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 10 + column,
                                   ctx.object.y_ + row,
                                   ctx.tiles[12 + column * 3 + row]);
    }
  }
}

enum class DiagonalCeilingAnchor {
  kTopLeft,
  kBottomLeft,
  kTopRight,
  kBottomRight,
};

void DrawDiagonalCeiling(const DrawContext& ctx, DiagonalCeilingAnchor anchor) {
  // USDASM parity:
  // RoomDraw_DiagonalCeiling* uses GetSize_1to16_timesA with A=4.
  // Effective side length is (size_nibble & 0x0F) + 4.
  if (ctx.tiles.empty()) {
    return;
  }

  const int side = (ctx.object.size_ & 0x0F) + 4;
  const gfx::TileInfo& fill_tile = ctx.tiles[0];

  // Each USDASM variant changes the repeated span and/or row direction. The
  // names describe the triangle, not a common corner-origin mirror transform.
  for (int row = 0; row < side; ++row) {
    int first_column = 0;
    int last_column = side - row - 1;
    int y = ctx.object.y_ + row;

    switch (anchor) {
      case DiagonalCeilingAnchor::kTopLeft:
        break;
      case DiagonalCeilingAnchor::kBottomLeft:
        last_column = row;
        break;
      case DiagonalCeilingAnchor::kTopRight:
        first_column = row;
        last_column = side - 1;
        break;
      case DiagonalCeilingAnchor::kBottomRight:
        first_column = row;
        last_column = side - 1;
        y = ctx.object.y_ - row;
        break;
    }

    for (int column = first_column; column <= last_column; ++column) {
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + column, y,
                                   fill_tile);
    }
  }
}

}  // namespace

// Note: DrawWaterFace is defined in special_routines.cc along with other
// water face variants (Empty, Spitting, Drenching)

void DrawCorner4x4(const DrawContext& ctx) {
  // Canonical 4x4 corners consume 16 words. Shorter payloads below are editor
  // compatibility fallbacks, not alternate vanilla subtype-2 formats.

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
    // Short-payload fallback: 2x4 column-major.
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
  // fallback shapes for abbreviated editor payloads.
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

void DrawDiagonalCeilingTopLeft(const DrawContext& ctx) {
  DrawDiagonalCeiling(ctx, DiagonalCeilingAnchor::kTopLeft);
}

void DrawDiagonalCeilingBottomLeft(const DrawContext& ctx) {
  DrawDiagonalCeiling(ctx, DiagonalCeilingAnchor::kBottomLeft);
}

void DrawDiagonalCeilingTopRight(const DrawContext& ctx) {
  DrawDiagonalCeiling(ctx, DiagonalCeilingAnchor::kTopRight);
}

void DrawDiagonalCeilingBottomRight(const DrawContext& ctx) {
  DrawDiagonalCeiling(ctx, DiagonalCeilingAnchor::kBottomRight);
}

void RegisterCornerRoutines(std::vector<DrawRoutineInfo>& registry) {
  // Note: Routine IDs are assigned based on the assembly routine table
  // These corner routines are part of the core 40 draw routines

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kSanctuaryWall,
      .name = "SanctuaryWall",
      .function = DrawSanctuaryWall,
      .draws_to_both_bgs = false,  // Mixed routing, not duplicated geometry.
      .base_width = 24,
      .base_height = 6,
      .min_tiles = 24,
      .category = DrawRoutineInfo::Category::Special,
  });

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

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kDiagonalCeilingTopLeft,
      .name = "DiagonalCeilingTopLeft",
      .function = DrawDiagonalCeilingTopLeft,
      .draws_to_both_bgs = false,
      .base_width = 4,   // size_nibble + 4
      .base_height = 4,  // size_nibble + 4
      .min_tiles = 1,
      .category = DrawRoutineInfo::Category::Corner,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kDiagonalCeilingBottomLeft,
      .name = "DiagonalCeilingBottomLeft",
      .function = DrawDiagonalCeilingBottomLeft,
      .draws_to_both_bgs = false,
      .base_width = 4,   // size_nibble + 4
      .base_height = 4,  // size_nibble + 4
      .min_tiles = 1,
      .category = DrawRoutineInfo::Category::Corner,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kDiagonalCeilingTopRight,
      .name = "DiagonalCeilingTopRight",
      .function = DrawDiagonalCeilingTopRight,
      .draws_to_both_bgs = false,
      .base_width = 4,   // size_nibble + 4
      .base_height = 4,  // size_nibble + 4
      .min_tiles = 1,
      .category = DrawRoutineInfo::Category::Corner,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kDiagonalCeilingBottomRight,
      .name = "DiagonalCeilingBottomRight",
      .function = DrawDiagonalCeilingBottomRight,
      .draws_to_both_bgs = false,
      .base_width = 4,   // size_nibble + 4
      .base_height = 4,  // size_nibble + 4
      .min_tiles = 1,
      .category = DrawRoutineInfo::Category::Corner,
  });
}

}  // namespace draw_routines
}  // namespace zelda3
}  // namespace yaze
