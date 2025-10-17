#ifndef YAZE_APP_GUI_BPP_FORMAT_UI_H
#define YAZE_APP_GUI_BPP_FORMAT_UI_H

#include <string>
#include <vector>
#include <functional>

#include "app/gfx/util/bpp_format_manager.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_palette.h"

namespace yaze {
namespace gui {

/**
 * @brief BPP format selection and conversion UI component
 * 
 * Provides a comprehensive UI for BPP format management in the YAZE ROM hacking editor.
 * Includes format selection, conversion preview, and analysis tools.
 */
class BppFormatUI {
 public:
  /**
   * @brief Constructor
   * @param id Unique identifier for this UI component
   */
  explicit BppFormatUI(const std::string& id);
  
  /**
   * @brief Render the BPP format selection UI
   * @param bitmap Current bitmap being edited
   * @param palette Current palette
   * @param on_format_changed Callback when format is changed
   * @return True if format was changed
   */
  bool RenderFormatSelector(gfx::Bitmap* bitmap, const gfx::SnesPalette& palette,
                           std::function<void(gfx::BppFormat)> on_format_changed);
  
  /**
   * @brief Render format analysis panel
   * @param bitmap Bitmap to analyze
   * @param palette Palette to analyze
   */
  void RenderAnalysisPanel(const gfx::Bitmap& bitmap, const gfx::SnesPalette& palette);
  
  /**
   * @brief Render conversion preview
   * @param bitmap Source bitmap
   * @param target_format Target BPP format
   * @param palette Source palette
   */
  void RenderConversionPreview(const gfx::Bitmap& bitmap, gfx::BppFormat target_format,
                              const gfx::SnesPalette& palette);
  
  /**
   * @brief Render graphics sheet analysis
   * @param sheet_data Graphics sheet data
   * @param sheet_id Sheet identifier
   * @param palette Sheet palette
   */
  void RenderSheetAnalysis(const std::vector<uint8_t>& sheet_data, int sheet_id,
                          const gfx::SnesPalette& palette);
  
  /**
   * @brief Get currently selected BPP format
   * @return Selected BPP format
   */
  gfx::BppFormat GetSelectedFormat() const { return selected_format_; }
  
  /**
   * @brief Set the selected BPP format
   * @param format BPP format to select
   */
  void SetSelectedFormat(gfx::BppFormat format) { selected_format_ = format; }
  
  /**
   * @brief Check if format conversion is available
   * @param from_format Source format
   * @param to_format Target format
   * @return True if conversion is available
   */
  bool IsConversionAvailable(gfx::BppFormat from_format, gfx::BppFormat to_format) const;
  
  /**
   * @brief Get conversion efficiency score
   * @param from_format Source format
   * @param to_format Target format
   * @return Efficiency score (0-100)
   */
  int GetConversionEfficiency(gfx::BppFormat from_format, gfx::BppFormat to_format) const;

 private:
  std::string id_;
  gfx::BppFormat selected_format_;
  gfx::BppFormat preview_format_;
  bool show_analysis_;
  bool show_preview_;
  bool show_sheet_analysis_;
  
  // Analysis cache
  std::unordered_map<int, gfx::GraphicsSheetAnalysis> cached_analysis_;
  
  // UI state
  bool format_changed_;
  std::string last_analysis_sheet_;
  
  // Helper methods
  void RenderFormatInfo(const gfx::BppFormatInfo& info);
  void RenderColorUsageChart(const std::vector<int>& color_usage);
  void RenderConversionHistory(const std::string& history);
  std::string GetFormatDescription(gfx::BppFormat format) const;
  ImVec4 GetFormatColor(gfx::BppFormat format) const;
  void UpdateAnalysisCache(int sheet_id, const gfx::GraphicsSheetAnalysis& analysis);
};

/**
 * @brief BPP format conversion dialog
 */
class BppConversionDialog {
 public:
  /**
   * @brief Constructor
   * @param id Unique identifier
   */
  explicit BppConversionDialog(const std::string& id);
  
  /**
   * @brief Show the conversion dialog
   * @param bitmap Bitmap to convert
   * @param palette Palette to use
   * @param on_convert Callback when conversion is confirmed
   */
  void Show(const gfx::Bitmap& bitmap, const gfx::SnesPalette& palette,
           std::function<void(gfx::BppFormat, bool)> on_convert);
  
  /**
   * @brief Render the dialog
   * @return True if dialog should remain open
   */
  bool Render();
  
  /**
   * @brief Check if dialog is open
   * @return True if dialog is open
   */
  bool IsOpen() const { return is_open_; }
  
  /**
   * @brief Close the dialog
   */
  void Close() { is_open_ = false; }

 private:
  std::string id_;
  bool is_open_;
  gfx::Bitmap source_bitmap_;
  gfx::SnesPalette source_palette_;
  gfx::BppFormat target_format_;
  bool preserve_palette_;
  std::function<void(gfx::BppFormat, bool)> convert_callback_;
  
  // Preview data
  std::vector<uint8_t> preview_data_;
  gfx::Bitmap preview_bitmap_;
  bool preview_valid_;
  
  // UI state
  bool show_preview_;
  float preview_scale_;
  
  // Helper methods
  void UpdatePreview();
  void RenderFormatSelector();
  void RenderPreview();
  void RenderOptions();
  void RenderButtons();
};

/**
 * @brief BPP format comparison tool
 */
class BppComparisonTool {
 public:
  /**
   * @brief Constructor
   * @param id Unique identifier
   */
  explicit BppComparisonTool(const std::string& id);
  
  /**
   * @brief Set source bitmap for comparison
   * @param bitmap Source bitmap
   * @param palette Source palette
   */
  void SetSource(const gfx::Bitmap& bitmap, const gfx::SnesPalette& palette);
  
  /**
   * @brief Render the comparison tool
   */
  void Render();
  
  /**
   * @brief Check if tool is open
   * @return True if tool is open
   */
  bool IsOpen() const { return is_open_; }
  
  /**
   * @brief Open the tool
   */
  void Open() { is_open_ = true; }
  
  /**
   * @brief Close the tool
   */
  void Close() { is_open_ = false; }

 private:
  std::string id_;
  bool is_open_;
  
  // Source data
  gfx::Bitmap source_bitmap_;
  gfx::SnesPalette source_palette_;
  bool has_source_;
  
  // Comparison data
  std::unordered_map<gfx::BppFormat, gfx::Bitmap> comparison_bitmaps_;
  std::unordered_map<gfx::BppFormat, gfx::SnesPalette> comparison_palettes_;
  std::unordered_map<gfx::BppFormat, bool> comparison_valid_;
  
  // UI state
  float comparison_scale_;
  bool show_metrics_;
  gfx::BppFormat selected_comparison_;
  
  // Helper methods
  void GenerateComparisons();
  void RenderComparisonGrid();
  void RenderMetrics();
  void RenderFormatSelector();
  std::string CalculateMetrics(gfx::BppFormat format) const;
};

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_BPP_FORMAT_UI_H
