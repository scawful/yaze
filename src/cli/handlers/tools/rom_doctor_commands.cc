#include "cli/handlers/tools/rom_doctor_commands.h"

#include <cmath>
#include <iostream>
#include <numeric>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "app/editor/message/message_data.h"
#include "cli/handlers/tools/diagnostic_types.h"
#include "rom/hm_support.h"
#include "rom/rom.h"
#include "zelda3/dungeon/water_fill_zone.h"

namespace yaze::cli {

namespace {

// ROM header locations (LoROM)
// kSnesHeaderBase, kChecksumComplementPos, kChecksumPos defined in diagnostic_types.h

// Expected sizes
constexpr size_t kVanillaSize = 0x100000;   // 1MB
constexpr size_t kExpandedSize = 0x200000;  // 2MB

struct RomHeaderInfo {
  std::string title;
  uint8_t map_mode = 0;
  uint8_t rom_type = 0;
  uint8_t rom_size = 0;
  uint8_t sram_size = 0;
  uint8_t country = 0;
  uint8_t license = 0;
  uint8_t version = 0;
  uint16_t checksum_complement = 0;
  uint16_t checksum = 0;
  bool checksum_valid = false;
};

RomHeaderInfo ReadRomHeader(Rom* rom) {
  RomHeaderInfo info;
  const auto& data = rom->data();

  if (rom->size() < yaze::cli::kSnesHeaderBase + 32) {
    return info;
  }

  // Read title (21 bytes)
  for (int i = 0; i < 21; ++i) {
    char chr = static_cast<char>(data[yaze::cli::kSnesHeaderBase + i]);
    if (chr >= 32 && chr < 127) {
      info.title += chr;
    }
  }

  // Trim trailing spaces
  while (!info.title.empty() && info.title.back() == ' ') {
    info.title.pop_back();
  }

  info.map_mode = data[yaze::cli::kSnesHeaderBase + 21];
  info.rom_type = data[yaze::cli::kSnesHeaderBase + 22];
  info.rom_size = data[yaze::cli::kSnesHeaderBase + 23];
  info.sram_size = data[yaze::cli::kSnesHeaderBase + 24];
  info.country = data[yaze::cli::kSnesHeaderBase + 25];
  info.license = data[yaze::cli::kSnesHeaderBase + 26];
  info.version = data[yaze::cli::kSnesHeaderBase + 27];

  // Read checksums
  info.checksum_complement = data[yaze::cli::kChecksumComplementPos] |
                             (data[yaze::cli::kChecksumComplementPos + 1] << 8);
  info.checksum =
      data[yaze::cli::kChecksumPos] | (data[yaze::cli::kChecksumPos + 1] << 8);

  // Validate checksum (complement XOR checksum should be 0xFFFF)
  info.checksum_valid = ((info.checksum_complement ^ info.checksum) == 0xFFFF);

  return info;
}

std::string GetMapModeName(uint8_t mode) {
  switch (mode & 0x0F) {
    case 0x00:
      return "LoROM";
    case 0x01:
      return "HiROM";
    case 0x02:
      return "LoROM + S-DD1";
    case 0x03:
      return "LoROM + SA-1";
    case 0x05:
      return "ExHiROM";
    default:
      return absl::StrFormat("Unknown (0x%02X)", mode);
  }
}

std::string GetCountryName(uint8_t country) {
  switch (country) {
    case 0x00:
      return "Japan";
    case 0x01:
      return "USA";
    case 0x02:
      return "Europe";
    default:
      return absl::StrFormat("Unknown (0x%02X)", country);
  }
}

void OutputTextBanner(bool is_json) {
  if (is_json)
    return;
  std::cout << "\n";
  std::cout
      << "╔═══════════════════════════════════════════════════════════════╗\n";
  std::cout
      << "║                      ROM DOCTOR                               ║\n";
  std::cout
      << "║         File Integrity & Validation Tool                      ║\n";
  std::cout
      << "╚═══════════════════════════════════════════════════════════════╝\n";
}

RomFeatures DetectRomFeaturesLocal(Rom* rom) {
  RomFeatures features;

  if (kZSCustomVersionPos < rom->size()) {
    features.zs_custom_version = rom->data()[kZSCustomVersionPos];
    features.is_vanilla = (features.zs_custom_version == 0xFF ||
                           features.zs_custom_version == 0x00);
    features.is_v2 = (!features.is_vanilla && features.zs_custom_version == 2);
    features.is_v3 = (!features.is_vanilla && features.zs_custom_version >= 3);
  } else {
    features.is_vanilla = true;
  }

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

  if (kExpandedPtrTableMarker < rom->size()) {
    features.has_expanded_pointer_tables =
        (rom->data()[kExpandedPtrTableMarker] == kExpandedPtrTableMagic);
  }

  return features;
}

double CalculateEntropy(const uint8_t* data, size_t size) {
  if (size == 0)
    return 0.0;
  std::array<size_t, 256> counts = {0};
  for (size_t i = 0; i < size; ++i) {
    counts[data[i]]++;
  }

  double entropy = 0.0;
  for (size_t count : counts) {
    if (count > 0) {
      double p = static_cast<double>(count) / size;
      entropy -= p * std::log2(p);
    }
  }
  return entropy;
}

void CheckCorruptionHeuristics(Rom* rom, DiagnosticReport& report, bool deep) {
  const auto* data = rom->data();
  size_t size = rom->size();

  // Check known problematic addresses
  for (uint32_t addr : kProblemAddresses) {
    if (addr < size) {
      if (data[addr] == 0x00) {
        DiagnosticFinding finding;
        finding.id = "known_corruption_pattern";
        finding.severity = DiagnosticSeverity::kWarning;
        finding.message = absl::StrFormat(
            "Potential corruption detected at known problematic address 0x%06X",
            addr);
        finding.location = absl::StrFormat("0x%06X", addr);
        finding.suggested_action =
            "Check if this byte should be 0x00. If not, restore from backup.";
        finding.fixable = false;
        report.AddFinding(finding);
      }
    }
  }

  // Check for zero-filled blocks in critical code regions (Bank 00)
  int zero_run = 0;
  for (uint32_t i = 0x0000; i < 0x1000; ++i) {
    if (data[i] == 0x00)
      zero_run++;
    else
      zero_run = 0;

    if (zero_run > 64) {
      DiagnosticFinding finding;
      finding.id = "bank00_erasure";
      finding.severity = DiagnosticSeverity::kCritical;
      finding.message = "Large block of zeros detected in Bank 00 code region";
      finding.location = absl::StrFormat("Around 0x%06X", i);
      finding.suggested_action =
          "ROM is likely corrupted. Restore from backup.";
      finding.fixable = false;
      report.AddFinding(finding);
      break;
    }
  }

  if (deep) {
    // Perform full ROM entropy scan per 32KB bank
    for (uint32_t bank = 0; bank < size / 0x8000; ++bank) {
      double entropy = CalculateEntropy(data + (bank * 0x8000), 0x8000);
      if (entropy < 0.5) {
        DiagnosticFinding finding;
        finding.id = "low_entropy_bank";
        finding.severity = DiagnosticSeverity::kWarning;
        finding.message = absl::StrFormat(
            "Very low entropy (%.2f) detected in Bank %02X. Region might be erased or uninitialized.",
            entropy, bank);
        finding.location = absl::StrFormat("Bank %02X", bank);
        finding.suggested_action = "Verify if this bank should contain data.";
        finding.fixable = false;
        report.AddFinding(finding);
      }
    }

    // Check for pointer chain integrity in overworld maps
    uint32_t high_table =
        report.features.has_expanded_pointer_tables ? kExpandedPtrTableHigh : kPtrTableHighBase;
    uint32_t low_table =
        report.features.has_expanded_pointer_tables ? kExpandedPtrTableLow : kPtrTableLowBase;
    int map_count =
        report.features.has_expanded_pointer_tables ? kExpandedMapCount : kVanillaMapCount;

    if (high_table + map_count < size && low_table + map_count < size) {
      for (int i = 0; i < map_count; ++i) {
        uint8_t high = data[high_table + i];
        uint16_t low = data[low_table + i] | (data[low_table + i + map_count] << 8);
        uint32_t target = (high << 16) | low;

        // LoROM address translation (simplified check)
        uint32_t pc_addr = 0;
        if ((target & 0x7FFF) >= 0 && target < 0xFF0000) {
            pc_addr = ((target & 0x7F0000) >> 1) | (target & 0x7FFF);
        }

        if (pc_addr >= size && target != 0) {
          DiagnosticFinding finding;
          finding.id = "invalid_map_pointer";
          finding.severity = DiagnosticSeverity::kError;
          finding.message = absl::StrFormat(
              "Map %02X points to invalid SNES address 0x%06X", i, target);
          finding.location = absl::StrFormat("Map %02X Pointer", i);
          finding.suggested_action = "Fix the pointer in the overworld editor.";
          finding.fixable = false;
          report.AddFinding(finding);
        }
      }
    }
  }
}

void ValidateExpandedTables(Rom* rom, DiagnosticReport& report) {
  if (!report.features.has_expanded_tile16)
    return;

  const auto* data = rom->data();
  size_t size = rom->size();

  // Check Tile16 expansion region (0x1E8000 - 0x1F0000)
  if (size >= kMap16TilesExpandedEnd) {
    bool all_empty = true;
    for (uint32_t i = kMap16TilesExpanded; i < kMap16TilesExpandedEnd;
         i += 256) {
      if (data[i] != 0xFF && data[i] != 0x00) {
        all_empty = false;
        break;
      }
    }

    if (all_empty) {
      DiagnosticFinding finding;
      finding.id = "empty_expanded_tile16";
      finding.severity = DiagnosticSeverity::kError;
      finding.message =
          "Expanded Tile16 region appears to be empty/uninitialized";
      finding.location = "0x1E8000-0x1F0000";
      finding.suggested_action =
          "Re-save Tile16 data from editor or re-apply expansion patch.";
      finding.fixable = false;
      report.AddFinding(finding);
    }
  }
}

void CheckParallelWorldsHeuristics(Rom* rom, DiagnosticReport& report) {
  // 1. Search for "PARALLEL WORLDS" string in decoded messages
  try {
    std::vector<uint8_t> rom_data_copy = rom->vector();  // Copy for safety
    auto messages = yaze::editor::ReadAllTextData(rom_data_copy.data());

    bool pw_string_found = false;
    for (const auto& msg : messages) {
      if (absl::StrContains(msg.ContentsParsed, "PARALLEL WORLDS") ||
          absl::StrContains(msg.ContentsParsed, "Parallel Worlds")) {
        pw_string_found = true;
        break;
      }
    }

    if (pw_string_found) {
      DiagnosticFinding finding;
      finding.id = "parallel_worlds_string";
      finding.severity = DiagnosticSeverity::kInfo;
      finding.message = "Found 'PARALLEL WORLDS' string in message data";
      finding.location = "Message Data";
      finding.suggested_action = "Confirmed Parallel Worlds ROM.";
      finding.fixable = false;
      report.AddFinding(finding);
    }
  } catch (...) {
    // Ignore parsing errors
  }
}

void CheckZScreamHeuristics(Rom* rom, DiagnosticReport& report) {
  const auto* data = rom->data();
  size_t size = rom->size();

  bool has_zscustom_features = false;
  std::vector<std::string> features_found;

  if (kCustomBGEnabledPos < size && data[kCustomBGEnabledPos] != 0x00 &&
      data[kCustomBGEnabledPos] != 0xFF) {
    has_zscustom_features = true;
    features_found.push_back("Custom BG");
  }
  if (kCustomMainPalettePos < size && data[kCustomMainPalettePos] != 0x00 &&
      data[kCustomMainPalettePos] != 0xFF) {
    has_zscustom_features = true;
    features_found.push_back("Custom Palette");
  }

  if (has_zscustom_features && report.features.is_vanilla) {
    DiagnosticFinding finding;
    finding.id = "zscustom_features_detected";
    finding.severity = DiagnosticSeverity::kInfo;
    finding.message = absl::StrFormat(
        "ZSCustom features detected despite missing version header: %s",
        absl::StrJoin(features_found, ", "));
    finding.location = "ZSCustom Flags";
    finding.suggested_action = "Treat as ZSCustom ROM.";
    finding.fixable = false;
    report.AddFinding(finding);

    report.features.is_vanilla = false;
    report.features.zs_custom_version = 0xFE;  // Unknown/Detected
  }
}

}  // namespace

absl::Status RomDoctorCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  const bool verbose = parser.HasFlag("verbose");
  const bool deep = parser.HasFlag("deep");
  const bool is_json = formatter.IsJson();

