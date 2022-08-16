#ifndef YAZE_APP_EDITOR_SCREEN_EDITOR_H
#define YAZE_APP_EDITOR_SCREEN_EDITOR_H

#include <imgui/imgui.h>

#include <array>

#include "app/asm/script.h"
#include "app/core/constants.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"
#include "app/zelda3/screen.h"
#include "gui/canvas.h"

namespace yaze {
namespace app {
namespace editor {

using MosaicArray = std::array<int, core::kNumOverworldMaps>;
static int overworldCustomMosaicASM = 0x1301D0;
static int overworldCustomMosaicArray = 0x1301F0;

class ScreenEditor {
 public:
  ScreenEditor();
  void SetupROM(ROM &rom) { rom_ = rom; }
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
  void DrawWorldGrid(int world, int h = 8, int w = 8);

  char mosaic_tiles_[core::kNumOverworldMaps];

  ROM rom_;
  snes_asm::Script mosaic_script_;
  zelda3::Screen current_screen_;
  gui::Canvas screen_canvas_;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif