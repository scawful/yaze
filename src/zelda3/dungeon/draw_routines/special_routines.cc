#include "special_routines.h"

#include <algorithm>
#include <array>

#include "core/features.h"
#include "util/log.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/dungeon_state.h"
#include "zelda3/dungeon/moving_wall_semantics.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {
namespace draw_routines {

namespace {

constexpr int kThievesTownEastAtticRoomId = 0x65;

const gfx::TileInfo& TileAtWrapped(std::span<const gfx::TileInfo> tiles,
                                   size_t index) {
  return tiles[index % tiles.size()];
}

void DrawColumnMajor(gfx::BackgroundBuffer& bg, int base_x, int base_y, int w,
                     int h, std::span<const gfx::TileInfo> tiles,
                     size_t start_index = 0) {
  if (tiles.empty())
    return;

  size_t index = start_index;
  for (int x = 0; x < w; ++x) {
    for (int y = 0; y < h; ++y) {
      DrawRoutineUtils::WriteTile8(bg, base_x + x, base_y + y,
                                   TileAtWrapped(tiles, index++));
    }
  }
}

void DrawRowMajor(gfx::BackgroundBuffer& bg, int base_x, int base_y, int w,
                  int h, std::span<const gfx::TileInfo> tiles,
                  size_t start_index = 0) {
  if (tiles.empty())
    return;

  size_t index = start_index;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      DrawRoutineUtils::WriteTile8(bg, base_x + x, base_y + y,
                                   TileAtWrapped(tiles, index++));
    }
  }
}

void Draw1x3NRightwards(const DrawContext& ctx, int columns, size_t start_index,
                        int y_offset = 0) {
  DrawColumnMajor(ctx.target_bg, ctx.object.x_, ctx.object.y_ + y_offset,
                  columns, 3, ctx.tiles, start_index);
}

void DrawNx4(const DrawContext& ctx, int columns, size_t start_index) {
  DrawColumnMajor(ctx.target_bg, ctx.object.x_, ctx.object.y_, columns, 4,
                  ctx.tiles, start_index);
}

void Draw1x5Column(const DrawContext& ctx, int x_offset, size_t start_index) {
  DrawColumnMajor(ctx.target_bg, ctx.object.x_ + x_offset, ctx.object.y_, 1, 5,
                  ctx.tiles, start_index);
}

void Draw4x4ColumnMajor(const DrawContext& ctx, int x_offset, int y_offset,
                        size_t start_index) {
  DrawColumnMajor(ctx.target_bg, ctx.object.x_ + x_offset,
                  ctx.object.y_ + y_offset, 4, 4, ctx.tiles, start_index);
}

void DrawFloorLightGrid(const DrawContext& ctx) {
  if (ctx.tiles.size() < 64)
    return;

  Draw4x4ColumnMajor(ctx, /*x_offset=*/0, /*y_offset=*/0, /*start_index=*/0);
  Draw4x4ColumnMajor(ctx, /*x_offset=*/4, /*y_offset=*/0, /*start_index=*/16);
  Draw4x4ColumnMajor(ctx, /*x_offset=*/0, /*y_offset=*/4, /*start_index=*/32);
  Draw4x4ColumnMajor(ctx, /*x_offset=*/4, /*y_offset=*/4, /*start_index=*/48);
}

void Draw4x4ColumnMajorTo(gfx::BackgroundBuffer& bg, const RoomObject& object,
                          std::span<const gfx::TileInfo> tiles) {
  DrawColumnMajor(bg, object.x_, object.y_, 4, 4, tiles);
}

void DrawMany32x32Block(gfx::BackgroundBuffer& bg, int base_x, int base_y,
                        std::span<const gfx::TileInfo> tiles) {
  // USDASM RoomDraw_A_Many32x32Blocks ($01:8A44) writes tiles 0..3 through
  // row-0 tilemap pointers and tiles 4..7 through row-1 pointers, then repeats
  // the same 4x2 stamp two tile rows lower.
  DrawRowMajor(bg, base_x, base_y, /*w=*/4, /*h=*/2, tiles);
  DrawRowMajor(bg, base_x, base_y + 2, /*w=*/4, /*h=*/2, tiles);
}

bool IsAutoStairsDualLayer(int16_t object_id) {
  return object_id == 0x0130 || object_id == 0x0131 || object_id == 0x0F9B ||
         object_id == 0x0F9C;
}

bool IsStraightInterroomUpper(int16_t object_id) {
  return object_id >= 0x0F9E && object_id <= 0x0FA1;
}

bool IsStraightInterroomLower(int16_t object_id) {
  return object_id >= 0x0FA6 && object_id <= 0x0FA9;
}

bool IsSouthLowerStraightInterroomStairs(int16_t object_id) {
  return object_id == 0x0FA8 || object_id == 0x0FA9;
}

void DrawWaterHopStairsA(const DrawContext& ctx) {
  // ASM: RoomDraw_WaterHopStairs_A ($019B1E) falls through PortraitOfMario's
  // single-pass Downwards4x2VariableSpacing path, which is a fixed 4x2
  // ROW-MAJOR stamp on the active layer.
  if (ctx.tiles.size() < 8)
    return;

  DrawRowMajor(ctx.target_bg, ctx.object.x_, ctx.object.y_, 4, 2, ctx.tiles);
}

void DrawWaterHopStairsB(const DrawContext& ctx) {
  // ASM: RoomDraw_WaterHopStairs_B ($01A3AE) writes the same 4x2 ROW-MAJOR
  // footprint to both BG tilemaps. The outer draw framework handles the dual
  // pass for routines marked draws_to_both_bgs.
  DrawWaterHopStairsA(ctx);
}

void DrawDamFloodGate(const DrawContext& ctx) {
  // ASM: RoomDraw_DamFloodGate ($019BF8) stamps a 10x4 column-major tile block
  // from the closed tile span by default, and swaps to the alternate water-open
  // tile span when the overworld event is active.
  constexpr size_t kSpanTiles = 40;
  const bool use_open_tiles = ctx.state != nullptr &&
                              ctx.state->IsDamFloodgateOpen(ctx.room_id) &&
                              ctx.tiles.size() >= kSpanTiles * 2;
  const size_t start_index = use_open_tiles ? kSpanTiles : 0;
  if (ctx.tiles.size() < start_index + kSpanTiles)
    return;

  DrawColumnMajor(ctx.target_bg, ctx.object.x_, ctx.object.y_, 10, 4, ctx.tiles,
                  start_index);
}

void WritePlatformTile(const DrawContext& ctx, int dx, int dy, size_t index) {
  if (index >= ctx.tiles.size())
    return;

  DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + dx,
                               ctx.object.y_ + dy, ctx.tiles[index]);
}

void DrawPlatform1x3Rightwards(const DrawContext& ctx, int dx, int dy,
                               size_t start_index, int columns) {
  for (int x = 0; x < columns; ++x) {
    for (int y = 0; y < 3; ++y) {
      WritePlatformTile(ctx, dx + x, dy + y, start_index + x * 3 + y);
    }
  }
}

void DrawPlatform2x2(const DrawContext& ctx, int dx, int dy,
                     size_t start_index) {
  WritePlatformTile(ctx, dx + 0, dy + 0, start_index + 0);
  WritePlatformTile(ctx, dx + 0, dy + 1, start_index + 1);
  WritePlatformTile(ctx, dx + 1, dy + 0, start_index + 2);
  WritePlatformTile(ctx, dx + 1, dy + 1, start_index + 3);
}

void DrawPlatform3x2(const DrawContext& ctx, int dx, int dy,
                     size_t start_index) {
  WritePlatformTile(ctx, dx + 0, dy + 0, start_index + 0);
  WritePlatformTile(ctx, dx + 1, dy + 0, start_index + 1);
  WritePlatformTile(ctx, dx + 2, dy + 0, start_index + 2);
  WritePlatformTile(ctx, dx + 0, dy + 1, start_index + 3);
  WritePlatformTile(ctx, dx + 1, dy + 1, start_index + 4);
  WritePlatformTile(ctx, dx + 2, dy + 1, start_index + 5);
}

constexpr int kMovingWallFillDataOffset = kRoomObjectTileAddress + 0x03D8;
constexpr std::array<uint16_t, 3> kMovingWallFallbackFillWords = {
    0x3C15, 0x3C15, 0x3C15};

std::array<gfx::TileInfo, 3> LoadMovingWallFillTiles(const DrawContext& ctx) {
  std::array<gfx::TileInfo, 3> fill_tiles;
  for (size_t i = 0; i < fill_tiles.size(); ++i) {
    uint16_t word = kMovingWallFallbackFillWords[i];
    if (ctx.rom != nullptr) {
      const auto word_or = ctx.rom->ReadWord(kMovingWallFillDataOffset +
                                             static_cast<int>(i * 2));
      if (word_or.ok()) {
        word = *word_or;
      }
    }
    fill_tiles[i] = gfx::WordToTileInfo(word);
  }
  return fill_tiles;
}

