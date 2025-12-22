#include "snes_tile.h"

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace yaze {
namespace gfx {

// Bit set for object priority
constexpr uint16_t TilePriorityBit = 0x2000;

// Bit set for object hflip
constexpr uint16_t TileHFlipBit = 0x4000;

// Bit set for object vflip
constexpr uint16_t TileVFlipBit = 0x8000;

// Bits used for tile name
constexpr uint16_t TileNameMask = 0x03FF;

snes_tile8 UnpackBppTile(std::span<uint8_t> data, const uint32_t offset,
                         const uint32_t bpp) {
  snes_tile8 tile = {};  // Initialize to zero
  assert(bpp >= 1 && bpp <= 8);
  unsigned int bpp_pos[8];               // More for conveniance and readibility
  for (int row = 0; row < 8; row++) {    // Process rows first (Y coordinate)
    for (int col = 0; col < 8; col++) {  // Then columns (X coordinate)
      if (bpp == 1) {
        tile.data[row * 8 + col] = (data[offset + row] >> (7 - col)) & 0x01;
        continue;
      }
      /* SNES bpp format interlace each byte of the first 2 bitplanes.
       * | byte 1 of first bitplane | byte 1 of second bitplane |
       * | byte 2 of first bitplane | byte 2 of second bitplane | ..
       */
      bpp_pos[0] = offset + row * 2;
      bpp_pos[1] = offset + row * 2 + 1;
      char mask = 1 << (7 - col);
      tile.data[row * 8 + col] = (data[bpp_pos[0]] & mask) ? 1 : 0;
      tile.data[row * 8 + col] |= ((data[bpp_pos[1]] & mask) ? 1 : 0) << 1;
      if (bpp == 3) {
        // When we have 3 bitplanes, the bytes for the third bitplane are after
        // the 16 bytes of the 2 bitplanes.
        bpp_pos[2] = offset + 16 + row;
        tile.data[row * 8 + col] |= ((data[bpp_pos[2]] & mask) ? 1 : 0) << 2;
      }
      if (bpp >= 4) {
        // For 4 bitplanes, the 2 added bitplanes are interlaced like the first
        // two.
        bpp_pos[2] = offset + 16 + row * 2;
        bpp_pos[3] = offset + 16 + row * 2 + 1;
        tile.data[row * 8 + col] |= ((data[bpp_pos[2]] & mask) ? 1 : 0) << 2;
        tile.data[row * 8 + col] |= ((data[bpp_pos[3]] & mask) ? 1 : 0) << 3;
      }
      if (bpp == 8) {
        bpp_pos[4] = offset + 32 + row * 2;
        bpp_pos[5] = offset + 32 + row * 2 + 1;
        bpp_pos[6] = offset + 48 + row * 2;
        bpp_pos[7] = offset + 48 + row * 2 + 1;
        tile.data[row * 8 + col] |= ((data[bpp_pos[4]] & mask) ? 1 : 0) << 4;
        tile.data[row * 8 + col] |= ((data[bpp_pos[5]] & mask) ? 1 : 0) << 5;
        tile.data[row * 8 + col] |= ((data[bpp_pos[6]] & mask) ? 1 : 0) << 6;
        tile.data[row * 8 + col] |= ((data[bpp_pos[7]] & mask) ? 1 : 0) << 7;
      }
    }
  }
  return tile;
}

std::vector<uint8_t> PackBppTile(const snes_tile8& tile, const uint32_t bpp) {
  // Allocate memory for output data
  std::vector<uint8_t> output(bpp * 8, 0);  // initialized with 0
  unsigned maxcolor = 1 << bpp;  // Fix: should be 1 << bpp, not 2 << bpp

  // Iterate over all rows and columns of the tile
  for (unsigned int row = 0; row < 8; row++) {
    for (unsigned int col = 0; col < 8; col++) {
      uint8_t color = tile.data[row * 8 + col];
      if (color >= maxcolor) {
        throw std::invalid_argument("Invalid color value.");
      }

      // 1bpp format
      if (bpp == 1)
        output[row] += (uint8_t)((color & 1) << (7 - col));

      // 2bpp format
      if (bpp >= 2) {
        output[row * 2] += ((color & 1) << (7 - col));
        output[row * 2 + 1] += (((color & 2) == 2) << (7 - col));
      }

      // 3bpp format
      if (bpp == 3)
        output[16 + row] += (((color & 4) == 4) << (7 - col));

      // 4bpp format
      if (bpp >= 4) {
        output[16 + row * 2] += (((color & 4) == 4) << (7 - col));
        output[16 + row * 2 + 1] += (((color & 8) == 8) << (7 - col));
      }

      // 8bpp format
      if (bpp == 8) {
        output[32 + row * 2] += (((color & 16) == 16) << (7 - col));
        output[32 + row * 2 + 1] += (((color & 32) == 32) << (7 - col));
        output[48 + row * 2] += (((color & 64) == 64) << (7 - col));
        output[48 + row * 2 + 1] += (((color & 128) == 128) << (7 - col));
      }
    }
  }
  return output;
}

std::vector<uint8_t> ConvertBpp(std::span<uint8_t> tiles, uint32_t from_bpp,
                                uint32_t to_bpp) {
  unsigned int nb_tile = tiles.size() / (from_bpp * 8);
  std::vector<uint8_t> converted(nb_tile * to_bpp * 8);

  for (unsigned int i = 0; i < nb_tile; i++) {
    snes_tile8 tile = UnpackBppTile(tiles, i * from_bpp * 8, from_bpp);
    std::vector<uint8_t> packed_tile = PackBppTile(tile, to_bpp);
    std::memcpy(converted.data() + i * to_bpp * 8, packed_tile.data(),
                to_bpp * 8);
  }
  return converted;
}

std::vector<uint8_t> SnesTo8bppSheet(std::span<uint8_t> sheet, int bpp,
                                     int num_sheets) {
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
  } else if (bpp == 4) {
    bpp = 32;
    buffer_size = 0x4000;
  } else if (bpp == 8) {
    bpp = 64;
  }

  if (num_sheets != 1) {
    num_tiles *= num_sheets;
    buffer_size *= num_sheets;
  }

  // Safety check: Ensure input data is sufficient for requested tiles
  if (static_cast<size_t>(num_tiles * bpp) > sheet.size()) {
    // Clamp number of tiles to what we can read from input
    // This prevents SIGSEGV if decompression returned truncated data
    if (bpp > 0) {
      num_tiles = static_cast<int>(sheet.size()) / bpp;
    } else {
      num_tiles = 0;
    }
  }

  std::vector<uint8_t> sheet_buffer_out(buffer_size); // Zero initialized

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

std::vector<uint8_t> Bpp8SnesToIndexed(std::vector<uint8_t> data,
                                       uint64_t bpp) {
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
  auto buffer = std::vector<uint8_t>(data.size());
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

uint16_t TileInfoToShort(TileInfo tile_info) {
  // uint16_t result = 0;

  // Copy the id_ value
  // result |= tile_info.id_ & 0x3FF;  // ids are 10 bits

  // Set the vertical_mirror_, horizontal_mirror_, and over_ flags
  // result |= (tile_info.vertical_mirror_ ? 1 : 0) << 10;
  // result |= (tile_info.horizontal_mirror_ ? 1 : 0) << 11;
  // result |= (tile_info.over_ ? 1 : 0) << 12;

  // Set the palette_
  // result |= (tile_info.palette_ & 0x07) << 13;  // palettes are 3 bits

  uint16_t value = 0;
  // vhopppcc cccccccc
  if (tile_info.over_) {
    value |= TilePriorityBit;
  }
  if (tile_info.horizontal_mirror_) {
    value |= TileHFlipBit;
  }
  if (tile_info.vertical_mirror_) {
    value |= TileVFlipBit;
  }
  value |= (uint16_t)((tile_info.palette_ << 10) & 0x1C00);
  value |= (uint16_t)(tile_info.id_ & TileNameMask);

  return value;
}

TileInfo GetTilesInfo(uint16_t tile) {
  // vhopppcc cccccccc
  uint16_t tid = (uint16_t)(tile & TileNameMask);
  uint8_t p = (uint8_t)((tile >> 10) & 0x07);

  bool o = ((tile & TilePriorityBit) == TilePriorityBit);
  bool h = ((tile & TileHFlipBit) == TileHFlipBit);
  bool v = ((tile & TileVFlipBit) == TileVFlipBit);

  return TileInfo(tid, p, v, h, o);
}

void CopyTile8bpp16(int x, int y, int tile, std::vector<uint8_t>& bitmap,
                    std::vector<uint8_t>& blockset) {
  int src_pos =
      ((tile - ((tile / 0x08) * 0x08)) * 0x10) + ((tile / 0x08) * 2048);
  int dest_pos = (x + (y * 0x200));
  for (int yy = 0; yy < 0x10; yy++) {
    for (int xx = 0; xx < 0x10; xx++) {
      bitmap[dest_pos + xx + (yy * 0x200)] =
          blockset[src_pos + xx + (yy * 0x80)];
    }
  }
}

std::vector<uint8_t> LoadSNES4bppGFXToIndexedColorMatrix(
    std::span<uint8_t> src) {
  std::vector<uint8_t> dest;
  uint8_t b0;
  uint8_t b1;
  uint8_t b2;
  uint8_t b3;
  int res;
  int mul;
  int y_adder = 0;
  int src_index;
  int dest_x;
  int dest_y;
  int dest_index;
  int main_index_limit = src.size() / 32;
  for (int main_index = 0; main_index <= main_index_limit; main_index += 32) {
    src_index = (main_index << 5);
    if (src_index + 31 >= src.size()) {
      throw std::invalid_argument("src_index + 31 >= src.size()");
    }
    dest_x = main_index & 0x0F;
    dest_y = main_index >> 4;
    dest_index = ((dest_y << 7) + dest_x) << 3;
    if (dest_index + 903 >= dest.size()) {
      throw std::invalid_argument("dest_index + 903 >= dest.size()");
    }
    for (int i = 0; i < 16; i += 2) {
      mul = 1;
      b0 = src[src_index + i];
      b1 = src[src_index + i + 1];
      b2 = src[src_index + i + 16];
      b3 = src[src_index + i + 17];
      for (int j = 0; j < 8; j++) {
        res = ((b0 & mul) | ((b1 & mul) << 1) | ((b2 & mul) << 2) |
               ((b3 & mul) << 3)) >>
              j;
        dest[dest_index + (7 - j) + y_adder] = res;
        mul <<= 1;
      }
      y_adder += 128;
    }
  }
  return dest;
}

}  // namespace gfx
}  // namespace yaze
