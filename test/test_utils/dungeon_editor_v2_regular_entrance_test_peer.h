#ifndef YAZE_TEST_TEST_UTILS_DUNGEON_EDITOR_V2_REGULAR_ENTRANCE_TEST_PEER_H_
#define YAZE_TEST_TEST_UTILS_DUNGEON_EDITOR_V2_REGULAR_ENTRANCE_TEST_PEER_H_

#include <cstddef>

#include "app/editor/dungeon/dungeon_editor_v2.h"

namespace yaze::editor {

// Narrow test-only access to the regular-entrance save path. Spawn records are
// deliberately excluded until their distinct ROM schema is modeled correctly.
class DungeonEditorV2RegularEntranceTestPeer {
 public:
  static zelda3::RoomEntrance& RegularEntrance(DungeonEditorV2& editor,
                                               int entrance_id) {
    return editor.entrances_.at(
        static_cast<size_t>(zelda3::kNumDungeonSpawnPoints + entrance_id));
  }

  static void LoadRegularEntranceFromRom(DungeonEditorV2& editor,
                                         int entrance_id) {
    RegularEntrance(editor, entrance_id) =
        zelda3::RoomEntrance(editor.rom_, entrance_id, false);
  }
};

// Test-only access to prove the editor rejects dirty spawn records before any
// save domain mutates the ROM. This does not expose or exercise spawn writing.
class DungeonEditorV2SpawnRejectionTestPeer {
 public:
  static zelda3::RoomEntrance& LoadSpawnFromRom(DungeonEditorV2& editor,
                                                int spawn_id) {
    auto& spawn = editor.entrances_.at(static_cast<size_t>(spawn_id));
    spawn = zelda3::RoomEntrance(editor.rom_, spawn_id, true);
    return spawn;
  }
};

}  // namespace yaze::editor

#endif  // YAZE_TEST_TEST_UTILS_DUNGEON_EDITOR_V2_REGULAR_ENTRANCE_TEST_PEER_H_
