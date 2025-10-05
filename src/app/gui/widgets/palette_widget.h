#ifndef YAZE_APP_GUI_PALETTE_WIDGET_H
#define YAZE_APP_GUI_PALETTE_WIDGET_H

#include <vector>
#include <map>
#include "app/gfx/snes_palette.h"
#include "app/gfx/bitmap.h"
#include "app/rom.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

/**
 * @brief Palette widget with ROM integration, analysis tools, and AI tool call support
 * 
 * This widget provides comprehensive palette editing capabilities including:
 * - Grid-based color editing with preview
 * - ROM palette browser and manager
 * - Color analysis and statistics
 * - Export/import functionality
 * - AI agent tool call integration for programmatic palette access
 */
class PaletteWidget {
public:
  PaletteWidget() = default;
  
  /**
   * @brief Initialize the palette editor with ROM data
   */
  void Initialize(Rom* rom);
  
  /**
   * @brief Show the main palette editor window
   */
  void ShowPaletteEditor(gfx::SnesPalette& palette, const std::string& title = "Palette Editor");
  
  /**
   * @brief Show the ROM palette manager window
   */
  void ShowROMPaletteManager();
  
  /**
   * @brief Show color analysis window for a bitmap
   */
  void ShowColorAnalysis(const gfx::Bitmap& bitmap, const std::string& title = "Color Analysis");
  
  /**
   * @brief Apply a ROM palette group to a bitmap
   */
  bool ApplyROMPalette(gfx::Bitmap* bitmap, int group_index, int palette_index);
  
  /**
   * @brief Get the currently selected ROM palette
   */
  const gfx::SnesPalette* GetSelectedROMPalette() const;
  
  /**
   * @brief Save current palette as backup
   */
  void SavePaletteBackup(const gfx::SnesPalette& palette);
  
  /**
   * @brief Restore palette from backup
   */
  bool RestorePaletteBackup(gfx::SnesPalette& palette);
  
  // Accessors
  bool IsROMLoaded() const { return rom_ != nullptr; }
  int GetCurrentGroupIndex() const { return current_group_index_; }
  int GetCurrentPaletteIndex() const { return current_palette_index_; }
  void DrawROMPaletteSelector();
 
private:
  void DrawPaletteGrid(gfx::SnesPalette& palette, int cols = 8);
  void DrawColorEditControls(gfx::SnesColor& color, int color_index);
  void DrawPaletteAnalysis(const gfx::SnesPalette& palette);
  void LoadROMPalettes();
  
  Rom* rom_ = nullptr;
  std::vector<gfx::SnesPalette> rom_palette_groups_;
  std::vector<std::string> palette_group_names_;
  gfx::SnesPalette backup_palette_;
  
  int current_group_index_ = 0;
  int current_palette_index_ = 0;
  bool rom_palettes_loaded_ = false;
  bool show_color_analysis_ = false;
  bool show_rom_manager_ = false;
  
  // Color editing state
  int editing_color_index_ = -1;
  ImVec4 temp_color_ = ImVec4(0, 0, 0, 1);
};

} // namespace gui
} // namespace yaze

#endif // YAZE_APP_GUI_WIDGETS_PALETTE_WIDGET_H
