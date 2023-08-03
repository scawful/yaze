#ifndef YAZE_APP_EDITOR_SCREEN_EDITOR_H
#define YAZE_APP_EDITOR_SCREEN_EDITOR_H

#include <imgui/imgui.h>

#include <array>

#include "app/core/constants.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/color.h"
#include "app/gui/icons.h"
#include "app/rom.h"
#include "app/zelda3/screen/inventory.h"

namespace yaze {
namespace app {
namespace editor {

using MosaicArray = std::array<int, core::kNumOverworldMaps>;

class ScreenEditor {
 public:
  ScreenEditor();
  void SetupROM(ROM &rom) {
    rom_ = rom;
    inventory_.SetupROM(rom_);
  }
  void Update();

 private:
  void DrawMosaicEditor();
  void DrawTitleScreenEditor();
  void DrawNamingScreenEditor();
  void DrawOverworldMapEditor();
  void DrawDungeonMapsEditor();
  void DrawInventoryMenuEditor();

  void DrawToolset();
  void DrawInventoryToolset();
  void DrawWorldGrid(int world, int h = 8, int w = 8);

  char mosaic_tiles_[core::kNumOverworldMaps];

  ROM rom_;
  Bytes all_gfx_;
  zelda3::Inventory inventory_;
  gfx::SNESPalette palette_;
  gui::Canvas screen_canvas_;
  gui::Canvas tilesheet_canvas_;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif