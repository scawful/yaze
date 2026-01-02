#include "cli/handlers/tools/graphics_doctor_commands.h"

#include <iostream>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/gfx/util/compression.h"
#include "cli/handlers/tools/diagnostic_types.h"
#include "rom/rom.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace cli {

namespace {

constexpr uint32_t kNumGfxSheets = 223;
constexpr uint32_t kNumMainBlocksets = 37;
constexpr uint32_t kNumRoomBlocksets = 82;
constexpr uint32_t kUncompressedSheetSize = 0x0800;  // 2048 bytes

// Get graphics address for a sheet (adapted from zelda3::GetGraphicsAddress)
uint32_t GetGfxAddress(const uint8_t* data, uint8_t sheet_id, size_t rom_size) {
  uint32_t ptr_base = zelda3::kGfxGroupsPointer;

  if (ptr_base + 0x200 + sheet_id >= rom_size) {
    return 0;
  }

  uint8_t bank = data[ptr_base + sheet_id];
  uint8_t high = data[ptr_base + 0x100 + sheet_id];
  uint8_t low = data[ptr_base + 0x200 + sheet_id];

  // Convert to SNES address then to PC address
  uint32_t snes_addr = (bank << 16) | (high << 8) | low;

  // LoROM conversion: bank * 0x8000 + (addr & 0x7FFF)
  uint32_t pc_addr = ((bank & 0x7F) * 0x8000) + (snes_addr & 0x7FFF);

  return pc_addr;
}

// Validate graphics pointer table
void ValidateGraphicsPointerTable(Rom* rom, DiagnosticReport& report,
                                  std::vector<uint32_t>& valid_addresses) {
  const auto& data = rom->vector();
  uint32_t ptr_base = zelda3::kGfxGroupsPointer;

  if (ptr_base + 0x300 >= rom->size()) {
    DiagnosticFinding finding;
    finding.id = "gfx_ptr_table_missing";
    finding.severity = DiagnosticSeverity::kCritical;
    finding.message = "Graphics pointer table beyond ROM bounds";
    finding.location = absl::StrFormat("0x%06X", ptr_base);
    finding.fixable = false;
    report.AddFinding(finding);
    return;
  }

  int invalid_count = 0;
  for (uint32_t i = 0; i < kNumGfxSheets; ++i) {
    uint32_t addr = GetGfxAddress(data.data(), i, rom->size());

    if (addr == 0 || addr >= rom->size()) {
      if (invalid_count < 10) {
        DiagnosticFinding finding;
        finding.id = "invalid_gfx_ptr";
        finding.severity = DiagnosticSeverity::kError;
        finding.message = absl::StrFormat(
            "Sheet %d has invalid pointer 0x%06X (ROM size: 0x%zX)", i, addr,
            rom->size());
        finding.location = absl::StrFormat("Sheet %d", i);
        finding.fixable = false;
        report.AddFinding(finding);
      }
      invalid_count++;
      valid_addresses.push_back(0);  // Mark as invalid
    } else {
      valid_addresses.push_back(addr);
    }
  }

  if (invalid_count > 0) {
    DiagnosticFinding finding;
    finding.id = "gfx_ptr_summary";
    finding.severity = DiagnosticSeverity::kInfo;
    finding.message =
        absl::StrFormat("Found %d sheets with invalid pointers", invalid_count);
    finding.location = "Graphics Pointer Table";
    finding.fixable = false;
    report.AddFinding(finding);
  }
}

// Test decompression for sheets
void ValidateCompression(Rom* rom, const std::vector<uint32_t>& addresses,
                         DiagnosticReport& report, bool verbose,
                         int& successful_decomp, int& failed_decomp) {
  const auto& data = rom->vector();

  for (uint32_t i = 0; i < kNumGfxSheets; ++i) {
    if (i >= addresses.size() || addresses[i] == 0) {
      failed_decomp++;
      continue;
    }

    uint32_t addr = addresses[i];

    // Try to decompress
    auto result = gfx::lc_lz2::DecompressV2(
        data.data(), addr, kUncompressedSheetSize, 1, rom->size());

    if (!result.ok()) {
      if (verbose || failed_decomp < 10) {
        DiagnosticFinding finding;
        finding.id = "decompression_failed";
        finding.severity = DiagnosticSeverity::kError;
        finding.message =
            absl::StrFormat("Sheet %d decompression failed at 0x%06X: %s", i,
                            addr, std::string(result.status().message()));
        finding.location = absl::StrFormat("Sheet %d", i);
        finding.fixable = false;
        report.AddFinding(finding);
      }
      failed_decomp++;
    } else {
      // Check decompressed size
      if (result->size() != kUncompressedSheetSize) {
        if (verbose || failed_decomp < 10) {
          DiagnosticFinding finding;
          finding.id = "unexpected_sheet_size";
          finding.severity = DiagnosticSeverity::kWarning;
          finding.message = absl::StrFormat(
              "Sheet %d decompressed to %zu bytes (expected %d)", i,
              result->size(), kUncompressedSheetSize);
          finding.location = absl::StrFormat("Sheet %d", i);
          finding.fixable = false;
          report.AddFinding(finding);
        }
      }
      successful_decomp++;
    }
  }
}

// Validate blockset references
void ValidateBlocksets(Rom* rom, DiagnosticReport& report) {
  const auto& data = rom->vector();

  // Main blocksets pointer
  // Main blocksets: 37 sets, 8 bytes each (8 sheet IDs)
  uint32_t main_blockset_ptr = 0x5B57;  // kSpriteBlocksetPointer area

  // For simplicity, we'll check that blockset IDs reference valid sheets
  // The actual blockset table structure varies by ROM version

  int invalid_refs = 0;

  // Check a sample of known blockset-like structures
  // Room blocksets at different locations - simplified check
  uint32_t room_blockset_ptr = 0x50C0;  // Approximate location

  if (room_blockset_ptr + (kNumRoomBlocksets * 4) < rom->size()) {
    for (uint32_t i = 0; i < kNumRoomBlocksets; ++i) {
      for (int slot = 0; slot < 4; ++slot) {
        uint32_t addr = room_blockset_ptr + (i * 4) + slot;
        if (addr >= rom->size())
          break;

        uint8_t sheet_id = data[addr];
        if (sheet_id != 0xFF && sheet_id >= kNumGfxSheets) {
          if (invalid_refs < 20) {
            DiagnosticFinding finding;
            finding.id = "invalid_blockset_ref";
            finding.severity = DiagnosticSeverity::kWarning;
            finding.message = absl::StrFormat(
                "Room blockset %d slot %d references invalid sheet %d", i, slot,
                sheet_id);
            finding.location = absl::StrFormat("Room blockset %d", i);
            finding.fixable = false;
            report.AddFinding(finding);
          }
          invalid_refs++;
        }
      }
    }
  }

  if (invalid_refs > 0) {
    DiagnosticFinding finding;
    finding.id = "blockset_summary";
    finding.severity = DiagnosticSeverity::kInfo;
    finding.message = absl::StrFormat(
        "Found %d invalid blockset sheet references", invalid_refs);
    finding.location = "Blockset Tables";
    finding.fixable = false;
    report.AddFinding(finding);
  }
}

// Check for empty/corrupted sheets
void CheckSheetIntegrity(Rom* rom, const std::vector<uint32_t>& addresses,
                         DiagnosticReport& report, bool verbose,
                         int& empty_sheets, int& suspicious_sheets) {
  const auto& data = rom->vector();

  for (uint32_t i = 0; i < kNumGfxSheets; ++i) {
    if (i >= addresses.size() || addresses[i] == 0) {
      continue;
    }

    uint32_t addr = addresses[i];

    // Try to decompress first
    auto result = gfx::lc_lz2::DecompressV2(
        data.data(), addr, kUncompressedSheetSize, 1, rom->size());

    if (!result.ok())
      continue;

    const auto& sheet_data = *result;

    // Check for all-zeros (empty)
    bool all_zero = true;
    bool all_ff = true;
    for (uint8_t byte : sheet_data) {
      if (byte != 0x00)
        all_zero = false;
      if (byte != 0xFF)
        all_ff = false;
      if (!all_zero && !all_ff)
        break;
    }

    if (all_zero) {
      if (verbose || empty_sheets < 10) {
        DiagnosticFinding finding;
        finding.id = "empty_sheet";
        finding.severity = DiagnosticSeverity::kWarning;
        finding.message = absl::StrFormat("Sheet %d is all zeros (empty)", i);
        finding.location = absl::StrFormat("Sheet %d at 0x%06X", i, addr);
        finding.fixable = false;
        report.AddFinding(finding);
      }
      empty_sheets++;
    } else if (all_ff) {
      if (verbose || suspicious_sheets < 10) {
        DiagnosticFinding finding;
        finding.id = "erased_sheet";
        finding.severity = DiagnosticSeverity::kWarning;
        finding.message =
            absl::StrFormat("Sheet %d is all 0xFF (erased/uninitialized)", i);
        finding.location = absl::StrFormat("Sheet %d at 0x%06X", i, addr);
        finding.suggested_action = "Sheet may need to be restored";
        finding.fixable = false;
        report.AddFinding(finding);
      }
      suspicious_sheets++;
    }
  }
}

}  // namespace

