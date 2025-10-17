#include "app/gfx/util/bpp_format_manager.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>

#include "app/gfx/resource/memory_pool.h"
#include "util/log.h"

namespace yaze {
namespace gfx {

BppFormatManager& BppFormatManager::Get() {
  static BppFormatManager instance;
  return instance;
}

void BppFormatManager::Initialize() {
  InitializeFormatInfo();
  cache_memory_usage_ = 0;
  max_cache_size_ = 64 * 1024 * 1024; // 64MB cache limit
}

void BppFormatManager::InitializeFormatInfo() {
  format_info_[BppFormat::kBpp2] = BppFormatInfo(
    BppFormat::kBpp2, "2BPP", 2, 4, 16, 2048, true,
    "2 bits per pixel - 4 colors, used for simple graphics and UI elements"
  );
  
  format_info_[BppFormat::kBpp3] = BppFormatInfo(
    BppFormat::kBpp3, "3BPP", 3, 8, 24, 3072, true,
    "3 bits per pixel - 8 colors, common for SNES sprites and tiles"
  );
  
  format_info_[BppFormat::kBpp4] = BppFormatInfo(
    BppFormat::kBpp4, "4BPP", 4, 16, 32, 4096, true,
    "4 bits per pixel - 16 colors, standard for SNES backgrounds"
  );
  
  format_info_[BppFormat::kBpp8] = BppFormatInfo(
    BppFormat::kBpp8, "8BPP", 8, 256, 64, 8192, false,
    "8 bits per pixel - 256 colors, high-color graphics and converted formats"
  );
}

const BppFormatInfo& BppFormatManager::GetFormatInfo(BppFormat format) const {
  auto it = format_info_.find(format);
  if (it == format_info_.end()) {
    throw std::invalid_argument("Unknown BPP format");
  }
  return it->second;
}

std::vector<BppFormat> BppFormatManager::GetAvailableFormats() const {
  return {BppFormat::kBpp2, BppFormat::kBpp3, BppFormat::kBpp4, BppFormat::kBpp8};
}

std::vector<uint8_t> BppFormatManager::ConvertFormat(const std::vector<uint8_t>& data,
                                                    BppFormat from_format, BppFormat to_format,
                                                    int width, int height) {
  if (from_format == to_format) {
    return data; // No conversion needed
  }
  
  ScopedTimer timer("bpp_format_conversion");
  
  // Check cache first
  std::string cache_key = GenerateCacheKey(data, from_format, to_format, width, height);
  auto cache_iter = conversion_cache_.find(cache_key);
  if (cache_iter != conversion_cache_.end()) {
    conversion_stats_["cache_hits"]++;
    return cache_iter->second;
  }
  
  std::vector<uint8_t> result;
  
  // Convert to 8BPP as intermediate format if needed
  std::vector<uint8_t> intermediate_data = data;
  if (from_format != BppFormat::kBpp8) {
    switch (from_format) {
      case BppFormat::kBpp2:
        intermediate_data = Convert2BppTo8Bpp(data, width, height);
        break;
      case BppFormat::kBpp3:
        intermediate_data = Convert3BppTo8Bpp(data, width, height);
        break;
      case BppFormat::kBpp4:
        intermediate_data = Convert4BppTo8Bpp(data, width, height);
        break;
      default:
        intermediate_data = data;
        break;
    }
  }
  
  // Convert from 8BPP to target format
  if (to_format != BppFormat::kBpp8) {
    switch (to_format) {
      case BppFormat::kBpp2:
        result = Convert8BppTo2Bpp(intermediate_data, width, height);
        break;
      case BppFormat::kBpp3:
        result = Convert8BppTo3Bpp(intermediate_data, width, height);
        break;
      case BppFormat::kBpp4:
        result = Convert8BppTo4Bpp(intermediate_data, width, height);
        break;
      default:
        result = intermediate_data;
        break;
    }
  } else {
    result = intermediate_data;
  }
  
  // Cache the result
  if (cache_memory_usage_ + result.size() < max_cache_size_) {
    conversion_cache_[cache_key] = result;
    cache_memory_usage_ += result.size();
  }
  
  conversion_stats_["conversions"]++;
  conversion_stats_["cache_misses"]++;
  
  return result;
}

GraphicsSheetAnalysis BppFormatManager::AnalyzeGraphicsSheet(const std::vector<uint8_t>& sheet_data,
                                                            int sheet_id,
                                                            const SnesPalette& palette) {
  // Check analysis cache
  auto cache_it = analysis_cache_.find(sheet_id);
  if (cache_it != analysis_cache_.end()) {
    return cache_it->second;
  }
  
  ScopedTimer timer("graphics_sheet_analysis");
  
  GraphicsSheetAnalysis analysis;
  analysis.sheet_id = sheet_id;
  analysis.original_size = sheet_data.size();
  analysis.current_size = sheet_data.size();
  
  // Detect current format
  analysis.current_format = DetectFormat(sheet_data, 128, 32); // Standard sheet size
  
  // Analyze color usage
  analysis.palette_entries_used = CountUsedColors(sheet_data, palette.size());
  
  // Determine if this was likely converted from a lower BPP format
  if (analysis.current_format == BppFormat::kBpp8 && analysis.palette_entries_used <= 16) {
    if (analysis.palette_entries_used <= 4) {
      analysis.original_format = BppFormat::kBpp2;
    } else if (analysis.palette_entries_used <= 8) {
      analysis.original_format = BppFormat::kBpp3;
    } else {
      analysis.original_format = BppFormat::kBpp4;
    }
    analysis.was_converted = true;
  } else {
    analysis.original_format = analysis.current_format;
    analysis.was_converted = false;
  }
  
  // Generate conversion history
  if (analysis.was_converted) {
    std::ostringstream history;
    history << "Originally " << GetFormatInfo(analysis.original_format).name
            << " (" << analysis.palette_entries_used << " colors used)";
    history << " -> Converted to " << GetFormatInfo(analysis.current_format).name;
    analysis.conversion_history = history.str();
  } else {
    analysis.conversion_history = "No conversion - original format";
  }
  
  // Analyze tile usage pattern
  analysis.tile_usage_pattern = AnalyzeTileUsagePattern(sheet_data, 128, 32, 8);
  
  // Calculate compression ratio (simplified)
  analysis.compression_ratio = 1.0f; // Would need original compressed data for accurate calculation
  
  // Cache the analysis
  analysis_cache_[sheet_id] = analysis;
  
  return analysis;
}

BppFormat BppFormatManager::DetectFormat(const std::vector<uint8_t>& data, int width, int height) {
  if (data.empty()) {
    return BppFormat::kBpp8; // Default
  }
  
  // Analyze color depth
  return AnalyzeColorDepth(data, width, height);
}

SnesPalette BppFormatManager::OptimizePaletteForFormat(const SnesPalette& palette,
                                                      BppFormat target_format,
                                                      const std::vector<int>& used_colors) {
  const auto& format_info = GetFormatInfo(target_format);
  
  // Create optimized palette with target format size
  SnesPalette optimized_palette;
  
  // Add used colors first
  for (int color_index : used_colors) {
    if (color_index < static_cast<int>(palette.size()) && 
        static_cast<int>(optimized_palette.size()) < format_info.max_colors) {
      optimized_palette.AddColor(palette[color_index]);
    }
  }
  
  // Fill remaining slots with unused colors or transparent
  while (static_cast<int>(optimized_palette.size()) < format_info.max_colors) {
    if (static_cast<int>(optimized_palette.size()) < static_cast<int>(palette.size())) {
      optimized_palette.AddColor(palette[optimized_palette.size()]);
    } else {
      // Add transparent color
      optimized_palette.AddColor(SnesColor(ImVec4(0, 0, 0, 0)));
    }
  }
  
  return optimized_palette;
}

std::unordered_map<std::string, int> BppFormatManager::GetConversionStats() const {
  return conversion_stats_;
}

void BppFormatManager::ClearCache() {
  conversion_cache_.clear();
  analysis_cache_.clear();
  cache_memory_usage_ = 0;
  conversion_stats_.clear();
}

std::pair<size_t, size_t> BppFormatManager::GetMemoryStats() const {
  return {cache_memory_usage_, max_cache_size_};
}

// Helper method implementations

std::string BppFormatManager::GenerateCacheKey(const std::vector<uint8_t>& data, 
                                              BppFormat from_format, BppFormat to_format,
                                              int width, int height) {
  std::ostringstream key;
  key << static_cast<int>(from_format) << "_" << static_cast<int>(to_format) 
      << "_" << width << "x" << height << "_" << data.size();
  
  // Add hash of data for uniqueness
  size_t hash = 0;
  for (size_t i = 0; i < std::min(data.size(), size_t(1024)); ++i) {
    hash = hash * 31 + data[i];
  }
  key << "_" << hash;
  
  return key.str();
}

BppFormat BppFormatManager::AnalyzeColorDepth(const std::vector<uint8_t>& data, int /*width*/, int /*height*/) {
  if (data.empty()) {
    return BppFormat::kBpp8;
  }
  
  // Find maximum color index used
  uint8_t max_color = 0;
  for (uint8_t pixel : data) {
    max_color = std::max(max_color, pixel);
  }
  
  // Determine BPP based on color usage
  if (max_color < 4) {
    return BppFormat::kBpp2;
  }
  if (max_color < 8) {
    return BppFormat::kBpp3;
  }
  if (max_color < 16) {
    return BppFormat::kBpp4;
  }
  return BppFormat::kBpp8;
}

std::vector<uint8_t> BppFormatManager::Convert2BppTo8Bpp(const std::vector<uint8_t>& data, int width, int height) {
  std::vector<uint8_t> result(width * height);
  
  for (int row = 0; row < height; ++row) {
    for (int col = 0; col < width; col += 4) { // 4 pixels per byte in 2BPP
      if (col / 4 < static_cast<int>(data.size())) {
        uint8_t byte = data[row * (width / 4) + (col / 4)];
        
        // Extract 4 pixels from the byte
        for (int i = 0; i < 4 && (col + i) < width; ++i) {
          uint8_t pixel = (byte >> (6 - i * 2)) & 0x03;
          result[row * width + col + i] = pixel;
        }
      }
    }
  }
  
  return result;
}

std::vector<uint8_t> BppFormatManager::Convert3BppTo8Bpp(const std::vector<uint8_t>& data, int width, int height) {
  // 3BPP is more complex - typically stored as 4BPP with unused bits
  return Convert4BppTo8Bpp(data, width, height);
}

std::vector<uint8_t> BppFormatManager::Convert4BppTo8Bpp(const std::vector<uint8_t>& data, int width, int height) {
  std::vector<uint8_t> result(width * height);
  
  for (int row = 0; row < height; ++row) {
    for (int col = 0; col < width; col += 2) { // 2 pixels per byte in 4BPP
      if (col / 2 < static_cast<int>(data.size())) {
        uint8_t byte = data[row * (width / 2) + (col / 2)];
        
        // Extract 2 pixels from the byte
        uint8_t pixel1 = byte & 0x0F;
        uint8_t pixel2 = (byte >> 4) & 0x0F;
        
        result[row * width + col] = pixel1;
        if (col + 1 < width) {
          result[row * width + col + 1] = pixel2;
        }
      }
    }
  }
  
  return result;
}

std::vector<uint8_t> BppFormatManager::Convert8BppTo2Bpp(const std::vector<uint8_t>& data, int width, int height) {
  std::vector<uint8_t> result((width * height) / 4); // 4 pixels per byte
  
  for (int row = 0; row < height; ++row) {
    for (int col = 0; col < width; col += 4) {
      uint8_t byte = 0;
      
      // Pack 4 pixels into one byte
      for (int i = 0; i < 4 && (col + i) < width; ++i) {
        uint8_t pixel = data[row * width + col + i] & 0x03; // Clamp to 2 bits
        byte |= (pixel << (6 - i * 2));
      }
      
      result[row * (width / 4) + (col / 4)] = byte;
    }
  }
  
  return result;
}

std::vector<uint8_t> BppFormatManager::Convert8BppTo3Bpp(const std::vector<uint8_t>& data, int width, int height) {
  // Convert to 4BPP first, then optimize
  auto result_4bpp = Convert8BppTo4Bpp(data, width, height);
  // Note: 3BPP conversion would require more sophisticated palette optimization
  return result_4bpp;
}

std::vector<uint8_t> BppFormatManager::Convert8BppTo4Bpp(const std::vector<uint8_t>& data, int width, int height) {
  std::vector<uint8_t> result((width * height) / 2); // 2 pixels per byte
  
  for (int row = 0; row < height; ++row) {
    for (int col = 0; col < width; col += 2) {
      uint8_t pixel1 = data[row * width + col] & 0x0F; // Clamp to 4 bits
      uint8_t pixel2 = (col + 1 < width) ? (data[row * width + col + 1] & 0x0F) : 0;
      
      uint8_t byte = pixel1 | (pixel2 << 4);
      result[row * (width / 2) + (col / 2)] = byte;
    }
  }
  
  return result;
}

int BppFormatManager::CountUsedColors(const std::vector<uint8_t>& data, int max_colors) {
  std::vector<bool> used_colors(max_colors, false);
  
  for (uint8_t pixel : data) {
    if (pixel < max_colors) {
      used_colors[pixel] = true;
    }
  }
  
  int count = 0;
  for (bool used : used_colors) {
    if (used) count++;
  }
  
  return count;
}

float BppFormatManager::CalculateCompressionRatio(const std::vector<uint8_t>& original, 
                                                 const std::vector<uint8_t>& compressed) {
  if (compressed.empty()) return 1.0f;
  return static_cast<float>(original.size()) / static_cast<float>(compressed.size());
}

std::vector<int> BppFormatManager::AnalyzeTileUsagePattern(const std::vector<uint8_t>& data, 
                                                          int width, int height, int tile_size) {
  std::vector<int> usage_pattern;
  int tiles_x = width / tile_size;
  int tiles_y = height / tile_size;
  
  for (int tile_row = 0; tile_row < tiles_y; ++tile_row) {
    for (int tile_col = 0; tile_col < tiles_x; ++tile_col) {
      int non_zero_pixels = 0;
      
      // Count non-zero pixels in this tile
      for (int row = 0; row < tile_size; ++row) {
        for (int col = 0; col < tile_size; ++col) {
          int pixel_x = tile_col * tile_size + col;
          int pixel_y = tile_row * tile_size + row;
          int pixel_index = pixel_y * width + pixel_x;
          
          if (pixel_index < static_cast<int>(data.size()) && data[pixel_index] != 0) {
            non_zero_pixels++;
          }
        }
      }
      
      usage_pattern.push_back(non_zero_pixels);
    }
  }
  
  return usage_pattern;
}

// BppConversionScope implementation

BppConversionScope::BppConversionScope(BppFormat from_format, BppFormat to_format, 
                                      int width, int height)
    : from_format_(from_format), to_format_(to_format), width_(width), height_(height),
      timer_("bpp_convert_scope") {
  std::ostringstream op_name;
  op_name << "bpp_convert_" << static_cast<int>(from_format) 
          << "_to_" << static_cast<int>(to_format);
  operation_name_ = op_name.str();
}

BppConversionScope::~BppConversionScope() {
  // Timer automatically ends in destructor
}

std::vector<uint8_t> BppConversionScope::Convert(const std::vector<uint8_t>& data) {
  return BppFormatManager::Get().ConvertFormat(data, from_format_, to_format_, width_, height_);
}

}  // namespace gfx
}  // namespace yaze
