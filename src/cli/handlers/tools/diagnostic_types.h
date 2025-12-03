#ifndef YAZE_CLI_HANDLERS_TOOLS_DIAGNOSTIC_TYPES_H
#define YAZE_CLI_HANDLERS_TOOLS_DIAGNOSTIC_TYPES_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"

namespace yaze::cli {

/**
 * @brief Severity level for diagnostic findings
 */
enum class DiagnosticSeverity {
  kInfo,      // Informational, no action needed
  kWarning,   // Potential issue, may need attention
  kError,     // Problem detected, should be fixed
  kCritical   // Severe issue, requires immediate attention
};

/**
 * @brief Convert severity to string for output
 */
inline std::string SeverityToString(DiagnosticSeverity severity) {
  switch (severity) {
    case DiagnosticSeverity::kInfo: return "info";
    case DiagnosticSeverity::kWarning: return "warning";
    case DiagnosticSeverity::kError: return "error";
    case DiagnosticSeverity::kCritical: return "critical";
  }
  return "unknown";
}

/**
 * @brief A single diagnostic finding
 */
struct DiagnosticFinding {
  std::string id;               // Unique identifier, e.g., "tile16_corruption"
  DiagnosticSeverity severity;
  std::string message;          // Human-readable description
  std::string location;         // Address or location, e.g., "0x1E878B"
  std::string suggested_action; // What to do about it
  bool fixable = false;         // Can this be auto-fixed?
  
  /**
   * @brief Format finding for text output
   */
  std::string FormatText() const {
    std::string prefix;
    switch (severity) {
      case DiagnosticSeverity::kInfo: prefix = "[INFO]"; break;
      case DiagnosticSeverity::kWarning: prefix = "[WARN]"; break;
      case DiagnosticSeverity::kError: prefix = "[ERROR]"; break;
      case DiagnosticSeverity::kCritical: prefix = "[CRITICAL]"; break;
    }
    std::string result = absl::StrFormat("%s %s", prefix, message);
    if (!location.empty()) {
      result += absl::StrFormat(" at %s", location);
    }
    if (fixable) {
      result += " [fixable]";
    }
    return result;
  }
  
  /**
   * @brief Format finding as JSON object string
   */
  std::string FormatJson() const {
    return absl::StrFormat(
        R"({"id":"%s","severity":"%s","message":"%s","location":"%s","suggested_action":"%s","fixable":%s})",
        id, SeverityToString(severity), message, location, suggested_action,
        fixable ? "true" : "false");
  }
};

/**
 * @brief ROM feature detection results
 */
struct RomFeatures {
  // Version info
  uint8_t zs_custom_version = 0xFF;  // 0xFF = vanilla, 2 = v2, 3 = v3
  bool is_vanilla = true;
  bool is_v2 = false;
  bool is_v3 = false;
  
  // Expanded data flags
  bool has_expanded_tile16 = false;
  bool has_expanded_tile32 = false;
  bool has_expanded_pointer_tables = false;  // Requires ASM patch
  
  // ZSCustomOverworld features (ROM-level enable flags)
  bool custom_bg_enabled = false;
  bool custom_main_palette_enabled = false;
  bool custom_mosaic_enabled = false;
  bool custom_animated_gfx_enabled = false;
  bool custom_overlay_enabled = false;
  bool custom_tile_gfx_enabled = false;
  
  /**
   * @brief Get version as human-readable string
   */
  std::string GetVersionString() const {
    if (is_vanilla) return "Vanilla";
    if (is_v2) return "ZSCustomOverworld v2";
    if (is_v3) return "ZSCustomOverworld v3";
    return absl::StrFormat("Unknown (0x%02X)", zs_custom_version);
  }
};

/**
 * @brief Map pointer validation status
 */
struct MapPointerStatus {
  bool lw_dw_maps_valid = true;   // Maps 0x00-0x7F
  bool sw_maps_valid = true;      // Maps 0x80-0x9F
  bool tail_maps_valid = false;   // Maps 0xA0-0xBF (requires expansion)
  int invalid_map_count = 0;
  bool can_support_tail = false;  // True only if expanded pointer tables exist
};

/**
 * @brief Tile16 corruption status
 */
struct Tile16Status {
  bool uses_expanded = false;
  bool corruption_detected = false;
  std::vector<uint32_t> corrupted_addresses;
  int corrupted_tile_count = 0;
};

/**
 * @brief Complete diagnostic report
 */
struct DiagnosticReport {
  std::string rom_path;
  RomFeatures features;
  MapPointerStatus map_status;
  Tile16Status tile16_status;
  std::vector<DiagnosticFinding> findings;
  
  // Summary counts
  int info_count = 0;
  int warning_count = 0;
  int error_count = 0;
  int critical_count = 0;
  int fixable_count = 0;
  
