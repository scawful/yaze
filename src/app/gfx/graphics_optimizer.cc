#include "app/gfx/graphics_optimizer.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "app/gfx/bpp_format_manager.h"

namespace yaze {
namespace gfx {

GraphicsOptimizer& GraphicsOptimizer::Get() {
  static GraphicsOptimizer instance;
  return instance;
}

void GraphicsOptimizer::Initialize() {
  max_quality_loss_ = 0.1f;
  min_memory_savings_ = 1024;
  performance_threshold_ = 0.05f;

  optimization_stats_.clear();
  optimization_cache_.clear();
}

OptimizationResult GraphicsOptimizer::OptimizeSheet(
    const std::vector<uint8_t>& sheet_data, int sheet_id,
    const SnesPalette& palette, OptimizationStrategy strategy) {
  ScopedTimer timer("graphics_optimize_sheet");

  OptimizationResult result;

  try {
    // Analyze the sheet
    SheetOptimizationData data = AnalyzeSheet(sheet_data, sheet_id, palette);

    if (!data.is_convertible) {
      result.success = false;
      result.message = "Sheet is not suitable for optimization";
      return result;
    }

    // Check if optimization meets criteria
    if (!ShouldOptimize(data, strategy)) {
      result.success = false;
      result.message = "Optimization does not meet criteria";
      return result;
    }

    // Calculate potential savings
    result.memory_saved = data.current_size - data.optimized_size;
    result.performance_gain =
        CalculatePerformanceGain(data.current_format, data.recommended_format);
    result.quality_loss = CalculateQualityLoss(
        data.current_format, data.recommended_format, sheet_data);

    // Check if optimization is worthwhile
    if (result.memory_saved < min_memory_savings_ &&
        result.performance_gain < performance_threshold_ &&
        result.quality_loss > max_quality_loss_) {
      result.success = false;
      result.message = "Optimization benefits do not justify quality loss";
      return result;
    }

    result.success = true;
    result.message = "Optimization recommended";
    result.recommended_formats.push_back(data.recommended_format);
    result.sheet_recommendations[sheet_id] = data.recommended_format;

    UpdateOptimizationStats("sheets_optimized", 1.0);
    UpdateOptimizationStats("memory_saved",
                            static_cast<double>(result.memory_saved));
    UpdateOptimizationStats("performance_gain",
                            static_cast<double>(result.performance_gain));

  } catch (const std::exception& e) {
    result.success = false;
    result.message = "Optimization failed: " + std::string(e.what());
    SDL_Log("GraphicsOptimizer::OptimizeSheet failed: %s", e.what());
  }

  return result;
}

OptimizationResult GraphicsOptimizer::OptimizeSheets(
    const std::unordered_map<int, std::vector<uint8_t>>& sheets,
    const std::unordered_map<int, SnesPalette>& palettes,
    OptimizationStrategy strategy) {
  ScopedTimer timer("graphics_optimize_sheets");

  OptimizationResult result;
  result.success = true;

  size_t total_memory_saved = 0;
  float total_performance_gain = 0.0f;
  float total_quality_loss = 0.0f;
  int optimized_sheets = 0;

  for (const auto& [sheet_id, sheet_data] : sheets) {
    auto palette_it = palettes.find(sheet_id);
    if (palette_it == palettes.end()) {
      continue;  // Skip sheets without palettes
    }

    auto sheet_result =
        OptimizeSheet(sheet_data, sheet_id, palette_it->second, strategy);

    if (sheet_result.success) {
      total_memory_saved += sheet_result.memory_saved;
      total_performance_gain += sheet_result.performance_gain;
      total_quality_loss += sheet_result.quality_loss;
      optimized_sheets++;

      // Merge recommendations
      result.recommended_formats.insert(
          result.recommended_formats.end(),
          sheet_result.recommended_formats.begin(),
          sheet_result.recommended_formats.end());
      result.sheet_recommendations.insert(
          sheet_result.sheet_recommendations.begin(),
          sheet_result.sheet_recommendations.end());
    }
  }

  result.memory_saved = total_memory_saved;
  result.performance_gain =
      optimized_sheets > 0 ? total_performance_gain / optimized_sheets : 0.0f;
  result.quality_loss =
      optimized_sheets > 0 ? total_quality_loss / optimized_sheets : 0.0f;

  if (optimized_sheets > 0) {
    result.message =
        "Optimized " + std::to_string(optimized_sheets) + " sheets";
  } else {
    result.success = false;
    result.message = "No sheets could be optimized";
  }

  UpdateOptimizationStats("batch_optimizations", 1.0);
  UpdateOptimizationStats("total_sheets_processed",
                          static_cast<double>(sheets.size()));

  return result;
}

SheetOptimizationData GraphicsOptimizer::AnalyzeSheet(
    const std::vector<uint8_t>& sheet_data, int sheet_id,
    const SnesPalette& palette) {
  // Check cache first
  std::string cache_key = GenerateCacheKey(sheet_data, sheet_id);
  auto cache_it = optimization_cache_.find(cache_key);
  if (cache_it != optimization_cache_.end()) {
    return cache_it->second;
  }

  ScopedTimer timer("graphics_analyze_sheet");

  SheetOptimizationData data;
  data.sheet_id = sheet_id;
  data.current_size = sheet_data.size();

  // Detect current format
  data.current_format = BppFormatManager::Get().DetectFormat(
      sheet_data, 128, 32);  // Standard sheet size

  // Analyze color usage
  data.colors_used = CountUsedColors(sheet_data, palette);

  // Determine optimal format
  data.recommended_format = DetermineOptimalFormat(
      sheet_data, palette, OptimizationStrategy::kBalanced);

  // Calculate potential savings
  const auto& current_info =
      BppFormatManager::Get().GetFormatInfo(data.current_format);
  const auto& recommended_info =
      BppFormatManager::Get().GetFormatInfo(data.recommended_format);

  data.optimized_size = (sheet_data.size() * recommended_info.bits_per_pixel) /
                        current_info.bits_per_pixel;
  data.compression_ratio =
      static_cast<float>(data.current_size) / data.optimized_size;

  // Determine if conversion is beneficial
  data.is_convertible =
      (data.current_format != data.recommended_format) &&
      (data.colors_used <= recommended_info.max_colors) &&
      (data.compression_ratio > 1.1f);  // At least 10% savings

  data.optimization_reason = GenerateOptimizationReason(data);

  // Cache the result
  optimization_cache_[cache_key] = data;

  return data;
}

std::unordered_map<int, SheetOptimizationData>
GraphicsOptimizer::GetOptimizationRecommendations(
    const std::unordered_map<int, std::vector<uint8_t>>& sheets,
    const std::unordered_map<int, SnesPalette>& palettes) {

  std::unordered_map<int, SheetOptimizationData> recommendations;

  for (const auto& [sheet_id, sheet_data] : sheets) {
    auto palette_it = palettes.find(sheet_id);
    if (palette_it == palettes.end()) {
      continue;
    }

    recommendations[sheet_id] =
        AnalyzeSheet(sheet_data, sheet_id, palette_it->second);
  }

  return recommendations;
}

OptimizationResult GraphicsOptimizer::ApplyOptimizations(
    const std::unordered_map<int, SheetOptimizationData>& recommendations,
    std::unordered_map<int, std::vector<uint8_t>>& sheets,
    std::unordered_map<int, SnesPalette>& palettes) {

  ScopedTimer timer("graphics_apply_optimizations");

  OptimizationResult result;
  result.success = true;

  size_t total_memory_saved = 0;
  int optimized_sheets = 0;

  for (const auto& [sheet_id, data] : recommendations) {
    if (!data.is_convertible) {
      continue;
    }

    auto sheet_it = sheets.find(sheet_id);
    if (sheet_it == sheets.end()) {
      continue;
    }

    try {
      // Convert the sheet data
      auto converted_data = BppFormatManager::Get().ConvertFormat(
          sheet_it->second, data.current_format, data.recommended_format, 128,
          32);

      // Update the sheet
      sheet_it->second = converted_data;

      // Optimize palette if needed
      auto palette_it = palettes.find(sheet_id);
      if (palette_it != palettes.end()) {
        std::vector<int> used_colors;
        for (int i = 0; i < data.colors_used; ++i) {
          used_colors.push_back(i);
        }

        palette_it->second = BppFormatManager::Get().OptimizePaletteForFormat(
            palette_it->second, data.recommended_format, used_colors);
      }

      total_memory_saved += data.current_size - data.optimized_size;
      optimized_sheets++;

      result.sheet_recommendations[sheet_id] = data.recommended_format;

    } catch (const std::exception& e) {
      SDL_Log("Failed to optimize sheet %d: %s", sheet_id, e.what());
    }
  }

  result.memory_saved = total_memory_saved;
  result.message = "Optimized " + std::to_string(optimized_sheets) + " sheets";

  UpdateOptimizationStats("optimizations_applied",
                          static_cast<double>(optimized_sheets));
  UpdateOptimizationStats("total_memory_saved",
                          static_cast<double>(total_memory_saved));

  return result;
}

std::unordered_map<std::string, double>
GraphicsOptimizer::GetOptimizationStats() const {
  return optimization_stats_;
}

void GraphicsOptimizer::ClearCache() {
  optimization_cache_.clear();
  optimization_stats_.clear();
}

void GraphicsOptimizer::SetOptimizationParameters(float max_quality_loss,
                                                  size_t min_memory_savings,
                                                  float performance_threshold) {
  max_quality_loss_ = max_quality_loss;
  min_memory_savings_ = min_memory_savings;
  performance_threshold_ = performance_threshold;
}

// Helper method implementations

BppFormat GraphicsOptimizer::DetermineOptimalFormat(
    const std::vector<uint8_t>& data, const SnesPalette& palette,
    OptimizationStrategy strategy) {
  int colors_used = CountUsedColors(data, palette);

  // Determine optimal format based on color usage and strategy
  switch (strategy) {
    case OptimizationStrategy::kMemoryOptimized:
      if (colors_used <= 4)
        return BppFormat::kBpp2;
      if (colors_used <= 8)
        return BppFormat::kBpp3;
      if (colors_used <= 16)
        return BppFormat::kBpp4;
      break;

    case OptimizationStrategy::kPerformanceOptimized:
      // Prefer formats that work well with atlas rendering
      if (colors_used <= 16)
        return BppFormat::kBpp4;
      break;

    case OptimizationStrategy::kQualityOptimized:
      // Only optimize if significant memory savings
      if (colors_used <= 4)
        return BppFormat::kBpp2;
      break;

    case OptimizationStrategy::kBalanced:
      if (colors_used <= 4)
        return BppFormat::kBpp2;
      if (colors_used <= 8)
        return BppFormat::kBpp3;
      if (colors_used <= 16)
        return BppFormat::kBpp4;
      break;
  }

  return BppFormat::kBpp8;  // Default to 8BPP
}

float GraphicsOptimizer::CalculateQualityLoss(
    BppFormat from_format, BppFormat to_format,
    const std::vector<uint8_t>& data) {
  if (from_format == to_format)
    return 0.0f;

  // Higher BPP to lower BPP conversions may lose quality
  if (static_cast<int>(from_format) > static_cast<int>(to_format)) {
    int bpp_diff = static_cast<int>(from_format) - static_cast<int>(to_format);
    return std::min(
        1.0f, static_cast<float>(bpp_diff) * 0.1f);  // 10% loss per BPP level
  }

  return 0.0f;  // Lower to higher BPP is lossless
}

size_t GraphicsOptimizer::CalculateMemorySavings(
    BppFormat from_format, BppFormat to_format,
    const std::vector<uint8_t>& data) {
  if (from_format == to_format)
    return 0;

  const auto& from_info = BppFormatManager::Get().GetFormatInfo(from_format);
  const auto& to_info = BppFormatManager::Get().GetFormatInfo(to_format);

  size_t from_size = data.size();
  size_t to_size =
      (from_size * to_info.bits_per_pixel) / from_info.bits_per_pixel;

  return from_size - to_size;
}

float GraphicsOptimizer::CalculatePerformanceGain(BppFormat from_format,
                                                  BppFormat to_format) {
  if (from_format == to_format)
    return 0.0f;

  // Lower BPP formats generally render faster
  if (static_cast<int>(from_format) > static_cast<int>(to_format)) {
    int bpp_diff = static_cast<int>(from_format) - static_cast<int>(to_format);
    return std::min(
        0.5f, static_cast<float>(bpp_diff) * 0.1f);  // 10% gain per BPP level
  }

  return 0.0f;
}

bool GraphicsOptimizer::ShouldOptimize(const SheetOptimizationData& data,
                                       OptimizationStrategy strategy) {
  if (!data.is_convertible)
    return false;

  switch (strategy) {
    case OptimizationStrategy::kMemoryOptimized:
      return data.compression_ratio > 1.2f;  // At least 20% savings

    case OptimizationStrategy::kPerformanceOptimized:
      return data.compression_ratio > 1.1f;  // At least 10% savings

    case OptimizationStrategy::kQualityOptimized:
      return data.compression_ratio > 1.5f;  // At least 50% savings

    case OptimizationStrategy::kBalanced:
      return data.compression_ratio > 1.15f;  // At least 15% savings
  }

  return false;
}

std::string GraphicsOptimizer::GenerateOptimizationReason(
    const SheetOptimizationData& data) {
  std::ostringstream reason;

  reason << "Convert from "
         << BppFormatManager::Get().GetFormatInfo(data.current_format).name
         << " to "
         << BppFormatManager::Get().GetFormatInfo(data.recommended_format).name
         << " (uses " << data.colors_used << " colors, " << std::fixed
         << std::setprecision(1) << (data.compression_ratio - 1.0f) * 100.0f
         << "% memory savings)";

  return reason.str();
}

int GraphicsOptimizer::CountUsedColors(const std::vector<uint8_t>& data,
                                       const SnesPalette& palette) {
  std::vector<bool> used_colors(palette.size(), false);

  for (uint8_t pixel : data) {
    if (pixel < palette.size()) {
      used_colors[pixel] = true;
    }
  }

  int count = 0;
  for (bool used : used_colors) {
    if (used)
      count++;
  }

  return count;
}

float GraphicsOptimizer::CalculateColorEfficiency(
    const std::vector<uint8_t>& data, const SnesPalette& palette) {
  int used_colors = CountUsedColors(data, palette);
  return static_cast<float>(used_colors) / palette.size();
}

std::vector<int> GraphicsOptimizer::AnalyzeColorDistribution(
    const std::vector<uint8_t>& data) {
  std::vector<int> distribution(256, 0);

  for (uint8_t pixel : data) {
    distribution[pixel]++;
  }

  return distribution;
}

std::string GraphicsOptimizer::GenerateCacheKey(
    const std::vector<uint8_t>& data, int sheet_id) {
  std::ostringstream key;
  key << "sheet_" << sheet_id << "_" << data.size();

  // Add hash of data for uniqueness
  size_t hash = 0;
  for (size_t i = 0; i < std::min(data.size(), size_t(1024)); ++i) {
    hash = hash * 31 + data[i];
  }
  key << "_" << hash;

  return key.str();
}

void GraphicsOptimizer::UpdateOptimizationStats(const std::string& operation,
                                                double value) {
  optimization_stats_[operation] += value;
}

// GraphicsOptimizationScope implementation

GraphicsOptimizationScope::GraphicsOptimizationScope(
    OptimizationStrategy strategy, int sheet_count)
    : strategy_(strategy),
      sheet_count_(sheet_count),
      timer_("graphics_optimize_scope") {
  std::ostringstream op_name;
  op_name << "graphics_optimize_" << static_cast<int>(strategy) << "_"
          << sheet_count;
  operation_name_ = op_name.str();
}

GraphicsOptimizationScope::~GraphicsOptimizationScope() {
  // Timer automatically ends in destructor
}

void GraphicsOptimizationScope::AddSheet(int sheet_id, size_t original_size,
                                         size_t optimized_size) {
  result_.memory_saved += (original_size - optimized_size);
}

void GraphicsOptimizationScope::SetResult(const OptimizationResult& result) {
  result_ = result;
}

}  // namespace gfx
}  // namespace yaze