void DrawMovingWallFill(const DrawContext& ctx, int start_x, int column_count,
                        int height) {
  const auto fill_tiles = LoadMovingWallFillTiles(ctx);
  for (int x = 0; x < column_count; ++x) {
    DrawRoutineUtils::WriteTile8(ctx.target_bg, start_x + x, ctx.object.y_,
                                 fill_tiles[0]);
    for (int y = 1; y < height - 1; ++y) {
      DrawRoutineUtils::WriteTile8(ctx.target_bg, start_x + x,
                                   ctx.object.y_ + y, fill_tiles[1]);
    }
    DrawRoutineUtils::WriteTile8(ctx.target_bg, start_x + x,
                                 ctx.object.y_ + height - 1, fill_tiles[2]);
  }
}

void DrawMovingWallPlatform(const DrawContext& ctx, int direction) {
  DrawPlatform1x3Rightwards(ctx, /*dx=*/0, /*dy=*/0, /*start_index=*/0,
                            /*columns=*/3);
  for (int i = 0; i < direction; ++i) {
    DrawPlatform3x2(ctx, /*dx=*/0, /*dy=*/3 + i * 2,
                    /*start_index=*/9);
  }
  // The ASM advances six words past the vertical pattern before stamping the
  // second 3x3 corner (payload slots 15..23).
  DrawPlatform1x3Rightwards(ctx, /*dx=*/0, /*dy=*/3 + direction * 2,
                            /*start_index=*/15, /*columns=*/3);
}

void DrawOpenChestPlatformSegment(const DrawContext& ctx, int dy,
                                  size_t start_index, int fill_width) {
  WritePlatformTile(ctx, /*dx=*/0, dy, start_index + 0);

  for (int i = 0; i < fill_width; ++i) {
    WritePlatformTile(ctx, /*dx=*/1 + i, dy, start_index + 3);
  }

  const int left_cap_x = 1 + fill_width;
  WritePlatformTile(ctx, left_cap_x, dy, start_index + 6);

  for (int i = 0; i < 4; ++i) {
    WritePlatformTile(ctx, left_cap_x + 1 + i, dy, start_index + 9);
  }

  const int right_cap_x = left_cap_x + 5;
  WritePlatformTile(ctx, right_cap_x, dy, start_index + 12);

  for (int i = 0; i < fill_width; ++i) {
    WritePlatformTile(ctx, right_cap_x + 1 + i, dy, start_index + 15);
  }

  WritePlatformTile(ctx, right_cap_x + 1 + fill_width, dy, start_index + 18);
}

}  // namespace

void DrawChest(const DrawContext& ctx, int chest_index) {
  // Routine 39 is used only by stateful Chest (F99) and fixed OpenChest
  // (F9A). BigChest uses its separate 4x3 routine.
  bool is_open = ctx.object.id_ == 0xF9A;
  if (!is_open && ctx.state) {
    is_open = ctx.state->IsChestOpen(ctx.room_id, chest_index);
  }

  // Preserve the legacy subtype-1 0xF8-0xFD family handled by this registry
  // routine. The subtype-3 chest opcodes below are always 2x2.
  if (ctx.object.id_ < 0xF80 && ctx.tiles.size() >= 16) {
    size_t start_index = is_open && ctx.tiles.size() >= 32 ? 16 : 0;
    for (int x = 0; x < 4; ++x) {
      for (int y = 0; y < 4; ++y) {
        DrawRoutineUtils::WriteTile8(
            ctx.target_bg, ctx.object.x_ + x, ctx.object.y_ + y,
            ctx.tiles[start_index + static_cast<size_t>(x * 4 + y)]);
      }
    }
    return;
  }

  size_t start_index = 0;
  if (is_open && ctx.object.id_ == 0xF99 && ctx.tiles.size() >= 8) {
    start_index = 4;
  }
  if (ctx.tiles.size() < start_index + 4) {
    return;
  }

  DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, ctx.object.y_,
                               ctx.tiles[start_index]);
  DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, ctx.object.y_ + 1,
                               ctx.tiles[start_index + 1]);
  DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1, ctx.object.y_,
                               ctx.tiles[start_index + 2]);
  DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1,
                               ctx.object.y_ + 1, ctx.tiles[start_index + 3]);
}

void DrawNothing([[maybe_unused]] const DrawContext& ctx) {
  // Intentionally empty - represents invisible logic objects or placeholders
  // ASM: RoomDraw_Nothing_A ($0190F2), RoomDraw_Nothing_B ($01932E), etc.
  // These routines typically just RTS.
}

void CustomDraw(const DrawContext& ctx) {
  // Pattern: Custom draw routine (objects 0x31-0x32)
  // When custom objects are enabled, load tile data from external binary files
  // managed by CustomObjectManager. Each binary encodes SNES tilemap entries
  // with relative x/y positions computed from the buffer stride layout.

  if (!core::FeatureFlags::get().kEnableCustomObjects) {
    // Feature disabled: fall back to vanilla 1x1 draw from ROM tile span
    if (ctx.tiles.size() >= 1) {
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, ctx.object.y_,
                                   ctx.tiles[0]);
    }
    return;
  }

  // Look up the custom object by ID and subtype.
  // ctx.object.id_ is 0x31 or 0x32; ctx.object.size_ encodes the subtype.
  auto result = CustomObjectManager::Get().GetObjectInternal(ctx.object.id_,
                                                             ctx.object.size_);

  if (!result.ok() || !result.value() || result.value()->IsEmpty()) {
    // Custom object not found or empty: fall back to 1x1 draw
    if (ctx.tiles.size() >= 1) {
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, ctx.object.y_,
                                   ctx.tiles[0]);
    }
    return;
  }

  const auto& custom_obj = *result.value();

  for (const auto& entry : custom_obj.tiles) {
    // Convert SNES tilemap word (vhopppcc cccccccc) to TileInfo.
    // Low byte = entry.tile_data & 0xFF, high byte = (entry.tile_data >> 8).
    uint8_t lo = static_cast<uint8_t>(entry.tile_data & 0xFF);
    uint8_t hi = static_cast<uint8_t>((entry.tile_data >> 8) & 0xFF);
    gfx::TileInfo tile_info(lo, hi);

    // rel_x/rel_y are already decoded as object-relative coordinates from the
    // binary stream's buffer position arithmetic; preserve those offsets.
    int draw_x = ctx.object.x_ + entry.rel_x;
    int draw_y = ctx.object.y_ + entry.rel_y;

    DrawRoutineUtils::WriteTile8(ctx.target_bg, draw_x, draw_y, tile_info);
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
  // USDASM RoomDraw_SomariaLine writes one object-data word at the object's
  // anchor. The apparent path is assembled from separate subtype-3 objects;
  // the object's size bits do not extend an individual piece.
  if (ctx.tiles.empty()) {
    return;
  }

  DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, ctx.object.y_,
                               ctx.tiles.front());
}

