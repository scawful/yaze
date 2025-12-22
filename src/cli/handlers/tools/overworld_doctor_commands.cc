#include "cli/handlers/tools/overworld_doctor_commands.h"

#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "rom/rom.h"
#include "cli/handlers/tools/diagnostic_types.h"
#include "core/asar_wrapper.h"
#include "zelda3/overworld/overworld_entrance.h"
#include "zelda3/overworld/overworld_exit.h"
#include "zelda3/overworld/overworld_item.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze::cli {

namespace {

// =============================================================================
// Address Conversion Helpers
// =============================================================================

uint32_t SnesToPc(uint32_t snes_addr) {
  return ((snes_addr & 0x7F0000) >> 1) | (snes_addr & 0x7FFF);
}

// Check if a tile16 entry looks valid
bool IsTile16Valid(uint16_t tile_info) {
  // Tile info format: tttttttt ttttpppp hvf00000
  // Bits 8-12 (0x1F00) should be 0 for valid tiles (unless flip bits are set)
  // Returns false if reserved bits are set without flip bits
  return (tile_info & 0x1F00) == 0 || (tile_info & 0xE000) != 0;
}

// =============================================================================
// Feature Detection
// =============================================================================

RomFeatures DetectRomFeatures(Rom* rom) {
  RomFeatures features;
  
  // Detect ZSCustomOverworld version
  if (kZSCustomVersionPos < rom->size()) {
    features.zs_custom_version = rom->data()[kZSCustomVersionPos];
    features.is_vanilla =
        (features.zs_custom_version == 0xFF || features.zs_custom_version == 0x00);
    features.is_v2 = (!features.is_vanilla && features.zs_custom_version == 2);
    features.is_v3 = (!features.is_vanilla && features.zs_custom_version >= 3);
  } else {
    features.is_vanilla = true;
  }

  // Detect expanded tile16/tile32 (only if ASM applied)
  if (!features.is_vanilla) {
    if (kMap16ExpandedFlagPos < rom->size()) {
      uint8_t flag = rom->data()[kMap16ExpandedFlagPos];
      features.has_expanded_tile16 = (flag != 0x0F);
    }
    
    if (kMap32ExpandedFlagPos < rom->size()) {
      uint8_t flag = rom->data()[kMap32ExpandedFlagPos];
      features.has_expanded_tile32 = (flag != 0x04);
    }
  }

  // Detect expanded pointer tables via ASM marker
  if (kExpandedPtrTableMarker < rom->size()) {
    features.has_expanded_pointer_tables =
        (rom->data()[kExpandedPtrTableMarker] == kExpandedPtrTableMagic);
  }

  // Detect ZSCustomOverworld feature enables
  if (!features.is_vanilla) {
    if (kCustomBGEnabledPos < rom->size()) {
      features.custom_bg_enabled = (rom->data()[kCustomBGEnabledPos] != 0);
    }
    if (kCustomMainPalettePos < rom->size()) {
      features.custom_main_palette_enabled =
          (rom->data()[kCustomMainPalettePos] != 0);
    }
    if (kCustomMosaicPos < rom->size()) {
      features.custom_mosaic_enabled = (rom->data()[kCustomMosaicPos] != 0);
    }
    if (kCustomAnimatedGFXPos < rom->size()) {
      features.custom_animated_gfx_enabled =
          (rom->data()[kCustomAnimatedGFXPos] != 0);
    }
    if (kCustomOverlayPos < rom->size()) {
      features.custom_overlay_enabled = (rom->data()[kCustomOverlayPos] != 0);
    }
    if (kCustomTileGFXPos < rom->size()) {
      features.custom_tile_gfx_enabled = (rom->data()[kCustomTileGFXPos] != 0);
    }
  }

  return features;
}

// =============================================================================
// Map Pointer Validation
// =============================================================================

void ValidateMapPointers(Rom* rom, DiagnosticReport& report) {
  report.map_status.lw_dw_maps_valid = true;
  report.map_status.sw_maps_valid = true;

  for (int map_id = 0; map_id < kVanillaMapCount; ++map_id) {
    uint32_t ptr_low_addr = kPtrTableLowBase + (3 * map_id);
    uint32_t ptr_high_addr = kPtrTableHighBase + (3 * map_id);
    
    if (ptr_low_addr + 3 > rom->size() || ptr_high_addr + 3 > rom->size()) {
      report.map_status.invalid_map_count++;
      if (map_id < 0x80) {
        report.map_status.lw_dw_maps_valid = false;
      } else {
        report.map_status.sw_maps_valid = false;
      }
      continue;
    }
    
    uint32_t snes_low = rom->data()[ptr_low_addr] |
                        (rom->data()[ptr_low_addr + 1] << 8) |
                        (rom->data()[ptr_low_addr + 2] << 16);
    uint32_t snes_high = rom->data()[ptr_high_addr] |
                         (rom->data()[ptr_high_addr + 1] << 8) |
                         (rom->data()[ptr_high_addr + 2] << 16);
    
    uint32_t pc_low = SnesToPc(snes_low);
    uint32_t pc_high = SnesToPc(snes_high);
    
    bool low_valid = (pc_low > 0 && pc_low < rom->size());
    bool high_valid = (pc_high > 0 && pc_high < rom->size());
    
    if (!low_valid || !high_valid) {
      report.map_status.invalid_map_count++;
      if (map_id < 0x80) {
        report.map_status.lw_dw_maps_valid = false;
      } else {
        report.map_status.sw_maps_valid = false;
      }

      DiagnosticFinding finding;
      finding.id = "invalid_map_pointer";
      finding.severity = DiagnosticSeverity::kError;
      finding.message =
          absl::StrFormat("Map 0x%02X has invalid pointer", map_id);
      finding.location = absl::StrFormat("0x%06X", ptr_low_addr);
      finding.suggested_action = "Restore from baseline ROM";
      finding.fixable = false;
      report.AddFinding(finding);
    }
  }

  // Tail maps status
  report.map_status.tail_maps_valid = report.features.has_expanded_pointer_tables;
  report.map_status.can_support_tail = report.features.has_expanded_pointer_tables;

  // Add finding if map pointer corruption detected
  if (!report.map_status.lw_dw_maps_valid) {
    DiagnosticFinding finding;
    finding.id = "lw_dw_corruption";
    finding.severity = DiagnosticSeverity::kCritical;
    finding.message = "Light/Dark World map pointers are corrupted";
    finding.location = absl::StrFormat("0x%06X-0x%06X", kPtrTableLowBase,
                                       kPtrTableLowBase + 0x180);
    finding.suggested_action =
        "ROM may be severely damaged. Restore from backup.";
    finding.fixable = false;
    report.AddFinding(finding);
  }

  if (!report.map_status.sw_maps_valid) {
    DiagnosticFinding finding;
    finding.id = "sw_corruption";
    finding.severity = DiagnosticSeverity::kError;
    finding.message = "Special World map pointers are corrupted";
    finding.location = absl::StrFormat("0x%06X-0x%06X", kPtrTableLowBase + 0x180,
                                       kPtrTableHighBase);
    finding.suggested_action = "Restore Special World data from baseline";
    finding.fixable = false;
    report.AddFinding(finding);
  }
}

// =============================================================================
// Tile16 Corruption Check
// =============================================================================

void CheckTile16Corruption(Rom* rom, DiagnosticReport& report) {
  report.tile16_status.uses_expanded = report.features.has_expanded_tile16;

  if (!report.features.has_expanded_tile16) {
    return;
  }
  
  for (uint32_t addr : kProblemAddresses) {
    if (addr >= kMap16TilesExpanded && addr < kMap16TilesExpandedEnd) {
      int tile_offset = addr - kMap16TilesExpanded;
      int tile_index = tile_offset / 8;
      
      uint16_t tile_data[4];
      for (int i = 0; i < 4 && (addr + i * 2 + 1) < rom->size(); ++i) {
        tile_data[i] =
            rom->data()[addr + i * 2] | (rom->data()[addr + i * 2 + 1] << 8);
      }

      bool looks_valid = true;
      for (int i = 0; i < 4; ++i) {
        if (!IsTile16Valid(tile_data[i])) {
          looks_valid = false;
          break;
        }
      }
      
      if (!looks_valid) {
        report.tile16_status.corruption_detected = true;
        report.tile16_status.corrupted_addresses.push_back(addr);
        report.tile16_status.corrupted_tile_count++;

        DiagnosticFinding finding;
        finding.id = "tile16_corruption";
        finding.severity = DiagnosticSeverity::kError;
        finding.message =
            absl::StrFormat("Corrupted tile16 #%d", tile_index);
        finding.location = absl::StrFormat("0x%06X", addr);
        finding.suggested_action = "Run with --fix to zero corrupted entries";
        finding.fixable = true;
        report.AddFinding(finding);
      }
    }
  }
}

// =============================================================================
// Baseline ROM Loading
// =============================================================================

std::unique_ptr<Rom> LoadBaselineRom(const std::optional<std::string>& path,
                                     std::string* resolved_path) {
  std::vector<std::string> candidates;
  if (path.has_value()) {
    candidates.push_back(*path);
  } else {
    candidates = {"alttp_vanilla.sfc", "vanilla.sfc", "zelda3.sfc"};
  }

  for (const auto& candidate : candidates) {
    std::ifstream probe(candidate, std::ios::binary);
    if (!probe.good()) continue;
    probe.close();

    auto baseline = std::make_unique<Rom>();
    auto status = baseline->LoadFromFile(candidate);
    if (status.ok()) {
      if (resolved_path) *resolved_path = candidate;
      return baseline;
    }
  }

  return nullptr;
}

// =============================================================================
// Distribution Stats for Entity Coverage
// =============================================================================

template <typename T, typename Getter>
MapDistributionStats BuildDistribution(const std::vector<T>& entries,
                                       Getter getter) {
  MapDistributionStats stats;
  for (const auto& entry : entries) {
    uint16_t map = getter(entry);
    stats.counts[map]++;
    stats.total++;
    if (map >= zelda3::kNumOverworldMaps) {
      stats.invalid++;
    }
  }
  stats.unique = static_cast<int>(stats.counts.size());

  for (const auto& [map, count] : stats.counts) {
    if (count > stats.most_common_count) {
      stats.most_common_count = count;
      stats.most_common_map = map;
    }
  }
  return stats;
}

absl::StatusOr<std::vector<zelda3::OverworldMap>> BuildOverworldMaps(Rom* rom) {
  std::vector<zelda3::OverworldMap> maps;
  maps.reserve(zelda3::kNumOverworldMaps);
  for (int i = 0; i < zelda3::kNumOverworldMaps; ++i) {
    maps.emplace_back(i, rom);
  }
  return maps;
}

// =============================================================================
// Repair Functions
// =============================================================================

absl::Status RepairTile16Region(Rom* rom, const DiagnosticReport& report,
                                bool dry_run) {
  if (!report.tile16_status.corruption_detected) {
    return absl::OkStatus();
  }

  for (uint32_t addr : report.tile16_status.corrupted_addresses) {
    if (!dry_run) {
    for (int i = 0; i < 8 && addr + i < rom->size(); ++i) {
      (*rom)[addr + i] = 0x00;
    }
    }
  }

  return absl::OkStatus();
}

// Apply tail map expansion ASM patch
absl::Status ApplyTailExpansion(Rom* rom, bool dry_run, bool verbose) {
  // Check if already applied
  if (kExpandedPtrTableMarker < rom->size() &&
      rom->data()[kExpandedPtrTableMarker] == kExpandedPtrTableMagic) {
    return absl::AlreadyExistsError(
        "Tail map expansion already applied (marker 0xEA found at 0x1423FF)");
  }

  // Check if ZSCustomOverworld v3 is present (required prerequisite)
  if (kZSCustomVersionPos < rom->size()) {
    uint8_t version = rom->data()[kZSCustomVersionPos];
    if (version < 3 && version != 0xFF && version != 0x00) {
      return absl::FailedPreconditionError(
          "Tail map expansion requires ZSCustomOverworld v3 or later. "
          "Apply ZSCustomOverworld v3 first.");
    }
  }

  if (dry_run) {
    return absl::OkStatus();
  }

  // Find the patch file in standard locations
  std::vector<std::string> patch_locations = {
      "assets/patches/Overworld/TailMapExpansion.asm",
      "../assets/patches/Overworld/TailMapExpansion.asm",
      "TailMapExpansion.asm"
  };

  std::string patch_path;
  for (const auto& loc : patch_locations) {
    std::ifstream probe(loc);
    if (probe.good()) {
      patch_path = loc;
      break;
    }
  }

  if (patch_path.empty()) {
    return absl::NotFoundError(
        "TailMapExpansion.asm patch file not found. "
        "Expected locations: assets/patches/Overworld/TailMapExpansion.asm");
  }

  // Apply the patch using Asar
  core::AsarWrapper asar;
  RETURN_IF_ERROR(asar.Initialize());

  std::vector<uint8_t> rom_data(rom->data(), rom->data() + rom->size());
  auto result = asar.ApplyPatch(patch_path, rom_data);

  if (!result.ok()) {
    return result.status();
  }

  if (!result->success) {
    std::string error_msg = "Asar patch failed:";
    for (const auto& err : result->errors) {
      error_msg += " " + err;
    }
    return absl::InternalError(error_msg);
  }

  // Handle ROM size changes - patches may expand the ROM for custom code
  if (rom_data.size() > rom->size()) {
    if (verbose) {
      std::cout << absl::StrFormat("  Expanding ROM from %zu to %zu bytes\n",
                                   rom->size(), rom_data.size());
    }
    rom->Expand(static_cast<int>(rom_data.size()));
  } else if (rom_data.size() < rom->size()) {
    // ROM shrinking is unexpected and likely an error
    return absl::InternalError(
        absl::StrFormat("ROM size decreased unexpectedly: %zu -> %zu",
                        rom->size(), rom_data.size()));
  }

  // Copy patched data back to ROM
  for (size_t i = 0; i < rom_data.size(); ++i) {
    (*rom)[i] = rom_data[i];
  }

  // Verify marker was written (with bounds check to prevent buffer overflow)
  if (kExpandedPtrTableMarker >= rom->size()) {
    return absl::InternalError(
        absl::StrFormat("ROM too small for expansion marker at 0x%06X "
                        "(ROM size: 0x%06zX). Patch may have failed.",
                        kExpandedPtrTableMarker, rom->size()));
  }
  if (rom->data()[kExpandedPtrTableMarker] != kExpandedPtrTableMagic) {
    return absl::InternalError(
        "Patch applied but marker not found. Patch may be incomplete.");
  }

  return absl::OkStatus();
}

// =============================================================================
// Output Helpers
// =============================================================================

void OutputFeaturesJson(resources::OutputFormatter& formatter,
                        const RomFeatures& features) {
  formatter.AddField("zs_custom_version", features.GetVersionString());
  formatter.AddField("is_vanilla", features.is_vanilla);
  formatter.AddField("expanded_tile16", features.has_expanded_tile16);
  formatter.AddField("expanded_tile32", features.has_expanded_tile32);
  formatter.AddField("expanded_pointer_tables",
                     features.has_expanded_pointer_tables);

  if (!features.is_vanilla) {
    formatter.AddField("custom_bg_enabled", features.custom_bg_enabled);
    formatter.AddField("custom_main_palette_enabled",
                       features.custom_main_palette_enabled);
    formatter.AddField("custom_mosaic_enabled", features.custom_mosaic_enabled);
    formatter.AddField("custom_animated_gfx_enabled",
                       features.custom_animated_gfx_enabled);
    formatter.AddField("custom_overlay_enabled", features.custom_overlay_enabled);
    formatter.AddField("custom_tile_gfx_enabled",
                       features.custom_tile_gfx_enabled);
  }
}

void OutputMapStatusJson(resources::OutputFormatter& formatter,
                         const MapPointerStatus& status) {
  formatter.AddField("lw_dw_maps_valid", status.lw_dw_maps_valid);
  formatter.AddField("sw_maps_valid", status.sw_maps_valid);
  formatter.AddField("tail_maps_available", status.can_support_tail);
  formatter.AddField("invalid_map_count", status.invalid_map_count);
}

void OutputFindingsJson(resources::OutputFormatter& formatter,
                        const DiagnosticReport& report) {
  formatter.BeginArray("findings");
  for (const auto& finding : report.findings) {
    formatter.AddArrayItem(finding.FormatJson());
  }
  formatter.EndArray();
}

void OutputSummaryJson(resources::OutputFormatter& formatter,
                       const DiagnosticReport& report) {
  formatter.AddField("total_findings", report.TotalFindings());
  formatter.AddField("critical_count", report.critical_count);
  formatter.AddField("error_count", report.error_count);
  formatter.AddField("warning_count", report.warning_count);
  formatter.AddField("info_count", report.info_count);
  formatter.AddField("fixable_count", report.fixable_count);
  formatter.AddField("has_problems", report.HasProblems());
}

void OutputTextBanner(bool is_json) {
  if (is_json) return;
  std::cout << "\n";
  std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
  std::cout << "║                    OVERWORLD DOCTOR                           ║\n";
  std::cout << "║         ROM Diagnostic & Repair Tool                          ║\n";
  std::cout << "╚═══════════════════════════════════════════════════════════════╝\n";
}

void OutputTextSummary(const DiagnosticReport& report) {
  std::cout << "\n";
  std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
  std::cout << "║                    DIAGNOSTIC SUMMARY                         ║\n";
  std::cout << "╠═══════════════════════════════════════════════════════════════╣\n";
  
  std::cout << absl::StrFormat(
      "║  ROM Version: %-46s ║\n",
      report.features.GetVersionString());

  std::cout << absl::StrFormat(
      "║  Expanded Tile16: %-42s ║\n",
      report.features.has_expanded_tile16 ? "YES" : "NO");
  std::cout << absl::StrFormat(
      "║  Expanded Tile32: %-42s ║\n",
      report.features.has_expanded_tile32 ? "YES" : "NO");
  std::cout << absl::StrFormat(
      "║  Expanded Ptr Tables: %-38s ║\n",
      report.features.has_expanded_pointer_tables ? "YES (192 maps)"
                                                   : "NO (160 maps)");
  
  std::cout << "╠═══════════════════════════════════════════════════════════════╣\n";
  
  std::cout << absl::StrFormat(
      "║  Light/Dark World (0x00-0x7F): %-29s ║\n",
      report.map_status.lw_dw_maps_valid ? "OK" : "CORRUPTED");
  std::cout << absl::StrFormat(
      "║  Special World (0x80-0x9F): %-32s ║\n",
      report.map_status.sw_maps_valid ? "OK" : "CORRUPTED");
  std::cout << absl::StrFormat(
      "║  Tail Maps (0xA0-0xBF): %-36s ║\n",
      report.map_status.can_support_tail ? "Available"
                                         : "N/A (no ASM expansion)");

  if (report.tile16_status.uses_expanded) {
    std::cout << "╠═══════════════════════════════════════════════════════════════╣\n";
    if (report.tile16_status.corruption_detected) {
      std::cout << absl::StrFormat(
          "║  Tile16 Corruption: DETECTED (%zu addresses)%-17s ║\n",
          report.tile16_status.corrupted_addresses.size(), "");
      for (uint32_t addr : report.tile16_status.corrupted_addresses) {
        int tile_idx = (addr - kMap16TilesExpanded) / 8;
        std::cout << absl::StrFormat("║    - 0x%06X (tile #%d)%-36s ║\n", 
            addr, tile_idx, "");
      }
    } else {
      std::cout << "║  Tile16 Corruption: None detected                             ║\n";
    }
  }
  
  std::cout << "╠═══════════════════════════════════════════════════════════════╣\n";
  std::cout << absl::StrFormat(
      "║  Total Findings: %-43d ║\n", report.TotalFindings());
  std::cout << absl::StrFormat(
      "║    Critical: %-3d  Errors: %-3d  Warnings: %-3d  Info: %-3d%-4s ║\n",
      report.critical_count, report.error_count, report.warning_count,
      report.info_count, "");
  std::cout << absl::StrFormat(
      "║  Fixable Issues: %-43d ║\n", report.fixable_count);
  std::cout << "╚═══════════════════════════════════════════════════════════════╝\n";
}

void OutputTextFindings(const DiagnosticReport& report) {
  if (report.findings.empty()) {
    return;
  }

  std::cout << "\n=== Detailed Findings ===\n";
  for (const auto& finding : report.findings) {
    std::cout << "  " << finding.FormatText() << "\n";
    if (!finding.suggested_action.empty()) {
      std::cout << "    → " << finding.suggested_action << "\n";
    }
  }
}

}  // namespace

