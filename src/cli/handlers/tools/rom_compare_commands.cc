#include "cli/handlers/tools/rom_compare_commands.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "cli/handlers/tools/diagnostic_types.h"
#include "rom/rom.h"

namespace yaze::cli {

namespace {

// =============================================================================
// ROM Region Definitions
// =============================================================================

struct RomRegion {
  const char* name;
  uint32_t start;
  uint32_t end;
  bool critical;
  const char* category;
};

const RomRegion kCriticalRegions[] = {
    {"Map32 Ptr Low", 0x1794D, 0x17B2D, true, "overworld"},
    {"Map32 Ptr High", 0x17B2D, 0x17D0D, true, "overworld"},
    {"Overworld Data", 0x70000, 0x78000, true, "overworld"},
    {"Tile16 Vanilla", 0x78000, 0x78000 + (3752 * 8), false, "overworld"},
    {"Tile16 Expanded", 0x1E8000, 0x1F0000, false, "overworld"},
    {"Tile32 BL Expanded", 0x1F0000, 0x1F8000, false, "overworld"},
    {"Tile32 BR Expanded", 0x1F8000, 0x200000, false, "overworld"},
    {"Dungeon Ptr Table", 0x01F800, 0x01FB00, true, "dungeon"},
    {"Dungeon Room Data", 0x1D8000, 0x1E8000, true, "dungeon"},
    {"Message Data", 0x1C0000, 0x1D8000, false, "text"},
    {"ZSCustom Tables", 0x140000, 0x142000, false, "system"},
    {"Overlay Space", 0x120000, 0x130000, false, "overworld"},
    {"SNES Header", 0x7FC0, 0x8000, true, "system"},
    {"Bank 00 Code", 0x000000, 0x008000, true, "code"},
    {"Bank 01 Code", 0x008000, 0x010000, true, "code"},
    {"Bank 02 Code", 0x010000, 0x018000, true, "code"},
};

// =============================================================================
// Checksum Calculation
// =============================================================================

uint32_t CalculateChecksum(const std::vector<uint8_t>& data) {
  uint32_t sum = 0;
  for (size_t i = 0; i < data.size(); ++i) {
    sum += data[i];
  }
  return sum;
}

// =============================================================================
// ROM Analysis
// =============================================================================

RomCompareResult::RomInfo AnalyzeRom(const std::vector<uint8_t>& data,
                                     const std::string& name) {
  RomCompareResult::RomInfo info;
  info.filename = name;
  info.size = data.size();
  info.checksum = CalculateChecksum(data);

  if (kZSCustomVersionPos < data.size()) {
    info.zs_version = data[kZSCustomVersionPos];
  }

  bool is_vanilla = (info.zs_version == 0xFF || info.zs_version == 0x00);

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
  if (version == 0xFF || version == 0x00) {
    return "Vanilla";
  }
  return absl::StrFormat("v%d", version);
}

void FindDiffRegions(const std::vector<uint8_t>& target,
                     const std::vector<uint8_t>& baseline,
                     RomCompareResult& result, bool smart_diff,
                     const std::string& region_filter, bool scan_all) {
  size_t min_size = std::min(target.size(), baseline.size());

  auto is_ignored = [&](uint32_t i) {
    if (!smart_diff) return false;
    // SNES Checksum region
    if (i >= yaze::cli::kChecksumComplementPos && i <= yaze::cli::kChecksumPos + 1)
      return true;
    // ZSCustom Version/Flags region
    if (i >= 0x140141 && i <= 0x140148)
      return true;
    return false;
  };

  if (scan_all) {
    // Perform full ROM scan for differences
    uint32_t start = 0;
    bool in_diff = false;
    size_t diff_count = 0;

    for (uint32_t i = 0; i < min_size; ++i) {
      if (is_ignored(i)) continue;

      if (target[i] != baseline[i]) {
        if (!in_diff) {
          start = i;
          in_diff = true;
          diff_count = 0;
        }
        diff_count++;
      } else if (in_diff) {
        RomCompareResult::DiffRegion diff;
        diff.start = start;
        diff.end = i;
        diff.diff_count = diff_count;
        diff.region_name = "Modified Region";
        diff.critical = false;
        result.diff_regions.push_back(diff);
        result.total_diff_bytes += diff_count;
        in_diff = false;
      }
    }
    return;
  }

  for (const auto& region : kCriticalRegions) {
    if (!region_filter.empty() && region.category != region_filter) {
      continue;
    }

    if (region.start >= min_size) {
      continue;
    }

    uint32_t end = std::min(region.end, static_cast<uint32_t>(min_size));
    size_t diff_count = 0;

    for (uint32_t i = region.start; i < end; ++i) {
      if (is_ignored(i)) continue;

      if (target[i] != baseline[i]) {
        diff_count++;
      }
    }

    if (diff_count > 0) {
      RomCompareResult::DiffRegion diff;
      diff.start = region.start;
      diff.end = end;
      diff.diff_count = diff_count;
      diff.region_name = region.name;
      diff.critical = region.critical;
      result.diff_regions.push_back(diff);
      result.total_diff_bytes += diff_count;
    }
  }
}

// =============================================================================
// Output Helpers
// =============================================================================

void OutputRomInfoJson(resources::OutputFormatter& formatter,
                       const std::string& prefix,
                       const RomCompareResult::RomInfo& info) {
  formatter.AddField(prefix + "_filename", info.filename);
  formatter.AddField(prefix + "_size", static_cast<int>(info.size));
  formatter.AddField(prefix + "_version", GetVersionString(info.zs_version));
  formatter.AddField(prefix + "_expanded_tile16", info.has_expanded_tile16);
  formatter.AddField(prefix + "_expanded_tile32", info.has_expanded_tile32);
  formatter.AddHexField(prefix + "_checksum", info.checksum, 8);
}

void OutputTextBanner(bool is_json) {
  if (is_json) {
    return;
  }
  std::cout << "\n";
  std::cout
      << "╔═══════════════════════════════════════════════════════════════╗\n";
  std::cout
      << "║                      ROM COMPARE                              ║\n";
  std::cout
      << "║         Baseline Comparison Tool                              ║\n";
  std::cout
      << "╚═══════════════════════════════════════════════════════════════╝\n";
}

void OutputTextRomInfo(const RomCompareResult& result) {
  std::cout << "\n=== ROM Information ===\n";
  std::cout << absl::StrFormat("%-20s %-30s %-30s\n", "", "Baseline", "Target");
  std::cout << std::string(80, '-') << "\n";
  std::cout << absl::StrFormat("%-20s %-30zu %-30zu\n", "Size (bytes)",
                               result.baseline.size, result.target.size);
  std::cout << absl::StrFormat("%-20s %-30s %-30s\n", "ZSCustom Version",
                               GetVersionString(result.baseline.zs_version),
                               GetVersionString(result.target.zs_version));
  std::cout << absl::StrFormat(
      "%-20s %-30s %-30s\n", "Expanded Tile16",
      result.baseline.has_expanded_tile16 ? "YES" : "NO",
      result.target.has_expanded_tile16 ? "YES" : "NO");
  std::cout << absl::StrFormat(
      "%-20s %-30s %-30s\n", "Expanded Tile32",
      result.baseline.has_expanded_tile32 ? "YES" : "NO",
      result.target.has_expanded_tile32 ? "YES" : "NO");
  std::cout << absl::StrFormat("%-20s 0x%08X%21s 0x%08X\n", "Checksum",
                               result.baseline.checksum, "",
                               result.target.checksum);
}

void OutputTextDiffSummary(const RomCompareResult& result) {
  std::cout << "\n=== Difference Summary ===\n";

  if (result.diff_regions.empty()) {
    std::cout << "No differences found in specified regions.\n";
    return;
  }

  std::cout << absl::StrFormat(
      "Found differences in %zu regions (%zu bytes total):\n",
      result.diff_regions.size(), result.total_diff_bytes);

  for (const auto& diff : result.diff_regions) {
    std::string marker = diff.critical ? "[CRITICAL] " : "";
    std::cout << absl::StrFormat("  %s%-25s 0x%06X-0x%06X  %6zu bytes differ\n",
                                 marker, diff.region_name, diff.start, diff.end,
                                 diff.diff_count);
  }
}

void OutputTextDetailedDiff(const std::vector<uint8_t>& target,
                            const std::vector<uint8_t>& baseline,
                            const RomCompareResult::DiffRegion& region,
                            int max_samples) {
  std::cout << absl::StrFormat("\n  Differences in %s (0x%06X-0x%06X):\n",
                               region.region_name, region.start, region.end);

  int samples_shown = 0;
  for (uint32_t i = region.start; i < region.end && samples_shown < max_samples;
       ++i) {
    if (target[i] != baseline[i]) {
      std::cout << absl::StrFormat(
          "    0x%06X:  base 0x%02X  |  target 0x%02X\n", i, baseline[i],
          target[i]);
      samples_shown++;
    }
  }

  if (region.diff_count > static_cast<size_t>(max_samples)) {
    std::cout << absl::StrFormat("    ... and %zu more differences\n",
                                 region.diff_count - max_samples);
  }
}

void OutputTextAssessment(const RomCompareResult& result) {
  std::cout << "\n";
  std::cout
      << "╔═══════════════════════════════════════════════════════════════╗\n";
  std::cout
      << "║                       ASSESSMENT                              ║\n";
  std::cout
      << "╠═══════════════════════════════════════════════════════════════╣\n";

  bool has_issues = false;

  if (!result.sizes_match) {
    std::cout
        << "║  SIZE MISMATCH: ROMs have different sizes                    ║\n";
    has_issues = true;
  }

  for (const auto& diff : result.diff_regions) {
    if (diff.critical) {
      std::cout << absl::StrFormat(
          "║  WARNING: %s modified (%zu bytes)%-14s ║\n", diff.region_name,
          diff.diff_count, "");
      has_issues = true;
    }
  }

  if (!has_issues && result.diff_regions.empty()) {
    std::cout
        << "║  ROMs are identical in all checked regions                   ║\n";
  } else if (!has_issues) {
    std::cout
        << "║  ROMs have expected differences (modifications detected)     ║\n";
  }

  std::cout
      << "╚═══════════════════════════════════════════════════════════════╝\n";
}

}  // namespace

absl::Status RomCompareCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto baseline_path = parser.GetString("baseline");
  bool verbose = parser.HasFlag("verbose");
  bool show_diff = parser.HasFlag("show-diff");
  bool smart_diff = parser.HasFlag("smart");
  bool scan_all = parser.HasFlag("all");
  std::string region_filter = parser.GetString("region").value_or("");
  bool is_json = formatter.IsJson();

