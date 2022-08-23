#include "overworld.h"

#include "app/gfx/snes_tile.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace zelda3 {

namespace {

uint GetOwMapGfxHighPtr(const uchar *rom, int index) {
  int map_high_ptr = core::compressedAllMap32PointersHigh;
  int p1 = (rom[(map_high_ptr) + 2 + (3 * index)] << 16) +
           (rom[(map_high_ptr) + 1 + (3 * index)] << 8) +
           (rom[(map_high_ptr + (3 * index))]);
  return core::SnesToPc(p1);
}

uint GetOwMapGfxLowPtr(const uchar *rom, int index) {
  int map_low_ptr = core::compressedAllMap32PointersLow;
  int p2 = (rom[(map_low_ptr) + 2 + (3 * index)] << 16) +
           (rom[(map_low_ptr) + 1 + (3 * index)] << 8) +
           (rom[(map_low_ptr + (3 * index))]);
  return core::SnesToPc(p2);
}

}  // namespace

absl::Status Overworld::Load(ROM &rom, uchar *ow_blockset) {
  rom_ = rom;

  AssembleMap32Tiles();
  AssembleMap16Tiles();
  RETURN_IF_ERROR(DecompressAllMapTiles())

  for (int map_index = 0; map_index < core::kNumOverworldMaps; ++map_index)
    overworld_maps_.emplace_back(map_index, rom_, tiles16);

  FetchLargeMaps();

  auto size = tiles16.size();
  for (int i = 0; i < core::kNumOverworldMaps; ++i) {
    if (i < 64) {
      RETURN_IF_ERROR(overworld_maps_[i].BuildMap(
          size, game_state_, 0, map_parent_, map_tiles_.light_world))
    } else if (i < 0x80 && i >= 0x40) {
      RETURN_IF_ERROR(overworld_maps_[i].BuildMap(
          size, game_state_, 1, map_parent_, map_tiles_.dark_world))
    } else {
      RETURN_IF_ERROR(overworld_maps_[i].BuildMap(
          size, game_state_, 2, map_parent_, map_tiles_.special_world))
    }
  }

  is_loaded_ = true;
  return absl::OkStatus();
}

ushort Overworld::GenerateTile32(int i, int k, int dimension) {
  return (ushort)(rom_[map32address[dimension] + k + (i)] +
                  (((rom_[map32address[dimension] + (i) + (k <= 1 ? 4 : 5)] >>
                     (k % 2 == 0 ? 4 : 0)) &
                    0x0F) *
                   256));
}

void Overworld::AssembleMap32Tiles() {
  for (int i = 0; i < 0x33F0; i += 6) {
    ushort tl;
    ushort tr;
    ushort bl;
    ushort br;
    for (int k = 0; k < 4; k++) {
      tl = GenerateTile32(i, k, (int)Dimension::map32TilesTL);
      tr = GenerateTile32(i, k, (int)Dimension::map32TilesTR);
      bl = GenerateTile32(i, k, (int)Dimension::map32TilesBL);
      br = GenerateTile32(i, k, (int)Dimension::map32TilesBR);
      tiles32.push_back(gfx::Tile32(tl, tr, bl, br));
    }
  }
  map_tiles_.light_world.resize(kTile32Num);
  map_tiles_.dark_world.resize(kTile32Num);
  map_tiles_.special_world.resize(kTile32Num);
  for (int i = 0; i < kTile32Num; i++) {
    map_tiles_.light_world[i].resize(kTile32Num);
    map_tiles_.dark_world[i].resize(kTile32Num);
    map_tiles_.special_world[i].resize(kTile32Num);
  }
}

void Overworld::AssembleMap16Tiles() {
  int tpos = core::map16Tiles;
  for (int i = 0; i < 4096; i += 1) {
    auto t0 = gfx::GetTilesInfo((uintptr_t)(rom_ + tpos));
    tpos += 2;
    auto t1 = gfx::GetTilesInfo((uintptr_t)(rom_ + tpos));
    tpos += 2;
    auto t2 = gfx::GetTilesInfo((uintptr_t)(rom_ + tpos));
    tpos += 2;
    auto t3 = gfx::GetTilesInfo((uintptr_t)(rom_ + tpos));
    tpos += 2;
    tiles16.emplace_back(t0, t1, t2, t3);
  }
}

