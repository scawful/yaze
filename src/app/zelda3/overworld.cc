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

absl::Status Overworld::Load(ROM &rom) {
  rom_ = rom;

  AssembleMap32Tiles();
  AssembleMap16Tiles();
  RETURN_IF_ERROR(DecompressAllMapTiles())

  for (int map_index = 0; map_index < core::kNumOverworldMaps; ++map_index)
    overworld_maps_.emplace_back(map_index, rom_, tiles16);

  FetchLargeMaps();
  LoadEntrances();
  LoadSprites();

  auto size = tiles16.size();
  std::vector<std::future<absl::Status>> futures;
  for (int i = 0; i < core::kNumOverworldMaps; ++i) {
    futures.push_back(std::async(std::launch::async, [this, i, size]() {
      if (i < 64) {
        return overworld_maps_[i].BuildMap(
            size, game_state_, 0, map_parent_, map_tiles_.light_world);
      } else if (i < 0x80 && i >= 0x40) {
        return overworld_maps_[i].BuildMap(
            size, game_state_, 1, map_parent_, map_tiles_.dark_world);
      } else {
        return overworld_maps_[i].BuildMap(
            size, game_state_, 2, map_parent_, map_tiles_.special_world);
      }
    }));
  }

  // Wait for all tasks to complete and check their results
  for (auto& future : futures) {
    absl::Status status = future.get();
    if (!status.ok()) {
      return status;
    }
  }

  is_loaded_ = true;
  return absl::OkStatus();
}

// ----------------------------------------------------------------------------

