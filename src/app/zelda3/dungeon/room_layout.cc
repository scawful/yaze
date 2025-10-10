#include "room_layout.h"

#include "absl/strings/str_format.h"
#include "app/snes.h"

namespace yaze::zelda3 {

namespace {
constexpr uint16_t kLayerTerminator = 0xFFFF;

uint16_t ReadWord(const Rom* rom, int pc_addr) {
  const auto& data = rom->data();
  return static_cast<uint16_t>(data[pc_addr] | (data[pc_addr + 1] << 8));
}
}

absl::StatusOr<int> RoomLayout::GetLayoutAddress(int layout_id) const {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  if (layout_id < 0 || layout_id >= static_cast<int>(kRoomLayoutPointers.size())) {
    return absl::InvalidArgumentError(absl::StrFormat("Invalid layout id %d", layout_id));
  }

  uint32_t snes_addr = kRoomLayoutPointers[layout_id];
  int pc_addr = SnesToPc(static_cast<int>(snes_addr));
  if (pc_addr < 0 || pc_addr >= static_cast<int>(rom_->size())) {
    return absl::OutOfRangeError(absl::StrFormat("Layout pointer %d out of range", layout_id));
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

  while (pos + 2 < static_cast<int>(rom_->size())) {
    uint8_t b1 = rom_data[pos];
    uint8_t b2 = rom_data[pos + 1];

    if (b1 == 0xFF && b2 == 0xFF) {
      layer++;
      pos += 2;
      if (layer >= 3) {
        break;
      }
      continue;
    }

    if (pos + 2 >= static_cast<int>(rom_->size())) {
      break;
    }

    uint8_t b3 = rom_data[pos + 2];
    pos += 3;

    RoomObject obj = RoomObject::DecodeObjectFromBytes(b1, b2, b3, static_cast<uint8_t>(layer));
    obj.set_rom(rom_);
    obj.EnsureTilesLoaded();
    objects_.push_back(obj);
  }

  return absl::OkStatus();
}

}  // namespace yaze::zelda3