  OutputTextBanner(is_json);

  DiagnosticReport report;
  report.rom_path = rom->filename();

  // Basic ROM info
  formatter.AddField("rom_path", rom->filename());
  formatter.AddField("size_bytes", static_cast<int>(rom->size()));
  formatter.AddHexField("size_hex", rom->size(), 6);

  // Size validation
  bool size_valid =
      (rom->size() == kVanillaSize || rom->size() == kExpandedSize);
  formatter.AddField("size_valid", size_valid);

  if (rom->size() == kVanillaSize) {
    formatter.AddField("size_type", "vanilla_1mb");
  } else if (rom->size() == kExpandedSize) {
    formatter.AddField("size_type", "expanded_2mb");
  } else {
    formatter.AddField("size_type", "non_standard");

    DiagnosticFinding finding;
    finding.id = "non_standard_size";
    finding.severity = DiagnosticSeverity::kWarning;
    finding.message = absl::StrFormat(
        "Non-standard ROM size: 0x%zX bytes (expected 0x%zX or 0x%zX)",
        rom->size(), kVanillaSize, kExpandedSize);
    finding.location = "ROM file";
    finding.fixable = false;
    report.AddFinding(finding);
  }

  // Read and validate header
  auto header = ReadRomHeader(rom);

  formatter.AddField("title", header.title);
  formatter.AddField("map_mode", GetMapModeName(header.map_mode));
  formatter.AddHexField("rom_type", header.rom_type, 2);
  formatter.AddField("rom_size_header", 1 << (header.rom_size + 10));
  formatter.AddField("sram_size",
                     header.sram_size > 0 ? (1 << (header.sram_size + 10)) : 0);
  formatter.AddField("country", GetCountryName(header.country));
  formatter.AddField("version", header.version);
  formatter.AddHexField("checksum_complement", header.checksum_complement, 4);
  formatter.AddHexField("checksum", header.checksum, 4);
  formatter.AddField("checksum_valid", header.checksum_valid);

