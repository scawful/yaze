#include "bpp_format_ui.h"

#include <algorithm>
#include <sstream>

#include "app/gfx/core/bitmap.h"
#include "app/gfx/util/bpp_format_manager.h"
#include "app/gui/core/ui_helpers.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

BppFormatUI::BppFormatUI(const std::string& id)
    : id_(id),
      selected_format_(gfx::BppFormat::kBpp8),
      preview_format_(gfx::BppFormat::kBpp8),
      show_analysis_(false),
      show_preview_(false),
      show_sheet_analysis_(false),
      format_changed_(false),
      last_analysis_sheet_("") {}

bool BppFormatUI::RenderFormatSelector(
    gfx::Bitmap* bitmap, const gfx::SnesPalette& palette,
    std::function<void(gfx::BppFormat)> on_format_changed) {
  if (!bitmap)
    return false;

  format_changed_ = false;

  ImGui::BeginGroup();
  ImGui::Text("BPP Format Selection");
  ImGui::Separator();

  // Current format detection
  gfx::BppFormat current_format = gfx::BppFormatManager::Get().DetectFormat(
      bitmap->vector(), bitmap->width(), bitmap->height());

  ImGui::Text(
      "Current Format: %s",
      gfx::BppFormatManager::Get().GetFormatInfo(current_format).name.c_str());

  // Format selection
  ImGui::Text("Target Format:");
  ImGui::SameLine();

  const char* format_names[] = {"2BPP", "3BPP", "4BPP", "8BPP"};
  int current_selection =
      static_cast<int>(selected_format_) - 2;  // Convert to 0-based index

  if (ImGui::Combo("##BppFormat", &current_selection, format_names, 4)) {
    selected_format_ = static_cast<gfx::BppFormat>(current_selection + 2);
    format_changed_ = true;
  }

  // Format information
  const auto& format_info =
      gfx::BppFormatManager::Get().GetFormatInfo(selected_format_);
  ImGui::Text("Max Colors: %d", format_info.max_colors);
  ImGui::Text("Bytes per Tile: %d", format_info.bytes_per_tile);
  ImGui::Text("Description: %s", format_info.description.c_str());

  // Conversion efficiency
  if (current_format != selected_format_) {
    int efficiency = GetConversionEfficiency(current_format, selected_format_);
    ImGui::Text("Conversion Efficiency: %d%%", efficiency);

    ImVec4 efficiency_color;
    if (efficiency >= 80) {
      efficiency_color = GetSuccessColor();  // Green
    } else if (efficiency >= 60) {
      efficiency_color = GetWarningColor();  // Yellow
    } else {
      efficiency_color = GetErrorColor();  // Red
    }
    ImGui::TextColored(efficiency_color, "Quality: %s",
                       efficiency >= 80   ? "Excellent"
                       : efficiency >= 60 ? "Good"
                                          : "Poor");
  }

  // Action buttons
  ImGui::Separator();

  if (ImGui::Button("Convert Format")) {
    if (on_format_changed) {
      on_format_changed(selected_format_);
    }
    format_changed_ = true;
  }

  ImGui::SameLine();
  if (ImGui::Button("Show Analysis")) {
    show_analysis_ = !show_analysis_;
  }

  ImGui::SameLine();
  if (ImGui::Button("Preview Conversion")) {
    show_preview_ = !show_preview_;
    preview_format_ = selected_format_;
  }

  ImGui::EndGroup();

  // Analysis panel
  if (show_analysis_) {
    RenderAnalysisPanel(*bitmap, palette);
  }

  // Preview panel
  if (show_preview_) {
    RenderConversionPreview(*bitmap, preview_format_, palette);
  }

  return format_changed_;
}

