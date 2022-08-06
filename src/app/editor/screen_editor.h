#ifndef YAZE_APP_EDITOR_SCREEN_EDITOR_H
#define YAZE_APP_EDITOR_SCREEN_EDITOR_H

#include <imgui/imgui.h>

#include <array>

#include "app/asm/script.h"
#include "app/core/constants.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"
#include "app/zelda3/screen.h"
#include "gui/canvas.h"

namespace yaze {
namespace app {
namespace editor {

using MosaicArray = std::array<int, core::kNumOverworldMaps>;

class ScreenEditor {
 public:
  ScreenEditor();
  void Update();

 private:
  void DrawMosaicEditor();
  void DrawTitleScreenEditor();
  void DrawNamingScreenEditor();
  void DrawOverworldMapEditor();
  void DrawDungeonMapsEditor();
  void DrawGameMenuEditor();
  void DrawHUDEditor();

  void DrawCanvas();
  void DrawToolset();

  std::array<int, core::kNumOverworldMaps> mosaic_tiles_;
  snes_asm::Script mosaic_script_;
  zelda3::Screen current_screen_;
  gui::Canvas screen_canvas_;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif