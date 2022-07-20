#ifndef YAZE_APP_EDITOR_SCREEN_EDITOR_H
#define YAZE_APP_EDITOR_SCREEN_EDITOR_H

#include <imgui/imgui.h>

#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"
#include "app/zelda3/screen.h"
#include "gui/canvas.h"

namespace yaze {
namespace app {
namespace editor {

class ScreenEditor {
 public:
  ScreenEditor();
  void Update();

 private:
  void DrawTitleScreenEditor();
  void DrawNamingScreenEditor();
  void DrawOverworldMapEditor();
  void DrawDungeonMapsEditor();
  void DrawGameMenuEditor();
  void DrawHUDEditor();

  void DrawCanvas();
  void DrawToolset();

  zelda3::Screen current_screen_;
  gui::Canvas screen_canvas_;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif