#include "bitmap.h"

#include <rommapping.h>

#include "rom.h"

namespace yaze {
namespace application {
namespace gfx {

int GetPCGfxAddress(char *romData, char id) {
  char **info1 = new char *[255];
  int gfxPointer1 =
      lorom_snes_to_pc((romData[core::Constants::gfx_1_pointer + 1] << 8) +
                           (romData[core::Constants::gfx_1_pointer]),
                       info1);
  int gfxPointer2 =
      lorom_snes_to_pc((romData[core::Constants::gfx_2_pointer + 1] << 8) +
                           (romData[core::Constants::gfx_2_pointer]),
                       info1);
  int gfxPointer3 =
      lorom_snes_to_pc((romData[core::Constants::gfx_3_pointer + 1] << 8) +
                           (romData[core::Constants::gfx_3_pointer]),
                       info1);

  char gfxGamePointer1 = romData[gfxPointer1 + id];
  char gfxGamePointer2 = romData[gfxPointer2 + id];
  char gfxGamePointer3 = romData[gfxPointer3 + id];

  return lorom_snes_to_pc(
      Data::AddressFromBytes(gfxGamePointer1, gfxGamePointer2, gfxGamePointer3),
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

  for (int i = 0; i < core::Constants::NumberOfSheets; i++) {
    isbpp3[i] = ((i >= 0 && i <= 112) ||    // Compressed 3bpp bg
                 (i >= 115 && i <= 126) ||  // Uncompressed 3bpp sprites
                 (i >= 127 && i <= 217)     // Compressed 3bpp sprites
    );

    // uncompressed sheets
    if (i >= 115 && i <= 126) {
      data = new char[core::Constants::Uncompressed3BPPSize];
      int startAddress = GetPCGfxAddress(romData, (char)i);
      for (int j = 0; j < core::Constants::Uncompressed3BPPSize; j++) {
        data[j] = romData[j + startAddress];
      }
    } else {
      data = alttp_decompress_gfx((char *)romData,
                                  GetPCGfxAddress(romData, (char)i),
                                  core::Constants::UncompressedSheetSize,
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
  for (int s = 0; s < core::Constants::NumberOfSheets; s++)  // Per Sheet
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
      sheetPosition += core::Constants::Uncompressed3BPPSize;
    } else {
      sheetPosition += core::Constants::UncompressedSheetSize;
    }
  }

  char *allgfx16Data = (char *)allgfx16Ptr;

  for (int i = 0; i < 0x6F800; i++) {
    allgfx16Data[i] = newData[i];
  }
}

}  // namespace gfx
}  // namespace application
}  // namespace yaze