  /**
   * @brief Add a finding and update counts
   */
  void AddFinding(const DiagnosticFinding& finding) {
    findings.push_back(finding);
    switch (finding.severity) {
      case DiagnosticSeverity::kInfo: info_count++; break;
      case DiagnosticSeverity::kWarning: warning_count++; break;
      case DiagnosticSeverity::kError: error_count++; break;
      case DiagnosticSeverity::kCritical: critical_count++; break;
    }
    if (finding.fixable) fixable_count++;
  }
  
  /**
   * @brief Check if report has any critical or error findings
   */
  bool HasProblems() const {
    return critical_count > 0 || error_count > 0;
  }
  
  /**
   * @brief Check if report has any fixable findings
   */
  bool HasFixable() const {
    return fixable_count > 0;
  }
  
  /**
   * @brief Get total finding count
   */
  int TotalFindings() const {
    return static_cast<int>(findings.size());
  }
};

/**
 * @brief Entity distribution statistics for coverage analysis
 */
struct MapDistributionStats {
  std::map<uint16_t, int> counts;
  int total = 0;
  int unique = 0;
  int invalid = 0;
  uint16_t most_common_map = 0;
  int most_common_count = 0;
};

/**
 * @brief ROM comparison result for baseline comparisons
 */
struct RomCompareResult {
  struct RomInfo {
    std::string filename;
    size_t size = 0;
    uint8_t zs_version = 0xFF;
    bool has_expanded_tile16 = false;
    bool has_expanded_tile32 = false;
    uint32_t checksum = 0;
  };
  
  struct DiffRegion {
    uint32_t start;
    uint32_t end;
    size_t diff_count;
    std::string region_name;
    bool critical;
  };
  
  RomInfo target;
  RomInfo baseline;
  bool sizes_match = false;
  bool versions_match = false;
  bool features_match = false;
  std::vector<DiffRegion> diff_regions;
  size_t total_diff_bytes = 0;
};

// =============================================================================
// ROM Layout Constants (shared across diagnostic commands)
// =============================================================================

// ROM header locations (LoROM)
constexpr uint32_t kSnesHeaderBase = 0x7FC0;
constexpr uint32_t kChecksumComplementPos = 0x7FDC;
constexpr uint32_t kChecksumPos = 0x7FDE;

// Tile16 expanded region
constexpr uint32_t kMap16TilesExpanded = 0x1E8000;
constexpr uint32_t kMap16TilesExpandedEnd = 0x1F0000;
constexpr uint32_t kMap16ExpandedFlagPos = 0x02FD28;
constexpr uint32_t kMap32ExpandedFlagPos = 0x01772E;
constexpr int kNumTile16Vanilla = 3752;
constexpr int kNumTile16Expanded = 4096;

// Pointer table layout (vanilla - 160 entries only!)
// CRITICAL: These tables only cover maps 0x00-0x9F (160 maps)
// Maps 0xA0-0xBF do NOT have pointer table entries without ASM expansion
constexpr uint32_t kPtrTableLowBase = 0x1794D;   // 160 entries × 3 bytes = 0x1E0
constexpr uint32_t kPtrTableHighBase = 0x17B2D;  // Starts right after low table
constexpr int kVanillaMapCount = 160;            // 0x00-0x9F only

// ZSCustomOverworld version markers
constexpr uint32_t kZSCustomVersionPos = 0x140145;

// ZSCustomOverworld feature enable flags
constexpr uint32_t kCustomBGEnabledPos = 0x140141;
constexpr uint32_t kCustomMainPalettePos = 0x140142;
constexpr uint32_t kCustomMosaicPos = 0x140143;
constexpr uint32_t kCustomAnimatedGFXPos = 0x140146;
constexpr uint32_t kCustomOverlayPos = 0x140147;
constexpr uint32_t kCustomTileGFXPos = 0x140148;

// ASM expansion markers for tail map support
// When TailMapExpansion.asm patch is applied, these locations will be set:
// - Marker byte at 0x1423FF ($28:A3FF) = 0xEA to indicate expansion
// - New High table at 0x142400 ($28:A400) with 192 entries
// - New Low table at 0x142640 ($28:A640) with 192 entries
constexpr uint32_t kExpandedPtrTableMarker = 0x1423FF;  // Marker location
constexpr uint8_t kExpandedPtrTableMagic = 0xEA;        // Marker value
constexpr uint32_t kExpandedPtrTableHigh = 0x142400;    // New high table
constexpr uint32_t kExpandedPtrTableLow = 0x142640;     // New low table
constexpr int kExpandedMapCount = 192;                  // 0x00-0xBF

// Known problematic addresses in tile16 region (from previous corruption)
const uint32_t kProblemAddresses[] = {
    0x1E878B, 0x1E95A3, 0x1ED6F3, 0x1EF540
};

}  // namespace yaze::cli

#endif  // YAZE_CLI_HANDLERS_TOOLS_DIAGNOSTIC_TYPES_H

