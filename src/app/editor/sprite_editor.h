#ifndef YAZE_APP_EDITOR_SPRITE_EDITOR_H
#define YAZE_APP_EDITOR_SPRITE_EDITOR_H

#include "absl/status/status.h"
#include "app/gui/canvas.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace editor {

class SpriteEditor : public SharedROM {
 public:
  absl::Status Update();

 private:
  void DrawEditorTable();
  void DrawSpriteCanvas();
  void DrawCurrentSheets();

  uint8_t current_sheets_[8];
  bool sheets_loaded_ = false;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SPRITE_EDITOR_H