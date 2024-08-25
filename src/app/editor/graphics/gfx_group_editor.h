#ifndef YAZE_APP_EDITOR_GFX_GROUP_EDITOR_H
#define YAZE_APP_EDITOR_GFX_GROUP_EDITOR_H

#include <cmath>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/utils/editor.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/gui/style.h"
#include "app/rom.h"
#include "app/zelda3/overworld/overworld.h"
#include "imgui/imgui.h"

namespace yaze {
namespace app {
namespace editor {

/**
 * @class GfxGroupEditor
 * @brief Manage graphics group configurations in a Rom.
 */
class GfxGroupEditor : public SharedRom {
 public:
  absl::Status Update();

  void DrawBlocksetViewer(bool sheet_only = false);
  void DrawRoomsetViewer();
  void DrawSpritesetViewer(bool sheet_only = false);
  void DrawPaletteViewer();

  void SetSelectedBlockset(uint8_t blockset) { selected_blockset_ = blockset; }
  void SetSelectedRoomset(uint8_t roomset) { selected_roomset_ = roomset; }
  void SetSelectedSpriteset(uint8_t spriteset) {
    selected_spriteset_ = spriteset;
  }

 private:
  uint8_t selected_blockset_ = 0;
  uint8_t selected_roomset_ = 0;
  uint8_t selected_spriteset_ = 0;
  uint8_t selected_paletteset_ = 0;

  gui::Canvas blockset_canvas_;
  gui::Canvas roomset_canvas_;
  gui::Canvas spriteset_canvas_;

  gfx::SnesPalette palette_;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze
#endif  // YAZE_APP_EDITOR_GFX_GROUP_EDITOR_H