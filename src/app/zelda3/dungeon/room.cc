#include "room.h"

#include <dungeon.h>

#include <cstdint>
#include <vector>

#include "absl/strings/str_cat.h"
#include "app/core/constants.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/room_object.h"
#include "app/zelda3/dungeon/room_tag.h"
#include "app/zelda3/sprite/sprite.h"

namespace yaze {
namespace zelda3 {

void Room::LoadHeader() {
  int header_pointer = (rom()->data()[kRoomHeaderPointer + 2] << 16) +
                       (rom()->data()[kRoomHeaderPointer + 1] << 8) +
                       (rom()->data()[kRoomHeaderPointer]);
  header_pointer = core::SnesToPc(header_pointer);

  int address = (rom()->data()[kRoomHeaderPointerBank] << 16) +
                (rom()->data()[(header_pointer + 1) + (room_id_ * 2)] << 8) +
                rom()->data()[(header_pointer) + (room_id_ * 2)];

  auto header_location = core::SnesToPc(address);

  bg2_ = (z3_dungeon_background2)((rom()->data()[header_location] >> 5) & 0x07);
  collision_ = (CollisionKey)((rom()->data()[header_location] >> 2) & 0x07);
  is_light_ = ((rom()->data()[header_location]) & 0x01) == 1;

  if (is_light_) {
    bg2_ = z3_dungeon_background2::DarkRoom;
  }

  palette = ((rom()->data()[header_location + 1] & 0x3F));
  blockset = (rom()->data()[header_location + 2]);
  spriteset = (rom()->data()[header_location + 3]);
  effect_ = (EffectKey)((rom()->data()[header_location + 4]));
  tag1_ = (TagKey)((rom()->data()[header_location + 5]));
  tag2_ = (TagKey)((rom()->data()[header_location + 6]));

  staircase_plane_[0] = ((rom()->data()[header_location + 7] >> 2) & 0x03);
  staircase_plane_[1] = ((rom()->data()[header_location + 7] >> 4) & 0x03);
  staircase_plane_[2] = ((rom()->data()[header_location + 7] >> 6) & 0x03);
  staircase_plane_[3] = ((rom()->data()[header_location + 8]) & 0x03);

  holewarp = (rom()->data()[header_location + 9]);
  staircase_rooms_[0] = (rom()->data()[header_location + 10]);
  staircase_rooms_[1] = (rom()->data()[header_location + 11]);
  staircase_rooms_[2] = (rom()->data()[header_location + 12]);
  staircase_rooms_[3] = (rom()->data()[header_location + 13]);

  // Calculate the size of the room based on how many objects are used per room
  // Some notes from hacker Zarby89
  // vanilla rooms are using in average ~0x80 bytes
  // a "normal" person who wants more details than vanilla will use around 0x100
  // bytes per rooms you could fit 128 rooms like that in 1 bank
  // F8000 I don't remember if that's PC or snes tho
  // Check last rooms
  // F8000+(roomid*3)
  // So we want to search the rom() object at this addressed based on the room
  // ID since it's the roomid * 3 we will by pulling 3 bytes at a time We can do
  // this with the rom()->ReadByteVector(addr, size)
  try {
    // Existing room size address calculation...
    auto room_size_address = 0xF8000 + (room_id_ * 3);

    std::cout << "Room #" << room_id_ << " Address: " << std::hex
              << room_size_address << std::endl;

    // Reading bytes for long address construction
    uint8_t low = rom()->data()[room_size_address];
    uint8_t high = rom()->data()[room_size_address + 1];
    uint8_t bank = rom()->data()[room_size_address + 2];

    // Constructing the long address
    int long_address = (bank << 16) | (high << 8) | low;
    std::cout << std::hex << std::setfill('0') << std::setw(6) << long_address
              << std::endl;
    room_size_pointer_ = long_address;

    if (long_address == 0x0A8000) {
      // Blank room disregard in size calculation
      std::cout << "Size of Room #" << room_id_ << ": 0 bytes" << std::endl;
      room_size_ = 0;
    } else {
      // use the long address to calculate the size of the room
      // we will use the room_id_ to calculate the next room's address
      // and subtract the two to get the size of the room

      int next_room_address = 0xF8000 + ((room_id_ + 1) * 3);

      std::cout << "Next Room Address: " << std::hex << next_room_address
                << std::endl;

      // Reading bytes for long address construction
      uint8_t next_low = rom()->data()[next_room_address];
      uint8_t next_high = rom()->data()[next_room_address + 1];
      uint8_t next_bank = rom()->data()[next_room_address + 2];

      // Constructing the long address
      int next_long_address = (next_bank << 16) | (next_high << 8) | next_low;

      std::cout << std::hex << std::setfill('0') << std::setw(6)
                << next_long_address << std::endl;

      // Calculate the size of the room
      int room_size = next_long_address - long_address;
      room_size_ = room_size;
      std::cout << "Size of Room #" << room_id_ << ": " << std::dec << room_size
                << " bytes" << std::endl;
    }
  } catch (const std::exception &e) {
    std::cout << "Error: " << e.what() << std::endl;
  }
}

void Room::LoadRoomFromROM() {
  // Load dungeon header
  auto rom_data = rom()->vector();
  int header_pointer = core::SnesToPc(kRoomHeaderPointer);

  message_id_ = messages_id_dungeon + (room_id_ * 2);

  int address = (rom()->data()[kRoomHeaderPointerBank] << 16) +
                (rom()->data()[(header_pointer + 1) + (room_id_ * 2)] << 8) +
                rom()->data()[(header_pointer) + (room_id_ * 2)];

  int hpos = core::SnesToPc(address);
  hpos++;
  uint8_t b = rom_data[hpos];

  layer2_mode_ = (b >> 5);
  layer_merging_ = kLayerMergeTypeList[(b & 0x0C) >> 2];

  is_dark_ = (b & 0x01) == 0x01;
  hpos++;

  palette_ = rom_data[hpos];
  hpos++;

  background_tileset_ = rom_data[hpos];
  hpos++;

  sprite_tileset_ = rom_data[hpos];
  hpos++;

  layer2_behavior_ = rom_data[hpos];
  hpos++;

  tag1_ = (TagKey)rom_data[hpos];
  hpos++;

  tag2_ = (TagKey)rom_data[hpos];
  hpos++;

  b = rom_data[hpos];

  pits_.target_layer = (uchar)(b & 0x03);
  stair1_.target_layer = (uchar)((b >> 2) & 0x03);
  stair2_.target_layer = (uchar)((b >> 4) & 0x03);
  stair3_.target_layer = (uchar)((b >> 6) & 0x03);
  hpos++;
  stair4_.target_layer = (uchar)(rom_data[hpos] & 0x03);
  hpos++;

  pits_.target = rom_data[hpos];
  hpos++;
  stair1_.target = rom_data[hpos];
  hpos++;
  stair2_.target = rom_data[hpos];
  hpos++;
  stair3_.target = rom_data[hpos];
  hpos++;
  stair4_.target = rom_data[hpos];
  hpos++;

  // Load room objects
  int object_pointer = core::SnesToPc(room_object_pointer);
  int room_address = object_pointer + (room_id_ * 3);
  int objects_location = core::SnesToPc(room_address);

  // Load sprites
  // int spr_ptr = 0x040000 | rooms_sprite_pointer;
  // int sprite_address =
  //     core::SnesToPc(dungeon_spr_ptrs | spr_ptr + (room_id_ * 2));
}

void Room::LoadRoomGraphics(uchar entrance_blockset) {
  const auto &main_gfx = rom()->main_blockset_ids;
  const auto &room_gfx = rom()->room_blockset_ids;
  const auto &sprite_gfx = rom()->spriteset_ids;
  current_gfx16_.reserve(0x4000);

  for (int i = 0; i < 8; i++) {
    blocks_[i] = main_gfx[blockset][i];
    if (i >= 6 && i <= 6) {
      // 3-6
      if (entrance_blockset != 0xFF) {
        if (room_gfx[entrance_blockset][i - 3] != 0) {
          blocks_[i] = room_gfx[entrance_blockset][i - 3];
        }
      }
    }
  }

  blocks_[8] = 115 + 0;  // Static Sprites Blocksets (fairy,pot,ect...)
  blocks_[9] = 115 + 10;
  blocks_[10] = 115 + 6;
  blocks_[11] = 115 + 7;
  for (int i = 0; i < 4; i++) {
    blocks_[12 + i] = (uchar)(sprite_gfx[spriteset + 64][i] + 115);
  }  // 12-16 sprites
}

constexpr int kGfxBufferOffset = 92 * 2048;
constexpr int kGfxBufferStride = 512;
constexpr int kGfxBufferAnimatedFrameOffset = 7 * 2048;
constexpr int kGfxBufferAnimatedFrameStride = 512;
constexpr int kGfxBufferRoomOffset = 2048;
constexpr int kGfxBufferRoomSpriteOffset = 512;
constexpr int kGfxBufferRoomSpriteStride = 2048;
constexpr int kGfxBufferRoomSpriteLastLineOffset = 0x88;

void Room::CopyRoomGraphicsToBuffer() {
  auto gfx_buffer_data = rom()->graphics_buffer();

  // Copy room graphics to buffer
  int sheet_pos = 0;
  for (int i = 0; i < 16; i++) {
    int data = 0;
    int block_offset = blocks_[i] * kGfxBufferRoomOffset;
    while (data < kGfxBufferRoomOffset) {
      uchar map_byte = gfx_buffer_data[data + block_offset];
      if (i < 4) {
        map_byte += kGfxBufferRoomSpriteLastLineOffset;
      }

      current_gfx16_[data + sheet_pos] = map_byte;
      data++;
    }

    sheet_pos += kGfxBufferRoomOffset;
  }

  LoadAnimatedGraphics();
}

void Room::LoadAnimatedGraphics() {
  int gfx_ptr = core::SnesToPc(rom()->version_constants().kGfxAnimatedPointer);

  auto gfx_buffer_data = rom()->graphics_buffer();
  auto rom_data = rom()->vector();
  int data = 0;
  while (data < 512) {
    uchar map_byte =
        gfx_buffer_data[data + (92 * 2048) + (512 * animated_frame_)];
    current_gfx16_[data + (7 * 2048)] = map_byte;

    map_byte =
        gfx_buffer_data[data +
                        (rom_data[gfx_ptr + background_tileset_] * 2048) +
                        (512 * animated_frame_)];
    current_gfx16_[data + (7 * 2048) - 512] = map_byte;
    data++;
  }
}

void Room::LoadObjects() {
  auto rom_data = rom()->vector();
  int object_pointer = (rom_data[room_object_pointer + 2] << 16) +
                       (rom_data[room_object_pointer + 1] << 8) +
                       (rom_data[room_object_pointer]);
  object_pointer = core::SnesToPc(object_pointer);
  int room_address = object_pointer + (room_id_ * 3);

  int tile_address = (rom_data[room_address + 2] << 16) +
                     (rom_data[room_address + 1] << 8) + rom_data[room_address];

  int objects_location = core::SnesToPc(tile_address);

  if (objects_location == 0x52CA2) {
    std::cout << "Room ID : " << room_id_ << std::endl;
  }

  if (is_floor_) {
    floor1_graphics_ = static_cast<uint8_t>(rom_data[objects_location] & 0x0F);
    floor2_graphics_ =
        static_cast<uint8_t>((rom_data[objects_location] >> 4) & 0x0F);
  }

  layout = static_cast<uint8_t>((rom_data[objects_location + 1] >> 2) & 0x07);

  LoadChests();

  z3_staircases_.clear();
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
  bool end_read = false;
  while (!end_read) {
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

      RoomObject r(oid, posX, posY, sizeXY, static_cast<uint8_t>(layer));
      tile_objects_.push_back(r);

      for (short stair : stairsObjects) {
        if (stair == oid) {
          if (nbr_of_staircase < 4) {
            tile_objects_.back().set_options(ObjectOption::Stairs |
                                             tile_objects_.back().options());
            z3_staircases_.push_back(z3_staircase(
                posX, posY,
                absl::StrCat("To ", staircase_rooms_[nbr_of_staircase])
                    .data()));
            nbr_of_staircase++;
          } else {
            tile_objects_.back().set_options(ObjectOption::Stairs |
                                             tile_objects_.back().options());
            z3_staircases_.push_back(z3_staircase(posX, posY, "To ???"));
          }
        }
      }

      if (oid == 0xF99) {
        if (chests_in_room_.size() > 0) {
          tile_objects_.back().set_options(ObjectOption::Chest |
                                           tile_objects_.back().options());
          // chest_list_.push_back(
          //     Chest(posX, posY, chests_in_room_.front().itemIn, false));
          chests_in_room_.erase(chests_in_room_.begin());
        }
      } else if (oid == 0xFB1) {
        if (chests_in_room_.size() > 0) {
          tile_objects_.back().set_options(ObjectOption::Chest |
                                           tile_objects_.back().options());
          // chest_list_.push_back(
          //     Chest(posX + 1, posY, chests_in_room_.front().item_in, true));
          chests_in_room_.erase(chests_in_room_.begin());
        }
      }
    } else {
      // tile_objects_.push_back(object_door(static_cast<short>((b2 << 8) + b1),
      // 0,
      //                                   0, 0, static_cast<uint8_t>(layer)));
    }
  }
}

void Room::LoadSprites() {
  auto rom_data = rom()->vector();
  int sprite_pointer = (0x04 << 16) +
                       (rom_data[rooms_sprite_pointer + 1] << 8) +
                       (rom_data[rooms_sprite_pointer]);
  int sprite_address_snes =
      (0x09 << 16) + (rom_data[sprite_pointer + (room_id_ * 2) + 1] << 8) +
      rom_data[sprite_pointer + (room_id_ * 2)];

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

    sprites_.emplace_back(b3, (b2 & 0x1F), (b1 & 0x1F),
                          ((b2 & 0xE0) >> 5) + ((b1 & 0x60) >> 2),
                          (b1 & 0x80) >> 7);

    if (sprites_.size() > 1) {
      Sprite &spr = sprites_.back();
      Sprite &prevSprite = sprites_[sprites_.size() - 2];

      if (spr.id() == 0xE4 && spr.x() == 0x00 && spr.y() == 0x1E &&
          spr.layer() == 1 && spr.subtype() == 0x18) {
        prevSprite.set_key_drop(1);
        sprites_.pop_back();
      }

      if (spr.id() == 0xE4 && spr.x() == 0x00 && spr.y() == 0x1D &&
          spr.layer() == 1 && spr.subtype() == 0x18) {
        prevSprite.set_key_drop(2);
        sprites_.pop_back();
      }
    }

    sprite_address += 3;
  }
}

void Room::LoadChests() {
  auto rom_data = rom()->vector();
  uint32_t cpos = core::SnesToPc((rom_data[chests_data_pointer1 + 2] << 16) +
                                 (rom_data[chests_data_pointer1 + 1] << 8) +
                                 (rom_data[chests_data_pointer1]));
  size_t clength = (rom_data[chests_length_pointer + 1] << 8) +
                   (rom_data[chests_length_pointer]);

  for (int i = 0; i < clength; i++) {
    if ((((rom_data[cpos + (i * 3) + 1] << 8) + (rom_data[cpos + (i * 3)])) &
         0x7FFF) == room_id_) {
      // There's a chest in that room !
      bool big = false;
      if ((((rom_data[cpos + (i * 3) + 1] << 8) + (rom_data[cpos + (i * 3)])) &
           0x8000) == 0x8000) {
        big = true;
      }

      chests_in_room_.emplace_back(
          z3_chest_data(rom_data[cpos + (i * 3) + 2], big));
    }
  }
}

}  // namespace zelda3
}  // namespace yaze
