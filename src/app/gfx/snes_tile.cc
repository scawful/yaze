#include "snes_tile.h"

#include <cstdint>
#include <vector>

#include "app/core/constants.h"

namespace yaze {
namespace app {
namespace gfx {

tile8 UnpackBppTile(const Bytes& data, const uint32_t offset,
                    const uint32_t bpp) {
  tile8 tile;
  assert(bpp >= 1 && bpp <= 8);
  unsigned int bpp_pos[8];  // More for conveniance and readibility
  for (int col = 0; col < 8; col++) {
    for (int row = 0; row < 8; row++) {
      if (bpp == 1) {
        tile.data[col * 8 + row] = (data[offset + col] >> (7 - row)) & 0x01;
        continue;
      }
      /* SNES bpp format interlace each byte of the first 2 bitplanes.
       * | byte 1 of first bitplane | byte 1 of second bitplane |
       * | byte 2 of first bitplane | byte 2 of second bitplane | ..
       */
      bpp_pos[0] = offset + col * 2;
      bpp_pos[1] = offset + col * 2 + 1;
      char mask = 1 << (7 - row);
      tile.data[col * 8 + row] = (data[bpp_pos[0]] & mask) == mask;
      tile.data[col * 8 + row] |= (uchar)((data[bpp_pos[1]] & mask) == mask)
                                  << 1;
      if (bpp == 3) {
        // When we have 3 bitplanes, the bytes for the third bitplane are after
        // the 16 bytes of the 2 bitplanes.
        bpp_pos[2] = offset + 16 + col;
        tile.data[col * 8 + row] |= (uchar)((data[bpp_pos[2]] & mask) == mask)
                                    << 2;
      }
      if (bpp >= 4) {
        // For 4 bitplanes, the 2 added bitplanes are interlaced like the first
        // two.
        bpp_pos[2] = offset + 16 + col * 2;
        bpp_pos[3] = offset + 16 + col * 2 + 1;
        tile.data[col * 8 + row] |= (uchar)((data[bpp_pos[2]] & mask) == mask)
                                    << 2;
        tile.data[col * 8 + row] |= (uchar)((data[bpp_pos[3]] & mask) == mask)
                                    << 3;
      }
      if (bpp == 8) {
        bpp_pos[4] = offset + 32 + col * 2;
        bpp_pos[5] = offset + 32 + col * 2 + 1;
        bpp_pos[6] = offset + 48 + col * 2;
        bpp_pos[7] = offset + 48 + col * 2 + 1;
        tile.data[col * 8 + row] |= (uchar)((data[bpp_pos[4]] & mask) == mask)
                                    << 4;
        tile.data[col * 8 + row] |= (uchar)((data[bpp_pos[5]] & mask) == mask)
                                    << 5;
        tile.data[col * 8 + row] |= (uchar)((data[bpp_pos[6]] & mask) == mask)
                                    << 6;
        tile.data[col * 8 + row] |= (uchar)((data[bpp_pos[7]] & mask) == mask)
                                    << 7;
      }
    }
  }
  return tile;
}

Bytes PackBppTile(const tile8& tile, const uint32_t bpp) {
  // Allocate memory for output data
  std::vector<uchar> output(bpp * 8, 0);  // initialized with 0
  unsigned maxcolor = 2 << bpp;

  // Iterate over all columns and rows of the tile
  for (unsigned int col = 0; col < 8; col++) {
    for (unsigned int row = 0; row < 8; row++) {
      uchar color = tile.data[col * 8 + row];
      if (color > maxcolor) {
        throw std::invalid_argument("Invalid color value.");
      }

      // 1bpp format
      if (bpp == 1) output[col] += (uchar)((color & 1) << (7 - row));

      // 2bpp format
      if (bpp >= 2) {
        output[col * 2] += (uchar)((color & 1) << (7 - row));
        output[col * 2 + 1] += (uchar)((uchar)((color & 2) == 2) << (7 - row));
      }

      // 3bpp format
      if (bpp == 3)
        output[16 + col] += (uchar)(((color & 4) == 4) << (7 - row));

      // 4bpp format
      if (bpp >= 4) {
        output[16 + col * 2] += (uchar)(((color & 4) == 4) << (7 - row));
        output[16 + col * 2 + 1] += (uchar)(((color & 8) == 8) << (7 - row));
      }

      // 8bpp format
      if (bpp == 8) {
        output[32 + col * 2] += (uchar)(((color & 16) == 16) << (7 - row));
        output[32 + col * 2 + 1] += (uchar)(((color & 32) == 32) << (7 - row));
        output[48 + col * 2] += (uchar)(((color & 64) == 64) << (7 - row));
        output[48 + col * 2 + 1] +=
            (uchar)(((color & 128) == 128) << (7 - row));
      }
    }
  }
  return output;
}

std::vector<uchar> ConvertBpp(const std::vector<uchar>& tiles,
                              uint32_t from_bpp, uint32_t to_bpp) {
  unsigned int nb_tile = tiles.size() / (from_bpp * 8);
  std::vector<uchar> converted(nb_tile * to_bpp * 8);

  for (unsigned int i = 0; i < nb_tile; i++) {
    tile8 tile = UnpackBppTile(tiles, i * from_bpp * 8, from_bpp);
    std::vector<uchar> packed_tile = PackBppTile(tile, to_bpp);
    std::memcpy(converted.data() + i * to_bpp * 8, packed_tile.data(),
                to_bpp * 8);
  }
  return converted;
}

std::vector<uchar> Convert3bppTo4bpp(const std::vector<uchar>& tiles) {
  return ConvertBpp(tiles, 3, 4);
}

std::vector<uchar> Convert4bppTo3bpp(const std::vector<uchar>& tiles) {
  return ConvertBpp(tiles, 4, 3);
}

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

Bytes Bpp8SnesToIndexed(Bytes data, uint64_t bpp) {
  // 3BPP
  // [r0,bp1],[r0,bp2],[r1,bp1],[r1,bp2],[r2,bp1],[r2,bp2],[r3,bp1],[r3,bp2]
  // [r4,bp1],[r4,bp2],[r5,bp1],[r5,bp2],[r6,bp1],[r6,bp2],[r7,bp1],[r7,bp2]
  // [r0,bp3],[r0,bp4],[r1,bp3],[r1,bp4],[r2,bp3],[r2,bp4],[r3,bp3],[r3,bp4]
  // [r4,bp3],[r4,bp4],[r5,bp3],[r5,bp4],[r6,bp3],[r6,bp4],[r7,bp3],[r7,bp4]
  // [r0,bp5],[r0,bp6],[r1,bp5],[r1,bp6],[r2,bp5],[r2,bp6],[r3,bp5],[r3,bp6]
  // [r4,bp5],[r4,bp6],[r5,bp5],[r5,bp6],[r6,bp5],[r6,bp6],[r7,bp5],[r7,bp6]
  // [r0,bp7],[r0,bp8],[r1,bp7],[r1,bp8],[r2,bp7],[r2,bp8],[r3,bp7],[r3,bp8]
  // [r4,bp7],[r4,bp8],[r5,bp7],[r5,bp8],[r6,bp7],[r6,bp8],[r7,bp7],[r7,bp8]

  // 16 tiles = 1024 bytes
  auto buffer = Bytes(data.size());
  std::vector<std::vector<uint8_t>> bitmap_data;
  bitmap_data.resize(0x80);
  for (auto& each : bitmap_data) {
    each.reserve(0x800);
  }
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
        // bitmap_data[((x + xx) * sheet_width) + y + (yy * 8)] = b;
        bitmap_data[x + xx][y + (yy * 8)] = b;
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
        // buffer[n] = bitmap_data[(x * sheet_width) + y];
        buffer[n] = bitmap_data[x][y];
        n++;
      }
    }
  }
  return buffer;
}