void BppFormatUI::RenderAnalysisPanel(const gfx::Bitmap& bitmap,
                                      const gfx::SnesPalette& palette) {
  ImGui::Begin("BPP Format Analysis", &show_analysis_);

  // Basic analysis
  gfx::BppFormat detected_format = gfx::BppFormatManager::Get().DetectFormat(
      bitmap.vector(), bitmap.width(), bitmap.height());

  ImGui::Text(
      "Detected Format: %s",
      gfx::BppFormatManager::Get().GetFormatInfo(detected_format).name.c_str());

  // Color usage analysis
  std::vector<int> color_usage(256, 0);
  for (uint8_t pixel : bitmap.vector()) {
    color_usage[pixel]++;
  }

  int used_colors = 0;
  for (int count : color_usage) {
    if (count > 0)
      used_colors++;
  }

  ImGui::Text("Colors Used: %d / %d", used_colors,
              static_cast<int>(palette.size()));
  ImGui::Text("Color Efficiency: %.1f%%",
              (static_cast<float>(used_colors) / palette.size()) * 100.0f);

  // Color usage chart
  if (ImGui::CollapsingHeader("Color Usage Chart")) {
    RenderColorUsageChart(color_usage);
  }

  // Format recommendations
  ImGui::Separator();
  ImGui::Text("Format Recommendations:");

  if (used_colors <= 4) {
    ImGui::TextColored(GetSuccessColor(), "✓ 2BPP format would be optimal");
  } else if (used_colors <= 8) {
    ImGui::TextColored(GetSuccessColor(), "✓ 3BPP format would be optimal");
  } else if (used_colors <= 16) {
    ImGui::TextColored(GetSuccessColor(), "✓ 4BPP format would be optimal");
  } else {
    ImGui::TextColored(GetWarningColor(), "⚠ 8BPP format is necessary");
  }

  // Memory usage comparison
  if (ImGui::CollapsingHeader("Memory Usage Comparison")) {
    const auto& current_info =
        gfx::BppFormatManager::Get().GetFormatInfo(detected_format);
    int current_bytes =
        (bitmap.width() * bitmap.height() * current_info.bits_per_pixel) / 8;

    ImGui::Text("Current Format (%s): %d bytes", current_info.name.c_str(),
                current_bytes);

    for (auto format : gfx::BppFormatManager::Get().GetAvailableFormats()) {
      if (format == detected_format)
        continue;

      const auto& info = gfx::BppFormatManager::Get().GetFormatInfo(format);
      int format_bytes =
          (bitmap.width() * bitmap.height() * info.bits_per_pixel) / 8;
      float ratio = static_cast<float>(format_bytes) / current_bytes;

      ImGui::Text("%s: %d bytes (%.1fx)", info.name.c_str(), format_bytes,
                  ratio);
    }
  }

  ImGui::End();
}

void BppFormatUI::RenderConversionPreview(const gfx::Bitmap& bitmap,
                                          gfx::BppFormat target_format,
                                          const gfx::SnesPalette& palette) {
  ImGui::Begin("BPP Conversion Preview", &show_preview_);

  gfx::BppFormat current_format = gfx::BppFormatManager::Get().DetectFormat(
      bitmap.vector(), bitmap.width(), bitmap.height());

  if (current_format == target_format) {
    ImGui::Text("No conversion needed - formats are identical");
    ImGui::End();
    return;
  }

  // Convert the bitmap
  auto converted_data = gfx::BppFormatManager::Get().ConvertFormat(
      bitmap.vector(), current_format, target_format, bitmap.width(),
      bitmap.height());

  // Create preview bitmap
  gfx::Bitmap preview_bitmap(bitmap.width(), bitmap.height(), bitmap.depth(),
                             converted_data, palette);

  // Render side-by-side comparison
  ImGui::Text(
      "Original (%s) vs Converted (%s)",
      gfx::BppFormatManager::Get().GetFormatInfo(current_format).name.c_str(),
      gfx::BppFormatManager::Get().GetFormatInfo(target_format).name.c_str());

  ImGui::Columns(2, "PreviewColumns");

  // Original
  ImGui::Text("Original");
  if (bitmap.texture()) {
    ImGui::Image((ImTextureID)(intptr_t)bitmap.texture(),
                 ImVec2(256, 256 * bitmap.height() / bitmap.width()));
  }

  ImGui::NextColumn();

  // Converted
  ImGui::Text("Converted");
  if (preview_bitmap.texture()) {
    ImGui::Image(
        (ImTextureID)(intptr_t)preview_bitmap.texture(),
        ImVec2(256, 256 * preview_bitmap.height() / preview_bitmap.width()));
  }

  ImGui::Columns(1);

  // Conversion statistics
  ImGui::Separator();
  ImGui::Text("Conversion Statistics:");

  const auto& from_info =
      gfx::BppFormatManager::Get().GetFormatInfo(current_format);
  const auto& to_info =
      gfx::BppFormatManager::Get().GetFormatInfo(target_format);

  int from_bytes =
      (bitmap.width() * bitmap.height() * from_info.bits_per_pixel) / 8;
  int to_bytes =
      (bitmap.width() * bitmap.height() * to_info.bits_per_pixel) / 8;

  ImGui::Text("Size: %d bytes -> %d bytes", from_bytes, to_bytes);
  ImGui::Text("Compression Ratio: %.2fx",
              static_cast<float>(from_bytes) / to_bytes);

  ImGui::End();
}

