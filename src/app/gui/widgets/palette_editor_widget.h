#ifndef YAZE_APP_GUI_WIDGETS_PALETTE_EDITOR_WIDGET_H
#define YAZE_APP_GUI_WIDGETS_PALETTE_EDITOR_WIDGET_H

#include <functional>
#include <map>
#include <vector>

#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_palette.h"
#include "app/rom.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

class PaletteEditorWidget {
 public:
  PaletteEditorWidget() = default;

  void Initialize(Rom* rom);

  // Embedded drawing function, like the old PaletteEditorWidget
  void Draw();

  // Modal dialogs from the more feature-rich PaletteWidget
  void ShowPaletteEditor(gfx::SnesPalette& palette,
                         const std::string& title = "Palette Editor");
  void ShowROMPaletteManager();
  void ShowColorAnalysis(const gfx::Bitmap& bitmap,
                         const std::string& title = "Color Analysis");

  bool ApplyROMPalette(gfx::Bitmap* bitmap, int group_index, int palette_index);
  const gfx::SnesPalette* GetSelectedROMPalette() const;
  void SavePaletteBackup(const gfx::SnesPalette& palette);
  bool RestorePaletteBackup(gfx::SnesPalette& palette);

  // Callback when palette is modified
  void SetOnPaletteChanged(std::function<void(int palette_id)> callback) {
    on_palette_changed_ = callback;
  }

  // Get/Set current editing palette
  int GetCurrentPaletteId() const { return current_palette_id_; }
  void SetCurrentPaletteId(int id) { current_palette_id_ = id; }

  bool IsROMLoaded() const { return rom_ != nullptr; }
  int GetCurrentGroupIndex() const { return current_group_index_; }
  void DrawROMPaletteSelector();

 private:
  void DrawPaletteGrid(gfx::SnesPalette& palette, int cols = 15);
  void DrawColorEditControls(gfx::SnesColor& color, int color_index);
  void DrawPaletteAnalysis(const gfx::SnesPalette& palette);
  void LoadROMPalettes();

  // For embedded view
  void DrawPaletteSelector();
  void DrawColorPicker();

  Rom* rom_ = nullptr;
  std::vector<gfx::SnesPalette> rom_palette_groups_;
  std::vector<std::string> palette_group_names_;
  gfx::SnesPalette backup_palette_;

  int current_group_index_ = 0;
  int current_palette_index_ = 0;  // used by ROM palette selector
  bool rom_palettes_loaded_ = false;
  bool show_color_analysis_ = false;
  bool show_rom_manager_ = false;

  // State for embedded editor
  int current_palette_id_ = 0;
  int selected_color_index_ = -1;
  ImVec4 editing_color_{0, 0, 0, 1};

  // Callback for palette changes
  std::function<void(int palette_id)> on_palette_changed_;

  // Color editing state
  int editing_color_index_ = -1;
  ImVec4 temp_color_ = ImVec4(0, 0, 0, 1);
};

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_WIDGETS_PALETTE_EDITOR_WIDGET_H