  if (!baseline_path.has_value()) {
    return absl::InvalidArgumentError(
        "Missing required --baseline <path> argument.\n"
        "Usage: rom-compare --rom <target> --baseline <baseline.sfc>");
  }

  OutputTextBanner(is_json);

  // Load baseline ROM
  if (!is_json) {
    std::cout << "Loading baseline ROM: " << baseline_path.value() << "\n";
  }

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
  RomCompareResult result;
  result.baseline = AnalyzeRom(baseline_data, baseline_path.value());
  result.target = AnalyzeRom(target_data, rom->filename());

  result.sizes_match = (result.target.size == result.baseline.size);
  result.versions_match =
      (result.target.zs_version == result.baseline.zs_version);
  result.features_match = (result.target.has_expanded_tile16 ==
                           result.baseline.has_expanded_tile16) &&
                          (result.target.has_expanded_tile32 ==
                           result.baseline.has_expanded_tile32);

  // Find differences
  FindDiffRegions(target_data, baseline_data, result, smart_diff, region_filter, scan_all);

  // JSON output
  OutputRomInfoJson(formatter, "baseline", result.baseline);
  OutputRomInfoJson(formatter, "target", result.target);
  formatter.AddField("sizes_match", result.sizes_match);
  formatter.AddField("versions_match", result.versions_match);
  formatter.AddField("features_match", result.features_match);
  formatter.AddField("total_diff_bytes",
                     static_cast<int>(result.total_diff_bytes));

  formatter.BeginArray("diff_regions");
  for (const auto& diff : result.diff_regions) {
    std::string json = absl::StrFormat(
        R"({"name":"%s","start":"0x%06X","end":"0x%06X","diff_count":%zu,"critical":%s})",
        diff.region_name, diff.start, diff.end, diff.diff_count,
        diff.critical ? "true" : "false");
    formatter.AddArrayItem(json);
  }
  formatter.EndArray();

  // Check for critical issues
  bool has_critical = false;
  for (const auto& diff : result.diff_regions) {
    if (diff.critical) {
      has_critical = true;
      break;
    }
  }
  formatter.AddField("has_critical_differences", has_critical);
  formatter.AddField("assessment", has_critical
                                       ? "warning"
                                       : (result.diff_regions.empty()
                                              ? "identical"
                                              : "expected_differences"));

  // Text output
  if (!is_json) {
    OutputTextRomInfo(result);
    OutputTextDiffSummary(result);

    if (show_diff && !result.diff_regions.empty()) {
      std::cout << "\n=== Detailed Differences ===\n";
      for (const auto& diff : result.diff_regions) {
        OutputTextDetailedDiff(target_data, baseline_data, diff,
                               verbose ? 10 : 5);
      }
    }

    OutputTextAssessment(result);
  }

  return absl::OkStatus();
}

}  // namespace yaze::cli