absl::Status Overworld::SaveOverworldMaps() {
  for (int i = 0; i < 0x40; i++) {
    int yPos = i / 8;
    int xPos = i % 8;
    int parentyPos = overworld_maps_[i].Parent() / 8;
    int parentxPos = overworld_maps_[i].Parent() % 8;

    // Always write the map parent since it should not matter
    rom_.Write(core::overworldMapParentId + i, overworld_maps_[i].Parent());

    // If it's large then save parent pos *
    // 0x200 otherwise pos * 0x200
    if (overworld_maps_[i].IsLargeMap()) {
      // Check 1
      rom_.Write(core::overworldMapSize + i, 0x20);
      rom_.Write(core::overworldMapSize + i + 1, 0x20);
      rom_.Write(core::overworldMapSize + i + 8, 0x20);
      rom_.Write(core::overworldMapSize + i + 9, 0x20);

      // Check 2
      rom_.Write(core::overworldMapSizeHighByte + i, 0x03);
      rom_.Write(core::overworldMapSizeHighByte + i + 1, 0x03);
      rom_.Write(core::overworldMapSizeHighByte + i + 8, 0x03);
      rom_.Write(core::overworldMapSizeHighByte + i + 9, 0x03);

      // Check 3
      rom_.Write(core::overworldScreenSize + i, 0x00);
      rom_.Write(core::overworldScreenSize + i + 64, 0x00);

      rom_.Write(core::overworldScreenSize + i + 1, 0x00);
      rom_.Write(core::overworldScreenSize + i + 1 + 64, 0x00);

      rom_.Write(core::overworldScreenSize + i + 8, 0x00);
      rom_.Write(core::overworldScreenSize + i + 8 + 64, 0x00);

      rom_.Write(core::overworldScreenSize + i + 9, 0x00);
      rom_.Write(core::overworldScreenSize + i + 9 + 64, 0x00);

      // Check 4
      rom_.Write(core::OverworldScreenSizeForLoading + i, 0x04);
      rom_.Write(core::OverworldScreenSizeForLoading + i + 64, 0x04);
      rom_.Write(core::OverworldScreenSizeForLoading + i + 128, 0x04);

      rom_.Write(core::OverworldScreenSizeForLoading + i + 1, 0x04);
      rom_.Write(core::OverworldScreenSizeForLoading + i + 1 + 64, 0x04);
      rom_.Write(core::OverworldScreenSizeForLoading + i + 1 + 128, 0x04);

      rom_.Write(core::OverworldScreenSizeForLoading + i + 8, 0x04);
      rom_.Write(core::OverworldScreenSizeForLoading + i + 8 + 64, 0x04);
      rom_.Write(core::OverworldScreenSizeForLoading + i + 8 + 128, 0x04);

      rom_.Write(core::OverworldScreenSizeForLoading + i + 9, 0x04);
      rom_.Write(core::OverworldScreenSizeForLoading + i + 9 + 64, 0x04);
      rom_.Write(core::OverworldScreenSizeForLoading + i + 9 + 128, 0x04);

      // Check 5 and 6
      rom_.WriteShort(
          core::transition_target_north + (i * 2) + 2,
          (short)((parentyPos * 0x200) -
                  0xE0));  // (short) is placed to reduce the int to 2 bytes.
      rom_.WriteShort(core::transition_target_west + (i * 2) + 2,
                      (short)((parentxPos * 0x200) - 0x100));

      rom_.WriteShort(
          core::transition_target_north + (i * 2) + 16,
          (short)((parentyPos * 0x200) -
                  0xE0));  // (short) is placed to reduce the int to 2 bytes.
      rom_.WriteShort(core::transition_target_west + (i * 2) + 16,
                      (short)((parentxPos * 0x200) - 0x100));

      rom_.WriteShort(
          core::transition_target_north + (i * 2) + 18,
          (short)((parentyPos * 0x200) -
                  0xE0));  // (short) is placed to reduce the int to 2 bytes.
      rom_.WriteShort(core::transition_target_west + (i * 2) + 18,
                      (short)((parentxPos * 0x200) - 0x100));

      // Check 7 and 8
      rom_.WriteShort(core::overworldTransitionPositionX + (i * 2),
                      (parentxPos * 0x200));
      rom_.WriteShort(core::overworldTransitionPositionY + (i * 2),
                      (parentyPos * 0x200));

      rom_.WriteShort(core::overworldTransitionPositionX + (i * 2) + 2,
                      (parentxPos * 0x200));
      rom_.WriteShort(core::overworldTransitionPositionY + (i * 2) + 2,
                      (parentyPos * 0x200));

      rom_.WriteShort(core::overworldTransitionPositionX + (i * 2) + 16,
                      (parentxPos * 0x200));
      rom_.WriteShort(core::overworldTransitionPositionY + (i * 2) + 16,
                      (parentyPos * 0x200));

      rom_.WriteShort(core::overworldTransitionPositionX + (i * 2) + 18,
                      (parentxPos * 0x200));
      rom_.WriteShort(core::overworldTransitionPositionY + (i * 2) + 18,
                      (parentyPos * 0x200));

      // Check 9
      rom_.WriteShort(core::OverworldScreenTileMapChangeByScreen + (i * 2),
                      0x0060);  // Always 0x0060
      rom_.WriteShort(core::OverworldScreenTileMapChangeByScreen + (i * 2) + 2,
                      0x0060);  // Always 0x0060

      // If parentX == 0 then lower submaps == 0x0060 too
      if (parentxPos == 0) {
        rom_.WriteShort(
            core::OverworldScreenTileMapChangeByScreen + (i * 2) + 16, 0x0060);
        rom_.WriteShort(
            core::OverworldScreenTileMapChangeByScreen + (i * 2) + 18, 0x0060);
      } else {
        // Otherwise lower submaps == 0x1060
        rom_.WriteShort(
            core::OverworldScreenTileMapChangeByScreen + (i * 2) + 16, 0x1060);
        rom_.WriteShort(
            core::OverworldScreenTileMapChangeByScreen + (i * 2) + 18, 0x1060);
      }

      rom_.WriteShort(
          core::OverworldScreenTileMapChangeByScreen + (i * 2) + 128,
          0x0080);  // Always 0x0080
      rom_.WriteShort(
          core::OverworldScreenTileMapChangeByScreen + (i * 2) + 2 + 128,
          0x0080);  // Always 0x0080
                    // Lower are always 8010
      rom_.WriteShort(
          core::OverworldScreenTileMapChangeByScreen + (i * 2) + 16 + 128,
          0x1080);  // Always 0x1080
      rom_.WriteShort(
          core::OverworldScreenTileMapChangeByScreen + (i * 2) + 18 + 128,
          0x1080);  // Always 0x1080

      rom_.WriteShort(
          core::OverworldScreenTileMapChangeByScreen + (i * 2) + 256,
          0x1800);  // Always 0x1800
      rom_.WriteShort(
          core::OverworldScreenTileMapChangeByScreen + (i * 2) + 16 + 256,
          0x1800);  // Always 0x1800
                    // Right side is always 1840
      rom_.WriteShort(
          core::OverworldScreenTileMapChangeByScreen + (i * 2) + 2 + 256,
          0x1840);  // Always 0x1840
      rom_.WriteShort(
          core::OverworldScreenTileMapChangeByScreen + (i * 2) + 18 + 256,
          0x1840);  // Always 0x1840

      rom_.WriteShort(
          core::OverworldScreenTileMapChangeByScreen + (i * 2) + 384,
          0x2000);  // Always 0x2000
      rom_.WriteShort(
          core::OverworldScreenTileMapChangeByScreen + (i * 2) + 16 + 384,
          0x2000);  // Always 0x2000
                    // Right side is always 0x2040
      rom_.WriteShort(
          core::OverworldScreenTileMapChangeByScreen + (i * 2) + 2 + 384,
          0x2040);  // Always 0x2000
      rom_.WriteShort(
          core::OverworldScreenTileMapChangeByScreen + (i * 2) + 18 + 384,
          0x2040);  // Always 0x2000

      // checkedMap.Add((uchar)i);
      // checkedMap.Add((uchar)(i + 1));
      // checkedMap.Add((uchar)(i + 8));
      // checkedMap.Add((uchar)(i + 9));

    } else {
      rom_.Write(core::overworldMapSize + i, 0x00);
      rom_.Write(core::overworldMapSizeHighByte + i, 0x01);

      rom_.Write(core::overworldScreenSize + i, 0x01);
      rom_.Write(core::overworldScreenSize + i + 64, 0x01);

      rom_.Write(core::OverworldScreenSizeForLoading + i, 0x02);
      rom_.Write(core::OverworldScreenSizeForLoading + i + 64, 0x02);
      rom_.Write(core::OverworldScreenSizeForLoading + i + 128, 0x02);

      rom_.WriteShort(core::OverworldScreenTileMapChangeByScreen + (i * 2),
                      0x0060);
      rom_.WriteShort(
          core::OverworldScreenTileMapChangeByScreen + (i * 2) + 128, 0x0040);
      rom_.WriteShort(
          core::OverworldScreenTileMapChangeByScreen + (i * 2) + 256, 0x1800);
      rom_.WriteShort(
          core::OverworldScreenTileMapChangeByScreen + (i * 2) + 384, 0x1000);

      rom_.WriteShort(core::transition_target_north + (i * 2),
                      (short)((yPos * 0x200) - 0xE0));
      rom_.WriteShort(core::transition_target_west + (i * 2),
                      (short)((xPos * 0x200) - 0x100));

      rom_.WriteShort(core::overworldTransitionPositionX + (i * 2),
                      (xPos * 0x200));
      rom_.WriteShort(core::overworldTransitionPositionY + (i * 2),
                      (yPos * 0x200));

      // checkedMap.Add((uchar)i);
    }
  }
  return absl::OkStatus();
}

