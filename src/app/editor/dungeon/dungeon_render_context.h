#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_RENDER_CONTEXT_H_
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_RENDER_CONTEXT_H_

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "zelda3/dungeon/room_census.h"

namespace yaze::editor {

enum class DungeonRenderContextSource {
  kSelectedEntrance,
  kInferredOwnerEntrance,
  kRoomHeaderFallback,
  kAmbiguous,
};

struct DungeonRenderEntranceCandidate {
  int slot = -1;
  int room_id = -1;
  uint8_t main_gfx = 0xFF;
};

struct DungeonRenderContext {
  DungeonRenderContextSource source =
      DungeonRenderContextSource::kRoomHeaderFallback;
  int room_id = -1;
  int owner_index = -1;
  std::string owner_id;
  std::string owner_name;
  int entrance_slot = -1;
  uint8_t entrance_blockset = 0xFF;
  uint8_t room_header_blockset = 0xFF;
  std::vector<int> candidate_slots;

  bool uses_entrance() const { return entrance_slot >= 0; }
  uint8_t effective_blockset() const {
    return uses_entrance() ? entrance_blockset : room_header_blockset;
  }
};

const char* DungeonRenderContextSourceName(DungeonRenderContextSource source);

// Resolves the graphics owner for a room without mutating either the census or
// entrance records. A selected entrance wins for its direct room and all rooms
// with the same census owner. Otherwise an owner entrance is inferred only
// when every candidate agrees on main GFX.
DungeonRenderContext ResolveDungeonRenderContext(
    int room_id, int selected_entrance_slot, uint8_t room_header_blockset,
    const zelda3::RoomCensus* census,
    std::span<const DungeonRenderEntranceCandidate> entrances);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_RENDER_CONTEXT_H_