void BppFormatUI::RenderSheetAnalysis(const std::vector<uint8_t>& sheet_data,
                                      int sheet_id,
                                      const gfx::SnesPalette& palette) {
  std::string analysis_key = "sheet_" + std::to_string(sheet_id);

  // Check if we need to update analysis
  if (last_analysis_sheet_ != analysis_key) {
    auto analysis = gfx::BppFormatManager::Get().AnalyzeGraphicsSheet(
        sheet_data, sheet_id, palette);
    UpdateAnalysisCache(sheet_id, analysis);
    last_analysis_sheet_ = analysis_key;
  }

  auto it = cached_analysis_.find(sheet_id);
  if (it == cached_analysis_.end())
    return;

  const auto& analysis = it->second;

  ImGui::Begin("Graphics Sheet Analysis", &show_sheet_analysis_);

  ImGui::Text("Sheet ID: %d", analysis.sheet_id);
  ImGui::Text("Original Format: %s",
              gfx::BppFormatManager::Get()
                  .GetFormatInfo(analysis.original_format)
                  .name.c_str());
  ImGui::Text("Current Format: %s", gfx::BppFormatManager::Get()
                                        .GetFormatInfo(analysis.current_format)
                                        .name.c_str());

  if (analysis.was_converted) {
    ImGui::TextColored(GetWarningColor(), "⚠ This sheet was converted");
    ImGui::Text("Conversion History: %s", analysis.conversion_history.c_str());
  } else {
    ImGui::TextColored(GetSuccessColor(), "✓ Original format preserved");
  }

  ImGui::Separator();
  ImGui::Text("Color Usage: %d / %d colors used", analysis.palette_entries_used,
              static_cast<int>(palette.size()));
  ImGui::Text("Compression Ratio: %.2fx", analysis.compression_ratio);
  ImGui::Text("Size: %zu -> %zu bytes", analysis.original_size,
              analysis.current_size);

  // Tile usage pattern
  if (ImGui::CollapsingHeader("Tile Usage Pattern")) {
    int total_tiles = analysis.tile_usage_pattern.size();
    int used_tiles = 0;
    int empty_tiles = 0;

    for (int usage : analysis.tile_usage_pattern) {
      if (usage > 0) {
        used_tiles++;
      } else {
        empty_tiles++;
      }
    }

    ImGui::Text("Total Tiles: %d", total_tiles);
    ImGui::Text("Used Tiles: %d (%.1f%%)", used_tiles,
                (static_cast<float>(used_tiles) / total_tiles) * 100.0f);
    ImGui::Text("Empty Tiles: %d (%.1f%%)", empty_tiles,
                (static_cast<float>(empty_tiles) / total_tiles) * 100.0f);
  }

  // Recommendations
  ImGui::Separator();
  ImGui::Text("Recommendations:");

  if (analysis.was_converted && analysis.palette_entries_used <= 16) {
    ImGui::TextColored(
        GetSuccessColor(),
        "✓ Consider reverting to %s format for better compression",
        gfx::BppFormatManager::Get()
            .GetFormatInfo(analysis.original_format)
            .name.c_str());
  }

  if (analysis.palette_entries_used < static_cast<int>(palette.size()) / 2) {
    ImGui::TextColored(GetWarningColor(),
                       "⚠ Palette is underutilized - consider optimization");
  }

  ImGui::End();
}

