#ifndef YAZE_APP_EDITOR_GFX_GROUP_EDITOR_H
#define YAZE_APP_EDITOR_GFX_GROUP_EDITOR_H

#include "absl/status/status.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/canvas/canvas.h"
#include "rom/rom.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace editor {

/**
 * @class GfxGroupEditor
 * @brief Manage graphics group configurations in a Rom.
 */
class GfxGroupEditor {
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
  void set_rom(Rom* rom) { rom_ = rom; }
  Rom* rom() const { return rom_; }
  void set_game_data(zelda3::GameData* data) { game_data_ = data; }
  zelda3::GameData* game_data() const { return game_data_; }

 private:
  uint8_t selected_blockset_ = 0;
  uint8_t selected_roomset_ = 0;
  uint8_t selected_spriteset_ = 0;
  uint8_t selected_paletteset_ = 0;

  gui::Canvas blockset_canvas_;
  gui::Canvas roomset_canvas_;
  gui::Canvas spriteset_canvas_;

  gfx::SnesPalette palette_;
  Rom* rom_ = nullptr;
  zelda3::GameData* game_data_ = nullptr;
};

}  // namespace editor
}  // namespace yaze
#endif  // YAZE_APP_EDITOR_GFX_GROUP_EDITOR_H