  if (!header.checksum_valid) {
    DiagnosticFinding finding;
    finding.id = "invalid_checksum";
    finding.severity = DiagnosticSeverity::kError;
    finding.message = absl::StrFormat(
        "Invalid SNES checksum: complement=0x%04X checksum=0x%04X (XOR=0x%04X, "
        "expected 0xFFFF)",
        header.checksum_complement, header.checksum,
        header.checksum_complement ^ header.checksum);
    finding.location =
        absl::StrFormat("0x%04X", yaze::cli::kChecksumComplementPos);
    finding.suggested_action =
        "ROM may be corrupted or modified without checksum update";
    finding.fixable = true;  // Could be fixed by recalculating
    report.AddFinding(finding);
  }

  // Detect ZSCustomOverworld version
  report.features = DetectRomFeaturesLocal(rom);
  formatter.AddField("zs_custom_version", report.features.GetVersionString());
  formatter.AddField("is_vanilla", report.features.is_vanilla);
  formatter.AddField("expanded_tile16", report.features.has_expanded_tile16);
  formatter.AddField("expanded_tile32", report.features.has_expanded_tile32);
  formatter.AddField("expanded_pointer_tables",
                     report.features.has_expanded_pointer_tables);

  // Free space analysis (simplified)
  if (rom->size() >= kExpandedSize) {
    // Check for free space in expansion region
    size_t free_bytes = 0;
    for (size_t i = 0x180000; i < 0x1E0000 && i < rom->size(); ++i) {
      if (rom->data()[i] == 0x00 || rom->data()[i] == 0xFF) {
        free_bytes++;
      }
    }
    formatter.AddField("free_space_estimate", static_cast<int>(free_bytes));
    formatter.AddField("free_space_region", "0x180000-0x1E0000");

    DiagnosticFinding finding;
    finding.id = "free_space_info";
    finding.severity = DiagnosticSeverity::kInfo;
    finding.message = absl::StrFormat(
        "Estimated free space in expansion region: %zu bytes (%.1f KB)",
        free_bytes, free_bytes / 1024.0);
    finding.location = "0x180000-0x1E0000";
    finding.fixable = false;
    report.AddFinding(finding);
  }

