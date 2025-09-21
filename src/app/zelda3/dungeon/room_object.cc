#include "room_object.h"

#include "absl/status/status.h"
#include "app/zelda3/dungeon/object_parser.h"

namespace yaze {
namespace zelda3 {

namespace {
struct SubtypeTableInfo {
  int base_ptr;   // base address of subtype table in ROM (PC)
  int index_mask; // mask to apply to object id for index
  
  SubtypeTableInfo(int base, int mask) : base_ptr(base), index_mask(mask) {}
};

SubtypeTableInfo GetSubtypeTable(int object_id) {
  // Heuristic: 0x00-0xFF => subtype1, 0x100-0x1FF => subtype2, >=0x200 => subtype3
  if (object_id >= 0x200) {
    return SubtypeTableInfo(kRoomObjectSubtype3, 0xFF);
  } else if (object_id >= 0x100) {
    return SubtypeTableInfo(kRoomObjectSubtype2, 0x7F);
  } else {
    return SubtypeTableInfo(kRoomObjectSubtype1, 0xFF);
  }
}
} // namespace

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
                          uint16_t tileUnder) {
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
      uint16_t td = 0;  // gfx::GetTilesInfo();

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

void RoomObject::EnsureTilesLoaded() {
  if (!tiles_.empty()) return;
  if (rom_ == nullptr) return;

  // Try the new parser first
  if (LoadTilesWithParser().ok()) {
    return;
  }

  // Fallback to old method
  auto rom_data = rom_->data();

  // Determine which subtype table to use and compute the tile data offset.
  SubtypeTableInfo sti = GetSubtypeTable(id_);
  int index = (id_ & sti.index_mask);
  int tile_ptr = sti.base_ptr + (index * 2);
  if (tile_ptr < 0 || tile_ptr + 1 >= (int)rom_->size()) return;

  int tile_rel = (int16_t)((rom_data[tile_ptr + 1] << 8) + rom_data[tile_ptr]);
  int pos = kRoomObjectTileAddress + tile_rel;

  // Read one 16x16 (4 words) worth of tile info as a preview.
  if (pos < 0 || pos + 7 >= (int)rom_->size()) return;
  uint16_t w0 = (uint16_t)(rom_data[pos] | (rom_data[pos + 1] << 8));
  uint16_t w1 = (uint16_t)(rom_data[pos + 2] | (rom_data[pos + 3] << 8));
  uint16_t w2 = (uint16_t)(rom_data[pos + 4] | (rom_data[pos + 5] << 8));
  uint16_t w3 = (uint16_t)(rom_data[pos + 6] | (rom_data[pos + 7] << 8));

  tiles_.push_back(gfx::Tile16(gfx::WordToTileInfo(w0), gfx::WordToTileInfo(w1),
                               gfx::WordToTileInfo(w2), gfx::WordToTileInfo(w3)));
}

absl::Status RoomObject::LoadTilesWithParser() {
  if (rom_ == nullptr) {
    return absl::InvalidArgumentError("ROM is null");
  }

  ObjectParser parser(rom_);
  auto result = parser.ParseObject(id_);
  if (!result.ok()) {
    return result.status();
  }

  tiles_ = std::move(result.value());
  return absl::OkStatus();
}

}  // namespace zelda3
}  // namespace yaze
