#include <memory>
#include <vector>

#include "app/core/constants.h"
#include "tile.h"

namespace yaze {
namespace app {
namespace gfx {

int GetPCGfxAddress(char *romData, char id) {
  char **info1 = new char *[255];
  int gfxPointer1 =
      lorom_snes_to_pc((romData[core::constants::gfx_1_pointer + 1] << 8) +
                           (romData[core::constants::gfx_1_pointer]),
                       info1);
  int gfxPointer2 =
      lorom_snes_to_pc((romData[core::constants::gfx_2_pointer + 1] << 8) +
                           (romData[core::constants::gfx_2_pointer]),
                       info1);
  int gfxPointer3 =
      lorom_snes_to_pc((romData[core::constants::gfx_3_pointer + 1] << 8) +
                           (romData[core::constants::gfx_3_pointer]),
                       info1);

  char gfxGamePointer1 = romData[gfxPointer1 + id];
  char gfxGamePointer2 = romData[gfxPointer2 + id];
  char gfxGamePointer3 = romData[gfxPointer3 + id];

  return lorom_snes_to_pc(
      yaze::app::rom::AddressFromBytes(gfxGamePointer1, gfxGamePointer2,
                                       gfxGamePointer3),
      info1);
}

char *CreateAllGfxDataRaw(char *romData) {
  // 0-112 -> compressed 3bpp bgr -> (decompressed each) 0x600 chars
  // 113-114 -> compressed 2bpp -> (decompressed each) 0x800 chars
  // 115-126 -> uncompressed 3bpp sprites -> (each) 0x600 chars
  // 127-217 -> compressed 3bpp sprites -> (decompressed each) 0x600 chars
  // 218-222 -> compressed 2bpp -> (decompressed each) 0x800 chars

  char *buffer = new char[346624];
  int bufferPos = 0;
  char *data = new char[2048];
  unsigned int uncompressedSize = 0;
  unsigned int compressedSize = 0;

  for (int i = 0; i < core::constants::NumberOfSheets; i++) {
    isbpp3[i] = ((i >= 0 && i <= 112) ||    // Compressed 3bpp bg
                 (i >= 115 && i <= 126) ||  // Uncompressed 3bpp sprites
                 (i >= 127 && i <= 217)     // Compressed 3bpp sprites
    );

    // uncompressed sheets
    if (i >= 115 && i <= 126) {
      data = new char[core::constants::Uncompressed3BPPSize];
      int startAddress = GetPCGfxAddress(romData, (char)i);
      for (int j = 0; j < core::constants::Uncompressed3BPPSize; j++) {
        data[j] = romData[j + startAddress];
      }
    } else {
      data = alttp_decompress_gfx((char *)romData,
                                  GetPCGfxAddress(romData, (char)i),
                                  core::constants::UncompressedSheetSize,
                                  &uncompressedSize, &compressedSize);
    }

    for (int j = 0; j < sizeof(data); j++) {
      buffer[j + bufferPos] = data[j];
    }

    bufferPos += sizeof(data);
  }

  return buffer;
}

void CreateAllGfxData(char *romData, char *allgfx16Ptr) {
  char *data = CreateAllGfxDataRaw(romData);
  char *newData = new char[0x6F800];
  uchar *mask = new uchar[]{0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
  int sheetPosition = 0;

  // 8x8 tile
  for (int s = 0; s < core::constants::NumberOfSheets; s++)  // Per Sheet
  {
    for (int j = 0; j < 4; j++)  // Per Tile Line Y
    {
      for (int i = 0; i < 16; i++)  // Per Tile Line X
      {
        for (int y = 0; y < 8; y++)  // Per Pixel Line
        {
          if (isbpp3[s]) {
            char lineBits0 =
                data[(y * 2) + (i * 24) + (j * 384) + sheetPosition];
            char lineBits1 =
                data[(y * 2) + (i * 24) + (j * 384) + 1 + sheetPosition];
            char lineBits2 =
                data[(y) + (i * 24) + (j * 384) + 16 + sheetPosition];

            for (int x = 0; x < 4; x++)  // Per Pixel X
            {
              char pixdata = 0;
              char pixdata2 = 0;

              if ((lineBits0 & mask[(x * 2)]) == mask[(x * 2)]) {
                pixdata += 1;
              }
              if ((lineBits1 & mask[(x * 2)]) == mask[(x * 2)]) {
                pixdata += 2;
              }
              if ((lineBits2 & mask[(x * 2)]) == mask[(x * 2)]) {
                pixdata += 4;
              }

              if ((lineBits0 & mask[(x * 2) + 1]) == mask[(x * 2) + 1]) {
                pixdata2 += 1;
              }
              if ((lineBits1 & mask[(x * 2) + 1]) == mask[(x * 2) + 1]) {
                pixdata2 += 2;
              }
              if ((lineBits2 & mask[(x * 2) + 1]) == mask[(x * 2) + 1]) {
                pixdata2 += 4;
              }

              newData[(y * 64) + (x) + (i * 4) + (j * 512) + (s * 2048)] =
                  (char)((pixdata << 4) | pixdata2);
            }
          } else {
            char lineBits0 =
                data[(y * 2) + (i * 16) + (j * 256) + sheetPosition];
            char lineBits1 =
                data[(y * 2) + (i * 16) + (j * 256) + 1 + sheetPosition];

            for (int x = 0; x < 4; x++)  // Per Pixel X
            {
              char pixdata = 0;
              char pixdata2 = 0;

              if ((lineBits0 & mask[(x * 2)]) == mask[(x * 2)]) {
                pixdata += 1;
              }
              if ((lineBits1 & mask[(x * 2)]) == mask[(x * 2)]) {
                pixdata += 2;
              }

              if ((lineBits0 & mask[(x * 2) + 1]) == mask[(x * 2) + 1]) {
                pixdata2 += 1;
              }
              if ((lineBits1 & mask[(x * 2) + 1]) == mask[(x * 2) + 1]) {
                pixdata2 += 2;
              }

              newData[(y * 64) + (x) + (i * 4) + (j * 512) + (s * 2048)] =
                  (char)((pixdata << 4) | pixdata2);
            }
          }
        }
      }
    }

    if (isbpp3[s]) {
      sheetPosition += core::constants::Uncompressed3BPPSize;
    } else {
      sheetPosition += core::constants::UncompressedSheetSize;
    }
  }

  char *allgfx16Data = (char *)allgfx16Ptr;

  for (int i = 0; i < 0x6F800; i++) {
    allgfx16Data[i] = newData[i];
  }
}

void BuildTiles16Gfx(uchar* mapblockset16, uchar* currentOWgfx16Ptr,
                     std::vector<Tile16>& allTiles) {
  uchar* gfx16Data = mapblockset16;
  uchar* gfx8Data = currentOWgfx16Ptr;
  const int offsets[4] = {0, 8, 1024, 1032};
  auto yy = 0;
  auto xx = 0;

  // Number of tiles16 3748? // its 3752
  for (auto i = 0; i < core::constants::NumberOfMap16; i++) {
    // 8x8 tile draw
    // gfx8 = 4bpp so everyting is /2
    auto tiles = allTiles[i];

    for (auto tile = 0; tile < 4; tile++) {
      TileInfo info = tiles.tiles_info[tile];
      int offset = offsets[tile];

      for (auto y = 0; y < 8; y++) {
        for (auto x = 0; x < 4; x++) {
          CopyTile16(x, y, xx, yy, offset, info, gfx16Data, gfx8Data);
        }
      }
    }

    xx += 16;
    if (xx >= 128) {
      yy += 2048;
      xx = 0;
    }
  }
}

void CopyTile16(int x, int y, int xx, int yy, int offset, TileInfo tile,
                uchar* gfx16Pointer, uchar* gfx8Pointer)  // map,current
{
  int mx = x;
  int my = y;
  uchar r = 0;

  if (tile.horizontal_mirror_) {
    mx = 3 - x;
    r = 1;
  }
  if (tile.vertical_mirror_) {
    my = 7 - y;
  }

  int tx = ((tile.id_ / 16) * 512) + ((tile.id_ - ((tile.id_ / 16) * 16)) * 4);
  auto index = xx + yy + offset + (mx * 2) + (my * 128);
  uchar pixel = gfx8Pointer[tx + (y * 64) + x];

  gfx16Pointer[index + r ^ 1] = (uchar)((pixel & 0x0F) + tile.palette_ * 16);
  gfx16Pointer[index + r] = (uchar)(((pixel >> 4) & 0x0F) + tile.palette_ * 16);
}

}  // namespace gfx
}  // namespace app
}  // namespace yaze