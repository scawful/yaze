#include "overworld.h"

#include <SDL.h>

#include <fstream>
#include <future>
#include <memory>
#include <unordered_map>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "app/core/common.h"
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
  LoadExits();
  RETURN_IF_ERROR(LoadOverworldMaps())
  if (flags()->kDrawOverworldSprites) {
    LoadSprites();
  }

  is_loaded_ = true;
  return absl::OkStatus();
}

absl::Status Overworld::Save(ROM &rom) {
  rom_ = rom;

  RETURN_IF_ERROR(SaveMap16Tiles())
  RETURN_IF_ERROR(SaveMap32Tiles())
  RETURN_IF_ERROR(SaveOverworldMaps())
  RETURN_IF_ERROR(SaveEntrances())
  RETURN_IF_ERROR(SaveExits())

  return absl::OkStatus();
}

absl::Status Overworld::LoadOverworldMaps() {
  auto size = tiles16.size();
  std::vector<std::future<absl::Status>> futures;
  for (int i = 0; i < kNumOverworldMaps; ++i) {
    int world_type = 0;
    if (i >= 64 && i < 0x80) {
      world_type = 1;
    } else if (i >= 0x80) {
      world_type = 2;
    }
    futures.push_back(std::async(std::launch::async, [this, i, size,
                                                      world_type]() {
      return overworld_maps_[i].BuildMap(size, game_state_, world_type,
                                         map_parent_, GetMapTiles(world_type));
    }));
  }

  // Wait for all tasks to complete and check their results
  for (auto &future : futures) {
    absl::Status status = future.get();
    if (!status.ok()) {
      return status;
    }
  }
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

absl::Status Overworld::SaveLargeMaps() {
  for (int i = 0; i < 0x40; i++) {
    int yPos = i / 8;
    int xPos = i % 8;
    int parentyPos = overworld_maps_[i].Parent() / 8;
    int parentxPos = overworld_maps_[i].Parent() % 8;

    std::unordered_map<uint8_t, uint8_t> checked_map;

    // Always write the map parent since it should not matter
    RETURN_IF_ERROR(
        rom()->Write(overworldMapParentId + i, overworld_maps_[i].Parent()))

    if (checked_map.count(overworld_maps_[i].Parent()) > 0) {
      continue;
    }

    // If it's large then save parent pos *
    // 0x200 otherwise pos * 0x200
    if (overworld_maps_[i].IsLargeMap()) {
      RETURN_IF_ERROR(rom()->RunTransaction(
          // Check 1
          WriteAction{overworldMapSize + i, 0x20},
          WriteAction{overworldMapSize + i + 1, 0x20},
          WriteAction{overworldMapSize + i + 8, 0x20},
          WriteAction{overworldMapSize + i + 9, 0x20},

          // Check 2
          WriteAction{overworldMapSizeHighByte + i, 0x03},
          WriteAction{overworldMapSizeHighByte + i + 1, 0x03},
          WriteAction{overworldMapSizeHighByte + i + 8, 0x03},
          WriteAction{overworldMapSizeHighByte + i + 9, 0x03},

          // Check 3
          WriteAction{overworldScreenSize + i, 0x00},
          WriteAction{overworldScreenSize + i + 64, 0x00},

          WriteAction{overworldScreenSize + i + 1, 0x00},
          WriteAction{overworldScreenSize + i + 1 + 64, 0x00},

          WriteAction{overworldScreenSize + i + 8, 0x00},
          WriteAction{overworldScreenSize + i + 8 + 64, 0x00},

          WriteAction{overworldScreenSize + i + 9, 0x00},
          WriteAction{overworldScreenSize + i + 9 + 64, 0x00},

          // Check 4
          WriteAction{OverworldScreenSizeForLoading + i, 0x04},
          WriteAction{OverworldScreenSizeForLoading + i + 64, 0x04},
          WriteAction{OverworldScreenSizeForLoading + i + 128, 0x04},

          WriteAction{OverworldScreenSizeForLoading + i + 1, 0x04},
          WriteAction{OverworldScreenSizeForLoading + i + 1 + 64, 0x04},
          WriteAction{OverworldScreenSizeForLoading + i + 1 + 128, 0x04},

          WriteAction{OverworldScreenSizeForLoading + i + 8, 0x04},
          WriteAction{OverworldScreenSizeForLoading + i + 8 + 64, 0x04},
          WriteAction{OverworldScreenSizeForLoading + i + 8 + 128, 0x04},

          WriteAction{OverworldScreenSizeForLoading + i + 9, 0x04},
          WriteAction{OverworldScreenSizeForLoading + i + 9 + 64, 0x04},
          WriteAction{OverworldScreenSizeForLoading + i + 9 + 128, 0x04},

          // Check 5 and 6
          WriteAction{transition_target_north + (i * 2) + 2,
                      (short)((parentyPos * 0x200) -
                              0xE0)},  // (short) is placed to reduce the int to
                                       // 2 bytes.
          WriteAction{transition_target_west + (i * 2) + 2,
                      (short)((parentxPos * 0x200) - 0x100)},

          // (short) is placed to reduce the int to 2 bytes.
          WriteAction{transition_target_north + (i * 2) + 16,
                      (short)((parentyPos * 0x200) - 0xE0)},
          WriteAction{transition_target_west + (i * 2) + 16,
                      (short)((parentxPos * 0x200) - 0x100)},

          // (short) is placed to reduce the int to 2 bytes.
          WriteAction{transition_target_north + (i * 2) + 18,
                      (short)((parentyPos * 0x200) - 0xE0)},
          WriteAction{transition_target_west + (i * 2) + 18,
                      (short)((parentxPos * 0x200) - 0x100)},

          // Check 7 and 8
          WriteAction{overworldTransitionPositionX + (i * 2),
                      (parentxPos * 0x200)},
          WriteAction{overworldTransitionPositionY + (i * 2),
                      (parentyPos * 0x200)},

          WriteAction{overworldTransitionPositionX + (i * 2) + 2,
                      (parentxPos * 0x200)},
          WriteAction{overworldTransitionPositionY + (i * 2) + 2,
                      (parentyPos * 0x200)},

          WriteAction{overworldTransitionPositionX + (i * 2) + 16,
                      (parentxPos * 0x200)},
          WriteAction{overworldTransitionPositionY + (i * 2) + 16,
                      (parentyPos * 0x200)},

          WriteAction{overworldTransitionPositionX + (i * 2) + 18,
                      (parentxPos * 0x200)},
          WriteAction{overworldTransitionPositionY + (i * 2) + 18,
                      (parentyPos * 0x200)},

          // Check 9
          // Always 0x0060
          WriteAction{OverworldScreenTileMapChangeByScreen + (i * 2), 0x0060},
          // Always 0x0060
          WriteAction{OverworldScreenTileMapChangeByScreen + (i * 2) + 2,
                      0x0060}))

      uint16_t lower_submaps;
      // If parentX == 0 then lower submaps == 0x0060 too
      if (parentxPos == 0) {
        lower_submaps = 0x0060;
      } else {
        // Otherwise lower submaps == 0x1060
        lower_submaps = 0x1060;
      }

      RETURN_IF_ERROR(rom()->RunTransaction(
          WriteAction{OverworldScreenTileMapChangeByScreen + (i * 2) + 16,
                      uint16_t(lower_submaps)},
          WriteAction{OverworldScreenTileMapChangeByScreen + (i * 2) + 18,
                      uint16_t(lower_submaps)},
          WriteAction{OverworldScreenTileMapChangeByScreen + (i * 2) + 128,
                      uint16_t(0x0080)},  // Always 0x0080
          WriteAction{OverworldScreenTileMapChangeByScreen + (i * 2) + 2 + 128,
                      uint16_t(0x0080)},  // Always 0x0080
                                          // Lower are always 8010
          WriteAction{OverworldScreenTileMapChangeByScreen + (i * 2) + 16 + 128,
                      uint16_t(0x1080)},  // Always 0x1080
          WriteAction{OverworldScreenTileMapChangeByScreen + (i * 2) + 18 + 128,
                      uint16_t(0x1080)},  // Always 0x1080

          WriteAction{OverworldScreenTileMapChangeByScreen + (i * 2) + 256,
                      uint16_t(0x1800)},  // Always 0x1800
          WriteAction{OverworldScreenTileMapChangeByScreen + (i * 2) + 16 + 256,
                      uint16_t(0x1800)},  // Always 0x1800
                                          // Right side is always 1840
          WriteAction{OverworldScreenTileMapChangeByScreen + (i * 2) + 2 + 256,
                      uint16_t(0x1840)},  // Always 0x1840
          WriteAction{OverworldScreenTileMapChangeByScreen + (i * 2) + 18 + 256,
                      uint16_t(0x1840)},  // Always 0x1840

          WriteAction{OverworldScreenTileMapChangeByScreen + (i * 2) + 384,
                      uint16_t(0x2000)},  // Always 0x2000
          WriteAction{OverworldScreenTileMapChangeByScreen + (i * 2) + 16 + 384,
                      uint16_t(0x2000)},  // Always 0x2000
                                          // Right side is always 0x2040
          WriteAction{OverworldScreenTileMapChangeByScreen + (i * 2) + 2 + 384,
                      uint16_t(0x2040)},  // Always 0x2000
          WriteAction{OverworldScreenTileMapChangeByScreen + (i * 2) + 18 + 384,
                      uint16_t(0x2040)}))  // Always 0x2000

      checked_map.emplace(i, 1);
      checked_map.emplace((i + 1), 1);
      checked_map.emplace((i + 8), 1);
      checked_map.emplace((i + 9), 1);

    } else {
      RETURN_IF_ERROR(rom()->RunTransaction(
          WriteAction{overworldMapSize + i, 0x00},
          WriteAction{overworldMapSizeHighByte + i, 0x01},
          WriteAction{overworldScreenSize + i, 0x01},
          WriteAction{overworldScreenSize + i + 64, 0x01},
          WriteAction{OverworldScreenSizeForLoading + i, 0x02},
          WriteAction{OverworldScreenSizeForLoading + i + 64, 0x02},
          WriteAction{OverworldScreenSizeForLoading + i + 128, 0x02},

          WriteAction{OverworldScreenTileMapChangeByScreen + (i * 2),
                      uint16_t(0x0060)},
          WriteAction{OverworldScreenTileMapChangeByScreen + (i * 2) + 128,
                      uint16_t(0x0040)},
          WriteAction{OverworldScreenTileMapChangeByScreen + (i * 2) + 256,
                      uint16_t(0x1800)},
          WriteAction{OverworldScreenTileMapChangeByScreen + (i * 2) + 384,
                      (0x1000)},
          WriteAction{transition_target_north + (i * 2),
                      uint16_t((yPos * 0x200) - 0xE0)},
          WriteAction{transition_target_west + (i * 2),
                      uint16_t((xPos * 0x200) - 0x100)},
          WriteAction{overworldTransitionPositionX + (i * 2),
                      uint16_t(xPos * 0x200)},
          WriteAction{overworldTransitionPositionY + (i * 2),
                      uint16_t(yPos * 0x200)}))

      checked_map.emplace(i, 1);
    }
  }
  return absl::OkStatus();
}

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

absl::Status Overworld::SaveMap16Tiles() {
  int tpos = kMap16Tiles;
  // 3760
  for (int i = 0; i < NumberOfMap16; i += 1) {
    RETURN_IF_ERROR(rom()->WriteShort(tpos, TileInfoToShort(tiles16[i].tile0_)))
    tpos += 2;
    RETURN_IF_ERROR(rom()->WriteShort(tpos, TileInfoToShort(tiles16[i].tile1_)))
    tpos += 2;
    RETURN_IF_ERROR(rom()->WriteShort(tpos, TileInfoToShort(tiles16[i].tile2_)))
    tpos += 2;
    RETURN_IF_ERROR(rom()->WriteShort(tpos, TileInfoToShort(tiles16[i].tile3_)))
    tpos += 2;
  }
  return absl::OkStatus();
}

absl::Status Overworld::SaveMap32Tiles() {
  constexpr int kMaxUniqueTiles = 0x4540;
  constexpr int kTilesPer32x32Tile = 6;
  constexpr int kQuadrantsPer32x32Tile = 4;

  if (tiles32_unique_.size() % kTilesPer32x32Tile != 0) {
    return absl::InvalidArgumentError("Invalid number of unique tiles.");
  }

  int unique_tile_index = 0;
  int num_unique_tiles = tiles32_unique_.size();
  int num_32x32_tiles = num_unique_tiles / kTilesPer32x32Tile;

  if (num_32x32_tiles > kMaxUniqueTiles / kQuadrantsPer32x32Tile) {
    return absl::AbortedError("Too many unique tile32 definitions.");
  }

  for (int i = 0; i < num_32x32_tiles; ++i) {
    int base_addr =
        rom()->version_constants().kMap32TileTL + i * kQuadrantsPer32x32Tile;

    auto write_quadrant_to_rom = [&](int quadrant,
                                     auto get_tile) -> absl::Status {
      for (int j = 0; j < kQuadrantsPer32x32Tile; ++j) {
        int tile_index = unique_tile_index + j;
        const gfx::Tile32 &tile = tiles32_unique_[tile_index];
        RETURN_IF_ERROR(
            rom()->Write(base_addr + quadrant + j, get_tile(tile) & 0xFF));
      }

      int tile0 = get_tile(tiles32_unique_[unique_tile_index]);
      int tile1 = get_tile(tiles32_unique_[unique_tile_index + 1]);
      int tile2 = get_tile(tiles32_unique_[unique_tile_index + 2]);
      int tile3 = get_tile(tiles32_unique_[unique_tile_index + 3]);

      RETURN_IF_ERROR(
          rom()->Write(base_addr + quadrant + 4,
                       ((tile0 >> 4) & 0xF0) | ((tile1 >> 8) & 0x0F)));
      RETURN_IF_ERROR(
          rom()->Write(base_addr + quadrant + 5,
                       ((tile2 >> 4) & 0xF0) | ((tile3 >> 8) & 0x0F)));
      return absl::OkStatus();
    };

    RETURN_IF_ERROR(write_quadrant_to_rom(
        0, [](const gfx::Tile32 &t) { return t.tile0_; }));
    RETURN_IF_ERROR(write_quadrant_to_rom(
        1, [](const gfx::Tile32 &t) { return t.tile1_; }));
    RETURN_IF_ERROR(write_quadrant_to_rom(
        2, [](const gfx::Tile32 &t) { return t.tile2_; }));
    RETURN_IF_ERROR(write_quadrant_to_rom(
        3, [](const gfx::Tile32 &t) { return t.tile3_; }));

    unique_tile_index += kTilesPer32x32Tile;
  }

  return absl::OkStatus();
}

uint16_t Overworld::GenerateTile32(int index, int quadrant, int dimension) {
  // The addresses of the four 32x32 pixel tiles in the ROM.
  const uint32_t map32address[4] = {rom()->version_constants().kMap32TileTL,
                                    rom()->version_constants().kMap32TileTR,
                                    rom()->version_constants().kMap32TileBL,
                                    rom()->version_constants().kMap32TileBR};

  return (ushort)(rom_[map32address[dimension] + quadrant + (index)] +
                  (((rom_[map32address[dimension] + (index) +
                          (quadrant <= 1 ? 4 : 5)] >>
                     (quadrant % 2 == 0 ? 4 : 0)) &
                    0x0F) *
                   256));
}

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

absl::Status Overworld::DecompressAllMapTiles() {
  int lowest = 0x0FFFFF;
  int highest = 0x0F8000;
  int sx = 0;
  int sy = 0;
  int c = 0;
  for (int i = 0; i < 160; i++) {
    auto p1 = GetOwMapGfxHighPtr(
        rom()->data(), i,
        rom()->version_constants().kCompressedAllMap32PointersHigh);
    auto p2 = GetOwMapGfxLowPtr(
        rom()->data(), i,
        rom()->version_constants().kCompressedAllMap32PointersLow);
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

void Overworld::LoadEntrances() {
  for (int i = 0; i < 129; i++) {
    short map_id = rom()->toint16(OWEntranceMap + (i * 2));
    ushort map_pos = rom()->toint16(OWEntrancePos + (i * 2));
    uchar entrance_id = rom_[OWEntranceEntranceId + i];
    int p = map_pos >> 1;
    int x = (p % 64);
    int y = (p >> 6);
    bool deleted = false;
    if (map_pos == 0xFFFF) {
      deleted = true;
    }
    all_entrances_.emplace_back(
        (x * 16) + (((map_id % 64) - (((map_id % 64) / 8) * 8)) * 512),
        (y * 16) + (((map_id % 64) / 8) * 512), entrance_id, map_id, map_pos,
        deleted);
  }

  for (int i = 0; i < 0x13; i++) {
    auto map_id = (short)((rom_[OWHoleArea + (i * 2) + 1] << 8) +
                          (rom_[OWHoleArea + (i * 2)]));
    auto map_pos = (short)((rom_[OWHolePos + (i * 2) + 1] << 8) +
                           (rom_[OWHolePos + (i * 2)]));
    uchar entrance_id = (rom_[OWHoleEntrance + i]);
    int p = (map_pos + 0x400) >> 1;
    int x = (p % 64);
    int y = (p >> 6);
    all_holes_.emplace_back(
        (x * 16) + (((map_id % 64) - (((map_id % 64) / 8) * 8)) * 512),
        (y * 16) + (((map_id % 64) / 8) * 512), entrance_id, map_id,
        (ushort)(map_pos + 0x400), true);
  }
}

absl::Status Overworld::SaveEntrances() {
  for (int i = 0; i < 129; i++) {
    RETURN_IF_ERROR(
        rom()->WriteShort(OWEntranceMap + (i * 2), all_entrances_[i].map_id_))
    RETURN_IF_ERROR(
        rom()->WriteShort(OWEntrancePos + (i * 2), all_entrances_[i].map_pos_))
    RETURN_IF_ERROR(rom()->WriteByte(OWEntranceEntranceId + i,
                                     all_entrances_[i].entrance_id_))
  }

  for (int i = 0; i < 0x13; i++) {
    RETURN_IF_ERROR(
        rom()->WriteShort(OWHoleArea + (i * 2), all_holes_[i].map_id_))
    RETURN_IF_ERROR(
        rom()->WriteShort(OWHolePos + (i * 2), all_holes_[i].map_pos_))
    RETURN_IF_ERROR(
        rom()->WriteByte(OWHoleEntrance + i, all_holes_[i].entrance_id_))
  }

  return absl::OkStatus();
}

absl::Status Overworld::SaveExits() {
  for (int i = 0; i < 0x4F; i++) {
    RETURN_IF_ERROR(
        rom()->WriteShort(OWExitRoomId + (i * 2), all_exits_[i].room_id_))
    RETURN_IF_ERROR(rom()->WriteByte(OWExitMapId + i, all_exits_[i].map_id_))
    RETURN_IF_ERROR(
        rom()->WriteShort(OWExitVram + (i * 2), all_exits_[i].map_pos_))
    RETURN_IF_ERROR(
        rom()->WriteShort(OWExitYScroll + (i * 2), all_exits_[i].y_scroll_))
    RETURN_IF_ERROR(
        rom()->WriteShort(OWExitXScroll + (i * 2), all_exits_[i].x_scroll_))
    RETURN_IF_ERROR(
        rom()->WriteShort(OWExitYPlayer + (i * 2), all_exits_[i].y_player_))
    RETURN_IF_ERROR(
        rom()->WriteShort(OWExitXPlayer + (i * 2), all_exits_[i].x_player_))
    RETURN_IF_ERROR(
        rom()->WriteShort(OWExitYCamera + (i * 2), all_exits_[i].y_camera_))
    RETURN_IF_ERROR(
        rom()->WriteShort(OWExitXCamera + (i * 2), all_exits_[i].x_camera_))
    RETURN_IF_ERROR(
        rom()->WriteByte(OWExitUnk1 + i, all_exits_[i].scroll_mod_y_))
    RETURN_IF_ERROR(
        rom()->WriteByte(OWExitUnk2 + i, all_exits_[i].scroll_mod_x_))
    RETURN_IF_ERROR(rom()->WriteShort(OWExitDoorType1 + (i * 2),
                                      all_exits_[i].door_type_1_))
    RETURN_IF_ERROR(rom()->WriteShort(OWExitDoorType2 + (i * 2),
                                      all_exits_[i].door_type_2_))
  }

  return absl::OkStatus();
}

void Overworld::LoadExits() {
  const int NumberOfOverworldExits = 0x4F;
  std::vector<OverworldExit> exits;
  for (int i = 0; i < NumberOfOverworldExits; i++) {
    auto rom_data = rom()->data();
    ushort exit_room_id = (ushort)((rom_data[OWExitRoomId + (i * 2) + 1] << 8) +
                                   rom_data[OWExitRoomId + (i * 2)]);
    ushort exit_map_id = rom_data[OWExitMapId + i];
    ushort exit_vram = (ushort)((rom_data[OWExitVram + (i * 2) + 1] << 8) +
                                rom_data[OWExitVram + (i * 2)]);
    ushort exit_y_scroll =
        (ushort)((rom_data[OWExitYScroll + (i * 2) + 1] << 8) +
                 rom_data[OWExitYScroll + (i * 2)]);
    ushort exit_x_scroll =
        (ushort)((rom_data[OWExitXScroll + (i * 2) + 1] << 8) +
                 rom_data[OWExitXScroll + (i * 2)]);
    ushort py = (ushort)((rom_data[OWExitYPlayer + (i * 2) + 1] << 8) +
                         rom_data[OWExitYPlayer + (i * 2)]);
    ushort px = (ushort)((rom_data[OWExitXPlayer + (i * 2) + 1] << 8) +
                         rom_data[OWExitXPlayer + (i * 2)]);
    ushort exit_y_camera =
        (ushort)((rom_data[OWExitYCamera + (i * 2) + 1] << 8) +
                 rom_data[OWExitYCamera + (i * 2)]);
    ushort exit_x_camera =
        (ushort)((rom_data[OWExitXCamera + (i * 2) + 1] << 8) +
                 rom_data[OWExitXCamera + (i * 2)]);
    ushort exit_scroll_mod_y = rom_data[OWExitUnk1 + i];
    ushort exit_scroll_mod_x = rom_data[OWExitUnk2 + i];
    ushort exit_door_type_1 =
        (ushort)((rom_data[OWExitDoorType1 + (i * 2) + 1] << 8) +
                 rom_data[OWExitDoorType1 + (i * 2)]);
    ushort exit_door_type_2 =
        (ushort)((rom_data[OWExitDoorType2 + (i * 2) + 1] << 8) +
                 rom_data[OWExitDoorType2 + (i * 2)]);
    OverworldExit exit(exit_room_id, exit_map_id, exit_vram, exit_y_scroll,
                       exit_x_scroll, py, px, exit_y_camera, exit_x_camera,
                       exit_scroll_mod_y, exit_scroll_mod_x, exit_door_type_1,
                       exit_door_type_2);

    if (rom()->flags()->kLogToConsole) {
      std::cout << "Exit: " << i << " RoomID: " << exit_room_id
                << " MapID: " << exit_map_id << " VRAM: " << exit_vram
                << " YScroll: " << exit_y_scroll
                << " XScroll: " << exit_x_scroll << " YPlayer: " << py
                << " XPlayer: " << px << " YCamera: " << exit_y_camera
                << " XCamera: " << exit_x_camera
                << " ScrollModY: " << exit_scroll_mod_y
                << " ScrollModX: " << exit_scroll_mod_x
                << " DoorType1: " << exit_door_type_1
                << " DoorType2: " << exit_door_type_2 << std::endl;
    }

    if (px == 0xFFFF && py == 0xFFFF) {
      exit.deleted = true;
    }

    exits.push_back(exit);
  }
  all_exits_ = exits;
}

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

OWBlockset &Overworld::GetMapTiles(int world_type) {
  switch (world_type) {
    case 0:
      return map_tiles_.light_world;
    case 1:
      return map_tiles_.dark_world;
    case 2:
      return map_tiles_.special_world;
    default:
      return map_tiles_.light_world;
  }
}

}  // namespace zelda3
}  // namespace app
}  // namespace yaze