void DrawWaterFace(const DrawContext& ctx) {
  // Legacy 2x2 helper used by older corner-family routines. The real subtype-3
  // water-face objects are handled by DrawEmptyWaterFace /
  // DrawSpittingWaterFace / DrawDrenchingWaterFace below.
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

void DrawBed4x5(const DrawContext& ctx) {
  // ASM: RoomDraw_Bed4x5 ($019AEE) writes 4 columns per row, 5 rows total.
  DrawRowMajor(ctx.target_bg, ctx.object.x_, ctx.object.y_, 4, 5, ctx.tiles);
}

void DrawRightwards3x6(const DrawContext& ctx) {
  // ASM: RoomDraw_DrawRightwards3x6 ($019B50) is RoomDraw_1x3N_rightwards with
  // A=6 -> 6 columns x 3 rows, column-major.
  Draw1x3NRightwards(ctx, /*columns=*/6, /*start_index=*/0);
}

void DrawUtility6x3(const DrawContext& ctx) {
  // ASM: RoomDraw_Utility6x3 ($019A0C) is also 1x3N_rightwards with A=6.
  Draw1x3NRightwards(ctx, /*columns=*/6, /*start_index=*/0);
}

void DrawUtility3x5(const DrawContext& ctx) {
  // ASM: RoomDraw_Utility3x5 ($01A194) uses:
  // - top row: tiles 0..2
  // - middle 3 rows: tiles 3..5 repeated per row
  // - bottom row: tiles 6..8
  if (ctx.tiles.empty())
    return;

  // Top row.
  for (int x = 0; x < 3; ++x) {
    DrawRoutineUtils::WriteTile8(
        ctx.target_bg, ctx.object.x_ + x, ctx.object.y_,
        TileAtWrapped(ctx.tiles, static_cast<size_t>(x)));
  }

  // Middle rows (rows 1..3) reuse tiles 3..5.
  for (int y = 1; y <= 3; ++y) {
    for (int x = 0; x < 3; ++x) {
      DrawRoutineUtils::WriteTile8(
          ctx.target_bg, ctx.object.x_ + x, ctx.object.y_ + y,
          TileAtWrapped(ctx.tiles, static_cast<size_t>(3 + x)));
    }
  }

  // Bottom row.
  for (int x = 0; x < 3; ++x) {
    DrawRoutineUtils::WriteTile8(
        ctx.target_bg, ctx.object.x_ + x, ctx.object.y_ + 4,
        TileAtWrapped(ctx.tiles, static_cast<size_t>(6 + x)));
  }
}

void DrawVerticalTurtleRockPipe(const DrawContext& ctx) {
  // ASM: RoomDraw_VerticalTurtleRockPipe ($019A90)
  // Two stacked 4x3 sections: first uses tiles 0..11, second 12..23.
  if (ctx.tiles.empty())
    return;
  Draw1x3NRightwards(ctx, /*columns=*/4, /*start_index=*/0, /*y_offset=*/0);
  Draw1x3NRightwards(ctx, /*columns=*/4, /*start_index=*/12, /*y_offset=*/3);
}

void DrawHorizontalTurtleRockPipe(const DrawContext& ctx) {
  // ASM: RoomDraw_HorizontalTurtleRockPipe ($019AA3) -> RoomDraw_Nx4 with A=6.
  DrawNx4(ctx, /*columns=*/6, /*start_index=*/0);
}

void DrawLightBeamOnFloor(const DrawContext& ctx) {
  // ASM: RoomDraw_LightBeamOnFloor ($01A7B6)
  // Draw three 4x4 blocks at y offsets 0, +2, +6. The middle block resets X
  // to obj2376 and reuses tiles 0..15; the bottom block uses obj2396 at
  // tiles 16..31.
  if (ctx.tiles.size() < 32)
    return;
  Draw4x4ColumnMajor(ctx, /*x_offset=*/0, /*y_offset=*/0, /*start_index=*/0);
  Draw4x4ColumnMajor(ctx, /*x_offset=*/0, /*y_offset=*/2, /*start_index=*/0);
  Draw4x4ColumnMajor(ctx, /*x_offset=*/0, /*y_offset=*/6, /*start_index=*/16);
}

void DrawBigLightBeamOnFloor(const DrawContext& ctx) {
  // ASM: RoomDraw_BigLightBeamOnFloor ($01A7D3) reads the persisted
  // bombed-floor flag for fixed room 0x065 ($7EF0CA & $0100), then falls
  // through to RoomDraw_FloorLight when it is active. Do not use ctx.room_id.
  // Null state is reserved for object/geometry previews, which keep the beam
  // visible and measurable.
  if (ctx.state != nullptr &&
      !ctx.state->IsFloorBombable(kThievesTownEastAtticRoomId)) {
    return;
  }
  DrawFloorLightGrid(ctx);
}

void DrawFloorLight(const DrawContext& ctx) {
  // ASM: RoomDraw_FloorLight ($01A7DC) is unconditional for object 0x274.
  DrawFloorLightGrid(ctx);
}

void DrawBossShell4x4(const DrawContext& ctx) {
  // ASM-mapped objects route through RoomDraw_4x4.
  Draw4x4ColumnMajor(ctx, /*x_offset=*/0, /*y_offset=*/0, /*start_index=*/0);
}

void DrawSolidWallDecor3x4(const DrawContext& ctx) {
  // ASM: RoomDraw_SolidWallDecor3x4 ($0199EC) -> RoomDraw_Nx4 with A=3.
  DrawNx4(ctx, /*columns=*/3, /*start_index=*/0);
}

void DrawArcheryGameTargetDoor(const DrawContext& ctx) {
  // ASM: RoomDraw_ArcheryGameTargetDoor ($01A7A3)
  // Two 3x3 sections stacked vertically.
  if (ctx.tiles.empty())
    return;
  Draw1x3NRightwards(ctx, /*columns=*/3, /*start_index=*/0, /*y_offset=*/0);
  Draw1x3NRightwards(ctx, /*columns=*/3, /*start_index=*/9, /*y_offset=*/3);
}

void DrawGanonTriforceFloorDecor(const DrawContext& ctx) {
  // ASM: RoomDraw_GanonTriforceFloorDecor ($01A7F0)
  // Top block uses 0..15 at +2 X, then two bottom 4x4 blocks use 16..31.
  if (ctx.tiles.empty())
    return;

  Draw4x4ColumnMajor(ctx, /*x_offset=*/2, /*y_offset=*/0, /*start_index=*/0);
  Draw4x4ColumnMajor(ctx, /*x_offset=*/0, /*y_offset=*/4, /*start_index=*/16);
  Draw4x4ColumnMajor(ctx, /*x_offset=*/4, /*y_offset=*/4, /*start_index=*/16);
}

void DrawSingle2x2(const DrawContext& ctx) {
  // ASM: RoomDraw_Single2x2 ($019A8D) -> RoomDraw_Downwards2x2
  // Single 2x2 in column-major order.
  if (ctx.tiles.size() < 4)
    return;

  DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, ctx.object.y_,
                               ctx.tiles[0]);
  DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, ctx.object.y_ + 1,
                               ctx.tiles[1]);
  DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1, ctx.object.y_,
                               ctx.tiles[2]);
  DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + 1,
                               ctx.object.y_ + 1, ctx.tiles[3]);
}

void DrawSingle4x4(const DrawContext& ctx) {
  // ASM: RoomDraw_4x4 ($0197ED), column-major 4x4 block (16 tiles).
  if (ctx.tiles.size() < 16)
    return;

  int tid = 0;
  for (int x = 0; x < 4; ++x) {
    for (int y = 0; y < 4; ++y) {
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + x,
                                   ctx.object.y_ + y, ctx.tiles[tid++]);
    }
  }
}

void DrawSingle4x3(const DrawContext& ctx) {
  // ASM: RoomDraw_TableRock4x3 ($0199E6), column-major 4x3 block (12 tiles).
  if (ctx.tiles.size() < 12)
    return;

  int tid = 0;
  for (int x = 0; x < 4; ++x) {
    for (int y = 0; y < 3; ++y) {
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + x,
                                   ctx.object.y_ + y, ctx.tiles[tid++]);
    }
  }
}

void DrawRupeeFloor(const DrawContext& ctx) {
  // USDASM RoomDraw_RupeeFloor ($01:9AA9-$01:9AED) exits when the
  // current-room $0402 & $1000 event is set. State-free selector/geometry
  // previews remain visible.
  if ((ctx.state != nullptr && ctx.state->IsRupeeFloorCleared(ctx.room_id)) ||
      ctx.tiles.size() < 2) {
    return;
  }

  constexpr std::array<int, 3> kTopRows = {0, 3, 6};
  constexpr std::array<int, 3> kBottomRows = {1, 4, 7};
  for (int col = 0; col < 3; ++col) {
    const int x = ctx.object.x_ + col * 2;
    for (int row : kTopRows) {
      DrawRoutineUtils::WriteTile8(ctx.target_bg, x, ctx.object.y_ + row,
                                   ctx.tiles[0]);
    }
    for (int row : kBottomRows) {
      DrawRoutineUtils::WriteTile8(ctx.target_bg, x, ctx.object.y_ + row,
                                   ctx.tiles[1]);
    }
  }
}

void DrawActual4x4(const DrawContext& ctx) {
  // ASM: RoomDraw_4x4 ($0197ED), used for true 4x4 tile8 objects.
  DrawSingle4x4(ctx);
}

void DrawWaterfall47(const DrawContext& ctx) {
  // ASM: RoomDraw_Waterfall47 ($019466)
  // First 1x5 column from 0..4, middle columns from 5..9, final from 10..14.
  if (ctx.tiles.empty())
    return;

  const int size = ctx.object.size_ & 0x0F;
  const int middle_columns = (size + 1) * 2;  // ASL $B2

  Draw1x5Column(ctx, /*x_offset=*/0, /*start_index=*/0);
  for (int i = 0; i < middle_columns; ++i) {
    Draw1x5Column(ctx, /*x_offset=*/1 + i, /*start_index=*/5);
  }
  Draw1x5Column(ctx, /*x_offset=*/1 + middle_columns, /*start_index=*/10);
}

