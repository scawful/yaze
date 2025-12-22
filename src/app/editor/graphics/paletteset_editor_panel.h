#ifndef YAZE_APP_EDITOR_GRAPHICS_PALETTESET_EDITOR_PANEL_H_
#define YAZE_APP_EDITOR_GRAPHICS_PALETTESET_EDITOR_PANEL_H_

#include <cstdint>

#include "absl/status/status.h"
#include "app/gfx/types/snes_palette.h"
#include "rom/rom.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace editor {

/**
 * @class PalettesetEditorPanel
 * @brief Dedicated panel for editing dungeon palette sets.
 *
 * A paletteset defines which palettes are used together in a dungeon room:
 * - Dungeon Main: The primary background/tileset palette
 * - Sprite Aux 1-3: Three auxiliary sprite palettes for enemies/NPCs
 *
 * This panel allows viewing and editing these associations, providing
 * a better UX than the combined GfxGroupEditor tab.
 */
class PalettesetEditorPanel {
 public:
  absl::Status Update();

  void SetRom(Rom* rom) { rom_ = rom; }
  Rom* rom() const { return rom_; }
  void SetGameData(zelda3::GameData* data) { game_data_ = data; }
  zelda3::GameData* game_data() const { return game_data_; }

 private:
  void DrawPalettesetList();
  void DrawPalettesetEditor();
  void DrawPalettePreview(gfx::SnesPalette& palette, const char* label);
  void DrawPaletteGrid(gfx::SnesPalette& palette, bool editable = false);

  uint8_t selected_paletteset_ = 0;
  bool show_all_colors_ = false;

  Rom* rom_ = nullptr;
  zelda3::GameData* game_data_ = nullptr;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_GRAPHICS_PALETTESET_EDITOR_PANEL_H_