// ----------------------------------------------------------------------------

/**
void Overworld::SaveMap16Tiles() {
  int tpos = core::map16Tiles;
  // 3760
  for (int i = 0; i < core::NumberOfMap16; i += 1) {
    rom_.WriteShort(tpos, tiles16[i].tile0_);
    tpos += 2;
    rom_.WriteShort(tpos, tiles16[i].tile1_);
    tpos += 2;
    rom_.WriteShort(tpos, tiles16[i].tile2_);
    tpos += 2;
    rom_.WriteShort(tpos, tiles16[i].tile3_);
    tpos += 2;
  }
}
*/

// ----------------------------------------------------------------------------

/**
void Overworld::SaveMap32Tiles() {
  int index = 0;
  int c = tiles32_unique_.size();
  for (int i = 0; i < c; i += 6) {
    if (index >= 0x4540)  // 3C87??
    {
      std::cout << "Too many unique tiles!" << std::endl;

      break;
    }

    // Top Left
    rom_.Write(core::map32TilesTL + (i),
               (uchar)(tiles32_unique_[index].tile0 & 0xFF));
    rom_.Write(core::map32TilesTL + (i + 1),
               (uchar)(tiles32_unique_[index + 1].tile0 & 0xFF));
    rom_.Write(core::map32TilesTL + (i + 2),
               (uchar)(tiles32_unique_[index + 2].tile0 & 0xFF));
    rom_.Write(core::map32TilesTL + (i + 3),
               (uchar)(tiles32_unique_[index + 3].tile0 & 0xFF));

    rom_.Write(core::map32TilesTL + (i + 4),
               (uchar)(((tiles32_unique_[index].tile0 >> 4) & 0xF0) +
                       ((tiles32_unique_[index + 1].tile0 >> 8) & 0x0F)));
    rom_.Write(core::map32TilesTL + (i + 5),
               (uchar)(((tiles32_unique_[index + 2].tile0 >> 4) & 0xF0) +
                       ((tiles32_unique_[index + 3].tile0 >> 8) & 0x0F)));

    // Top Right
    rom_.Write(core::map32TilesTR + (i),
               (uchar)(tiles32_unique_[index].tile1 & 0xFF));
    rom_.Write(core::map32TilesTR + (i + 1),
               (uchar)(tiles32_unique_[index + 1].tile1 & 0xFF));
    rom_.Write(core::map32TilesTR + (i + 2),
               (uchar)(tiles32_unique_[index + 2].tile1 & 0xFF));
    rom_.Write(core::map32TilesTR + (i + 3),
               (uchar)(tiles32_unique_[index + 3].tile1 & 0xFF));

    rom_.Write(core::map32TilesTR + (i + 4),
               (uchar)(((tiles32_unique_[index].tile1 >> 4) & 0xF0) |
                       ((tiles32_unique_[index + 1].tile1 >> 8) & 0x0F)));
    rom_.Write(core::map32TilesTR + (i + 5),
               (uchar)(((tiles32_unique_[index + 2].tile1 >> 4) & 0xF0) |
                       ((tiles32_unique_[index + 3].tile1 >> 8) & 0x0F)));

    // Bottom Left
    rom_.Write(core::map32TilesBL + (i),
               (uchar)(tiles32_unique_[index].tile2 & 0xFF));
    rom_.Write(core::map32TilesBL + (i + 1),
               (uchar)(tiles32_unique_[index + 1].tile2 & 0xFF));
    rom_.Write(core::map32TilesBL + (i + 2),
               (uchar)(tiles32_unique_[index + 2].tile2 & 0xFF));
    rom_.Write(core::map32TilesBL + (i + 3),
               (uchar)(tiles32_unique_[index + 3].tile2 & 0xFF));

    rom_.Write(core::map32TilesBL + (i + 4),
               (uchar)(((tiles32_unique_[index].tile2 >> 4) & 0xF0) |
                       ((tiles32_unique_[index + 1].tile2 >> 8) & 0x0F)));
    rom_.Write(core::map32TilesBL + (i + 5),
               (uchar)(((tiles32_unique_[index + 2].tile2 >> 4) & 0xF0) |
                       ((tiles32_unique_[index + 3].tile2 >> 8) & 0x0F)));

    // Bottom Right
    rom_.Write(core::map32TilesBR + (i),
               (uchar)(tiles32_unique_[index].tile3 & 0xFF));
    rom_.Write(core::map32TilesBR + (i + 1),
               (uchar)(tiles32_unique_[index + 1].tile3 & 0xFF));
    rom_.Write(core::map32TilesBR + (i + 2),
               (uchar)(tiles32_unique_[index + 2].tile3 & 0xFF));
    rom_.Write(core::map32TilesBR + (i + 3),
               (uchar)(tiles32_unique_[index + 3].tile3 & 0xFF));

    rom_.Write(core::map32TilesBR + (i + 4),
               (uchar)(((tiles32_unique_[index].tile3 >> 4) & 0xF0) |
                       ((tiles32_unique_[index + 1].tile3 >> 8) & 0x0F)));
    rom_.Write(core::map32TilesBR + (i + 5),
               (uchar)(((tiles32_unique_[index + 2].tile3 >> 4) & 0xF0) |
                       ((tiles32_unique_[index + 3].tile3 >> 8) & 0x0F)));

    index += 4;
    c += 2;
  }
}
*/

