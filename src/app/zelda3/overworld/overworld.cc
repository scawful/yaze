#include "overworld.h"

#include <algorithm>
#include <fstream>
#include <future>
#include <memory>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "app/core/constants.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/compression.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"
#include "app/zelda3/overworld/overworld_map.h"
#include "app/zelda3/sprite/sprite.h"

namespace yaze {
namespace app {
namespace zelda3 {
namespace overworld {

absl::Status Overworld::Load(Rom &rom) {
  rom_ = rom;

  RETURN_IF_ERROR(AssembleMap32Tiles());
  AssembleMap16Tiles();
  RETURN_IF_ERROR(DecompressAllMapTiles())

  const bool load_custom_overworld = flags()->overworld.kLoadCustomOverworld;
  for (int map_index = 0; map_index < kNumOverworldMaps; ++map_index)
    overworld_maps_.emplace_back(map_index, rom_, load_custom_overworld);

  FetchLargeMaps();
  LoadEntrances();
  RETURN_IF_ERROR(LoadExits());
  RETURN_IF_ERROR(LoadItems());
  RETURN_IF_ERROR(LoadSprites());
  RETURN_IF_ERROR(LoadOverworldMaps())

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

  std::vector<bool> map_checked;
  map_checked.reserve(0x40);
  for (int i = 0; i < 64; i++) {
    map_checked[i] = false;
  }
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

absl::StatusOr<uint16_t> Overworld::GetTile16ForTile32(int index, int quadrant,
                                                       int dimension) {
  const uint32_t map32address[4] = {rom_.version_constants().kMap32TileTL,
                                    rom_.version_constants().kMap32TileTR,
                                    rom_.version_constants().kMap32TileBL,
                                    rom_.version_constants().kMap32TileBR};
  ASSIGN_OR_RETURN(auto arg1,
                   rom_.ReadByte(map32address[dimension] + quadrant + (index)));
  ASSIGN_OR_RETURN(auto arg2, rom_.ReadWord(map32address[dimension] + (index) +
                                            (quadrant <= 1 ? 4 : 5)));
  return (uint16_t)(arg1 +
                    (((arg2 >> (quadrant % 2 == 0 ? 4 : 0)) & 0x0F) * 256));
}

constexpr int kMap32TilesLength = 0x33F0;

absl::Status Overworld::AssembleMap32Tiles() {
  // Loop through each 32x32 pixel tile in the rom
  for (int i = 0; i < kMap32TilesLength; i += 6) {
    // Loop through each quadrant of the 32x32 pixel tile.
    for (int k = 0; k < 4; k++) {
      // Generate the 16-bit tile for the current quadrant of the current
      // 32x32 pixel tile.
      ASSIGN_OR_RETURN(uint16_t tl,
                       GetTile16ForTile32(i, k, (int)Dimension::map32TilesTL));
      ASSIGN_OR_RETURN(uint16_t tr,
                       GetTile16ForTile32(i, k, (int)Dimension::map32TilesTR));
      ASSIGN_OR_RETURN(uint16_t bl,
                       GetTile16ForTile32(i, k, (int)Dimension::map32TilesBL));
      ASSIGN_OR_RETURN(uint16_t br,
                       GetTile16ForTile32(i, k, (int)Dimension::map32TilesBR));

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

void Overworld::AssembleMap16Tiles() {
  int tpos = kMap16Tiles;
  for (int i = 0; i < kNumTile16Individual; i += 1) {
    gfx::TileInfo t0 = gfx::GetTilesInfo(rom()->toint16(tpos));
    tpos += 2;
    gfx::TileInfo t1 = gfx::GetTilesInfo(rom()->toint16(tpos));
    tpos += 2;
    gfx::TileInfo t2 = gfx::GetTilesInfo(rom()->toint16(tpos));
    tpos += 2;
    gfx::TileInfo t3 = gfx::GetTilesInfo(rom()->toint16(tpos));
    tpos += 2;
    tiles16_.emplace_back(t0, t1, t2, t3);
  }
}

void Overworld::AssignWorldTiles(int x, int y, int sx, int sy, int tpos,
                                 OWBlockset &world) {
  int position_x1 = (x * 2) + (sx * 32);
  int position_y1 = (y * 2) + (sy * 32);
  int position_x2 = (x * 2) + 1 + (sx * 32);
  int position_y2 = (y * 2) + 1 + (sy * 32);
  world[position_x1][position_y1] = tiles32_unique_[tpos].tile0_;
  world[position_x2][position_y1] = tiles32_unique_[tpos].tile1_;
  world[position_x1][position_y2] = tiles32_unique_[tpos].tile2_;
  world[position_x2][position_y2] = tiles32_unique_[tpos].tile3_;
}

void Overworld::OrganizeMapTiles(std::vector<uint8_t> &bytes,
                                 std::vector<uint8_t> &bytes2, int i, int sx,
                                 int sy, int &ttpos) {
  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 16; x++) {
      auto tidD = (uint16_t)((bytes2[ttpos] << 8) + bytes[ttpos]);
      if (int tpos = tidD; tpos < tiles32_unique_.size()) {
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
  const auto get_ow_map_gfx_ptr = [this](int index, uint32_t map_ptr) {
    int p = (rom()->data()[map_ptr + 2 + (3 * index)] << 16) +
            (rom()->data()[map_ptr + 1 + (3 * index)] << 8) +
            (rom()->data()[map_ptr + (3 * index)]);
    return core::SnesToPc(p);
  };

  uint32_t lowest = 0x0FFFFF;
  uint32_t highest = 0x0F8000;
  int sx = 0;
  int sy = 0;
  int c = 0;
  for (int i = 0; i < 160; i++) {
    auto p1 = get_ow_map_gfx_ptr(
        i, rom()->version_constants().kCompressedAllMap32PointersHigh);
    auto p2 = get_ow_map_gfx_ptr(
        i, rom()->version_constants().kCompressedAllMap32PointersLow);

    int ttpos = 0;

    if (p1 >= highest) highest = p1;
    if (p2 >= highest) highest = p2;

    if (p1 <= lowest && p1 > 0x0F8000) lowest = p1;
    if (p2 <= lowest && p2 > 0x0F8000) lowest = p2;

    std::vector<uint8_t> bytes, bytes2;
    int size1, size2;
    auto decomp = gfx::lc_lz2::Uncompress(rom()->data() + p2, &size1, 1);
    bytes.resize(size1);
    for (int j = 0; j < size1; j++) {
      bytes[j] = decomp[j];
    }
    free(decomp);
    decomp = gfx::lc_lz2::Uncompress(rom()->data() + p1, &size2, 1);
    bytes2.resize(size2);
    for (int j = 0; j < size2; j++) {
      bytes2[j] = decomp[j];
    }
    free(decomp);

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
  std::vector<std::future<absl::Status>> futures;
  for (int i = 0; i < kNumOverworldMaps; ++i) {
    int world_type = 0;
    if (i >= 64 && i < 0x80) {
      world_type = 1;
    } else if (i >= 0x80) {
      world_type = 2;
    }
    auto task_function = [this, i, size, world_type]() {
      return overworld_maps_[i].BuildMap(size, game_state_, world_type,
                                         tiles16_, GetMapTiles(world_type));
    };
    futures.emplace_back(std::async(std::launch::async, task_function));
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

void Overworld::LoadTileTypes() {
  for (int i = 0; i < 0x200; i++) {
    all_tiles_types_[i] =
        rom()->data()[rom()->version_constants().overworldTilesType + i];
  }
}

void Overworld::LoadEntrances() {
  for (int i = 0; i < 129; i++) {
    short map_id = rom()->toint16(OWEntranceMap + (i * 2));
    uint16_t map_pos = rom()->toint16(OWEntrancePos + (i * 2));
    uint8_t entrance_id = rom_[OWEntranceEntranceId + i];
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
    uint8_t entrance_id = (rom_[OWHoleEntrance + i]);
    int p = (map_pos + 0x400) >> 1;
    int x = (p % 64);
    int y = (p >> 6);
    all_holes_.emplace_back(
        (x * 16) + (((map_id % 64) - (((map_id % 64) / 8) * 8)) * 512),
        (y * 16) + (((map_id % 64) / 8) * 512), entrance_id, map_id,
        (uint16_t)(map_pos + 0x400), true);
  }
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

    exits.emplace_back(exit_room_id, exit_map_id, exit_vram, exit_y_scroll,
                       exit_x_scroll, py, px, exit_y_camera, exit_x_camera,
                       exit_scroll_mod_y, exit_scroll_mod_x, exit_door_type_1,
                       exit_door_type_2, (px & py) == 0xFFFF);
  }
  all_exits_ = exits;
  return absl::OkStatus();
}

absl::Status Overworld::LoadItems() {
  ASSIGN_OR_RETURN(uint32_t pointer,
                   rom()->ReadLong(zelda3::overworld::kOverworldItemsAddress));
  uint32_t pointer_pc = core::SnesToPc(pointer);  // 1BC2F9 -> 0DC2F9
  for (int i = 0; i < 128; i++) {
    ASSIGN_OR_RETURN(uint16_t word_address,
                     rom()->ReadWord(pointer_pc + i * 2));
    uint32_t addr = (pointer & 0xFF0000) | word_address;  // 1B F9  3C
    addr = core::SnesToPc(addr);

    if (overworld_maps_[i].is_large_map()) {
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

      int x = p % 64;
      int y = p >> 6;

      int fakeID = i;
      if (fakeID >= 64) {
        fakeID -= 64;
      }

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
  for (int i = 0; i < 3; i++) {
    all_sprites_.emplace_back();
  }

  RETURN_IF_ERROR(LoadSpritesFromMap(overworldSpritesBegining, 64, 0));
  RETURN_IF_ERROR(LoadSpritesFromMap(overworldSpritesZelda, 144, 1));
  RETURN_IF_ERROR(LoadSpritesFromMap(overworldSpritesAgahnim, 144, 2));
  return absl::OkStatus();
}

absl::Status Overworld::LoadSpritesFromMap(int sprites_per_gamestate_ptr,
                                           int num_maps_per_gamestate,
                                           int game_state) {
  for (int i = 0; i < num_maps_per_gamestate; i++) {
    if (map_parent_[i] != i) continue;

    int current_spr_ptr = sprites_per_gamestate_ptr + (i * 2);
    ASSIGN_OR_RETURN(auto word_addr, rom()->ReadWord(current_spr_ptr));
    int sprite_address = core::SnesToPc((0x09 << 0x10) | word_addr);
    while (true) {
      ASSIGN_OR_RETURN(uint8_t b1, rom()->ReadByte(sprite_address));
      ASSIGN_OR_RETURN(uint8_t b2, rom()->ReadByte(sprite_address + 1));
      ASSIGN_OR_RETURN(uint8_t b3, rom()->ReadByte(sprite_address + 2));
      if (b1 == 0xFF) break;

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
          overworld_maps_[i].current_graphics(), (uint8_t)i, b3,
          (uint8_t)(b2 & 0x3F), (uint8_t)(b1 & 0x3F), realX, realY);
      // all_sprites_[game_state][i].Draw();

      sprite_address += 3;
    }
  }

  return absl::OkStatus();
}

// ---------------------------------------------------------------------------

absl::Status Overworld::Save(Rom &rom) {
  rom_ = rom;

  RETURN_IF_ERROR(SaveMap16Tiles())
  RETURN_IF_ERROR(SaveMap32Tiles())
  RETURN_IF_ERROR(SaveOverworldMaps())
  RETURN_IF_ERROR(SaveEntrances())
  RETURN_IF_ERROR(SaveExits())

  return absl::OkStatus();
}

absl::Status Overworld::SaveOverworldMaps() {
  core::Logger::log("Saving Overworld Maps");

  // Initialize map pointers
  std::fill(map_pointers1_id.begin(), map_pointers1_id.end(), -1);
  std::fill(map_pointers2_id.begin(), map_pointers2_id.end(), -1);

  // Compress and save each map
  int pos = 0x058000;
  for (int i = 0; i < 160; i++) {
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

    std::vector<uint8_t> a, b;
    int size_a, size_b;
    // Compress single_map_1 and single_map_2
    auto a_char = gfx::lc_lz2::Compress(single_map_1.data(), 256, &size_a, 1);
    auto b_char = gfx::lc_lz2::Compress(single_map_2.data(), 256, &size_b, 1);
    if (a_char == nullptr || b_char == nullptr) {
      return absl::AbortedError("Error compressing map gfx.");
    }
    // Copy the compressed data to a and b
    a.resize(size_a);
    b.resize(size_b);
    // Copy the arrays manually
    for (int k = 0; k < size_a; k++) {
      a[k] = a_char[k];
    }
    for (int k = 0; k < size_b; k++) {
      b[k] = b_char[k];
    }

    // Save compressed data and pointers
    map_data_p1[i] = std::vector<uint8_t>(size_a);
    map_data_p2[i] = std::vector<uint8_t>(size_b);

    if ((pos + size_a) >= 0x5FE70 && (pos + size_a) <= 0x60000) {
      pos = 0x60000;
    }

    if ((pos + size_a) >= 0x6411F && (pos + size_a) <= 0x70000) {
      core::Logger::log("Pos set to overflow region for map " +
                        std::to_string(i) + " at " +
                        core::UppercaseHexLong(pos));
      pos = OverworldMapDataOverflow;  // 0x0F8780;
    }

    auto compareArray = [](const std::vector<uint8_t> &array1,
                           const std::vector<uint8_t> &array2) -> bool {
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
      if (compareArray(a, map_data_p1[j])) {
        // Reuse pointer id j for P1 (a)
        map_pointers1_id[i] = j;
      }

      if (compareArray(b, map_data_p2[j])) {
        map_pointers2_id[i] = j;
        // Reuse pointer id j for P2 (b)
      }
    }

    if (map_pointers1_id[i] == -1) {
      // Save compressed data and pointer for map1
      std::copy(a.begin(), a.end(), map_data_p1[i].begin());
      int snes_pos = core::PcToSnes(pos);
      map_pointers1[i] = snes_pos;
      core::Logger::log("Saving map pointers1 and compressed data for map " +
                        core::UppercaseHexByte(i) + " at " +
                        core::UppercaseHexLong(snes_pos));
      RETURN_IF_ERROR(rom()->WriteLong(
          rom()->version_constants().kCompressedAllMap32PointersLow + (3 * i),
          snes_pos));
      RETURN_IF_ERROR(rom()->WriteVector(pos, a));
      pos += size_a;
    } else {
      // Save pointer for map1
      int snes_pos = map_pointers1[map_pointers1_id[i]];
      core::Logger::log("Saving map pointers1 for map " +
                        core::UppercaseHexByte(i) + " at " +
                        core::UppercaseHexLong(snes_pos));
      RETURN_IF_ERROR(rom()->WriteLong(
          rom()->version_constants().kCompressedAllMap32PointersLow + (3 * i),
          snes_pos));
    }

    if ((pos + b.size()) >= 0x5FE70 && (pos + b.size()) <= 0x60000) {
      pos = 0x60000;
    }

    if ((pos + b.size()) >= 0x6411F && (pos + b.size()) <= 0x70000) {
      core::Logger::log("Pos set to overflow region for map " +
                        core::UppercaseHexByte(i) + " at " +
                        core::UppercaseHexLong(pos));
      pos = OverworldMapDataOverflow;
    }

    if (map_pointers2_id[i] == -1) {
      // Save compressed data and pointer for map2
      std::copy(b.begin(), b.end(), map_data_p2[i].begin());
      int snes_pos = core::PcToSnes(pos);
      map_pointers2[i] = snes_pos;
      core::Logger::log("Saving map pointers2 and compressed data for map " +
                        core::UppercaseHexByte(i) + " at " +
                        core::UppercaseHexLong(snes_pos));
      RETURN_IF_ERROR(rom()->WriteLong(
          rom()->version_constants().kCompressedAllMap32PointersHigh + (3 * i),
          snes_pos));
      RETURN_IF_ERROR(rom()->WriteVector(pos, b));
      pos += size_b;
    } else {
      // Save pointer for map2
      int snes_pos = map_pointers2[map_pointers2_id[i]];
      core::Logger::log("Saving map pointers2 for map " +
                        core::UppercaseHexByte(i) + " at " +
                        core::UppercaseHexLong(snes_pos));
      RETURN_IF_ERROR(rom()->WriteLong(
          rom()->version_constants().kCompressedAllMap32PointersHigh + (3 * i),
          snes_pos));
    }
  }

  // Check if too many maps data
  if (pos > 0x137FFF) {
    std::cerr << "Too many maps data " << std::hex << pos << std::endl;
    return absl::AbortedError("Too many maps data " + std::to_string(pos));
  }

  // Save large maps
  RETURN_IF_ERROR(SaveLargeMaps())

  return absl::OkStatus();
}

absl::Status Overworld::SaveLargeMaps() {
  core::Logger::log("Saving Large Maps");
  std::vector<uint8_t> checked_map;

  for (int i = 0; i < 0x40; i++) {
    int y_pos = i / 8;
    int x_pos = i % 8;
    int parent_y_pos = overworld_maps_[i].parent() / 8;
    int parent_x_pos = overworld_maps_[i].parent() % 8;

    // Always write the map parent since it should not matter
    RETURN_IF_ERROR(
        rom()->Write(overworldMapParentId + i, overworld_maps_[i].parent()))

    if (std::find(checked_map.begin(), checked_map.end(), i) !=
        checked_map.end()) {
      continue;
    }

    // If it's large then save parent pos *
    // 0x200 otherwise pos * 0x200
    if (overworld_maps_[i].is_large_map()) {
      const uint8_t large_map_offsets[] = {0, 1, 8, 9};
      for (const auto &offset : large_map_offsets) {
        // Check 1
        RETURN_IF_ERROR(rom()->WriteByte(overworldMapSize + i + offset, 0x20));
        // Check 2
        RETURN_IF_ERROR(
            rom()->WriteByte(overworldMapSizeHighByte + i + offset, 0x03));
        // Check 3
        RETURN_IF_ERROR(
            rom()->WriteByte(overworldScreenSize + i + offset, 0x00));
        RETURN_IF_ERROR(
            rom()->WriteByte(overworldScreenSize + i + offset + 64, 0x00));
        // Check 4
        RETURN_IF_ERROR(
            rom()->WriteByte(OverworldScreenSizeForLoading + i + offset, 0x04));
        RETURN_IF_ERROR(rom()->WriteByte(
            OverworldScreenSizeForLoading + i + offset + 64, 0x04));
        RETURN_IF_ERROR(rom()->WriteByte(
            OverworldScreenSizeForLoading + i + offset + 128, 0x04));
      }

      // Check 5 and 6
      RETURN_IF_ERROR(
          rom()->WriteShort(transition_target_north + (i * 2),
                            (uint16_t)((parent_y_pos * 0x200) - 0xE0)));
      RETURN_IF_ERROR(
          rom()->WriteShort(transition_target_west + (i * 2),
                            (uint16_t)((parent_x_pos * 0x200) - 0x100)));

      RETURN_IF_ERROR(
          rom()->WriteShort(transition_target_north + (i * 2) + 2,
                            (uint16_t)((parent_y_pos * 0x200) - 0xE0)));
      RETURN_IF_ERROR(
          rom()->WriteShort(transition_target_west + (i * 2) + 2,
                            (uint16_t)((parent_x_pos * 0x200) - 0x100)));

      RETURN_IF_ERROR(
          rom()->WriteShort(transition_target_north + (i * 2) + 16,
                            (uint16_t)((parent_y_pos * 0x200) - 0xE0)));
      RETURN_IF_ERROR(
          rom()->WriteShort(transition_target_west + (i * 2) + 16,
                            (uint16_t)((parent_x_pos * 0x200) - 0x100)));

      RETURN_IF_ERROR(
          rom()->WriteShort(transition_target_north + (i * 2) + 18,
                            (uint16_t)((parent_y_pos * 0x200) - 0xE0)));
      RETURN_IF_ERROR(
          rom()->WriteShort(transition_target_west + (i * 2) + 18,
                            (uint16_t)((parent_x_pos * 0x200) - 0x100)));

      // Check 7 and 8
      RETURN_IF_ERROR(rom()->WriteShort(overworldTransitionPositionX + (i * 2),
                                        (parent_x_pos * 0x200)));
      RETURN_IF_ERROR(rom()->WriteShort(overworldTransitionPositionY + (i * 2),
                                        (parent_y_pos * 0x200)));

      RETURN_IF_ERROR(rom()->WriteShort(
          overworldTransitionPositionX + (i * 2) + 02, (parent_x_pos * 0x200)));
      RETURN_IF_ERROR(rom()->WriteShort(
          overworldTransitionPositionY + (i * 2) + 02, (parent_y_pos * 0x200)));

      // problematic
      RETURN_IF_ERROR(rom()->WriteShort(
          overworldTransitionPositionX + (i * 2) + 16, (parent_x_pos * 0x200)));
      RETURN_IF_ERROR(rom()->WriteShort(
          overworldTransitionPositionY + (i * 2) + 16, (parent_y_pos * 0x200)));

      RETURN_IF_ERROR(rom()->WriteShort(
          overworldTransitionPositionX + (i * 2) + 18, (parent_x_pos * 0x200)));
      RETURN_IF_ERROR(rom()->WriteShort(
          overworldTransitionPositionY + (i * 2) + 18, (parent_y_pos * 0x200)));

      // Check 9
      RETURN_IF_ERROR(rom()->WriteShort(
          OverworldScreenTileMapChangeByScreen1 + (i * 2) + 00, 0x0060));
      RETURN_IF_ERROR(rom()->WriteShort(
          OverworldScreenTileMapChangeByScreen1 + (i * 2) + 02, 0x0060));

      // If parentX == 0 then lower submaps == 0x0060 too
      if (parent_x_pos == 0) {
        RETURN_IF_ERROR(rom()->WriteShort(
            OverworldScreenTileMapChangeByScreen1 + (i * 2) + 16, 0x0060));
        RETURN_IF_ERROR(rom()->WriteShort(
            OverworldScreenTileMapChangeByScreen1 + (i * 2) + 18, 0x0060));
      } else {
        // Otherwise lower submaps == 0x1060
        RETURN_IF_ERROR(rom()->WriteShort(
            OverworldScreenTileMapChangeByScreen1 + (i * 2) + 16, 0x1060));
        RETURN_IF_ERROR(rom()->WriteShort(
            OverworldScreenTileMapChangeByScreen1 + (i * 2) + 18, 0x1060));

        // If the area to the left is a large map, we don't need to add an
        // offset to it. otherwise leave it the same. Just to make sure where
        // don't try to read outside of the array.
        if ((i - 1) >= 0) {
          // If the area to the left is a large area.
          if (overworld_maps_[i - 1].is_large_map()) {
            // If the area to the left is the bottom right of a large area.
            if (overworld_maps_[i - 1].large_index() == 1) {
              RETURN_IF_ERROR(rom()->WriteShort(
                  OverworldScreenTileMapChangeByScreen1 + (i * 2) + 16,
                  0x0060));
            }
          }
        }
      }

      // Always 0x0080
      RETURN_IF_ERROR(rom()->WriteShort(
          OverworldScreenTileMapChangeByScreen2 + (i * 2) + 00, 0x0080));
      RETURN_IF_ERROR(rom()->WriteShort(
          OverworldScreenTileMapChangeByScreen2 + (i * 2) + 2, 0x0080));
      // Lower always 0x1080
      RETURN_IF_ERROR(rom()->WriteShort(
          OverworldScreenTileMapChangeByScreen2 + (i * 2) + 16, 0x1080));
      RETURN_IF_ERROR(rom()->WriteShort(
          OverworldScreenTileMapChangeByScreen2 + (i * 2) + 18, 0x1080));

      // If the area to the right is a large map, we don't need to add an offset
      // to it. otherwise leave it the same. Just to make sure where don't try
      // to read outside of the array.
      if ((i + 2) < 64) {
        // If the area to the right is a large area.
        if (overworld_maps_[i + 2].is_large_map()) {
          // If the area to the right is the top left of a large area.
          if (overworld_maps_[i + 2].large_index() == 0) {
            RETURN_IF_ERROR(rom()->WriteShort(
                OverworldScreenTileMapChangeByScreen2 + (i * 2) + 18, 0x0080));
          }
        }
      }

      // Always 0x1800
      RETURN_IF_ERROR(rom()->WriteShort(
          OverworldScreenTileMapChangeByScreen3 + (i * 2), 0x1800));
      RETURN_IF_ERROR(rom()->WriteShort(
          OverworldScreenTileMapChangeByScreen3 + (i * 2) + 16, 0x1800));
      // Right side is always 0x1840
      RETURN_IF_ERROR(rom()->WriteShort(
          OverworldScreenTileMapChangeByScreen3 + (i * 2) + 2, 0x1840));
      RETURN_IF_ERROR(rom()->WriteShort(
          OverworldScreenTileMapChangeByScreen3 + (i * 2) + 18, 0x1840));

      // If the area above is a large map, we don't need to add an offset to it.
      // otherwise leave it the same.
      // Just to make sure where don't try to read outside of the array.
      if (i - 8 >= 0) {
        // If the area just above us is a large area.
        if (overworld_maps_[i - 8].is_large_map()) {
          // If the area just above us is the bottom left of a large area.
          if (overworld_maps_[i - 8].large_index() == 2) {
            RETURN_IF_ERROR(rom()->WriteShort(
                OverworldScreenTileMapChangeByScreen3 + (i * 2) + 02, 0x1800));
          }
        }
      }

      // Always 0x2000
      RETURN_IF_ERROR(rom()->WriteShort(
          OverworldScreenTileMapChangeByScreen4 + (i * 2) + 00, 0x2000));
      RETURN_IF_ERROR(rom()->WriteShort(
          OverworldScreenTileMapChangeByScreen4 + (i * 2) + 16, 0x2000));
      // Right side always 0x2040
      RETURN_IF_ERROR(rom()->WriteShort(
          OverworldScreenTileMapChangeByScreen4 + (i * 2) + 2, 0x2040));
      RETURN_IF_ERROR(rom()->WriteShort(
          OverworldScreenTileMapChangeByScreen4 + (i * 2) + 18, 0x2040));

      // If the area below is a large map, we don't need to add an offset to it.
      // otherwise leave it the same.
      // Just to make sure where don't try to read outside of the array.
      if (i + 16 < 64) {
        // If the area just below us is a large area.
        if (overworld_maps_[i + 16].is_large_map()) {
          // If the area just below us is the top left of a large area.
          if (overworld_maps_[i + 16].large_index() == 0) {
            RETURN_IF_ERROR(rom()->WriteShort(
                OverworldScreenTileMapChangeByScreen4 + (i * 2) + 18, 0x2000));
          }
        }
      }

      checked_map.emplace_back(i);
      checked_map.emplace_back((i + 1));
      checked_map.emplace_back((i + 8));
      checked_map.emplace_back((i + 9));

    } else {
      RETURN_IF_ERROR(rom()->WriteByte(overworldMapSize + i, 0x00));
      RETURN_IF_ERROR(rom()->WriteByte(overworldMapSizeHighByte + i, 0x01));

      RETURN_IF_ERROR(rom()->WriteByte(overworldScreenSize + i, 0x01));
      RETURN_IF_ERROR(rom()->WriteByte(overworldScreenSize + i + 64, 0x01));

      RETURN_IF_ERROR(
          rom()->WriteByte(OverworldScreenSizeForLoading + i, 0x02));
      RETURN_IF_ERROR(
          rom()->WriteByte(OverworldScreenSizeForLoading + i + 64, 0x02));
      RETURN_IF_ERROR(
          rom()->WriteByte(OverworldScreenSizeForLoading + i + 128, 0x02));

      RETURN_IF_ERROR(rom()->WriteShort(
          OverworldScreenTileMapChangeByScreen1 + (i * 2), 0x0060));

      // If the area to the left is a large map, we don't need to add an offset
      // to it. otherwise leave it the same.
      // Just to make sure where don't try to read outside of the array.
      if (i - 1 >= 0 && parent_x_pos != 0) {
        if (overworld_maps_[i - 1].is_large_map()) {
          if (overworld_maps_[i - 1].large_index() == 3) {
            RETURN_IF_ERROR(rom()->WriteShort(
                OverworldScreenTileMapChangeByScreen1 + (i * 2), 0xF060));
          }
        }
      }

      RETURN_IF_ERROR(rom()->WriteShort(
          OverworldScreenTileMapChangeByScreen2 + (i * 2), 0x0040));

      if (i + 1 < 64 && parent_x_pos != 7) {
        if (overworld_maps_[i + 1].is_large_map()) {
          if (overworld_maps_[i + 1].large_index() == 2) {
            RETURN_IF_ERROR(rom()->WriteShort(
                OverworldScreenTileMapChangeByScreen2 + (i * 2), 0xF040));
          }
        }
      }

      RETURN_IF_ERROR(rom()->WriteShort(
          OverworldScreenTileMapChangeByScreen3 + (i * 2), 0x1800));

      // If the area above is a large map, we don't need to add an offset to it.
      // otherwise leave it the same.
      // Just to make sure where don't try to read outside of the array.
      if (i - 8 >= 0) {
        // If the area just above us is a large area.
        if (overworld_maps_[i - 8].is_large_map()) {
          // If we are under the bottom right of the large area.
          if (overworld_maps_[i - 8].large_index() == 3) {
            RETURN_IF_ERROR(rom()->WriteShort(
                OverworldScreenTileMapChangeByScreen3 + (i * 2), 0x17C0));
          }
        }
      }

      RETURN_IF_ERROR(rom()->WriteShort(
          OverworldScreenTileMapChangeByScreen4 + (i * 2), 0x1000));

      // If the area below is a large map, we don't need to add an offset to it.
      // otherwise leave it the same.
      // Just to make sure where don't try to read outside of the array.
      if (i + 8 < 64) {
        // If the area just below us is a large area.
        if (overworld_maps_[i + 8].is_large_map()) {
          // If we are on top of the top right of the large area.
          if (overworld_maps_[i + 8].large_index() == 1) {
            RETURN_IF_ERROR(rom()->WriteShort(
                OverworldScreenTileMapChangeByScreen4 + (i * 2), 0x0FC0));
          }
        }
      }

      RETURN_IF_ERROR(rom()->WriteShort(transition_target_north + (i * 2),
                                        (uint16_t)((y_pos * 0x200) - 0xE0)));
      RETURN_IF_ERROR(rom()->WriteShort(transition_target_west + (i * 2),
                                        (uint16_t)((x_pos * 0x200) - 0x100)));

      RETURN_IF_ERROR(rom()->WriteShort(overworldTransitionPositionX + (i * 2),
                                        (x_pos * 0x200)));
      RETURN_IF_ERROR(rom()->WriteShort(overworldTransitionPositionY + (i * 2),
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

namespace {
std::vector<uint64_t> GetAllTile16(OWMapTiles &map_tiles_) {
  std::vector<uint64_t> all_tile_16;  // Ensure it's 64 bits

  int sx = 0;
  int sy = 0;
  int c = 0;
  OWBlockset tiles_used;
  for (int i = 0; i < kNumOverworldMaps; i++) {
    if (i < 64) {
      tiles_used = map_tiles_.light_world;
    } else if (i < 128 && i >= 64) {
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

  if (flags()->kLogToConsole) {
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

absl::Status Overworld::SaveMap32Tiles() {
  core::Logger::log("Saving Map32 Tiles");
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

absl::Status Overworld::SaveMap16Tiles() {
  core::Logger::log("Saving Map16 Tiles");
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
  core::Logger::log("Saving Entrances");
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
  core::Logger::log("Saving Exits");
  for (int i = 0; i < 0x4F; i++) {
    RETURN_IF_ERROR(
        rom()->WriteShort(OWExitRoomId + (i * 2), all_exits_[i].room_id_));
    RETURN_IF_ERROR(rom()->Write(OWExitMapId + i, all_exits_[i].map_id_));
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

bool compareItemsArrays(std::vector<OverworldItem> itemArray1,
                        std::vector<OverworldItem> itemArray2) {
  if (itemArray1.size() != itemArray2.size()) {
    return false;
  }

  bool match;
  for (size_t i = 0; i < itemArray1.size(); i++) {
    match = false;
    for (size_t j = 0; j < itemArray2.size(); j++) {
      // Check all sprite in 2nd array if one match
      if (itemArray1[i].x_ == itemArray2[j].x_ &&
          itemArray1[i].y_ == itemArray2[j].y_ &&
          itemArray1[i].id_ == itemArray2[j].id_) {
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
  std::vector<std::vector<OverworldItem>> room_items(128);

  for (int i = 0; i < 128; i++) {
    room_items[i] = std::vector<OverworldItem>();
    for (const OverworldItem &item : all_items_) {
      if (item.room_map_id_ == i) {
        room_items[i].emplace_back(item);
        if (item.id_ == 0x86) {
          RETURN_IF_ERROR(rom()->WriteWord(
              0x16DC5 + (i * 2), (item.game_x_ + (item.game_y_ * 64)) * 2));
        }
      }
    }
  }

  int data_pos = overworldItemsPointers + 0x100;

  int item_pointers[128];
  int item_pointers_reuse[128];
  int empty_pointer = 0;

  for (int i = 0; i < 128; i++) {
    item_pointers_reuse[i] = -1;
    for (int ci = 0; ci < i; ci++) {
      if (room_items[i].empty()) {
        item_pointers_reuse[i] = -2;
        break;
      }

      // Copy into separator vectors from i to ci, then ci to end
      if (compareItemsArrays(
              std::vector<OverworldItem>(room_items[i].begin(),
                                         room_items[i].end()),
              std::vector<OverworldItem>(room_items[ci].begin(),
                                         room_items[ci].end()))) {
        item_pointers_reuse[i] = ci;
        break;
      }
    }
  }

  for (int i = 0; i < 128; i++) {
    if (item_pointers_reuse[i] == -1) {
      item_pointers[i] = data_pos;
      for (const OverworldItem &item : room_items[i]) {
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

    int snesaddr = core::PcToSnes(item_pointers[i]);
    RETURN_IF_ERROR(
        rom()->WriteWord(overworldItemsPointers + (i * 2), snesaddr));
  }

  if (data_pos > overworldItemsEndData) {
    return absl::AbortedError("Too many items");
  }

  if (flags()->kLogToConsole) {
    std::cout << "End of Items : " << data_pos << std::endl;
  }

  return absl::OkStatus();
}

absl::Status Overworld::SaveMapProperties() {
  core::Logger::log("Saving Map Properties");
  for (int i = 0; i < 64; i++) {
    RETURN_IF_ERROR(rom()->WriteByte(kAreaGfxIdPtr + i,
                                     overworld_maps_[i].area_graphics()));
    RETURN_IF_ERROR(rom()->WriteByte(kOverworldMapPaletteIds + i,
                                     overworld_maps_[i].area_palette()));
    RETURN_IF_ERROR(rom()->WriteByte(overworldSpriteset + i,
                                     overworld_maps_[i].sprite_graphics(0)));
    RETURN_IF_ERROR(rom()->WriteByte(overworldSpriteset + 64 + i,
                                     overworld_maps_[i].sprite_graphics(1)));
    RETURN_IF_ERROR(rom()->WriteByte(overworldSpriteset + 128 + i,
                                     overworld_maps_[i].sprite_graphics(2)));
    RETURN_IF_ERROR(rom()->WriteByte(kOverworldSpritePaletteIds + i,
                                     overworld_maps_[i].sprite_palette(0)));
    RETURN_IF_ERROR(rom()->WriteByte(kOverworldSpritePaletteIds + 64 + i,
                                     overworld_maps_[i].sprite_palette(1)));
    RETURN_IF_ERROR(rom()->WriteByte(kOverworldSpritePaletteIds + 128 + i,
                                     overworld_maps_[i].sprite_palette(2)));
  }

  for (int i = 64; i < 128; i++) {
    RETURN_IF_ERROR(rom()->WriteByte(kAreaGfxIdPtr + i,
                                     overworld_maps_[i].area_graphics()));
    RETURN_IF_ERROR(rom()->WriteByte(overworldSpriteset + i,
                                     overworld_maps_[i].sprite_graphics(0)));
    RETURN_IF_ERROR(rom()->WriteByte(overworldSpriteset + 64 + i,
                                     overworld_maps_[i].sprite_graphics(1)));
    RETURN_IF_ERROR(rom()->WriteByte(overworldSpriteset + 128 + i,
                                     overworld_maps_[i].sprite_graphics(2)));
    RETURN_IF_ERROR(rom()->WriteByte(kOverworldMapPaletteIds + i,
                                     overworld_maps_[i].area_palette()));
    RETURN_IF_ERROR(rom()->WriteByte(kOverworldSpritePaletteIds + 64 + i,
                                     overworld_maps_[i].sprite_palette(0)));
    RETURN_IF_ERROR(rom()->WriteByte(kOverworldSpritePaletteIds + 128 + i,
                                     overworld_maps_[i].sprite_palette(1)));
    RETURN_IF_ERROR(rom()->WriteByte(kOverworldSpritePaletteIds + 192 + i,
                                     overworld_maps_[i].sprite_palette(2)));
  }

  return absl::OkStatus();
}

}  // namespace overworld
}  // namespace zelda3
}  // namespace app
}  // namespace yaze
