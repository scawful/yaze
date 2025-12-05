#include "room_layout.h"

#include "absl/strings/str_format.h"
#include "rom/snes.h"
#include "util/log.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/object_drawer.h"

namespace yaze::zelda3 {

namespace {
// kLayerTerminator unused
// ReadWord unused
}  // namespace

absl::StatusOr<int> RoomLayout::GetLayoutAddress(int layout_id) const {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  if (layout_id < 0 ||
      layout_id >= static_cast<int>(kRoomLayoutPointers.size())) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid layout id %d", layout_id));
  }

  uint32_t snes_addr = kRoomLayoutPointers[layout_id];
  int pc_addr = SnesToPc(static_cast<int>(snes_addr));
  if (pc_addr < 0 || pc_addr >= static_cast<int>(rom_->size())) {
    return absl::OutOfRangeError(
        absl::StrFormat("Layout pointer %d out of range", layout_id));
  }
  return pc_addr;
}

absl::Status RoomLayout::LoadLayout(int layout_id) {
  objects_.clear();

  auto addr_result = GetLayoutAddress(layout_id);
  if (!addr_result.ok()) {
    return addr_result.status();
  }

  int pos = addr_result.value();
  const auto& rom_data = rom_->data();
  int layer = 0;

  // DEBUG: Use printf for visibility (LOG_DEBUG may be filtered)
  printf("[RoomLayout] Loading layout %d from PC address 0x%05X\n", 
         layout_id, pos);
  fflush(stdout);  // Ensure output is visible
  LOG_DEBUG("[RoomLayout]", "Loading layout %d from PC address 0x%05X", 
            layout_id, pos);

  int obj_index = 0;
  while (pos + 2 < static_cast<int>(rom_->size())) {
    uint8_t b1 = rom_data[pos];
    uint8_t b2 = rom_data[pos + 1];

    if (b1 == 0xFF && b2 == 0xFF) {
      // $FF $FF is the END terminator for this layout
      // Each RoomLayout_XX in ROM is a single block terminated by $FF $FF
      // We should NOT continue reading - that would read the NEXT layout's data
      printf("[RoomLayout] Layout %d terminated at pos=0x%05X after %d objects\n",
             layout_id, pos, obj_index);
      fflush(stdout);
      break;
    }

    if (pos + 2 >= static_cast<int>(rom_->size())) {
      break;
    }

    uint8_t b3 = rom_data[pos + 2];
    pos += 3;

    RoomObject obj = RoomObject::DecodeObjectFromBytes(
        b1, b2, b3, static_cast<uint8_t>(layer));
    obj.SetRom(rom_);
    obj.EnsureTilesLoaded();
    objects_.push_back(obj);

    // Enhanced debug logging for ALL layouts to trace rendering issues
    // DEBUG: Use printf for wall/corner objects (LOG_DEBUG may be filtered)
    if (obj.id_ == 0x001 || obj.id_ == 0x002 || 
        obj.id_ == 0x061 || obj.id_ == 0x062 ||
        (obj.id_ >= 0x100 && obj.id_ <= 0x103)) {
      printf("[RoomLayout] Layout %d obj[%d]: bytes=[%02X,%02X,%02X] -> id=0x%03X x=%d y=%d size=%d layer=%d tiles=%zu\n",
             layout_id, obj_index, b1, b2, b3,
             obj.id_, obj.x_, obj.y_, obj.size_, layer, obj.tiles().size());
      fflush(stdout);  // Ensure output is visible
    }
    LOG_DEBUG("[RoomLayout]", 
              "Layout %d obj[%d]: bytes=[%02X,%02X,%02X] -> id=0x%03X x=%d y=%d size=%d layer=%d tiles=%zu",
              layout_id, obj_index, b1, b2, b3,
              obj.id_, obj.x_, obj.y_, obj.size_, layer, obj.tiles().size());
    obj_index++;
  }

  printf("[RoomLayout] Layout %d loaded with %zu objects\n", 
         layout_id, objects_.size());
  fflush(stdout);  // Ensure output is visible
  LOG_DEBUG("[RoomLayout]", "Layout %d loaded with %zu objects", 
            layout_id, objects_.size());

  return absl::OkStatus();
}

absl::Status RoomLayout::Draw(int room_id, const uint8_t* gfx_data,
                              gfx::BackgroundBuffer& bg1,
                              gfx::BackgroundBuffer& bg2,
                              const gfx::PaletteGroup& palette_group,
                              DungeonState* state) const {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  if (objects_.empty()) {
    return absl::OkStatus();
  }

  ObjectDrawer drawer(rom_, room_id, gfx_data);
  return drawer.DrawObjectList(objects_, bg1, bg2, palette_group, state);
}

}  // namespace yaze::zelda3
