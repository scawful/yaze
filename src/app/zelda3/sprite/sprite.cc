#include "sprite.h"

#include "app/zelda3/overworld/overworld.h"

namespace yaze {
namespace app {
namespace zelda3 {

void Sprite::UpdateMapProperties(uint16_t map_id) {
  map_x_ = x_;
  map_y_ = y_;
  name_ = kSpriteDefaultNames[id_];
}

void Sprite::UpdateCoordinates(int map_x, int map_y) {
  map_x_ = map_x;
  map_y_ = map_y;
}

void Sprite::updateBBox() {
  lowerX_ = 1;
  lowerY_ = 1;
  higherX_ = 15;
  higherY_ = 15;
}

void Sprite::Draw() {
  uint8_t x = nx_;
  uint8_t y = ny_;

  if (overlord_ == 0x07) {
    if (id_ == 0x1A) {
      DrawSpriteTile((x * 16), (y * 16), 14, 6, 11);  // bomb
    } else if (id_ == 0x05) {
      DrawSpriteTile((x * 16), (y * 16) - 12, 12, 16, 12, false, true);
      DrawSpriteTile((x * 16), (y * 16), 0, 16, 12, false, true);
    } else if (id_ == 0x06) {
      DrawSpriteTile((x * 16), (y * 16), 10, 26, 14, true, true, 2,
                     2);  // snek
    } else if (id_ == 0x09) {
      DrawSpriteTile((x * 16), (y * 16), 6, 26, 14);
      DrawSpriteTile((x * 16) + 8, (y * 16) + 8, 8, 26, 14);
      DrawSpriteTile((x * 16), (y * 16) + 16, 10, 27, 14, false, false, 1, 1);
    } else if (id_ == 0x14) {
      DrawSpriteTile((x * 16), (y * 16) + 8, 12, 06, 12, false, false, 2,
                     1);  // shadow tile
      DrawSpriteTile((x * 16), (y * 16) - 8, 3, 29, 8, false, false, 1,
                     1);  // tile
      DrawSpriteTile((x * 16) + 8, (y * 16) - 8, 3, 29, 8, true, false, 1,
                     1);  // tile
      DrawSpriteTile((x * 16), (y * 16), 3, 29, 8, false, true, 1,
                     1);  // tile
      DrawSpriteTile((x * 16) + 8, (y * 16), 3, 29, 8, true, true, 1,
                     1);  // tile
    } else {
      DrawSpriteTile((x * 16), (y * 16), 4, 4, 5);
    }

    if (nx_ != x || ny_ != y) {
      bounding_box_.x = (lowerX_ + (nx_ * 16));
      bounding_box_.y = (lowerY_ + (ny_ * 16));
      bounding_box_.w = width_;
      bounding_box_.h = height_;
    } else {
      bounding_box_.x = (lowerX_ + (x * 16));
      bounding_box_.y = (lowerY_ + (y * 16));
      bounding_box_.w = width_;
      bounding_box_.h = height_;
    }

    return;
  }

  if (id_ == 0x00) {
    DrawSpriteTile((x * 16), (y * 16), 4, 28, 10);
  } else if (id_ == 0x01) {
    DrawSpriteTile((x * 16) - 8, (y * 16), 6, 24, 12, false, false, 2, 2);
    DrawSpriteTile((x * 16) + 8, (y * 16), 6, 24, 12, true, false, 2, 2);
  } else if (id_ == 0x02) {
    DrawSpriteTile((x * 16), (y * 16), 0, 16, 10);
  } else if (id_ == 0x04) {
    uint8_t p = 3;

    DrawSpriteTile((x * 16), (y * 16), 14, 28, p);
    DrawSpriteTile((x * 16), (y * 16), 14, 30, p);
  } else if (id_ == 0x05) {
    uint8_t p = 3;

    DrawSpriteTile((x * 16), (y * 16), 14, 28, p);
    DrawSpriteTile((x * 16), (y * 16), 14, 30, p);
  } else if (id_ == 0x06) {
    uint8_t p = 3;

    DrawSpriteTile((x * 16), (y * 16), 14, 28, p);
    DrawSpriteTile((x * 16), (y * 16), 14, 30, p);
  } else if (id_ == 0x07) {
    uint8_t p = 3;

    DrawSpriteTile((x * 16), (y * 16), 14, 28, p);
    DrawSpriteTile((x * 16), (y * 16), 14, 30, p);
  } else if (id_ == 0x08) {
    DrawSpriteTile((x * 16), (y * 16), 0, 24, 6);
    DrawSpriteTile((x * 16) + 4, (y * 16) + 6, 0, 24, 6, false, false, 1, 1);
  } else if (id_ == 0x09) {
    DrawSpriteTile((x * 16) - 22, (y * 16) - 24, 12, 24, 12, false, false, 2,
                   2);  // Moldorm tail
    DrawSpriteTile((x * 16) - 16, (y * 16) - 20, 8, 24, 12, false, false, 2,
                   2);  // Moldorm b2
    DrawSpriteTile((x * 16) - 12, (y * 16) - 16, 4, 24, 12, false, false, 4,
                   4);  // Moldorm b
    DrawSpriteTile((x * 16), (y * 16), 0, 24, 12, false, false, 4,
                   4);  // Moldorm head

    DrawSpriteTile((x * 16) + 20, (y * 16) + 12, 8, 26, 14, false, false, 2,
                   2);  // Moldorm eye
    DrawSpriteTile((x * 16) + 12, (y * 16) + 20, 8, 26, 14, false, false, 2,
                   2);  // Moldorm eye
  } else if (id_ == 0x0A) {
    DrawSpriteTile((x * 16), (y * 16), 0, 24, 8);
    DrawSpriteTile((x * 16) + 4, (y * 16) + 6, 0, 24, 8, false, false, 1, 1);
  } else if (id_ == 0x0B) {
    DrawSpriteTile((x * 16), (y * 16), 10, 30, 10);
  } else if (id_ == 0x0C) {
    DrawSpriteTile((x * 16), (y * 16), 0, 24, 8);
    DrawSpriteTile((x * 16) + 4, (y * 16) + 6, 0, 24, 8, false, false, 1, 1);
  } else if (id_ == 0x0D) {
    DrawSpriteTile((x * 16), (y * 16), 14, 28, 12);
  } else if (id_ == 0x0E) {
    DrawSpriteTile((x * 16), (y * 16), 8, 18, 10, false, false, 3, 2);
  } else if (id_ == 0x0F) {
    DrawSpriteTile((x * 16), (y * 16), 14, 24, 8, false, false, 2, 3);
    DrawSpriteTile((x * 16) + 16, (y * 16), 30, 8, 8, true, false, 1, 3);
  } else if (id_ == 0x10) {
    DrawSpriteTile((x * 16), (y * 16), 12, 31, 8, false, false, 1, 1);
  } else if (id_ == 0x11) {
    DrawSpriteTile((x * 16), (y * 16) + 16, 6, 16, 8, false, false, 2,
                   2);  // Feet
    DrawSpriteTile((x * 16) - 8, (y * 16) + 8, 4, 18, 8, false, false, 2,
                   2);  // Body1
    DrawSpriteTile((x * 16) + 8, (y * 16) + 8, 4, 18, 8, true, false, 2,
                   2);  // Body2
    DrawSpriteTile((x * 16), (y * 16), 0, 16, 8, false, false, 2,
                   2);  // Head
  } else if (id_ == 0x12) {
    DrawSpriteTile((x * 16), (y * 16) + 8, 8, 26, 8);
    DrawSpriteTile((x * 16), (y * 16), 6, 24, 8);
  } else if (id_ == 0x13) {
    DrawSpriteTile((x * 16), (y * 16), 4, 22, 2);
  } else if (id_ == 0x15) {
    // Antifairy
    DrawSpriteTile((x * 16) + 2, (y * 16) + 8, 3, 30, 5, false, false, 1, 1);
    DrawSpriteTile((x * 16) + 8, (y * 16) + 2, 3, 30, 5, false, false, 1, 1);
    DrawSpriteTile((x * 16) + 14, (y * 16) + 8, 3, 30, 5, false, false, 1, 1);
    DrawSpriteTile((x * 16) + 8, (y * 16) + 14, 3, 30, 5, false, false, 1, 1);
    DrawSpriteTile((x * 16) + 8, (y * 16) + 8, 1, 30, 5, false, false, 1,
                   1);  // Middle
  } else if (id_ == 0x16) {
    DrawSpriteTile((x * 16), (y * 16) + 8, 2, 26, 2);
    DrawSpriteTile((x * 16), (y * 16), 0, 26, 2);
  } else if (id_ == 0x17)  // Bush hoarder
  {
    DrawSpriteTile((x * 16), (y * 16), 8, 30, 10);
  } else if (id_ == 0x18)  // Mini moldorm
  {
    DrawSpriteTile((x * 16) + 13, (y * 16) + 17, 13, 21, 8, false, false, 1,
                   1);                                     // Tail
    DrawSpriteTile((x * 16) + 5, (y * 16) + 8, 2, 22, 8);  // Body
    DrawSpriteTile((x * 16), (y * 16), 0, 22, 8);          // Head
    DrawSpriteTile((x * 16), (y * 16) - 4, 13, 20, 8, false, false, 1,
                   1);  // Eyes
    DrawSpriteTile((x * 16) - 4, (y * 16), 13, 20, 8, false, false, 1,
                   1);     // Eyes
  } else if (id_ == 0x19)  // Poe - ghost
  {
    DrawSpriteTile((x * 16), (y * 16), 6, 31, 2);  //
  } else if (id_ == 0x1A)                          // Smith
  {
    // DrawSpriteTile((x*16), (y *16), 2, 4, 10,true); // Smitty
    // DrawSpriteTile((x*16)+12, (y *16) - 7, 0, 6, 10); // Hammer
    DrawSpriteTile((x * 16), (y * 16), 4, 22, 10);
  } else if (id_ == 0x1C)  // Statue
  {
    DrawSpriteTile((x * 16), (y * 16) + 8, 0, 28, 15);
    DrawSpriteTile((x * 16), (y * 16), 2, 28, 15, false, false, 1, 1);
    DrawSpriteTile((x * 16) + 8, (y * 16), 2, 28, 15, true, false, 1, 1);
  } else if (id_ == 0x1E)  // Crystal switch
  {
    DrawSpriteTile((x * 16), (y * 16), 4, 30, 5);
  } else if (id_ == 0x1F)  // Sick kid
  {
    DrawSpriteTile((x * 16) - 8, (y * 16) + 8, 10, 16, 14);
    DrawSpriteTile((x * 16) + 16 - 8, (y * 16) + 8, 10, 16, 14, true);
    DrawSpriteTile((x * 16) - 8, (y * 16) + 16, 10, 16, 14, false, true, 2, 2);
    DrawSpriteTile((x * 16) + 16 - 8, (y * 16) + 16, 10, 16, 14, true, true, 2,
                   2);
    DrawSpriteTile((x * 16), (y * 16) - 4, 14, 16, 10);
  } else if (id_ == 0x20) {
    DrawSpriteTile((x * 16), (y * 16), 2, 24, 7);
  } else if (id_ == 0x21)  // Push switch
  {
    DrawSpriteTile((x * 16) + 4, (y * 16) + 20, 13, 29, 3, false, false, 1, 1);
    DrawSpriteTile((x * 16) + 4, (y * 16) + 28, 12, 29, 3, false, false, 1, 1);
    DrawSpriteTile((x * 16), (y * 16) + 8, 10, 28, 3);
  } else if (id_ == 0x22)  // Rope
  {
    DrawSpriteTile((x * 16), (y * 16), 8, 26, 5);
  } else if (id_ == 0x23)  // Red bari
  {
    DrawSpriteTile((x * 16), (y * 16), 2, 18, 4, false, false, 1, 2);
    DrawSpriteTile((x * 16) + 8, (y * 16), 2, 18, 4, true, false, 1, 2);
  } else if (id_ == 0x24)  // Blue bari
  {
    DrawSpriteTile((x * 16), (y * 16), 2, 18, 6, false, false, 1, 2);
    DrawSpriteTile((x * 16) + 8, (y * 16), 2, 18, 6, true, false, 1, 2);
  } else if (id_ == 0x25)  // Talking tree?
  {
    // TODO: Add something here?
  } else if (id_ == 0x26)  // Hardhat beetle
  {
    if ((x & 0x01) == 0x00) {
      DrawSpriteTile((x * 16), (y * 16), 4, 20, 8);
      DrawSpriteTile((x * 16), (y * 16) - 6, 0, 20, 8);
    } else {
      DrawSpriteTile((x * 16), (y * 16), 4, 20, 10);
      DrawSpriteTile((x * 16), (y * 16) - 6, 0, 20, 10);
    }
  } else if (id_ == 0x27)  // Deadrock
  {
    DrawSpriteTile((x * 16), (y * 16), 2, 30, 10);
  } else if (id_ == 0x28)  // Npcs
  {
    DrawSpriteTile((x * 16), (y * 16), 14, 22, 10);
  } else if (id_ == 0x29)  // Npcs
  {
    DrawSpriteTile((x * 16), (y * 16), 14, 22, 10);
  } else if (id_ == 0x2A)  // Npcs
  {
    DrawSpriteTile((x * 16), (y * 16), 14, 22, 10);
  } else if (id_ == 0x2B)  // ???
  {
    DrawSpriteTile((x * 16), (y * 16), 14, 22, 10);
  } else if (id_ == 0x2C)  // Lumberjack
  {
    DrawSpriteTile((x * 16) - 24, (y * 16) + 12, 6, 26, 12, true);  // Body
    DrawSpriteTile((x * 16) - 24, (y * 16), 8, 26, 12, true);       // Head

    DrawSpriteTile((x * 16) - 14, (y * 16) + 12, 14, 27, 10, false, false, 1,
                   1);  // Saw left edge
    DrawSpriteTile((x * 16) - 6, (y * 16) + 12, 15, 27, 10, false, false, 1,
                   1);  // Saw left edge
    DrawSpriteTile((x * 16) + 2, (y * 16) + 12, 15, 27, 10, false, false, 1,
                   1);  // Saw left edge
    DrawSpriteTile((x * 16) + 10, (y * 16) + 12, 15, 27, 10, false, false, 1,
                   1);  // Saw left edge
    DrawSpriteTile((x * 16) + 18, (y * 16) + 12, 15, 27, 10, false, false, 1,
                   1);  // Saw left edge
    DrawSpriteTile((x * 16) + 26, (y * 16) + 12, 15, 27, 10, false, false, 1,
                   1);  // Saw left edge
    DrawSpriteTile((x * 16) + 34, (y * 16) + 12, 14, 27, 10, true, false, 1,
                   1);  // Saw left edge

    DrawSpriteTile((x * 16) + 40, (y * 16) + 12, 4, 26, 12);  // Body
    DrawSpriteTile((x * 16) + 40, (y * 16), 8, 26, 12);       // Head
  } else if (id_ == 0x2D)                                     // Npcs
  {
    DrawSpriteTile((x * 16), (y * 16), 14, 22, 10);
  } else if (id_ == 0x2E)  // Npcs
  {
    DrawSpriteTile((x * 16), (y * 16), 14, 22, 10);
  } else if (id_ == 0x2F)  // Npcs
  {
    DrawSpriteTile((x * 16), (y * 16), 14, 22, 10);
  } else if (id_ == 0x30)  // Npcs
  {
    DrawSpriteTile((x * 16), (y * 16), 14, 22, 10);
  } else if (id_ == 0x31)  // Npcs
  {
    DrawSpriteTile((x * 16), (y * 16), 14, 22, 10);
  } else if (id_ == 0x32)  // Npcs
  {
    DrawSpriteTile((x * 16), (y * 16), 14, 22, 10);
  }
  /*
  else if (id_== 0x33) // Pull for rupees
  {

  }
  */
  else if (id_ == 0x34)  // Npcs
  {
    DrawSpriteTile((x * 16), (y * 16), 14, 22, 10);
  } else if (id_ == 0x35)  // Npcs
  {
    DrawSpriteTile((x * 16), (y * 16), 14, 22, 10);
  } else if (id_ == 0x36)  // Npcs
  {
    DrawSpriteTile((x * 16), (y * 16), 14, 22, 10);
  }
  /*
  else if (id_== 0x37) // Waterfall
  {
  DrawSpriteTile((x*16), (y *16), 14, 6, 10);
  }
  */
  else if (id_ == 0x38)  // Arrowtarget
  {
    DrawSpriteTile((x * 16), (y * 16), 14, 22, 10);
  } else if (id_ == 0x39)  // Npcs
  {
    DrawSpriteTile((x * 16), (y * 16), 14, 22, 10);
  } else if (id_ == 0x3A)  // Npcs
  {
    DrawSpriteTile((x * 16), (y * 16), 14, 22, 10);
  } else if (id_ == 0x3B)  // Dash item
  {
  } else if (id_ == 0x3C)  // Npcs
  {
    DrawSpriteTile((x * 16), (y * 16), 14, 22, 10);
  } else if (id_ == 0x3D)  // Npcs
  {
    DrawSpriteTile((x * 16), (y * 16), 14, 22, 10);
  } else if (id_ == 0x3E)  // Npcs
  {
    DrawSpriteTile((x * 16), (y * 16), 14, 22, 10);
  } else if (id_ == 0x3F)  // Npcs
  {
    DrawSpriteTile((x * 16), (y * 16), 14, 22, 10);
  } else if (id_ == 0x40)  // Lightning lock (agah tower)
  {
    DrawSpriteTile((x * 16) - 24, (y * 16), 10, 28, 2, false, false, 1, 2);
    DrawSpriteTile((x * 16) - 16, (y * 16), 6, 30, 2);
    DrawSpriteTile((x * 16), (y * 16), 8, 30, 2);
    DrawSpriteTile((x * 16) + 16, (y * 16), 6, 30, 2);
    DrawSpriteTile((x * 16) + 24, (y * 16), 10, 28, 2, false, false, 1, 2);
  } else if (id_ == 0x41)  // Blue soldier
  {
    DrawSpriteTile((x * 16) - 4, (y * 16) + 8, 6, 20, 10);
    DrawSpriteTile((x * 16) + 12, (y * 16) + 8, 6, 20, 10, true, false, 1, 2);
    DrawSpriteTile((x * 16), (y * 16), 0, 20, 10);
    DrawSpriteTile((x * 16) + 12, (y * 16) + 8, 13, 22, 10, false, false, 1,
                   2);  // Shield
    DrawSpriteTile((x * 16) - 4, (y * 16) + 16, 14, 22, 10, false, true, 1,
                   2);     // Sword
  } else if (id_ == 0x42)  // Green soldier
  {
    DrawSpriteTile((x * 16) - 4, (y * 16) + 8, 6, 20, 12);
    DrawSpriteTile((x * 16) + 12, (y * 16) + 8, 6, 20, 12, true, false, 1, 2);
    DrawSpriteTile((x * 16), (y * 16), 0, 20, 12);
    DrawSpriteTile((x * 16) + 12, (y * 16) + 8, 13, 22, 12, false, false, 1,
                   2);  // Shield
    DrawSpriteTile((x * 16) - 4, (y * 16) + 16, 14, 22, 12, false, true, 1,
                   2);     // Sword
  } else if (id_ == 0x43)  // Red spear soldier
  {
    DrawSpriteTile((x * 16) - 4, (y * 16) + 8, 6, 20, 8);
    DrawSpriteTile((x * 16) + 12, (y * 16) + 8, 6, 20, 8, true, false, 1, 2);
    DrawSpriteTile((x * 16), (y * 16), 0, 20, 8);
    DrawSpriteTile((x * 16) + 12, (y * 16) + 8, 13, 22, 8, false, false, 1,
                   2);  // Shield
    DrawSpriteTile((x * 16) - 4, (y * 16) + 16, 11, 22, 8, false, true, 1,
                   2);     // Spear
  } else if (id_ == 0x44)  // Sword blue holding up
  {
    DrawSpriteTile((x * 16) + 4, (y * 16) + 8, 6, 16, 10);
    DrawSpriteTile((x * 16) - 4, (y * 16) + 8, 6, 20, 10, false, false, 1, 2);
    DrawSpriteTile((x * 16), (y * 16), 0, 16, 10);  // Head
    DrawSpriteTile((x * 16) + 12, (y * 16) + 8, 14, 22, 10, false, true, 1,
                   2);     // Sword
  } else if (id_ == 0x45)  // Green spear soldier
  {
    DrawSpriteTile((x * 16) - 4, (y * 16) + 8, 6, 20, 12);
    DrawSpriteTile((x * 16) + 12, (y * 16) + 8, 6, 20, 12, true, false, 1, 2);
    DrawSpriteTile((x * 16), (y * 16), 0, 20, 12);
    DrawSpriteTile((x * 16) + 12, (y * 16) + 8, 13, 22, 12, false, false, 1,
                   2);  // Shield
    DrawSpriteTile((x * 16) - 4, (y * 16) + 16, 11, 22, 12, false, true, 1,
                   2);     // Spear
  } else if (id_ == 0x46)  // Blue archer
  {
    DrawSpriteTile((x * 16) - 4, (y * 16) + 8, 6, 20, 10);
    DrawSpriteTile((x * 16) + 12, (y * 16) + 8, 6, 20, 10, true, false, 1, 2);
    DrawSpriteTile((x * 16), (y * 16), 0, 20, 10);  // Head
    DrawSpriteTile((x * 16), (y * 16) + 16, 10, 16, 10, false, false, 1,
                   1);  // Bow1
    DrawSpriteTile((x * 16) + 8, (y * 16) + 16, 10, 16, 10, true, false, 1,
                   1);     // Bow2
  } else if (id_ == 0x47)  // Green archer
  {
    DrawSpriteTile((x * 16), (y * 16) + 8, 14, 16, 12);
    DrawSpriteTile((x * 16), (y * 16), 0, 20, 12);
    DrawSpriteTile((x * 16), (y * 16) + 16, 10, 16, 12, false, false, 1,
                   1);  // Bow1
    DrawSpriteTile((x * 16) + 8, (y * 16) + 16, 10, 16, 12, true, false, 1,
                   1);     // Bow2
  } else if (id_ == 0x48)  // Javelin soldier red
  {
    DrawSpriteTile((x * 16) + 4, (y * 16) + 8, 6, 16, 8);
    DrawSpriteTile((x * 16) - 4, (y * 16) + 8, 6, 20, 8, false, false, 1, 2);
    DrawSpriteTile((x * 16), (y * 16), 0, 16, 8);  // Head
    DrawSpriteTile((x * 16) + 12, (y * 16) + 8, 11, 22, 8, false, true, 1,
                   2);     // Sword
  } else if (id_ == 0x49)  // Javelin soldier red from bush
  {
    DrawSpriteTile((x * 16) + 4, (y * 16) + 8, 6, 16, 8);
    DrawSpriteTile((x * 16) - 4, (y * 16) + 8, 6, 20, 8, false, false, 1, 2);
    DrawSpriteTile((x * 16), (y * 16), 0, 18, 8);  // Head
    DrawSpriteTile((x * 16), (y * 16) + 24, 0, 20, 2);
    DrawSpriteTile((x * 16) + 12, (y * 16) + 8, 11, 22, 8, false, true, 1,
                   2);     // Sword
  } else if (id_ == 0x4A)  // Red bomb soldier
  {
    DrawSpriteTile((x * 16) + 4, (y * 16) + 8, 6, 16, 8);
    DrawSpriteTile((x * 16) - 4, (y * 16) + 8, 6, 20, 8, false, false, 1, 2);
    DrawSpriteTile((x * 16), (y * 16), 0, 16, 8);            // Head
    DrawSpriteTile((x * 16) + 8, (y * 16) - 8, 14, 22, 11);  // Bomb
  } else if (id_ == 0x4B)  // Green soldier recruit
  {
    // 0,4
    DrawSpriteTile((x * 16), (y * 16), 6, 24, 12);
    DrawSpriteTile((x * 16), (y * 16) - 10, 0, 20, 12);
  } else if (id_ == 0x4C)  // Jazzhand
  {
    DrawSpriteTile((x * 16), (y * 16), 0, 26, 14, false, false, 6, 2);
  } else if (id_ == 0x4D)  // Rabit??
  {
    DrawSpriteTile((x * 16), (y * 16), 0, 26, 12, false, false, 6, 2);
  } else if (id_ == 0x4E)  // Popo1
  {
    DrawSpriteTile((x * 16), (y * 16), 0, 20, 10);
  } else if (id_ == 0x4F)  // Popo2
  {
    DrawSpriteTile((x * 16), (y * 16), 2, 20, 10);
  } else if (id_ == 0x50)  // Canon ball
  {
    DrawSpriteTile((x * 16), (y * 16), 0, 24, 10);
  } else if (id_ == 0x51)  // Armos
  {
    DrawSpriteTile((x * 16), (y * 16), 0, 28, 11, false, false, 2, 4);
  } else if (id_ == 0x53)  // Armos Knight
  {
    DrawSpriteTile((x * 16), (y * 16), 0, 28, 10, false, false, 4, 4);
  } else if (id_ == 0x54) {
    DrawSpriteTile((x * 16), (y * 16), 2, 28, 12);
    DrawSpriteTile((x * 16) + 8, (y * 16) + 10, 6, 28, 12);
    DrawSpriteTile((x * 16) + 16, (y * 16) + 18, 10, 28, 12);
  } else if (id_ == 0x55)  // Fireball Zora
  {
    DrawSpriteTile((x * 16), (y * 16), 4, 26, 11);
  } else if (id_ == 0x56)  // Zora
  {
    DrawSpriteTile((x * 16), (y * 16), 10, 20, 2);
    DrawSpriteTile((x * 16), (y * 16) + 8, 8, 30, 2);
  } else if (id_ == 0x57)  // Desert Rocks
  {
    DrawSpriteTile((x * 16), (y * 16), 14, 24, 2, false, false, 2, 4);
    DrawSpriteTile((x * 16) + 16, (y * 16), 14, 24, 2, true, false, 2, 4);
  } else if (id_ == 0x58)  // Crab
  {
    DrawSpriteTile((x * 16), (y * 16), 14, 24, 12);
    DrawSpriteTile((x * 16) + 16, (y * 16), 14, 24, 12, true);
  } else if (id_ == 0x5B)  // Spark
  {
    DrawSpriteTile((x * 16), (y * 16), 8, 18, 4);
  } else if (id_ == 0x5C)  // Spark
  {
    DrawSpriteTile((x * 16), (y * 16), 8, 18, 4, true);
  } else if (id_ == 0x5D)  // Roller vertical1
  {
    // Subset3
    if (((y * 16) & 0x10) == 0x10) {
      DrawSpriteTile((x * 16), (y * 16), 8, 24, 11);

      for (int i = 0; i < 7; i++) {
        DrawSpriteTile((x * 16) + 8 + (i * 16), (y * 16), 9, 24, 11);
      }

      DrawSpriteTile((x * 16) + (16 * 7), (y * 16), 8, 24, 11, true);
    } else {
      DrawSpriteTile((x * 16), (y * 16), 8, 24, 11);
      DrawSpriteTile((x * 16) + 16, (y * 16), 9, 24, 11);
      DrawSpriteTile((x * 16) + 32, (y * 16), 9, 24, 11);
      DrawSpriteTile((x * 16) + 48, (y * 16), 8, 24, 11, true);
    }

  } else if (id_ == 0x5E)  // Roller vertical2
  {
    // Subset3
    if (((y * 16) & 0x10) == 0x10) {
      DrawSpriteTile((x * 16), (y * 16), 8, 24, 11);

      for (int i = 0; i < 7; i++) {
        DrawSpriteTile((x * 16) + 8 + (i * 16), (y * 16), 9, 24, 11);
      }

      DrawSpriteTile((x * 16) + (16 * 7), (y * 16), 8, 24, 11, true);
    } else {
      DrawSpriteTile((x * 16), (y * 16), 8, 24, 11);
      DrawSpriteTile((x * 16) + 16, (y * 16), 9, 24, 11);
      DrawSpriteTile((x * 16) + 32, (y * 16), 9, 24, 11);
      DrawSpriteTile((x * 16) + 48, (y * 16), 8, 24, 11, true);
    }
  } else if (id_ == 0x5F)  // Roller horizontal
  {
    if (((x * 16) & 0x10) == 0x10) {
      DrawSpriteTile((x * 16), (y * 16), 14, 24, 11);
      DrawSpriteTile((x * 16), (y * 16) + 16, 14, 25, 11);
      DrawSpriteTile((x * 16), (y * 16) + 32, 14, 25, 11);
      DrawSpriteTile((x * 16), (y * 16) + 48, 14, 24, 11, false, true);
    } else {
      for (int i = 0; i < 7; i++) {
        DrawSpriteTile((x * 16), (y * 16) + i * 16, 14, 25, 11);
      }

      DrawSpriteTile((x * 16), (y * 16), 14, 24, 11);
      DrawSpriteTile((x * 16), (y * 16) + (7 * 16), 14, 24, 11, false, true);
    }
  } else if (id_ == 0x60)  // Roller horizontal2 (right to left)
  {
    // Subset3
    if (((x * 16) & 0x10) == 0x10) {
      DrawSpriteTile((x * 16), (y * 16), 14, 24, 11);
      DrawSpriteTile((x * 16), (y * 16) + 16, 14, 25, 11);
      DrawSpriteTile((x * 16), (y * 16) + 32, 14, 25, 11);
      DrawSpriteTile((x * 16), (y * 16) + 48, 14, 24, 11, false, true);
    } else {
      for (int i = 0; i < 7; i++) {
        DrawSpriteTile((x * 16), (y * 16) + i * 16, 14, 25, 11);
      }

      DrawSpriteTile((x * 16), (y * 16), 14, 24, 11);
      DrawSpriteTile((x * 16), (y * 16) + (7 * 16), 14, 24, 11, false, true);
    }

  } else if (id_ == 0x61)  // Beamos
  {
    DrawSpriteTile((x * 16), (y * 16) - 16, 8, 20, 14, false, false, 2, 4);
    DrawSpriteTile((x * 16) + 4, (y * 16) - 8, 10, 20, 14, false, false, 1, 1);
  } else if (id_ == 0x63)  // Devalant non-shooter
  {
    DrawSpriteTile((x * 16) - 8, (y * 16) - 8, 2, 16, 2);
    DrawSpriteTile((x * 16) + 8, (y * 16) - 8, 2, 16, 2, true);
    DrawSpriteTile((x * 16) - 8, (y * 16) + 8, 2, 16, 2, false, true);
    DrawSpriteTile((x * 16) + 8, (y * 16) + 8, 2, 16, 2, true, true);
    DrawSpriteTile((x * 16), (y * 16), 0, 16, 10);
  } else if (id_ == 0x64)  // Devalant non-shooter
  {
    DrawSpriteTile((x * 16) - 8, (y * 16) - 8, 2, 16, 2);
    DrawSpriteTile((x * 16) + 8, (y * 16) - 8, 2, 16, 2, true);
    DrawSpriteTile((x * 16) - 8, (y * 16) + 8, 2, 16, 2, false, true);
    DrawSpriteTile((x * 16) + 8, (y * 16) + 8, 2, 16, 2, true, true);
    DrawSpriteTile((x * 16), (y * 16), 0, 16, 8);
  } else if (id_ == 0x66)  // Moving wall canon right
  {
    DrawSpriteTile((x * 16), (y * 16), 14, 16, 14, true);
  } else if (id_ == 0x67)  // Moving wall canon right
  {
    DrawSpriteTile((x * 16), (y * 16), 14, 16, 14);
  } else if (id_ == 0x68)  // Moving wall canon right
  {
    DrawSpriteTile((x * 16), (y * 16), 12, 16, 14);
  } else if (id_ == 0x69)  // Moving wall canon right
  {
    DrawSpriteTile((x * 16), (y * 16), 12, 16, 14, false, true);
  } else if (id_ == 0x6A)  // Chainball soldier
  {
    DrawSpriteTile((x * 16) + 4, (y * 16) + 8, 6, 16, 14);
    DrawSpriteTile((x * 16) - 4, (y * 16) + 8, 6, 20, 14, false, false, 1, 2);
    DrawSpriteTile((x * 16), (y * 16), 0, 16, 14);             // Head
    DrawSpriteTile((x * 16) + 12, (y * 16) - 16, 10, 18, 14);  // Ball
  } else if (id_ == 0x6B)                                      // Cannon soldier
  {
    DrawSpriteTile((x * 16) + 4, (y * 16) + 8, 6, 16, 14);
    DrawSpriteTile((x * 16) - 4, (y * 16) + 8, 6, 20, 14, false, false, 1, 2);
    DrawSpriteTile((x * 16), (y * 16), 0, 16, 14);           // Head
    DrawSpriteTile((x * 16) + 12, (y * 16) + 8, 4, 18, 14);  // Cannon
  } else if (id_ == 0x6C)                                    // Mirror portal
  {
    // Useless
  } else if (id_ == 0x6D)  // Rat
  {
    DrawSpriteTile((x * 16), (y * 16), 14, 24, 5);
  } else if (id_ == 0x6E)  // Rope
  {
    DrawSpriteTile((x * 16), (y * 16), 10, 26, 5);
  } else if (id_ == 0x6F) {
    DrawSpriteTile((x * 16), (y * 16), 4, 24, 10);
  } else if (id_ == 0x70)  // Helma fireball
  {
    DrawSpriteTile((x * 16), (y * 16), 10, 28, 4);
  } else if (id_ == 0x71)  // Leever
  {
    DrawSpriteTile((x * 16), (y * 16), 6, 16, 4);
  } else if (id_ == 0x73)  // Uncle priest
  {
  } else if (id_ == 0x79)  // Bee
  {
    DrawSpriteTile((x * 16), (y * 16), 4, 14, 11, false, false, 1, 1);
  } else if (id_ == 0x7C)  // Skull head
  {
    DrawSpriteTile((x * 16), (y * 16), 0, 16, 10);
  } else if (id_ == 0x7D)  // Big spike
  {
    DrawSpriteTile((x * 16), (y * 16), 4, 28, 11);
    DrawSpriteTile((x * 16) + 16, (y * 16), 4, 28, 11, true);
    DrawSpriteTile((x * 16), (y * 16) + 16, 4, 28, 11, false, true);
    DrawSpriteTile((x * 16) + 16, (y * 16) + 16, 4, 28, 11, true, true);
  } else if (id_ == 0x7E)  // Guruguru clockwise
  {
    DrawSpriteTile((x * 16), (y * 16) - 14, 8, 18, 4);
    DrawSpriteTile((x * 16), (y * 16) - 28, 8, 18, 4);
    DrawSpriteTile((x * 16), (y * 16) - 42, 8, 18, 4);
    DrawSpriteTile((x * 16), (y * 16) - 56, 8, 18, 4);
  } else if (id_ == 0x7F)  // Guruguru Counterclockwise
  {
    DrawSpriteTile((x * 16), (y * 16) - 14, 8, 18, 4);
    DrawSpriteTile((x * 16), (y * 16) - 28, 8, 18, 4);
    DrawSpriteTile((x * 16), (y * 16) - 42, 8, 18, 4);
    DrawSpriteTile((x * 16), (y * 16) - 56, 8, 18, 4);
  } else if (id_ == 0x80)  // Winder (moving firebar)
  {
    DrawSpriteTile((x * 16), (y * 16), 8, 18, 4);
    DrawSpriteTile((x * 16) - 14, (y * 16), 8, 18, 4);
    DrawSpriteTile((x * 16) - 28, (y * 16), 8, 18, 4);
    DrawSpriteTile((x * 16) - 42, (y * 16), 8, 18, 4);
    DrawSpriteTile((x * 16) - 56, (y * 16), 8, 18, 4);
  } else if (id_ == 0x81)  // Water tektite
  {
    DrawSpriteTile((x * 16), (y * 16), 0, 24, 11);
  } else if (id_ == 0x82)  // circle antifairy
  {
    // Antifairy top
    DrawSpriteTile((x * 16 + 2) - 4, (y * 16 + 8) - 16, 3, 30, 5, false, false,
                   1, 1);
    DrawSpriteTile((x * 16 + 8) - 4, (y * 16 + 2) - 16, 3, 30, 5, false, false,
                   1, 1);
    DrawSpriteTile((x * 16 + 14) - 4, (y * 16 + 8) - 16, 3, 30, 5, false, false,
                   1, 1);
    DrawSpriteTile((x * 16 + 8) - 4, (y * 16 + 14) - 16, 3, 30, 5, false, false,
                   1, 1);
    DrawSpriteTile((x * 16 + 8) - 4, (y * 16 + 8) - 16, 1, 30, 5, false, false,
                   1, 1);  // Middle
                           // Left
    DrawSpriteTile((x * 16 + 2) - 16, (y * 16 + 8) - 4, 3, 30, 5, false, false,
                   1, 1);
    DrawSpriteTile((x * 16 + 8) - 16, (y * 16 + 2) - 4, 3, 30, 5, false, false,
                   1, 1);
    DrawSpriteTile((x * 16 + 14) - 16, (y * 16 + 8) - 4, 3, 30, 5, false, false,
                   1, 1);
    DrawSpriteTile((x * 16 + 8) - 16, (y * 16 + 14) - 4, 3, 30, 5, false, false,
                   1, 1);
    DrawSpriteTile((x * 16 + 8) - 16, (y * 16 + 8) - 4, 1, 30, 5, false, false,
                   1, 1);  // Middle

    DrawSpriteTile((x * 16 + 2) - 4, (y * 16 + 8) + 8, 3, 30, 5, false, false,
                   1, 1);
    DrawSpriteTile((x * 16 + 8) - 4, (y * 16 + 2) + 8, 3, 30, 5, false, false,
                   1, 1);
    DrawSpriteTile((x * 16 + 14) - 4, (y * 16 + 8) + 8, 3, 30, 5, false, false,
                   1, 1);
    DrawSpriteTile((x * 16 + 8) - 4, (y * 16 + 14) + 8, 3, 30, 5, false, false,
                   1, 1);
    DrawSpriteTile((x * 16 + 8) - 4, (y * 16 + 8) + 8, 1, 30, 5, false, false,
                   1, 1);  // Middle
                           // Left
    DrawSpriteTile((x * 16 + 2) + 8, (y * 16 + 8) - 4, 3, 30, 5, false, false,
                   1, 1);
    DrawSpriteTile((x * 16 + 8) + 8, (y * 16 + 2) - 4, 3, 30, 5, false, false,
                   1, 1);
    DrawSpriteTile((x * 16 + 14) + 8, (y * 16 + 8) - 4, 3, 30, 5, false, false,
                   1, 1);
    DrawSpriteTile((x * 16 + 8) + 8, (y * 16 + 14) - 4, 3, 30, 5, false, false,
                   1, 1);
    DrawSpriteTile((x * 16 + 8) + 8, (y * 16 + 8) - 4, 1, 30, 5, false, false,
                   1, 1);  // Middle
  } else if (id_ == 0x83)  // Green eyegore
  {
    DrawSpriteTile((x * 16), (y * 16), 12, 24, 14, false, false, 2, 3);
    DrawSpriteTile((x * 16) + 16, (y * 16), 12, 24, 14, true, false, 1, 3);
  } else if (id_ == 0x84)  // Red eyegore
  {
    DrawSpriteTile((x * 16), (y * 16), 12, 24, 8, false, false, 2, 3);
    DrawSpriteTile((x * 16) + 16, (y * 16), 12, 24, 8, true, false, 1, 3);
  } else if (id_ == 0x85)  // Yellow stalfos
  {
    DrawSpriteTile((x * 16), (y * 16), 10, 16, 11);
    DrawSpriteTile((x * 16), (y * 16) - 12, 0, 16, 11);  // Head
  } else if (id_ == 0x86)                                // Kodongo
  {
    DrawSpriteTile((x * 16), (y * 16), 4, 26, 14);
  } else if (id_ == 0x88)  // Mothula
  {
    DrawSpriteTile((x * 16), (y * 16), 8, 24, 14, false, false, 2, 4);
    DrawSpriteTile((x * 16) + 16, (y * 16), 8, 24, 14, true, false, 2, 4);
  } else if (id_ == 0x8A)  // Spike
  {
    DrawSpriteTile((x * 16), (y * 16), 6, 30, 15);
  } else if (id_ == 0x8B)  // Gibdo
  {
    DrawSpriteTile((x * 16), (y * 16), 10, 24, 14);
    DrawSpriteTile((x * 16), (y * 16) - 8, 0, 24, 14);
  } else if (id_ == 0x8C)  // Arrghus
  {
    DrawSpriteTile((x * 16), (y * 16), 0, 24, 14, false, false, 2, 4);
    DrawSpriteTile((x * 16) + 16, (y * 16), 0, 24, 14, true, false, 2, 4);
  } else if (id_ == 0x8D)  // Arrghus spawn
  {
    DrawSpriteTile((x * 16), (y * 16), 6, 24, 14);
  } else if (id_ == 0x8E)  // Terrorpin
  {
    DrawSpriteTile((x * 16), (y * 16), 14, 24, 12);
  } else if (id_ == 0x8F)  // Slime
  {
    DrawSpriteTile((x * 16), (y * 16), 0, 20, 12);
  } else if (id_ == 0x90)  // Wall master
  {
    DrawSpriteTile((x * 16), (y * 16), 6, 26, 12);
    DrawSpriteTile((x * 16) + 16, (y * 16), 15, 26, 12, false, false, 1, 1);
    DrawSpriteTile((x * 16) + 16, (y * 16) + 8, 9, 26, 12, false, false, 1, 2);
    DrawSpriteTile((x * 16), (y * 16) + 16, 10, 27, 12, false, false, 1, 1);
    DrawSpriteTile((x * 16) + 8, (y * 16) + 16, 8, 27, 12, false, false, 1, 1);
  } else if (id_ == 0x91)  // Stalfos knight
  {
    DrawSpriteTile((x * 16) - 2, (y * 16) + 12, 4, 22, 12, false, false, 1, 2);
    DrawSpriteTile((x * 16) + 10, (y * 16) + 12, 4, 22, 12, true, false, 1, 2);
    DrawSpriteTile((x * 16) - 4, (y * 16) + 4, 1, 22, 12);
    DrawSpriteTile((x * 16) + 12, (y * 16) + 4, 3, 22, 12, false, false, 1, 2);
    DrawSpriteTile((x * 16), (y * 16) - 8, 6, 20, 12);
  } else if (id_ == 0x92)  // Helmaking
  {
    DrawSpriteTile((x * 16), (y * 16) + 32, 14, 26, 14);
    DrawSpriteTile((x * 16) + 16, (y * 16) + 32, 0, 28, 14);
    DrawSpriteTile((x * 16) + 32, (y * 16) + 32, 14, 26, 14, true);

    DrawSpriteTile((x * 16), (y * 16) + 16 + 32, 2, 28, 14);
    DrawSpriteTile((x * 16) + 16, (y * 16) + 16 + 32, 4, 28, 14);
    DrawSpriteTile((x * 16) + 32, (y * 16) + 16 + 32, 2, 28, 14, true);

    DrawSpriteTile((x * 16) + 8, (y * 16) + 32 + 32, 6, 28, 14);
    DrawSpriteTile((x * 16) + 24, (y * 16) + 32 + 32, 6, 28, 14, true);
  } else if (id_ == 0x93)  // Bumper
  {
    DrawSpriteTile((x * 16), (y * 16), 12, 30, 7);
    DrawSpriteTile((x * 16) + 16, (y * 16), 12, 30, 7, true);
    DrawSpriteTile((x * 16) + 16, (y * 16) + 16, 12, 30, 7, true, true);
    DrawSpriteTile((x * 16), (y * 16) + 16, 12, 30, 7, false, true);
  } else if (id_ == 0x95)  // Right laser eye
  {
    DrawSpriteTile((x * 16), (y * 16), 9, 28, 3, true, false, 1, 2);
    DrawSpriteTile((x * 16), (y * 16) + 16, 9, 28, 3, true, true, 1, 1);
  } else if (id_ == 0x96)  // Left laser eye
  {
    DrawSpriteTile((x * 16) + 8, (y * 16) - 4, 9, 28, 3, false, false, 1, 2);
    DrawSpriteTile((x * 16) + 8, (y * 16) + 12, 9, 28, 3, false, true, 1, 1);
  } else if (id_ == 0x97)  // Right laser eye
  {
    DrawSpriteTile((x * 16), (y * 16), 6, 28, 3, false, false, 2, 1);
    DrawSpriteTile((x * 16) + 16, (y * 16), 6, 28, 3, true, false, 1, 1);
  } else if (id_ == 0x98)  // Right laser eye
  {
    DrawSpriteTile((x * 16), (y * 16), 6, 28, 3, false, true, 2, 1);
    DrawSpriteTile((x * 16) + 16, (y * 16), 6, 28, 3, true, true, 1, 1);
  } else if (id_ == 0x99) {
    DrawSpriteTile((x * 16), (y * 16), 6, 24, 12);
    DrawSpriteTile((x * 16), (y * 16) - 8, 0, 24, 12);
  } else if (id_ == 0x9A)  // Water bubble kyameron
  {
    DrawSpriteTile((x * 16), (y * 16), 10, 24, 6);
  } else if (id_ == 0x9B)  // Water bubble kyameron
  {
    DrawSpriteTile((x * 16), (y * 16), 6, 24, 11);
    DrawSpriteTile((x * 16), (y * 16) - 8, 2, 27, 11, false, false, 2, 1);
  } else if (id_ == 0x9C)  // Water bubble kyameron
  {
    DrawSpriteTile((x * 16), (y * 16), 12, 22, 11);
    DrawSpriteTile((x * 16) + 16, (y * 16), 13, 22, 11, false, false, 1, 2);
  } else if (id_ == 0x9D)  // Water bubble kyameron
  {
    DrawSpriteTile((x * 16), (y * 16), 14, 21, 11);
    DrawSpriteTile((x * 16), (y * 16) - 16, 14, 20, 11, false, false, 2, 1);
  } else if (id_ == 0xA1) {
    DrawSpriteTile((x * 16) - 8, (y * 16) + 8, 6, 26, 14);
    DrawSpriteTile((x * 16) + 8, (y * 16) + 8, 6, 26, 14, true);
  } else if (id_ == 0xA2) {
    DrawSpriteTile((x * 16), (y * 16) + 8, 0, 24, 14, false, false, 4, 4);
  } else if (id_ == 0xA5) {
    DrawSpriteTile((x * 16), (y * 16), 0, 26, 10, false, false, 3, 2);
    DrawSpriteTile((x * 16) + 4, (y * 16) - 8, 0, 24, 10);
  } else if (id_ == 0xA6) {
    DrawSpriteTile((x * 16), (y * 16), 0, 26, 8, false, false, 3, 2);
    DrawSpriteTile((x * 16) + 4, (y * 16) - 8, 0, 24, 8);
  } else if (id_ == 0xA7) {
    DrawSpriteTile((x * 16), (y * 16) + 12, 12, 16, 10);
    DrawSpriteTile((x * 16), (y * 16), 0, 16, 10);
  } else if (id_ == 0xAC) {
    DrawSpriteTile((x * 16), (y * 16), 5, 14, 4);
  } else if (id_ == 0xAD) {
    DrawSpriteTile((x * 16), (y * 16) + 8, 14, 10, 10);
    DrawSpriteTile((x * 16), (y * 16), 12, 10, 10);
  } else if (id_ == 0xBA) {
    DrawSpriteTile((x * 16), (y * 16), 14, 14, 6);
  } else if (id_ == 0xC1 || id_ == 0x7A) {
    DrawSpriteTile((x * 16), (y * 16) - 16, 2, 24, 12, false, false, 2, 4);
    DrawSpriteTile((x * 16) + 16, (y * 16) - 16, 2, 24, 12, true, false, 2, 4);
  } else if (id_ == 0xC3) {
    DrawSpriteTile((x * 16), (y * 16), 10, 14, 12);
  } else if (id_ == 0xC4) {
    DrawSpriteTile((x * 16), (y * 16), 0, 18, 14);
    DrawSpriteTile((x * 16), (y * 16) - 8, 0, 16, 14);
  } else if (id_ == 0xC5) {
  } else if (id_ == 0xC6) {
    DrawSpriteTile((x * 16) + 4, (y * 16) + 14, 3, 30, 14, false, false, 1, 1);
    DrawSpriteTile((x * 16) + 14, (y * 16) + 4, 3, 30, 14, false, false, 1, 1);
    DrawSpriteTile((x * 16) + 4, (y * 16) + 2, 1, 31, 14, false, false, 1, 1);
    DrawSpriteTile((x * 16) - 6, (y * 16) + 4, 3, 30, 14, false, false, 1, 1);
    DrawSpriteTile((x * 16) + 4, (y * 16) - 6, 3, 30, 14, false, false, 1, 1);
  } else if (id_ == 0xC7) {
    DrawSpriteTile((x * 16), (y * 16), 0, 26, 4);
    DrawSpriteTile((x * 16), (y * 16) - 10, 0, 26, 4);
    DrawSpriteTile((x * 16), (y * 16) - 20, 0, 26, 4);
    DrawSpriteTile((x * 16), (y * 16) - 30, 2, 26, 4);
  } else if (id_ == 0xC8) {
    DrawSpriteTile((x * 16), (y * 16), 12, 24, 12, false, false, 2, 3);
    DrawSpriteTile((x * 16) + 16, (y * 16), 12, 24, 12, true, false, 1, 3);
  } else if (id_ == 0xC9) {
    DrawSpriteTile((x * 16), (y * 16), 8, 28, 8, false);
    DrawSpriteTile((x * 16) + 16, (y * 16), 8, 28, 8, true);
  } else if (id_ == 0xCA) {
    DrawSpriteTile((x * 16), (y * 16), 8, 10, 10);
  } else if (id_ == 0xD0) {
    DrawSpriteTile((x * 16), (y * 16), 7, 14, 11, false, false, 3, 2);
    DrawSpriteTile((x * 16), (y * 16) - 10, 8, 12, 11);
  } else if (id_ == 0xD1) {
    DrawSpriteTile((x * 16) + 2, (y * 16) + 8, 7, 13, 11, true, true, 1, 1);
    DrawSpriteTile((x * 16) + 8, (y * 16) + 2, 7, 13, 11, true, false, 1, 1);
    DrawSpriteTile((x * 16) + 14, (y * 16) + 8, 7, 13, 11, true, true, 1, 1);
    DrawSpriteTile((x * 16) + 8, (y * 16) + 14, 7, 13, 11, false, true, 1, 1);
    DrawSpriteTile((x * 16) + 8, (y * 16) + 8, 7, 13, 11, false, false, 1,
                   1);  // Middle
                        // 6,7 / 13
  } else if (id_ == 0xD4) {
    DrawSpriteTile((x * 16) - 4, (y * 16), 0, 7, 7, false, false, 1, 1);
    DrawSpriteTile((x * 16) + 4, (y * 16), 0, 7, 7, true, false, 1, 1);
  } else if (id_ == 0xE3)  // Fairy
  {
    DrawSpriteTile((x * 16), (y * 16), 10, 14, 10);
  } else if (id_ == 0xE4)  // Key
  {
    DrawSpriteTile((x * 16), (y * 16), 11, 06, 11, false, false, 1, 2);
  } else if (id_ == 0xE7)  // Mushroom
  {
    DrawSpriteTile((x * 16), (y * 16), 14, 30, 16);
  } else if (id_ == 0xE8)  // Fake ms
  {
    DrawSpriteTile((x * 16) + 4, (y * 16), 4, 31, 10, false, false, 1, 1);
    DrawSpriteTile((x * 16) + 4, (y * 16) + 8, 5, 31, 10, false, false, 1, 1);
  } else if (id_ == 0xEB) {
    DrawSpriteTile((x * 16), (y * 16), 0, 14, 5);
  } else if (id_ == 0xF2) {
    DrawSpriteTile((x * 16), (y * 16) - 16, 12, 24, 2, false, false, 2, 4);
    DrawSpriteTile((x * 16) + 16, (y * 16) - 16, 12, 24, 2, true, false, 2, 4);
  } else if (id_ == 0xF4) {
    DrawSpriteTile((x * 16), (y * 16), 12, 28, 5, false, false, 4, 4);
  } else {
    DrawSpriteTile((x * 16), (y * 16), 4, 4, 5);
  }

  bounding_box_.x = (lowerX_ + (x * 16));
  bounding_box_.y = (lowerY_ + (y * 16));
  bounding_box_.w = width_;
  bounding_box_.h = height_;
}

void Sprite::DrawSpriteTile(int x, int y, int srcx, int srcy, int pal,
                            bool mirror_x, bool mirror_y, int sizex,
                            int sizey) {
  if (current_gfx_.empty()) {
    std::cout << "No gfx loaded" << std::endl;
    return;
  }

  x += 16;
  y += 16;
  int drawid_ = (srcx + (srcy * 16)) + 512;
  for (auto yl = 0; yl < sizey * 8; yl++) {
    for (auto xl = 0; xl < (sizex * 8) / 2; xl++) {
      int mx = xl;
      int my = yl;

      if (mirror_x) {
        mx = (((sizex * 8) / 2) - 1) - xl;
      }
      if (mirror_y) {
        my = ((sizey * 8) - 1) - yl;
      }

      // Formula information to get tile index position in the array
      //((ID / nbrofXtiles) * (imgwidth/2) + (ID - ((ID/16)*16) ))

      int tx = ((drawid_ / 0x10) * 0x400) +
               ((drawid_ - ((drawid_ / 0x10) * 0x10)) * 8);
      auto pixel = current_gfx_[tx + (yl * 0x80) + xl];
      // nx,ny = object position, xx,yy = tile position, xl,yl = pixel
      // position
      int index = (x) + (y * 64) + (mx + (my * 0x80));

      if (index >= 0 && index <= 4096) {
        preview_gfx_[index] = (uint8_t)((pixel & 0x0F) + 112 + (pal * 8));
      }
    }
  }
}

}  // namespace zelda3
}  // namespace app
}  // namespace yaze