  // 3. Check for corruption heuristics
  CheckCorruptionHeuristics(rom, report, deep);

  // 4. Hyrule Magic / Parallel Worlds Analysis
  yaze::rom::HyruleMagicValidator hm_validator(rom);
  if (hm_validator.IsParallelWorlds()) {
    DiagnosticFinding finding;
    finding.id = "parallel_worlds_detected";
    finding.severity = DiagnosticSeverity::kInfo;
    finding.message = "Parallel Worlds (1.5MB) detected (Header check)";
    finding.location = "ROM Header";
    finding.suggested_action =
        "Use z3ed for editing. Custom pointer tables are supported.";
    finding.fixable = false;
    report.AddFinding(finding);
  } else if (hm_validator.HasBank00Erasure()) {
    DiagnosticFinding finding;
    finding.id = "hm_corruption_detected";
    finding.severity = DiagnosticSeverity::kCritical;
    finding.message = "Hyrule Magic corruption detected (Bank 00 erasure)";
    finding.location = "Bank 00";
    finding.suggested_action = "ROM is likely unstable. Restore from backup.";
    finding.fixable = false;
    report.AddFinding(finding);
  }

  // Advanced Heuristics
  CheckParallelWorldsHeuristics(rom, report);
  CheckZScreamHeuristics(rom, report);