bool BppFormatUI::IsConversionAvailable(gfx::BppFormat from_format,
                                        gfx::BppFormat to_format) const {
  // All conversions are available in our implementation
  return from_format != to_format;
}

int BppFormatUI::GetConversionEfficiency(gfx::BppFormat from_format,
                                         gfx::BppFormat to_format) const {
  // Calculate efficiency based on format compatibility
  if (from_format == to_format)
    return 100;

  // Higher BPP to lower BPP conversions may lose quality
  if (static_cast<int>(from_format) > static_cast<int>(to_format)) {
    int bpp_diff = static_cast<int>(from_format) - static_cast<int>(to_format);
    return std::max(
        20, 100 - (bpp_diff * 20));  // Reduce efficiency by 20% per BPP level
  }

  // Lower BPP to higher BPP conversions are lossless
  return 100;
}

void BppFormatUI::RenderFormatInfo(const gfx::BppFormatInfo& info) {
  ImGui::Text("Format: %s", info.name.c_str());
  ImGui::Text("Bits per Pixel: %d", info.bits_per_pixel);
  ImGui::Text("Max Colors: %d", info.max_colors);
  ImGui::Text("Bytes per Tile: %d", info.bytes_per_tile);
  ImGui::Text("Compressed: %s", info.is_compressed ? "Yes" : "No");
  ImGui::Text("Description: %s", info.description.c_str());
}

void BppFormatUI::RenderColorUsageChart(const std::vector<int>& color_usage) {
  // Find maximum usage for scaling
  int max_usage = *std::max_element(color_usage.begin(), color_usage.end());
  if (max_usage == 0)
    return;

  // Render simple bar chart
  ImGui::Text("Color Usage Distribution:");

  for (size_t i = 0; i < std::min(color_usage.size(), size_t(16)); ++i) {
    if (color_usage[i] > 0) {
      float usage_ratio = static_cast<float>(color_usage[i]) / max_usage;
      ImGui::Text("Color %zu: %d pixels (%.1f%%)", i, color_usage[i],
                  (static_cast<float>(color_usage[i]) / (16 * 16)) * 100.0f);
      ImGui::SameLine();
      ImGui::ProgressBar(usage_ratio, ImVec2(100, 0));
    }
  }
}

void BppFormatUI::RenderConversionHistory(const std::string& history) {
  ImGui::Text("Conversion History:");
  ImGui::TextWrapped("%s", history.c_str());
}

std::string BppFormatUI::GetFormatDescription(gfx::BppFormat format) const {
  return gfx::BppFormatManager::Get().GetFormatInfo(format).description;
}

ImVec4 BppFormatUI::GetFormatColor(gfx::BppFormat format) const {
  switch (format) {
    case gfx::BppFormat::kBpp2:
      return ImVec4(1, 0, 0, 1);  // Red
    case gfx::BppFormat::kBpp3:
      return ImVec4(1, 1, 0, 1);  // Yellow
    case gfx::BppFormat::kBpp4:
      return ImVec4(0, 1, 0, 1);  // Green
    case gfx::BppFormat::kBpp8:
      return ImVec4(0, 0, 1, 1);  // Blue
    default:
      return ImVec4(1, 1, 1, 1);  // White
  }
}

void BppFormatUI::UpdateAnalysisCache(
    int sheet_id, const gfx::GraphicsSheetAnalysis& analysis) {
  cached_analysis_[sheet_id] = analysis;
}

// BppConversionDialog implementation

BppConversionDialog::BppConversionDialog(const std::string& id)
    : id_(id),
      is_open_(false),
      target_format_(gfx::BppFormat::kBpp8),
      preserve_palette_(true),
      preview_valid_(false),
      show_preview_(true),
      preview_scale_(1.0f) {}

