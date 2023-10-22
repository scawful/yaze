#include "overworld.h"

#include <SDL.h>

#include <fstream>
#include <future>
#include <memory>
#include <unordered_map>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "app/core/constants.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/compression.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"
#include "app/zelda3/overworld_map.h"
#include "app/zelda3/sprite/sprite.h"

namespace yaze {
namespace app {
namespace zelda3 {

namespace {

uint GetOwMapGfxHighPtr(const uchar *rom, int index, uint32_t map_high_ptr) {
  int p1 = (rom[map_high_ptr + 2 + (3 * index)] << 16) +
           (rom[map_high_ptr + 1 + (3 * index)] << 8) +
           (rom[map_high_ptr + (3 * index)]);
  return core::SnesToPc(p1);
}

uint GetOwMapGfxLowPtr(const uchar *rom, int index, uint32_t map_low_ptr) {
  int p2 = (rom[map_low_ptr + 2 + (3 * index)] << 16) +
           (rom[map_low_ptr + 1 + (3 * index)] << 8) +
           (rom[map_low_ptr + (3 * index)]);
  return core::SnesToPc(p2);
}

std::vector<uint64_t> GetAllTile16(OWBlockset &tiles_used) {
  std::vector<uint64_t> all_tile_16;  // Ensure it's 64 bits

  int sx = 0;
  int sy = 0;
  int c = 0;
  for (int i = 0; i < kNumOverworldMaps; i++) {
    for (int y = 0; y < 32; y += 2) {
      for (int x = 0; x < 32; x += 2) {
        gfx::Tile32 current_tile(
            tiles_used[x + (sx * 32)][y + (sy * 32)],
            tiles_used[x + 1 + (sx * 32)][y + (sy * 32)],
            tiles_used[x + (sx * 32)][y + 1 + (sy * 32)],
            tiles_used[x + 1 + (sx * 32)][y + 1 + (sy * 32)]);

        all_tile_16.push_back(current_tile.GetPackedValue());
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

  return all_tile_16;
}

absl::flat_hash_map<int, MapData> parseFile(const std::string &filename) {
  absl::flat_hash_map<int, MapData> resultMap;

  std::ifstream file(filename);
  if (!file.is_open()) {
    std::cerr << "Failed to open file: " << filename << std::endl;
    return resultMap;
  }

  std::string line;
  int currentKey;
  bool isHigh = true;

  while (getline(file, line)) {
    // Skip empty or whitespace-only lines
    if (line.find_first_not_of(" \t\r\n") == std::string::npos) {
      continue;
    }

    // If the line starts with "MAPDTH" or "MAPDTL", extract the ID.
    if (line.find("MAPDTH") == 0) {
      auto num_str = line.substr(6);  // Extract ID after "MAPDTH"
      currentKey = std::stoi(num_str);
      isHigh = true;
    } else if (line.find("MAPDTL") == 0) {
      auto num_str = line.substr(6);  // Extract ID after "MAPDTH"
      currentKey = std::stoi(num_str);
      isHigh = false;
    } else {
      // Check if the currentKey is already in the map. If not, initialize it.
      if (resultMap.find(currentKey) == resultMap.end()) {
        resultMap[currentKey] = MapData();
      }

      // Split the line by commas and convert to uint8_t.
      std::stringstream ss(line);
      std::string valueStr;
      while (getline(ss, valueStr, ',')) {
        uint8_t value = std::stoi(valueStr, nullptr, 16);
        if (isHigh) {
          resultMap[currentKey].highData.push_back(value);
        } else {
          resultMap[currentKey].lowData.push_back(value);
        }
      }
    }
  }

  return resultMap;
}

}  // namespace

absl::Status Overworld::Load(ROM &rom) {
  rom_ = rom;

  AssembleMap32Tiles();
  AssembleMap16Tiles();
  RETURN_IF_ERROR(DecompressAllMapTiles())

  for (int map_index = 0; map_index < kNumOverworldMaps; ++map_index)
    overworld_maps_.emplace_back(map_index, rom_, tiles16);

  FetchLargeMaps();
  LoadEntrances();

  auto size = tiles16.size();
  std::vector<std::future<absl::Status>> futures;
  for (int i = 0; i < kNumOverworldMaps; ++i) {
    futures.push_back(std::async(std::launch::async, [this, i, size]() {
      if (i < 64) {
        return overworld_maps_[i].BuildMap(size, game_state_, 0, map_parent_,
                                           map_tiles_.light_world);
      } else if (i < 0x80 && i >= 0x40) {
        return overworld_maps_[i].BuildMap(size, game_state_, 1, map_parent_,
                                           map_tiles_.dark_world);
      } else {
        return overworld_maps_[i].BuildMap(size, game_state_, 2, map_parent_,
                                           map_tiles_.special_world);
      }
    }));
  }

  // Wait for all tasks to complete and check their results
  for (auto &future : futures) {
    absl::Status status = future.get();
    if (!status.ok()) {
      return status;
    }
  }

  // LoadSprites();

  is_loaded_ = true;
  return absl::OkStatus();
}

absl::Status Overworld::SaveOverworldMaps() {
  // Initialize map pointers
  std::fill(map_pointers1_id.begin(), map_pointers1_id.end(), -1);
  std::fill(map_pointers1_id.begin(), map_pointers1_id.end(), -1);

  // Compress and save each map
  int pos = 0x058000;
  for (int i = 0; i < 160; i++) {
    std::vector<uint8_t> single_map_1(512);
    std::vector<uint8_t> single_map_2(512);

    // Copy tiles32 data to single_map_1 and single_map_2
    int npos = 0;
    for (int y = 0; y < 16; y++) {
      for (int x = 0; x < 16; x++) {
        auto packed1 = tiles32[npos + (i * 256)].GetPackedValue();
        auto packed2 = tiles32[npos + (i * 256) + 16].GetPackedValue();
        single_map_1[npos] = static_cast<uint8_t>(packed1 & 0xFF);
        single_map_2[npos] = static_cast<uint8_t>(packed2 & 0xFF);
        npos++;
      }
    }

    // Compress single_map_1 and single_map_2
    ASSIGN_OR_RETURN(
        auto a, gfx::lc_lz2::CompressOverworld(single_map_1.data(), 0, 256))
    ASSIGN_OR_RETURN(
        auto b, gfx::lc_lz2::CompressOverworld(single_map_2.data(), 0, 256))
    if (a.empty() || b.empty()) {
      return absl::AbortedError("Error compressing map gfx.");
    }

    // Save compressed data and pointers
    map_data_p1[i] = a;
    map_data_p2[i] = b;

    if (map_pointers1_id[i] == -1) {
      // Save compressed data and pointer for map1
      std::copy(a.begin(), a.end(), map_data_p1[i].begin());
      int snes_pos = core::PcToSnes(pos);
      map_pointers1[i] = snes_pos;

      RETURN_IF_ERROR(rom()->RunTransaction(
          WriteAction{kCompressedAllMap32PointersLow + 0 + 3 * i,
                      uint8_t(snes_pos & 0xFF)},
          WriteAction{kCompressedAllMap32PointersLow + 1 + 3 * i,
                      uint8_t((snes_pos >> 8) & 0xFF)},
          WriteAction{kCompressedAllMap32PointersLow + 2 + 3 * i,
                      uint8_t((snes_pos >> 16) & 0xFF)},
          WriteAction{pos, std::vector<uint8_t>(a)}))

      pos += a.size();
    } else {
      // Save pointer for map1
      int snes_pos = map_pointers1[map_pointers1_id[i]];
      RETURN_IF_ERROR(rom()->RunTransaction(
          WriteAction{kCompressedAllMap32PointersLow + 0 + 3 * i,
                      uint8_t(snes_pos & 0xFF)},
          WriteAction{kCompressedAllMap32PointersLow + 1 + 3 * i,
                      uint8_t((snes_pos >> 8) & 0xFF)},
          WriteAction{kCompressedAllMap32PointersLow + 2 + 3 * i,
                      uint8_t((snes_pos >> 16) & 0xFF)}))
    }

    if (map_pointers2_id[i] == -1) {
      // Save compressed data and pointer for map2
      std::copy(b.begin(), b.end(), map_data_p2[i].begin());
      int snes_pos = core::PcToSnes(pos);
      map_pointers2[i] = snes_pos;
      RETURN_IF_ERROR(rom()->RunTransaction(
          WriteAction{kCompressedAllMap32PointersHigh + 0 + 3 * i,
                      static_cast<uint8_t>(snes_pos & 0xFF)},
          WriteAction{kCompressedAllMap32PointersHigh + 1 + 3 * i,
                      static_cast<uint8_t>((snes_pos >> 8) & 0xFF)},
          WriteAction{kCompressedAllMap32PointersHigh + 2 + 3 * i,
                      static_cast<uint8_t>((snes_pos >> 16) & 0xFF)},
          WriteAction{pos, std::vector<uint8_t>(b)}))
      pos += b.size();
    } else {
      // Save pointer for map2
      int snes_pos = map_pointers2[map_pointers2_id[i]];
      RETURN_IF_ERROR(rom()->RunTransaction(
          WriteAction{kCompressedAllMap32PointersHigh + 0 + 3 * i,
                      static_cast<uint8_t>(snes_pos & 0xFF)},
          WriteAction{kCompressedAllMap32PointersHigh + 1 + 3 * i,
                      static_cast<uint8_t>((snes_pos >> 8) & 0xFF)},
          WriteAction{kCompressedAllMap32PointersHigh + 2 + 3 * i,
                      static_cast<uint8_t>((snes_pos >> 16) & 0xFF)}))
    }
  }

  // Check if too many maps data
  if (pos > 0x137FFF) {
    std::cerr << "Too many maps data " << std::hex << pos << std::endl;
    return absl::AbortedError("Too many maps data");
  }

  // Save large maps
  RETURN_IF_ERROR(SaveLargeMaps())

  return absl::OkStatus();
}

// ----------------------------------------------------------------------------

absl::Status Overworld::SaveLargeMaps() {
  for (int i = 0; i < 0x40; i++) {
    int yPos = i / 8;
    int xPos = i % 8;
    int parentyPos = overworld_maps_[i].Parent() / 8;
    int parentxPos = overworld_maps_[i].Parent() % 8;

    std::unordered_map<uint8_t, uint8_t> checkedMap;

    // Always write the map parent since it should not matter
    rom()->Write(overworldMapParentId + i, overworld_maps_[i].Parent());

    if (checkedMap.count(overworld_maps_[i].Parent()) > 0) {
      continue;
    }

    // If it's large then save parent pos *
    // 0x200 otherwise pos * 0x200
    if (overworld_maps_[i].IsLargeMap()) {
      // Check 1
      rom()->Write(overworldMapSize + i, 0x20);
      rom()->Write(overworldMapSize + i + 1, 0x20);
      rom()->Write(overworldMapSize + i + 8, 0x20);
      rom()->Write(overworldMapSize + i + 9, 0x20);

      // Check 2
      rom()->Write(overworldMapSizeHighByte + i, 0x03);
      rom()->Write(overworldMapSizeHighByte + i + 1, 0x03);
      rom()->Write(overworldMapSizeHighByte + i + 8, 0x03);
      rom()->Write(overworldMapSizeHighByte + i + 9, 0x03);

      // Check 3
      rom()->Write(overworldScreenSize + i, 0x00);
      rom()->Write(overworldScreenSize + i + 64, 0x00);

      rom()->Write(overworldScreenSize + i + 1, 0x00);
      rom()->Write(overworldScreenSize + i + 1 + 64, 0x00);

      rom()->Write(overworldScreenSize + i + 8, 0x00);
      rom()->Write(overworldScreenSize + i + 8 + 64, 0x00);

      rom()->Write(overworldScreenSize + i + 9, 0x00);
      rom()->Write(overworldScreenSize + i + 9 + 64, 0x00);

      // Check 4
      rom()->Write(OverworldScreenSizeForLoading + i, 0x04);
      rom()->Write(OverworldScreenSizeForLoading + i + 64, 0x04);
      rom()->Write(OverworldScreenSizeForLoading + i + 128, 0x04);

      rom()->Write(OverworldScreenSizeForLoading + i + 1, 0x04);
      rom()->Write(OverworldScreenSizeForLoading + i + 1 + 64, 0x04);
      rom()->Write(OverworldScreenSizeForLoading + i + 1 + 128, 0x04);

      rom()->Write(OverworldScreenSizeForLoading + i + 8, 0x04);
      rom()->Write(OverworldScreenSizeForLoading + i + 8 + 64, 0x04);
      rom()->Write(OverworldScreenSizeForLoading + i + 8 + 128, 0x04);

      rom()->Write(OverworldScreenSizeForLoading + i + 9, 0x04);
      rom()->Write(OverworldScreenSizeForLoading + i + 9 + 64, 0x04);
      rom()->Write(OverworldScreenSizeForLoading + i + 9 + 128, 0x04);

      // Check 5 and 6
      rom()->WriteShort(
          transition_target_north + (i * 2) + 2,
          (short)((parentyPos * 0x200) -
                  0xE0));  // (short) is placed to reduce the int to 2 bytes.
      rom()->WriteShort(transition_target_west + (i * 2) + 2,
                        (short)((parentxPos * 0x200) - 0x100));

      rom()->WriteShort(
          transition_target_north + (i * 2) + 16,
          (short)((parentyPos * 0x200) -
                  0xE0));  // (short) is placed to reduce the int to 2 bytes.
      rom()->WriteShort(transition_target_west + (i * 2) + 16,
                        (short)((parentxPos * 0x200) - 0x100));

      rom()->WriteShort(
          transition_target_north + (i * 2) + 18,
          (short)((parentyPos * 0x200) -
                  0xE0));  // (short) is placed to reduce the int to 2 bytes.
      rom()->WriteShort(transition_target_west + (i * 2) + 18,
                        (short)((parentxPos * 0x200) - 0x100));

      // Check 7 and 8
      rom()->WriteShort(overworldTransitionPositionX + (i * 2),
                        (parentxPos * 0x200));
      rom()->WriteShort(overworldTransitionPositionY + (i * 2),
                        (parentyPos * 0x200));

      rom()->WriteShort(overworldTransitionPositionX + (i * 2) + 2,
                        (parentxPos * 0x200));
      rom()->WriteShort(overworldTransitionPositionY + (i * 2) + 2,
                        (parentyPos * 0x200));

      rom()->WriteShort(overworldTransitionPositionX + (i * 2) + 16,
                        (parentxPos * 0x200));
      rom()->WriteShort(overworldTransitionPositionY + (i * 2) + 16,
                        (parentyPos * 0x200));

      rom()->WriteShort(overworldTransitionPositionX + (i * 2) + 18,
                        (parentxPos * 0x200));
      rom()->WriteShort(overworldTransitionPositionY + (i * 2) + 18,
                        (parentyPos * 0x200));

      // Check 9
      rom()->WriteShort(OverworldScreenTileMapChangeByScreen + (i * 2),
                        0x0060);  // Always 0x0060
      rom()->WriteShort(OverworldScreenTileMapChangeByScreen + (i * 2) + 2,
                        0x0060);  // Always 0x0060

      // If parentX == 0 then lower submaps == 0x0060 too
      if (parentxPos == 0) {
        rom()->WriteShort(OverworldScreenTileMapChangeByScreen + (i * 2) + 16,
                          0x0060);
        rom()->WriteShort(OverworldScreenTileMapChangeByScreen + (i * 2) + 18,
                          0x0060);
      } else {
        // Otherwise lower submaps == 0x1060
        rom()->WriteShort(OverworldScreenTileMapChangeByScreen + (i * 2) + 16,
                          0x1060);
        rom()->WriteShort(OverworldScreenTileMapChangeByScreen + (i * 2) + 18,
                          0x1060);
      }

      rom()->WriteShort(OverworldScreenTileMapChangeByScreen + (i * 2) + 128,
                        0x0080);  // Always 0x0080
      rom()->WriteShort(
          OverworldScreenTileMapChangeByScreen + (i * 2) + 2 + 128,
          0x0080);  // Always 0x0080
                    // Lower are always 8010
      rom()->WriteShort(
          OverworldScreenTileMapChangeByScreen + (i * 2) + 16 + 128,
          0x1080);  // Always 0x1080
      rom()->WriteShort(
          OverworldScreenTileMapChangeByScreen + (i * 2) + 18 + 128,
          0x1080);  // Always 0x1080

      rom()->WriteShort(OverworldScreenTileMapChangeByScreen + (i * 2) + 256,
                        0x1800);  // Always 0x1800
      rom()->WriteShort(
          OverworldScreenTileMapChangeByScreen + (i * 2) + 16 + 256,
          0x1800);  // Always 0x1800
                    // Right side is always 1840
      rom()->WriteShort(
          OverworldScreenTileMapChangeByScreen + (i * 2) + 2 + 256,
          0x1840);  // Always 0x1840
      rom()->WriteShort(
          OverworldScreenTileMapChangeByScreen + (i * 2) + 18 + 256,
          0x1840);  // Always 0x1840

      rom()->WriteShort(OverworldScreenTileMapChangeByScreen + (i * 2) + 384,
                        0x2000);  // Always 0x2000
      rom()->WriteShort(
          OverworldScreenTileMapChangeByScreen + (i * 2) + 16 + 384,
          0x2000);  // Always 0x2000
                    // Right side is always 0x2040
      rom()->WriteShort(
          OverworldScreenTileMapChangeByScreen + (i * 2) + 2 + 384,
          0x2040);  // Always 0x2000
      rom()->WriteShort(
          OverworldScreenTileMapChangeByScreen + (i * 2) + 18 + 384,
          0x2040);  // Always 0x2000

      checkedMap.emplace(i, 1);
      checkedMap.emplace((i + 1), 1);
      checkedMap.emplace((i + 8), 1);
      checkedMap.emplace((i + 9), 1);

    } else {
      rom()->Write(overworldMapSize + i, 0x00);
      rom()->Write(overworldMapSizeHighByte + i, 0x01);

      rom()->Write(overworldScreenSize + i, 0x01);
      rom()->Write(overworldScreenSize + i + 64, 0x01);

      rom()->Write(OverworldScreenSizeForLoading + i, 0x02);
      rom()->Write(OverworldScreenSizeForLoading + i + 64, 0x02);
      rom()->Write(OverworldScreenSizeForLoading + i + 128, 0x02);

      rom()->WriteShort(OverworldScreenTileMapChangeByScreen + (i * 2), 0x0060);
      rom()->WriteShort(OverworldScreenTileMapChangeByScreen + (i * 2) + 128,
                        0x0040);
      rom()->WriteShort(OverworldScreenTileMapChangeByScreen + (i * 2) + 256,
                        0x1800);
      rom()->WriteShort(OverworldScreenTileMapChangeByScreen + (i * 2) + 384,
                        0x1000);

      rom()->WriteShort(transition_target_north + (i * 2),
                        (short)((yPos * 0x200) - 0xE0));
      rom()->WriteShort(transition_target_west + (i * 2),
                        (short)((xPos * 0x200) - 0x100));

      rom()->WriteShort(overworldTransitionPositionX + (i * 2), (xPos * 0x200));
      rom()->WriteShort(overworldTransitionPositionY + (i * 2), (yPos * 0x200));

      checkedMap.emplace(i, 1);
    }
  }
  return absl::OkStatus();
}

// ----------------------------------------------------------------------------

bool Overworld::CreateTile32Tilemap(bool only_show) {
  tiles32_unique_.clear();
  tiles32.clear();

  OWBlockset *tiles_used;
  for (int i = 0; i < kNumOverworldMaps; i++) {
    if (i < 64) {
      tiles_used = &map_tiles_.light_world;
    } else if (i < 128 && i >= 64) {
      tiles_used = &map_tiles_.dark_world;
    } else {
      tiles_used = &map_tiles_.special_world;
    }

    std::vector<uint64_t> all_tile_16 = GetAllTile16(*tiles_used);

    std::vector<uint64_t> unique_tiles(all_tile_16);  // Ensure it's 64 bits
    std::sort(unique_tiles.begin(), unique_tiles.end());
    unique_tiles.erase(std::unique(unique_tiles.begin(), unique_tiles.end()),
                       unique_tiles.end());

    // Ensure it's 64 bits
    std::unordered_map<uint64_t, ushort> all_tiles_indexed;
    for (size_t j = 0; j < unique_tiles.size(); j++) {
      all_tiles_indexed.insert({unique_tiles[j], static_cast<ushort>(j)});
    }

    for (int j = 0; j < NumberOfMap32; j++) {
      tiles32.push_back(all_tiles_indexed[all_tile_16[j]]);
    }

    for (const auto &tile : unique_tiles) {
      tiles32_unique_.push_back(static_cast<ushort>(tile));
    }

    while (tiles32_unique_.size() % 4 != 0) {
      gfx::Tile32 padding_tile(420, 420, 420, 420);
      tiles32_unique_.push_back(padding_tile.GetPackedValue());
    }
  }

  if (only_show) {
    std::cout << "Number of unique Tiles32: " << tiles32_unique_.size()
              << " Out of: " << LimitOfMap32 << std::endl;
  } else if (tiles32_unique_.size() > LimitOfMap32) {
    std::cerr << "Number of unique Tiles32: " << tiles32_unique_.size()
              << " Out of: " << LimitOfMap32
              << "\nUnique Tile32 count exceed the limit"
              << "\nThe ROM Has not been saved"
              << "\nYou can fill maps with grass tiles to free some space"
              << "\nOr use the option Clear DW Tiles in the Overworld Menu"
              << std::endl;
    return true;
  }

  std::cout << "Number of unique Tiles32: " << tiles32_unique_.size()
            << " Saved:" << tiles32_unique_.size()
            << " Out of: " << LimitOfMap32 << std::endl;

  int v = tiles32_unique_.size();
  for (int i = v; i < LimitOfMap32; i++) {
    gfx::Tile32 padding_tile(420, 420, 420, 420);
    tiles32_unique_.push_back(padding_tile.GetPackedValue());
  }

  return false;
}

// ----------------------------------------------------------------------------

absl::Status Overworld::SaveMap16Tiles() {
  int tpos = kMap16Tiles;
  // 3760
  for (int i = 0; i < NumberOfMap16; i += 1) {
    RETURN_IF_ERROR(rom()->RunTransaction(
        WriteAction{tpos, uint16_t(TileInfoToShort(tiles16[i].tile0_))},
        WriteAction{tpos += 2, uint16_t(TileInfoToShort(tiles16[i].tile1_))},
        WriteAction{tpos += 2, uint16_t(TileInfoToShort(tiles16[i].tile2_))},
        WriteAction{tpos += 2, uint16_t(TileInfoToShort(tiles16[i].tile3_))}));

    // rom()->WriteShort(tpos, TileInfoToShort(tiles16[i].tile0_));
    // tpos += 2;
    // rom()->WriteShort(tpos, TileInfoToShort(tiles16[i].tile1_));
    // tpos += 2;
    // rom()->WriteShort(tpos, TileInfoToShort(tiles16[i].tile2_));
    // tpos += 2;
    // rom()->WriteShort(tpos, TileInfoToShort(tiles16[i].tile3_));
    // tpos += 2;
  }
}

// ----------------------------------------------------------------------------

absl::Status Overworld::SaveMap32Tiles() {
  int index = 0;
  int c = tiles32_unique_.size();

  for (int i = 0; i < c; i += 6) {
    if (index >= 0x4540) {
      return absl::AbortedError("Too many unique tile32 definitions.");
    }

    // Helper lambda to avoid code repetition
    auto writeTilesToRom = [&](int base_addr, auto get_tile) {
      for (int j = 0; j < 4; ++j) {
        rom()->Write(base_addr + i + j,
                     get_tile(tiles32_unique_[index + j]) & 0xFF);
      }
      rom()->Write(base_addr + i + 4,
                   ((get_tile(tiles32_unique_[index]) >> 4) & 0xF0) |
                       ((get_tile(tiles32_unique_[index + 1]) >> 8) & 0x0F));
      rom()->Write(base_addr + i + 5,
                   ((get_tile(tiles32_unique_[index + 2]) >> 4) & 0xF0) |
                       ((get_tile(tiles32_unique_[index + 3]) >> 8) & 0x0F));
    };

    writeTilesToRom(rom()->GetVersionConstants().kMap32TileTL,
                    [](const gfx::Tile32 &t) { return t.tile0_; });
    writeTilesToRom(rom()->GetVersionConstants().kMap32TileTR,
                    [](const gfx::Tile32 &t) { return t.tile1_; });
    writeTilesToRom(rom()->GetVersionConstants().kMap32TileBL,
                    [](const gfx::Tile32 &t) { return t.tile2_; });
    writeTilesToRom(rom()->GetVersionConstants().kMap32TileBR,
                    [](const gfx::Tile32 &t) { return t.tile3_; });

    index += 4;
    c += 2;
  }
  return absl::OkStatus();
}

// ----------------------------------------------------------------------------

uint16_t Overworld::GenerateTile32(int index, int quadrant, int dimension) {
  // The addresses of the four 32x32 pixel tiles in the ROM.
  const uint32_t map32address[4] = {rom()->GetVersionConstants().kMap32TileTL,
                                    rom()->GetVersionConstants().kMap32TileTR,
                                    rom()->GetVersionConstants().kMap32TileBL,
                                    rom()->GetVersionConstants().kMap32TileBR};

  return (ushort)(rom_[map32address[dimension] + quadrant + (index)] +
                  (((rom_[map32address[dimension] + (index) +
                          (quadrant <= 1 ? 4 : 5)] >>
                     (quadrant % 2 == 0 ? 4 : 0)) &
                    0x0F) *
                   256));
}

// ----------------------------------------------------------------------------

void Overworld::AssembleMap32Tiles() {
  // Loop through each 32x32 pixel tile in the ROM.
  for (int i = 0; i < 0x33F0; i += 6) {
    // Loop through each quadrant of the 32x32 pixel tile.
    for (int k = 0; k < 4; k++) {
      // Generate the 16-bit tile for the current quadrant of the current
      // 32x32 pixel tile.
      uint16_t tl = GenerateTile32(i, k, (int)Dimension::map32TilesTL);
      uint16_t tr = GenerateTile32(i, k, (int)Dimension::map32TilesTR);
      uint16_t bl = GenerateTile32(i, k, (int)Dimension::map32TilesBL);
      uint16_t br = GenerateTile32(i, k, (int)Dimension::map32TilesBR);

      // Add the generated 16-bit tiles to the tiles32 vector.
      tiles32.push_back(gfx::Tile32(tl, tr, bl, br));
    }
  }

  // Initialize the light_world, dark_world, and special_world vectors with
  // the appropriate number of tiles.
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
  int tpos = kMap16Tiles;
  for (int i = 0; i < 4096; i += 1) {
    auto t0 = gfx::GetTilesInfo(rom()->toint16(tpos));
    tpos += 2;
    auto t1 = gfx::GetTilesInfo(rom()->toint16(tpos));
    tpos += 2;
    auto t2 = gfx::GetTilesInfo(rom()->toint16(tpos));
    tpos += 2;
    auto t3 = gfx::GetTilesInfo(rom()->toint16(tpos));
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
      if (int tpos = tidD; tpos < tiles32.size()) {
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
    auto p1 = GetOwMapGfxHighPtr(
        rom()->data(), i,
        rom()->GetVersionConstants().kCompressedAllMap32PointersHigh);
    auto p2 = GetOwMapGfxLowPtr(
        rom()->data(), i,
        rom()->GetVersionConstants().kCompressedAllMap32PointersLow);
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

    ASSIGN_OR_RETURN(auto bytes,
                     gfx::lc_lz2::DecompressOverworld(rom()->data(), p2, 1000))
    ASSIGN_OR_RETURN(auto bytes2,
                     gfx::lc_lz2::DecompressOverworld(rom()->data(), p1, 1000))
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

absl::Status Overworld::DecompressProtoMapTiles(const std::string &filename) {
  proto_map_data_ = parseFile(filename);
  int sx = 0;
  int sy = 0;
  int c = 0;
  for (int i = 0; i < proto_map_data_.size(); i++) {
    int ttpos = 0;

    ASSIGN_OR_RETURN(auto bytes, gfx::lc_lz2::DecompressOverworld(
                                     proto_map_data_[i].lowData, 0,
                                     proto_map_data_[i].lowData.size()))
    ASSIGN_OR_RETURN(auto bytes2, gfx::lc_lz2::DecompressOverworld(
                                      proto_map_data_[i].highData, 0,
                                      proto_map_data_[i].highData.size()))
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
    short mapId = rom()->toint16(OWEntranceMap + (i * 2));
    ushort mapPos = rom()->toint16(OWEntrancePos + (i * 2));
    uchar entranceId = (rom_[OWEntranceEntranceId + i]);
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
    auto mapId = (short)((rom_[OWHoleArea + (i * 2) + 1] << 8) +
                         (rom_[OWHoleArea + (i * 2)]));
    auto mapPos = (short)((rom_[OWHolePos + (i * 2) + 1] << 8) +
                          (rom_[OWHolePos + (i * 2)]));
    uchar entranceId = (rom_[OWHoleEntrance + i]);
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
  for (int i = 0; i < 3; i++) {
    all_sprites_.emplace_back();
  }

  for (int i = 0; i < 64; i++) {
    all_sprites_[0].emplace_back();
  }

  for (int i = 0; i < 144; i++) {
    all_sprites_[1].emplace_back();
  }

  for (int i = 0; i < 144; i++) {
    all_sprites_[2].emplace_back();
  }

  LoadSpritesFromMap(overworldSpritesBegining, 64, 0);
  LoadSpritesFromMap(overworldSpritesZelda, 144, 1);
  LoadSpritesFromMap(overworldSpritesAgahnim, 144, 2);
}

// ----------------------------------------------------------------------------

void Overworld::LoadSpritesFromMap(int spriteStart, int spriteCount,
                                   int spriteIndex) {
  for (int i = 0; i < spriteCount; i++) {
    if (map_parent_[i] != i) continue;

    int ptrPos = spriteStart + (i * 2);
    int spriteAddress = core::SnesToPc((0x09 << 0x10) + rom()->toint16(ptrPos));
    while (true) {
      uchar b1 = rom_[spriteAddress];
      uchar b2 = rom_[spriteAddress + 1];
      uchar b3 = rom_[spriteAddress + 2];
      if (b1 == 0xFF) break;

      int editorMapIndex = i;
      if (editorMapIndex >= 128)
        editorMapIndex -= 128;
      else if (editorMapIndex >= 64)
        editorMapIndex -= 64;

      int mapY = (editorMapIndex / 8);
      int mapX = (editorMapIndex % 8);

      int realX = ((b2 & 0x3F) * 16) + mapX * 512;
      int realY = ((b1 & 0x3F) * 16) + mapY * 512;
      auto graphics_bytes = overworld_maps_[i].AreaGraphics();
      all_sprites_[spriteIndex][i].InitSprite(graphics_bytes, (uchar)i, b3,
                                              (uchar)(b2 & 0x3F),
                                              (uchar)(b1 & 0x3F), realX, realY);
      all_sprites_[spriteIndex][i].Draw();

      spriteAddress += 3;
    }
  }
}

absl::Status Overworld::LoadPrototype(ROM &rom,
                                      const std::string &tilemap_filename) {
  rom_ = rom;

  AssembleMap32Tiles();
  AssembleMap16Tiles();
  RETURN_IF_ERROR(DecompressProtoMapTiles(tilemap_filename))

  for (int map_index = 0; map_index < kNumOverworldMaps; ++map_index)
    overworld_maps_.emplace_back(map_index, rom_, tiles16);

  FetchLargeMaps();
  LoadEntrances();

  auto size = tiles16.size();
  std::vector<std::future<absl::Status>> futures;
  for (int i = 0; i < kNumOverworldMaps; ++i) {
    futures.push_back(std::async(std::launch::async, [this, i, size]() {
      if (i < 64) {
        return overworld_maps_[i].BuildMap(size, game_state_, 0, map_parent_,
                                           map_tiles_.light_world);
      } else if (i < 0x80 && i >= 0x40) {
        return overworld_maps_[i].BuildMap(size, game_state_, 1, map_parent_,
                                           map_tiles_.dark_world);
      } else {
        return overworld_maps_[i].BuildMap(size, game_state_, 2, map_parent_,
                                           map_tiles_.special_world);
      }
    }));
  }

  // Wait for all tasks to complete and check their results
  for (auto &future : futures) {
    absl::Status status = future.get();
    if (!status.ok()) {
      return status;
    }
  }

  // LoadSprites();

  is_loaded_ = true;
  return absl::OkStatus();
}

}  // namespace zelda3
}  // namespace app
}  // namespace yaze