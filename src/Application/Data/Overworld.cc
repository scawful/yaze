#include "Overworld.h"

#include "Tile.h"

namespace yaze {
namespace Application {
namespace Data {

using namespace Core;

static TileInfo GetTilesInfo(ushort tile) {
  // vhopppcc cccccccc
  ushort o = 0;
  ushort v = 0;
  ushort h = 0;
  ushort tid = (ushort)(tile & 0x3FF);
  byte p = (byte)((tile >> 10) & 0x07);

  o = (ushort)((tile & 0x2000) >> 13);
  h = (ushort)((tile & 0x4000) >> 14);
  v = (ushort)((tile & 0x8000) >> 15);

  return TileInfo(tid, p, v, h, o);
}

Overworld::Overworld(Utils::ROM rom) : rom_(rom) {
  for (int i = 0; i < 0x2B; i++) {
    tileLeftEntrance[i] = (ushort)rom_.ReadShort(
        Constants::overworldEntranceAllowedTilesLeft + (i * 2));
    tileRightEntrance[i] = (ushort)rom_.ReadShort(
        Constants::overworldEntranceAllowedTilesRight + (i * 2));
  }

  AssembleMap32Tiles();
  AssembleMap16Tiles();
}

ushort Overworld::GenerateTile32(int i, int k, int dimension) {
  return (ushort)(rom_.GetRawData()[map32address[dimension] + k + (i)] +
                  (((rom_.GetRawData()[map32address[dimension] + (i) +
                                       (k <= 1 ? 4 : 5)] >>
                     (k % 2 == 0 ? 4 : 0)) &
                    0x0F) *
                   256));
}

void Overworld::AssembleMap32Tiles() {
  for (int i = 0; i < 0x33F0; i += 6) {
    ushort tl, tr, bl, br;
    for (int k = 0; k < 4; k++) {
      tl = GenerateTile32(i, k, (int)Dimension::map32TilesTL);
      tr = GenerateTile32(i, k, (int)Dimension::map32TilesTR);
      bl = GenerateTile32(i, k, (int)Dimension::map32TilesBL);
      br = GenerateTile32(i, k, (int)Dimension::map32TilesBR);
      tiles32.push_back(Tile32(tl, tr, bl, br));
    }
  }
}

void Overworld::AssembleMap16Tiles() {
  int tpos = Core::Constants::map16Tiles;
  for (int i = 0; i < 4096; i += 1)  // 3760
  {
    TileInfo t0 = GetTilesInfo((uintptr_t)rom_.GetRawData() + tpos);
    tpos += 2;
    TileInfo t1 = GetTilesInfo((uintptr_t)rom_.GetRawData() + tpos);
    tpos += 2;
    TileInfo t2 = GetTilesInfo((uintptr_t)rom_.GetRawData() + tpos);
    tpos += 2;
    TileInfo t3 = GetTilesInfo((uintptr_t)rom_.GetRawData() + tpos);
    tpos += 2;
    tiles16.push_back(Tile16(t0, t1, t2, t3));
  }
}

void Overworld::DecompressAllMapTiles() {
  int lowest = 0x0FFFFF;
  int highest = 0x0F8000;
  // int npos = 0;
  int sx = 0;
  int sy = 0;
  int c = 0;
  // int furthestPtr = 0;
  for (int i = 0; i < 160; i++) {
    int p1 = (rom_.GetRawData()[(Constants::compressedAllMap32PointersHigh) +
                                2 + (int)(3 * i)]
              << 16) +
             (rom_.GetRawData()[(Constants::compressedAllMap32PointersHigh) +
                                1 + (int)(3 * i)]
              << 8) +
             (rom_.GetRawData()[(Constants::compressedAllMap32PointersHigh +
                                 (int)(3 * i))]);
    p1 = rom_.SnesToPc(p1);

    int p2 = (rom_.GetRawData()[(Constants::compressedAllMap32PointersLow) + 2 +
                                (int)(3 * i)]
              << 16) +
             (rom_.GetRawData()[(Constants::compressedAllMap32PointersLow) + 1 +
                                (int)(3 * i)]
              << 8) +
             (rom_.GetRawData()[(Constants::compressedAllMap32PointersLow +
                                 (int)(3 * i))]);
    p2 = rom_.SnesToPc(p2);

    int ttpos = 0;
    unsigned int compressedSize1 = 0;
    unsigned int compressedSize2 = 0;
    unsigned int compressedLength1 = 0;
    unsigned int compressedLength2 = 0;

    if (p1 >= highest) {
      highest = p1;
    }
    if (p2 >= highest) {
      highest = p2;
    }

    if (p1 <= lowest) {
      if (p1 > 0x0F8000) {
        lowest = p1;
      }
    }
    if (p2 <= lowest) {
      if (p2 > 0x0F8000) {
        lowest = p2;
      }
    }

    auto bytes = alttp_compressor_.DecompressOverworld(
        rom_.GetRawData(), p2, 1000, &compressedSize1, &compressedLength1);
    auto bytes2 = alttp_compressor_.DecompressOverworld(
        rom_.GetRawData(), p1, 1000, &compressedSize2, &compressedLength2);

    for (int y = 0; y < 16; y++) {
      for (int x = 0; x < 16; x++) {
        ushort tidD = (ushort)((bytes2[ttpos] << 8) + bytes[ttpos]);

        int tpos = tidD;
        if (tpos < tiles32.size()) {
          // map16tiles[npos] = new Tile32(tiles32[tpos].tile0,
          // tiles32[tpos].tile1, tiles32[tpos].tile2, tiles32[tpos].tile3);

          if (i < 64) {
            allmapsTilesLW[(x * 2) + (sx * 32)][(y * 2) + (sy * 32)] =
                tiles32[tpos].tile0_;
            allmapsTilesLW[(x * 2) + 1 + (sx * 32)][(y * 2) + (sy * 32)] =
                tiles32[tpos].tile1_;
            allmapsTilesLW[(x * 2) + (sx * 32)][(y * 2) + 1 + (sy * 32)] =
                tiles32[tpos].tile2_;
            allmapsTilesLW[(x * 2) + 1 + (sx * 32)][(y * 2) + 1 + (sy * 32)] =
                tiles32[tpos].tile3_;
          } else if (i < 128 && i >= 64) {
            allmapsTilesDW[(x * 2) + (sx * 32)][(y * 2) + (sy * 32)] =
                tiles32[tpos].tile0_;
            allmapsTilesDW[(x * 2) + 1 + (sx * 32)][(y * 2) + (sy * 32)] =
                tiles32[tpos].tile1_;
            allmapsTilesDW[(x * 2) + (sx * 32)][(y * 2) + 1 + (sy * 32)] =
                tiles32[tpos].tile2_;
            allmapsTilesDW[(x * 2) + 1 + (sx * 32)][(y * 2) + 1 + (sy * 32)] =
                tiles32[tpos].tile3_;
          } else {
            allmapsTilesSP[(x * 2) + (sx * 32)][(y * 2) + (sy * 32)] =
                tiles32[tpos].tile0_;
            allmapsTilesSP[(x * 2) + 1 + (sx * 32)][(y * 2) + (sy * 32)] =
                tiles32[tpos].tile1_;
            allmapsTilesSP[(x * 2) + (sx * 32)][(y * 2) + 1 + (sy * 32)] =
                tiles32[tpos].tile2_;
            allmapsTilesSP[(x * 2) + 1 + (sx * 32)][(y * 2) + 1 + (sy * 32)] =
                tiles32[tpos].tile3_;
          }
        }

        ttpos += 1;
      }
    }

    sx++;
    if (sx >= 8) {
      sy++;
      sx = 0;
    }

    c++;
    if (c >= 64) {
      sx = 0;
      sy = 0;
      c = 0;
    }
  }

  std::cout << "MapPointers(lowest) : " << lowest << std::endl;
  std::cout << "MapPointers(highest) : " << highest << std::endl;
}

void Overworld::LoadOverworldMap() {
  // GFX.overworldMapBitmap = new Bitmap(
  //     128, 128, 128, PixelFormat.Format8bppIndexed, GFX.overworldMapPointer);
  // GFX.owactualMapBitmap = new Bitmap(
  //     512, 512, 512, PixelFormat.Format8bppIndexed, GFX.owactualMapPointer);

  // Mode 7
  byte* ptr = (byte*)overworldMapPointer.get();

  int pos = 0;
  for (int sy = 0; sy < 16; sy++) {
    for (int sx = 0; sx < 16; sx++) {
      for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
          ptr[x + (sx * 8) + (y * 128) + (sy * 1024)] =
              rom_.GetRawData()[0x0C4000 + pos];
          pos++;
        }
      }
    }
  }

  // ColorPalette cp = overworldMapBitmap.Palette;
  // for (int i = 0; i < 256; i += 2)
  // {
  //     //55B27 = US LW
  //     //55C27 = US DW
  //     cp.Entries[i / 2] = getColor((short)((ROM.DATA[0x55B27 + i + 1] << 8) +
  //     ROM.DATA[0x55B27 + i]));

  //     int k = 0;
  //     int j = 0;
  //     for (int y = 10; y < 14; y++)
  //     {
  //         for (int x = 0; x < 15; x++)
  //         {
  //             cp.Entries[145 + k] = Palettes.globalSprite_Palettes[0][j];
  //             k++;
  //             j++;
  //         }
  //         k++;
  //     }
  // }

  // overworldMapBitmap.Palette = cp;
  // owactualMapBitmap.Palette = cp;
}

}  // namespace Data
}  // namespace Application
}  // namespace yaze