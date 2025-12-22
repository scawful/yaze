#ifndef YAZE_APP_GUI_WIDGETS_DUNGEON_OBJECT_EMULATOR_PREVIEW_H_
#define YAZE_APP_GUI_WIDGETS_DUNGEON_OBJECT_EMULATOR_PREVIEW_H_

#include "app/emu/snes.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/render/background_buffer.h"
#include "rom/rom.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace gfx {
class IRenderer;
}  // namespace gfx
namespace zelda3 {
class ObjectDrawer;
}  // namespace zelda3
namespace emu {
namespace render {
class EmulatorRenderService;
}  // namespace render
}  // namespace emu
}  // namespace yaze

namespace yaze {
namespace gui {

class DungeonObjectEmulatorPreview {
 public:
  DungeonObjectEmulatorPreview();
  ~DungeonObjectEmulatorPreview();

  // Initialize with optional shared render service
  // If render_service is nullptr, uses local SNES instance (legacy mode)
  void Initialize(gfx::IRenderer* renderer, Rom* rom,
                  zelda3::GameData* game_data = nullptr,
                  emu::render::EmulatorRenderService* render_service = nullptr);
  void Render();

  // Visibility control for external toggling
  void set_visible(bool visible) { show_window_ = visible; }
  bool is_visible() const { return show_window_; }

  // GameData accessor for post-initialization setting
  void SetGameData(zelda3::GameData* game_data) { game_data_ = game_data; }

 private:
  void RenderControls();
  void RenderObjectBrowser();
  void RenderStatusPanel();
  void TriggerEmulatedRender();

  // ObjectDrawer-based rendering (static, works reliably)
  void TriggerStaticRender();

  // Get object name from ID
  const char* GetObjectName(int id) const;

  // Get object type (1, 2, or 3) from ID
  int GetObjectType(int id) const;

  gfx::IRenderer* renderer_ = nullptr;
  Rom* rom_ = nullptr;
  zelda3::GameData* game_data_ = nullptr;
  emu::render::EmulatorRenderService* render_service_ = nullptr;  // Shared service (optional)
  std::unique_ptr<emu::Snes> snes_instance_;  // Legacy local instance
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

  // Rendering mode selection
  enum class RenderMode { kStatic, kEmulator };
  RenderMode render_mode_ = RenderMode::kStatic;  // Default to working mode

  // Static rendering components (ObjectDrawer-based)
  std::unique_ptr<zelda3::ObjectDrawer> object_drawer_;
  gfx::BackgroundBuffer preview_bg1_;
  gfx::BackgroundBuffer preview_bg2_;
  gfx::Bitmap preview_bitmap_;
  bool static_render_dirty_ = true;  // Need to re-render

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
