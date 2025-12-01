#include "cli/handlers/tools/rom_compare_commands.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/rom.h"
#include "util/log.h"

namespace yaze::cli {

namespace {

// =============================================================================
// ROM Constants for Comparison
// =============================================================================

// Critical data regions
struct RomRegion {
  const char* name;
  uint32_t start;
  uint32_t end;
  bool critical;  // If different, indicates potential corruption
};

const RomRegion kCriticalRegions[] = {
    // Map pointer tables (CRITICAL - must match for LW/DW/SW)
    {"Map32 Ptr Low", 0x1794D, 0x17B2D, true},
    {"Map32 Ptr High", 0x17B2D, 0x17D0D, true},
    
    // Tile data regions
    {"Tile16 Vanilla", 0x78000, 0x78000 + (3752 * 8), false},
    {"Tile16 Expanded", 0x1E8000, 0x1F0000, false},
    {"Tile32 BL Expanded", 0x1F0000, 0x1F8000, false},
    {"Tile32 BR Expanded", 0x1F8000, 0x200000, false},
    
    // ZSCustomOverworld data
    {"ZSCustom Tables", 0x140000, 0x142000, false},
    {"Overlay Space", 0x120000, 0x130000, false},
};

// Feature detection addresses (from zelda3/common.h and overworld_map.h)
constexpr uint32_t kZSCustomVersionPos = 0x140145;    // OverworldCustomASMHasBeenApplied
constexpr uint32_t kMap16ExpandedFlagPos = 0x02FD28;  // Only valid after ASM applied
constexpr uint32_t kMap32ExpandedFlagPos = 0x01772E;  // Only valid after ASM applied

// =============================================================================
// Comparison Structures
// =============================================================================

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
};

struct CompareResult {
  RomInfo target;
  RomInfo baseline;
  bool sizes_match = false;
  bool versions_match = false;
  bool features_match = false;
  std::vector<DiffRegion> diff_regions;
  size_t total_diff_bytes = 0;
};

// =============================================================================
// Helper Functions
// =============================================================================

uint32_t CalculateChecksum(const std::vector<uint8_t>& data) {
  uint32_t sum = 0;
  for (size_t i = 0; i < data.size(); ++i) {
    sum += data[i];
  }
  return sum;
}

RomInfo AnalyzeRom(const std::vector<uint8_t>& data, const std::string& name) {
  RomInfo info;
  info.filename = name;
  info.size = data.size();
  info.checksum = CalculateChecksum(data);
  
  // Detect ZSCustomOverworld version
  // 0xFF or 0x00 means vanilla (no ASM applied)
  if (kZSCustomVersionPos < data.size()) {
    info.zs_version = data[kZSCustomVersionPos];
  }
  
  bool is_vanilla = (info.zs_version == 0xFF || info.zs_version == 0x00);
  
  // Only check expanded flags if NOT vanilla
  // In vanilla ROMs, these addresses contain game code, not flags
  if (!is_vanilla) {
    if (kMap16ExpandedFlagPos < data.size()) {
      info.has_expanded_tile16 = (data[kMap16ExpandedFlagPos] != 0x0F);
    }
    
    if (kMap32ExpandedFlagPos < data.size()) {
      info.has_expanded_tile32 = (data[kMap32ExpandedFlagPos] != 0x04);
    }
  }
  
  return info;
}

std::string GetVersionString(uint8_t version) {
  if (version == 0xFF || version == 0x00) return "Vanilla";
  return absl::StrFormat("v%d", version);
}

void FindDiffRegions(const std::vector<uint8_t>& target,
                     const std::vector<uint8_t>& baseline,
                     CompareResult& result) {
  size_t min_size = std::min(target.size(), baseline.size());
  
  // Check each critical region
  for (const auto& region : kCriticalRegions) {
    if (region.start >= min_size) continue;
    
    uint32_t end = std::min(region.end, static_cast<uint32_t>(min_size));
    size_t diff_count = 0;
    
    for (uint32_t i = region.start; i < end; ++i) {
      if (target[i] != baseline[i]) {
        diff_count++;
      }
    }
    
    if (diff_count > 0) {
      DiffRegion dr;
      dr.start = region.start;
      dr.end = end;
      dr.diff_count = diff_count;
      dr.region_name = region.name;
      result.diff_regions.push_back(dr);
      result.total_diff_bytes += diff_count;
    }
  }
}

void ShowDetailedDiff(const std::vector<uint8_t>& target,
                      const std::vector<uint8_t>& baseline,
                      const DiffRegion& region,
                      int max_samples = 5) {
  std::cout << absl::StrFormat("\n  Differences in %s (0x%06X-0x%06X):\n",
      region.region_name, region.start, region.end);
  
  int samples_shown = 0;
  for (uint32_t i = region.start; i < region.end && samples_shown < max_samples; ++i) {
    if (target[i] != baseline[i]) {
      std::cout << absl::StrFormat("    0x%06X: baseline=0x%02X target=0x%02X\n",
          i, baseline[i], target[i]);
      samples_shown++;
    }
  }
  
  if (region.diff_count > static_cast<size_t>(max_samples)) {
    std::cout << absl::StrFormat("    ... and %zu more differences\n",
        region.diff_count - max_samples);
  }
}

}  // namespace