absl::Status GraphicsDoctorCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  bool verbose = parser.HasFlag("verbose");
  bool scan_all = parser.HasFlag("all");

  DiagnosticReport report;

  if (!rom || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }

  // Check for specific sheet
  auto sheet_arg = parser.GetInt("sheet");
  bool single_sheet = sheet_arg.ok();
  int target_sheet = single_sheet ? *sheet_arg : -1;

  if (single_sheet) {
    if (target_sheet < 0 || target_sheet >= static_cast<int>(kNumGfxSheets)) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Sheet ID %d out of range (0-%d)", target_sheet, kNumGfxSheets - 1));
    }
  }

  // 1. Validate graphics pointer table
  std::vector<uint32_t> valid_addresses;
  ValidateGraphicsPointerTable(rom, report, valid_addresses);

  // 2. Test decompression
  int successful_decomp = 0;
  int failed_decomp = 0;

  if (single_sheet) {
    // Just test the one sheet
    if (target_sheet < static_cast<int>(valid_addresses.size()) &&
        valid_addresses[target_sheet] != 0) {
      const auto& data = rom->vector();
      auto result =
          gfx::lc_lz2::DecompressV2(data.data(), valid_addresses[target_sheet],
                                    kUncompressedSheetSize, 1, rom->size());
      if (result.ok()) {
        successful_decomp = 1;
        formatter.AddField("decompressed_size",
                           static_cast<int>(result->size()));
      } else {
        failed_decomp = 1;
      }
    }
  } else if (scan_all || !single_sheet) {
    ValidateCompression(rom, valid_addresses, report, verbose,
                        successful_decomp, failed_decomp);
  }

  // 3. Validate blocksets
  ValidateBlocksets(rom, report);

  // 4. Check sheet integrity
  int empty_sheets = 0;
  int suspicious_sheets = 0;
  CheckSheetIntegrity(rom, valid_addresses, report, verbose, empty_sheets,
                      suspicious_sheets);

  // Output results
  formatter.AddField("total_sheets", static_cast<int>(kNumGfxSheets));
  formatter.AddField("successful_decompressions", successful_decomp);
  formatter.AddField("failed_decompressions", failed_decomp);
  formatter.AddField("empty_sheets", empty_sheets);
  formatter.AddField("suspicious_sheets", suspicious_sheets);
  formatter.AddField("total_findings", report.TotalFindings());
  formatter.AddField("critical_count", report.critical_count);
  formatter.AddField("error_count", report.error_count);
  formatter.AddField("warning_count", report.warning_count);
  formatter.AddField("info_count", report.info_count);

  // JSON findings array
  if (formatter.IsJson()) {
    formatter.BeginArray("findings");
    for (const auto& finding : report.findings) {
      formatter.AddArrayItem(finding.FormatJson());
    }
    formatter.EndArray();
  }

  // Text output
  if (!formatter.IsJson()) {
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════"
                 "═══╗\n";
    std::cout << "║                     GRAPHICS DOCTOR                        "
                 "   ║\n";
    std::cout << "╠════════════════════════════════════════════════════════════"
                 "═══╣\n";
    std::cout << absl::StrFormat("║  Total Sheets: %-46d ║\n",
                                 static_cast<int>(kNumGfxSheets));
    std::cout << absl::StrFormat("║  Successful Decompressions: %-33d ║\n",
                                 successful_decomp);
    std::cout << absl::StrFormat("║  Failed Decompressions: %-37d ║\n",
                                 failed_decomp);
    std::cout << absl::StrFormat("║  Empty Sheets: %-46d ║\n", empty_sheets);
    std::cout << absl::StrFormat("║  Suspicious Sheets: %-41d ║\n",
                                 suspicious_sheets);
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
    } else if (!verbose && report.HasProblems()) {
      std::cout << "\nUse --verbose to see detailed findings.\n";
    }

    if (!report.HasProblems()) {
      std::cout << "\n  \033[1;32mNo critical issues found.\033[0m\n";
    }
    std::cout << "\n";
  }

  return absl::OkStatus();
}

}  // namespace cli
}  // namespace yaze
