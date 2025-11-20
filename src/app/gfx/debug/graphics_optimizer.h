#ifndef YAZE_APP_GFX_GRAPHICS_OPTIMIZER_H
#define YAZE_APP_GFX_GRAPHICS_OPTIMIZER_H

#include <string>
#include <unordered_map>
#include <vector>

#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/util/bpp_format_manager.h"

namespace yaze {
namespace gfx {

/**
 * @brief Graphics optimization strategy
 */
enum class OptimizationStrategy {
  kMemoryOptimized,       ///< Minimize memory usage
  kPerformanceOptimized,  ///< Maximize rendering performance
  kQualityOptimized,      ///< Maintain highest quality
  kBalanced               ///< Balance memory, performance, and quality
};

/**
 * @brief Graphics optimization result
 */
struct OptimizationResult {
  bool success;
  std::string message;
  size_t memory_saved;
  float performance_gain;
  float quality_loss;
  std::vector<BppFormat> recommended_formats;
  std::unordered_map<int, BppFormat> sheet_recommendations;

  OptimizationResult()
      : success(false),
        memory_saved(0),
        performance_gain(0.0f),
        quality_loss(0.0f) {}
};

/**
 * @brief Graphics sheet optimization data
 */
struct SheetOptimizationData {
  int sheet_id;
  BppFormat current_format;
  BppFormat recommended_format;
  size_t current_size;
  size_t optimized_size;
  float compression_ratio;
  int colors_used;
  bool is_convertible;
  std::string optimization_reason;

  SheetOptimizationData()
      : sheet_id(-1),
        current_format(BppFormat::kBpp8),
        recommended_format(BppFormat::kBpp8),
        current_size(0),
        optimized_size(0),
        compression_ratio(1.0f),
        colors_used(0),
        is_convertible(false) {}
};

/**
 * @brief Comprehensive graphics optimization system for YAZE ROM hacking
 * 
 * The GraphicsOptimizer provides intelligent optimization of graphics data
 * for Link to the Past ROM hacking workflows, balancing memory usage,
 * performance, and visual quality.
 * 
 * Key Features:
 * - Intelligent BPP format optimization based on actual color usage
 * - Graphics sheet analysis and conversion recommendations
 * - Memory usage optimization with quality preservation
 * - Performance optimization through atlas rendering
 * - Batch processing for multiple graphics sheets
 * - Quality analysis and loss estimation
 * 
 * Optimization Strategies:
 * - Memory Optimized: Minimize ROM size by using optimal BPP formats
 * - Performance Optimized: Maximize rendering speed through atlas optimization
 * - Quality Optimized: Preserve visual fidelity while optimizing
 * - Balanced: Optimal balance of memory, performance, and quality
 * 
 * ROM Hacking Specific:
 * - SNES-specific optimization patterns
 * - Graphics sheet format analysis and conversion tracking
 * - Palette optimization based on actual usage
 * - Integration with existing YAZE graphics pipeline
 */
class GraphicsOptimizer {
 public:
  static GraphicsOptimizer& Get();

  /**
   * @brief Initialize the graphics optimizer
   */
  void Initialize();

  /**
   * @brief Optimize a single graphics sheet
   * @param sheet_data Graphics sheet data
   * @param sheet_id Sheet identifier
   * @param palette Sheet palette
   * @param strategy Optimization strategy
   * @return Optimization result
   */
  OptimizationResult OptimizeSheet(
      const std::vector<uint8_t>& sheet_data, int sheet_id,
      const SnesPalette& palette,
      OptimizationStrategy strategy = OptimizationStrategy::kBalanced);

  /**
   * @brief Optimize multiple graphics sheets
   * @param sheets Map of sheet ID to sheet data
   * @param palettes Map of sheet ID to palette
   * @param strategy Optimization strategy
   * @return Optimization result
   */
  OptimizationResult OptimizeSheets(
      const std::unordered_map<int, std::vector<uint8_t>>& sheets,
      const std::unordered_map<int, SnesPalette>& palettes,
      OptimizationStrategy strategy = OptimizationStrategy::kBalanced);