absl::Status RomCompareCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  
  auto baseline_path = parser.GetString("baseline");
  bool verbose = parser.HasFlag("verbose");
  bool show_diff = parser.HasFlag("show-diff");
  
  if (!baseline_path.has_value()) {
    return absl::InvalidArgumentError(
        "Missing required --baseline <path> argument.\n"
        "Usage: rom-compare --rom <target> --baseline <baseline.sfc>");
  }
  
  std::cout << "\n";
  std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
  std::cout << "║                      ROM COMPARE                              ║\n";
  std::cout << "║         Baseline Comparison Tool                              ║\n";
  std::cout << "╚═══════════════════════════════════════════════════════════════╝\n\n";
  
  // Load baseline ROM
  std::cout << "Loading baseline ROM: " << baseline_path.value() << "\n";
  
  std::ifstream baseline_file(baseline_path.value(), std::ios::binary);
  if (!baseline_file) {
    return absl::NotFoundError(
        absl::StrFormat("Cannot open baseline ROM: %s", baseline_path.value()));
  }
  
  std::vector<uint8_t> baseline_data(
      (std::istreambuf_iterator<char>(baseline_file)),
      std::istreambuf_iterator<char>());
  baseline_file.close();
  
  // Get target ROM data
  const std::vector<uint8_t>& target_data = rom->vector();
  
  // Analyze both ROMs
  CompareResult result;
  result.baseline = AnalyzeRom(baseline_data, baseline_path.value());
  result.target = AnalyzeRom(target_data, rom->filename());
  
  result.sizes_match = (result.target.size == result.baseline.size);
  result.versions_match = (result.target.zs_version == result.baseline.zs_version);
  result.features_match = 
      (result.target.has_expanded_tile16 == result.baseline.has_expanded_tile16) &&
      (result.target.has_expanded_tile32 == result.baseline.has_expanded_tile32);
  
  // Print ROM info
  std::cout << "\n=== ROM Information ===\n";
  std::cout << absl::StrFormat("%-20s %-30s %-30s\n", "", "Baseline", "Target");
  std::cout << std::string(80, '-') << "\n";
  std::cout << absl::StrFormat("%-20s %-30zu %-30zu\n", "Size (bytes)", 
      result.baseline.size, result.target.size);
  std::cout << absl::StrFormat("%-20s %-30s %-30s\n", "ZSCustom Version",
      GetVersionString(result.baseline.zs_version),
      GetVersionString(result.target.zs_version));
  std::cout << absl::StrFormat("%-20s %-30s %-30s\n", "Expanded Tile16",
      result.baseline.has_expanded_tile16 ? "YES" : "NO",
      result.target.has_expanded_tile16 ? "YES" : "NO");
  std::cout << absl::StrFormat("%-20s %-30s %-30s\n", "Expanded Tile32",
      result.baseline.has_expanded_tile32 ? "YES" : "NO",
      result.target.has_expanded_tile32 ? "YES" : "NO");
  std::cout << absl::StrFormat("%-20s 0x%08X%21s 0x%08X\n", "Checksum",
      result.baseline.checksum, "", result.target.checksum);
  
  // Find differences
  FindDiffRegions(target_data, baseline_data, result);
  
  // Print difference summary
  std::cout << "\n=== Difference Summary ===\n";
  
  if (result.diff_regions.empty()) {
    std::cout << "No differences found in critical regions.\n";
  } else {
    std::cout << absl::StrFormat("Found differences in %zu regions (%zu bytes total):\n",
        result.diff_regions.size(), result.total_diff_bytes);
    
    for (const auto& dr : result.diff_regions) {
      std::cout << absl::StrFormat("  %-25s 0x%06X-0x%06X  %6zu bytes differ\n",
          dr.region_name, dr.start, dr.end, dr.diff_count);
    }
  }
  
  // Show detailed diff if requested
  if (show_diff && !result.diff_regions.empty()) {
    std::cout << "\n=== Detailed Differences ===\n";
    for (const auto& dr : result.diff_regions) {
      ShowDetailedDiff(target_data, baseline_data, dr);
    }
  }
  
  // Overall assessment
  std::cout << "\n";
  std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
  std::cout << "║                       ASSESSMENT                              ║\n";
  std::cout << "╠═══════════════════════════════════════════════════════════════╣\n";
  
  bool has_issues = false;
  
  if (!result.sizes_match) {
    std::cout << "║  SIZE MISMATCH: ROMs have different sizes                    ║\n";
    has_issues = true;
  }
  
  // Check for pointer table corruption (critical!)
  for (const auto& dr : result.diff_regions) {
    if (dr.region_name == "Map32 Ptr Low" || dr.region_name == "Map32 Ptr High") {
      std::cout << absl::StrFormat(
          "║  WARNING: %s modified (%zu bytes)%-14s ║\n",
          dr.region_name, dr.diff_count, "");
      has_issues = true;
    }
  }
  
  if (!has_issues && result.diff_regions.empty()) {
    std::cout << "║  ROMs are identical in all critical regions                  ║\n";
  } else if (!has_issues) {
    std::cout << "║  ROMs have expected differences (version upgrade, etc.)      ║\n";
  }
  
  std::cout << "╚═══════════════════════════════════════════════════════════════╝\n";
  
  return absl::OkStatus();
}

}  // namespace yaze::cli

