#include "room_layout.h"

#include "absl/strings/str_format.h"
#include "rom/snes.h"
#include "util/log.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/object_drawer.h"

namespace yaze::zelda3 {

namespace {

// Returns true if the object ID is a pit or layer mask that should be on BG2.
// These objects create transparency holes in BG1 to show BG2 content through.
// Based on ALTTP object IDs and draw routine analysis.
bool IsPitOrMaskObject(int16_t id) {
  // BigHole4x4 (0xA4)
  if (id == 0xA4) return true;

  // Pit edge objects (0x9B - 0xA6 range)
  if (id >= 0x9B && id <= 0xA6) return true;

  // Type 3 pit objects (0xFE6)
  if (id == 0xFE6) return true;

  // Layer 2 pit mask objects (Type 3: 0xFBE, 0xFBF)
  if (id == 0xFBE || id == 0xFBF) return true;

  return false;
}

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

  LOG_DEBUG("RoomLayout", "Loading layout %d from PC address 0x%05X",
            layout_id, pos);

  int obj_index = 0;
  while (pos + 2 < static_cast<int>(rom_->size())) {
    uint8_t b1 = rom_data[pos];
    uint8_t b2 = rom_data[pos + 1];

    if (b1 == 0xFF && b2 == 0xFF) {
      // $FF $FF is the END terminator for this layout
      LOG_DEBUG("RoomLayout", "Layout %d terminated at pos=0x%05X after %d objects",
                layout_id, pos, obj_index);
      break;
    }

    if (pos + 2 >= static_cast<int>(rom_->size())) {
      break;
    }

    uint8_t b3 = rom_data[pos + 2];
    pos += 3;

    RoomObject obj = RoomObject::DecodeObjectFromBytes(
        b1, b2, b3, static_cast<uint8_t>(layer));

    // Pit/mask objects should be on BG2 layer to create transparency holes.
    // This allows them to show through BG1 content (walls, floor).
    if (IsPitOrMaskObject(obj.id_)) {
      obj.layer_ = RoomObject::LayerType::BG2;
      LOG_DEBUG("RoomLayout", "Pit/mask object 0x%03X assigned to BG2 layer", obj.id_);
    }

    obj.SetRom(rom_);
    obj.EnsureTilesLoaded();
    objects_.push_back(obj);

    LOG_DEBUG("RoomLayout",
              "Layout %d obj[%d]: bytes=[%02X,%02X,%02X] -> id=0x%03X x=%d y=%d size=%d tiles=%zu",
              layout_id, obj_index, b1, b2, b3,
              obj.id_, obj.x_, obj.y_, obj.size_, obj.tiles().size());
    obj_index++;
  }

  LOG_DEBUG("RoomLayout", "Layout %d loaded with %zu objects",
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
  // Pass bg1 as layout_bg1 so BG2 objects (pits/masks) will mark the
  // corresponding area in BG1 as transparent via MarkBG1Transparent().
  // This creates "holes" in BG1 that allow BG2 content to show through.
  return drawer.DrawObjectList(objects_, bg1, bg2, palette_group, state, &bg1);
}

}  // namespace yaze::zelda3