uint16_t TileInfoToWord(TileInfo tile_info) {
  uint16_t result = 0;

  // Copy the id_ value
  result |= tile_info.id_ & 0x3FF;  // ids are 10 bits

  // Set the vertical_mirror_, horizontal_mirror_, and over_ flags
  result |= (tile_info.vertical_mirror_ ? 1 : 0) << 15;
  result |= (tile_info.horizontal_mirror_ ? 1 : 0) << 14;
  result |= (tile_info.over_ ? 1 : 0) << 13;

  // Set the palette_
  result |= (tile_info.palette_ & 0x07) << 10;  // palettes are 3 bits

  return result;
}

TileInfo WordToTileInfo(uint16_t word) {
  // Extract the id_ value
  uint16_t id = word & 0x3FF;  // ids are 10 bits

  // Extract the vertical_mirror_, horizontal_mirror_, and over_ flags
  bool vertical_mirror = (word >> 15) & 0x01;
  bool horizontal_mirror = (word >> 14) & 0x01;
  bool over = (word >> 13) & 0x01;

  // Extract the palette_
  uint8_t palette = (word >> 10) & 0x07;  // palettes are 3 bits

  return TileInfo(id, palette, vertical_mirror, horizontal_mirror, over);
}

ushort TileInfoToShort(TileInfo tile_info) {
  // ushort result = 0;

  // // Copy the id_ value
  // result |= tile_info.id_ & 0x3FF;  // ids are 10 bits

  // // Set the vertical_mirror_, horizontal_mirror_, and over_ flags
  // result |= (tile_info.vertical_mirror_ ? 1 : 0) << 10;
  // result |= (tile_info.horizontal_mirror_ ? 1 : 0) << 11;
  // result |= (tile_info.over_ ? 1 : 0) << 12;

  // // Set the palette_
  // result |= (tile_info.palette_ & 0x07) << 13;  // palettes are 3 bits

  ushort value = 0;
  // vhopppcc cccccccc
  if (tile_info.over_) {
    value |= core::TilePriorityBit;
  }
  if (tile_info.horizontal_mirror_) {
    value |= core::TileHFlipBit;
  }
  if (tile_info.vertical_mirror_) {
    value |= core::TileVFlipBit;
  }
  value |= (ushort)((tile_info.palette_ << 10) & 0x1C00);
  value |= (ushort)(tile_info.id_ & core::TileNameMask);

  return value;
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