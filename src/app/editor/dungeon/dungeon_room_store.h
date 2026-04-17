#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_ROOM_STORE_H
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_ROOM_STORE_H

#include <array>
#include <cstddef>
#include <memory>

#include "rom/rom.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/game_data.h"

namespace yaze::editor {

class DungeonRoomStore {
 public:
  static constexpr size_t kRoomCount = zelda3::kNumberOfRooms;

  DungeonRoomStore() = default;
  explicit DungeonRoomStore(Rom* rom, zelda3::GameData* game_data = nullptr)
      : rom_(rom), game_data_(game_data) {}

  size_t size() const { return kRoomCount; }

  zelda3::Room& operator[](size_t index) { return EnsureRoom(index); }
  const zelda3::Room& operator[](size_t index) const {
    return EnsureRoom(index);
  }

  zelda3::Room* GetIfMaterialized(int room_id) {
    return const_cast<zelda3::Room*>(
        const_cast<const DungeonRoomStore*>(this)->GetIfMaterialized(room_id));
  }

  const zelda3::Room* GetIfMaterialized(int room_id) const {
    if (room_id < 0 || room_id >= static_cast<int>(kRoomCount)) {
      return nullptr;
    }
    return rooms_[room_id].get();
  }

  zelda3::Room* GetIfLoaded(int room_id) {
    auto* room = GetIfMaterialized(room_id);
    return room != nullptr && room->IsLoaded() ? room : nullptr;
  }

  const zelda3::Room* GetIfLoaded(int room_id) const {
    auto* room = GetIfMaterialized(room_id);
    return room != nullptr && room->IsLoaded() ? room : nullptr;
  }

  void SetRom(Rom* rom) {
    rom_ = rom;
    ForEachMaterialized([rom](int, zelda3::Room& room) { room.SetRom(rom); });
  }

  void SetGameData(zelda3::GameData* game_data) {
    game_data_ = game_data;
    ForEachMaterialized(
        [game_data](int, zelda3::Room& room) { room.SetGameData(game_data); });
  }

  Rom* rom() const { return rom_; }
  zelda3::GameData* game_data() const { return game_data_; }

  void Clear() {
    for (auto& room : rooms_) {
      room.reset();
    }
  }

  template <typename Fn>
  void ForEachMaterialized(Fn&& fn) {
    for (size_t i = 0; i < rooms_.size(); ++i) {
      if (rooms_[i]) {
        fn(static_cast<int>(i), *rooms_[i]);
      }
    }
  }

  template <typename Fn>
  void ForEachMaterialized(Fn&& fn) const {
    for (size_t i = 0; i < rooms_.size(); ++i) {
      if (rooms_[i]) {
        fn(static_cast<int>(i), *rooms_[i]);
      }
    }
  }

  template <typename Fn>
  void ForEachLoaded(Fn&& fn) {
    ForEachMaterialized([&fn](int room_id, zelda3::Room& room) {
      if (room.IsLoaded()) {
        fn(room_id, room);
      }
    });
  }

  template <typename Fn>
  void ForEachLoaded(Fn&& fn) const {
    ForEachMaterialized([&fn](int room_id, const zelda3::Room& room) {
      if (room.IsLoaded()) {
        fn(room_id, room);
      }
    });
  }

  int LoadedCount() const {
    int count = 0;
    ForEachLoaded([&count](int, const zelda3::Room&) { ++count; });
    return count;
  }

 private:
  zelda3::Room& EnsureRoom(size_t index) const {
    if (!rooms_[index]) {
      auto room = std::make_unique<zelda3::Room>();
      room->SetRom(rom_);
      room->SetGameData(game_data_);
      rooms_[index] = std::move(room);
    }
    return *rooms_[index];
  }

  Rom* rom_ = nullptr;
  zelda3::GameData* game_data_ = nullptr;
  mutable std::array<std::unique_ptr<zelda3::Room>, kRoomCount> rooms_;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_ROOM_STORE_H