  /**
   * @brief Analyze graphics sheet for optimization opportunities
   * @param sheet_data Graphics sheet data
   * @param sheet_id Sheet identifier
   * @param palette Sheet palette
   * @return Optimization data
   */
  SheetOptimizationData AnalyzeSheet(const std::vector<uint8_t>& sheet_data,
                                     int sheet_id, const SnesPalette& palette);

  /**
   * @brief Get optimization recommendations for all sheets
   * @param sheets Map of sheet ID to sheet data
   * @param palettes Map of sheet ID to palette
   * @return Map of sheet ID to optimization data
   */
  std::unordered_map<int, SheetOptimizationData> GetOptimizationRecommendations(
      const std::unordered_map<int, std::vector<uint8_t>>& sheets,
      const std::unordered_map<int, SnesPalette>& palettes);

  /**
   * @brief Apply optimization recommendations
   * @param recommendations Optimization recommendations
   * @param sheets Map of sheet ID to sheet data (modified in place)
   * @param palettes Map of sheet ID to palette (modified in place)
   * @return Optimization result
   */
  OptimizationResult ApplyOptimizations(
      const std::unordered_map<int, SheetOptimizationData>& recommendations,
      std::unordered_map<int, std::vector<uint8_t>>& sheets,
      std::unordered_map<int, SnesPalette>& palettes);

  /**
   * @brief Get optimization statistics
   * @return Map of optimization statistics
   */
  std::unordered_map<std::string, double> GetOptimizationStats() const;

  /**
   * @brief Clear optimization cache
   */
  void ClearCache();

  /**
   * @brief Set optimization parameters
   * @param max_quality_loss Maximum acceptable quality loss (0.0-1.0)
   * @param min_memory_savings Minimum required memory savings (bytes)
   * @param performance_threshold Minimum performance gain threshold
   */
  void SetOptimizationParameters(float max_quality_loss = 0.1f,
                                 size_t min_memory_savings = 1024,
                                 float performance_threshold = 0.05f);

 private:
  GraphicsOptimizer() = default;
  ~GraphicsOptimizer() = default;

  // Optimization parameters
  float max_quality_loss_;
  size_t min_memory_savings_;
  float performance_threshold_;

  // Statistics tracking
  std::unordered_map<std::string, double> optimization_stats_;

  // Cache for optimization results
  std::unordered_map<std::string, SheetOptimizationData> optimization_cache_;

  // Helper methods
  BppFormat DetermineOptimalFormat(const std::vector<uint8_t>& data,
                                   const SnesPalette& palette,
                                   OptimizationStrategy strategy);
  float CalculateQualityLoss(BppFormat from_format, BppFormat to_format,
                             const std::vector<uint8_t>& data);
  size_t CalculateMemorySavings(BppFormat from_format, BppFormat to_format,
                                const std::vector<uint8_t>& data);
  float CalculatePerformanceGain(BppFormat from_format, BppFormat to_format);
  bool ShouldOptimize(const SheetOptimizationData& data,
                      OptimizationStrategy strategy);
  std::string GenerateOptimizationReason(const SheetOptimizationData& data);

  // Analysis helpers
  int CountUsedColors(const std::vector<uint8_t>& data,
                      const SnesPalette& palette);
  float CalculateColorEfficiency(const std::vector<uint8_t>& data,
                                 const SnesPalette& palette);
  std::vector<int> AnalyzeColorDistribution(const std::vector<uint8_t>& data);

  // Cache management
  std::string GenerateCacheKey(const std::vector<uint8_t>& data, int sheet_id);
  void UpdateOptimizationStats(const std::string& operation, double value);
};

/**
 * @brief RAII wrapper for graphics optimization operations
 */
class GraphicsOptimizationScope {
 public:
  GraphicsOptimizationScope(OptimizationStrategy strategy, int sheet_count);
  ~GraphicsOptimizationScope();

  void AddSheet(int sheet_id, size_t original_size, size_t optimized_size);
  void SetResult(const OptimizationResult& result);

 private:
  OptimizationStrategy strategy_;
  int sheet_count_;
  std::string operation_name_;
  ScopedTimer timer_;
  OptimizationResult result_;
};

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_APP_GFX_GRAPHICS_OPTIMIZER_H
