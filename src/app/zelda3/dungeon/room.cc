#include "room.h"

#include "app/core/common.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"
#include "gui/canvas.h"

namespace yaze {
namespace app {
namespace zelda3 {
namespace dungeon {

void Room::LoadChests() {}

void Room::LoadBlocks() {}

void Room::LoadTorches() {}

void Room::LoadSecrets() {}

void Room::LoadObjectsFromArray(int loc) {}

void Room::LoadSpritesFromArray(int loc) {}

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