#include "room_object.h"

namespace yaze {
namespace zelda3 {
namespace dungeon {

ObjectOption operator|(ObjectOption lhs, ObjectOption rhs) {
  return static_cast<ObjectOption>(static_cast<int>(lhs) |
                                   static_cast<int>(rhs));
}

ObjectOption operator&(ObjectOption lhs, ObjectOption rhs) {
  return static_cast<ObjectOption>(static_cast<int>(lhs) &
                                   static_cast<int>(rhs));
}

ObjectOption operator^(ObjectOption lhs, ObjectOption rhs) {
  return static_cast<ObjectOption>(static_cast<int>(lhs) ^
                                   static_cast<int>(rhs));
}

ObjectOption operator~(ObjectOption option) {
  return static_cast<ObjectOption>(~static_cast<int>(option));
}

SubtypeInfo FetchSubtypeInfo(uint16_t object_id) {
  SubtypeInfo info;

  // TODO: Determine the subtype based on object_id
  uint8_t subtype = 1;

  switch (subtype) {
    case 1:  // Subtype 1
      info.subtype_ptr = kRoomObjectSubtype1 + (object_id & 0xFF) * 2;
      info.routine_ptr = kRoomObjectSubtype1 + 0x200 + (object_id & 0xFF) * 2;
      break;
    case 2:  // Subtype 2
      info.subtype_ptr = kRoomObjectSubtype2 + (object_id & 0x7F) * 2;
      info.routine_ptr = kRoomObjectSubtype2 + 0x80 + (object_id & 0x7F) * 2;
      break;
    case 3:  // Subtype 3
      info.subtype_ptr = kRoomObjectSubtype3 + (object_id & 0xFF) * 2;
      info.routine_ptr = kRoomObjectSubtype3 + 0x100 + (object_id & 0xFF) * 2;
      break;
    default:
      throw std::runtime_error("Invalid object subtype");
  }

  return info;
}

void RoomObject::DrawTile(gfx::Tile16 t, int xx, int yy,
                          std::vector<uint8_t>& current_gfx16,
                          std::vector<uint8_t>& tiles_bg1_buffer,
                          std::vector<uint8_t>& tiles_bg2_buffer,
                          ushort tileUnder) {
  bool preview = false;
  if (width_ < xx + 8) {
    width_ = xx + 8;
  }
  if (height_ < yy + 8) {
    height_ = yy + 8;
  }
  if (preview) {
    if (xx < 0x39 && yy < 0x39 && xx >= 0 && yy >= 0) {
      gfx::TileInfo ti;  // t.GetTileInfo();
      for (auto yl = 0; yl < 8; yl++) {
        for (auto xl = 0; xl < 4; xl++) {
          int mx = xl;
          int my = yl;
          uint8_t r = 0;

          if (ti.horizontal_mirror_) {
            mx = 3 - xl;
            r = 1;
          }
          if (ti.vertical_mirror_) {
            my = 7 - yl;
          }

          // Formula information to get tile index position in the array.
          //((ID / nbrofXtiles) * (imgwidth/2) + (ID - ((ID/16)*16) ))
          int tx = ((ti.id_ / 0x10) * 0x200) +
                   ((ti.id_ - ((ti.id_ / 0x10) * 0x10)) * 4);
          auto pixel = current_gfx16[tx + (yl * 0x40) + xl];
          // nx,ny = object position, xx,yy = tile position, xl,yl = pixel
          // position

          int index =
              ((xx / 8) * 8) + ((yy / 8) * 0x200) + ((mx * 2) + (my * 0x40));
          preview_object_data_[index + r ^ 1] =
              (uint8_t)((pixel & 0x0F) + ti.palette_ * 0x10);
          preview_object_data_[index + r] =
              (uint8_t)(((pixel >> 4) & 0x0F) + ti.palette_ * 0x10);
        }
      }
    }
  } else {
    if (((xx / 8) + nx_ + offset_x_) + ((ny_ + offset_y_ + (yy / 8)) * 0x40) <
            0x1000 &&
        ((xx / 8) + nx_ + offset_x_) + ((ny_ + offset_y_ + (yy / 8)) * 0x40) >=
            0) {
      ushort td = 0;  // gfx::GetTilesInfo();

      // collisionPoint.Add(
      //     new Point(xx + ((nx + offsetX) * 8), yy + ((ny + +offsetY) * 8)));

      if (layer_ == 0 || (uint8_t)layer_ == 2 || all_bgs_) {
        if (tileUnder ==
            tiles_bg1_buffer[((xx / 8) + offset_x_ + nx_) +
                             ((ny_ + offset_y_ + (yy / 8)) * 0x40)]) {
          return;
        }

        tiles_bg1_buffer[((xx / 8) + offset_x_ + nx_) +
                         ((ny_ + offset_y_ + (yy / 8)) * 0x40)] = td;
      }

      if ((uint8_t)layer_ == 1 || all_bgs_) {
        if (tileUnder ==
            tiles_bg2_buffer[((xx / 8) + nx_ + offset_x_) +
                             ((ny_ + offset_y_ + (yy / 8)) * 0x40)]) {
          return;
        }

        tiles_bg2_buffer[((xx / 8) + nx_ + offset_x_) +
                         ((ny_ + offset_y_ + (yy / 8)) * 0x40)] = td;
      }
    }
  }
}

}  // namespace dungeon
}  // namespace zelda3

}  // namespace yaze
