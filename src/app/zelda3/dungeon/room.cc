#include "room.h"

#include "app/core/common.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace zelda3 {
namespace dungeon {

void Room::LoadGfxGroups() {
  int gfxPointer =
      (rom_[gfx_groups_pointer + 1] << 8) + rom_[gfx_groups_pointer];
  gfxPointer = core::SnesToPc(gfxPointer);

  for (int i = 0; i < 37; i++) {
    for (int j = 0; j < 8; j++) {
      mainGfx[i][j] = rom_[gfxPointer + (i * 8) + j];
    }
  }

  for (int i = 0; i < 82; i++) {
    for (int j = 0; j < 4; j++) {
      roomGfx[i][j] = rom_[entrance_gfx_group + (i * 4) + j];
    }
  }

  for (int i = 0; i < 144; i++) {
    for (int j = 0; j < 4; j++) {
      spriteGfx[i][j] = rom_[sprite_blockset_pointer + (i * 4) + j];
    }
  }

  for (int i = 0; i < 72; i++) {
    for (int j = 0; j < 4; j++) {
      paletteGfx[i][j] = rom_[dungeons_palettes_groups + (i * 4) + j];
    }
  }
}

bool Room::SaveGroupsToROM() {
  int gfxPointer =
      (rom_[gfx_groups_pointer + 1] << 8) + rom_[gfx_groups_pointer];
  gfxPointer = core::SnesToPc(gfxPointer);

  for (int i = 0; i < 37; i++) {
    for (int j = 0; j < 8; j++) {
      rom_.Write(gfxPointer + (i * 8) + j, mainGfx[i][j]);
    }
  }

  for (int i = 0; i < 82; i++) {
    for (int j = 0; j < 4; j++) {
      rom_.Write(entrance_gfx_group + (i * 4) + j, roomGfx[i][j]);
    }
  }

  for (int i = 0; i < 144; i++) {
    for (int j = 0; j < 4; j++) {
      rom_.Write(sprite_blockset_pointer + (i * 4) + j, spriteGfx[i][j]);
    }
  }

  for (int i = 0; i < 72; i++) {
    for (int j = 0; j < 4; j++) {
      rom_.Write(dungeons_palettes_groups + (i * 4) + j, paletteGfx[i][j]);
    }
  }

  return false;
}

void Room::LoadChests() {
  // ChestList.Clear();

  // int cpos = rom_.Read24(core::constants::chests_data_pointer1).SNEStoPC();
  // int clength = rom_.Read16(core::constants::chests_length_pointer);

  // for (int i = 0; i < clength; i += 3) {
  //   ushort roomid = (ushort)(rom_.Read16(cpos) & 0x7FFF);
  //   cpos += 2;
  //   uchar item = rom_[cpos++];  // get now so cpos is incremented too

  //   if (roomid == RoomID) {
  //     ChestList.Add(new DungeonChestItem(ItemReceipt.GetTypeFromID(item)));
  //   }
  // }
}

void Room::LoadBlocks() {}

void Room::LoadTorches() {}

void Room::LoadSecrets() {}

void Room::Resync() {}

void Room::LoadObjectsFromArray(int loc) {}

void Room::LoadSpritesFromArray(int loc) {}

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

  auto newPdata = rom_.GetGraphicsBuffer();

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

  auto newPdata = rom_.GetGraphicsBuffer();

  uchar* sheetsData = current_graphics_.data();
  int data = 0;
  while (data < 512) {
    uchar mapByte = newPdata[data + (92 * 2048) + (512 * animated_frame)];
    sheetsData[data + (7 * 2048)] = mapByte;

    mapByte =
        newPdata[data + (rom_[gfxanimatedPointer + BackgroundTileset] * 2048) +
                 (512 * animated_frame)];
    sheetsData[data + (7 * 2048) - 512] = mapByte;
    data++;
  }
}

void Room::LoadRoomFromROM() {
  // Load dungeon header
  int headerPointer = core::SnesToPc(room_header_pointer);

  MessageID = messages_id_dungeon + (RoomID * 2);

  int hpos = core::SnesToPc((rom_[room_header_pointers_bank] << 16) |
                            headerPointer + (RoomID * 2));
  hpos++;
  uchar b = rom_[hpos];

  Layer2Mode = (uchar)(b >> 5);
  // TODO(@scawful): Make LayerMerging object.
  // LayerMerging = LayerMergeType.ListOf[(b & 0x0C) >> 2];

  IsDark = (b & 0x01) == 0x01;
  hpos++;

  Palette = rom_[hpos];
  hpos++;

  BackgroundTileset = rom_[hpos];
  hpos++;

  SpriteTileset = rom_[hpos];
  hpos++;

  Layer2Behavior = rom_[hpos];
  hpos++;

  Tag1 = rom_[hpos];
  hpos++;

  Tag2 = rom_[hpos];
  hpos++;

  b = rom_[hpos];

  Pits.TargetLayer = (uchar)(b & 0x03);
  Stair1.TargetLayer = (uchar)((b >> 2) & 0x03);
  Stair2.TargetLayer = (uchar)((b >> 4) & 0x03);
  Stair3.TargetLayer = (uchar)((b >> 6) & 0x03);
  hpos++;
  Stair4.TargetLayer = (uchar)(rom_[hpos] & 0x03);
  hpos++;

  Pits.Target = rom_[hpos];
  hpos++;
  Stair1.Target = rom_[hpos];
  hpos++;
  Stair2.Target = rom_[hpos];
  hpos++;
  Stair3.Target = rom_[hpos];
  hpos++;
  Stair4.Target = rom_[hpos];
  hpos++;

  // Load room objects
  int objectPointer = core::SnesToPc(room_object_pointer);
  int room_address = objectPointer + (RoomID * 3);

  int objects_location = core::SnesToPc(room_address);

  LoadObjectsFromArray(objects_location);

  // Load sprites
  int spr_ptr = 0x040000 | rooms_sprite_pointer;
  int sprite_address =
      core::SnesToPc(dungeon_spr_ptrs | spr_ptr + (RoomID * 2));
  LoadSpritesFromArray(sprite_address);

  // Load other stuff
  LoadChests();
  LoadBlocks();
  LoadTorches();
  LoadSecrets();
  Resync();
}

}  // namespace dungeon
}  // namespace zelda3
}  // namespace app
}  // namespace yaze