#ifndef YAZE_APP_GUI_WIDGETS_DUNGEON_OBJECT_EMULATOR_PREVIEW_H_
#define YAZE_APP_GUI_WIDGETS_DUNGEON_OBJECT_EMULATOR_PREVIEW_H_

#include "app/emu/snes.h"
#include "app/rom.h"

namespace yaze {
namespace gfx {
class IRenderer;
} // namespace gfx
}

namespace yaze {
namespace gui {

class DungeonObjectEmulatorPreview {
 public:
  DungeonObjectEmulatorPreview();
  ~DungeonObjectEmulatorPreview();

  void Initialize(gfx::IRenderer* renderer, Rom* rom);
  void Render();

 private:
  void RenderControls();
  void TriggerEmulatedRender();

  gfx::IRenderer* renderer_ = nullptr;
  Rom* rom_ = nullptr;
  std::unique_ptr<emu::Snes> snes_instance_;
  void* object_texture_ = nullptr;

  int object_id_ = 0;
  int room_id_ = 0;
  int object_x_ = 16;
  int object_y_ = 16;
  bool show_window_ = true;
  
  // Debug info
  int last_cycle_count_ = 0;
  std::string last_error_;
};

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_WIDGETS_DUNGEON_OBJECT_EMULATOR_PREVIEW_H_