void BppConversionDialog::Show(
    const gfx::Bitmap& bitmap, const gfx::SnesPalette& palette,
    std::function<void(gfx::BppFormat, bool)> on_convert) {
  source_bitmap_ = bitmap;
  source_palette_ = palette;
  convert_callback_ = on_convert;
  is_open_ = true;
  preview_valid_ = false;
}

bool BppConversionDialog::Render() {
  if (!is_open_)
    return false;

  ImGui::OpenPopup("BPP Format Conversion");

  if (ImGui::BeginPopupModal("BPP Format Conversion", &is_open_,
                             ImGuiWindowFlags_AlwaysAutoResize)) {

    RenderFormatSelector();
    ImGui::Separator();
    RenderOptions();
    ImGui::Separator();

    if (show_preview_) {
      RenderPreview();
      ImGui::Separator();
    }

    RenderButtons();

    ImGui::EndPopup();
  }

  return is_open_;
}

void BppConversionDialog::UpdatePreview() {
  if (preview_valid_)
    return;

  gfx::BppFormat current_format = gfx::BppFormatManager::Get().DetectFormat(
      source_bitmap_.vector(), source_bitmap_.width(), source_bitmap_.height());

  if (current_format == target_format_) {
    preview_bitmap_ = source_bitmap_;
    preview_valid_ = true;
    return;
  }

  auto converted_data = gfx::BppFormatManager::Get().ConvertFormat(
      source_bitmap_.vector(), current_format, target_format_,
      source_bitmap_.width(), source_bitmap_.height());

  preview_bitmap_ =
      gfx::Bitmap(source_bitmap_.width(), source_bitmap_.height(),
                  source_bitmap_.depth(), converted_data, source_palette_);
  preview_valid_ = true;
}

void BppConversionDialog::RenderFormatSelector() {
  ImGui::Text("Convert to BPP Format:");

  const char* format_names[] = {"2BPP", "3BPP", "4BPP", "8BPP"};
  int current_selection = static_cast<int>(target_format_) - 2;

  if (ImGui::Combo("##TargetFormat", &current_selection, format_names, 4)) {
    target_format_ = static_cast<gfx::BppFormat>(current_selection + 2);
    preview_valid_ = false;  // Invalidate preview
  }

  const auto& format_info =
      gfx::BppFormatManager::Get().GetFormatInfo(target_format_);
  ImGui::Text("Max Colors: %d", format_info.max_colors);
  ImGui::Text("Description: %s", format_info.description.c_str());
}

void BppConversionDialog::RenderPreview() {
  if (ImGui::Button("Update Preview")) {
    preview_valid_ = false;
  }

  UpdatePreview();

  if (preview_valid_ && preview_bitmap_.texture()) {
    ImGui::Text("Preview:");
    ImGui::Image((ImTextureID)(intptr_t)preview_bitmap_.texture(),
                 ImVec2(128 * preview_scale_, 128 * preview_scale_));

    ImGui::SliderFloat("Scale", &preview_scale_, 0.5f, 3.0f);
  }
}

void BppConversionDialog::RenderOptions() {
  ImGui::Checkbox("Preserve Palette", &preserve_palette_);
  ImGui::SameLine();
  ImGui::Checkbox("Show Preview", &show_preview_);
}

void BppConversionDialog::RenderButtons() {
  if (ImGui::Button("Convert")) {
    if (convert_callback_) {
      convert_callback_(target_format_, preserve_palette_);
    }
    is_open_ = false;
  }

  ImGui::SameLine();
  if (ImGui::Button("Cancel")) {
    is_open_ = false;
  }
}

// BppComparisonTool implementation

BppComparisonTool::BppComparisonTool(const std::string& id)
    : id_(id),
      is_open_(false),
      has_source_(false),
      comparison_scale_(1.0f),
      show_metrics_(true),
      selected_comparison_(gfx::BppFormat::kBpp8) {}

void BppComparisonTool::SetSource(const gfx::Bitmap& bitmap,
                                  const gfx::SnesPalette& palette) {
  source_bitmap_ = bitmap;
  source_palette_ = palette;
  has_source_ = true;
  GenerateComparisons();
}

