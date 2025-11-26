#ifndef YAZE_APP_GUI_WIDGETS_DUNGEON_OBJECT_EMULATOR_PREVIEW_H_
#define YAZE_APP_GUI_WIDGETS_DUNGEON_OBJECT_EMULATOR_PREVIEW_H_

#include "app/emu/snes.h"
#include "app/rom.h"

namespace yaze {
namespace gfx {
class IRenderer;
}  // namespace gfx
}  // namespace yaze

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
  void RenderObjectBrowser();
  void RenderStatusPanel();
  void TriggerEmulatedRender();

  // Get object name from ID
  const char* GetObjectName(int id) const;

  // Get object type (1, 2, or 3) from ID
  int GetObjectType(int id) const;

  gfx::IRenderer* renderer_ = nullptr;
  Rom* rom_ = nullptr;
  std::unique_ptr<emu::Snes> snes_instance_;
  void* object_texture_ = nullptr;

  int object_id_ = 0;
  int room_id_ = 0;
  int object_x_ = 16;
  int object_y_ = 16;
  int object_size_ = 0;  // Size parameter for rendering
  bool show_window_ = true;
  bool show_browser_ = false;  // Toggle for object browser

  // Debug info
  int last_cycle_count_ = 0;
  std::string last_error_;

  // Lazy initialization flag - defer heavy SNES init until actually needed
  bool initialized_ = false;
  void EnsureInitialized();

  // Quick select presets
  struct ObjectPreset {
    int id;
    const char* name;
  };
  static constexpr ObjectPreset kQuickPresets[] = {
    {0x00, "Ceiling"},
    {0x01, "Wall (top, north)"},
    {0x60, "Wall (top, west)"},
    {0x96, "Ceiling (large)"},
    {0xF8, "Chest"},
    {0xF0, "Door"},
    {0xEE, "Pot"},
    {0x80, "Floor 1"},
  };
};

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_WIDGETS_DUNGEON_OBJECT_EMULATOR_PREVIEW_H_