// ----------------------------------------------------------------------------

ushort Overworld::GenerateTile32(int i, int k, int dimension) {
  return (ushort)(rom_[map32address[dimension] + k + (i)] +
                  (((rom_[map32address[dimension] + (i) + (k <= 1 ? 4 : 5)] >>
                     (k % 2 == 0 ? 4 : 0)) &
                    0x0F) *
                   256));
}

// ----------------------------------------------------------------------------

void Overworld::AssembleMap32Tiles() {
  for (int i = 0; i < 0x33F0; i += 6) {
    for (int k = 0; k < 4; k++) {
      tiles32.push_back(gfx::Tile32(
          /*top-left=*/GenerateTile32(i, k, (int)Dimension::map32TilesTL),
          /*top-right=*/GenerateTile32(i, k, (int)Dimension::map32TilesTR),
          /*bottom-left=*/GenerateTile32(i, k, (int)Dimension::map32TilesBL),
          /*bottom-right=*/GenerateTile32(i, k, (int)Dimension::map32TilesBR)));
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

// ----------------------------------------------------------------------------

void Overworld::AssembleMap16Tiles() {
  int tpos = core::map16Tiles;
  for (int i = 0; i < 4096; i += 1) {
    auto t0 = gfx::GetTilesInfo((rom_.toint16(tpos)));
    tpos += 2;
    auto t1 = gfx::GetTilesInfo((rom_.toint16(tpos)));
    tpos += 2;
    auto t2 = gfx::GetTilesInfo((rom_.toint16(tpos)));
    tpos += 2;
    auto t3 = gfx::GetTilesInfo((rom_.toint16(tpos)));
    tpos += 2;
    tiles16.emplace_back(t0, t1, t2, t3);
  }
}

// ----------------------------------------------------------------------------

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

// ----------------------------------------------------------------------------

void Overworld::OrganizeMapTiles(Bytes &bytes, Bytes &bytes2, int i, int sx,
                                 int sy, int &ttpos) {
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

// ----------------------------------------------------------------------------

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
    OrganizeMapTiles(bytes, bytes2, i, sx, sy, ttpos);

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

// ----------------------------------------------------------------------------

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

  std::vector<bool> mapChecked;
  mapChecked.reserve(0x40);
  for (int i = 0; i < 64; i++) {
    mapChecked[i] = false;
  }
  int xx = 0;
  int yy = 0;
  while (true) {
    if (int i = xx + (yy * 8); mapChecked[i] == false) {
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

// ----------------------------------------------------------------------------

void Overworld::LoadEntrances() {
  for (int i = 0; i < 129; i++) {
    short mapId = rom_.toint16(core::OWEntranceMap + (i * 2));
    ushort mapPos = rom_.toint16(core::OWEntrancePos + (i * 2));
    uchar entranceId = (rom_[core::OWEntranceEntranceId + i]);
    int p = mapPos >> 1;
    int x = (p % 64);
    int y = (p >> 6);
    bool deleted = false;
    if (mapPos == 0xFFFF) {
      deleted = true;
    }
    all_entrances_.emplace_back(
        (x * 16) + (((mapId % 64) - (((mapId % 64) / 8) * 8)) * 512),
        (y * 16) + (((mapId % 64) / 8) * 512), entranceId, mapId, mapPos,
        deleted);
  }

  for (int i = 0; i < 0x13; i++) {
    auto mapId = (short)((rom_[core::OWHoleArea + (i * 2) + 1] << 8) +
                         (rom_[core::OWHoleArea + (i * 2)]));
    auto mapPos = (short)((rom_[core::OWHolePos + (i * 2) + 1] << 8) +
                          (rom_[core::OWHolePos + (i * 2)]));
    uchar entranceId = (rom_[core::OWHoleEntrance + i]);
    int p = (mapPos + 0x400) >> 1;
    int x = (p % 64);
    int y = (p >> 6);
    all_holes_.emplace_back(
        (x * 16) + (((mapId % 64) - (((mapId % 64) / 8) * 8)) * 512),
        (y * 16) + (((mapId % 64) / 8) * 512), entranceId, mapId,
        (ushort)(mapPos + 0x400), true);
  }
}

// ----------------------------------------------------------------------------

void Overworld::LoadSprites() {
  // LW[0] = RainState 0 to 63 there's no data for DW
  // LW[1] = ZeldaState 0 to 128 ; Contains LW and DW <128 or 144 wtf
  // LW[2] = AgahState 0 to ?? ;Contains data for LW and DW

  for (int i = 0; i < 3; i++) {
    all_sprites_.emplace_back(std::vector<Sprite>());
  }

  for (int i = 0; i < 64; i++) {
    if (map_parent_[i] == i) {
      // Beginning Sprites
      int ptrPos = core::overworldSpritesBegining + (i * 2);
      int spriteAddress = core::SnesToPc((0x09 << 0x10) + rom_.toint16(ptrPos));
      while (true) {
        uchar b1 = rom_[spriteAddress];
        uchar b2 = rom_[spriteAddress + 1];
        uchar b3 = rom_[spriteAddress + 2];
        if (b1 == 0xFF) {
          break;
        }

        int mapY = (i / 8);
        int mapX = (i % 8);

        int realX = ((b2 & 0x3F) * 16) + mapX * 512;
        int realY = ((b1 & 0x3F) * 16) + mapY * 512;

        all_sprites_[0].emplace_back(overworld_maps_[i].AreaGraphics(),
                                     (uchar)i, b3, (uchar)(b2 & 0x3F),
                                     (uchar)(b1 & 0x3F), realX, realY);

        spriteAddress += 3;
      }
    }
  }

  for (int i = 0; i < 144; i++) {
    if (map_parent_[i] == i) {
      // Zelda Saved Sprites
      int ptrPos = core::overworldSpritesZelda + (i * 2);
      int spriteAddress = core::SnesToPc((0x09 << 0x10) + rom_.toint16(ptrPos));
      while (true) {
        uchar b1 = rom_[spriteAddress];
        uchar b2 = rom_[spriteAddress + 1];
        uchar b3 = rom_[spriteAddress + 2];
        if (b1 == 0xFF) {
          break;
        }

        int editorMapIndex = i;
        if (editorMapIndex >= 128) {
          editorMapIndex = i - 128;
        } else if (editorMapIndex >= 64) {
          editorMapIndex = i - 64;
        }

        int mapY = (editorMapIndex / 8);
        int mapX = (editorMapIndex % 8);

        int realX = ((b2 & 0x3F) * 16) + mapX * 512;
        int realY = ((b1 & 0x3F) * 16) + mapY * 512;

        all_sprites_[1].emplace_back(overworld_maps_[i].AreaGraphics(),
                                     (uchar)i, b3, (uchar)(b2 & 0x3F),
                                     (uchar)(b1 & 0x3F), realX, realY);

        spriteAddress += 3;
      }
    }

    // Agahnim Dead Sprites
    if (map_parent_[i] == i) {
      int ptrPos = core::overworldSpritesAgahnim + (i * 2);
      int spriteAddress = core::SnesToPc((0x09 << 0x10) + rom_.toint16(ptrPos));
      while (true) {
        uchar b1 = rom_[spriteAddress];
        uchar b2 = rom_[spriteAddress + 1];
        uchar b3 = rom_[spriteAddress + 2];
        if (b1 == 0xFF) {
          break;
        }

        int editorMapIndex = i;
        if (editorMapIndex >= 128) {
          editorMapIndex = i - 128;
        } else if (editorMapIndex >= 64) {
          editorMapIndex = i - 64;
        }

        int mapY = (editorMapIndex / 8);
        int mapX = (editorMapIndex % 8);

        int realX = ((b2 & 0x3F) * 16) + mapX * 512;
        int realY = ((b1 & 0x3F) * 16) + mapY * 512;

        all_sprites_[2].emplace_back(overworld_maps_[i].AreaGraphics(),
                                     (uchar)i, b3, (uchar)(b2 & 0x3F),
                                     (uchar)(b1 & 0x3F), realX, realY);

        spriteAddress += 3;
      }
    }
  }
}

}  // namespace zelda3
}  // namespace app
}  // namespace yaze