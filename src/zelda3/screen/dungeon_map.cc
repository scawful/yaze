#include "dungeon_map.h"

#include <fstream>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/render/tilemap.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_tile.h"
#include "app/platform/window.h"
#include "rom/snes.h"
#include "util/file_util.h"
#include "util/hex.h"
#include "zelda3/game_data.h"

namespace yaze::zelda3 {

absl::StatusOr<std::vector<DungeonMap>> LoadDungeonMaps(
    Rom& rom, DungeonMapLabels& dungeon_map_labels) {
  std::vector<DungeonMap> dungeon_maps;
  std::vector<std::array<uint8_t, kNumRooms>> current_floor_rooms_d;
  std::vector<std::array<uint8_t, kNumRooms>> current_floor_gfx_d;
  int total_floors_d;
  uint8_t nbr_floor_d;
  uint8_t nbr_basement_d;

  for (int d = 0; d < kNumDungeons; d++) {
    current_floor_rooms_d.clear();
    current_floor_gfx_d.clear();
    ASSIGN_OR_RETURN(int ptr, rom.ReadWord(kDungeonMapRoomsPtr + (d * 2)));
    ASSIGN_OR_RETURN(int ptr_gfx, rom.ReadWord(kDungeonMapGfxPtr + (d * 2)));
    ptr |= 0x0A0000;                     // Add bank to the short ptr
    ptr_gfx |= 0x0A0000;                 // Add bank to the short ptr
    int pc_ptr = SnesToPc(ptr);          // Contains data for the next 25 rooms
    int pc_ptr_gfx = SnesToPc(ptr_gfx);  // Contains data for the next 25 rooms

    ASSIGN_OR_RETURN(uint16_t boss_room_d,
                     rom.ReadWord(kDungeonMapBossRooms + (d * 2)));

    ASSIGN_OR_RETURN(nbr_basement_d, rom.ReadByte(kDungeonMapFloors + (d * 2)));
    nbr_basement_d &= 0x0F;

    ASSIGN_OR_RETURN(nbr_floor_d, rom.ReadByte(kDungeonMapFloors + (d * 2)));
    nbr_floor_d &= 0xF0;
    nbr_floor_d = nbr_floor_d >> 4;

    total_floors_d = nbr_basement_d + nbr_floor_d;

    // for each floor in the dungeon
    for (int i = 0; i < total_floors_d; i++) {
      dungeon_map_labels[d].emplace_back();

      std::array<uint8_t, kNumRooms> rdata;
      std::array<uint8_t, kNumRooms> gdata;

      // for each room on the floor
      for (int j = 0; j < kNumRooms; j++) {
        gdata[j] = 0xFF;
        rdata[j] = rom.data()[pc_ptr + j + (i * kNumRooms)];  // Set the rooms

        gdata[j] = rdata[j] == 0x0F ? 0xFF : rom.data()[pc_ptr_gfx++];

        std::string label = util::HexByte(rdata[j]);
        dungeon_map_labels[d][i][j] = label;
      }

      current_floor_gfx_d.push_back(gdata);    // Add new floor gfx data
      current_floor_rooms_d.push_back(rdata);  // Add new floor data
    }

    dungeon_maps.emplace_back(boss_room_d, nbr_floor_d, nbr_basement_d,
                              current_floor_rooms_d, current_floor_gfx_d);
  }

  return dungeon_maps;
}

absl::Status SaveDungeonMaps(Rom& rom, std::vector<DungeonMap>& dungeon_maps) {
  int pos = kDungeonMapDataStart;

  for (int d = 0; d < kNumDungeons; d++) {
    if (d >= static_cast<int>(dungeon_maps.size())) {
      break;
    }

    auto& map = dungeon_maps[d];
    const int total_floors = map.nbr_of_floor + map.nbr_of_basement;

    uint16_t floors = (map.nbr_of_floor << 4) | map.nbr_of_basement;
    RETURN_IF_ERROR(rom.WriteWord(kDungeonMapFloors + (d * 2), floors));
    RETURN_IF_ERROR(
        rom.WriteWord(kDungeonMapBossRooms + (d * 2), map.boss_room));

    const bool has_boss = map.boss_room != 0x000F && map.boss_room != 0xFFFF;
    bool search_boss = has_boss;
    if (!has_boss) {
      RETURN_IF_ERROR(rom.WriteWord(kDungeonMapBossFloors + (d * 2), 0xFFFF));
    }

    RETURN_IF_ERROR(
        rom.WriteWord(kDungeonMapRoomsPtr + (d * 2), PcToSnes(pos)));

    bool restart = false;
    for (int f = 0; f < total_floors; f++) {
      for (int r = 0; r < kNumRooms; r++) {
        if (search_boss && map.floor_rooms[f][r] == map.boss_room) {
          RETURN_IF_ERROR(rom.WriteWord(kDungeonMapBossFloors + (d * 2), f));
          search_boss = false;
        }

        if (pos >= kDungeonMapDataReservedStart &&
            pos <= kDungeonMapDataReservedEnd) {
          pos = kDungeonMapDataReservedEnd + 1;
          restart = true;
          break;
        }

        RETURN_IF_ERROR(rom.WriteByte(pos, map.floor_rooms[f][r]));
        pos++;
      }
      if (restart)
        break;
    }

    if (restart) {
      d--;
      continue;
    }

    RETURN_IF_ERROR(rom.WriteWord(kDungeonMapGfxPtr + (d * 2), PcToSnes(pos)));
    for (int f = 0; f < total_floors; f++) {
      for (int r = 0; r < kNumRooms; r++) {
        if (map.floor_rooms[f][r] != 0x0F) {
          if (pos >= kDungeonMapDataReservedStart &&
              pos <= kDungeonMapDataReservedEnd) {
            pos = kDungeonMapDataReservedEnd + 1;
            RETURN_IF_ERROR(
                rom.WriteWord(kDungeonMapGfxPtr + (d * 2), PcToSnes(pos)));
            restart = true;
            break;
          }

          RETURN_IF_ERROR(rom.WriteByte(pos, map.floor_gfx[f][r]));
          pos++;
        }
      }
      if (restart)
        break;
    }

    if (pos >= kDungeonMapDataLimit) {
      return absl::OutOfRangeError("Dungeon map data exceeds reserved space");
    }

    if (restart) {
      d--;
      continue;
    }

    if (search_boss) {
      return absl::NotFoundError(
          absl::StrFormat("Boss room not found for dungeon %d", d));
    }
  }

  return absl::OkStatus();
}

absl::Status LoadDungeonMapTile16(gfx::Tilemap& tile16_blockset, Rom& rom,
                                  GameData* game_data,
                                  const std::vector<uint8_t>& gfx_data,
                                  bool bin_mode) {
  tile16_blockset.tile_size = {16, 16};
  tile16_blockset.map_size = {186, 186};
  tile16_blockset.atlas.Create(256, 192, 8,
                               std::vector<uint8_t>(256 * 192, 0x00));

  for (int i = 0; i < kNumDungeonMapTile16; i++) {
    int addr = kDungeonMapTile16;
    if (rom.data()[kDungeonMapExpCheck] != 0xB9) {
      addr = kDungeonMapTile16Expanded;
    }

    ASSIGN_OR_RETURN(auto tl, rom.ReadWord(addr + (i * 8)));
    gfx::TileInfo t1 = gfx::WordToTileInfo(tl);  // Top left

    ASSIGN_OR_RETURN(auto tr, rom.ReadWord(addr + 2 + (i * 8)));
    gfx::TileInfo t2 = gfx::WordToTileInfo(tr);  // Top right

    ASSIGN_OR_RETURN(auto bl, rom.ReadWord(addr + 4 + (i * 8)));
    gfx::TileInfo t3 = gfx::WordToTileInfo(bl);  // Bottom left

    ASSIGN_OR_RETURN(auto br, rom.ReadWord(addr + 6 + (i * 8)));
    gfx::TileInfo t4 = gfx::WordToTileInfo(br);  // Bottom right

    int sheet_offset = 212;
    if (bin_mode) {
      sheet_offset = 0;
    }
    ComposeTile16(tile16_blockset, gfx_data, t1, t2, t3, t4, sheet_offset);
  }

  if (game_data) {
    tile16_blockset.atlas.SetPalette(
        *game_data->palette_groups.dungeon_main.mutable_palette(3));
  }

  // Queue texture creation via Arena's deferred system
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE,
                                        &tile16_blockset.atlas);

