#include "draw_routine_types.h"

#include "app/gfx/types/snes_tile.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {
namespace DrawRoutineUtils {

void WriteTile8(gfx::BackgroundBuffer& bg, int tile_x, int tile_y,
                const gfx::TileInfo& tile_info) {
  if (!IsValidTilePosition(tile_x, tile_y)) {
    return;
  }
  // Convert TileInfo to the 16-bit word format and store in tile buffer
  uint16_t word = gfx::TileInfoToWord(tile_info);
  bg.SetTileAt(tile_x, tile_y, word);
}

void DrawBlock2x2(gfx::BackgroundBuffer& bg, int tile_x, int tile_y,
                  std::span<const gfx::TileInfo> tiles, int offset) {
  // Draw a 2x2 block (4 tiles) at the given position
  // Layout:  [0][1]
  //          [2][3]
  if (offset + 3 >= static_cast<int>(tiles.size())) return;

  WriteTile8(bg, tile_x, tile_y, tiles[offset]);
  WriteTile8(bg, tile_x + 1, tile_y, tiles[offset + 1]);
  WriteTile8(bg, tile_x, tile_y + 1, tiles[offset + 2]);
  WriteTile8(bg, tile_x + 1, tile_y + 1, tiles[offset + 3]);
}

void DrawBlock2x4(gfx::BackgroundBuffer& bg, int tile_x, int tile_y,
                  std::span<const gfx::TileInfo> tiles, int offset) {
  // Draw a 2x4 block (8 tiles) at the given position
  // Layout:  [0][1]
  //          [2][3]
  //          [4][5]
  //          [6][7]
  if (offset + 7 >= static_cast<int>(tiles.size())) return;

  for (int row = 0; row < 4; row++) {
    WriteTile8(bg, tile_x, tile_y + row, tiles[offset + row * 2]);
    WriteTile8(bg, tile_x + 1, tile_y + row, tiles[offset + row * 2 + 1]);
  }
}

void DrawBlock4x2(gfx::BackgroundBuffer& bg, int tile_x, int tile_y,
                  std::span<const gfx::TileInfo> tiles, int offset) {
  // Draw a 4x2 block (8 tiles) at the given position
  // Layout:  [0][1][2][3]
  //          [4][5][6][7]
  if (offset + 7 >= static_cast<int>(tiles.size())) return;

  for (int col = 0; col < 4; col++) {
    WriteTile8(bg, tile_x + col, tile_y, tiles[offset + col]);
    WriteTile8(bg, tile_x + col, tile_y + 1, tiles[offset + 4 + col]);
  }
}

void DrawBlock4x4(gfx::BackgroundBuffer& bg, int tile_x, int tile_y,
                  std::span<const gfx::TileInfo> tiles, int offset) {
  // Draw a 4x4 block (16 tiles) at the given position
  // Layout:  [0 ][1 ][2 ][3 ]
  //          [4 ][5 ][6 ][7 ]
  //          [8 ][9 ][10][11]
  //          [12][13][14][15]
  if (offset + 15 >= static_cast<int>(tiles.size())) return;

  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 4; col++) {
      WriteTile8(bg, tile_x + col, tile_y + row, tiles[offset + row * 4 + col]);
    }
  }
}

}  // namespace DrawRoutineUtils
}  // namespace zelda3
}  // namespace yaze
