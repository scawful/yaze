#include "snes_tile.h"

#include <cstdint>
#include <vector>

#include "app/core/constants.h"

namespace yaze {
namespace app {
namespace gfx {

Bytes SnesTo8bppSheet(Bytes sheet, int bpp) {
  int xx = 0;  // positions where we are at on the sheet
  int yy = 0;
  int pos = 0;
  int ypos = 0;
  int num_tiles = 64;
  int buffer_size = 0x1000;
  if (bpp == 2) {
    bpp = 16;
    num_tiles = 128;
    buffer_size = 0x2000;
  } else if (bpp == 3) {
    bpp = 24;
  }
  Bytes sheet_buffer_out(buffer_size);

  for (int i = 0; i < num_tiles; i++) {  // for each tiles, 16 per line
    for (int y = 0; y < 8; y++) {        // for each line
      for (int x = 0; x < 8; x++) {      //[0] + [1] + [16]
        auto b1 = (sheet[(y * 2) + (bpp * pos)] & (kGraphicsBitmap[x]));
        auto b2 = (sheet[((y * 2) + (bpp * pos)) + 1] & (kGraphicsBitmap[x]));
        auto b3 = (sheet[(16 + y) + (bpp * pos)] & (kGraphicsBitmap[x]));
        unsigned char b = 0;
        if (b1 != 0) {
          b |= 1;
        }
        if (b2 != 0) {
          b |= 2;
        }
        if (b3 != 0 && bpp != 16) {
          b |= 4;
        }
        sheet_buffer_out[x + xx + (y * 128) + (yy * 1024)] = b;
      }
    }
    pos++;
    ypos++;
    xx += 8;
    if (ypos >= 16) {
      yy++;
      xx = 0;
      ypos = 0;
    }
  }
  return sheet_buffer_out;
}

Bytes BPP8SNESToIndexed(Bytes data, uint64_t bpp) {
  // 3BPP
  //[r0, bp1], [r0, bp2], [r1, bp1], [r1, bp2], [r2, bp1], [r2, bp2], [r3, bp1],
  //[r3, bp2] [r4, bp1], [r4, bp2], [r5, bp1], [r5, bp2], [r6, bp1], [r6, bp2],
  //[r7, bp1], [r7, bp2] [r0, bp3], [r0, bp4], [r1, bp3], [r1, bp4], [r2, bp3],
  //[r2, bp4], [r3, bp3], [r3, bp4] [r4, bp3], [r4, bp4], [r5, bp3], [r5, bp4],
  //[r6, bp3], [r6, bp4], [r7, bp3], [r7, bp4] [r0, bp5], [r0, bp6], [r1, bp5],
  //[r1, bp6], [r2, bp5], [r2, bp6], [r3, bp5], [r3, bp6] [r4, bp5], [r4, bp6],
  //[r5, bp5], [r5, bp6], [r6, bp5], [r6, bp6], [r7, bp5], [r7, bp6] [r0, bp7],
  //[r0, bp8], [r1, bp7], [r1, bp8], [r2, bp7], [r2, bp8], [r3, bp7], [r3, bp8]
  //[r4, bp7], [r4, bp8], [r5, bp7], [r5, bp8], [r6, bp7], [r6, bp8], [r7, bp7],
  //[r7, bp8]

  // 16 tiles = 1024 bytes
  auto buffer = Bytes(data.size());
  auto bitmap_data = Bytes(0x80 * 0x800);
  int yy = 0;
  int xx = 0;
  int pos = 0;

  const uint16_t sheet_width = 128;

  // 64 = 4096 bytes
  // 16 = 1024?
  int ypos = 0;
  // for each tiles //16 per lines
  for (int i = 0; i < 4096; i++) {
    // for each lines
    for (int y = 0; y < 8; y++) {
      //[0] + [1] + [16]
      for (int x = 0; x < 8; x++) {
        const uint16_t bitmask[] = {0x80, 0x40, 0x20, 0x10,
                                    0x08, 0x04, 0x02, 0x01};
        auto b1 = (data[(y * 2) + ((bpp * 8) * pos)] & (bitmask[x]));
        auto b2 = (data[((y * 2) + ((bpp * 8) * pos)) + 1] & (bitmask[x]));
        auto b3 = (data[(y * 2) + ((bpp * 8) * pos) + 16] & (bitmask[x]));
        auto b4 = (data[(y * 2) + ((bpp * 8) * pos) + 17] & (bitmask[x]));
        auto b5 = (data[(y * 2) + ((bpp * 8) * pos) + 32] & (bitmask[x]));
        auto b6 = (data[(y * 2) + ((bpp * 8) * pos) + 33] & (bitmask[x]));
        auto b7 = (data[(y * 2) + ((bpp * 8) * pos) + 48] & (bitmask[x]));
        auto b8 = (data[(y * 2) + ((bpp * 8) * pos) + 49] & (bitmask[x]));

        auto b = 0;
        if (b1 != 0) {
          b |= 1;
        }
        if (b2 != 0) {
          b |= 2;
        }
        if (bpp >= 4) {
          if (b3 != 0) {
            b |= 4;
          }
          if (b4 != 0) {
            b |= 8;
          }
        }
        if (bpp >= 8) {
          if (b5 != 0) {
            b |= 0x10;
          }
          if (b6 != 0) {
            b |= 0x20;
          }
          if (b7 != 0) {
            b |= 0x40;
          }
          if (b8 != 0) {
            b |= 0x80;
          }
        }
        bitmap_data[((x + xx) * sheet_width) + y + (yy * 8)] = b;
      }
    }
    pos++;
    ypos++;
    xx += 8;
    if (ypos >= 16) {
      yy++;
      xx = 0;
      ypos = 0;
    }
  }

  int n = 0;

  for (int y = 0; y < (data.size() / 64); y++) {
    for (int x = 0; x < sheet_width; x++) {  // 128 assumption
      if (n < data.size()) {
        buffer[n] = bitmap_data[(x * sheet_width) + y];
        n++;
      }
    }
  }
  return buffer;
}

ushort TileInfoToShort(TileInfo tile_info) {
  ushort result = 0;

  // Copy the id_ value
  result |= tile_info.id_ & 0x3FF;  // ids are 10 bits

  // Set the vertical_mirror_, horizontal_mirror_, and over_ flags
  result |= (tile_info.vertical_mirror_ ? 1 : 0) << 10;
  result |= (tile_info.horizontal_mirror_ ? 1 : 0) << 11;
  result |= (tile_info.over_ ? 1 : 0) << 12;

  // Set the palette_
  result |= (tile_info.palette_ & 0x07) << 13;  // palettes are 3 bits

  return result;
}

TileInfo GetTilesInfo(ushort tile) {
  // vhopppcc cccccccc
  bool o = false;
  bool v = false;
  bool h = false;
  auto tid = (ushort)(tile & core::TileNameMask);
  auto p = (uchar)((tile >> 10) & 0x07);

  o = ((tile & core::TilePriorityBit) == core::TilePriorityBit);
  h = ((tile & core::TileHFlipBit) == core::TileHFlipBit);
  v = ((tile & core::TileVFlipBit) == core::TileVFlipBit);

  return TileInfo(tid, p, v, h, o);
}

}  // namespace gfx
}  // namespace app
}  // namespace yaze