  // 5. Validate expanded tables
  ValidateExpandedTables(rom, report);

  // 6. Oracle of Secrets: validate WaterFill reserved region integrity.
  //
  // This is a ROM safety check for editor-authored water fill zones which
  // reserve a tail region inside the expanded custom collision bank. If custom
  // collision data overlaps that reserved tail, the ROM layout is incompatible
  // with the WaterFill table format and yaze must not attempt to use it.
  {
    auto zones_or = yaze::zelda3::LoadWaterFillTable(rom);
    if (!zones_or.ok()) {
      DiagnosticFinding finding;
      finding.id = "water_fill_table_invalid";
      finding.severity = DiagnosticSeverity::kWarning;
      finding.message = absl::StrFormat("WaterFill table parse failed: %s",
                                        zones_or.status().message());
      finding.location = "Custom collision bank ($13:xxxx)";
      finding.suggested_action =
          "Restore from a known-good ROM or fix custom collision layout. "
          "This must be resolved before using WaterFill authoring.";
      finding.fixable = false;
      report.AddFinding(finding);
    } else if (verbose) {
      formatter.AddField("water_fill_zone_count",
                         static_cast<int>(zones_or.value().size()));
    }
  }

  // Output findings
  formatter.BeginArray("findings");
  for (const auto& finding : report.findings) {
    formatter.AddArrayItem(finding.FormatJson());
  }
  formatter.EndArray();

  // Summary
  formatter.AddField("total_findings", report.TotalFindings());
  formatter.AddField("critical_count", report.critical_count);
  formatter.AddField("error_count", report.error_count);
  formatter.AddField("warning_count", report.warning_count);
  formatter.AddField("info_count", report.info_count);
  formatter.AddField("has_problems", report.HasProblems());

  // Text output
  if (!is_json) {
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════"
                 "═══╗\n";
    std::cout << "║                    DIAGNOSTIC SUMMARY                      "
                 "   ║\n";
    std::cout << "╠════════════════════════════════════════════════════════════"
                 "═══╣\n";
    std::cout << absl::StrFormat("║  ROM Title: %-49s ║\n", header.title);
    std::cout << absl::StrFormat("║  Size: 0x%06zX bytes (%zu KB)%-26s ║\n",
                                 rom->size(), rom->size() / 1024, "");
    std::cout << absl::StrFormat("║  Map Mode: %-50s ║\n",
                                 GetMapModeName(header.map_mode));
    std::cout << absl::StrFormat("║  Country: %-51s ║\n",
                                 GetCountryName(header.country));
    std::cout << "╠════════════════════════════════════════════════════════════"
                 "═══╣\n";
    std::cout << absl::StrFormat(
        "║  Checksum: 0x%04X (complement: 0x%04X) - %s%-14s ║\n",
        header.checksum, header.checksum_complement,
        header.checksum_valid ? "VALID" : "INVALID", "");
    std::cout << absl::StrFormat("║  ZSCustomOverworld: %-41s ║\n",
                                 report.features.GetVersionString());
    std::cout << absl::StrFormat(
        "║  Expanded Tile16: %-43s ║\n",
        report.features.has_expanded_tile16 ? "YES" : "NO");
    std::cout << absl::StrFormat(
        "║  Expanded Tile32: %-43s ║\n",
        report.features.has_expanded_tile32 ? "YES" : "NO");
    std::cout << absl::StrFormat(
        "║  Expanded Ptr Tables: %-39s ║\n",
        report.features.has_expanded_pointer_tables ? "YES" : "NO");
    std::cout << "╠════════════════════════════════════════════════════════════"
                 "═══╣\n";
    std::cout << absl::StrFormat(
        "║  Findings: %d total (%d errors, %d warnings, %d info)%-8s ║\n",
        report.TotalFindings(), report.error_count, report.warning_count,
        report.info_count, "");
    std::cout << "╚════════════════════════════════════════════════════════════"
                 "═══╝\n";

    if (verbose && !report.findings.empty()) {
      std::cout << "\n=== Detailed Findings ===\n";
      for (const auto& finding : report.findings) {
        std::cout << "  " << finding.FormatText() << "\n";
      }
    }
  }

  return absl::OkStatus();
}

}  // namespace yaze::cli
