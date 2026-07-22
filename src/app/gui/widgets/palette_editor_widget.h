#ifndef YAZE_APP_GUI_WIDGETS_PALETTE_EDITOR_WIDGET_H
#define YAZE_APP_GUI_WIDGETS_PALETTE_EDITOR_WIDGET_H

#include <functional>
#include <map>
#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_palette.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace gui {

enum class DungeonRenderPaletteSource {
  kHud,
  kDungeonMain,
};

struct DungeonPaletteChange {
  int palette_id;
  DungeonRenderPaletteSource source;
};

class PaletteEditorWidget {
 public:
  PaletteEditorWidget() = default;

  void Initialize(zelda3::GameData* game_data);
  void Initialize(Rom* rom);  // Legacy, deprecated

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

  // Original palette-id-only callback retained for source compatibility.
  void SetOnPaletteChanged(std::function<void(int)> callback) {
    if (!callback) {
      on_palette_changed_ = {};
      return;
    }
    on_palette_changed_ = [callback](DungeonPaletteChange change) {
      callback(change.palette_id);
    };
  }

  // Typed callback for dungeon rendering consumers that must distinguish
  // shared HUD edits from palette-scoped dungeon-main edits.
  void SetOnDungeonPaletteChanged(
      std::function<void(DungeonPaletteChange)> callback) {
    on_palette_changed_ = callback;
  }

  // Get/Set current editing palette
  int GetCurrentPaletteId() const { return current_palette_id_; }
  void SetCurrentPaletteId(int id) { current_palette_id_ = id; }
  void SetDungeonRenderPaletteMode(bool enabled) {
    dungeon_render_palette_mode_ = enabled;
  }

  // Apply one editable CGRAM slot from the dungeon render palette. Passing a
  // color records the edit through PaletteManager; std::nullopt resets the
  // slot to PaletteManager's original ROM snapshot. The palette-change
  // callback is fired only after a successful managed edit.
  absl::Status ApplyDungeonRenderColorEdit(int display_index,
                                           std::optional<gfx::SnesColor> color);

  bool IsROMLoaded() const { return rom_ != nullptr; }
  int GetCurrentGroupIndex() const { return current_group_index_; }
  void DrawROMPaletteSelector();

 private:
  void DrawPaletteGrid(gfx::SnesPalette& palette, int cols = 15);
  void DrawColorEditControls(gfx::SnesColor& color, int color_index);
  void DrawPaletteAnalysis(const gfx::SnesPalette& palette);
  void LoadROMPalettes();
  float ComputeSwatchSize(int columns, float min_size, float max_size) const;

  // For embedded view
  void DrawPaletteSelector();
  void DrawColorPicker();
  void DrawDungeonRenderPalette();
  void DrawDungeonRenderColorPicker();

  zelda3::GameData* game_data_ = nullptr;
  Rom* rom_ = nullptr;  // Legacy, deprecated
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
  std::function<void(DungeonPaletteChange)> on_palette_changed_;

  // Color editing state
  int editing_color_index_ = -1;
  ImVec4 temp_color_ = ImVec4(0, 0, 0, 1);
  bool dungeon_render_palette_mode_ = false;
};

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_WIDGETS_PALETTE_EDITOR_WIDGET_H