void DrawWaterfall48(const DrawContext& ctx) {
  // ASM: RoomDraw_Waterfall48 ($019488)
  // First 1x3 column from 0..2, middle columns from 3..5, final from 6..8.
  if (ctx.tiles.empty())
    return;

  const int size = ctx.object.size_ & 0x0F;
  const int middle_columns = (size + 1) * 2;  // ASL $B2

  DrawColumnMajor(ctx.target_bg, ctx.object.x_, ctx.object.y_, /*w=*/1, /*h=*/3,
                  ctx.tiles, /*start_index=*/0);
  for (int i = 0; i < middle_columns; ++i) {
    DrawColumnMajor(ctx.target_bg, ctx.object.x_ + 1 + i, ctx.object.y_,
                    /*w=*/1, /*h=*/3, ctx.tiles, /*start_index=*/3);
  }
  DrawColumnMajor(ctx.target_bg, ctx.object.x_ + 1 + middle_columns,
                  ctx.object.y_, /*w=*/1, /*h=*/3, ctx.tiles,
                  /*start_index=*/6);
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
  int size_x = ((ctx.object.size_ >> 2) & 0x03) + 1;
  int size_y = (ctx.object.size_ & 0x03) + 1;

  LOG_DEBUG("DrawRoutines",
            "Draw4x4BlocksIn4x4SuperSquare: obj=0x%03X pos=(%d,%d) size=0x%02X "
            "tiles=%zu size_x=%d size_y=%d",
            ctx.object.id_, ctx.object.x_, ctx.object.y_, ctx.object.size_,
            ctx.tiles.size(), size_x, size_y);

  if (ctx.tiles.empty()) {
    LOG_DEBUG("DrawRoutines",
              "Draw4x4BlocksIn4x4SuperSquare: SKIPPING - no tiles loaded!");
    return;
  }

  // Use first tile for all blocks
  const auto& tile = ctx.tiles[0];
  LOG_DEBUG("DrawRoutines",
            "Draw4x4BlocksIn4x4SuperSquare: tile[0] id=%d palette=%d", tile.id_,
            tile.palette_);

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
  int size_x = ((ctx.object.size_ >> 2) & 0x03) + 1;
  int size_y = (ctx.object.size_ & 0x03) + 1;

  if (ctx.tiles.empty())
    return;

  const auto& tile = ctx.tiles[0];
  for (int sy = 0; sy < size_y; ++sy) {
    for (int sx = 0; sx < size_x; ++sx) {
      int base_x = ctx.object.x_ + (sx * 3);
      int base_y = ctx.object.y_ + (sy * 3);

      for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
          DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x, base_y + y,
                                       tile);
        }
      }
    }
  }
}

void Draw4x4FloorIn4x4SuperSquare(const DrawContext& ctx) {
  // ASM: RoomDraw_4x4FloorIn4x4SuperSquare ($018FA5)
  // Most common floor pattern - draws 4x4 floor tiles in super square units.
  // Uses RoomDraw_A_Many32x32Blocks internally in assembly.
  int size_x = ((ctx.object.size_ >> 2) & 0x03) + 1;
  int size_y = (ctx.object.size_ & 0x03) + 1;

  if (ctx.tiles.empty())
    return;
  if (ctx.tiles.size() < 8) {
    // Some hacks provide abbreviated tile payloads for these objects.
    // Fall back to a visible fill instead of silently skipping draw.
    Draw4x4BlocksIn4x4SuperSquare(ctx);
    return;
  }

  for (int sy = 0; sy < size_y; ++sy) {
    for (int sx = 0; sx < size_x; ++sx) {
      int base_x = ctx.object.x_ + (sx * 4);
      int base_y = ctx.object.y_ + (sy * 4);

      DrawMany32x32Block(ctx.target_bg, base_x, base_y, ctx.tiles);
    }
  }
}

void Draw4x4FloorOneIn4x4SuperSquare(const DrawContext& ctx) {
  // ASM: RoomDraw_4x4FloorOneIn4x4SuperSquare ($018FA2)
  // Single 4x4 floor pattern (starts at different tile offset in assembly).
  // For our purposes, same as 4x4FloorIn4x4SuperSquare with offset tiles.
  int size_x = ((ctx.object.size_ >> 2) & 0x03) + 1;
  int size_y = (ctx.object.size_ & 0x03) + 1;

  if (ctx.tiles.size() < 8) {
    Draw4x4FloorIn4x4SuperSquare(ctx);
    return;
  }

  for (int sy = 0; sy < size_y; ++sy) {
    for (int sx = 0; sx < size_x; ++sx) {
      int base_x = ctx.object.x_ + (sx * 4);
      int base_y = ctx.object.y_ + (sy * 4);

      DrawMany32x32Block(ctx.target_bg, base_x, base_y, ctx.tiles);
    }
  }
}

void Draw4x4FloorTwoIn4x4SuperSquare(const DrawContext& ctx) {
  // ASM: RoomDraw_4x4FloorTwoIn4x4SuperSquare ($018F9D)
  // Two 4x4 floor patterns (uses $0490 offset in assembly).
  int size_x = ((ctx.object.size_ >> 2) & 0x03) + 1;
  int size_y = (ctx.object.size_ & 0x03) + 1;

  if (ctx.tiles.size() < 8) {
    Draw4x4FloorIn4x4SuperSquare(ctx);
    return;
  }

  for (int sy = 0; sy < size_y; ++sy) {
    for (int sx = 0; sx < size_x; ++sx) {
      int base_x = ctx.object.x_ + (sx * 4);
      int base_y = ctx.object.y_ + (sy * 4);

      DrawMany32x32Block(ctx.target_bg, base_x, base_y, ctx.tiles);
    }
  }
}

void DrawBigHole4x4_1to16(const DrawContext& ctx) {
  // ASM: Object 0xA4 - Big hole pattern
  // Draws a rectangular hole with border tiles using Size as the expansion.
  int size = ctx.object.size_ & 0x0F;

  if (ctx.tiles.size() < 24)
    return;

  int base_x = ctx.object.x_;
  int base_y = ctx.object.y_;
  int max = size + 3;  // Bottom/right edge offset

  // Corners
  DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x, base_y, ctx.tiles[8]);
  DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + max, base_y,
                               ctx.tiles[14]);
  DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x, base_y + max,
                               ctx.tiles[17]);
  DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + max, base_y + max,
                               ctx.tiles[23]);

  // Edges and interior
  for (int xx = 1; xx < max; ++xx) {
    for (int yy = 1; yy < max; ++yy) {
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + xx, base_y + yy,
                                   ctx.tiles[0]);
    }
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + xx, base_y,
                                 ctx.tiles[10]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + xx, base_y + max,
                                 ctx.tiles[19]);
  }

  for (int yy = 1; yy < max; ++yy) {
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x, base_y + yy,
                                 ctx.tiles[9]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + max, base_y + yy,
                                 ctx.tiles[15]);
  }
}

void DrawSpike2x2In4x4SuperSquare(const DrawContext& ctx) {
  // ASM: RoomDraw_Spike2x2In4x4SuperSquare ($019708)
  // Draws 2x2 spike patterns in a tiled grid
  int size_x = ((ctx.object.size_ >> 2) & 0x03) + 1;
  int size_y = (ctx.object.size_ & 0x03) + 1;

  if (ctx.tiles.size() < 4)
    return;

  for (int sy = 0; sy < size_y; ++sy) {
    for (int sx = 0; sx < size_x; ++sx) {
      int base_x = ctx.object.x_ + (sx * 2);
      int base_y = ctx.object.y_ + (sy * 2);

      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x, base_y, ctx.tiles[0]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, base_y,
                                   ctx.tiles[2]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x, base_y + 1,
                                   ctx.tiles[1]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, base_y + 1,
                                   ctx.tiles[3]);
    }
  }
}

void DrawTableRock4x4_1to16(const DrawContext& ctx) {
  // ASM: Object 0xDD - Table rock pattern
  int size_x = ((ctx.object.size_ >> 2) & 0x03);
  int size_y = (ctx.object.size_ & 0x03);

  if (ctx.tiles.size() < 16)
    return;

  int right_x = ctx.object.x_ + (3 + (size_x * 2));
  int bottom_y = ctx.object.y_ + (3 + (size_y * 2));

  // Interior
  for (int xx = 0; xx < size_x + 1; ++xx) {
    for (int yy = 0; yy < size_y + 1; ++yy) {
      int base_x = ctx.object.x_ + (xx * 2);
      int base_y = ctx.object.y_ + (yy * 2);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, base_y + 1,
                                   ctx.tiles[5]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 2, base_y + 1,
                                   ctx.tiles[6]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, base_y + 2,
                                   ctx.tiles[9]);
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 2, base_y + 2,
                                   ctx.tiles[10]);
    }
  }

  // Left/right borders
  for (int yy = 0; yy < size_y + 1; ++yy) {
    int base_y = ctx.object.y_ + (yy * 2);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, base_y + 1,
                                 ctx.tiles[4]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, base_y + 2,
                                 ctx.tiles[8]);

    DrawRoutineUtils::WriteTile8(ctx.target_bg, right_x, base_y + 1,
                                 ctx.tiles[7]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, right_x, base_y + 2,
                                 ctx.tiles[11]);
  }

  // Top/bottom borders
  for (int xx = 0; xx < size_x + 1; ++xx) {
    int base_x = ctx.object.x_ + (xx * 2);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, ctx.object.y_,
                                 ctx.tiles[1]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 2, ctx.object.y_,
                                 ctx.tiles[2]);

    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 1, bottom_y,
                                 ctx.tiles[13]);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 2, bottom_y,
                                 ctx.tiles[14]);
  }

  // Corners
  DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, ctx.object.y_,
                               ctx.tiles[0]);
  DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, bottom_y,
                               ctx.tiles[12]);
  DrawRoutineUtils::WriteTile8(ctx.target_bg, right_x, ctx.object.y_,
                               ctx.tiles[3]);
  DrawRoutineUtils::WriteTile8(ctx.target_bg, right_x, bottom_y, ctx.tiles[15]);
}

