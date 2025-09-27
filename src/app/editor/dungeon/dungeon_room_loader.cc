#include "dungeon_room_loader.h"

#include <algorithm>
#include <map>

#include "app/gfx/snes_palette.h"
#include "app/zelda3/dungeon/room.h"

namespace yaze::editor {

absl::Status DungeonRoomLoader::LoadAllRooms(std::array<zelda3::Room, 0x128>& rooms) {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  
  auto dungeon_man_pal_group = rom_->palette_group().dungeon_main;

  for (int i = 0; i < 0x100 + 40; i++) {
    rooms[i] = zelda3::LoadRoomFromRom(rom_, i);

    auto room_size = zelda3::CalculateRoomSize(rom_, i);
    room_size_pointers_.push_back(room_size.room_size_pointer);
    room_sizes_.push_back(room_size.room_size);
    if (room_size.room_size_pointer != 0x0A8000) {
      room_size_addresses_[i] = room_size.room_size_pointer;
    }

    rooms[i].LoadObjects();

    auto dungeon_palette_ptr = rom_->paletteset_ids[rooms[i].palette][0];
    auto palette_id = rom_->ReadWord(0xDEC4B + dungeon_palette_ptr);
    if (palette_id.status() != absl::OkStatus()) {
      continue;
    }
    int p_id = palette_id.value() / 180;
    auto color = dungeon_man_pal_group[p_id][3];
    room_palette_[rooms[i].palette] = color.rgb();
  }

  LoadDungeonRoomSize();
  return absl::OkStatus();
}

absl::Status DungeonRoomLoader::LoadRoomEntrances(std::array<zelda3::RoomEntrance, 0x8C>& entrances) {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  
  // Load entrances
  for (int i = 0; i < 0x07; ++i) {
    entrances[i] = zelda3::RoomEntrance(rom_, i, true);
  }

  for (int i = 0; i < 0x85; ++i) {
    entrances[i + 0x07] = zelda3::RoomEntrance(rom_, i, false);
  }
  
  return absl::OkStatus();
}

void DungeonRoomLoader::LoadDungeonRoomSize() {
  std::map<int, std::vector<int>> rooms_by_bank;
  for (const auto& room : room_size_addresses_) {
    int bank = room.second >> 16;
    rooms_by_bank[bank].push_back(room.second);
  }

  // Process and calculate room sizes within each bank
  for (auto& bank_rooms : rooms_by_bank) {
    std::ranges::sort(bank_rooms.second);

    for (size_t i = 0; i < bank_rooms.second.size(); ++i) {
      int room_ptr = bank_rooms.second[i];

      // Identify the room ID for the current room pointer
      int room_id =
          std::ranges::find_if(room_size_addresses_, [room_ptr](
                                                         const auto& entry) {
            return entry.second == room_ptr;
          })->first;

      if (room_ptr != 0x0A8000) {
        if (i < bank_rooms.second.size() - 1) {
          room_sizes_[room_id] = bank_rooms.second[i + 1] - room_ptr;
        } else {
          int bank_end_address = (bank_rooms.first << 16) | 0xFFFF;
          room_sizes_[room_id] = bank_end_address - room_ptr + 1;
        }
        total_room_size_ += room_sizes_[room_id];
      } else {
        room_sizes_[room_id] = 0x00;
      }
    }
  }
}

absl::Status DungeonRoomLoader::LoadAndRenderRoomGraphics(int room_id, zelda3::Room& room) {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  
  // Load room graphics with proper blockset
  room.LoadRoomGraphics(room.blockset);
  
  // Render the room graphics to the graphics arena
  room.RenderRoomGraphics();
  
  return absl::OkStatus();
}

absl::Status DungeonRoomLoader::ReloadAllRoomGraphics(std::array<zelda3::Room, 0x128>& rooms) {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  
  // Reload graphics for all rooms
  for (size_t i = 0; i < rooms.size(); ++i) {
    auto status = LoadAndRenderRoomGraphics(static_cast<int>(i), rooms[i]);
    if (!status.ok()) {
      continue; // Log error but continue with other rooms
    }
  }
  
  return absl::OkStatus();
}

}  // namespace yaze::editor
