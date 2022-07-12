#include "overworld.h"

#include "app/gfx/snes_tile.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace zelda3 {

using namespace core;
using namespace gfx;

void Overworld::Load(ROM& rom) {
  rom_ = rom;

  overworldMapPointer = std::make_shared<uchar[]>(0x40000);
  mapblockset16 = std::make_shared<uchar[]>(1048576);
  currentOWgfx16Ptr = std::make_shared<uchar[]>((128 * 512) / 2);

  AssembleMap32Tiles();
  AssembleMap16Tiles();
  DecompressAllMapTiles();

  // Map Initialization
  for (int i = 0; i < constants::NumberOfOWMaps; i++) {
    overworld_maps_.emplace_back(rom_, tiles16, i);
  }
  FetchLargeMaps();
  LoadOverworldMap();

  auto size = tiles16.size();
  for (int i = 0; i < 160; i++) {
    overworld_maps_[i].BuildMap(mapParent, size, gameState, allmapsTilesLW,
                                allmapsTilesDW, allmapsTilesSP,
                                currentOWgfx16Ptr.get(), mapblockset16.get());
  }

  isLoaded = true;
}

ushort Overworld::GenerateTile32(int i, int k, int dimension) {
  return (ushort)(rom_.data()[map32address[dimension] + k + (i)] +
                  (((rom_.data()[map32address[dimension] + (i) +
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

  allmapsTilesLW.resize(tiles32.size());
  allmapsTilesDW.resize(tiles32.size());
  allmapsTilesSP.resize(tiles32.size());
  for (int i = 0; i < tiles32.size(); i++) {
    allmapsTilesLW[i].resize(tiles32.size());
    allmapsTilesDW[i].resize(tiles32.size());
    allmapsTilesSP[i].resize(tiles32.size());
  }
}

void Overworld::AssembleMap16Tiles() {
  int tpos = core::constants::map16Tiles;
  auto rom_data = rom_.data();
  for (int i = 0; i < 4096; i += 1)  // 3760
  {
    TileInfo t0 = GetTilesInfo((uintptr_t)(rom_data + tpos));
    tpos += 2;
    TileInfo t1 = GetTilesInfo((uintptr_t)(rom_data + tpos));
    tpos += 2;
    TileInfo t2 = GetTilesInfo((uintptr_t)(rom_data + tpos));
    tpos += 2;
    TileInfo t3 = GetTilesInfo((uintptr_t)(rom_data + tpos));
    tpos += 2;
    tiles16.emplace_back(t0, t1, t2, t3);
  }
}

void Overworld::DecompressAllMapTiles() {
  int lowest = 0x0FFFFF;
  int highest = 0x0F8000;
  int sx = 0;
  int sy = 0;
  int c = 0;
  for (int i = 0; i < 160; i++) {
    int p1 =
        (rom_.data()[(constants::compressedAllMap32PointersHigh) + 2 + (3 * i)]
         << 16) +
        (rom_.data()[(constants::compressedAllMap32PointersHigh) + 1 + (3 * i)]
         << 8) +
        (rom_.data()[(constants::compressedAllMap32PointersHigh + (3 * i))]);

    auto tmp2 = std::make_unique<char[]>(256);
    auto tmp = tmp2.get();
    p1 = lorom_snes_to_pc(p1, &tmp);
    int p2 =
        (rom_.data()[(constants::compressedAllMap32PointersLow) + 2 + (3 * i)]
         << 16) +
        (rom_.data()[(constants::compressedAllMap32PointersLow) + 1 + (3 * i)]
         << 8) +
        (rom_.data()[(constants::compressedAllMap32PointersLow + (3 * i))]);
    p2 = lorom_snes_to_pc(p2, &tmp);

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

    if (p1 <= lowest && p1 > 0x0F8000) {
      lowest = p1;
    }
    if (p2 <= lowest && p2 > 0x0F8000) {
      lowest = p2;
    }

    auto bytes = alttp_decompress_overworld(
        (char*)rom_.data(), p2, 1000, &compressedSize1, &compressedLength1);
    auto bytes2 = alttp_decompress_overworld(
        (char*)rom_.data(), p1, 1000, &compressedSize2, &compressedLength2);

    for (int y = 0; y < 16; y++) {
      for (int x = 0; x < 16; x++) {
        auto tidD = (ushort)((bytes2[ttpos] << 8) + bytes[ttpos]);

        int tpos = tidD;
        if (tpos < tiles32.size()) {
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

void Overworld::FetchLargeMaps() {
  for (int i = 128; i < 145; i++) {
    mapParent[i] = 0;
  }

  mapParent[128] = 128;
  mapParent[129] = 129;
  mapParent[130] = 129;
  mapParent[137] = 129;
  mapParent[138] = 129;
  mapParent[136] = 136;
  overworld_maps_[136].large_map_ = false;

  bool mapChecked[64];
  for (auto& each : mapChecked) {
    each = false;
  }
  int xx = 0;
  int yy = 0;
  while (true) {
    int i = xx + (yy * 8);
    if (mapChecked[i] == false) {
      if (overworld_maps_[i].large_map_ == true) {
        mapChecked[i] = true;
        mapParent[i] = (uchar)i;
        mapParent[i + 64] = (uchar)(i + 64);

        mapChecked[i + 1] = true;
        mapParent[i + 1] = (uchar)i;
        mapParent[i + 65] = (uchar)(i + 64);

        mapChecked[i + 8] = true;
        mapParent[i + 8] = (uchar)i;
        mapParent[i + 72] = (uchar)(i + 64);

        mapChecked[i + 9] = true;
        mapParent[i + 9] = (uchar)i;
        mapParent[i + 73] = (uchar)(i + 64);
        xx++;
      } else {
        mapParent[i] = (uchar)i;
        mapParent[i + 64] = (uchar)(i + 64);
        mapChecked[i] = true;
      }
    }

    xx++;
    if (xx >= 8) {
      xx = 0;
      yy += 1;
      if (yy >= 8) {
        break;
      }
    }
  }
}

void Overworld::LoadOverworldMap() {
  overworldMapBitmap.Create(128, 128, 8, overworldMapPointer.get());

  auto ptr = overworldMapPointer;

  int pos = 0;
  for (int sy = 0; sy < 16; sy++) {
    for (int sx = 0; sx < 16; sx++) {
      for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
          ptr[x + (sx * 8) + (y * 128) + (sy * 1024)] =
              rom_.data()[0x0C4000 + pos];
          pos++;
        }
      }
    }
  }

  auto renderer = rom_.Renderer();
  overworldMapBitmap.CreateTexture(renderer);
}

}  // namespace zelda3
}  // namespace app
}  // namespace yaze