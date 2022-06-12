#include "Tile.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "png.h"

namespace yaze {
namespace Application {
namespace Graphics {

ushort TileInfo::toShort() {
  ushort value = 0;
  // vhopppcc cccccccc
  if (over_ == 1) {
    value |= 0x2000;
  };
  if (horizontal_mirror_ == 1) {
    value |= 0x4000;
  };
  if (vertical_mirror_ == 1) {
    value |= 0x8000;
  };
  value |= (ushort)((palette_ << 10) & 0x1C00);
  value |= (ushort)(id_ & 0x3FF);
  return value;
}

char *hexString(const char *str, const unsigned int size) {
  char *toret = (char *)malloc(size * 3 + 1);

  unsigned int i;
  for (i = 0; i < size; i++) {
    sprintf(toret + i * 3, "%02X ", (unsigned char)str[i]);
  }
  toret[size * 3] = 0;
  return toret;
}

void export_all_gfx_to_png(byte *tiledata) {

  auto tile = unpack_bpp3_tile(tiledata, 0);
  Graphics::r_palette *pal = palette_create(8, 0);

  for (unsigned int i = 0; i < 8; i++) {
    pal->colors[i].red = i * 30;
    pal->colors[i].blue = i * 30;
    pal->colors[i].green = i * 30;
  }

  FILE *fp = fopen("test.png", "wb");
  png_structp png_ptr =
      png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  png_infop info_ptr = png_create_info_struct(png_ptr);
  png_init_io(png_ptr, fp);
  png_set_strip_alpha(png_ptr);
  png_read_update_info(png_ptr, info_ptr);

  png_color *png_palette =
      (png_color *)png_malloc(png_ptr, pal->size * sizeof(png_color));

  for (unsigned int i = 0; i < pal->size; i++) {
    png_palette[i].blue = pal->colors[i].blue;
    png_palette[i].green = pal->colors[i].green;
    png_palette[i].red = pal->colors[i].red;
  }
  png_set_IHDR(png_ptr, info_ptr, 8, 8, 8, PNG_COLOR_TYPE_PALETTE,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
               PNG_FILTER_TYPE_BASE);
  png_set_PLTE(png_ptr, info_ptr, png_palette, pal->size);

  png_write_info(png_ptr, info_ptr);
  png_set_packing(png_ptr);

  png_byte *row_pointers[8];
  for (unsigned int i = 0; i < 8; i++) {
    row_pointers[i] = (png_byte *)png_malloc(png_ptr, sizeof(png_byte));
    memcpy(row_pointers[i], tile.data + i * 8, 8);
  }

  png_write_image(png_ptr, row_pointers);

  png_write_end(png_ptr, info_ptr);
  png_destroy_write_struct(&png_ptr, &info_ptr);

  png_free(png_ptr, png_palette);
  png_free(png_ptr, row_pointers);
}

void export_tile_to_png(tile8 rawtile, const r_palette pal,
                        const char *filename) {
  FILE *fp = fopen(filename, "wb");
  png_structp png_ptr =
      png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  png_infop info_ptr = png_create_info_struct(png_ptr);
  png_init_io(png_ptr, fp);
  png_set_strip_alpha(png_ptr);
  png_read_update_info(png_ptr, info_ptr);

  png_color *png_palette =
      (png_color *)png_malloc(png_ptr, pal.size * sizeof(png_color));

  for (unsigned int i = 0; i < pal.size; i++) {
    png_palette[i].blue = pal.colors[i].blue;
    png_palette[i].green = pal.colors[i].green;
    png_palette[i].red = pal.colors[i].red;
  }
  png_set_IHDR(png_ptr, info_ptr, 8, 8, 8, PNG_COLOR_TYPE_PALETTE,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
               PNG_FILTER_TYPE_BASE);
  png_set_PLTE(png_ptr, info_ptr, png_palette, pal.size);

  png_write_info(png_ptr, info_ptr);
  png_set_packing(png_ptr);

  png_byte *row_pointers[8];
  for (unsigned int i = 0; i < 8; i++) {
    row_pointers[i] = (png_byte *)png_malloc(png_ptr, sizeof(png_byte));
    memcpy(row_pointers[i], rawtile.data + i * 8, 8);
  }

  png_write_image(png_ptr, row_pointers);

  png_write_end(png_ptr, info_ptr);
  png_destroy_write_struct(&png_ptr, &info_ptr);

  png_free(png_ptr, png_palette);
  png_free(png_ptr, row_pointers);
}

tile8 unpack_bpp1_tile(const byte *data, const unsigned int offset) {
  return (unpack_bpp_tile(data, offset, 1));
}

tile8 unpack_bpp2_tile(const byte *data, const unsigned int offset) {
  return (unpack_bpp_tile(data, offset, 2));
}

tile8 unpack_bpp3_tile(const byte *data, const unsigned int offset) {
  return (unpack_bpp_tile(data, offset, 3));
}

tile8 unpack_bpp4_tile(const byte *data, const unsigned int offset) {
  return (unpack_bpp_tile(data, offset, 4));
}

tile8 unpack_bpp8_tile(const byte *data, const unsigned int offset) {
  return (unpack_bpp_tile(data, offset, 8));
}

tile8 unpack_mode7_tile(const byte *data, const unsigned int offset) {
  tile8 tile;
  memcpy(tile.data, data + offset, 64);
  return tile;
}

tile8 unpack_bpp_tile(const byte *data, const unsigned int offset,
                      const unsigned bpp) {
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
       * | byte 1 of first bitplane | byte 1 of the second bitplane | byte 2 of
       * first bitplane | byte 2 of second bitplane | ..
       */
      bpp_pos[0] = offset + col * 2;
      bpp_pos[1] = offset + col * 2 + 1;
      char mask = 1 << (7 - row);
      tile.data[col * 8 + row] = (data[bpp_pos[0]] & mask) == mask;
      tile.data[col * 8 + row] |= ((data[bpp_pos[1]] & mask) == mask) << 1;
      if (bpp == 3) {
        // When we have 3 bitplanes, the bytes for the third bitplane are after
        // the 16 bytes of the 2 bitplanes.
        bpp_pos[2] = offset + 16 + col;
        tile.data[col * 8 + row] |= ((data[bpp_pos[2]] & mask) == mask) << 2;
      }
      if (bpp >= 4) {
        // For 4 bitplanes, the 2 added bitplanes are interlaced like the first
        // two.
        bpp_pos[2] = offset + 16 + col * 2;
        bpp_pos[3] = offset + 16 + col * 2 + 1;
        tile.data[col * 8 + row] |= ((data[bpp_pos[2]] & mask) == mask) << 2;
        tile.data[col * 8 + row] |= ((data[bpp_pos[3]] & mask) == mask) << 3;
      }
      if (bpp == 8) {
        bpp_pos[4] = offset + 32 + col * 2;
        bpp_pos[5] = offset + 32 + col * 2 + 1;
        bpp_pos[6] = offset + 48 + col * 2;
        bpp_pos[7] = offset + 48 + col * 2 + 1;
        tile.data[col * 8 + row] |= ((data[bpp_pos[4]] & mask) == mask) << 4;
        tile.data[col * 8 + row] |= ((data[bpp_pos[5]] & mask) == mask) << 5;
        tile.data[col * 8 + row] |= ((data[bpp_pos[6]] & mask) == mask) << 6;
        tile.data[col * 8 + row] |= ((data[bpp_pos[7]] & mask) == mask) << 7;
      }
    }
  }
  return tile;
}

byte *pack_bpp1_tile(const tile8 tile) {
  unsigned int p = 1;
  return pack_bpp_tile(tile, 1, &p);
}

byte *pack_bpp2_tile(const tile8 tile) {
  unsigned int p = 1;
  return pack_bpp_tile(tile, 2, &p);
}

byte *pack_bpp3_tile(const tile8 tile) {
  unsigned int p = 1;
  return pack_bpp_tile(tile, 3, &p);
}

byte *pack_bpp4_tile(const tile8 tile) {
  unsigned int p = 1;
  return pack_bpp_tile(tile, 4, &p);
}

byte *pack_bpp8_tile(const tile8 tile) {
  unsigned int p = 1;
  return pack_bpp_tile(tile, 8, &p);
}

byte *pack_bpp_tile(tile8 tile, const unsigned int bpp, unsigned int *size) {
  byte *output = (byte *)malloc(bpp * 8);
  memset(output, 0, bpp * 8);
  unsigned maxcolor = 2 << bpp;
  *size = 0;

  for (unsigned int col = 0; col < 8; col++) {
    for (unsigned int row = 0; row < 8; row++) {
      byte color = tile.data[col * 8 + row];
      if (color > maxcolor) return NULL;

      if (bpp == 1) output[col] += (byte)((color & 1) << (7 - row));
      if (bpp >= 2) {
        output[col * 2] += (byte)((color & 1) << (7 - row));
        output[col * 2 + 1] += (byte)(((color & 2) == 2) << (7 - row));
      }
      if (bpp == 3) output[16 + col] += (byte)(((color & 4) == 4) << (7 - row));
      if (bpp >= 4) {
        output[16 + col * 2] += (byte)(((color & 4) == 4) << (7 - row));
        output[16 + col * 2 + 1] += (byte)(((color & 8) == 8) << (7 - row));
      }
      if (bpp == 8) {
        output[32 + col * 2] += (byte)(((color & 16) == 16) << (7 - row));
        output[32 + col * 2 + 1] += (byte)(((color & 32) == 32) << (7 - row));
        output[48 + col * 2] += (byte)(((color & 64) == 64) << (7 - row));
        output[48 + col * 2 + 1] += (byte)(((color & 128) == 128) << (7 - row));
      }
    }
  }
  *size = bpp * 8;
  return output;
}

}  // namespace Graphics
}  // namespace Application
}  // namespace yaze
