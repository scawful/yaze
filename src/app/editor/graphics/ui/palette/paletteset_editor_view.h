#ifndef YAZE_APP_EDITOR_GRAPHICS_PALETTESET_EDITOR_VIEW_H_
#define YAZE_APP_EDITOR_GRAPHICS_PALETTESET_EDITOR_VIEW_H_

#include <cstdint>

#include "absl/status/status.h"
#include "app/editor/system/editor_panel.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/core/icons.h"
#include "rom/rom.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace editor {

/**
 * @class PalettesetEditorView
 * @brief Dedicated view for editing dungeon palette sets.
 *
 * A paletteset defines which palettes are used together in a dungeon room:
 * - Dungeon Main: The primary background/tileset palette
 * - Sprite Aux 1-3: Three auxiliary sprite palettes for enemies/NPCs
 *
 * This view allows inspecting and editing these associations, providing
 * a better UX than the combined GfxGroupEditor tab.
 */
class PalettesetEditorView : public WindowContent {
 public:
  std::string GetId() const override { return "graphics.paletteset_editor"; }
  std::string GetDisplayName() const override { return "Palettesets"; }
  std::string GetIcon() const override { return ICON_MD_COLOR_LENS; }
  std::string GetEditorCategory() const override { return "Graphics"; }
  int GetPriority() const override { return 45; }
  float GetPreferredWidth() const override { return 500.0f; }
  void Draw(bool* p_open) override;
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

#endif  // YAZE_APP_EDITOR_GRAPHICS_PALETTESET_EDITOR_VIEW_H_