void DrawWaterOverlay8x8_1to16(const DrawContext& ctx) {
  // ASM: RoomDraw_WaterOverlayA8x8_1to16 ($0195D6) / RoomDraw_WaterOverlayB8x8
  // NOTE: In the original game, this is an HDMA control object that sets up
  // the wavy water distortion effect. It doesn't draw tiles directly.
  // For the editor, we draw the available tile data as a visual indicator.

  int size_x = ((ctx.object.size_ >> 2) & 0x03);
  int size_y = (ctx.object.size_ & 0x03);

  int count_x = size_x + 2;
  int count_y = size_y + 2;

  if (ctx.tiles.empty())
    return;
  if (ctx.tiles.size() < 8) {
    // Fallback for abbreviated tile payloads: still stamp a visible overlay.
    for (int yy = 0; yy < count_y; ++yy) {
      for (int xx = 0; xx < count_x; ++xx) {
        int base_x = ctx.object.x_ + (xx * 4);
        int base_y = ctx.object.y_ + (yy * 4);
        const auto& tile =
            ctx.tiles[static_cast<size_t>((xx + yy) % ctx.tiles.size())];
        for (int y = 0; y < 4; ++y) {
          for (int x = 0; x < 4; ++x) {
            DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x, base_y + y,
                                         tile);
          }
        }
      }
    }
    return;
  }

  for (int yy = 0; yy < count_y; ++yy) {
    for (int xx = 0; xx < count_x; ++xx) {
      int base_x = ctx.object.x_ + (xx * 4);
      int base_y = ctx.object.y_ + (yy * 4);

      for (int x = 0; x < 4; ++x) {
        DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x, base_y,
                                     ctx.tiles[x]);
        DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x, base_y + 2,
                                     ctx.tiles[x]);
        DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x, base_y + 1,
                                     ctx.tiles[4 + x]);
        DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x, base_y + 3,
                                     ctx.tiles[4 + x]);
      }
    }
  }
}

// ============================================================================
// Stair Routines
// ============================================================================

void DrawInterRoomFatStairsUp(const DrawContext& ctx) {
  // ASM: RoomDraw_InterRoomFatStairsUp ($01A41B)
  // Uses RoomDraw_4x4, which consumes a 4x4 tile block in COLUMN-MAJOR order.
  // In original game, registers position in $06B0 for transition handling
  // For editor display, we just draw the visual representation

  if (ctx.tiles.size() < 16)
    return;

  Draw4x4ColumnMajorTo(ctx.target_bg, ctx.object, ctx.tiles);
}

void DrawInterRoomFatStairsDownA(const DrawContext& ctx) {
  // ASM: RoomDraw_InterRoomFatStairsDownA ($01A458)
  // Uses tile data at obj10A8
  DrawInterRoomFatStairsUp(ctx);  // Same visual structure
}

void DrawInterRoomFatStairsDownB(const DrawContext& ctx) {
  // ASM: RoomDraw_InterRoomFatStairsDownB ($01A486)
  // Uses tile data at obj10A8
  DrawInterRoomFatStairsUp(ctx);  // Same visual structure
}

void DrawSpiralStairs(const DrawContext& ctx, bool going_up, bool is_upper) {
  // ASM: RoomDraw_SpiralStairsGoingUpUpper, etc.
  // Calls RoomDraw_1x3N_rightwards with A=4 -> 4 columns x 3 rows = 12 tiles
  // Tile order is COLUMN-MAJOR (down first, then right).
  // Upper variants render to BG1, lower variants render to BG2.
  (void)going_up;

  if (ctx.tiles.size() < 12)
    return;

  gfx::BackgroundBuffer* dest = &ctx.target_bg;
  if (!is_upper && ctx.secondary_bg != nullptr) {
    dest = ctx.secondary_bg;
  }

  DrawColumnMajor(*dest, ctx.object.x_, ctx.object.y_, 4, 3, ctx.tiles);
}

void DrawAutoStairs(const DrawContext& ctx) {
  // ASM: RoomDraw_AutoStairs* routines
  // The multi-layer variants stamp both BG tilemaps; merged/single-layer
  // variants stay on the active target layer.
  if (ctx.tiles.size() < 16)
    return;

  Draw4x4ColumnMajorTo(ctx.target_bg, ctx.object, ctx.tiles);

  if (ctx.secondary_bg != nullptr && IsAutoStairsDualLayer(ctx.object.id_)) {
    Draw4x4ColumnMajorTo(*ctx.secondary_bg, ctx.object, ctx.tiles);
  }
}

void DrawStraightInterRoomStairs(const DrawContext& ctx) {
  // ASM: RoomDraw_StraightInterroomStairs* routines
  // Upper variants render on BG1 only. Lower variants render the 4x4 block on
  // BG2 and expose the front edge on BG1 as well.
  if (ctx.tiles.size() < 16)
    return;

  if (IsStraightInterroomUpper(ctx.object.id_) || ctx.secondary_bg == nullptr ||
      !IsStraightInterroomLower(ctx.object.id_)) {
    Draw4x4ColumnMajorTo(ctx.target_bg, ctx.object, ctx.tiles);
    return;
  }

  const bool south_lower = IsSouthLowerStraightInterroomStairs(ctx.object.id_);
  size_t tile_index = 0;
  for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      const auto& tile = TileAtWrapped(ctx.tiles, tile_index++);
      DrawRoutineUtils::WriteTile8(*ctx.secondary_bg, ctx.object.x_ + col,
                                   ctx.object.y_ + row, tile);
      if ((!south_lower && row == 0) || (south_lower && row == 3)) {
        DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + col,
                                     ctx.object.y_ + row, tile);
      }
    }
  }
}

// ============================================================================
// Interactive Object Routines
// ============================================================================

void DrawPrisonCell(const DrawContext& ctx) {
  // ASM: RoomDraw_PrisonCell ($019C44)
  // Draws prison cell bars to BOTH BG layers with horizontal flip for symmetry
  // The ASM writes to $7E2xxx (BG1) and also uses ORA #$4000 for horizontal flip
  // Pattern: 5 iterations drawing a complex bar pattern

  if (ctx.tiles.size() < 6)
    return;

  // Prison cell layout based on ASM analysis:
  // The routine draws 5 columns of bars, each with specific tile patterns
  // Tiles at positions: (x, y), (x+7, y) for outer bars
  // Middle bars with horizontal flip on one side

  int base_x = ctx.object.x_;
  int base_y = ctx.object.y_;

  // Draw the prison cell pattern - 5 vertical bar segments
  for (int col = 0; col < 5; ++col) {
    int x_offset = col;

    // Each column has 4 rows of tiles
    for (int row = 0; row < 4; ++row) {
      size_t tile_idx = (row < static_cast<int>(ctx.tiles.size())) ? row : 0;

      // Left side bar
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + x_offset,
                                   base_y + row, ctx.tiles[tile_idx]);

      // Right side bar (mirrored horizontally)
      auto mirrored_tile = ctx.tiles[tile_idx];
      mirrored_tile.horizontal_mirror_ = !mirrored_tile.horizontal_mirror_;
      DrawRoutineUtils::WriteTile8(ctx.target_bg, base_x + 9 - x_offset,
                                   base_y + row, mirrored_tile);
    }
  }

  // If we have a secondary BG buffer, draw the same pattern there
  // This ensures the prison bars appear on both background layers
  if (ctx.HasSecondaryBG()) {
    for (int col = 0; col < 5; ++col) {
      int x_offset = col;

      for (int row = 0; row < 4; ++row) {
        size_t tile_idx = (row < static_cast<int>(ctx.tiles.size())) ? row : 0;

        // Left side bar
        DrawRoutineUtils::WriteTile8(*ctx.secondary_bg, base_x + x_offset,
                                     base_y + row, ctx.tiles[tile_idx]);

        // Right side bar (mirrored)
        auto mirrored_tile = ctx.tiles[tile_idx];
        mirrored_tile.horizontal_mirror_ = !mirrored_tile.horizontal_mirror_;
        DrawRoutineUtils::WriteTile8(*ctx.secondary_bg, base_x + 9 - x_offset,
                                     base_y + row, mirrored_tile);
      }
    }
  }
}

void DrawBigKeyLock(const DrawContext& ctx) {
  // ASM: RoomDraw_BigKeyLock ($0198AE)
  // The opened-state room-event check and shared chest/lock slot advancement
  // live in ObjectDrawer, which owns stream ordering. The ROM routine's closed
  // branch jumps to RoomDraw_Rightwards2x2 and consumes obj1494 in column-major
  // order; there is no second tile state.
  DrawSingle2x2(ctx);
}