void BppComparisonTool::Render() {
  if (!is_open_ || !has_source_)
    return;

  ImGui::Begin("BPP Format Comparison", &is_open_);

  RenderFormatSelector();
  ImGui::Separator();
  RenderComparisonGrid();

  if (show_metrics_) {
    ImGui::Separator();
    RenderMetrics();
  }

  ImGui::End();
}

void BppComparisonTool::GenerateComparisons() {
  gfx::BppFormat source_format = gfx::BppFormatManager::Get().DetectFormat(
      source_bitmap_.vector(), source_bitmap_.width(), source_bitmap_.height());

  for (auto format : gfx::BppFormatManager::Get().GetAvailableFormats()) {
    if (format == source_format) {
      comparison_bitmaps_[format] = source_bitmap_;
      comparison_palettes_[format] = source_palette_;
      comparison_valid_[format] = true;
      continue;
    }

    try {
      auto converted_data = gfx::BppFormatManager::Get().ConvertFormat(
          source_bitmap_.vector(), source_format, format,
          source_bitmap_.width(), source_bitmap_.height());

      comparison_bitmaps_[format] =
          gfx::Bitmap(source_bitmap_.width(), source_bitmap_.height(),
                      source_bitmap_.depth(), converted_data, source_palette_);
      comparison_palettes_[format] = source_palette_;
      comparison_valid_[format] = true;
    } catch (...) {
      comparison_valid_[format] = false;
    }
  }
}

void BppComparisonTool::RenderComparisonGrid() {
  ImGui::Text("Format Comparison (Scale: %.1fx)", comparison_scale_);
  ImGui::SliderFloat("##Scale", &comparison_scale_, 0.5f, 3.0f);

  ImGui::Columns(2, "ComparisonColumns");

  for (auto format : gfx::BppFormatManager::Get().GetAvailableFormats()) {
    auto it = comparison_bitmaps_.find(format);
    if (it == comparison_bitmaps_.end() || !comparison_valid_[format])
      continue;

    const auto& bitmap = it->second;
    const auto& format_info =
        gfx::BppFormatManager::Get().GetFormatInfo(format);

    ImGui::Text("%s", format_info.name.c_str());

    if (bitmap.texture()) {
      ImGui::Image((ImTextureID)(intptr_t)bitmap.texture(),
                   ImVec2(128 * comparison_scale_, 128 * comparison_scale_));
    }

    ImGui::NextColumn();
  }

  ImGui::Columns(1);
}

void BppComparisonTool::RenderMetrics() {
  ImGui::Text("Format Metrics:");

  for (auto format : gfx::BppFormatManager::Get().GetAvailableFormats()) {
    if (!comparison_valid_[format])
      continue;

    const auto& format_info =
        gfx::BppFormatManager::Get().GetFormatInfo(format);
    std::string metrics = CalculateMetrics(format);

    ImGui::Text("%s: %s", format_info.name.c_str(), metrics.c_str());
  }
}

void BppComparisonTool::RenderFormatSelector() {
  ImGui::Text("Selected for Analysis: ");
  ImGui::SameLine();

  const char* format_names[] = {"2BPP", "3BPP", "4BPP", "8BPP"};
  int selection = static_cast<int>(selected_comparison_) - 2;

  if (ImGui::Combo("##SelectedFormat", &selection, format_names, 4)) {
    selected_comparison_ = static_cast<gfx::BppFormat>(selection + 2);
  }

  ImGui::SameLine();
  ImGui::Checkbox("Show Metrics", &show_metrics_);
}

std::string BppComparisonTool::CalculateMetrics(gfx::BppFormat format) const {
  const auto& format_info = gfx::BppFormatManager::Get().GetFormatInfo(format);
  int bytes = (source_bitmap_.width() * source_bitmap_.height() *
               format_info.bits_per_pixel) /
              8;

  std::ostringstream metrics;
  metrics << bytes << " bytes, " << format_info.max_colors << " colors";

  return metrics.str();
}

}  // namespace gui
}  // namespace yaze
