#ifndef YAZE_APP_EDITOR_GFX_GROUP_EDITOR_H
#define YAZE_APP_EDITOR_GFX_GROUP_EDITOR_H

#include <array>

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
 *
 * Provides a UI for viewing and editing:
 * - Blocksets (8 sheets per blockset)
 * - Roomsets (4 sheets that override blockset slots 4-7)
 * - Spritesets (4 sheets for enemy/NPC graphics)
 *
 * Features palette preview controls for viewing sheets with different palettes.
 */
class GfxGroupEditor {
 public:
  absl::Status Update();

  void DrawBlocksetViewer(bool sheet_only = false);
  void DrawRoomsetViewer();
  void DrawSpritesetViewer(bool sheet_only = false);
  void DrawPaletteControls();

  void SetSelectedBlockset(uint8_t blockset) { selected_blockset_ = blockset; }
  void SetSelectedRoomset(uint8_t roomset) { selected_roomset_ = roomset; }
  void SetSelectedSpriteset(uint8_t spriteset) {
    selected_spriteset_ = spriteset;
  }
  void SetRom(Rom* rom) { rom_ = rom; }
  Rom* rom() const { return rom_; }
  void SetGameData(zelda3::GameData* data) { game_data_ = data; }
  zelda3::GameData* game_data() const { return game_data_; }

 private:
  void UpdateCurrentPalette();

  // Selection state
  uint8_t selected_blockset_ = 0;
  uint8_t selected_roomset_ = 0;
  uint8_t selected_spriteset_ = 0;

  // View controls
  float view_scale_ = 2.0f;

  // Palette controls
  gfx::PaletteCategory selected_palette_category_ =
      gfx::PaletteCategory::kDungeons;
  uint8_t selected_palette_index_ = 0;
  bool use_custom_palette_ = false;
  gfx::SnesPalette* current_palette_ = nullptr;

  // Individual canvases for each sheet slot to avoid ID conflicts
  std::array<gui::Canvas, 8> blockset_canvases_;
  std::array<gui::Canvas, 4> roomset_canvases_;
  std::array<gui::Canvas, 4> spriteset_canvases_;

  Rom* rom_ = nullptr;
  zelda3::GameData* game_data_ = nullptr;
};

}  // namespace editor
}  // namespace yaze
#endif  // YAZE_APP_EDITOR_GFX_GROUP_EDITOR_H