void DrawBombableFloor(const DrawContext& ctx) {
  // USDASM RoomDraw_BombableFloor ($01:B3E1) draws four 2x2 quadrants.
  // ParseSubtype3 stores the intact obj0220 block at tiles[0..15] and the
  // non-contiguous obj05BA replacement block at tiles[16..31]. Each quadrant
  // is ordered upper-left, lower-left, upper-right, lower-right.
  if (ctx.tiles.size() < 16) {
    return;
  }

  // Vanilla persists this state for room 0x65, while ROM hacks can relocate
  // the floor (Oracle of Secrets uses 0xAD). Keep preview selection keyed to
  // the current room instead of hardcoding a vanilla room ID.
  const bool is_open = ctx.state != nullptr &&
                       ctx.state->IsFloorBombable(ctx.room_id) &&
                       ctx.tiles.size() >= 32;
  const size_t state_offset = is_open ? 16 : 0;

  for (int block_y = 0; block_y < 2; ++block_y) {
    for (int block_x = 0; block_x < 2; ++block_x) {
      const size_t block_offset =
          state_offset + static_cast<size_t>((block_y * 2 + block_x) * 4);
      for (int x = 0; x < 2; ++x) {
        for (int y = 0; y < 2; ++y) {
          DrawRoutineUtils::WriteTile8(
              ctx.target_bg, ctx.object.x_ + block_x * 2 + x,
              ctx.object.y_ + block_y * 2 + y,
              ctx.tiles[block_offset + static_cast<size_t>(x * 2 + y)]);
        }
      }
    }
  }
}

void DrawMovingWallWest(const DrawContext& ctx) {
  // USDASM RoomDraw_MovingWallWest ($01:9190): moved walls emit no tiles.
  if ((ctx.state != nullptr && ctx.state->IsWallMoved(ctx.room_id)) ||
      ctx.tiles.size() < 24) {
    return;
  }

  const int direction = moving_wall::DirectionForSize(ctx.object.size_);
  const int column_count = moving_wall::ObjectCountForSize(ctx.object.size_);
  const int height = direction * 2 + 6;

  // West writes the obj03D8 fill first, growing left from the object anchor,
  // then stamps the 3x3 corner and repeated 3x2 vertical wall at the anchor.
  DrawMovingWallFill(ctx, ctx.object.x_ - column_count, column_count, height);
  DrawMovingWallPlatform(ctx, direction);
}

void DrawMovingWallEast(const DrawContext& ctx) {
  // USDASM RoomDraw_MovingWallEast ($01:921C): moved walls emit no tiles.
  if ((ctx.state != nullptr && ctx.state->IsWallMoved(ctx.room_id)) ||
      ctx.tiles.size() < 24) {
    return;
  }

  const int direction = moving_wall::DirectionForSize(ctx.object.size_);
  const int column_count = moving_wall::ObjectCountForSize(ctx.object.size_);
  const int height = direction * 2 + 6;

  // East stamps the platform first, then grows the obj03D8 fill right from
  // the first column beyond the three-tile-wide platform.
  DrawMovingWallPlatform(ctx, direction);
  DrawMovingWallFill(ctx, ctx.object.x_ + 3, column_count, height);
}

// ============================================================================
// Water Face Variants
// ============================================================================

namespace {
constexpr int kWaterFaceWidthTiles = 4;

void DrawWaterFaceRows(const DrawContext& ctx, int row_count, int tile_offset) {
  if (row_count <= 0 || tile_offset < 0 || ctx.tiles.empty()) {
    return;
  }

  if (tile_offset >= static_cast<int>(ctx.tiles.size())) {
    return;
  }

  const int available_tiles = static_cast<int>(ctx.tiles.size()) - tile_offset;
  const int available_rows = available_tiles / kWaterFaceWidthTiles;
  const int rows_to_draw = std::min(row_count, available_rows);

  for (int row = 0; row < rows_to_draw; ++row) {
    const int row_base = tile_offset + (row * kWaterFaceWidthTiles);
    for (int col = 0; col < kWaterFaceWidthTiles; ++col) {
      DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + col,
                                   ctx.object.y_ + row,
                                   ctx.tiles[row_base + col]);
    }
  }
}
}  // namespace

void DrawEmptyWaterFace(const DrawContext& ctx) {
  // ASM: RoomDraw_EmptyWaterFace ($019D29)
  //
  // usdasm behavior:
  // - Base state draws a 4x3 face using data at offset 0x1614.
  // - "Water active" branch draws a 4x5 variant using data at offset 0x162C.
  //
  // IMPORTANT: this uses dedicated water-face state, not door state. Tying
  // this branch to IsDoorOpen created cross-feature rendering regressions.
  const bool water_active =
      (ctx.state != nullptr) && ctx.state->IsWaterFaceActive(ctx.room_id);

  const int row_count = water_active ? 5 : 3;
  const int tile_offset = water_active ? 12 : 0;  // 0x162C - 0x1614 = 24 bytes
  DrawWaterFaceRows(ctx, row_count, tile_offset);
}

void DrawSpittingWaterFace(const DrawContext& ctx) {
  // ASM: RoomDraw_SpittingWaterFace ($019D64)
  // Draws a 4x5 face/spout shape.
  DrawWaterFaceRows(ctx, /*row_count=*/5, /*tile_offset=*/0);
}

void DrawDrenchingWaterFace(const DrawContext& ctx) {
  // ASM: RoomDraw_DrenchingWaterFace ($019D83)
  // Draws a 4x7 continuous stream.
  DrawWaterFaceRows(ctx, /*row_count=*/7, /*tile_offset=*/0);
}

// ============================================================================
// Chest Platform Multi-Part Routines
// ============================================================================

void DrawClosedChestPlatform(const DrawContext& ctx) {
  // ASM: RoomDraw_ClosedChestPlatform ($018CC7)
  // Size fields are subtype-1 packed 2-bit values: $B2=size_x, $B4=size_y.
  // The routine expands them to width_blocks=size_x+4 and
  // height_blocks=size_y+1 before drawing top, center, bottom, and the center
  // 2x2 overlay.

  if (ctx.tiles.size() < 68)
    return;

  const int size_x = (ctx.object.size_ >> 2) & 0x03;
  const int size_y = ctx.object.size_ & 0x03;
  const int width_blocks = size_x + 4;
  const int height_blocks = size_y + 1;

  // Top 3 rows: left cap, repeated 2-column middle, right cap.
  DrawPlatform1x3Rightwards(ctx, /*dx=*/0, /*dy=*/0, /*start_index=*/0,
                            /*columns=*/3);
  for (int x = 0; x < width_blocks; ++x) {
    DrawPlatform1x3Rightwards(ctx, /*dx=*/3 + x * 2, /*dy=*/0,
                              /*start_index=*/9, /*columns=*/2);
  }
  DrawPlatform1x3Rightwards(ctx, /*dx=*/3 + width_blocks * 2, /*dy=*/0,
                            /*start_index=*/15, /*columns=*/3);

  // Center rows: left wall, repeated 2x2 carpet, right wall.
  for (int y = 0; y < height_blocks; ++y) {
    const int dy = 3 + y * 2;
    DrawPlatform3x2(ctx, /*dx=*/0, dy, /*start_index=*/24);
    for (int x = 0; x < width_blocks; ++x) {
      DrawPlatform2x2(ctx, /*dx=*/3 + x * 2, dy, /*start_index=*/30);
    }
    DrawPlatform3x2(ctx, /*dx=*/3 + width_blocks * 2, dy,
                    /*start_index=*/34);
  }

  // Bottom 3 rows.
  const int bottom_y = 3 + height_blocks * 2;
  DrawPlatform1x3Rightwards(ctx, /*dx=*/0, bottom_y, /*start_index=*/40,
                            /*columns=*/3);
  for (int x = 0; x < width_blocks; ++x) {
    DrawPlatform1x3Rightwards(ctx, /*dx=*/3 + x * 2, bottom_y,
                              /*start_index=*/49, /*columns=*/2);
  }
  DrawPlatform1x3Rightwards(ctx, /*dx=*/3 + width_blocks * 2, bottom_y,
                            /*start_index=*/55, /*columns=*/3);

  // Center 2x2 overlay, matching the final SrcPtr(0x590) draw in the native
  // C++ port and ZScream's tile indices 64,66/65,67.
  DrawPlatform2x2(ctx, /*dx=*/width_blocks + 2,
                  /*dy=*/bottom_y - (height_blocks + 1),
                  /*start_index=*/64);
}