absl::Status OverworldDoctorCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  bool fix_mode = parser.HasFlag("fix");
  bool apply_tail_expansion = parser.HasFlag("apply-tail-expansion");
  bool dry_run = parser.HasFlag("dry-run");
  bool verbose = parser.HasFlag("verbose");
  auto output_path = parser.GetString("output");
  auto baseline_path = parser.GetString("baseline");
  bool is_json = formatter.IsJson();

  // Show text banner for text mode
  OutputTextBanner(is_json);

  // Build diagnostic report
  DiagnosticReport report;
  report.rom_path = rom->filename();
  report.features = DetectRomFeatures(rom);
  ValidateMapPointers(rom, report);
  CheckTile16Corruption(rom, report);

  // Load baseline if provided
  std::string resolved_baseline;
  auto baseline_rom = LoadBaselineRom(baseline_path, &resolved_baseline);
  
  // Add info finding if no ASM expansion for tail maps
  if (!report.features.has_expanded_pointer_tables) {
    DiagnosticFinding finding;
    finding.id = "no_tail_support";
    finding.severity = DiagnosticSeverity::kInfo;
    finding.message = "Tail maps (0xA0-0xBF) not available";
    finding.location = "";
    finding.suggested_action =
        "Apply TailMapExpansion.asm patch (after ZSCustomOverworld v3) to "
        "expand pointer tables to 192 entries. Use: z3ed overworld-doctor "
        "--apply-tail-expansion or apply manually with Asar.";
    finding.fixable = false;
    report.AddFinding(finding);
  }

  // Output to formatter
  formatter.AddField("rom_path", report.rom_path);
  formatter.AddField("fix_mode", fix_mode);
  formatter.AddField("dry_run", dry_run);

  // Features section
  if (is_json) {
    OutputFeaturesJson(formatter, report.features);
    OutputMapStatusJson(formatter, report.map_status);
    OutputFindingsJson(formatter, report);
    OutputSummaryJson(formatter, report);
  }

  // Text mode: show nice ASCII summary
  if (!is_json) {
    OutputTextSummary(report);
    if (verbose) {
      OutputTextFindings(report);
    }
  }

  // Entity coverage (text mode only for now)
  if (!is_json) {
  ASSIGN_OR_RETURN(auto exits, zelda3::LoadExits(rom));
  ASSIGN_OR_RETURN(auto entrances, zelda3::LoadEntrances(rom));
  ASSIGN_OR_RETURN(auto maps, BuildOverworldMaps(rom));
  ASSIGN_OR_RETURN(auto items, zelda3::LoadItems(rom, maps));

    auto exit_stats =
        BuildDistribution(exits, [](const auto& exit) { return exit.map_id_; });
    auto entrance_stats = BuildDistribution(
        entrances,
        [](const auto& ent) { return static_cast<uint16_t>(ent.map_id_); });
    auto item_stats =
        BuildDistribution(items, [](const auto& item) { return item.map_id_; });

  std::cout << "\n=== Overworld Entity Coverage ===\n";
    std::cout << absl::StrFormat(
        "  exits     : total=%d unique=%d most_common=0x%02X (%d)\n",
        exit_stats.total, exit_stats.unique, exit_stats.most_common_map,
        exit_stats.most_common_count);
    std::cout << absl::StrFormat(
        "  entrances : total=%d unique=%d most_common=0x%02X (%d)\n",
        entrance_stats.total, entrance_stats.unique,
        entrance_stats.most_common_map, entrance_stats.most_common_count);
    std::cout << absl::StrFormat(
        "  items     : total=%d unique=%d most_common=0x%02X (%d)\n",
        item_stats.total, item_stats.unique, item_stats.most_common_map,
        item_stats.most_common_count);

    if (baseline_rom) {
      std::cout << absl::StrFormat("  Baseline used: %s\n", resolved_baseline);
    }
  }

  // Apply tail expansion if requested
  if (apply_tail_expansion) {
    if (dry_run) {
      if (!is_json) {
        std::cout << "\n=== Dry Run - Tail Map Expansion ===\n";
        if (report.features.has_expanded_pointer_tables) {
          std::cout << "  Tail expansion already applied.\n";
        } else {
          std::cout << "  Would apply TailMapExpansion.asm patch.\n";
          std::cout << "  This will:\n";
          std::cout << "    - Relocate pointer tables to $28:A400\n";
          std::cout << "    - Expand from 160 to 192 map entries\n";
          std::cout << "    - Write marker byte 0xEA at $28:A3FF\n";
          std::cout << "    - Add blank map data at $30:8000\n";
        }
        std::cout << "\nNo changes made (dry run).\n";
      }
      formatter.AddField("dry_run_tail_expansion", true);
    } else {
      auto status = ApplyTailExpansion(rom, false, verbose);
      if (status.ok()) {
        if (!is_json) {
          std::cout << "\n=== Tail Map Expansion Applied ===\n";
          std::cout << "  Pointer tables relocated to $28:A400/$28:A640\n";
          std::cout << "  Maps 0xA0-0xBF now available for editing\n";
        }
        formatter.AddField("tail_expansion_applied", true);

        // Re-detect features after patch
        report.features = DetectRomFeatures(rom);
      } else if (absl::IsAlreadyExists(status)) {
        if (!is_json) {
          std::cout << "\n[INFO] Tail expansion already applied.\n";
        }
        formatter.AddField("tail_expansion_already_applied", true);
      } else {
        if (!is_json) {
          std::cout << "\n[ERROR] Failed to apply tail expansion: "
                    << status.message() << "\n";
        }
        formatter.AddField("tail_expansion_error", std::string(status.message()));
        // Continue with diagnostics, don't fail the whole command
      }
    }
  }

  // Fix mode handling
  if (fix_mode) {
    if (dry_run) {
      if (!is_json) {
        std::cout << "\n=== Dry Run - Planned Fixes ===\n";
        if (report.tile16_status.corruption_detected) {
          std::cout << absl::StrFormat(
              "  Would zero %zu corrupted tile16 entries\n",
              report.tile16_status.corrupted_addresses.size());
          for (uint32_t addr : report.tile16_status.corrupted_addresses) {
            std::cout << absl::StrFormat("    - 0x%06X\n", addr);
          }
        } else {
          std::cout << "  No fixes needed.\n";
        }
        std::cout << "\nNo changes made (dry run).\n";
      }
      formatter.AddField("dry_run_fixes_planned",
                         static_cast<int>(
                             report.tile16_status.corrupted_addresses.size()));
  } else {
      // Actually apply fixes
      if (report.tile16_status.corruption_detected) {
        RETURN_IF_ERROR(RepairTile16Region(rom, report, false));
        if (!is_json) {
          std::cout << "\n=== Fixes Applied ===\n";
          std::cout << absl::StrFormat("  Zeroed %zu corrupted tile16 entries\n",
                                       report.tile16_status.corrupted_addresses.size());
        }
        formatter.AddField("fixes_applied", true);
        formatter.AddField(
            "tile16_entries_fixed",
            static_cast<int>(report.tile16_status.corrupted_addresses.size()));
      }

      // Save if output path provided
  if (output_path.has_value()) {
    Rom::SaveSettings settings;
    settings.filename = output_path.value();
    RETURN_IF_ERROR(rom->SaveToFile(settings));
        if (!is_json) {
          std::cout << absl::StrFormat("\nSaved fixed ROM to: %s\n",
                                       output_path.value());
        }
        formatter.AddField("output_file", output_path.value());
      } else if (report.HasFixable()) {
        if (!is_json) {
          std::cout << "\nNo output path specified. Use --output <path> to save.\n";
        }
      }
    }
  } else {
    // Not in fix mode - show hint
    if (!is_json && report.HasFixable()) {
      std::cout << "\nTo apply available fixes, run with --fix flag.\n";
      std::cout << "To preview fixes, use --fix --dry-run.\n";
      std::cout << "To save to a new file, use --output <path>.\n";
    }
  }
  
  return absl::OkStatus();
}

}  // namespace yaze::cli
