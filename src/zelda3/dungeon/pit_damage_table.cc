#include "zelda3/dungeon/pit_damage_table.h"

#include <algorithm>
#include <vector>

#include "absl/strings/str_format.h"
#include "rom/snes.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"

namespace yaze {
namespace zelda3 {
namespace {

int PitTableEntryCount(uint8_t max_offset_byte) {
  return static_cast<int>(max_offset_byte) / 2 + 1;
}

int PitTableByteLength(uint8_t max_offset_byte) {
  return max_offset_byte + 2;
}

}  // namespace

absl::Status PitDamageTable::LoadFromRom(Rom* rom, PitDamageTable* out) {
  if (!rom || !rom->is_loaded() || out == nullptr) {
    return absl::InvalidArgumentError("ROM not loaded");
  }
  const auto& rom_data = rom->vector();
  if (kPitCount < 0 || kPitCount >= static_cast<int>(rom_data.size()) ||
      kPitPointer + 2 >= static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Pit count/pointer out of range");
  }

  const uint8_t max_offset = rom_data[kPitCount];
  const int entry_count = PitTableEntryCount(max_offset);
  const int data_len = PitTableByteLength(max_offset);

  const int pit_ptr_snes = (rom_data[kPitPointer + 2] << 16) |
                           (rom_data[kPitPointer + 1] << 8) |
                           rom_data[kPitPointer];
  const int pit_data_pc = SnesToPc(pit_ptr_snes);
  if (pit_data_pc < 0 ||
      pit_data_pc + data_len > static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Pit data region out of range");
  }

  std::vector<uint16_t> room_ids;
  room_ids.reserve(entry_count);
  for (int offset = 0; offset < data_len; offset += 2) {
    const int addr = pit_data_pc + offset;
    const uint16_t room_id = static_cast<uint16_t>(rom_data[addr]) |
                             (static_cast<uint16_t>(rom_data[addr + 1]) << 8);
    room_ids.push_back(room_id);
  }

  out->room_ids_ = std::move(room_ids);
  out->dirty_ = false;
  return absl::OkStatus();
}

absl::Status PitDamageTable::SaveToRom(Rom* rom) const {
  if (!rom || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }
  const auto& rom_data = rom->vector();
  if (kPitCount < 0 || kPitCount >= static_cast<int>(rom_data.size()) ||
      kPitPointer + 2 >= static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Pit count/pointer out of range");
  }

  const uint8_t max_offset = rom_data[kPitCount];
  const int expected_entries = PitTableEntryCount(max_offset);
  const int data_len = PitTableByteLength(max_offset);
  if (static_cast<int>(room_ids_.size()) != expected_entries) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "PitDamageTable entry count %zu does not match ROM table capacity %d",
        room_ids_.size(), expected_entries));
  }
  std::vector<bool> seen(kNumberOfRooms, false);
  for (size_t index = 0; index < room_ids_.size(); ++index) {
    const uint16_t room_id = room_ids_[index];
    if (room_id >= kNumberOfRooms) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "PitDamageTable room 0x%03X at index %zu is outside the dungeon "
          "room range",
          room_id, index));
    }
    if (seen[room_id]) {
      return absl::AlreadyExistsError(absl::StrFormat(
          "PitDamageTable room 0x%03X appears more than once", room_id));
    }
    seen[room_id] = true;
  }

  const int pit_ptr_snes = (rom_data[kPitPointer + 2] << 16) |
                           (rom_data[kPitPointer + 1] << 8) |
                           rom_data[kPitPointer];
  const int pit_data_pc = SnesToPc(pit_ptr_snes);
  if (pit_data_pc < 0 ||
      pit_data_pc + data_len > static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Pit data region out of range");
  }

  std::vector<uint8_t> encoded;
  encoded.reserve(data_len);
  for (uint16_t room_id : room_ids_) {
    encoded.push_back(static_cast<uint8_t>(room_id & 0xFF));
    encoded.push_back(static_cast<uint8_t>((room_id >> 8) & 0xFF));
  }
  return rom->WriteVector(pit_data_pc, encoded);
}

bool PitDamageTable::Contains(uint16_t room_id) const {
  return std::find(room_ids_.begin(), room_ids_.end(), room_id) !=
         room_ids_.end();
}

void PitDamageTable::SetRoomIds(std::vector<uint16_t> room_ids) {
  room_ids_ = std::move(room_ids);
  dirty_ = true;
}

absl::Status PitDamageTable::ReplaceRoomId(uint16_t old_room_id,
                                           uint16_t new_room_id) {
  if (new_room_id >= kNumberOfRooms) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Replacement room 0x%03X is outside the dungeon room range",
        new_room_id));
  }

  auto old_it = std::find(room_ids_.begin(), room_ids_.end(), old_room_id);
  if (old_it == room_ids_.end()) {
    return absl::NotFoundError(absl::StrFormat(
        "Room 0x%03X is not in RoomsWithPitDamage", old_room_id));
  }

  if (old_room_id == new_room_id) {
    return absl::OkStatus();
  }

  if (Contains(new_room_id)) {
    return absl::AlreadyExistsError(absl::StrFormat(
        "Room 0x%03X is already in RoomsWithPitDamage", new_room_id));
  }

  *old_it = new_room_id;
  dirty_ = true;
  return absl::OkStatus();
}

}  // namespace zelda3
}  // namespace yaze
