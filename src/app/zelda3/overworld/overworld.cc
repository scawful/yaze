#include "overworld.h"

#include <algorithm>
#include <future>
#include <mutex>
#include <set>
#include <thread>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "app/core/features.h"
#include "app/gfx/performance_profiler.h"
#include "app/gfx/compression.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"
#include "app/snes.h"
#include "app/zelda3/overworld/overworld_entrance.h"
#include "app/zelda3/overworld/overworld_exit.h"
#include "util/hex.h"
#include "util/log.h"
#include "util/macro.h"

namespace yaze {
namespace zelda3 {

absl::Status Overworld::Load(Rom* rom) {
  gfx::ScopedTimer timer("Overworld::Load");
  
  if (rom->size() == 0) {
    return absl::InvalidArgumentError("ROM file not loaded");
  }
  rom_ = rom;

  // Phase 1: Tile Assembly (can be parallelized)
  {
    gfx::ScopedTimer assembly_timer("AssembleTiles");
    RETURN_IF_ERROR(AssembleMap32Tiles());
    RETURN_IF_ERROR(AssembleMap16Tiles());
  }

  // Phase 2: Map Decompression (major bottleneck - now parallelized)
  {
    gfx::ScopedTimer decompression_timer("DecompressAllMapTiles");
    RETURN_IF_ERROR(DecompressAllMapTilesParallel());
  }

  // Phase 3: Map Object Creation (fast)
  {
    gfx::ScopedTimer map_creation_timer("CreateOverworldMapObjects");
    for (int map_index = 0; map_index < kNumOverworldMaps; ++map_index)
      overworld_maps_.emplace_back(map_index, rom_);

    // Populate map_parent_ array with parent information from each map
    for (int map_index = 0; map_index < kNumOverworldMaps; ++map_index) {
      map_parent_[map_index] = overworld_maps_[map_index].parent();
    }
  }

  // Phase 4: Map Configuration
  uint8_t asm_version = (*rom_)[OverworldCustomASMHasBeenApplied];
  if (asm_version >= 3) {
    AssignMapSizes(overworld_maps_);
  } else {
    FetchLargeMaps();
  }

  // Phase 5: Data Loading (with individual timing)
  {
    gfx::ScopedTimer data_loading_timer("LoadOverworldData");
    
    {
      gfx::ScopedTimer tile_types_timer("LoadTileTypes");
      LoadTileTypes();
    }
    
    {
      gfx::ScopedTimer entrances_timer("LoadEntrances");
      RETURN_IF_ERROR(LoadEntrances());
    }
    
    {
      gfx::ScopedTimer holes_timer("LoadHoles");
      RETURN_IF_ERROR(LoadHoles());
    }
    
    {
      gfx::ScopedTimer exits_timer("LoadExits");
      RETURN_IF_ERROR(LoadExits());
    }
    
    {
      gfx::ScopedTimer items_timer("LoadItems");
      RETURN_IF_ERROR(LoadItems());
    }
    
    {
      gfx::ScopedTimer overworld_maps_timer("LoadOverworldMaps");
      RETURN_IF_ERROR(LoadOverworldMaps());
    }
    
    {
      gfx::ScopedTimer sprites_timer("LoadSprites");
      RETURN_IF_ERROR(LoadSprites());
    }
  }

  is_loaded_ = true;
  return absl::OkStatus();
}

void Overworld::FetchLargeMaps() {
  for (int i = 128; i < 145; i++) {
    overworld_maps_[i].SetAsSmallMap(0);
  }

  overworld_maps_[129].SetAsLargeMap(129, 0);
  overworld_maps_[130].SetAsLargeMap(129, 1);
  overworld_maps_[137].SetAsLargeMap(129, 2);
  overworld_maps_[138].SetAsLargeMap(129, 3);
  overworld_maps_[136].SetAsSmallMap();

  std::array<bool, kNumMapsPerWorld> map_checked;
  std::ranges::fill(map_checked, false);

  int xx = 0;
  int yy = 0;
  while (true) {
    if (int i = xx + (yy * 8); map_checked[i] == false) {
      if (overworld_maps_[i].is_large_map()) {
        map_checked[i] = true;
        overworld_maps_[i].SetAsLargeMap(i, 0);
        overworld_maps_[i + 64].SetAsLargeMap(i + 64, 0);

        map_checked[i + 1] = true;
        overworld_maps_[i + 1].SetAsLargeMap(i, 1);
        overworld_maps_[i + 65].SetAsLargeMap(i + 64, 1);

        map_checked[i + 8] = true;
        overworld_maps_[i + 8].SetAsLargeMap(i, 2);
        overworld_maps_[i + 72].SetAsLargeMap(i + 64, 2);

        map_checked[i + 9] = true;
        overworld_maps_[i + 9].SetAsLargeMap(i, 3);
        overworld_maps_[i + 73].SetAsLargeMap(i + 64, 3);
        xx++;
      } else {
        overworld_maps_[i].SetAsSmallMap();
        overworld_maps_[i + 64].SetAsSmallMap();
        map_checked[i] = true;
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

/**
 * @brief Loads all maps from ROM to see what size they are.
 * @param maps The maps to update (passed by reference)
 */
void Overworld::AssignMapSizes(std::vector<OverworldMap>& maps) {
  std::vector<bool> map_checked(kNumOverworldMaps, false);

  int xx = 0;
  int yy = 0;
  int world = 0;

  while (true) {
    int i = world + xx + (yy * 8);

    if (i >= static_cast<int>(map_checked.size())) {
      break;
    }

    if (!map_checked[i]) {
      switch (maps[i].area_size()) {
        case AreaSizeEnum::SmallArea:
          map_checked[i] = true;
          maps[i].SetAreaSize(AreaSizeEnum::SmallArea);
          break;

        case AreaSizeEnum::LargeArea:
          map_checked[i] = true;
          maps[i].SetAsLargeMap(i, 0);

          if (i + 1 < static_cast<int>(maps.size())) {
            map_checked[i + 1] = true;
            maps[i + 1].SetAsLargeMap(i, 1);
          }

          if (i + 8 < static_cast<int>(maps.size())) {
            map_checked[i + 8] = true;
            maps[i + 8].SetAsLargeMap(i, 2);
          }

          if (i + 9 < static_cast<int>(maps.size())) {
            map_checked[i + 9] = true;
            maps[i + 9].SetAsLargeMap(i, 3);
          }

          xx++;
          break;

        case AreaSizeEnum::WideArea:
          map_checked[i] = true;
          // CRITICAL FIX: Set parent for wide area maps
          // Map i is parent (left), map i+1 is child (right)
          maps[i].SetParent(i);  // Parent points to itself
          maps[i].SetAreaSize(AreaSizeEnum::WideArea);

          if (i + 1 < static_cast<int>(maps.size())) {
            map_checked[i + 1] = true;
            maps[i + 1].SetParent(i);  // Child points to parent
            maps[i + 1].SetAreaSize(AreaSizeEnum::WideArea);
          }

          xx++;
          break;

        case AreaSizeEnum::TallArea:
          map_checked[i] = true;
          // CRITICAL FIX: Set parent for tall area maps
          // Map i is parent (top), map i+8 is child (bottom)
          maps[i].SetParent(i);  // Parent points to itself
          maps[i].SetAreaSize(AreaSizeEnum::TallArea);

          if (i + 8 < static_cast<int>(maps.size())) {
            map_checked[i + 8] = true;
            maps[i + 8].SetParent(i);  // Child points to parent
            maps[i + 8].SetAreaSize(AreaSizeEnum::TallArea);
          }
          break;
      }
    }

    xx++;
    if (xx >= 8) {
      xx = 0;
      yy += 1;

      if (yy >= 8) {
        yy = 0;
        world += 0x40;
      }
    }
  }
}

absl::Status Overworld::ConfigureMultiAreaMap(int parent_index, AreaSizeEnum size) {
  if (parent_index < 0 || parent_index >= kNumOverworldMaps) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid parent index: %d", parent_index));
  }
  
  // Check ROM version
  uint8_t asm_version = (*rom_)[OverworldCustomASMHasBeenApplied];
  
  // Version requirements:
  // - Vanilla (0xFF): Supports Small and Large only
  // - v1-v2: Supports Small and Large only
  // - v3+: Supports all 4 sizes (Small, Large, Wide, Tall)
  if ((size == AreaSizeEnum::WideArea || size == AreaSizeEnum::TallArea) &&
      (asm_version < 3 || asm_version == 0xFF)) {
    return absl::FailedPreconditionError(
        "Wide and Tall areas require ZSCustomOverworld v3+");
  }
  
  LOG_INFO("Overworld", "ConfigureMultiAreaMap: parent=%d, current_size=%d, new_size=%d, version=%d",
           parent_index, static_cast<int>(overworld_maps_[parent_index].area_size()),
           static_cast<int>(size), asm_version);
  
  // CRITICAL: First, get OLD siblings (before changing) so we can reset them
  std::vector<int> old_siblings;
  auto old_size = overworld_maps_[parent_index].area_size();
  int old_parent = overworld_maps_[parent_index].parent();
  
  switch (old_size) {
    case AreaSizeEnum::LargeArea:
      old_siblings = {old_parent, old_parent + 1, old_parent + 8, old_parent + 9};
      break;
    case AreaSizeEnum::WideArea:
      old_siblings = {old_parent, old_parent + 1};
      break;
    case AreaSizeEnum::TallArea:
      old_siblings = {old_parent, old_parent + 8};
      break;
    default:
      old_siblings = {parent_index};  // Was small, just this map
      break;
  }
  
  // Reset all old siblings to SmallArea first (clean slate)
  for (int old_sibling : old_siblings) {
    if (old_sibling >= 0 && old_sibling < kNumOverworldMaps) {
      overworld_maps_[old_sibling].SetAsSmallMap(old_sibling);
    }
  }
  
  // Now configure NEW siblings based on requested size
  std::vector<int> new_siblings;
  
  switch (size) {
    case AreaSizeEnum::SmallArea:
      // Just configure this single map as small
      overworld_maps_[parent_index].SetParent(parent_index);
      overworld_maps_[parent_index].SetAreaSize(AreaSizeEnum::SmallArea);
      new_siblings = {parent_index};
      break;
      
    case AreaSizeEnum::LargeArea:
      new_siblings = {parent_index, parent_index + 1, parent_index + 8, parent_index + 9};
      for (size_t i = 0; i < new_siblings.size(); ++i) {
        int sibling = new_siblings[i];
        if (sibling < 0 || sibling >= kNumOverworldMaps) continue;
        overworld_maps_[sibling].SetAsLargeMap(parent_index, i);
      }
      break;
      
    case AreaSizeEnum::WideArea:
      new_siblings = {parent_index, parent_index + 1};
      for (int sibling : new_siblings) {
        if (sibling < 0 || sibling >= kNumOverworldMaps) continue;
        overworld_maps_[sibling].SetParent(parent_index);
        overworld_maps_[sibling].SetAreaSize(AreaSizeEnum::WideArea);
      }
      break;
      
    case AreaSizeEnum::TallArea:
      new_siblings = {parent_index, parent_index + 8};
      for (int sibling : new_siblings) {
        if (sibling < 0 || sibling >= kNumOverworldMaps) continue;
        overworld_maps_[sibling].SetParent(parent_index);
        overworld_maps_[sibling].SetAreaSize(AreaSizeEnum::TallArea);
      }
      break;
  }
  
  // Update ROM data for ALL affected siblings (old + new)
  std::set<int> all_affected;
  for (int s : old_siblings) all_affected.insert(s);
  for (int s : new_siblings) all_affected.insert(s);
  
  if (asm_version >= 3 && asm_version != 0xFF) {
    // v3+: Update expanded tables
    for (int sibling : all_affected) {
      if (sibling < 0 || sibling >= kNumOverworldMaps) continue;
      
      RETURN_IF_ERROR(rom()->WriteByte(kOverworldMapParentIdExpanded + sibling,
                                       overworld_maps_[sibling].parent()));
      RETURN_IF_ERROR(rom()->WriteByte(
          kOverworldScreenSize + sibling,
          static_cast<uint8_t>(overworld_maps_[sibling].area_size())));
    }
  } else if (asm_version < 3 && asm_version != 0xFF) {
    // v1/v2: Update basic parent table
    for (int sibling : all_affected) {
      if (sibling < 0 || sibling >= kNumOverworldMaps) continue;
      
      RETURN_IF_ERROR(rom()->WriteByte(kOverworldMapParentId + sibling,
                                       overworld_maps_[sibling].parent()));
      RETURN_IF_ERROR(rom()->WriteByte(
          kOverworldScreenSize + (sibling & 0x3F),
          static_cast<uint8_t>(overworld_maps_[sibling].area_size())));
    }
  } else {
    // Vanilla: Update parent and screen size tables
    for (int sibling : all_affected) {
      if (sibling < 0 || sibling >= kNumOverworldMaps) continue;
      
      RETURN_IF_ERROR(rom()->WriteByte(kOverworldMapParentId + sibling,
                                       overworld_maps_[sibling].parent()));
      RETURN_IF_ERROR(rom()->WriteByte(
          kOverworldScreenSize + (sibling & 0x3F),
          (overworld_maps_[sibling].area_size() == AreaSizeEnum::LargeArea) ? 0x00 : 0x01));
    }
  }
  
  LOG_INFO("Overworld", "Configured %s area: parent=%d, old_siblings=%zu, new_siblings=%zu",
           (size == AreaSizeEnum::LargeArea) ? "Large" :
           (size == AreaSizeEnum::WideArea) ? "Wide" : 
           (size == AreaSizeEnum::TallArea) ? "Tall" : "Small",
           parent_index, old_siblings.size(), new_siblings.size());
  
  return absl::OkStatus();
}

absl::StatusOr<uint16_t> Overworld::GetTile16ForTile32(
    int index, int quadrant, int dimension, const uint32_t* map32address) {
  ASSIGN_OR_RETURN(
      auto arg1, rom()->ReadByte(map32address[dimension] + quadrant + (index)));
  ASSIGN_OR_RETURN(auto arg2,
                   rom()->ReadWord(map32address[dimension] + (index) +
                                   (quadrant <= 1 ? 4 : 5)));
  return (uint16_t)(arg1 +
                    (((arg2 >> (quadrant % 2 == 0 ? 4 : 0)) & 0x0F) * 256));
}

absl::Status Overworld::AssembleMap32Tiles() {
  constexpr int kMap32TilesLength = 0x33F0;
  int num_tile32 = kMap32TilesLength;
  uint32_t map32address[4] = {rom()->version_constants().kMap32TileTL,
                              rom()->version_constants().kMap32TileTR,
                              rom()->version_constants().kMap32TileBL,
                              rom()->version_constants().kMap32TileBR};

  // Check if expanded tile32 data is actually present in ROM
  // The flag position should contain 0x04 for vanilla, something else for expanded
  uint8_t asm_version = (*rom_)[OverworldCustomASMHasBeenApplied];
  uint8_t expanded_flag = rom()->data()[kMap32ExpandedFlagPos];
  util::logf("Expanded tile32 flag: %d", expanded_flag);
  if (expanded_flag != 0x04 || asm_version >= 3) {
    // ROM has expanded tile32 data - use expanded addresses
    map32address[0] = rom()->version_constants().kMap32TileTL;
    map32address[1] = kMap32TileTRExpanded;
    map32address[2] = kMap32TileBLExpanded;
    map32address[3] = kMap32TileBRExpanded;
    num_tile32 = kMap32TileCountExpanded;
    expanded_tile32_ = true;
  }
  // Otherwise use vanilla addresses (already set above)

  // Loop through each 32x32 pixel tile in the rom
  for (int i = 0; i < num_tile32; i += 6) {
    // Loop through each quadrant of the 32x32 pixel tile.
    for (int k = 0; k < 4; k++) {
      // Generate the 16-bit tile for the current quadrant of the current
      // 32x32 pixel tile.
      ASSIGN_OR_RETURN(
          uint16_t tl,
          GetTile16ForTile32(i, k, (int)Dimension::map32TilesTL, map32address));
      ASSIGN_OR_RETURN(
          uint16_t tr,
          GetTile16ForTile32(i, k, (int)Dimension::map32TilesTR, map32address));
      ASSIGN_OR_RETURN(
          uint16_t bl,
          GetTile16ForTile32(i, k, (int)Dimension::map32TilesBL, map32address));
      ASSIGN_OR_RETURN(
          uint16_t br,
          GetTile16ForTile32(i, k, (int)Dimension::map32TilesBR, map32address));

      // Add the generated 16-bit tiles to the tiles32 vector.
      tiles32_unique_.emplace_back(gfx::Tile32(tl, tr, bl, br));
    }
  }

  map_tiles_.light_world.resize(0x200);
  map_tiles_.dark_world.resize(0x200);
  map_tiles_.special_world.resize(0x200);
  for (int i = 0; i < 0x200; i++) {
    map_tiles_.light_world[i].resize(0x200);
    map_tiles_.dark_world[i].resize(0x200);
    map_tiles_.special_world[i].resize(0x200);
  }

  return absl::OkStatus();
}

absl::Status Overworld::AssembleMap16Tiles() {
  int tpos = kMap16Tiles;
  int num_tile16 = kNumTile16Individual;

  // Check if expanded tile16 data is actually present in ROM
  // The flag position should contain 0x0F for vanilla, something else for expanded
  uint8_t asm_version = (*rom_)[OverworldCustomASMHasBeenApplied];
  uint8_t expanded_flag = rom()->data()[kMap16ExpandedFlagPos];
  util::logf("Expanded tile16 flag: %d", expanded_flag);
  if (rom()->data()[kMap16ExpandedFlagPos] == 0x0F || asm_version >= 3) {
    // ROM has expanded tile16 data - use expanded addresses
    tpos = kMap16TilesExpanded;
    num_tile16 = NumberOfMap16Ex;
    expanded_tile16_ = true;
  }
  // Otherwise use vanilla addresses (already set above)

  for (int i = 0; i < num_tile16; i += 1) {
    ASSIGN_OR_RETURN(auto t0_data, rom()->ReadWord(tpos));
    gfx::TileInfo t0 = gfx::GetTilesInfo(t0_data);
    tpos += 2;
    ASSIGN_OR_RETURN(auto t1_data, rom()->ReadWord(tpos));
    gfx::TileInfo t1 = gfx::GetTilesInfo(t1_data);
    tpos += 2;
    ASSIGN_OR_RETURN(auto t2_data, rom()->ReadWord(tpos));
    gfx::TileInfo t2 = gfx::GetTilesInfo(t2_data);
    tpos += 2;
    ASSIGN_OR_RETURN(auto t3_data, rom()->ReadWord(tpos));
    gfx::TileInfo t3 = gfx::GetTilesInfo(t3_data);
    tpos += 2;
    tiles16_.emplace_back(t0, t1, t2, t3);
  }
  return absl::OkStatus();
}

void Overworld::AssignWorldTiles(int x, int y, int sx, int sy, int tpos,
                                 OverworldBlockset& world) {
  int position_x1 = (x * 2) + (sx * 32);
  int position_y1 = (y * 2) + (sy * 32);
  int position_x2 = (x * 2) + 1 + (sx * 32);
  int position_y2 = (y * 2) + 1 + (sy * 32);
  world[position_x1][position_y1] = tiles32_unique_[tpos].tile0_;
  world[position_x2][position_y1] = tiles32_unique_[tpos].tile1_;
  world[position_x1][position_y2] = tiles32_unique_[tpos].tile2_;
  world[position_x2][position_y2] = tiles32_unique_[tpos].tile3_;
}

void Overworld::OrganizeMapTiles(std::vector<uint8_t>& bytes,
                                 std::vector<uint8_t>& bytes2, int i, int sx,
                                 int sy, int& ttpos) {
  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 16; x++) {
      auto tidD = (uint16_t)((bytes2[ttpos] << 8) + bytes[ttpos]);
      if (int tpos = tidD; tpos < tiles32_unique_.size()) {
        if (i < kDarkWorldMapIdStart) {
          AssignWorldTiles(x, y, sx, sy, tpos, map_tiles_.light_world);
        } else if (i < kSpecialWorldMapIdStart && i >= kDarkWorldMapIdStart) {
          AssignWorldTiles(x, y, sx, sy, tpos, map_tiles_.dark_world);
        } else {
          AssignWorldTiles(x, y, sx, sy, tpos, map_tiles_.special_world);
        }
      }
      ttpos += 1;
    }
  }
}

void Overworld::DecompressAllMapTiles() {
  // Keep original method for fallback/compatibility
  // Note: This method is void, so we can't return status
  // The parallel version will be called from Load()
}

absl::Status Overworld::DecompressAllMapTilesParallel() {
  // For now, fall back to the original sequential implementation
  // The parallel version has synchronization issues that cause data corruption
  util::logf("Using sequential decompression (parallel version disabled due to data integrity issues)");
  
  const auto get_ow_map_gfx_ptr = [this](int index, uint32_t map_ptr) {
    int p = (rom()->data()[map_ptr + 2 + (3 * index)] << 16) +
            (rom()->data()[map_ptr + 1 + (3 * index)] << 8) +
            (rom()->data()[map_ptr + (3 * index)]);
    return SnesToPc(p);
  };

  constexpr uint32_t kBaseLowest = 0x0FFFFF;
  constexpr uint32_t kBaseHighest = 0x0F8000;

  uint32_t lowest = kBaseLowest;
  uint32_t highest = kBaseHighest;
  int sx = 0;
  int sy = 0;
  int c = 0;
  
  for (int i = 0; i < kNumOverworldMaps; i++) {
    auto p1 = get_ow_map_gfx_ptr(
        i, rom()->version_constants().kCompressedAllMap32PointersHigh);
    auto p2 = get_ow_map_gfx_ptr(
        i, rom()->version_constants().kCompressedAllMap32PointersLow);

    int ttpos = 0;

    if (p1 >= highest)
      highest = p1;
    if (p2 >= highest)
      highest = p2;

    if (p1 <= lowest && p1 > kBaseHighest)
      lowest = p1;
    if (p2 <= lowest && p2 > kBaseHighest)
      lowest = p2;

    int size1, size2;
    auto bytes = gfx::HyruleMagicDecompress(rom()->data() + p2, &size1, 1);
    auto bytes2 = gfx::HyruleMagicDecompress(rom()->data() + p1, &size2, 1);
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

absl::Status Overworld::LoadOverworldMaps() {
  auto size = tiles16_.size();
  
  // Performance optimization: Only build essential maps initially
  // Essential maps are the first few maps of each world that are commonly accessed
  constexpr int kEssentialMapsPerWorld = 8;  // Build first 8 maps of each world
  constexpr int kLightWorldEssential = kEssentialMapsPerWorld;
  constexpr int kDarkWorldEssential = kDarkWorldMapIdStart + kEssentialMapsPerWorld;
  constexpr int kSpecialWorldEssential = kSpecialWorldMapIdStart + kEssentialMapsPerWorld;
  
  util::logf("Building essential maps only (first %d maps per world) for faster loading", kEssentialMapsPerWorld);
  
  std::vector<std::future<absl::Status>> futures;
  
  // Build essential maps only
  for (int i = 0; i < kNumOverworldMaps; ++i) {
    bool is_essential = false;
    
    // Check if this is an essential map
    if (i < kLightWorldEssential) {
      is_essential = true;
    } else if (i >= kDarkWorldMapIdStart && i < kDarkWorldEssential) {
      is_essential = true;
    } else if (i >= kSpecialWorldMapIdStart && i < kSpecialWorldEssential) {
      is_essential = true;
    }
    
    if (is_essential) {
      int world_type = 0;
      if (i >= kDarkWorldMapIdStart && i < kSpecialWorldMapIdStart) {
        world_type = 1;
      } else if (i >= kSpecialWorldMapIdStart) {
        world_type = 2;
      }
      
      auto task_function = [this, i, size, world_type]() {
        return overworld_maps_[i].BuildMap(size, game_state_, world_type,
                                           tiles16_, GetMapTiles(world_type));
      };
      futures.emplace_back(std::async(std::launch::async, task_function));
    } else {
      // Mark non-essential maps as not built yet
      overworld_maps_[i].SetNotBuilt();
    }
  }

  // Wait for essential maps to complete
  for (auto& future : futures) {
    future.wait();
    RETURN_IF_ERROR(future.get());
  }
  
  util::logf("Essential maps built. Remaining maps will be built on-demand.");
  return absl::OkStatus();
}

absl::Status Overworld::EnsureMapBuilt(int map_index) {
  if (map_index < 0 || map_index >= kNumOverworldMaps) {
    return absl::InvalidArgumentError("Invalid map index");
  }
  
  // Check if map is already built
  if (overworld_maps_[map_index].is_built()) {
    return absl::OkStatus();
  }
  
  // Build the map on-demand
  auto size = tiles16_.size();
  int world_type = 0;
  if (map_index >= kDarkWorldMapIdStart && map_index < kSpecialWorldMapIdStart) {
    world_type = 1;
  } else if (map_index >= kSpecialWorldMapIdStart) {
    world_type = 2;
  }
  
  util::logf("Building map %d on-demand", map_index);
  return overworld_maps_[map_index].BuildMap(size, game_state_, world_type,
                                             tiles16_, GetMapTiles(world_type));
}

void Overworld::LoadTileTypes() {
  for (int i = 0; i < kNumTileTypes; ++i) {
    all_tiles_types_[i] =
        rom()->data()[rom()->version_constants().kOverworldTilesType + i];
  }
}

absl::Status Overworld::LoadEntrances() {
  int ow_entrance_map_ptr = kOverworldEntranceMap;
  int ow_entrance_pos_ptr = kOverworldEntrancePos;
  int ow_entrance_id_ptr = kOverworldEntranceEntranceId;
  int num_entrances = 129;

  // Check if expanded entrance data is actually present in ROM
  // The flag position should contain 0xB8 for vanilla, something else for expanded
  if (rom()->data()[kOverworldEntranceExpandedFlagPos] != 0xB8) {
    // ROM has expanded entrance data - use expanded addresses
    ow_entrance_map_ptr = kOverworldEntranceMapExpanded;
    ow_entrance_pos_ptr = kOverworldEntrancePosExpanded;
    ow_entrance_id_ptr = kOverworldEntranceEntranceIdExpanded;
    expanded_entrances_ = true;
    num_entrances = 256;  // Expanded entrance count
  }
  // Otherwise use vanilla addresses (already set above)

  for (int i = 0; i < num_entrances; i++) {
    ASSIGN_OR_RETURN(auto map_id,
                     rom()->ReadWord(ow_entrance_map_ptr + (i * 2)));
    ASSIGN_OR_RETURN(auto map_pos,
                     rom()->ReadWord(ow_entrance_pos_ptr + (i * 2)));
    ASSIGN_OR_RETURN(auto entrance_id, rom()->ReadByte(ow_entrance_id_ptr + i));
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

  return absl::OkStatus();
}

absl::Status Overworld::LoadHoles() {
  constexpr int kNumHoles = 0x13;
  for (int i = 0; i < kNumHoles; i++) {
    ASSIGN_OR_RETURN(auto map_id,
                     rom()->ReadWord(kOverworldHoleArea + (i * 2)));
    ASSIGN_OR_RETURN(auto map_pos,
                     rom()->ReadWord(kOverworldHolePos + (i * 2)));
    ASSIGN_OR_RETURN(auto entrance_id,
                     rom()->ReadByte(kOverworldHoleEntrance + i));
    int p = (map_pos + 0x400) >> 1;
    int x = (p % 64);
    int y = (p >> 6);
    all_holes_.emplace_back(
        (x * 16) + (((map_id % 64) - (((map_id % 64) / 8) * 8)) * 512),
        (y * 16) + (((map_id % 64) / 8) * 512), entrance_id, map_id,
        (uint16_t)(map_pos + 0x400), true);
  }
  return absl::OkStatus();
}

absl::Status Overworld::LoadExits() {
  const int NumberOfOverworldExits = 0x4F;
  std::vector<OverworldExit> exits;
  for (int i = 0; i < NumberOfOverworldExits; i++) {
    auto rom_data = rom()->data();

    uint16_t exit_room_id;
    uint16_t exit_map_id;
    uint16_t exit_vram;
    uint16_t exit_y_scroll;
    uint16_t exit_x_scroll;
    uint16_t exit_y_player;
    uint16_t exit_x_player;
    uint16_t exit_y_camera;
    uint16_t exit_x_camera;
    uint16_t exit_scroll_mod_y;
    uint16_t exit_scroll_mod_x;
    uint16_t exit_door_type_1;
    uint16_t exit_door_type_2;
    RETURN_IF_ERROR(rom()->ReadTransaction(
        exit_room_id, (OWExitRoomId + (i * 2)), exit_map_id, OWExitMapId + i,
        exit_vram, OWExitVram + (i * 2), exit_y_scroll, OWExitYScroll + (i * 2),
        exit_x_scroll, OWExitXScroll + (i * 2), exit_y_player,
        OWExitYPlayer + (i * 2), exit_x_player, OWExitXPlayer + (i * 2),
        exit_y_camera, OWExitYCamera + (i * 2), exit_x_camera,
        OWExitXCamera + (i * 2), exit_scroll_mod_y, OWExitUnk1 + i,
        exit_scroll_mod_x, OWExitUnk2 + i, exit_door_type_1,
        OWExitDoorType1 + (i * 2), exit_door_type_2,
        OWExitDoorType2 + (i * 2)));

    uint16_t py = (uint16_t)((rom_data[OWExitYPlayer + (i * 2) + 1] << 8) +
                             rom_data[OWExitYPlayer + (i * 2)]);
    uint16_t px = (uint16_t)((rom_data[OWExitXPlayer + (i * 2) + 1] << 8) +
                             rom_data[OWExitXPlayer + (i * 2)]);

    exits.emplace_back(exit_room_id, exit_map_id, exit_vram, exit_y_scroll,
                       exit_x_scroll, py, px, exit_y_camera, exit_x_camera,
                       exit_scroll_mod_y, exit_scroll_mod_x, exit_door_type_1,
                       exit_door_type_2, (px & py) == 0xFFFF);
  }
  all_exits_ = exits;
  return absl::OkStatus();
}

absl::Status Overworld::LoadItems() {
  // Version 0x03 of the OW ASM added item support for the SW.
  uint8_t asm_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];

  // Determine max number of overworld maps based on actual ASM version
  // Only use expanded maps (0xA0) if v3+ ASM is actually applied
  int max_ow =
      (asm_version >= 0x03 && asm_version != 0xFF) ? kNumOverworldMaps : 0x80;

  ASSIGN_OR_RETURN(uint32_t pointer_snes,
                   rom()->ReadLong(zelda3::overworldItemsAddress));
  uint32_t item_pointer_address =
      SnesToPc(pointer_snes);  // 0x1BC2F9 -> 0x0DC2F9

  for (int i = 0; i < max_ow; i++) {
    ASSIGN_OR_RETURN(uint8_t bank_byte,
                     rom()->ReadByte(zelda3::overworldItemsAddressBank));
    int bank = bank_byte & 0x7F;

    ASSIGN_OR_RETURN(uint8_t addr_low,
                     rom()->ReadByte(item_pointer_address + (i * 2)));
    ASSIGN_OR_RETURN(uint8_t addr_high,
                     rom()->ReadByte(item_pointer_address + (i * 2) + 1));

    uint32_t addr = (bank << 16) +      // 1B
                    (addr_high << 8) +  // F9
                    addr_low;           // 3C
    addr = SnesToPc(addr);

    // Check if this is a large map and skip if not the parent
    if (overworld_maps_[i].area_size() != zelda3::AreaSizeEnum::SmallArea) {
      if (overworld_maps_[i].parent() != (uint8_t)i) {
        continue;
      }
    }

    while (true) {
      ASSIGN_OR_RETURN(uint8_t b1, rom()->ReadByte(addr));
      ASSIGN_OR_RETURN(uint8_t b2, rom()->ReadByte(addr + 1));
      ASSIGN_OR_RETURN(uint8_t b3, rom()->ReadByte(addr + 2));

      if (b1 == 0xFF && b2 == 0xFF) {
        break;
      }

      int p = (((b2 & 0x1F) << 8) + b1) >> 1;

      int x = p % 0x40;  // Use 0x40 instead of 64 to match ZS
      int y = p >> 6;

      int fakeID = i % 0x40;  // Use modulo 0x40 to match ZS

      int sy = fakeID / 8;
      int sx = fakeID - (sy * 8);

      all_items_.emplace_back(b3, (uint16_t)i, (x * 16) + (sx * 512),
                              (y * 16) + (sy * 512), false);
      auto size = all_items_.size();

      all_items_[size - 1].game_x_ = (uint8_t)x;
      all_items_[size - 1].game_y_ = (uint8_t)y;
      addr += 3;
    }
  }
  return absl::OkStatus();
}

absl::Status Overworld::LoadSprites() {
  std::vector<std::future<absl::Status>> futures;

  // Determine sprite table locations based on actual ASM version in ROM
  uint8_t asm_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];

  if (asm_version >= 3 && asm_version != 0xFF) {
    // v3: Use expanded sprite tables
    futures.emplace_back(std::async(std::launch::async, [this]() {
      return LoadSpritesFromMap(overworldSpritesBeginingExpanded, 64, 0);
    }));
    futures.emplace_back(std::async(std::launch::async, [this]() {
      return LoadSpritesFromMap(overworldSpritesZeldaExpanded, 144, 1);
    }));
    futures.emplace_back(std::async(std::launch::async, [this]() {
      return LoadSpritesFromMap(overworldSpritesAgahnimExpanded, 144, 2);
    }));
  } else {
    // Vanilla/v2: Use original sprite tables
    futures.emplace_back(std::async(std::launch::async, [this]() {
      return LoadSpritesFromMap(kOverworldSpritesBeginning, 64, 0);
    }));
    futures.emplace_back(std::async(std::launch::async, [this]() {
      return LoadSpritesFromMap(kOverworldSpritesZelda, 144, 1);
    }));
    futures.emplace_back(std::async(std::launch::async, [this]() {
      return LoadSpritesFromMap(kOverworldSpritesAgahnim, 144, 2);
    }));
  }

  for (auto& future : futures) {
    future.wait();
    RETURN_IF_ERROR(future.get());
  }
  return absl::OkStatus();
}

absl::Status Overworld::LoadSpritesFromMap(int sprites_per_gamestate_ptr,
                                           int num_maps_per_gamestate,
                                           int game_state) {
  for (int i = 0; i < num_maps_per_gamestate; i++) {
    if (map_parent_[i] != i)
      continue;

    int current_spr_ptr = sprites_per_gamestate_ptr + (i * 2);
    ASSIGN_OR_RETURN(auto word_addr, rom()->ReadWord(current_spr_ptr));
    int sprite_address = SnesToPc((0x09 << 0x10) | word_addr);
    while (true) {
      ASSIGN_OR_RETURN(uint8_t b1, rom()->ReadByte(sprite_address));
      ASSIGN_OR_RETURN(uint8_t b2, rom()->ReadByte(sprite_address + 1));
      ASSIGN_OR_RETURN(uint8_t b3, rom()->ReadByte(sprite_address + 2));
      if (b1 == 0xFF)
        break;

      int editor_map_index = i;
      if (game_state != 0) {
        if (editor_map_index >= 128)
          editor_map_index -= 128;
        else if (editor_map_index >= 64)
          editor_map_index -= 64;
      }
      int mapY = (editor_map_index / 8);
      int mapX = (editor_map_index % 8);

      int realX = ((b2 & 0x3F) * 16) + mapX * 512;
      int realY = ((b1 & 0x3F) * 16) + mapY * 512;
      all_sprites_[game_state].emplace_back(
          *overworld_maps_[i].mutable_current_graphics(), (uint8_t)i, b3,
          (uint8_t)(b2 & 0x3F), (uint8_t)(b1 & 0x3F), realX, realY);
      all_sprites_[game_state].back().Draw();

      sprite_address += 3;
    }
  }

  return absl::OkStatus();
}

absl::Status Overworld::Save(Rom* rom) {
  rom_ = rom;
  if (expanded_tile16_) {
    RETURN_IF_ERROR(SaveMap16Expanded())
  } else {
    RETURN_IF_ERROR(SaveMap16Tiles())
  }
  if (expanded_tile32_) {
    RETURN_IF_ERROR(SaveMap32Expanded())
  } else {
    RETURN_IF_ERROR(SaveMap32Tiles())
  }
  RETURN_IF_ERROR(SaveOverworldMaps())
  RETURN_IF_ERROR(SaveEntrances())
  RETURN_IF_ERROR(SaveExits())
  RETURN_IF_ERROR(SaveItems())
  RETURN_IF_ERROR(SaveMapOverlays())
  RETURN_IF_ERROR(SaveOverworldTilesType())
  RETURN_IF_ERROR(SaveAreaSpecificBGColors())
  RETURN_IF_ERROR(SaveMusic())
  RETURN_IF_ERROR(SaveAreaSizes())
  return absl::OkStatus();
}

absl::Status Overworld::SaveOverworldMaps() {
  util::logf("Saving Overworld Maps");

  // Initialize map pointers
  std::fill(map_pointers1_id.begin(), map_pointers1_id.end(), -1);
  std::fill(map_pointers2_id.begin(), map_pointers2_id.end(), -1);

  // Compress and save each map
  int pos = kOverworldCompressedMapPos;
  for (int i = 0; i < kNumOverworldMaps; i++) {
    std::vector<uint8_t> single_map_1(512);
    std::vector<uint8_t> single_map_2(512);

    // Copy tiles32 data to single_map_1 and single_map_2
    int npos = 0;
    for (int y = 0; y < 16; y++) {
      for (int x = 0; x < 16; x++) {
        auto packed = tiles32_list_[npos + (i * 256)];
        single_map_1[npos] = packed & 0xFF;         // Lower 8 bits
        single_map_2[npos] = (packed >> 8) & 0xFF;  // Next 8 bits
        npos++;
      }
    }

    int size_a, size_b;
    // Compress single_map_1 and single_map_2
    auto a = gfx::HyruleMagicCompress(single_map_1.data(), 256, &size_a, 1);
    auto b = gfx::HyruleMagicCompress(single_map_2.data(), 256, &size_b, 1);
    if (a.empty() || b.empty()) {
      return absl::AbortedError("Error compressing map gfx.");
    }

    // Save compressed data and pointers
    map_data_p1[i] = std::vector<uint8_t>(size_a);
    map_data_p2[i] = std::vector<uint8_t>(size_b);

    if ((pos + size_a) >= 0x5FE70 && (pos + size_a) <= 0x60000) {
      pos = 0x60000;
    }

    if ((pos + size_a) >= 0x6411F && (pos + size_a) <= 0x70000) {
      util::logf("Pos set to overflow region for map %s at %s",
                 std::to_string(i), util::HexLong(pos));
      pos = kOverworldMapDataOverflow;  // 0x0F8780;
    }

    const auto compare_array = [](const std::vector<uint8_t>& array1,
                                  const std::vector<uint8_t>& array2) -> bool {
      if (array1.size() != array2.size()) {
        return false;
      }

      for (size_t i = 0; i < array1.size(); i++) {
        if (array1[i] != array2[i]) {
          return false;
        }
      }

      return true;
    };

    for (int j = 0; j < i; j++) {
      if (compare_array(a, map_data_p1[j])) {
        // Reuse pointer id j for P1 (a)
        map_pointers1_id[i] = j;
      }

      if (compare_array(b, map_data_p2[j])) {
        map_pointers2_id[i] = j;
        // Reuse pointer id j for P2 (b)
      }
    }

    if (map_pointers1_id[i] == -1) {
      // Save compressed data and pointer for map1
      std::copy(a.begin(), a.end(), map_data_p1[i].begin());
      int snes_pos = PcToSnes(pos);
      map_pointers1[i] = snes_pos;
      util::logf("Saving map pointers1 and compressed data for map %s at %s",
                 util::HexByte(i), util::HexLong(snes_pos));
      RETURN_IF_ERROR(rom()->WriteLong(
          rom()->version_constants().kCompressedAllMap32PointersLow + (3 * i),
          snes_pos));
      RETURN_IF_ERROR(rom()->WriteVector(pos, a));
      pos += size_a;
    } else {
      // Save pointer for map1
      int snes_pos = map_pointers1[map_pointers1_id[i]];
      util::logf("Saving map pointers1 for map %s at %s", util::HexByte(i),
                 util::HexLong(snes_pos));
      RETURN_IF_ERROR(rom()->WriteLong(
          rom()->version_constants().kCompressedAllMap32PointersLow + (3 * i),
          snes_pos));
    }

    if ((pos + b.size()) >= 0x5FE70 && (pos + b.size()) <= 0x60000) {
      pos = 0x60000;
    }

    if ((pos + b.size()) >= 0x6411F && (pos + b.size()) <= 0x70000) {
      util::logf("Pos set to overflow region for map %s at %s",
                 util::HexByte(i), util::HexLong(pos));
      pos = kOverworldMapDataOverflow;
    }

    if (map_pointers2_id[i] == -1) {
      // Save compressed data and pointer for map2
      std::copy(b.begin(), b.end(), map_data_p2[i].begin());
      int snes_pos = PcToSnes(pos);
      map_pointers2[i] = snes_pos;
      util::logf("Saving map pointers2 and compressed data for map %s at %s",
                 util::HexByte(i), util::HexLong(snes_pos));
      RETURN_IF_ERROR(rom()->WriteLong(
          rom()->version_constants().kCompressedAllMap32PointersHigh + (3 * i),
          snes_pos));
      RETURN_IF_ERROR(rom()->WriteVector(pos, b));
      pos += size_b;
    } else {
      // Save pointer for map2
      int snes_pos = map_pointers2[map_pointers2_id[i]];
      util::logf("Saving map pointers2 for map %s at %s", util::HexByte(i),
                 util::HexLong(snes_pos));
      RETURN_IF_ERROR(rom()->WriteLong(
          rom()->version_constants().kCompressedAllMap32PointersHigh + (3 * i),
          snes_pos));
    }
  }

  // Check if too many maps data
  if (pos > kOverworldCompressedOverflowPos) {
    util::logf("Too many maps data %s", util::HexLong(pos));
    return absl::AbortedError("Too many maps data " + std::to_string(pos));
  }

  RETURN_IF_ERROR(SaveLargeMaps())
  return absl::OkStatus();
}

absl::Status Overworld::SaveLargeMaps() {
  util::logf("Saving Large Maps");

  // Check if this is a v3+ ROM to use expanded transition system
  uint8_t asm_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
  bool use_expanded_transitions = (asm_version >= 3 && asm_version != 0xFF);

  if (use_expanded_transitions) {
    // Use new v3+ complex transition system with neighbor awareness
    return SaveLargeMapsExpanded();
  }

  // Original vanilla/v2 logic preserved
  std::vector<uint8_t> checked_map;

  for (int i = 0; i < kNumMapsPerWorld; ++i) {
    int y_pos = i / 8;
    int x_pos = i % 8;
    int parent_y_pos = overworld_maps_[i].parent() / 8;
    int parent_x_pos = overworld_maps_[i].parent() % 8;

    // Always write the map parent since it should not matter
    RETURN_IF_ERROR(rom()->WriteByte(kOverworldMapParentId + i,
                                     overworld_maps_[i].parent()))

    if (std::find(checked_map.begin(), checked_map.end(), i) !=
        checked_map.end()) {
      continue;
    }

    // If it's large then save parent pos *
    // 0x200 otherwise pos * 0x200
    if (overworld_maps_[i].is_large_map()) {
      const uint8_t large_map_offsets[] = {0, 1, 8, 9};
      for (const auto& offset : large_map_offsets) {
        // Check 1
        RETURN_IF_ERROR(rom()->WriteByte(kOverworldMapSize + i + offset, 0x20));
        // Check 2
        RETURN_IF_ERROR(
            rom()->WriteByte(kOverworldMapSizeHighByte + i + offset, 0x03));
        // Check 3
        RETURN_IF_ERROR(
            rom()->WriteByte(kOverworldScreenSize + i + offset, 0x00));
        RETURN_IF_ERROR(
            rom()->WriteByte(kOverworldScreenSize + i + offset + 64, 0x00));
        // Check 4
        RETURN_IF_ERROR(rom()->WriteByte(
            kOverworldScreenSizeForLoading + i + offset, 0x04));
        RETURN_IF_ERROR(rom()->WriteByte(
            kOverworldScreenSizeForLoading + i + offset + kDarkWorldMapIdStart,
            0x04));
        RETURN_IF_ERROR(rom()->WriteByte(kOverworldScreenSizeForLoading + i +
                                             offset + kSpecialWorldMapIdStart,
                                         0x04));
      }

      // Check 5 and 6 - transition targets
      RETURN_IF_ERROR(
          rom()->WriteShort(kTransitionTargetNorth + (i * 2),
                            (uint16_t)((parent_y_pos * 0x200) - 0xE0)));
      RETURN_IF_ERROR(
          rom()->WriteShort(kTransitionTargetWest + (i * 2),
                            (uint16_t)((parent_x_pos * 0x200) - 0x100)));

      RETURN_IF_ERROR(
          rom()->WriteShort(kTransitionTargetNorth + (i * 2) + 2,
                            (uint16_t)((parent_y_pos * 0x200) - 0xE0)));
      RETURN_IF_ERROR(
          rom()->WriteShort(kTransitionTargetWest + (i * 2) + 2,
                            (uint16_t)((parent_x_pos * 0x200) - 0x100)));

      RETURN_IF_ERROR(
          rom()->WriteShort(kTransitionTargetNorth + (i * 2) + 16,
                            (uint16_t)((parent_y_pos * 0x200) - 0xE0)));
      RETURN_IF_ERROR(
          rom()->WriteShort(kTransitionTargetWest + (i * 2) + 16,
                            (uint16_t)((parent_x_pos * 0x200) - 0x100)));

      RETURN_IF_ERROR(
          rom()->WriteShort(kTransitionTargetNorth + (i * 2) + 18,
                            (uint16_t)((parent_y_pos * 0x200) - 0xE0)));
      RETURN_IF_ERROR(
          rom()->WriteShort(kTransitionTargetWest + (i * 2) + 18,
                            (uint16_t)((parent_x_pos * 0x200) - 0x100)));

      // Check 7 and 8 - transition positions
      RETURN_IF_ERROR(rom()->WriteShort(kOverworldTransitionPositionX + (i * 2),
                                        (parent_x_pos * 0x200)));
      RETURN_IF_ERROR(rom()->WriteShort(kOverworldTransitionPositionY + (i * 2),
                                        (parent_y_pos * 0x200)));

      RETURN_IF_ERROR(
          rom()->WriteShort(kOverworldTransitionPositionX + (i * 2) + 02,
                            (parent_x_pos * 0x200)));
      RETURN_IF_ERROR(
          rom()->WriteShort(kOverworldTransitionPositionY + (i * 2) + 02,
                            (parent_y_pos * 0x200)));

      RETURN_IF_ERROR(
          rom()->WriteShort(kOverworldTransitionPositionX + (i * 2) + 16,
                            (parent_x_pos * 0x200)));
      RETURN_IF_ERROR(
          rom()->WriteShort(kOverworldTransitionPositionY + (i * 2) + 16,
                            (parent_y_pos * 0x200)));

      RETURN_IF_ERROR(
          rom()->WriteShort(kOverworldTransitionPositionX + (i * 2) + 18,
                            (parent_x_pos * 0x200)));
      RETURN_IF_ERROR(
          rom()->WriteShort(kOverworldTransitionPositionY + (i * 2) + 18,
                            (parent_y_pos * 0x200)));

      // Check 9 - simple vanilla large area transitions
      RETURN_IF_ERROR(rom()->WriteShort(
          kOverworldScreenTileMapChangeByScreen1 + (i * 2) + 00, 0x0060));
      RETURN_IF_ERROR(rom()->WriteShort(
          kOverworldScreenTileMapChangeByScreen1 + (i * 2) + 02, 0x0060));

      // If parentX == 0 then lower submaps == 0x0060 too
      if (parent_x_pos == 0) {
        RETURN_IF_ERROR(rom()->WriteShort(
            kOverworldScreenTileMapChangeByScreen1 + (i * 2) + 16, 0x0060));
        RETURN_IF_ERROR(rom()->WriteShort(
            kOverworldScreenTileMapChangeByScreen1 + (i * 2) + 18, 0x0060));
      } else {
        // Otherwise lower submaps == 0x1060
        RETURN_IF_ERROR(rom()->WriteShort(
            kOverworldScreenTileMapChangeByScreen1 + (i * 2) + 16, 0x1060));
        RETURN_IF_ERROR(rom()->WriteShort(
            kOverworldScreenTileMapChangeByScreen1 + (i * 2) + 18, 0x1060));

        // If the area to the left is a large map, we don't need to add an
        // offset to it. otherwise leave it the same. Just to make sure where
        // don't try to read outside of the array.
        if ((i - 1) >= 0) {
          // If the area to the left is a large area.
          if (overworld_maps_[i - 1].is_large_map()) {
            // If the area to the left is the bottom right of a large area.
            if (overworld_maps_[i - 1].large_index() == 1) {
              RETURN_IF_ERROR(rom()->WriteShort(
                  kOverworldScreenTileMapChangeByScreen1 + (i * 2) + 16,
                  0x0060));
            }
          }
        }
      }

      // Always 0x0080
      RETURN_IF_ERROR(rom()->WriteShort(
          kOverworldScreenTileMapChangeByScreen2 + (i * 2) + 00, 0x0080));
      RETURN_IF_ERROR(rom()->WriteShort(
          kOverworldScreenTileMapChangeByScreen2 + (i * 2) + 2, 0x0080));
      // Lower always 0x1080
      RETURN_IF_ERROR(rom()->WriteShort(
          kOverworldScreenTileMapChangeByScreen2 + (i * 2) + 16, 0x1080));
      RETURN_IF_ERROR(rom()->WriteShort(
          kOverworldScreenTileMapChangeByScreen2 + (i * 2) + 18, 0x1080));

      // If the area to the right is a large map, we don't need to add an offset
      // to it. otherwise leave it the same. Just to make sure where don't try
      // to read outside of the array.
      if ((i + 2) < 64) {
        // If the area to the right is a large area.
        if (overworld_maps_[i + 2].is_large_map()) {
          // If the area to the right is the top left of a large area.
          if (overworld_maps_[i + 2].large_index() == 0) {
            RETURN_IF_ERROR(rom()->WriteShort(
                kOverworldScreenTileMapChangeByScreen2 + (i * 2) + 18, 0x0080));
          }
        }
      }

      // Always 0x1800
      RETURN_IF_ERROR(rom()->WriteShort(
          kOverworldScreenTileMapChangeByScreen3 + (i * 2), 0x1800));
      RETURN_IF_ERROR(rom()->WriteShort(
          kOverworldScreenTileMapChangeByScreen3 + (i * 2) + 16, 0x1800));
      // Right side is always 0x1840
      RETURN_IF_ERROR(rom()->WriteShort(
          kOverworldScreenTileMapChangeByScreen3 + (i * 2) + 2, 0x1840));
      RETURN_IF_ERROR(rom()->WriteShort(
          kOverworldScreenTileMapChangeByScreen3 + (i * 2) + 18, 0x1840));

      // If the area above is a large map, we don't need to add an offset to it.
      // otherwise leave it the same.
      // Just to make sure where don't try to read outside of the array.
      if (i - 8 >= 0) {
        // If the area just above us is a large area.
        if (overworld_maps_[i - 8].is_large_map()) {
          // If the area just above us is the bottom left of a large area.
          if (overworld_maps_[i - 8].large_index() == 2) {
            RETURN_IF_ERROR(rom()->WriteShort(
                kOverworldScreenTileMapChangeByScreen3 + (i * 2) + 02, 0x1800));
          }
        }
      }

      // Always 0x2000
      RETURN_IF_ERROR(rom()->WriteShort(
          kOverworldScreenTileMapChangeByScreen4 + (i * 2) + 00, 0x2000));
      RETURN_IF_ERROR(rom()->WriteShort(
          kOverworldScreenTileMapChangeByScreen4 + (i * 2) + 16, 0x2000));
      // Right side always 0x2040
      RETURN_IF_ERROR(rom()->WriteShort(
          kOverworldScreenTileMapChangeByScreen4 + (i * 2) + 2, 0x2040));
      RETURN_IF_ERROR(rom()->WriteShort(
          kOverworldScreenTileMapChangeByScreen4 + (i * 2) + 18, 0x2040));

      // If the area below is a large map, we don't need to add an offset to it.
      // otherwise leave it the same.
      // Just to make sure where don't try to read outside of the array.
      if (i + 16 < 64) {
        // If the area just below us is a large area.
        if (overworld_maps_[i + 16].is_large_map()) {
          // If the area just below us is the top left of a large area.
          if (overworld_maps_[i + 16].large_index() == 0) {
            RETURN_IF_ERROR(rom()->WriteShort(
                kOverworldScreenTileMapChangeByScreen4 + (i * 2) + 18, 0x2000));
          }
        }
      }

      checked_map.emplace_back(i);
      checked_map.emplace_back((i + 1));
      checked_map.emplace_back((i + 8));
      checked_map.emplace_back((i + 9));

    } else {
      RETURN_IF_ERROR(rom()->WriteByte(kOverworldMapSize + i, 0x00));
      RETURN_IF_ERROR(rom()->WriteByte(kOverworldMapSizeHighByte + i, 0x01));

      RETURN_IF_ERROR(rom()->WriteByte(kOverworldScreenSize + i, 0x01));
      RETURN_IF_ERROR(rom()->WriteByte(kOverworldScreenSize + i + 64, 0x01));

      RETURN_IF_ERROR(
          rom()->WriteByte(kOverworldScreenSizeForLoading + i, 0x02));
      RETURN_IF_ERROR(rom()->WriteByte(
          kOverworldScreenSizeForLoading + i + kDarkWorldMapIdStart, 0x02));
      RETURN_IF_ERROR(rom()->WriteByte(
          kOverworldScreenSizeForLoading + i + kSpecialWorldMapIdStart, 0x02));

      RETURN_IF_ERROR(rom()->WriteShort(
          kOverworldScreenTileMapChangeByScreen1 + (i * 2), 0x0060));

      // If the area to the left is a large map, we don't need to add an offset
      // to it. otherwise leave it the same.
      // Just to make sure where don't try to read outside of the array.
      if (i - 1 >= 0 && parent_x_pos != 0) {
        if (overworld_maps_[i - 1].is_large_map()) {
          if (overworld_maps_[i - 1].large_index() == 3) {
            RETURN_IF_ERROR(rom()->WriteShort(
                kOverworldScreenTileMapChangeByScreen1 + (i * 2), 0xF060));
          }
        }
      }

      RETURN_IF_ERROR(rom()->WriteShort(
          kOverworldScreenTileMapChangeByScreen2 + (i * 2), 0x0040));

      if (i + 1 < 64 && parent_x_pos != 7) {
        if (overworld_maps_[i + 1].is_large_map()) {
          if (overworld_maps_[i + 1].large_index() == 2) {
            RETURN_IF_ERROR(rom()->WriteShort(
                kOverworldScreenTileMapChangeByScreen2 + (i * 2), 0xF040));
          }
        }
      }

      RETURN_IF_ERROR(rom()->WriteShort(
          kOverworldScreenTileMapChangeByScreen3 + (i * 2), 0x1800));

      // If the area above is a large map, we don't need to add an offset to it.
      // otherwise leave it the same.
      // Just to make sure where don't try to read outside of the array.
      if (i - 8 >= 0) {
        // If the area just above us is a large area.
        if (overworld_maps_[i - 8].is_large_map()) {
          // If we are under the bottom right of the large area.
          if (overworld_maps_[i - 8].large_index() == 3) {
            RETURN_IF_ERROR(rom()->WriteShort(
                kOverworldScreenTileMapChangeByScreen3 + (i * 2), 0x17C0));
          }
        }
      }

      RETURN_IF_ERROR(rom()->WriteShort(
          kOverworldScreenTileMapChangeByScreen4 + (i * 2), 0x1000));

      // If the area below is a large map, we don't need to add an offset to it.
      // otherwise leave it the same.
      // Just to make sure where don't try to read outside of the array.
      if (i + 8 < 64) {
        // If the area just below us is a large area.
        if (overworld_maps_[i + 8].is_large_map()) {
          // If we are on top of the top right of the large area.
          if (overworld_maps_[i + 8].large_index() == 1) {
            RETURN_IF_ERROR(rom()->WriteShort(
                kOverworldScreenTileMapChangeByScreen4 + (i * 2), 0x0FC0));
          }
        }
      }

      RETURN_IF_ERROR(rom()->WriteShort(kTransitionTargetNorth + (i * 2),
                                        (uint16_t)((y_pos * 0x200) - 0xE0)));
      RETURN_IF_ERROR(rom()->WriteShort(kTransitionTargetWest + (i * 2),
                                        (uint16_t)((x_pos * 0x200) - 0x100)));

      RETURN_IF_ERROR(rom()->WriteShort(kOverworldTransitionPositionX + (i * 2),
                                        (x_pos * 0x200)));
      RETURN_IF_ERROR(rom()->WriteShort(kOverworldTransitionPositionY + (i * 2),
                                        (y_pos * 0x200)));

      checked_map.emplace_back(i);
    }
  }

  constexpr int OverworldScreenTileMapChangeMask = 0x1262C;

  RETURN_IF_ERROR(
      rom()->WriteShort(OverworldScreenTileMapChangeMask + 0, 0x1F80));
  RETURN_IF_ERROR(
      rom()->WriteShort(OverworldScreenTileMapChangeMask + 2, 0x1F80));
  RETURN_IF_ERROR(
      rom()->WriteShort(OverworldScreenTileMapChangeMask + 4, 0x007F));
  RETURN_IF_ERROR(
      rom()->WriteShort(OverworldScreenTileMapChangeMask + 6, 0x007F));

  return absl::OkStatus();
}

absl::Status Overworld::SaveSmallAreaTransitions(
    int i, int parent_x_pos, int parent_y_pos, int transition_target_north,
    int transition_target_west, int transition_pos_x, int transition_pos_y,
    int screen_change_1, int screen_change_2, int screen_change_3,
    int screen_change_4) {
  // Set basic transition targets
  RETURN_IF_ERROR(
      rom()->WriteShort(transition_target_north + (i * 2),
                        (uint16_t)((parent_y_pos * 0x0200) - 0x00E0)));
  RETURN_IF_ERROR(
      rom()->WriteShort(transition_target_west + (i * 2),
                        (uint16_t)((parent_x_pos * 0x0200) - 0x0100)));

  RETURN_IF_ERROR(
      rom()->WriteShort(transition_pos_x + (i * 2), parent_x_pos * 0x0200));
  RETURN_IF_ERROR(
      rom()->WriteShort(transition_pos_y + (i * 2), parent_y_pos * 0x0200));

  // byScreen1 = Transitioning right
  uint16_t by_screen1_small = 0x0060;

  // Check west neighbor for transition adjustments
  if ((i % 0x40) - 1 >= 0) {
    auto& west_neighbor = overworld_maps_[i - 1];

    // Transition from bottom right quadrant of large area to small area
    if (west_neighbor.area_size() == AreaSizeEnum::LargeArea &&
        west_neighbor.large_index() == 3) {
      by_screen1_small = 0xF060;
    }
    // Transition from bottom quadrant of tall area to small area
    else if (west_neighbor.area_size() == AreaSizeEnum::TallArea &&
             west_neighbor.large_index() == 2) {
      by_screen1_small = 0xF060;
    }
  }

  RETURN_IF_ERROR(
      rom()->WriteShort(screen_change_1 + (i * 2), by_screen1_small));

  // byScreen2 = Transitioning left
  uint16_t by_screen2_small = 0x0040;

  // Check east neighbor for transition adjustments
  if ((i % 0x40) + 1 < 0x40 && i + 1 < kNumOverworldMaps) {
    auto& east_neighbor = overworld_maps_[i + 1];

    // Transition from bottom left quadrant of large area to small area
    if (east_neighbor.area_size() == AreaSizeEnum::LargeArea &&
        east_neighbor.large_index() == 2) {
      by_screen2_small = 0xF040;
    }
    // Transition from bottom quadrant of tall area to small area
    else if (east_neighbor.area_size() == AreaSizeEnum::TallArea &&
             east_neighbor.large_index() == 2) {
      by_screen2_small = 0xF040;
    }
  }

  RETURN_IF_ERROR(
      rom()->WriteShort(screen_change_2 + (i * 2), by_screen2_small));

  // byScreen3 = Transitioning down
  uint16_t by_screen3_small = 0x1800;

  // Check north neighbor for transition adjustments
  if ((i % 0x40) - 8 >= 0) {
    auto& north_neighbor = overworld_maps_[i - 8];

    // Transition from bottom right quadrant of large area to small area
    if (north_neighbor.area_size() == AreaSizeEnum::LargeArea &&
        north_neighbor.large_index() == 3) {
      by_screen3_small = 0x17C0;
    }
    // Transition from right quadrant of wide area to small area
    else if (north_neighbor.area_size() == AreaSizeEnum::WideArea &&
             north_neighbor.large_index() == 1) {
      by_screen3_small = 0x17C0;
    }
  }

  RETURN_IF_ERROR(
      rom()->WriteShort(screen_change_3 + (i * 2), by_screen3_small));

  // byScreen4 = Transitioning up
  uint16_t by_screen4_small = 0x1000;

  // Check south neighbor for transition adjustments
  if ((i % 0x40) + 8 < 0x40 && i + 8 < kNumOverworldMaps) {
    auto& south_neighbor = overworld_maps_[i + 8];

    // Transition from top right quadrant of large area to small area
    if (south_neighbor.area_size() == AreaSizeEnum::LargeArea &&
        south_neighbor.large_index() == 1) {
      by_screen4_small = 0x0FC0;
    }
    // Transition from right quadrant of wide area to small area
    else if (south_neighbor.area_size() == AreaSizeEnum::WideArea &&
             south_neighbor.large_index() == 1) {
      by_screen4_small = 0x0FC0;
    }
  }

  RETURN_IF_ERROR(
      rom()->WriteShort(screen_change_4 + (i * 2), by_screen4_small));

  return absl::OkStatus();
}

absl::Status Overworld::SaveLargeAreaTransitions(
    int i, int parent_x_pos, int parent_y_pos, int transition_target_north,
    int transition_target_west, int transition_pos_x, int transition_pos_y,
    int screen_change_1, int screen_change_2, int screen_change_3,
    int screen_change_4) {
  // Set transition targets for all 4 quadrants
  const uint16_t offsets[] = {0, 2, 16, 18};
  for (auto offset : offsets) {
    RETURN_IF_ERROR(
        rom()->WriteShort(transition_target_north + (i * 2) + offset,
                          (uint16_t)((parent_y_pos * 0x0200) - 0x00E0)));
    RETURN_IF_ERROR(
        rom()->WriteShort(transition_target_west + (i * 2) + offset,
                          (uint16_t)((parent_x_pos * 0x0200) - 0x0100)));
    RETURN_IF_ERROR(rom()->WriteShort(transition_pos_x + (i * 2) + offset,
                                      parent_x_pos * 0x0200));
    RETURN_IF_ERROR(rom()->WriteShort(transition_pos_y + (i * 2) + offset,
                                      parent_y_pos * 0x0200));
  }

  // Complex neighbor-aware transition calculations for large areas
  // byScreen1 = Transitioning right
  std::array<uint16_t, 4> by_screen1_large = {0x0060, 0x0060, 0x1060, 0x1060};

  // Check west neighbor
  if ((i % 0x40) - 1 >= 0) {
    auto& west_neighbor = overworld_maps_[i - 1];

    if (west_neighbor.area_size() == AreaSizeEnum::LargeArea) {
      switch (west_neighbor.large_index()) {
        case 1:  // From bottom right to bottom left of large area
          by_screen1_large[2] = 0x0060;
          break;
        case 3:  // From bottom right to top left of large area
          by_screen1_large[0] = 0xF060;
          break;
      }
    } else if (west_neighbor.area_size() == AreaSizeEnum::TallArea) {
      switch (west_neighbor.large_index()) {
        case 0:  // From bottom of tall to bottom left of large
          by_screen1_large[2] = 0x0060;
          break;
        case 2:  // From bottom of tall to top left of large
          by_screen1_large[0] = 0xF060;
          break;
      }
    }
  }

  for (int j = 0; j < 4; j++) {
    RETURN_IF_ERROR(rom()->WriteShort(screen_change_1 + (i * 2) + offsets[j],
                                      by_screen1_large[j]));
  }

  // byScreen2 = Transitioning left
  std::array<uint16_t, 4> by_screen2_large = {0x0080, 0x0080, 0x1080, 0x1080};

  // Check east neighbor
  if ((i % 0x40) + 2 < 0x40 && i + 2 < kNumOverworldMaps) {
    auto& east_neighbor = overworld_maps_[i + 2];

    if (east_neighbor.area_size() == AreaSizeEnum::LargeArea) {
      switch (east_neighbor.large_index()) {
        case 0:  // From bottom left to bottom right of large area
          by_screen2_large[3] = 0x0080;
          break;
        case 2:  // From bottom left to top right of large area
          by_screen2_large[1] = 0xF080;
          break;
      }
    } else if (east_neighbor.area_size() == AreaSizeEnum::TallArea) {
      switch (east_neighbor.large_index()) {
        case 0:  // From bottom of tall to bottom right of large
          by_screen2_large[3] = 0x0080;
          break;
        case 2:  // From bottom of tall to top right of large
          by_screen2_large[1] = 0xF080;
          break;
      }
    }
  }

  for (int j = 0; j < 4; j++) {
    RETURN_IF_ERROR(rom()->WriteShort(screen_change_2 + (i * 2) + offsets[j],
                                      by_screen2_large[j]));
  }

  // byScreen3 = Transitioning down
  std::array<uint16_t, 4> by_screen3_large = {0x1800, 0x1840, 0x1800, 0x1840};

  // Check north neighbor
  if ((i % 0x40) - 8 >= 0) {
    auto& north_neighbor = overworld_maps_[i - 8];

    if (north_neighbor.area_size() == AreaSizeEnum::LargeArea) {
      switch (north_neighbor.large_index()) {
        case 2:  // From bottom right to top right of large area
          by_screen3_large[1] = 0x1800;
          break;
        case 3:  // From bottom right to top left of large area
          by_screen3_large[0] = 0x17C0;
          break;
      }
    } else if (north_neighbor.area_size() == AreaSizeEnum::WideArea) {
      switch (north_neighbor.large_index()) {
        case 0:  // From right of wide to top right of large
          by_screen3_large[1] = 0x1800;
          break;
        case 1:  // From right of wide to top left of large
          by_screen3_large[0] = 0x17C0;
          break;
      }
    }
  }

  for (int j = 0; j < 4; j++) {
    RETURN_IF_ERROR(rom()->WriteShort(screen_change_3 + (i * 2) + offsets[j],
                                      by_screen3_large[j]));
  }

  // byScreen4 = Transitioning up
  std::array<uint16_t, 4> by_screen4_large = {0x2000, 0x2040, 0x2000, 0x2040};

  // Check south neighbor
  if ((i % 0x40) + 16 < 0x40 && i + 16 < kNumOverworldMaps) {
    auto& south_neighbor = overworld_maps_[i + 16];

    if (south_neighbor.area_size() == AreaSizeEnum::LargeArea) {
      switch (south_neighbor.large_index()) {
        case 0:  // From top right to bottom right of large area
          by_screen4_large[3] = 0x2000;
          break;
        case 1:  // From top right to bottom left of large area
          by_screen4_large[2] = 0x1FC0;
          break;
      }
    } else if (south_neighbor.area_size() == AreaSizeEnum::WideArea) {
      switch (south_neighbor.large_index()) {
        case 0:  // From right of wide to bottom right of large
          by_screen4_large[3] = 0x2000;
          break;
        case 1:  // From right of wide to bottom left of large
          by_screen4_large[2] = 0x1FC0;
          break;
      }
    }
  }

  for (int j = 0; j < 4; j++) {
    RETURN_IF_ERROR(rom()->WriteShort(screen_change_4 + (i * 2) + offsets[j],
                                      by_screen4_large[j]));
  }

  return absl::OkStatus();
}

absl::Status Overworld::SaveWideAreaTransitions(
    int i, int parent_x_pos, int parent_y_pos, int transition_target_north,
    int transition_target_west, int transition_pos_x, int transition_pos_y,
    int screen_change_1, int screen_change_2, int screen_change_3,
    int screen_change_4) {
  // Set transition targets for both quadrants
  const uint16_t offsets[] = {0, 2};
  for (auto offset : offsets) {
    RETURN_IF_ERROR(
        rom()->WriteShort(transition_target_north + (i * 2) + offset,
                          (uint16_t)((parent_y_pos * 0x0200) - 0x00E0)));
    RETURN_IF_ERROR(
        rom()->WriteShort(transition_target_west + (i * 2) + offset,
                          (uint16_t)((parent_x_pos * 0x0200) - 0x0100)));
    RETURN_IF_ERROR(rom()->WriteShort(transition_pos_x + (i * 2) + offset,
                                      parent_x_pos * 0x0200));
    RETURN_IF_ERROR(rom()->WriteShort(transition_pos_y + (i * 2) + offset,
                                      parent_y_pos * 0x0200));
  }

  // byScreen1 = Transitioning right
  std::array<uint16_t, 2> by_screen1_wide = {0x0060, 0x0060};

  // Check west neighbor
  if ((i % 0x40) - 1 >= 0) {
    auto& west_neighbor = overworld_maps_[i - 1];

    // From bottom right of large to left of wide
    if (west_neighbor.area_size() == AreaSizeEnum::LargeArea &&
        west_neighbor.large_index() == 3) {
      by_screen1_wide[0] = 0xF060;
    }
    // From bottom of tall to left of wide
    else if (west_neighbor.area_size() == AreaSizeEnum::TallArea &&
             west_neighbor.large_index() == 2) {
      by_screen1_wide[0] = 0xF060;
    }
  }

  for (int j = 0; j < 2; j++) {
    RETURN_IF_ERROR(rom()->WriteShort(screen_change_1 + (i * 2) + offsets[j],
                                      by_screen1_wide[j]));
  }

  // byScreen2 = Transitioning left
  std::array<uint16_t, 2> by_screen2_wide = {0x0080, 0x0080};

  // Check east neighbor
  if ((i % 0x40) + 2 < 0x40 && i + 2 < kNumOverworldMaps) {
    auto& east_neighbor = overworld_maps_[i + 2];

    // From bottom left of large to right of wide
    if (east_neighbor.area_size() == AreaSizeEnum::LargeArea &&
        east_neighbor.large_index() == 2) {
      by_screen2_wide[1] = 0xF080;
    }
    // From bottom of tall to right of wide
    else if (east_neighbor.area_size() == AreaSizeEnum::TallArea &&
             east_neighbor.large_index() == 2) {
      by_screen2_wide[1] = 0xF080;
    }
  }

  for (int j = 0; j < 2; j++) {
    RETURN_IF_ERROR(rom()->WriteShort(screen_change_2 + (i * 2) + offsets[j],
                                      by_screen2_wide[j]));
  }

  // byScreen3 = Transitioning down
  std::array<uint16_t, 2> by_screen3_wide = {0x1800, 0x1840};

  // Check north neighbor
  if ((i % 0x40) - 8 >= 0) {
    auto& north_neighbor = overworld_maps_[i - 8];

    if (north_neighbor.area_size() == AreaSizeEnum::LargeArea) {
      switch (north_neighbor.large_index()) {
        case 2:  // From bottom right of large to right of wide
          by_screen3_wide[1] = 0x1800;
          break;
        case 3:  // From bottom right of large to left of wide
          by_screen3_wide[0] = 0x17C0;
          break;
      }
    } else if (north_neighbor.area_size() == AreaSizeEnum::WideArea) {
      switch (north_neighbor.large_index()) {
        case 0:  // From right of wide to right of wide
          by_screen3_wide[1] = 0x1800;
          break;
        case 1:  // From right of wide to left of wide
          by_screen3_wide[0] = 0x07C0;
          break;
      }
    }
  }

  for (int j = 0; j < 2; j++) {
    RETURN_IF_ERROR(rom()->WriteShort(screen_change_3 + (i * 2) + offsets[j],
                                      by_screen3_wide[j]));
  }

  // byScreen4 = Transitioning up
  std::array<uint16_t, 2> by_screen4_wide = {0x1000, 0x1040};

  // Check south neighbor
  if ((i % 0x40) + 8 < 0x40 && i + 8 < kNumOverworldMaps) {
    auto& south_neighbor = overworld_maps_[i + 8];

    if (south_neighbor.area_size() == AreaSizeEnum::LargeArea) {
      switch (south_neighbor.large_index()) {
        case 0:  // From top right of large to right of wide
          by_screen4_wide[1] = 0x1000;
          break;
        case 1:  // From top right of large to left of wide
          by_screen4_wide[0] = 0x0FC0;
          break;
      }
    } else if (south_neighbor.area_size() == AreaSizeEnum::WideArea) {
      if (south_neighbor.large_index() == 1) {
        by_screen4_wide[0] = 0x0FC0;
      }
      switch (south_neighbor.large_index()) {
        case 0:  // From right of wide to right of wide
          by_screen4_wide[1] = 0x1000;
          break;
        case 1:  // From right of wide to left of wide
          by_screen4_wide[0] = 0x0FC0;
          break;
      }
    }
  }

  for (int j = 0; j < 2; j++) {
    RETURN_IF_ERROR(rom()->WriteShort(screen_change_4 + (i * 2) + offsets[j],
                                      by_screen4_wide[j]));
  }

  return absl::OkStatus();
}

absl::Status Overworld::SaveTallAreaTransitions(
    int i, int parent_x_pos, int parent_y_pos, int transition_target_north,
    int transition_target_west, int transition_pos_x, int transition_pos_y,
    int screen_change_1, int screen_change_2, int screen_change_3,
    int screen_change_4) {
  // Set transition targets for both quadrants
  const uint16_t offsets[] = {0, 16};
  for (auto offset : offsets) {
    RETURN_IF_ERROR(
        rom()->WriteShort(transition_target_north + (i * 2) + offset,
                          (uint16_t)((parent_y_pos * 0x0200) - 0x00E0)));
    RETURN_IF_ERROR(
        rom()->WriteShort(transition_target_west + (i * 2) + offset,
                          (uint16_t)((parent_x_pos * 0x0200) - 0x0100)));
    RETURN_IF_ERROR(rom()->WriteShort(transition_pos_x + (i * 2) + offset,
                                      parent_x_pos * 0x0200));
    RETURN_IF_ERROR(rom()->WriteShort(transition_pos_y + (i * 2) + offset,
                                      parent_y_pos * 0x0200));
  }

  // byScreen1 = Transitioning right
  std::array<uint16_t, 2> by_screen1_tall = {0x0060, 0x1060};

  // Check west neighbor
  if ((i % 0x40) - 1 >= 0) {
    auto& west_neighbor = overworld_maps_[i - 1];

    if (west_neighbor.area_size() == AreaSizeEnum::LargeArea) {
      switch (west_neighbor.large_index()) {
        case 1:  // From bottom right of large to bottom of tall
          by_screen1_tall[1] = 0x0060;
          break;
        case 3:  // From bottom right of large to top of tall
          by_screen1_tall[0] = 0xF060;
          break;
      }
    } else if (west_neighbor.area_size() == AreaSizeEnum::TallArea) {
      switch (west_neighbor.large_index()) {
        case 0:  // From bottom of tall to bottom of tall
          by_screen1_tall[1] = 0x0060;
          break;
        case 2:  // From bottom of tall to top of tall
          by_screen1_tall[0] = 0xF060;
          break;
      }
    }
  }

  for (int j = 0; j < 2; j++) {
    RETURN_IF_ERROR(rom()->WriteShort(screen_change_1 + (i * 2) + offsets[j],
                                      by_screen1_tall[j]));
  }

  // byScreen2 = Transitioning left
  std::array<uint16_t, 2> by_screen2_tall = {0x0040, 0x1040};

  // Check east neighbor
  if ((i % 0x40) + 1 < 0x40 && i + 1 < kNumOverworldMaps) {
    auto& east_neighbor = overworld_maps_[i + 1];

    if (east_neighbor.area_size() == AreaSizeEnum::LargeArea) {
      switch (east_neighbor.large_index()) {
        case 0:  // From bottom left of large to bottom of tall
          by_screen2_tall[1] = 0x0040;
          break;
        case 2:  // From bottom left of large to top of tall
          by_screen2_tall[0] = 0xF040;
          break;
      }
    } else if (east_neighbor.area_size() == AreaSizeEnum::TallArea) {
      switch (east_neighbor.large_index()) {
        case 0:  // From bottom of tall to bottom of tall
          by_screen2_tall[1] = 0x0040;
          break;
        case 2:  // From bottom of tall to top of tall
          by_screen2_tall[0] = 0xF040;
          break;
      }
    }
  }

  for (int j = 0; j < 2; j++) {
    RETURN_IF_ERROR(rom()->WriteShort(screen_change_2 + (i * 2) + offsets[j],
                                      by_screen2_tall[j]));
  }

  // byScreen3 = Transitioning down
  std::array<uint16_t, 2> by_screen3_tall = {0x1800, 0x1800};

  // Check north neighbor
  if ((i % 0x40) - 8 >= 0) {
    auto& north_neighbor = overworld_maps_[i - 8];

    // From bottom right of large to top of tall
    if (north_neighbor.area_size() == AreaSizeEnum::LargeArea &&
        north_neighbor.large_index() == 3) {
      by_screen3_tall[0] = 0x17C0;
    }
    // From right of wide to top of tall
    else if (north_neighbor.area_size() == AreaSizeEnum::WideArea &&
             north_neighbor.large_index() == 1) {
      by_screen3_tall[0] = 0x17C0;
    }
  }

  for (int j = 0; j < 2; j++) {
    RETURN_IF_ERROR(rom()->WriteShort(screen_change_3 + (i * 2) + offsets[j],
                                      by_screen3_tall[j]));
  }

  // byScreen4 = Transitioning up
  std::array<uint16_t, 2> by_screen4_tall = {0x2000, 0x2000};

  // Check south neighbor
  if ((i % 0x40) + 16 < 0x40 && i + 16 < kNumOverworldMaps) {
    auto& south_neighbor = overworld_maps_[i + 16];

    // From top right of large to bottom of tall
    if (south_neighbor.area_size() == AreaSizeEnum::LargeArea &&
        south_neighbor.large_index() == 1) {
      by_screen4_tall[1] = 0x1FC0;
    }
    // From right of wide to bottom of tall
    else if (south_neighbor.area_size() == AreaSizeEnum::WideArea &&
             south_neighbor.large_index() == 1) {
      by_screen4_tall[1] = 0x1FC0;
    }
  }

  for (int j = 0; j < 2; j++) {
    RETURN_IF_ERROR(rom()->WriteShort(screen_change_4 + (i * 2) + offsets[j],
                                      by_screen4_tall[j]));
  }

  return absl::OkStatus();
}

absl::Status Overworld::SaveLargeMapsExpanded() {
  util::logf("Saving Large Maps (v3+ Expanded)");

  // Use expanded memory locations for v3+
  int transition_target_north = zelda3::transition_target_northExpanded;
  int transition_target_west = zelda3::transition_target_westExpanded;
  int transition_pos_x = zelda3::kOverworldTransitionPositionXExpanded;
  int transition_pos_y = zelda3::kOverworldTransitionPositionYExpanded;
  int screen_change_1 = zelda3::kOverworldScreenTileMapChangeByScreen1Expanded;
  int screen_change_2 = zelda3::kOverworldScreenTileMapChangeByScreen2Expanded;
  int screen_change_3 = zelda3::kOverworldScreenTileMapChangeByScreen3Expanded;
  int screen_change_4 = zelda3::kOverworldScreenTileMapChangeByScreen4Expanded;

  std::vector<uint8_t> checked_map;

  // Process all overworld maps (0xA0 for v3)
  for (int i = 0; i < kNumOverworldMaps; ++i) {
    // Skip if this map was already processed as part of a multi-area structure
    if (std::find(checked_map.begin(), checked_map.end(), i) !=
        checked_map.end()) {
      continue;
    }

    int parent_y_pos = (overworld_maps_[i].parent() % 0x40) / 8;
    int parent_x_pos = (overworld_maps_[i].parent() % 0x40) % 8;

    // Write the map parent ID to expanded parent table
    RETURN_IF_ERROR(rom()->WriteByte(zelda3::kOverworldMapParentIdExpanded + i,
                                     overworld_maps_[i].parent()));

    // Handle transitions based on area size
    switch (overworld_maps_[i].area_size()) {
      case AreaSizeEnum::SmallArea:
        RETURN_IF_ERROR(SaveSmallAreaTransitions(
            i, parent_x_pos, parent_y_pos, transition_target_north,
            transition_target_west, transition_pos_x, transition_pos_y,
            screen_change_1, screen_change_2, screen_change_3,
            screen_change_4));
        checked_map.emplace_back(i);
        break;

      case AreaSizeEnum::LargeArea:
        RETURN_IF_ERROR(SaveLargeAreaTransitions(
            i, parent_x_pos, parent_y_pos, transition_target_north,
            transition_target_west, transition_pos_x, transition_pos_y,
            screen_change_1, screen_change_2, screen_change_3,
            screen_change_4));
        // Mark all 4 quadrants as processed
        checked_map.emplace_back(i);
        checked_map.emplace_back(i + 1);
        checked_map.emplace_back(i + 8);
        checked_map.emplace_back(i + 9);
        break;

      case AreaSizeEnum::WideArea:
        RETURN_IF_ERROR(SaveWideAreaTransitions(
            i, parent_x_pos, parent_y_pos, transition_target_north,
            transition_target_west, transition_pos_x, transition_pos_y,
            screen_change_1, screen_change_2, screen_change_3,
            screen_change_4));
        // Mark both horizontal quadrants as processed
        checked_map.emplace_back(i);
        checked_map.emplace_back(i + 1);
        break;

      case AreaSizeEnum::TallArea:
        RETURN_IF_ERROR(SaveTallAreaTransitions(
            i, parent_x_pos, parent_y_pos, transition_target_north,
            transition_target_west, transition_pos_x, transition_pos_y,
            screen_change_1, screen_change_2, screen_change_3,
            screen_change_4));
        // Mark both vertical quadrants as processed
        checked_map.emplace_back(i);
        checked_map.emplace_back(i + 8);
        break;
    }
  }

  return absl::OkStatus();
}

namespace {
std::vector<uint64_t> GetAllTile16(OverworldMapTiles& map_tiles_) {
  std::vector<uint64_t> all_tile_16;  // Ensure it's 64 bits

  int sx = 0;
  int sy = 0;
  int c = 0;
  OverworldBlockset tiles_used;
  for (int i = 0; i < kNumOverworldMaps; i++) {
    if (i < kDarkWorldMapIdStart) {
      tiles_used = map_tiles_.light_world;
    } else if (i < kSpecialWorldMapIdStart && i >= kDarkWorldMapIdStart) {
      tiles_used = map_tiles_.dark_world;
    } else {
      tiles_used = map_tiles_.special_world;
    }

    for (int y = 0; y < 32; y += 2) {
      for (int x = 0; x < 32; x += 2) {
        gfx::Tile32 current_tile(
            tiles_used[x + (sx * 32)][y + (sy * 32)],
            tiles_used[x + 1 + (sx * 32)][y + (sy * 32)],
            tiles_used[x + (sx * 32)][y + 1 + (sy * 32)],
            tiles_used[x + 1 + (sx * 32)][y + 1 + (sy * 32)]);

        all_tile_16.emplace_back(current_tile.GetPackedValue());
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
}  // namespace

absl::Status Overworld::CreateTile32Tilemap() {
  tiles32_unique_.clear();
  tiles32_list_.clear();

  // Get all tiles16 and packs them into tiles32
  std::vector<uint64_t> all_tile_16 = GetAllTile16(map_tiles_);

  // Convert to set then back to vector
  std::set<uint64_t> unique_tiles_set(all_tile_16.begin(), all_tile_16.end());

  std::vector<uint64_t> unique_tiles(all_tile_16);
  unique_tiles.assign(unique_tiles_set.begin(), unique_tiles_set.end());

  // Create the indexed tiles list
  std::unordered_map<uint64_t, uint16_t> all_tiles_indexed;
  for (size_t tile32_id = 0; tile32_id < unique_tiles.size(); tile32_id++) {
    all_tiles_indexed.insert(
        {unique_tiles[tile32_id], static_cast<uint16_t>(tile32_id)});
  }

  // Add all tiles32 from all maps.
  // Convert all tiles32 non-unique IDs into unique array of IDs.
  for (int j = 0; j < NumberOfMap32; j++) {
    tiles32_list_.emplace_back(all_tiles_indexed[all_tile_16[j]]);
  }

  // Create the unique tiles list
  for (size_t i = 0; i < unique_tiles.size(); ++i) {
    tiles32_unique_.emplace_back(gfx::Tile32(unique_tiles[i]));
  }

  while (tiles32_unique_.size() % 4 != 0) {
    gfx::Tile32 padding_tile(0, 0, 0, 0);
    tiles32_unique_.emplace_back(padding_tile.GetPackedValue());
  }

  if (tiles32_unique_.size() > LimitOfMap32) {
    return absl::InternalError(absl::StrFormat(
        "Number of unique Tiles32: %d Out of: %d\nUnique Tile32 count exceed "
        "the limit\nThe ROM Has not been saved\nYou can fill maps with grass "
        "tiles to free some space\nOr use the option Clear DW Tiles in the "
        "Overworld Menu",
        unique_tiles.size(), LimitOfMap32));
  }

  if (core::FeatureFlags::get().kLogToConsole) {
    std::cout << "Number of unique Tiles32: " << tiles32_unique_.size()
              << " Saved:" << tiles32_unique_.size()
              << " Out of: " << LimitOfMap32 << std::endl;
  }

  int v = tiles32_unique_.size();
  for (int i = v; i < LimitOfMap32; i++) {
    gfx::Tile32 padding_tile(420, 420, 420, 420);
    tiles32_unique_.emplace_back(padding_tile.GetPackedValue());
  }

  return absl::OkStatus();
}

absl::Status Overworld::SaveMap32Expanded() {
  int bottomLeft = kMap32TileBLExpanded;
  int bottomRight = kMap32TileBRExpanded;
  int topRight = kMap32TileTRExpanded;
  int limit = 0x8A80;

  // Updates the pointers too for the tile32
  // Top Right
  RETURN_IF_ERROR(rom()->WriteLong(0x0176EC, PcToSnes(kMap32TileTRExpanded)));
  RETURN_IF_ERROR(
      rom()->WriteLong(0x0176F3, PcToSnes(kMap32TileTRExpanded + 1)));
  RETURN_IF_ERROR(
      rom()->WriteLong(0x0176FA, PcToSnes(kMap32TileTRExpanded + 2)));
  RETURN_IF_ERROR(
      rom()->WriteLong(0x017701, PcToSnes(kMap32TileTRExpanded + 3)));
  RETURN_IF_ERROR(
      rom()->WriteLong(0x017708, PcToSnes(kMap32TileTRExpanded + 4)));
  RETURN_IF_ERROR(
      rom()->WriteLong(0x01771A, PcToSnes(kMap32TileTRExpanded + 5)));

  // BottomLeft
  RETURN_IF_ERROR(rom()->WriteLong(0x01772C, PcToSnes(kMap32TileBLExpanded)));
  RETURN_IF_ERROR(
      rom()->WriteLong(0x017733, PcToSnes(kMap32TileBLExpanded + 1)));
  RETURN_IF_ERROR(
      rom()->WriteLong(0x01773A, PcToSnes(kMap32TileBLExpanded + 2)));
  RETURN_IF_ERROR(
      rom()->WriteLong(0x017741, PcToSnes(kMap32TileBLExpanded + 3)));
  RETURN_IF_ERROR(
      rom()->WriteLong(0x017748, PcToSnes(kMap32TileBLExpanded + 4)));
  RETURN_IF_ERROR(
      rom()->WriteLong(0x01775A, PcToSnes(kMap32TileBLExpanded + 5)));

  // BottomRight
  RETURN_IF_ERROR(rom()->WriteLong(0x01776C, PcToSnes(kMap32TileBRExpanded)));
  RETURN_IF_ERROR(
      rom()->WriteLong(0x017773, PcToSnes(kMap32TileBRExpanded + 1)));
  RETURN_IF_ERROR(
      rom()->WriteLong(0x01777A, PcToSnes(kMap32TileBRExpanded + 2)));
  RETURN_IF_ERROR(
      rom()->WriteLong(0x017781, PcToSnes(kMap32TileBRExpanded + 3)));
  RETURN_IF_ERROR(
      rom()->WriteLong(0x017788, PcToSnes(kMap32TileBRExpanded + 4)));
  RETURN_IF_ERROR(
      rom()->WriteLong(0x01779A, PcToSnes(kMap32TileBRExpanded + 5)));

  constexpr int kTilesPer32x32Tile = 6;
  int unique_tile_index = 0;
  int num_unique_tiles = tiles32_unique_.size();

  for (int i = 0; i < num_unique_tiles; i += kTilesPer32x32Tile) {
    if (unique_tile_index >= limit) {
      return absl::AbortedError("Too many unique tile32 definitions.");
    }

    // Top Left.
    auto top_left = rom()->version_constants().kMap32TileTL;
    RETURN_IF_ERROR(rom()->WriteByte(
        top_left + i,
        (uint8_t)(tiles32_unique_[unique_tile_index].tile0_ & 0xFF)));
    RETURN_IF_ERROR(rom()->WriteByte(
        top_left + (i + 1),
        (uint8_t)(tiles32_unique_[unique_tile_index + 1].tile0_ & 0xFF)));
    RETURN_IF_ERROR(rom()->WriteByte(
        top_left + (i + 2),
        (uint8_t)(tiles32_unique_[unique_tile_index + 2].tile0_ & 0xFF)));
    RETURN_IF_ERROR(rom()->WriteByte(
        top_left + (i + 3),
        (uint8_t)(tiles32_unique_[unique_tile_index + 3].tile0_ & 0xFF)));

    RETURN_IF_ERROR(rom()->WriteByte(
        top_left + (i + 4),
        (uint8_t)(((tiles32_unique_[unique_tile_index].tile0_ >> 4) & 0xF0) +
                  ((tiles32_unique_[unique_tile_index + 1].tile0_ >> 8) &
                   0x0F))));
    RETURN_IF_ERROR(rom()->WriteByte(
        top_left + (i + 5),
        (uint8_t)(((tiles32_unique_[unique_tile_index + 2].tile0_ >> 4) &
                   0xF0) +
                  ((tiles32_unique_[unique_tile_index + 3].tile0_ >> 8) &
                   0x0F))));

    // Top Right.
    auto top_right = topRight;
    RETURN_IF_ERROR(rom()->WriteByte(
        top_right + i,
        (uint8_t)(tiles32_unique_[unique_tile_index].tile1_ & 0xFF)));
    RETURN_IF_ERROR(rom()->WriteByte(
        top_right + (i + 1),
        (uint8_t)(tiles32_unique_[unique_tile_index + 1].tile1_ & 0xFF)));
    RETURN_IF_ERROR(rom()->WriteByte(
        top_right + (i + 2),
        (uint8_t)(tiles32_unique_[unique_tile_index + 2].tile1_ & 0xFF)));
    RETURN_IF_ERROR(rom()->WriteByte(
        top_right + (i + 3),
        (uint8_t)(tiles32_unique_[unique_tile_index + 3].tile1_ & 0xFF)));

    RETURN_IF_ERROR(rom()->WriteByte(
        top_right + (i + 4),
        (uint8_t)(((tiles32_unique_[unique_tile_index].tile1_ >> 4) & 0xF0) |
                  ((tiles32_unique_[unique_tile_index + 1].tile1_ >> 8) &
                   0x0F))));
    RETURN_IF_ERROR(rom()->WriteByte(
        top_right + (i + 5),
        (uint8_t)(((tiles32_unique_[unique_tile_index + 2].tile1_ >> 4) &
                   0xF0) |
                  ((tiles32_unique_[unique_tile_index + 3].tile1_ >> 8) &
                   0x0F))));

    // Bottom Left.
    auto bottom_left = bottomLeft;
    RETURN_IF_ERROR(rom()->WriteByte(
        bottom_left + i,
        (uint8_t)(tiles32_unique_[unique_tile_index].tile2_ & 0xFF)));
    RETURN_IF_ERROR(rom()->WriteByte(
        bottom_left + (i + 1),
        (uint8_t)(tiles32_unique_[unique_tile_index + 1].tile2_ & 0xFF)));
    RETURN_IF_ERROR(rom()->WriteByte(
        bottom_left + (i + 2),
        (uint8_t)(tiles32_unique_[unique_tile_index + 2].tile2_ & 0xFF)));
    RETURN_IF_ERROR(rom()->WriteByte(
        bottom_left + (i + 3),
        (uint8_t)(tiles32_unique_[unique_tile_index + 3].tile2_ & 0xFF)));

    RETURN_IF_ERROR(rom()->WriteByte(
        bottom_left + (i + 4),
        (uint8_t)(((tiles32_unique_[unique_tile_index].tile2_ >> 4) & 0xF0) |
                  ((tiles32_unique_[unique_tile_index + 1].tile2_ >> 8) &
                   0x0F))));
    RETURN_IF_ERROR(rom()->WriteByte(
        bottom_left + (i + 5),
        (uint8_t)(((tiles32_unique_[unique_tile_index + 2].tile2_ >> 4) &
                   0xF0) |
                  ((tiles32_unique_[unique_tile_index + 3].tile2_ >> 8) &
                   0x0F))));

    // Bottom Right.
    auto bottom_right = bottomRight;
    RETURN_IF_ERROR(rom()->WriteByte(
        bottom_right + i,
        (uint8_t)(tiles32_unique_[unique_tile_index].tile3_ & 0xFF)));
    RETURN_IF_ERROR(rom()->WriteByte(
        bottom_right + (i + 1),
        (uint8_t)(tiles32_unique_[unique_tile_index + 1].tile3_ & 0xFF)));
    RETURN_IF_ERROR(rom()->WriteByte(
        bottom_right + (i + 2),
        (uint8_t)(tiles32_unique_[unique_tile_index + 2].tile3_ & 0xFF)));
    RETURN_IF_ERROR(rom()->WriteByte(
        bottom_right + (i + 3),
        (uint8_t)(tiles32_unique_[unique_tile_index + 3].tile3_ & 0xFF)));

    RETURN_IF_ERROR(rom()->WriteByte(
        bottom_right + (i + 4),
        (uint8_t)(((tiles32_unique_[unique_tile_index].tile3_ >> 4) & 0xF0) |
                  ((tiles32_unique_[unique_tile_index + 1].tile3_ >> 8) &
                   0x0F))));
    RETURN_IF_ERROR(rom()->WriteByte(
        bottom_right + (i + 5),
        (uint8_t)(((tiles32_unique_[unique_tile_index + 2].tile3_ >> 4) &
                   0xF0) |
                  ((tiles32_unique_[unique_tile_index + 3].tile3_ >> 8) &
                   0x0F))));

    unique_tile_index += 4;
  }

  return absl::OkStatus();
}

absl::Status Overworld::SaveMap32Tiles() {
  util::logf("Saving Map32 Tiles");
  constexpr int kMaxUniqueTiles = 0x4540;
  constexpr int kTilesPer32x32Tile = 6;

  int unique_tile_index = 0;
  int num_unique_tiles = tiles32_unique_.size();

  for (int i = 0; i < num_unique_tiles; i += kTilesPer32x32Tile) {
    if (unique_tile_index >= kMaxUniqueTiles) {
      return absl::AbortedError("Too many unique tile32 definitions.");
    }

    // Top Left.
    auto top_left = rom()->version_constants().kMap32TileTL;

    RETURN_IF_ERROR(rom()->WriteByte(
        top_left + i,
        (uint8_t)(tiles32_unique_[unique_tile_index].tile0_ & 0xFF)));
    RETURN_IF_ERROR(rom()->WriteByte(
        top_left + (i + 1),
        (uint8_t)(tiles32_unique_[unique_tile_index + 1].tile0_ & 0xFF)));
    RETURN_IF_ERROR(rom()->WriteByte(
        top_left + (i + 2),
        (uint8_t)(tiles32_unique_[unique_tile_index + 2].tile0_ & 0xFF)));
    RETURN_IF_ERROR(rom()->WriteByte(
        top_left + (i + 3),
        (uint8_t)(tiles32_unique_[unique_tile_index + 3].tile0_ & 0xFF)));

    RETURN_IF_ERROR(rom()->WriteByte(
        top_left + (i + 4),
        (uint8_t)(((tiles32_unique_[unique_tile_index].tile0_ >> 4) & 0xF0) +
                  ((tiles32_unique_[unique_tile_index + 1].tile0_ >> 8) &
                   0x0F))));
    RETURN_IF_ERROR(rom()->WriteByte(
        top_left + (i + 5),
        (uint8_t)(((tiles32_unique_[unique_tile_index + 2].tile0_ >> 4) &
                   0xF0) +
                  ((tiles32_unique_[unique_tile_index + 3].tile0_ >> 8) &
                   0x0F))));

    // Top Right.
    auto top_right = rom()->version_constants().kMap32TileTR;
    RETURN_IF_ERROR(rom()->WriteByte(
        top_right + i,
        (uint8_t)(tiles32_unique_[unique_tile_index].tile1_ & 0xFF)));
    RETURN_IF_ERROR(rom()->WriteByte(
        top_right + (i + 1),
        (uint8_t)(tiles32_unique_[unique_tile_index + 1].tile1_ & 0xFF)));
    RETURN_IF_ERROR(rom()->WriteByte(
        top_right + (i + 2),
        (uint8_t)(tiles32_unique_[unique_tile_index + 2].tile1_ & 0xFF)));
    RETURN_IF_ERROR(rom()->WriteByte(
        top_right + (i + 3),
        (uint8_t)(tiles32_unique_[unique_tile_index + 3].tile1_ & 0xFF)));

    RETURN_IF_ERROR(rom()->WriteByte(
        top_right + (i + 4),
        (uint8_t)(((tiles32_unique_[unique_tile_index].tile1_ >> 4) & 0xF0) |
                  ((tiles32_unique_[unique_tile_index + 1].tile1_ >> 8) &
                   0x0F))));
    RETURN_IF_ERROR(rom()->WriteByte(
        top_right + (i + 5),
        (uint8_t)(((tiles32_unique_[unique_tile_index + 2].tile1_ >> 4) &
                   0xF0) |
                  ((tiles32_unique_[unique_tile_index + 3].tile1_ >> 8) &
                   0x0F))));

    // Bottom Left.
    const auto map32TilesBL = rom()->version_constants().kMap32TileBL;
    RETURN_IF_ERROR(rom()->WriteByte(
        map32TilesBL + i,
        (uint8_t)(tiles32_unique_[unique_tile_index].tile2_ & 0xFF)));
    RETURN_IF_ERROR(rom()->WriteByte(
        map32TilesBL + (i + 1),
        (uint8_t)(tiles32_unique_[unique_tile_index + 1].tile2_ & 0xFF)));
    RETURN_IF_ERROR(rom()->WriteByte(
        map32TilesBL + (i + 2),
        (uint8_t)(tiles32_unique_[unique_tile_index + 2].tile2_ & 0xFF)));
    RETURN_IF_ERROR(rom()->WriteByte(
        map32TilesBL + (i + 3),
        (uint8_t)(tiles32_unique_[unique_tile_index + 3].tile2_ & 0xFF)));

    RETURN_IF_ERROR(rom()->WriteByte(
        map32TilesBL + (i + 4),
        (uint8_t)(((tiles32_unique_[unique_tile_index].tile2_ >> 4) & 0xF0) |
                  ((tiles32_unique_[unique_tile_index + 1].tile2_ >> 8) &
                   0x0F))));
    RETURN_IF_ERROR(rom()->WriteByte(
        map32TilesBL + (i + 5),
        (uint8_t)(((tiles32_unique_[unique_tile_index + 2].tile2_ >> 4) &
                   0xF0) |
                  ((tiles32_unique_[unique_tile_index + 3].tile2_ >> 8) &
                   0x0F))));

    // Bottom Right.
    const auto map32TilesBR = rom()->version_constants().kMap32TileBR;
    RETURN_IF_ERROR(rom()->WriteByte(
        map32TilesBR + i,
        (uint8_t)(tiles32_unique_[unique_tile_index].tile3_ & 0xFF)));
    RETURN_IF_ERROR(rom()->WriteByte(
        map32TilesBR + (i + 1),
        (uint8_t)(tiles32_unique_[unique_tile_index + 1].tile3_ & 0xFF)));
    RETURN_IF_ERROR(rom()->WriteByte(
        map32TilesBR + (i + 2),
        (uint8_t)(tiles32_unique_[unique_tile_index + 2].tile3_ & 0xFF)));
    RETURN_IF_ERROR(rom()->WriteByte(
        map32TilesBR + (i + 3),
        (uint8_t)(tiles32_unique_[unique_tile_index + 3].tile3_ & 0xFF)));

    RETURN_IF_ERROR(rom()->WriteByte(
        map32TilesBR + (i + 4),
        (uint8_t)(((tiles32_unique_[unique_tile_index].tile3_ >> 4) & 0xF0) |
                  ((tiles32_unique_[unique_tile_index + 1].tile3_ >> 8) &
                   0x0F))));
    RETURN_IF_ERROR(rom()->WriteByte(
        map32TilesBR + (i + 5),
        (uint8_t)(((tiles32_unique_[unique_tile_index + 2].tile3_ >> 4) &
                   0xF0) |
                  ((tiles32_unique_[unique_tile_index + 3].tile3_ >> 8) &
                   0x0F))));

    unique_tile_index += 4;
    num_unique_tiles += 2;
  }

  return absl::OkStatus();
}

absl::Status Overworld::SaveMap16Expanded() {
  RETURN_IF_ERROR(
      rom()->WriteLong(SnesToPc(0x008865), PcToSnes(kMap16TilesExpanded)));
  RETURN_IF_ERROR(
      rom()->WriteLong(SnesToPc(0x0EDE4F), PcToSnes(kMap16TilesExpanded)));
  RETURN_IF_ERROR(
      rom()->WriteLong(SnesToPc(0x0EDEE9), PcToSnes(kMap16TilesExpanded)));

  RETURN_IF_ERROR(
      rom()->WriteLong(SnesToPc(0x1BBC2D), PcToSnes(kMap16TilesExpanded + 2)));
  RETURN_IF_ERROR(
      rom()->WriteLong(SnesToPc(0x1BBC4C), PcToSnes(kMap16TilesExpanded)));
  RETURN_IF_ERROR(
      rom()->WriteLong(SnesToPc(0x1BBCC2), PcToSnes(kMap16TilesExpanded + 4)));
  RETURN_IF_ERROR(
      rom()->WriteLong(SnesToPc(0x1BBCCB), PcToSnes(kMap16TilesExpanded + 6)));

  RETURN_IF_ERROR(
      rom()->WriteLong(SnesToPc(0x1BBEF6), PcToSnes(kMap16TilesExpanded)));
  RETURN_IF_ERROR(
      rom()->WriteLong(SnesToPc(0x1BBF23), PcToSnes(kMap16TilesExpanded)));
  RETURN_IF_ERROR(
      rom()->WriteLong(SnesToPc(0x1BC041), PcToSnes(kMap16TilesExpanded)));
  RETURN_IF_ERROR(
      rom()->WriteLong(SnesToPc(0x1BC9B3), PcToSnes(kMap16TilesExpanded)));

  RETURN_IF_ERROR(
      rom()->WriteLong(SnesToPc(0x1BC9BA), PcToSnes(kMap16TilesExpanded + 2)));
  RETURN_IF_ERROR(
      rom()->WriteLong(SnesToPc(0x1BC9C1), PcToSnes(kMap16TilesExpanded + 4)));
  RETURN_IF_ERROR(
      rom()->WriteLong(SnesToPc(0x1BC9C8), PcToSnes(kMap16TilesExpanded + 6)));

  RETURN_IF_ERROR(
      rom()->WriteLong(SnesToPc(0x1BCA40), PcToSnes(kMap16TilesExpanded)));
  RETURN_IF_ERROR(
      rom()->WriteLong(SnesToPc(0x1BCA47), PcToSnes(kMap16TilesExpanded + 2)));
  RETURN_IF_ERROR(
      rom()->WriteLong(SnesToPc(0x1BCA4E), PcToSnes(kMap16TilesExpanded + 4)));
  RETURN_IF_ERROR(
      rom()->WriteLong(SnesToPc(0x1BCA55), PcToSnes(kMap16TilesExpanded + 6)));

  RETURN_IF_ERROR(
      rom()->WriteLong(SnesToPc(0x02F457), PcToSnes(kMap16TilesExpanded)));
  RETURN_IF_ERROR(
      rom()->WriteLong(SnesToPc(0x02F45E), PcToSnes(kMap16TilesExpanded + 2)));
  RETURN_IF_ERROR(
      rom()->WriteLong(SnesToPc(0x02F467), PcToSnes(kMap16TilesExpanded + 4)));
  RETURN_IF_ERROR(
      rom()->WriteLong(SnesToPc(0x02F46E), PcToSnes(kMap16TilesExpanded + 6)));
  RETURN_IF_ERROR(
      rom()->WriteLong(SnesToPc(0x02F51F), PcToSnes(kMap16TilesExpanded)));
  RETURN_IF_ERROR(
      rom()->WriteLong(SnesToPc(0x02F526), PcToSnes(kMap16TilesExpanded + 4)));
  RETURN_IF_ERROR(
      rom()->WriteLong(SnesToPc(0x02F52F), PcToSnes(kMap16TilesExpanded + 2)));
  RETURN_IF_ERROR(
      rom()->WriteLong(SnesToPc(0x02F536), PcToSnes(kMap16TilesExpanded + 6)));

  RETURN_IF_ERROR(
      rom()->WriteShort(SnesToPc(0x02FE1C), PcToSnes(kMap16TilesExpanded)));
  RETURN_IF_ERROR(
      rom()->WriteShort(SnesToPc(0x02FE23), PcToSnes(kMap16TilesExpanded + 4)));
  RETURN_IF_ERROR(
      rom()->WriteShort(SnesToPc(0x02FE2C), PcToSnes(kMap16TilesExpanded + 2)));
  RETURN_IF_ERROR(
      rom()->WriteShort(SnesToPc(0x02FE33), PcToSnes(kMap16TilesExpanded + 6)));

  RETURN_IF_ERROR(rom()->WriteByte(
      SnesToPc(0x02FD28),
      static_cast<uint8_t>(PcToSnes(kMap16TilesExpanded) >> 16)));
  RETURN_IF_ERROR(rom()->WriteByte(
      SnesToPc(0x02FD39),
      static_cast<uint8_t>(PcToSnes(kMap16TilesExpanded) >> 16)));

  int tpos = kMap16TilesExpanded;
  for (int i = 0; i < NumberOfMap16Ex; i += 1)  // 4096
  {
    RETURN_IF_ERROR(
        rom()->WriteShort(tpos, TileInfoToShort(tiles16_[i].tile0_)));
    tpos += 2;
    RETURN_IF_ERROR(
        rom()->WriteShort(tpos, TileInfoToShort(tiles16_[i].tile1_)));
    tpos += 2;
    RETURN_IF_ERROR(
        rom()->WriteShort(tpos, TileInfoToShort(tiles16_[i].tile2_)));
    tpos += 2;
    RETURN_IF_ERROR(
        rom()->WriteShort(tpos, TileInfoToShort(tiles16_[i].tile3_)));
    tpos += 2;
  }

  return absl::OkStatus();
}

absl::Status Overworld::SaveMap16Tiles() {
  util::logf("Saving Map16 Tiles");
  int tpos = kMap16Tiles;
  // 3760
  for (int i = 0; i < NumberOfMap16; i += 1) {
    RETURN_IF_ERROR(
        rom()->WriteShort(tpos, TileInfoToShort(tiles16_[i].tile0_)))
    tpos += 2;
    RETURN_IF_ERROR(
        rom()->WriteShort(tpos, TileInfoToShort(tiles16_[i].tile1_)))
    tpos += 2;
    RETURN_IF_ERROR(
        rom()->WriteShort(tpos, TileInfoToShort(tiles16_[i].tile2_)))
    tpos += 2;
    RETURN_IF_ERROR(
        rom()->WriteShort(tpos, TileInfoToShort(tiles16_[i].tile3_)))
    tpos += 2;
  }
  return absl::OkStatus();
}

absl::Status Overworld::SaveEntrances() {
  util::logf("Saving Entrances");

  // Use expanded entrance tables if available
  if (expanded_entrances_) {
    for (int i = 0; i < kNumOverworldEntrances; i++) {
      RETURN_IF_ERROR(rom()->WriteShort(kOverworldEntranceMapExpanded + (i * 2),
                                        all_entrances_[i].map_id_))
      RETURN_IF_ERROR(rom()->WriteShort(kOverworldEntrancePosExpanded + (i * 2),
                                        all_entrances_[i].map_pos_))
      RETURN_IF_ERROR(rom()->WriteByte(kOverworldEntranceEntranceIdExpanded + i,
                                       all_entrances_[i].entrance_id_))
    }
  } else {
    for (int i = 0; i < kNumOverworldEntrances; i++) {
      RETURN_IF_ERROR(rom()->WriteShort(kOverworldEntranceMap + (i * 2),
                                        all_entrances_[i].map_id_))
      RETURN_IF_ERROR(rom()->WriteShort(kOverworldEntrancePos + (i * 2),
                                        all_entrances_[i].map_pos_))
      RETURN_IF_ERROR(rom()->WriteByte(kOverworldEntranceEntranceId + i,
                                       all_entrances_[i].entrance_id_))
    }
  }

  for (int i = 0; i < kNumOverworldHoles; i++) {
    RETURN_IF_ERROR(
        rom()->WriteShort(kOverworldHoleArea + (i * 2), all_holes_[i].map_id_))
    RETURN_IF_ERROR(
        rom()->WriteShort(kOverworldHolePos + (i * 2), all_holes_[i].map_pos_))
    RETURN_IF_ERROR(rom()->WriteByte(kOverworldHoleEntrance + i,
                                     all_holes_[i].entrance_id_))
  }

  return absl::OkStatus();
}

absl::Status Overworld::SaveExits() {
  util::logf("Saving Exits");

  // ASM version 0x03 added SW support and the exit leading to Zora's Domain specifically
  // needs to be updated because its camera values are incorrect.
  // We only update it if it was a vanilla ROM though because we don't know if the
  // user has already adjusted it or not.
  uint8_t asm_version = (*rom_)[OverworldCustomASMHasBeenApplied];
  if (asm_version == 0x00) {
    // Apply special fix for Zora's Domain exit (index 0x4D)
    // TODO: Implement SpecialUpdatePosition for OverworldExit
    // if (all_exits_.size() > 0x4D) {
    //   all_exits_[0x4D].SpecialUpdatePosition();
    // }
  }

  for (int i = 0; i < kNumOverworldExits; i++) {
    RETURN_IF_ERROR(
        rom()->WriteShort(OWExitRoomId + (i * 2), all_exits_[i].room_id_));
    RETURN_IF_ERROR(rom()->WriteByte(OWExitMapId + i, all_exits_[i].map_id_));
    RETURN_IF_ERROR(
        rom()->WriteShort(OWExitVram + (i * 2), all_exits_[i].map_pos_));
    RETURN_IF_ERROR(
        rom()->WriteShort(OWExitYScroll + (i * 2), all_exits_[i].y_scroll_));
    RETURN_IF_ERROR(
        rom()->WriteShort(OWExitXScroll + (i * 2), all_exits_[i].x_scroll_));
    RETURN_IF_ERROR(
        rom()->WriteByte(OWExitYPlayer + (i * 2), all_exits_[i].y_player_));
    RETURN_IF_ERROR(
        rom()->WriteByte(OWExitXPlayer + (i * 2), all_exits_[i].x_player_));
    RETURN_IF_ERROR(
        rom()->WriteByte(OWExitYCamera + (i * 2), all_exits_[i].y_camera_));
    RETURN_IF_ERROR(
        rom()->WriteByte(OWExitXCamera + (i * 2), all_exits_[i].x_camera_));
    RETURN_IF_ERROR(
        rom()->WriteByte(OWExitUnk1 + i, all_exits_[i].scroll_mod_y_));
    RETURN_IF_ERROR(
        rom()->WriteByte(OWExitUnk2 + i, all_exits_[i].scroll_mod_x_));
    RETURN_IF_ERROR(rom()->WriteShort(OWExitDoorType1 + (i * 2),
                                      all_exits_[i].door_type_1_));
    RETURN_IF_ERROR(rom()->WriteShort(OWExitDoorType2 + (i * 2),
                                      all_exits_[i].door_type_2_));
  }

  return absl::OkStatus();
}

namespace {
bool CompareItemsArrays(std::vector<OverworldItem> item_array1,
                        std::vector<OverworldItem> item_array2) {
  if (item_array1.size() != item_array2.size()) {
    return false;
  }

  bool match;
  for (size_t i = 0; i < item_array1.size(); i++) {
    match = false;
    for (size_t j = 0; j < item_array2.size(); j++) {
      // Check all sprite in 2nd array if one match
      if (item_array1[i].x_ == item_array2[j].x_ &&
          item_array1[i].y_ == item_array2[j].y_ &&
          item_array1[i].id_ == item_array2[j].id_) {
        match = true;
        break;
      }
    }

    if (!match) {
      return false;
    }
  }

  return true;
}
}  // namespace

absl::Status Overworld::SaveItems() {
  std::vector<std::vector<OverworldItem>> room_items(
      kNumOverworldMapItemPointers);

  for (int i = 0; i < kNumOverworldMapItemPointers; i++) {
    room_items[i] = std::vector<OverworldItem>();
    for (const OverworldItem& item : all_items_) {
      if (item.room_map_id_ == i) {
        room_items[i].emplace_back(item);
        if (item.id_ == 0x86) {
          RETURN_IF_ERROR(rom()->WriteWord(
              0x16DC5 + (i * 2), (item.game_x_ + (item.game_y_ * 64)) * 2));
        }
      }
    }
  }

  int data_pos = kOverworldItemsPointers + 0x100;
  int item_pointers[kNumOverworldMapItemPointers];
  int item_pointers_reuse[kNumOverworldMapItemPointers];
  int empty_pointer = 0;
  for (int i = 0; i < kNumOverworldMapItemPointers; i++) {
    item_pointers_reuse[i] = -1;
    for (int ci = 0; ci < i; ci++) {
      if (room_items[i].empty()) {
        item_pointers_reuse[i] = -2;
        break;
      }

      // Copy into separator vectors from i to ci, then ci to end
      if (CompareItemsArrays(
              std::vector<OverworldItem>(room_items[i].begin(),
                                         room_items[i].end()),
              std::vector<OverworldItem>(room_items[ci].begin(),
                                         room_items[ci].end()))) {
        item_pointers_reuse[i] = ci;
        break;
      }
    }
  }

  for (int i = 0; i < kNumOverworldMapItemPointers; i++) {
    if (item_pointers_reuse[i] == -1) {
      item_pointers[i] = data_pos;
      for (const OverworldItem& item : room_items[i]) {
        short map_pos =
            static_cast<short>(((item.game_y_ << 6) + item.game_x_) << 1);

        uint32_t data = static_cast<uint8_t>(map_pos & 0xFF) |
                        static_cast<uint8_t>(map_pos >> 8) |
                        static_cast<uint8_t>(item.id_);
        RETURN_IF_ERROR(rom()->WriteLong(data_pos, data));
        data_pos += 3;
      }

      empty_pointer = data_pos;
      RETURN_IF_ERROR(rom()->WriteWord(data_pos, 0xFFFF));
      data_pos += 2;
    } else if (item_pointers_reuse[i] == -2) {
      item_pointers[i] = empty_pointer;
    } else {
      item_pointers[i] = item_pointers[item_pointers_reuse[i]];
    }

    int snesaddr = PcToSnes(item_pointers[i]);
    RETURN_IF_ERROR(
        rom()->WriteWord(kOverworldItemsPointers + (i * 2), snesaddr));
  }

  if (data_pos > kOverworldItemsEndData) {
    return absl::AbortedError("Too many items");
  }

  util::logf("End of Items : %d", data_pos);

  return absl::OkStatus();
}

absl::Status Overworld::SaveMapOverlays() {
  util::logf("Saving Map Overlays");

  // Generate the new overlay code that handles interactive overlays
  std::vector<uint8_t> new_overlay_code = {
      0xC2, 0x30,              // REP #$30
      0xA5, 0x8A,              // LDA $8A
      0x0A, 0x18,              // ASL : CLC
      0x65, 0x8A,              // ADC $8A
      0xAA,                    // TAX
      0xBF, 0x00, 0x00, 0x00,  // LDA, X
      0x85, 0x00,              // STA $00
      0xBF, 0x00, 0x00, 0x00,  // LDA, X +2
      0x85, 0x02,              // STA $02
      0x4B,                    // PHK
      0xF4, 0x00, 0x00,        // This position +3 ?
      0xDC, 0x00, 0x00,        // JML [$00 00]
      0xE2, 0x30,              // SEP #$30
      0xAB,                    // PLB
      0x6B,                    // RTL
  };

  // Write overlay code to ROM
  constexpr int kOverlayCodeStart = 0x077657;
  RETURN_IF_ERROR(rom()->WriteVector(kOverlayCodeStart, new_overlay_code));

  // Set up overlay pointers
  int ptr_start = kOverlayCodeStart + 0x20;
  int snes_ptr_start = PcToSnes(ptr_start);

  // Write overlay pointer addresses in the code
  RETURN_IF_ERROR(rom()->WriteLong(kOverlayCodeStart + 10, snes_ptr_start));
  RETURN_IF_ERROR(rom()->WriteLong(kOverlayCodeStart + 16, snes_ptr_start + 2));

  int pea_addr = PcToSnes(kOverlayCodeStart + 27);
  RETURN_IF_ERROR(rom()->WriteShort(kOverlayCodeStart + 23, pea_addr));

  // Write overlay data to expanded space
  constexpr int kExpandedOverlaySpace = 0x120000;
  int pos = kExpandedOverlaySpace;
  int ptr_pos = kOverlayCodeStart + 32;

  for (int i = 0; i < kNumOverworldMaps; i++) {
    int snes_addr = PcToSnes(pos);
    RETURN_IF_ERROR(rom()->WriteLong(ptr_pos, snes_addr & 0xFFFFFF));
    ptr_pos += 3;

    // Write overlay data for each map that has overlays
    if (overworld_maps_[i].has_overlay()) {
      const auto& overlay_data = overworld_maps_[i].overlay_data();
      for (size_t t = 0; t < overlay_data.size(); t += 3) {
        if (t + 2 < overlay_data.size()) {
          // Generate LDA/STA sequence for each overlay tile
          RETURN_IF_ERROR(rom()->WriteByte(pos, 0xA9));  // LDA #$
          RETURN_IF_ERROR(rom()->WriteShort(
              pos + 1, overlay_data[t] | (overlay_data[t + 1] << 8)));
          pos += 3;

          RETURN_IF_ERROR(rom()->WriteByte(pos, 0x8D));  // STA $xxxx
          RETURN_IF_ERROR(rom()->WriteShort(pos + 1, overlay_data[t + 2]));
          pos += 3;
        }
      }
    }

    RETURN_IF_ERROR(rom()->WriteByte(pos, 0x6B));  // RTL
    pos++;
  }

  return absl::OkStatus();
}

absl::Status Overworld::SaveOverworldTilesType() {
  util::logf("Saving Overworld Tiles Types");

  for (int i = 0; i < kNumTileTypes; i++) {
    RETURN_IF_ERROR(
        rom()->WriteByte(overworldTilesType + i, all_tiles_types_[i]));
  }

  return absl::OkStatus();
}

absl::Status Overworld::SaveCustomOverworldASM(bool enable_bg_color,
                                               bool enable_main_palette,
                                               bool enable_mosaic,
                                               bool enable_gfx_groups,
                                               bool enable_subscreen_overlay,
                                               bool enable_animated) {
  util::logf("Applying Custom Overworld ASM");

  // Set the enable/disable settings
  uint8_t enable_value = enable_bg_color ? 0xFF : 0x00;
  RETURN_IF_ERROR(
      rom()->WriteByte(OverworldCustomAreaSpecificBGEnabled, enable_value));

  enable_value = enable_main_palette ? 0xFF : 0x00;
  RETURN_IF_ERROR(
      rom()->WriteByte(OverworldCustomMainPaletteEnabled, enable_value));

  enable_value = enable_mosaic ? 0xFF : 0x00;
  RETURN_IF_ERROR(rom()->WriteByte(OverworldCustomMosaicEnabled, enable_value));

  enable_value = enable_gfx_groups ? 0xFF : 0x00;
  RETURN_IF_ERROR(
      rom()->WriteByte(OverworldCustomTileGFXGroupEnabled, enable_value));

  enable_value = enable_animated ? 0xFF : 0x00;
  RETURN_IF_ERROR(
      rom()->WriteByte(OverworldCustomAnimatedGFXEnabled, enable_value));

  enable_value = enable_subscreen_overlay ? 0xFF : 0x00;
  RETURN_IF_ERROR(
      rom()->WriteByte(OverworldCustomSubscreenOverlayEnabled, enable_value));

  // Write the main palette table
  for (int i = 0; i < kNumOverworldMaps; i++) {
    RETURN_IF_ERROR(rom()->WriteByte(OverworldCustomMainPaletteArray + i,
                                     overworld_maps_[i].main_palette()));
  }

  // Write the mosaic table
  for (int i = 0; i < kNumOverworldMaps; i++) {
    const auto& mosaic = overworld_maps_[i].mosaic_expanded();
    // .... udlr bit format
    uint8_t mosaic_byte = (mosaic[0] ? 0x08 : 0x00) |  // up
                          (mosaic[1] ? 0x04 : 0x00) |  // down
                          (mosaic[2] ? 0x02 : 0x00) |  // left
                          (mosaic[3] ? 0x01 : 0x00);   // right

    RETURN_IF_ERROR(
        rom()->WriteByte(OverworldCustomMosaicArray + i, mosaic_byte));
  }

  // Write the main and animated gfx tiles table
  for (int i = 0; i < kNumOverworldMaps; i++) {
    for (int j = 0; j < 8; j++) {
      RETURN_IF_ERROR(
          rom()->WriteByte(OverworldCustomTileGFXGroupArray + (i * 8) + j,
                           overworld_maps_[i].custom_tileset(j)));
    }
    RETURN_IF_ERROR(rom()->WriteByte(OverworldCustomAnimatedGFXArray + i,
                                     overworld_maps_[i].animated_gfx()));
  }

  // Write the subscreen overlay table
  for (int i = 0; i < kNumOverworldMaps; i++) {
    RETURN_IF_ERROR(
        rom()->WriteShort(OverworldCustomSubscreenOverlayArray + (i * 2),
                          overworld_maps_[i].subscreen_overlay()));
  }

  return absl::OkStatus();
}

absl::Status Overworld::SaveAreaSpecificBGColors() {
  util::logf("Saving Area Specific Background Colors");

  // Write area-specific background colors if enabled
  for (int i = 0; i < kNumOverworldMaps; i++) {
    uint16_t bg_color = overworld_maps_[i].area_specific_bg_color();
    RETURN_IF_ERROR(rom()->WriteShort(
        OverworldCustomAreaSpecificBGPalette + (i * 2), bg_color));
  }

  return absl::OkStatus();
}

absl::Status Overworld::SaveMapProperties() {
  util::logf("Saving Map Properties");
  for (int i = 0; i < kDarkWorldMapIdStart; i++) {
    RETURN_IF_ERROR(rom()->WriteByte(kAreaGfxIdPtr + i,
                                     overworld_maps_[i].area_graphics()));
    RETURN_IF_ERROR(rom()->WriteByte(kOverworldMapPaletteIds + i,
                                     overworld_maps_[i].area_palette()));
    RETURN_IF_ERROR(rom()->WriteByte(kOverworldSpriteset + i,
                                     overworld_maps_[i].sprite_graphics(0)));
    RETURN_IF_ERROR(
        rom()->WriteByte(kOverworldSpriteset + kDarkWorldMapIdStart + i,
                         overworld_maps_[i].sprite_graphics(1)));
    RETURN_IF_ERROR(
        rom()->WriteByte(kOverworldSpriteset + kSpecialWorldMapIdStart + i,
                         overworld_maps_[i].sprite_graphics(2)));
    RETURN_IF_ERROR(rom()->WriteByte(kOverworldSpritePaletteIds + i,
                                     overworld_maps_[i].sprite_palette(0)));
    RETURN_IF_ERROR(
        rom()->WriteByte(kOverworldSpritePaletteIds + kDarkWorldMapIdStart + i,
                         overworld_maps_[i].sprite_palette(1)));
    RETURN_IF_ERROR(rom()->WriteByte(
        kOverworldSpritePaletteIds + kSpecialWorldMapIdStart + i,
        overworld_maps_[i].sprite_palette(2)));
  }

  for (int i = kDarkWorldMapIdStart; i < kSpecialWorldMapIdStart; i++) {
    RETURN_IF_ERROR(rom()->WriteByte(kAreaGfxIdPtr + i,
                                     overworld_maps_[i].area_graphics()));
    RETURN_IF_ERROR(rom()->WriteByte(kOverworldSpriteset + i,
                                     overworld_maps_[i].sprite_graphics(0)));
    RETURN_IF_ERROR(
        rom()->WriteByte(kOverworldSpriteset + kDarkWorldMapIdStart + i,
                         overworld_maps_[i].sprite_graphics(1)));
    RETURN_IF_ERROR(
        rom()->WriteByte(kOverworldSpriteset + kSpecialWorldMapIdStart + i,
                         overworld_maps_[i].sprite_graphics(2)));
    RETURN_IF_ERROR(rom()->WriteByte(kOverworldMapPaletteIds + i,
                                     overworld_maps_[i].area_palette()));
    RETURN_IF_ERROR(
        rom()->WriteByte(kOverworldSpritePaletteIds + kDarkWorldMapIdStart + i,
                         overworld_maps_[i].sprite_palette(0)));
    RETURN_IF_ERROR(rom()->WriteByte(
        kOverworldSpritePaletteIds + kSpecialWorldMapIdStart + i,
        overworld_maps_[i].sprite_palette(1)));
    RETURN_IF_ERROR(rom()->WriteByte(kOverworldSpritePaletteIds + 192 + i,
                                     overworld_maps_[i].sprite_palette(2)));
  }

  return absl::OkStatus();
}

absl::Status Overworld::SaveMusic() {
  util::logf("Saving Music Data");

  // Save music data for Light World maps
  for (int i = 0; i < kDarkWorldMapIdStart; i++) {
    RETURN_IF_ERROR(rom()->WriteByte(kOverworldMusicBeginning + i,
                                     overworld_maps_[i].area_music(0)));
    RETURN_IF_ERROR(rom()->WriteByte(kOverworldMusicZelda + i,
                                     overworld_maps_[i].area_music(1)));
    RETURN_IF_ERROR(rom()->WriteByte(kOverworldMusicMasterSword + i,
                                     overworld_maps_[i].area_music(2)));
    RETURN_IF_ERROR(rom()->WriteByte(kOverworldMusicAgahnim + i,
                                     overworld_maps_[i].area_music(3)));
  }

  // Save music data for Dark World maps
  for (int i = kDarkWorldMapIdStart; i < kSpecialWorldMapIdStart; i++) {
    RETURN_IF_ERROR(
        rom()->WriteByte(kOverworldMusicDarkWorld + (i - kDarkWorldMapIdStart),
                         overworld_maps_[i].area_music(0)));
  }

  return absl::OkStatus();
}

absl::Status Overworld::SaveAreaSizes() {
  util::logf("Saving V3 Area Sizes");

  // Check if this is a v3 ROM
  uint8_t asm_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
  if (asm_version < 3 || asm_version == 0xFF) {
    return absl::OkStatus();  // Not a v3 ROM, nothing to do
  }

  // Save area sizes to the expanded table
  for (int i = 0; i < kNumOverworldMaps; i++) {
    uint8_t area_size_byte =
        static_cast<uint8_t>(overworld_maps_[i].area_size());
    RETURN_IF_ERROR(rom()->WriteByte(kOverworldScreenSize + i, area_size_byte));
  }

  // Save message IDs to expanded table
  for (int i = 0; i < kNumOverworldMaps; i++) {
    uint16_t message_id = overworld_maps_[i].message_id();
    RETURN_IF_ERROR(
        rom()->WriteShort(kOverworldMessagesExpanded + (i * 2), message_id));
  }

  return absl::OkStatus();
}

}  // namespace zelda3
}  // namespace yaze