void DrawOpenChestPlatform(const DrawContext& ctx) {
  // ASM: RoomDraw_OpenChestPlatform ($019733). The helper draws one row of a
  // 10+2*size_x tile platform, repeated (2*size_y+5) times, then two closing
  // rows from src+1/src+2.
  if (ctx.tiles.size() < 21)
    return;

  const int size_x = (ctx.object.size_ >> 2) & 0x03;
  const int size_y = ctx.object.size_ & 0x03;
  const int fill_width = size_x + 1;
  const int body_rows = size_y * 2 + 5;

  for (int row = 0; row < body_rows; ++row) {
    DrawOpenChestPlatformSegment(ctx, row, /*start_index=*/0, fill_width);
  }
  DrawOpenChestPlatformSegment(ctx, body_rows, /*start_index=*/1, fill_width);
  DrawOpenChestPlatformSegment(ctx, body_rows + 1, /*start_index=*/2,
                               fill_width);
}

void DrawChestPlatformHorizontalWall(const DrawContext& ctx) {
  // ASM: RoomDraw_ChestPlatformHorizontalWallWithCorners ($018D0D)
  int width = (ctx.object.size_ & 0x0F) + 1;

  if (ctx.tiles.size() < 3)
    return;

  for (int x = 0; x < width; ++x) {
    size_t tile_idx = (x == 0) ? 0 : ((x == width - 1) ? 2 : 1);
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_ + x,
                                 ctx.object.y_, ctx.tiles[tile_idx]);
  }
}