void Overworld::AssignWorldTiles(int x, int y, int sx, int sy, int tpos,
                                 OWBlockset &world) {
  int position_x1 = (x * 2) + (sx * 32);
  int position_y1 = (y * 2) + (sy * 32);
  int position_x2 = (x * 2) + 1 + (sx * 32);
  int position_y2 = (y * 2) + 1 + (sy * 32);
  world[position_x1][position_y1] = tiles32[tpos].tile0_;
  world[position_x2][position_y1] = tiles32[tpos].tile1_;
  world[position_x1][position_y2] = tiles32[tpos].tile2_;
  world[position_x2][position_y2] = tiles32[tpos].tile3_;
}

void Overworld::OrganizeMapTiles(Bytes &bytes, Bytes &bytes2, OWBlockset &world,
                                 int i, int sx, int sy, int &ttpos) {
  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 16; x++) {
      auto tidD = (ushort)((bytes2[ttpos] << 8) + bytes[ttpos]);
      int tpos = tidD;
      if (tpos < tiles32.size()) {
        if (i < 64) {
          AssignWorldTiles(x, y, sx, sy, tpos, map_tiles_.light_world);
        } else if (i < 128 && i >= 64) {
          AssignWorldTiles(x, y, sx, sy, tpos, map_tiles_.dark_world);
        } else {
          AssignWorldTiles(x, y, sx, sy, tpos, map_tiles_.special_world);
        }
      }
      ttpos += 1;
    }
  }
}

absl::Status Overworld::DecompressAllMapTiles() {
  int lowest = 0x0FFFFF;
  int highest = 0x0F8000;
  int sx = 0;
  int sy = 0;
  int c = 0;
  for (int i = 0; i < 160; i++) {
    auto p1 = GetOwMapGfxHighPtr(rom_.data(), i);
    auto p2 = GetOwMapGfxLowPtr(rom_.data(), i);
    int ttpos = 0;

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

    ASSIGN_OR_RETURN(auto bytes, rom_.DecompressOverworld(p2, 1000))
    ASSIGN_OR_RETURN(auto bytes2, rom_.DecompressOverworld(p1, 1000))
    OrganizeMapTiles(bytes, bytes2, map_tiles_.light_world, i, sx, sy, ttpos);

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
  return absl::OkStatus();
}

void Overworld::FetchLargeMaps() {
  for (int i = 128; i < 145; i++) {
    map_parent_[i] = 0;
  }

  map_parent_[128] = 128;
  map_parent_[129] = 129;
  map_parent_[130] = 129;
  map_parent_[137] = 129;
  map_parent_[138] = 129;
  map_parent_[136] = 136;
  overworld_maps_[136].SetLargeMap(false);

  bool mapChecked[64];
  for (auto &each : mapChecked) {
    each = false;
  }
  int xx = 0;
  int yy = 0;
  while (true) {
    int i = xx + (yy * 8);
    if (mapChecked[i] == false) {
      if (overworld_maps_[i].IsLargeMap() == true) {
        mapChecked[i] = true;
        map_parent_[i] = (uchar)i;
        map_parent_[i + 64] = (uchar)(i + 64);

        mapChecked[i + 1] = true;
        map_parent_[i + 1] = (uchar)i;
        map_parent_[i + 65] = (uchar)(i + 64);

        mapChecked[i + 8] = true;
        map_parent_[i + 8] = (uchar)i;
        map_parent_[i + 72] = (uchar)(i + 64);

        mapChecked[i + 9] = true;
        map_parent_[i + 9] = (uchar)i;
        map_parent_[i + 73] = (uchar)(i + 64);
        xx++;
      } else {
        map_parent_[i] = (uchar)i;
        map_parent_[i + 64] = (uchar)(i + 64);
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

}  // namespace zelda3
}  // namespace app
}  // namespace yaze