#ifndef YAZE_APP_GFX_BPP_FORMAT_MANAGER_H
#define YAZE_APP_GFX_BPP_FORMAT_MANAGER_H

#include <SDL.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "app/gfx/core/bitmap.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/types/snes_palette.h"

namespace yaze {
namespace gfx {

/**
 * @brief BPP format enumeration for SNES graphics
 */
enum class BppFormat {
  kBpp2 = 2,  ///< 2 bits per pixel (4 colors)
  kBpp3 = 3,  ///< 3 bits per pixel (8 colors)
  kBpp4 = 4,  ///< 4 bits per pixel (16 colors)
  kBpp8 = 8   ///< 8 bits per pixel (256 colors)
};

/**
 * @brief BPP format metadata and conversion information
 */
struct BppFormatInfo {
  BppFormat format;
  std::string name;
  int bits_per_pixel;
  int max_colors;
  int bytes_per_tile;
  int bytes_per_sheet;
  bool is_compressed;
  std::string description;

  BppFormatInfo() = default;

  BppFormatInfo(BppFormat fmt, const std::string& n, int bpp, int max_col,
                int bytes_tile, int bytes_sheet, bool compressed,
                const std::string& desc)
      : format(fmt),
        name(n),
        bits_per_pixel(bpp),
        max_colors(max_col),
        bytes_per_tile(bytes_tile),
        bytes_per_sheet(bytes_sheet),
        is_compressed(compressed),
        description(desc) {}
};

/**
 * @brief Graphics sheet analysis result
 */
struct GraphicsSheetAnalysis {
  int sheet_id;
  BppFormat original_format;
  BppFormat current_format;
  bool was_converted;
  std::string conversion_history;
  int palette_entries_used;
  float compression_ratio;
  size_t original_size;
  size_t current_size;
  std::vector<int> tile_usage_pattern;

  GraphicsSheetAnalysis()
      : sheet_id(-1),
        original_format(BppFormat::kBpp8),
        current_format(BppFormat::kBpp8),
        was_converted(false),
        palette_entries_used(0),
        compression_ratio(1.0f),
        original_size(0),
        current_size(0) {}
};

/**
 * @brief Comprehensive BPP format management system for SNES ROM hacking
 *
 * The BppFormatManager provides advanced BPP format handling, conversion,
 * and analysis capabilities specifically designed for Link to the Past
 * ROM hacking workflows.
 *
 * Key Features:
 * - Multi-format BPP support (2BPP, 3BPP, 4BPP, 8BPP)
 * - Intelligent format detection and analysis
 * - High-performance format conversion with caching
 * - Graphics sheet analysis and conversion history tracking
 * - Palette depth analysis and optimization
 * - Memory-efficient conversion algorithms
 *
 * Performance Optimizations:
 * - Cached conversion results to avoid redundant processing
 * - SIMD-optimized conversion algorithms where possible
 * - Lazy evaluation for expensive analysis operations
 * - Memory pool integration for efficient temporary allocations
 *
 * ROM Hacking Specific:
 * - SNES-specific BPP format handling
 * - Graphics sheet format analysis and conversion tracking
 * - Palette optimization based on actual color usage
 * - Integration with existing YAZE graphics pipeline
 */
class BppFormatManager {
 public:
  static BppFormatManager& Get();

  /**
   * @brief Initialize the BPP format manager
   */
  void Initialize();

  /**
   * @brief Get BPP format information
   * @param format BPP format to get info for
   * @return Format information structure
   */
  const BppFormatInfo& GetFormatInfo(BppFormat format) const;

  /**
   * @brief Get all available BPP formats
   * @return Vector of all supported BPP formats
   */
  std::vector<BppFormat> GetAvailableFormats() const;

  /**
   * @brief Convert bitmap data between BPP formats
   * @param data Source bitmap data
   * @param from_format Source BPP format
   * @param to_format Target BPP format
   * @param width Bitmap width
   * @param height Bitmap height
   * @return Converted bitmap data
   */
  std::vector<uint8_t> ConvertFormat(const std::vector<uint8_t>& data,
                                     BppFormat from_format, BppFormat to_format,
                                     int width, int height);