void DrawChestPlatformVerticalWall(const DrawContext& ctx) {
  // ASM: RoomDraw_ChestPlatformVerticalWall ($019E70)
  int height = (ctx.object.size_ & 0x0F) + 1;

  if (ctx.tiles.empty())
    return;

  for (int y = 0; y < height; ++y) {
    DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_,
                                 ctx.object.y_ + y, ctx.tiles[0]);
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
      .min_tiles = 4,  // 2x2 block
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
      .min_tiles = 1,  // at least one tile for door graphic
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 33,  // DrawSomariaLine
      .name = "SomariaLine",
      .function = DrawSomariaLine,
      .draws_to_both_bgs = false,
      .base_width = 1,
      .base_height = 1,
      .min_tiles = 1,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 34,  // DrawWaterFace
      .name = "WaterFace",
      .function = DrawWaterFace,
      .draws_to_both_bgs = false,
      .base_width = 2,
      .base_height = 2,
      .min_tiles = 4,  // 2x2 block
      .category = DrawRoutineInfo::Category::Special,
  });

  // ============================================================================
  // SuperSquare Routines (Phase 4) - IDs 56-64
  // ============================================================================

  registry.push_back(DrawRoutineInfo{
      .id = 56,  // Draw4x4BlocksIn4x4SuperSquare
      .name = "4x4BlocksIn4x4SuperSquare",
      .function = Draw4x4BlocksIn4x4SuperSquare,
      .draws_to_both_bgs = false,
      .base_width = 0,   // Variable: width = (((size >> 2) & 3) + 1) * 4
      .base_height = 0,  // Variable: height = ((size & 3) + 1) * 4
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 57,  // Draw3x3FloorIn4x4SuperSquare
      .name = "3x3FloorIn4x4SuperSquare",
      .function = Draw3x3FloorIn4x4SuperSquare,
      .draws_to_both_bgs = false,
      .base_width = 0,   // Variable
      .base_height = 0,  // Variable
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 58,  // Draw4x4FloorIn4x4SuperSquare
      .name = "4x4FloorIn4x4SuperSquare",
      .function = Draw4x4FloorIn4x4SuperSquare,
      .draws_to_both_bgs = false,
      .base_width = 0,   // Variable
      .base_height = 0,  // Variable
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 59,  // Draw4x4FloorOneIn4x4SuperSquare
      .name = "4x4FloorOneIn4x4SuperSquare",
      .function = Draw4x4FloorOneIn4x4SuperSquare,
      .draws_to_both_bgs = false,
      .base_width = 0,   // Variable
      .base_height = 0,  // Variable
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 60,  // Draw4x4FloorTwoIn4x4SuperSquare
      .name = "4x4FloorTwoIn4x4SuperSquare",
      .function = Draw4x4FloorTwoIn4x4SuperSquare,
      .draws_to_both_bgs = false,
      .base_width = 0,   // Variable
      .base_height = 0,  // Variable
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 61,  // DrawBigHole4x4_1to16
      .name = "BigHole4x4_1to16",
      .function = DrawBigHole4x4_1to16,
      .draws_to_both_bgs = false,
      .base_width = 0,   // Variable
      .base_height = 0,  // Variable
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 62,  // DrawSpike2x2In4x4SuperSquare
      .name = "Spike2x2In4x4SuperSquare",
      .function = DrawSpike2x2In4x4SuperSquare,
      .draws_to_both_bgs = false,
      .base_width = 0,   // Variable
      .base_height = 0,  // Variable
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 63,  // DrawTableRock4x4_1to16
      .name = "TableRock4x4_1to16",
      .function = DrawTableRock4x4_1to16,
      .draws_to_both_bgs = false,
      .base_width = 0,   // Variable
      .base_height = 0,  // Variable
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 64,  // DrawWaterOverlay8x8_1to16
      .name = "WaterOverlay8x8_1to16",
      .function = DrawWaterOverlay8x8_1to16,
      .draws_to_both_bgs = false,
      .base_width = 0,   // Variable
      .base_height = 0,  // Variable
      .category = DrawRoutineInfo::Category::Special,
  });

  // Stair routines (IDs 83-88)
  registry.push_back(DrawRoutineInfo{
      .id = 83,  // DrawInterRoomFatStairsUp
      .name = "InterRoomFatStairsUp",
      .function = DrawInterRoomFatStairsUp,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 4,
      .min_tiles = 16,  // 4x4 stair pattern
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 84,  // DrawInterRoomFatStairsDownA
      .name = "InterRoomFatStairsDownA",
      .function = DrawInterRoomFatStairsDownA,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 4,
      .min_tiles = 16,  // 4x4 stair pattern
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 85,  // DrawInterRoomFatStairsDownB
      .name = "InterRoomFatStairsDownB",
      .function = DrawInterRoomFatStairsDownB,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 4,
      .min_tiles = 16,  // 4x4 stair pattern
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 86,  // DrawAutoStairs
      .name = "AutoStairs",
      .function = DrawAutoStairs,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 4,
      .min_tiles = 16,  // 4x4 stair pattern
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 87,  // DrawStraightInterRoomStairs
      .name = "StraightInterRoomStairs",
      .function = DrawStraightInterRoomStairs,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 4,
      .min_tiles = 16,  // 4x4 stair pattern
      .category = DrawRoutineInfo::Category::Special,
  });

  // Spiral stairs variants (IDs 88-91)
  registry.push_back(DrawRoutineInfo{
      .id = 88,  // DrawSpiralStairsGoingUpUpper
      .name = "SpiralStairsGoingUpUpper",
      .function =
          [](const DrawContext& ctx) { DrawSpiralStairs(ctx, true, true); },
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 3,  // 4x3 pattern
      .min_tiles = 12,   // 4x3 block
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 89,  // DrawSpiralStairsGoingDownUpper
      .name = "SpiralStairsGoingDownUpper",
      .function =
          [](const DrawContext& ctx) { DrawSpiralStairs(ctx, false, true); },
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 3,  // 4x3 pattern
      .min_tiles = 12,   // 4x3 block
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 90,  // DrawSpiralStairsGoingUpLower
      .name = "SpiralStairsGoingUpLower",
      .function =
          [](const DrawContext& ctx) { DrawSpiralStairs(ctx, true, false); },
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 3,  // 4x3 pattern
      .min_tiles = 12,   // 4x3 block
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 91,  // DrawSpiralStairsGoingDownLower
      .name = "SpiralStairsGoingDownLower",
      .function =
          [](const DrawContext& ctx) { DrawSpiralStairs(ctx, false, false); },
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 3,  // 4x3 pattern
      .min_tiles = 12,   // 4x3 block
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kWaterHopStairsA,  // 119
      .name = "WaterHopStairsA",
      .function = DrawWaterHopStairsA,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 2,
      .min_tiles = 8,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kWaterHopStairsB,  // 120
      .name = "WaterHopStairsB",
      .function = DrawWaterHopStairsB,
      .draws_to_both_bgs = true,
      .base_width = 4,
      .base_height = 2,
      .min_tiles = 8,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kDamFloodGate,  // 121
      .name = "DamFloodGate",
      .function = DrawDamFloodGate,
      .draws_to_both_bgs = false,
      .base_width = 10,
      .base_height = 4,
      .min_tiles = 40,
      .category = DrawRoutineInfo::Category::Special,
  });

  // Interactive object routines (IDs 92-95)
  registry.push_back(DrawRoutineInfo{
      .id = 92,  // DrawBigKeyLock
      .name = "BigKeyLock",
      .function = DrawBigKeyLock,
      .draws_to_both_bgs = false,
      .base_width = 2,
      .base_height = 2,
      .min_tiles = 4,  // 2x2 block
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 93,  // DrawBombableFloor
      .name = "BombableFloor",
      .function = DrawBombableFloor,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 4,
      .min_tiles = 16,  // One complete 4x4 state
      .category = DrawRoutineInfo::Category::Special,
  });

  // Water face variants (IDs 94-96)
  registry.push_back(DrawRoutineInfo{
      .id = 94,  // DrawEmptyWaterFace
      .name = "EmptyWaterFace",
      .function = DrawEmptyWaterFace,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 3,
      .min_tiles = 12,  // base 4x3 variant
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 95,  // DrawSpittingWaterFace
      .name = "SpittingWaterFace",
      .function = DrawSpittingWaterFace,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 5,
      .min_tiles = 20,  // 4x5 variant
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = 96,  // DrawDrenchingWaterFace
      .name = "DrenchingWaterFace",
      .function = DrawDrenchingWaterFace,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 7,
      .min_tiles = 28,  // 4x7 variant
      .category = DrawRoutineInfo::Category::Special,
  });

  // Prison cell (Type 3 objects 0x20D, 0x217) draws to both BG layers.
  registry.push_back(DrawRoutineInfo{
      .id = 97,  // DrawPrisonCell
      .name = "PrisonCell",
      .function = DrawPrisonCell,
      .draws_to_both_bgs = true,
      .base_width = 10,  // Columns x..x+9
      .base_height = 4,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kBed4x5,  // 98
      .name = "Bed4x5",
      .function = DrawBed4x5,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 5,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRightwards3x6,  // 99
      .name = "Rightwards3x6",
      .function = DrawRightwards3x6,
      .draws_to_both_bgs = false,
      .base_width = 6,
      .base_height = 3,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kUtility6x3,  // 100
      .name = "Utility6x3",
      .function = DrawUtility6x3,
      .draws_to_both_bgs = false,
      .base_width = 6,
      .base_height = 3,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kUtility3x5,  // 101
      .name = "Utility3x5",
      .function = DrawUtility3x5,
      .draws_to_both_bgs = false,
      .base_width = 3,
      .base_height = 5,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kVerticalTurtleRockPipe,  // 102
      .name = "VerticalTurtleRockPipe",
      .function = DrawVerticalTurtleRockPipe,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 6,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kHorizontalTurtleRockPipe,  // 103
      .name = "HorizontalTurtleRockPipe",
      .function = DrawHorizontalTurtleRockPipe,
      .draws_to_both_bgs = false,
      .base_width = 6,
      .base_height = 4,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kLightBeam,  // 104
      .name = "LightBeam",
      .function = DrawLightBeamOnFloor,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 10,
      .min_tiles = 32,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kBigLightBeam,  // 105
      .name = "BigLightBeam",
      .function = DrawBigLightBeamOnFloor,
      .draws_to_both_bgs = false,
      .base_width = 8,
      .base_height = 8,
      .min_tiles = 64,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kFloorLight,  // 123
      .name = "FloorLight",
      .function = DrawFloorLight,
      .draws_to_both_bgs = false,
      .base_width = 8,
      .base_height = 8,
      .min_tiles = 64,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kBossShell4x4,  // 106
      .name = "BossShell4x4",
      .function = DrawBossShell4x4,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 4,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kSolidWallDecor3x4,  // 107
      .name = "SolidWallDecor3x4",
      .function = DrawSolidWallDecor3x4,
      .draws_to_both_bgs = false,
      .base_width = 3,
      .base_height = 4,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kArcheryGameTargetDoor,  // 108
      .name = "ArcheryGameTargetDoor",
      .function = DrawArcheryGameTargetDoor,
      .draws_to_both_bgs = false,
      .base_width = 3,
      .base_height = 6,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kGanonTriforceFloorDecor,  // 109
      .name = "GanonTriforceFloorDecor",
      .function = DrawGanonTriforceFloorDecor,
      .draws_to_both_bgs = false,
      .base_width = 8,
      .base_height = 8,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kSingle2x2,  // 110
      .name = "Single2x2",
      .function = DrawSingle2x2,
      .draws_to_both_bgs = false,
      .base_width = 2,
      .base_height = 2,
      .min_tiles = 4,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kSingle4x4,  // 113
      .name = "Single4x4",
      .function = DrawSingle4x4,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 4,
      .min_tiles = 16,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kSingle4x3,  // 114
      .name = "Single4x3",
      .function = DrawSingle4x3,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 3,
      .min_tiles = 12,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kRupeeFloor,  // 115
      .name = "RupeeFloor",
      .function = DrawRupeeFloor,
      .draws_to_both_bgs = false,
      .base_width = 5,
      .base_height = 8,
      .min_tiles = 2,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kActual4x4,  // 116
      .name = "Actual4x4",
      .function = DrawActual4x4,
      .draws_to_both_bgs = false,
      .base_width = 4,
      .base_height = 4,
      .min_tiles = 16,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kWaterfall47,  // 111
      .name = "Waterfall47",
      .function = DrawWaterfall47,
      .draws_to_both_bgs = false,
      .base_width = 0,  // Variable with size
      .base_height = 5,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kWaterfall48,  // 112
      .name = "Waterfall48",
      .function = DrawWaterfall48,
      .draws_to_both_bgs = false,
      .base_width = 0,  // Variable with size
      .base_height = 3,
      .category = DrawRoutineInfo::Category::Special,
  });

  // Chest platform routines - use canonical IDs from DrawRoutineIds
  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kClosedChestPlatform,  // 79
      .name = "ClosedChestPlatform",
      .function = DrawClosedChestPlatform,
      .draws_to_both_bgs = false,
      .base_width = 0,   // Variable: width = (size & 0x0F) + 4
      .base_height = 0,  // Variable: height = ((size >> 4) & 0x0F) + 1
      .category = DrawRoutineInfo::Category::Special,
  });

  // Moving wall routines
  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kMovingWallWest,  // 80
      .name = "MovingWallWest",
      .function = DrawMovingWallWest,
      .draws_to_both_bgs = false,
      .base_width = 0,   // Variable: count selector + 3 platform columns
      .base_height = 0,  // Variable: 2 * direction selector + 6
      .min_tiles = 24,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kMovingWallEast,  // 81
      .name = "MovingWallEast",
      .function = DrawMovingWallEast,
      .draws_to_both_bgs = false,
      .base_width = 0,   // Variable: count selector + 3 platform columns
      .base_height = 0,  // Variable: 2 * direction selector + 6
      .min_tiles = 24,
      .category = DrawRoutineInfo::Category::Special,
  });

  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kOpenChestPlatform,  // 82
      .name = "OpenChestPlatform",
      .function = DrawOpenChestPlatform,
      .draws_to_both_bgs = false,
      .base_width = 0,   // Variable
      .base_height = 0,  // Variable
      .category = DrawRoutineInfo::Category::Special,
  });

  // Long vertical rail with CORNER+MIDDLE+END pattern (ID 117) - object 0x8A
  // Matches horizontal rail 0x22 but in vertical orientation
  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kDownwardsHasEdge1x1_1to16_plus23,  // 117
      .name = "DownwardsHasEdge1x1_1to16_plus23",
      .function =
          [](const DrawContext& ctx) {
            // CORNER+MIDDLE+END pattern vertically
            int size = ctx.object.size_ & 0x0F;
            int count = size + 21;
            if (ctx.tiles.size() < 3)
              return;

            int tile_y = ctx.object.y_;
            // USDASM $01:8EC9-$01:8ED4 suppresses the leading corner when the
            // current slot already contains the small vertical rail corner.
            if (!DrawRoutineUtils::ExistingTileMatchesAny(
                    ctx.target_bg, ctx.object.x_, tile_y, {0x00E3})) {
              DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, tile_y,
                                           ctx.tiles[0]);
            }
            tile_y++;
            // Middle tiles
            for (int s = 0; s < count; s++) {
              DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, tile_y,
                                           ctx.tiles[1]);
              tile_y++;
            }
            // End tile
            DrawRoutineUtils::WriteTile8(ctx.target_bg, ctx.object.x_, tile_y,
                                         ctx.tiles[2]);
          },
      .draws_to_both_bgs = false,
      .base_width = 1,
      .base_height = 23,  // size + 23
      .min_tiles = 3,     // corner + middle + end tiles
      .category = DrawRoutineInfo::Category::Special,
  });

  // Custom Object routine (ID 130) - Oracle of Secrets objects 0x31, 0x32
  // These use external binary files instead of ROM tile data.
  // CustomDraw() handles feature-flag gating and binary file lookup.
  registry.push_back(DrawRoutineInfo{
      .id = DrawRoutineIds::kCustomObject,  // 130
      .name = "CustomObject",
      .function = CustomDraw,
      .draws_to_both_bgs = false,
      .base_width = 0,   // Variable: depends on binary file content
      .base_height = 0,  // Variable: depends on binary file content
      .category = DrawRoutineInfo::Category::Special,
  });
}

}  // namespace draw_routines
}  // namespace zelda3
}  // namespace yaze
