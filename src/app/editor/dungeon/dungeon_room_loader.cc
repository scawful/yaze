#include "dungeon_room_loader.h"

#include <algorithm>
#include <map>
#include <future>
#include <thread>
#include <mutex>

#include "app/gfx/performance_profiler.h"
#include "app/gfx/snes_palette.h"
#include "app/zelda3/dungeon/room.h"
#include "util/log.h"

namespace yaze::editor {

absl::Status DungeonRoomLoader::LoadRoom(int room_id, zelda3::Room& room) {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  if (room_id < 0 || room_id >= 0x128) {
    return absl::InvalidArgumentError("Invalid room ID");
  }

  room = zelda3::LoadRoomFromRom(rom_, room_id);
  room.LoadObjects();

  return absl::OkStatus();
}

absl::Status DungeonRoomLoader::LoadAllRooms(std::array<zelda3::Room, 0x128>& rooms) {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  
  constexpr int kTotalRooms = 0x100 + 40; // 296 rooms
  constexpr int kMaxConcurrency = 8; // Reasonable thread limit for room loading
  
  // Determine optimal number of threads
  const int max_concurrency = std::min(kMaxConcurrency, 
                                       static_cast<int>(std::thread::hardware_concurrency()));
  const int rooms_per_thread = (kTotalRooms + max_concurrency - 1) / max_concurrency;
  
  LOG_INFO("Dungeon", "Loading %d dungeon rooms using %d threads (%d rooms per thread)", 
             kTotalRooms, max_concurrency, rooms_per_thread);
  
  // Thread-safe data structures for collecting results
  std::mutex results_mutex;
  std::vector<std::pair<int, zelda3::RoomSize>> room_size_results;
  std::vector<std::pair<int, ImVec4>> room_palette_results;
  
  // Process rooms in parallel batches
  std::vector<std::future<absl::Status>> futures;
  
  for (int thread_id = 0; thread_id < max_concurrency; ++thread_id) {
    auto task = [this, &rooms, thread_id, rooms_per_thread, &results_mutex, 
                 &room_size_results, &room_palette_results, kTotalRooms]() -> absl::Status {
      const int start_room = thread_id * rooms_per_thread;
      const int end_room = std::min(start_room + rooms_per_thread, kTotalRooms);
      
      auto dungeon_man_pal_group = rom_->palette_group().dungeon_main;
      
      for (int i = start_room; i < end_room; ++i) {
        // Load room data (this is the expensive operation)
        rooms[i] = zelda3::LoadRoomFromRom(rom_, i);
        
        // Calculate room size
        auto room_size = zelda3::CalculateRoomSize(rom_, i);
        
        // Load room objects
        rooms[i].LoadObjects();
        
        // Process palette
        auto dungeon_palette_ptr = rom_->paletteset_ids[rooms[i].palette][0];
        auto palette_id = rom_->ReadWord(0xDEC4B + dungeon_palette_ptr);
        if (palette_id.status() == absl::OkStatus()) {
          int p_id = palette_id.value() / 180;
          auto color = dungeon_man_pal_group[p_id][3];
          
          // Thread-safe collection of results
          {
            std::lock_guard<std::mutex> lock(results_mutex);
            room_size_results.emplace_back(i, room_size);
            room_palette_results.emplace_back(rooms[i].palette, color.rgb());
          }
        }
      }
      
      return absl::OkStatus();
    };
    
    futures.emplace_back(std::async(std::launch::async, task));
  }
  
  // Wait for all threads to complete
  for (auto& future : futures) {
    RETURN_IF_ERROR(future.get());
  }
  
  // Process collected results on main thread
  {
    gfx::ScopedTimer postprocess_timer("DungeonRoomLoader::PostProcessResults");
    
    // Sort results by room ID for consistent ordering
    std::sort(room_size_results.begin(), room_size_results.end(), 
              [](const auto& a, const auto& b) { return a.first < b.first; });
    std::sort(room_palette_results.begin(), room_palette_results.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    
    // Process room size results
    for (const auto& [room_id, room_size] : room_size_results) {
      room_size_pointers_.push_back(room_size.room_size_pointer);
      room_sizes_.push_back(room_size.room_size);
      if (room_size.room_size_pointer != 0x0A8000) {
        room_size_addresses_[room_id] = room_size.room_size_pointer;
      }
    }
    
    // Process palette results
    for (const auto& [palette_id, color] : room_palette_results) {
      room_palette_[palette_id] = color;
    }
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

absl::Status DungeonRoomLoader::LoadAndRenderRoomGraphics(zelda3::Room& room) {
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
  for (auto& room : rooms) {
    auto status = LoadAndRenderRoomGraphics(room);
    if (!status.ok()) {
      continue; // Log error but continue with other rooms
    }
  }
  
  return absl::OkStatus();
}

}  // namespace yaze::editor