  return absl::OkStatus();
}

absl::Status SaveDungeonMapTile16(gfx::Tilemap& tile16_blockset, Rom& rom) {
  for (int i = 0; i < kNumDungeonMapTile16; i++) {
    int addr = kDungeonMapTile16;
    if (rom.data()[kDungeonMapExpCheck] != 0xB9) {
      addr = kDungeonMapTile16Expanded;
    }

    gfx::TileInfo t1 = tile16_blockset.tile_info[i][0];
    gfx::TileInfo t2 = tile16_blockset.tile_info[i][1];
    gfx::TileInfo t3 = tile16_blockset.tile_info[i][2];
    gfx::TileInfo t4 = tile16_blockset.tile_info[i][3];

    auto tl = gfx::TileInfoToWord(t1);
    RETURN_IF_ERROR(rom.WriteWord(addr + (i * 8), tl));

    auto tr = gfx::TileInfoToWord(t2);
    RETURN_IF_ERROR(rom.WriteWord(addr + 2 + (i * 8), tr));

    auto bl = gfx::TileInfoToWord(t3);
    RETURN_IF_ERROR(rom.WriteWord(addr + 4 + (i * 8), bl));

    auto br = gfx::TileInfoToWord(t4);
    RETURN_IF_ERROR(rom.WriteWord(addr + 6 + (i * 8), br));
  }
  return absl::OkStatus();
}

absl::Status LoadDungeonMapGfxFromBinary(Rom& rom, GameData* game_data,
                                         gfx::Tilemap& tile16_blockset,
                                         std::array<gfx::Bitmap, 4>& sheets,
                                         std::vector<uint8_t>& gfx_bin_data) {
  std::string bin_file = util::FileDialogWrapper::ShowOpenFileDialog();
  if (bin_file.empty()) {
    return absl::InternalError("No file selected");
  }

  std::ifstream file(bin_file, std::ios::binary);
  if (!file.is_open()) {
    return absl::InternalError("Failed to open file");
  }

  // Read the gfx data into a buffer
  std::vector<uint8_t> bin_data((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
  auto converted_bin = gfx::SnesTo8bppSheet(bin_data, 4, 4);
  gfx_bin_data = converted_bin;
  if (LoadDungeonMapTile16(tile16_blockset, rom, game_data, converted_bin, true)
          .ok()) {
    std::vector<std::vector<uint8_t>> gfx_sheets;
    for (int i = 0; i < 4; i++) {
      gfx_sheets.emplace_back(converted_bin.begin() + (i * 0x1000),
                              converted_bin.begin() + ((i + 1) * 0x1000));
      sheets[i] = gfx::Bitmap(128, 32, 8, gfx_sheets[i]);
      if (game_data) {
        sheets[i].SetPalette(
            *game_data->palette_groups.dungeon_main.mutable_palette(3));
      }

      // Queue texture creation via Arena's deferred system
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::CREATE, &sheets[i]);
    }
  }
  file.close();

  return absl::OkStatus();
}

}  // namespace yaze::zelda3
