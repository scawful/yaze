#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_JUMP_TARGETS_H_
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_JUMP_TARGETS_H_

#include <string>
#include <vector>

namespace yaze {
class Rom;
namespace project {
struct YazeProject;
}  // namespace project
}  // namespace yaze

namespace yaze::editor {

struct DungeonJumpTarget {
  std::string name;
  int room_id = -1;
};

// Named dungeons with the room to open for each. A project registry is
// authoritative when present (its first "entrance" room, else its first room).
// Otherwise regular ROM entrances are grouped by their dungeon byte; the
// lowest-numbered entrance supplies the room. Caves (dungeon byte 0xFF) are
// omitted.
std::vector<DungeonJumpTarget> BuildDungeonJumpTargets(
    Rom* rom, const project::YazeProject* project);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_JUMP_TARGETS_H_