  /**
   * @brief Analyze graphics sheet to determine original and current BPP formats
   * @param sheet_data Graphics sheet data
   * @param sheet_id Sheet identifier
   * @param palette Palette data for analysis
   * @return Analysis result with format information
   */
  GraphicsSheetAnalysis AnalyzeGraphicsSheet(
      const std::vector<uint8_t>& sheet_data, int sheet_id,
      const SnesPalette& palette);

  /**
   * @brief Detect BPP format from bitmap data
   * @param data Bitmap data to analyze
   * @param width Bitmap width
   * @param height Bitmap height
   * @return Detected BPP format
   */
  BppFormat DetectFormat(const std::vector<uint8_t>& data, int width,
                         int height);

  /**
   * @brief Optimize palette for specific BPP format
   * @param palette Source palette
   * @param target_format Target BPP format
   * @param used_colors Vector of actually used color indices
   * @return Optimized palette
   */
  SnesPalette OptimizePaletteForFormat(const SnesPalette& palette,
                                       BppFormat target_format,
                                       const std::vector<int>& used_colors);

  /**
   * @brief Get conversion statistics
   * @return Map of conversion operation statistics
   */
  std::unordered_map<std::string, int> GetConversionStats() const;

  /**
   * @brief Clear conversion cache
   */
  void ClearCache();

  /**
   * @brief Get memory usage statistics
   * @return Memory usage information
   */
  std::pair<size_t, size_t> GetMemoryStats() const;

 private:
  BppFormatManager() = default;
  ~BppFormatManager() = default;

  // Format information storage
  std::unordered_map<BppFormat, BppFormatInfo> format_info_;

  // Conversion cache for performance
  std::unordered_map<std::string, std::vector<uint8_t>> conversion_cache_;

  // Analysis cache
  std::unordered_map<int, GraphicsSheetAnalysis> analysis_cache_;

  // Statistics tracking
  std::unordered_map<std::string, int> conversion_stats_;

  // Memory usage tracking
  size_t cache_memory_usage_;
  size_t max_cache_size_;

  // Helper methods
  void InitializeFormatInfo();
  std::string GenerateCacheKey(const std::vector<uint8_t>& data,
                               BppFormat from_format, BppFormat to_format,
                               int width, int height);
  BppFormat AnalyzeColorDepth(const std::vector<uint8_t>& data, int width,
                              int height);
  std::vector<uint8_t> Convert2BppTo8Bpp(const std::vector<uint8_t>& data,
                                         int width, int height);
  std::vector<uint8_t> Convert3BppTo8Bpp(const std::vector<uint8_t>& data,
                                         int width, int height);
  std::vector<uint8_t> Convert4BppTo8Bpp(const std::vector<uint8_t>& data,
                                         int width, int height);
  std::vector<uint8_t> Convert8BppTo2Bpp(const std::vector<uint8_t>& data,
                                         int width, int height);
  std::vector<uint8_t> Convert8BppTo3Bpp(const std::vector<uint8_t>& data,
                                         int width, int height);
  std::vector<uint8_t> Convert8BppTo4Bpp(const std::vector<uint8_t>& data,
                                         int width, int height);

  // Analysis helpers
  int CountUsedColors(const std::vector<uint8_t>& data, int max_colors);
  float CalculateCompressionRatio(const std::vector<uint8_t>& original,
                                  const std::vector<uint8_t>& compressed);
  std::vector<int> AnalyzeTileUsagePattern(const std::vector<uint8_t>& data,
                                           int width, int height,
                                           int tile_size);
};

/**
 * @brief RAII wrapper for BPP format conversion operations
 */
class BppConversionScope {
 public:
  BppConversionScope(BppFormat from_format, BppFormat to_format, int width,
                     int height);
  ~BppConversionScope();

  std::vector<uint8_t> Convert(const std::vector<uint8_t>& data);

 private:
  BppFormat from_format_;
  BppFormat to_format_;
  int width_;
  int height_;
  std::string operation_name_;
  ScopedTimer timer_;
};

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_APP_GFX_BPP_FORMAT_MANAGER_H
