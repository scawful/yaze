#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_ROOM_SELECTOR_H
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_ROOM_SELECTOR_H

#include <functional>
#include <string>
#include <vector>

#include "app/editor/editor.h"
#include "rom/rom.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_entrance.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace editor {

/**
 * @brief Intent for room selection in the dungeon editor.
 */
enum class RoomSelectionIntent {
  kFocusInWorkbench,  // Default: navigate workbench to this room
  kOpenStandalone,    // Open as separate panel (even in workbench mode)
  kPreview,           // Update state only, don't show/focus panels
};

/**
 * @brief Handles room and entrance selection UI
 */
class DungeonRoomSelector {
 public:
  explicit DungeonRoomSelector(Rom* rom = nullptr) : rom_(rom) {}

  void Draw();
  void DrawRoomSelector(
      RoomSelectionIntent single_click_intent =
          RoomSelectionIntent::kFocusInWorkbench);
  void DrawEntranceSelector();

  // Unified context setter (preferred)
  void SetContext(EditorContext ctx) {
    rom_ = ctx.rom;
    game_data_ = ctx.game_data;
  }
  EditorContext context() const { return {rom_, game_data_}; }

  // Individual setters for compatibility
  void SetRom(Rom* rom) { rom_ = rom; }
  Rom* rom() const { return rom_; }
  void SetGameData(zelda3::GameData* game_data) { game_data_ = game_data; }
  zelda3::GameData* game_data() const { return game_data_; }

  // Room selection
  void set_current_room_id(uint16_t room_id) { current_room_id_ = room_id; }
  int current_room_id() const { return current_room_id_; }

  void set_active_rooms(const ImVector<int>& rooms) { active_rooms_ = rooms; }
  const ImVector<int>& active_rooms() const { return active_rooms_; }
  ImVector<int>& mutable_active_rooms() { return active_rooms_; }

  // Entrance selection
  void set_current_entrance_id(int entrance_id) {
    current_entrance_id_ = entrance_id;
  }
  int current_entrance_id() const { return current_entrance_id_; }

  // Room data access
  void set_rooms(std::array<zelda3::Room, 0x128>* rooms) { rooms_ = rooms; }
  void set_entrances(std::array<zelda3::RoomEntrance, 0x8C>* entrances) {
    entrances_ = entrances;
  }

  // Callback for room selection events (single-click / default)
  void SetRoomSelectedCallback(std::function<void(int)> callback) {
    room_selected_callback_ = std::move(callback);
  }
  [[deprecated("Use SetRoomSelectedCallback() instead")]]
  void set_room_selected_callback(std::function<void(int)> callback) {
    SetRoomSelectedCallback(std::move(callback));
  }

  // Intent-aware room selection callback (double-click, context menu)
  void SetRoomSelectedWithIntentCallback(
      std::function<void(int, RoomSelectionIntent)> callback) {
    room_intent_callback_ = std::move(callback);
  }

  // Callback for entrance selection events (triggers room opening)
  void SetEntranceSelectedCallback(std::function<void(int)> callback) {
    entrance_selected_callback_ = std::move(callback);
  }
  [[deprecated("Use SetEntranceSelectedCallback() instead")]]
  void set_entrance_selected_callback(std::function<void(int)> callback) {
    SetEntranceSelectedCallback(std::move(callback));
  }

 private:
  Rom* rom_ = nullptr;
  zelda3::GameData* game_data_ = nullptr;
  uint16_t current_room_id_ = 0;
  int current_entrance_id_ = 0;
  ImVector<int> active_rooms_;

  std::array<zelda3::Room, 0x128>* rooms_ = nullptr;
  std::array<zelda3::RoomEntrance, 0x8C>* entrances_ = nullptr;

  // Callback for room selection events (single-click / default)
  std::function<void(int)> room_selected_callback_;

  // Intent-aware room selection callback (double-click, context menu)
  std::function<void(int, RoomSelectionIntent)> room_intent_callback_;

  // Callback for entrance selection events
  std::function<void(int)> entrance_selected_callback_;

  ImGuiTextFilter room_filter_;
  ImGuiTextFilter entrance_filter_;

  // Cached filtered indices for virtualization
  std::vector<int> filtered_room_indices_;
  std::vector<int> filtered_entrance_indices_;
  std::string last_room_filter_;
  std::string last_entrance_filter_;

  // Rebuild filtered indices when filter changes
  void RebuildRoomFilterCache();
  void RebuildEntranceFilterCache();
};

}  // namespace editor
}  // namespace yaze

#endif
