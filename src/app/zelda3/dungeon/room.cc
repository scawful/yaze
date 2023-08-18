#include "room.h"

#include <cstdint>
#include <vector>

#include "app/core/common.h"
#include "app/core/constants.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/room_object.h"
#include "app/zelda3/sprite/sprite.h"

namespace yaze {
namespace app {
namespace zelda3 {
namespace dungeon {

void DrawDungeonRoomBG1(std::vector<uint8_t>& tiles_bg1_buffer,
                        std::vector<uint8_t>& current_gfx16,
                        std::vector<uint8_t>& room_bg1_data) {
  for (int yy = 0; yy < 64; ++yy) {
    for (int xx = 0; xx < 64; ++xx) {
      if (tiles_bg1_buffer[xx + (yy * 64)] != 0xFFFF) {
        auto t = gfx::GetTilesInfo(tiles_bg1_buffer[xx + (yy * 64)]);
        for (int yl = 0; yl < 8; ++yl) {
          for (int xl = 0; xl < 4; ++xl) {
            int mx = xl * (1 - t.horizontal_mirror_) +
                     (3 - xl) * t.horizontal_mirror_;
            int my =
                yl * (1 - t.vertical_mirror_) + (7 - yl) * t.vertical_mirror_;

            int ty = (t.id_ / 16) * 512;
            int tx = (t.id_ % 16) * 4;
            uint8_t pixel = current_gfx16[(tx + ty) + (yl * 64) + xl];

            int index = (xx * 8) + (yy * 4096) + ((mx * 2) + (my * 512));
            room_bg1_data[index + t.horizontal_mirror_ ^ 1] =
                static_cast<uint8_t>((pixel & 0x0F) + t.palette_ * 16);
            room_bg1_data[index + t.horizontal_mirror_] =
                static_cast<uint8_t>(((pixel >> 4) & 0x0F) + t.palette_ * 16);
          }
        }
      }
    }
  }
}

void DrawDungeonRoomBG2(std::vector<uint8_t>& tiles_bg2_buffer,
                        std::vector<uint8_t>& current_gfx16,
                        std::vector<uint8_t>& room_bg2_data) {
  for (int yy = 0; yy < 64; ++yy) {
    for (int xx = 0; xx < 64; ++xx) {
      if (tiles_bg2_buffer[xx + (yy * 64)] != 0xFFFF) {
        auto t = gfx::GetTilesInfo(tiles_bg2_buffer[xx + (yy * 64)]);
        for (int yl = 0; yl < 8; ++yl) {
          for (int xl = 0; xl < 4; ++xl) {
            int mx = xl * (1 - t.horizontal_mirror_) +
                     (3 - xl) * t.horizontal_mirror_;
            int my =
                yl * (1 - t.vertical_mirror_) + (7 - yl) * t.vertical_mirror_;

            int ty = (t.id_ / 16) * 512;
            int tx = (t.id_ % 16) * 4;
            uint8_t pixel = current_gfx16[(tx + ty) + (yl * 64) + xl];

            int index = (xx * 8) + (yy * 4096) + ((mx * 2) + (my * 512));
            room_bg2_data[index + t.horizontal_mirror_ ^ 1] =
                static_cast<uint8_t>((pixel & 0x0F) + t.palette_ * 16);
            room_bg2_data[index + t.horizontal_mirror_] =
                static_cast<uint8_t>(((pixel >> 4) & 0x0F) + t.palette_ * 16);
          }
        }
      }
    }
  }
}

// ==================================================================

void Room::LoadGfxGroups() {
  auto rom_data = rom()->vector();
  int gfxPointer =
      (rom_data[gfx_groups_pointer + 1] << 8) + rom_data[gfx_groups_pointer];
  gfxPointer = core::SnesToPc(gfxPointer);

  for (int i = 0; i < 37; i++) {
    for (int j = 0; j < 8; j++) {
      mainGfx[i][j] = rom_data[gfxPointer + (i * 8) + j];
    }
  }

  for (int i = 0; i < 82; i++) {
    for (int j = 0; j < 4; j++) {
      roomGfx[i][j] = rom_data[entrance_gfx_group + (i * 4) + j];
    }
  }

  for (int i = 0; i < 144; i++) {
    for (int j = 0; j < 4; j++) {
      spriteGfx[i][j] = rom_data[sprite_blockset_pointer + (i * 4) + j];
    }
  }

  for (int i = 0; i < 72; i++) {
    for (int j = 0; j < 4; j++) {
      paletteGfx[i][j] = rom_data[dungeons_palettes_groups + (i * 4) + j];
    }
  }
}

bool Room::SaveGroupsToROM() {
  auto rom_data = rom()->vector();
  int gfxPointer =
      (rom_data[gfx_groups_pointer + 1] << 8) + rom_data[gfx_groups_pointer];
  gfxPointer = core::SnesToPc(gfxPointer);

  for (int i = 0; i < 37; i++) {
    for (int j = 0; j < 8; j++) {
      rom()->Write(gfxPointer + (i * 8) + j, mainGfx[i][j]);
    }
  }

  for (int i = 0; i < 82; i++) {
    for (int j = 0; j < 4; j++) {
      rom()->Write(entrance_gfx_group + (i * 4) + j, roomGfx[i][j]);
    }
  }

  for (int i = 0; i < 144; i++) {
    for (int j = 0; j < 4; j++) {
      rom()->Write(sprite_blockset_pointer + (i * 4) + j, spriteGfx[i][j]);
    }
  }

  for (int i = 0; i < 72; i++) {
    for (int j = 0; j < 4; j++) {
      rom()->Write(dungeons_palettes_groups + (i * 4) + j, paletteGfx[i][j]);
    }
  }

  return false;
}

void Room::LoadSprites() {
  auto rom_data = rom()->vector();
  int spritePointer = (0x04 << 16) + (rom_data[rooms_sprite_pointer + 1] << 8) +
                      (rom_data[rooms_sprite_pointer]);
  int sprite_address_snes =
      (0x09 << 16) + (rom_data[spritePointer + (room_id_ * 2) + 1] << 8) +
      rom_data[spritePointer + (room_id_ * 2)];

  int sprite_address = core::SnesToPc(sprite_address_snes);
  bool sortsprites = rom_data[sprite_address] == 1;
  sprite_address += 1;

  while (true) {
    uint8_t b1 = rom_data[sprite_address];
    uint8_t b2 = rom_data[sprite_address + 1];
    uint8_t b3 = rom_data[sprite_address + 2];

    if (b1 == 0xFF) {
      break;
    }

    // sprites_.emplace_back(this, b3, (b2 & 0x1F), (b1 & 0x1F),
    //                       ((b2 & 0xE0) >> 5) + ((b1 & 0x60) >> 2),
    //                       (b1 & 0x80) >> 7);

    if (sprites_.size() > 1) {
      Sprite& spr = sprites_.back();
      Sprite& prevSprite = sprites_[sprites_.size() - 2];

      if (spr.id() == 0xE4 && spr.x() == 0x00 && spr.y() == 0x1E &&
          spr.layer() == 1 && spr.subtype() == 0x18) {
        // prevSprite.keyDrop() = 1;
        sprites_.pop_back();
      }

      if (spr.id() == 0xE4 && spr.x() == 0x00 && spr.y() == 0x1D &&
          spr.layer() == 1 && spr.subtype() == 0x18) {
        // prevSprite.keyDrop() = 2;
        sprites_.pop_back();
      }
    }

    sprite_address += 3;
  }
}

void Room::LoadChests() {
  auto rom_data = rom()->vector();
  int cpos = (rom_data[chests_data_pointer1 + 2] << 16) +
             (rom_data[chests_data_pointer1 + 1] << 8) +
             (rom_data[chests_data_pointer1]);
  cpos = core::SnesToPc(cpos);
  int clength = (rom_data[chests_length_pointer + 1] << 8) +
                (rom_data[chests_length_pointer]);

  for (int i = 0; i < clength; i++) {
    if ((((rom_data[cpos + (i * 3) + 1] << 8) + (rom_data[cpos + (i * 3)])) &
         0x7FFF) == room_id_) {
      // There's a chest in that room !
      bool big = false;
      if ((((rom_data[cpos + (i * 3) + 1] << 8) + (rom_data[cpos + (i * 3)])) &
           0x8000) == 0x8000)  // ?????
      {
        big = true;
      }

      chests_in_room.emplace_back(ChestData(rom_data[cpos + (i * 3) + 2], big));
    }
  }
}

void Room::LoadObjects() {
  auto rom_data = rom()->vector();
  int objectPointer = (rom_data[room_object_pointer + 2] << 16) +
                      (rom_data[room_object_pointer + 1] << 8) +
                      (rom_data[room_object_pointer]);
  objectPointer = core::SnesToPc(objectPointer);
  int room_address = objectPointer + (room_id_ * 3);

  int tile_address = (rom_data[room_address + 2] << 16) +
                     (rom_data[room_address + 1] << 8) + rom_data[room_address];

  int objects_location = core::SnesToPc(tile_address);

  if (objects_location == 0x52CA2) {
    std::cout << "Room ID : " << room_id_ << std::endl;
  }

  if (floor) {
    floor1 = static_cast<uint8_t>(rom_data[objects_location] & 0x0F);
    floor2 = static_cast<uint8_t>((rom_data[objects_location] >> 4) & 0x0F);
  }

  layout = static_cast<uint8_t>((rom_data[objects_location + 1] >> 2) & 0x07);

  LoadChests();

  staircaseRooms.clear();
  int nbr_of_staircase = 0;

  int pos = objects_location + 2;
  uint8_t b1 = 0;
  uint8_t b2 = 0;
  uint8_t b3 = 0;
  uint8_t posX = 0;
  uint8_t posY = 0;
  uint8_t sizeX = 0;
  uint8_t sizeY = 0;
  uint8_t sizeXY = 0;
  short oid = 0;
  int layer = 0;
  bool door = false;
  bool endRead = false;
  while (!endRead) {
    b1 = rom_data[pos];
    b2 = rom_data[pos + 1];

    if (b1 == 0xFF && b2 == 0xFF) {
      pos += 2;  // We jump to layer2
      layer++;
      door = false;
      if (layer == 3) {
        break;
      }
      continue;
    }

    if (b1 == 0xF0 && b2 == 0xFF) {
      pos += 2;  // We jump to layer2
      door = true;
      continue;
    }

    b3 = rom_data[pos + 2];
    if (door) {
      pos += 2;
    } else {
      pos += 3;
    }

    if (!door) {
      if (b3 >= 0xF8) {
        oid = static_cast<short>((b3 << 4) |
                                 0x80 + (((b2 & 0x03) << 2) + ((b1 & 0x03))));
        posX = static_cast<uint8_t>((b1 & 0xFC) >> 2);
        posY = static_cast<uint8_t>((b2 & 0xFC) >> 2);
        sizeXY = static_cast<uint8_t>((((b1 & 0x03) << 2) + (b2 & 0x03)));
      } else {
        oid = b3;
        posX = static_cast<uint8_t>((b1 & 0xFC) >> 2);
        posY = static_cast<uint8_t>((b2 & 0xFC) >> 2);
        sizeX = static_cast<uint8_t>((b1 & 0x03));
        sizeY = static_cast<uint8_t>((b2 & 0x03));
        sizeXY = static_cast<uint8_t>(((sizeX << 2) + sizeY));
      }

      if (b1 >= 0xFC) {
        oid = static_cast<short>((b3 & 0x3F) + 0x100);
        posX = static_cast<uint8_t>(((b2 & 0xF0) >> 4) + ((b1 & 0x3) << 4));
        posY = static_cast<uint8_t>(((b2 & 0x0F) << 2) + ((b3 & 0xC0) >> 6));
        sizeXY = 0;
      }

      RoomObject r =
          AddObject(oid, posX, posY, sizeXY, static_cast<uint8_t>(layer));

      /**
      if (r != nullptr) {
        tilesObjects.push_back(r);
      }


      for (short stair : stairsObjects) {
        if (stair == oid) {
          if (nbr_of_staircase < 4) {
            tilesObjects.back().options |= ObjectOption::Stairs;
            staircaseRooms.push_back(StaircaseRoom(
                posX, posY, "To " + staircase_rooms[nbr_of_staircase]));
            nbr_of_staircase++;
          } else {
            tilesObjects.back().options |= ObjectOption::Stairs;
            staircaseRooms.push_back(StaircaseRoom(posX, posY, "To ???"));
          }
        }
      }

      if (oid == 0xF99) {
        if (chests_in_room.size() > 0) {
          tilesObjects.back().options |= ObjectOption::Chest;
          chest_list.push_back(
              Chest(posX, posY, chests_in_room.front().itemIn, false));
          chests_in_room.erase(chests_in_room.begin());
        }
      } else if (oid == 0xFB1) {
        if (chests_in_room.size() > 0) {
          tilesObjects.back().options |= ObjectOption::Chest;
          chest_list.push_back(
              Chest(posX + 1, posY, chests_in_room.front().itemIn, true));
          chests_in_room.erase(chests_in_room.begin());
        }
      }
    } else {
      tilesObjects.push_back(object_door(static_cast<short>((b2 << 8) + b1), 0,
                                         0, 0, static_cast<uint8_t>(layer)));
    }

    **/
    }
  }
}

RoomObject Room::AddObject(short oid, uint8_t x, uint8_t y, uint8_t size,
                           uint8_t layer) {
  return RoomObject(oid, x, y, size, layer);
}

void Room::LoadRoomGraphics(uchar entrance_blockset) {
  for (int i = 0; i < 8; i++) {
    blocks[i] = mainGfx[BackgroundTileset][i];
    if (i >= 6 && i <= 6) {
      // 3-6
      if (entrance_blockset != 0xFF) {
        // 6 is wrong for the entrance? -NOP need to fix that
        // TODO: Find why this is wrong - Thats because of the stairs need to
        // find a workaround
        if (roomGfx[entrance_blockset][i - 3] != 0) {
          blocks[i] = roomGfx[entrance_blockset][i - 3];
        }
      }
    }
  }

  blocks[8] = 115 + 0;  // Static Sprites Blocksets (fairy,pot,ect...)
  blocks[9] = 115 + 10;
  blocks[10] = 115 + 6;
  blocks[11] = 115 + 7;
  for (int i = 0; i < 4; i++) {
    blocks[12 + i] = (uchar)(spriteGfx[SpriteTileset + 64][i] + 115);
  }  // 12-16 sprites

  auto newPdata = rom()->GetGraphicsBuffer();

  uchar* sheetsData = current_graphics_.data();
  // Into "room gfx16" 16 of them

  int sheetPos = 0;
  for (int i = 0; i < 16; i++) {
    int d = 0;
    int ioff = blocks[i] * 2048;
    while (d < 2048) {
      // NOTE LOAD BLOCKSETS SOMEWHERE FIRST
      uchar mapByte = newPdata[d + ioff];
      if (i < 4)  // removed switch
      {
        mapByte += 0x88;
      }  // Last line of 6, first line of 7 ?

      sheetsData[d + sheetPos] = mapByte;
      d++;
    }

    sheetPos += 2048;
  }

  LoadAnimatedGraphics();
}

void Room::LoadAnimatedGraphics() {
  int gfxanimatedPointer = core::SnesToPc(gfx_animated_pointer);

  auto newPdata = rom()->GetGraphicsBuffer();
  auto rom_data = rom()->vector();
  uchar* sheetsData = current_graphics_.data();
  int data = 0;
  while (data < 512) {
    uchar mapByte = newPdata[data + (92 * 2048) + (512 * animated_frame)];
    sheetsData[data + (7 * 2048)] = mapByte;

    mapByte =
        newPdata[data +
                 (rom_data[gfxanimatedPointer + BackgroundTileset] * 2048) +
                 (512 * animated_frame)];
    sheetsData[data + (7 * 2048) - 512] = mapByte;
    data++;
  }
}

void Room::LoadRoomFromROM() {
  // Load dungeon header
  auto rom_data = rom()->vector();
  int headerPointer = core::SnesToPc(room_header_pointer);

  message_id_ = messages_id_dungeon + (room_id_ * 2);

  int hpos = core::SnesToPc((rom_data[room_header_pointers_bank] << 16) |
                            headerPointer + (room_id_ * 2));
  hpos++;
  uchar b = rom_data[hpos];

  Layer2Mode = (uchar)(b >> 5);
  // TODO(@scawful): Make LayerMerging object.
  // LayerMerging = LayerMergeType.ListOf[(b & 0x0C) >> 2];

  IsDark = (b & 0x01) == 0x01;
  hpos++;

  Palette = rom_data[hpos];
  hpos++;

  BackgroundTileset = rom_data[hpos];
  hpos++;

  SpriteTileset = rom_data[hpos];
  hpos++;

  Layer2Behavior = rom_data[hpos];
  hpos++;

  Tag1 = rom_data[hpos];
  hpos++;

  Tag2 = rom_data[hpos];
  hpos++;

  b = rom_data[hpos];

  Pits.TargetLayer = (uchar)(b & 0x03);
  Stair1.TargetLayer = (uchar)((b >> 2) & 0x03);
  Stair2.TargetLayer = (uchar)((b >> 4) & 0x03);
  Stair3.TargetLayer = (uchar)((b >> 6) & 0x03);
  hpos++;
  Stair4.TargetLayer = (uchar)(rom_data[hpos] & 0x03);
  hpos++;

  Pits.Target = rom_data[hpos];
  hpos++;
  Stair1.Target = rom_data[hpos];
  hpos++;
  Stair2.Target = rom_data[hpos];
  hpos++;
  Stair3.Target = rom_data[hpos];
  hpos++;
  Stair4.Target = rom_data[hpos];
  hpos++;

  // Load room objects
  // int objectPointer = core::SnesToPc(room_object_pointer);
  // int room_address = objectPointer + (room_id_ * 3);
  // int objects_location = core::SnesToPc(room_address);

  // Load sprites
  // int spr_ptr = 0x040000 | rooms_sprite_pointer;
  // int sprite_address =
  //     core::SnesToPc(dungeon_spr_ptrs | spr_ptr + (room_id_ * 2));
}

}  // namespace dungeon
}  // namespace zelda3
}  // namespace app
}  // namespace yaze