#include "cli/handlers/tools/rom_doctor_commands.h"

#include <iostream>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/rom.h"
#include "cli/handlers/tools/diagnostic_types.h"

namespace yaze::cli {

namespace {

// ROM header locations (LoROM)
constexpr uint32_t kSnesHeaderBase = 0x7FC0;
constexpr uint32_t kChecksumComplementPos = 0x7FDC;
constexpr uint32_t kChecksumPos = 0x7FDE;

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

  if (rom->size() < kSnesHeaderBase + 32) {
    return info;
  }

  // Read title (21 bytes)
  for (int i = 0; i < 21; ++i) {
    char chr = static_cast<char>(data[kSnesHeaderBase + i]);
    if (chr >= 32 && chr < 127) {
      info.title += chr;
    }
  }

  // Trim trailing spaces
  while (!info.title.empty() && info.title.back() == ' ') {
    info.title.pop_back();
  }

  info.map_mode = data[kSnesHeaderBase + 21];
  info.rom_type = data[kSnesHeaderBase + 22];
  info.rom_size = data[kSnesHeaderBase + 23];
  info.sram_size = data[kSnesHeaderBase + 24];
  info.country = data[kSnesHeaderBase + 25];
  info.license = data[kSnesHeaderBase + 26];
  info.version = data[kSnesHeaderBase + 27];

  // Read checksums
  info.checksum_complement =
      data[kChecksumComplementPos] | (data[kChecksumComplementPos + 1] << 8);
  info.checksum =
      data[kChecksumPos] | (data[kChecksumPos + 1] << 8);

  // Validate checksum (complement XOR checksum should be 0xFFFF)
  info.checksum_valid =
      ((info.checksum_complement ^ info.checksum) == 0xFFFF);

  return info;
}

std::string GetMapModeName(uint8_t mode) {
  switch (mode & 0x0F) {
    case 0x00: return "LoROM";
    case 0x01: return "HiROM";
    case 0x02: return "LoROM + S-DD1";
    case 0x03: return "LoROM + SA-1";
    case 0x05: return "ExHiROM";
    default: return absl::StrFormat("Unknown (0x%02X)", mode);
  }
}

std::string GetCountryName(uint8_t country) {
  switch (country) {
    case 0x00: return "Japan";
    case 0x01: return "USA";
    case 0x02: return "Europe";
    default: return absl::StrFormat("Unknown (0x%02X)", country);
  }
}

void OutputTextBanner(bool is_json) {
  if (is_json) return;
  std::cout << "\n";
  std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
  std::cout << "║                      ROM DOCTOR                               ║\n";
  std::cout << "║         File Integrity & Validation Tool                      ║\n";
  std::cout << "╚═══════════════════════════════════════════════════════════════╝\n";
}

RomFeatures DetectRomFeaturesLocal(Rom* rom) {
  RomFeatures features;

  if (kZSCustomVersionPos < rom->size()) {
    features.zs_custom_version = rom->data()[kZSCustomVersionPos];
    features.is_vanilla =
        (features.zs_custom_version == 0xFF || features.zs_custom_version == 0x00);
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

}  // namespace

absl::Status RomDoctorCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  const bool verbose = parser.HasFlag("verbose");
  const bool is_json = formatter.IsJson();

  OutputTextBanner(is_json);

  DiagnosticReport report;
  report.rom_path = rom->filename();

  // Basic ROM info
  formatter.AddField("rom_path", rom->filename());
  formatter.AddField("size_bytes", static_cast<int>(rom->size()));
  formatter.AddHexField("size_hex", rom->size(), 6);

  // Size validation
  bool size_valid = (rom->size() == kVanillaSize || rom->size() == kExpandedSize);
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
  formatter.AddField("sram_size", header.sram_size > 0 ? (1 << (header.sram_size + 10)) : 0);
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
        "Invalid SNES checksum: complement=0x%04X checksum=0x%04X (XOR=0x%04X, expected 0xFFFF)",
        header.checksum_complement, header.checksum,
        header.checksum_complement ^ header.checksum);
    finding.location = absl::StrFormat("0x%04X", kChecksumComplementPos);
    finding.suggested_action = "ROM may be corrupted or modified without checksum update";
    finding.fixable = true;  // Could be fixed by recalculating
    report.AddFinding(finding);
  }

  // Detect ZSCustomOverworld version
  report.features = DetectRomFeaturesLocal(rom);
  formatter.AddField("zs_custom_version", report.features.GetVersionString());
  formatter.AddField("is_vanilla", report.features.is_vanilla);
  formatter.AddField("expanded_tile16", report.features.has_expanded_tile16);
  formatter.AddField("expanded_tile32", report.features.has_expanded_tile32);
  formatter.AddField("expanded_pointer_tables", report.features.has_expanded_pointer_tables);

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
    std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                    DIAGNOSTIC SUMMARY                         ║\n";
    std::cout << "╠═══════════════════════════════════════════════════════════════╣\n";
    std::cout << absl::StrFormat("║  ROM Title: %-49s ║\n", header.title);
    std::cout << absl::StrFormat("║  Size: 0x%06zX bytes (%zu KB)%-26s ║\n",
                                 rom->size(), rom->size() / 1024, "");
    std::cout << absl::StrFormat("║  Map Mode: %-50s ║\n", GetMapModeName(header.map_mode));
    std::cout << absl::StrFormat("║  Country: %-51s ║\n", GetCountryName(header.country));
    std::cout << "╠═══════════════════════════════════════════════════════════════╣\n";
    std::cout << absl::StrFormat("║  Checksum: 0x%04X (complement: 0x%04X) - %s%-14s ║\n",
                                 header.checksum, header.checksum_complement,
                                 header.checksum_valid ? "VALID" : "INVALID", "");
    std::cout << absl::StrFormat("║  ZSCustomOverworld: %-41s ║\n",
                                 report.features.GetVersionString());
    std::cout << absl::StrFormat("║  Expanded Tile16: %-43s ║\n",
                                 report.features.has_expanded_tile16 ? "YES" : "NO");
    std::cout << absl::StrFormat("║  Expanded Tile32: %-43s ║\n",
                                 report.features.has_expanded_tile32 ? "YES" : "NO");
    std::cout << absl::StrFormat("║  Expanded Ptr Tables: %-39s ║\n",
                                 report.features.has_expanded_pointer_tables ? "YES" : "NO");
    std::cout << "╠═══════════════════════════════════════════════════════════════╣\n";
    std::cout << absl::StrFormat("║  Findings: %d total (%d errors, %d warnings, %d info)%-8s ║\n",
                                 report.TotalFindings(), report.error_count,
                                 report.warning_count, report.info_count, "");
    std::cout << "╚═══════════════════════════════════════════════════════════════╝\n";

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

