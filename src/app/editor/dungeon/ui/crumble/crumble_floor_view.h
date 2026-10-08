#ifndef YAZE_APP_EDITOR_DUNGEON_UI_CRUMBLE_CRUMBLE_FLOOR_VIEW_H_
#define YAZE_APP_EDITOR_DUNGEON_UI_CRUMBLE_CRUMBLE_FLOOR_VIEW_H_

#include <string>

#include "app/editor/dungeon/ui/crumble/crumble_floor_model.h"

namespace yaze {
class Rom;
namespace project {
struct YazeProject;
}
namespace zelda3 {
class Room;
}
namespace gui {
struct CanvasRuntime;
}
namespace editor {

// Source-backed recognition is separate from vanilla Holes 3. A base editing
// ROM need not contain the assembled hook yet, but its dispatch vector must
// resolve to the entry documented by the Oracle source manifest.
bool HasOracleCrumbleTag(const Rom& rom, const project::YazeProject* project,
                         const zelda3::Room& room);
CrumbleFloorModel::Words ReadCrumbleFloorWords(const zelda3::Room& room);

class CrumbleFloorView {
 public:
  void Sync(const Rom* rom, const project::YazeProject* project,
            const zelda3::Room& room);
  void DrawControls(bool interactive);
  // Returns true while rehearsal owns canvas input, including its exit click.
  bool DrawCanvas(const gui::CanvasRuntime& rt, bool interactive,
                  bool shortcut_focus);
  void Stop() { rehearsing_ = false; }
  void Reset() { *this = CrumbleFloorView{}; }
  bool rehearsing() const { return rehearsing_; }
  const CrumbleFloorModel& model() const { return model_; }

 private:
  friend class CrumbleFloorViewTestPeer;
  CrumbleFloorModel model_;
  CrumbleFloorModel::Words words_{};
  const Rom* rom_ = nullptr;
  const project::YazeProject* project_ = nullptr;
  int room_id_ = -1;
  bool supported_ = false;
  bool rehearsing_ = false;
  std::string message_;
};

}  // namespace editor
}  // namespace yaze
#endif
