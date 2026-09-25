#include "cli/handlers/tools/graphics_doctor_commands.h"

#include <algorithm>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "cli/handlers/tools/diagnostic_types.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/game_data.h"
#include "zelda3/gfx_sheet_storage.h"

namespace yaze {
namespace cli {

namespace {

constexpr uint32_t kNumGfxSheets = zelda3::kGfxSheetCount;
constexpr uint32_t kNumMainBlocksets = 37;
constexpr uint32_t kNumRoomBlocksets = 82;
constexpr uint32_t kNumSpritesets = 144;
// Spriteset value + 0x73 = graphics sheet id.
constexpr uint32_t kSpriteSheetBase = 0x73;

void AddFinding(DiagnosticReport& report, const std::string& id,
                DiagnosticSeverity severity, const std::string& message,
                const std::string& location,
                const std::string& suggested_action = "") {
  DiagnosticFinding finding;
  finding.id = id;
  finding.severity = severity;
  finding.message = message;
  finding.location = location;
  finding.suggested_action = suggested_action;
  finding.fixable = false;
  report.AddFinding(finding);
}

bool IsSelected(int target_sheet, uint32_t sheet) {
  return target_sheet < 0 || static_cast<uint32_t>(target_sheet) == sheet;
}

// Resolves every sheet through the pointer tables at PC 0x4F80 (bank),
// 0x505F (high) and 0x513E (low), decodes compressed sheets exactly as the
// game does, and checks the decoded size against the game's WRAM buffer.
// Extents are collected for all sheets so overlap checks see the whole ROM.
std::vector<std::optional<zelda3::GfxSheetExtent>> ScanSheets(
    const Rom& rom, DiagnosticReport& report, int target_sheet, bool verbose,
    int& successful, int& failed, int& oversized) {
  std::vector<std::optional<zelda3::GfxSheetExtent>> extents(kNumGfxSheets);
  int reported = 0;
  for (uint32_t sheet = 0; sheet < kNumGfxSheets; ++sheet) {
    auto extent = zelda3::ReadGfxSheetExtent(rom, static_cast<uint16_t>(sheet));
    const bool selected = IsSelected(target_sheet, sheet);
    if (!extent.ok()) {
      if (selected) {
        ++failed;
        if (verbose || reported++ < 10) {
          AddFinding(report, "sheet_unreadable", DiagnosticSeverity::kError,
                     absl::StrFormat("Sheet 0x%02X cannot be read: %s", sheet,
                                     extent.status().message()),
                     absl::StrFormat("Sheet 0x%02X", sheet));
        }
      }
      continue;
    }
    extents[sheet] = *extent;
    if (!selected) {
      continue;
    }
    ++successful;

    const size_t expected =
        extent->kind == zelda3::GfxSheetStorageKind::kCompressed2bpp
            ? zelda3::kGfxSheet2bppBytes
            : zelda3::kGfxSheet3bppBytes;
    if (extent->decoded_size != expected) {
      ++oversized;
      AddFinding(
          report, "sheet_decoded_size", DiagnosticSeverity::kError,
          absl::StrFormat("Sheet 0x%02X (%s) decodes to 0x%zX bytes; the game "
                          "expects 0x%zX",
                          sheet, zelda3::GfxSheetStorageKindName(extent->kind),
                          extent->decoded_size, expected),
          absl::StrFormat("Sheet 0x%02X at 0x%06X", sheet, extent->pc),
          "A longer stream overwrites the next sheet in WRAM; re-save the "
          "sheet with the graphics editor");
    }
  }
  return extents;
}

// Two sheets with the same pointer and length share data (vanilla aliases
// 0x71/0xDD and 0x72/0xDE). Any other overlap means one sheet's stream runs
// into another's.
void CheckSheetOverlaps(
    const std::vector<std::optional<zelda3::GfxSheetExtent>>& extents,
    DiagnosticReport& report, int target_sheet, int& aliased,
    int& overlapping) {
  for (uint32_t a = 0; a < extents.size(); ++a) {
    if (!extents[a].has_value()) {
      continue;
    }
    for (uint32_t b = a + 1; b < extents.size(); ++b) {
      if (!extents[b].has_value() ||
          !(IsSelected(target_sheet, a) || IsSelected(target_sheet, b))) {
        continue;
      }
      const auto& ea = *extents[a];
      const auto& eb = *extents[b];
      const uint64_t a_end = ea.pc + ea.stored_size;
      const uint64_t b_end = eb.pc + eb.stored_size;
      if (!(ea.pc < b_end && eb.pc < a_end)) {
        continue;
      }
      if (ea.pc == eb.pc && ea.stored_size == eb.stored_size) {
        ++aliased;
        AddFinding(report, "sheet_alias", DiagnosticSeverity::kInfo,
                   absl::StrFormat("Sheets 0x%02X and 0x%02X share the same "
                                   "data at 0x%06X",
                                   a, b, ea.pc),
                   absl::StrFormat("0x%06X", ea.pc));
      } else {
        ++overlapping;
        AddFinding(
            report, "sheet_overlap", DiagnosticSeverity::kError,
            absl::StrFormat("Sheet 0x%02X [0x%06X, 0x%06llX) overlaps sheet "
                            "0x%02X [0x%06X, 0x%06llX)",
                            a, ea.pc, static_cast<unsigned long long>(a_end), b,
                            eb.pc, static_cast<unsigned long long>(b_end)),
            absl::StrFormat("0x%06X", std::max(ea.pc, eb.pc)));
      }
    }
  }
}

// Main blocksets (word pointer at kGfxGroupsPointer, 37 x 8 sheet ids), room
// blocksets (kEntranceGfxGroup, 82 x 4 sheet ids) and spritesets
// (kSpriteBlocksetPointer, 144 x 4 values, sheet = value + 0x73) must name
// sheets that exist in the 223-entry pointer tables.
void ValidateGroupTables(const Rom& rom, DiagnosticReport& report,
                         int& invalid_refs) {
  const auto& data = rom.vector();
  int reported = 0;
  auto report_ref = [&](const std::string& group, uint32_t index, int slot,
                        uint32_t pc, uint32_t value, uint32_t sheet) {
    ++invalid_refs;
    if (reported++ < 20) {
      AddFinding(
          report, "invalid_group_sheet_ref", DiagnosticSeverity::kWarning,
          absl::StrFormat("%s %u slot %d value 0x%02X names sheet %u "
                          "(the pointer tables hold %u)",
                          group, index, slot, value, sheet, kNumGfxSheets),
          absl::StrFormat("0x%06X", pc));
    }
  };

  if (static_cast<size_t>(zelda3::kGfxGroupsPointer) + 1 < data.size()) {
    const uint32_t word = data[zelda3::kGfxGroupsPointer] |
                          (data[zelda3::kGfxGroupsPointer + 1] << 8);
    const uint32_t main_pc = SnesToPc(word);
    for (uint32_t i = 0; i < kNumMainBlocksets; ++i) {
      for (int slot = 0; slot < 8; ++slot) {
        const uint32_t pc = main_pc + i * 8 + slot;
        if (pc >= data.size()) {
          break;
        }
        if (data[pc] >= kNumGfxSheets) {
          report_ref("Main blockset", i, slot, pc, data[pc], data[pc]);
        }
      }
    }
  }

  for (uint32_t i = 0; i < kNumRoomBlocksets; ++i) {
    for (int slot = 0; slot < 4; ++slot) {
      const uint32_t pc = zelda3::kEntranceGfxGroup + i * 4 + slot;
      if (pc >= data.size()) {
        break;
      }
      if (data[pc] >= kNumGfxSheets) {
        report_ref("Room blockset", i, slot, pc, data[pc], data[pc]);
      }
    }
  }

  for (uint32_t i = 0; i < kNumSpritesets; ++i) {
    for (int slot = 0; slot < 4; ++slot) {
      const uint32_t pc = zelda3::kSpriteBlocksetPointer + i * 4 + slot;
      if (pc >= data.size()) {
        break;
      }
      const uint32_t sheet = data[pc] + kSpriteSheetBase;
      if (sheet >= kNumGfxSheets) {
        report_ref("Spriteset", i, slot, pc, data[pc], sheet);
      }
    }
  }

  if (invalid_refs > 0) {
    AddFinding(report, "group_ref_summary", DiagnosticSeverity::kInfo,
               absl::StrFormat("Found %d group slot(s) naming a sheet past "
                               "the pointer tables",
                               invalid_refs),
               "Graphics group tables");
  }
}

// Empty sheets are normal (filler and reserved sheets); all-0xFF sheets
// usually mean erased or uninitialized data.
void CheckSheetContents(
    const Rom& rom,
    const std::vector<std::optional<zelda3::GfxSheetExtent>>& extents,
    DiagnosticReport& report, int target_sheet, bool verbose, int& empty_sheets,
    int& suspicious_sheets) {
  for (uint32_t sheet = 0; sheet < extents.size(); ++sheet) {
    if (!extents[sheet].has_value() || !IsSelected(target_sheet, sheet)) {
      continue;
    }
    auto data = zelda3::ReadGfxSheetData(rom, static_cast<uint16_t>(sheet));
    if (!data.ok() || data->empty()) {
      continue;
    }
    const bool all_zero = std::all_of(data->begin(), data->end(),
                                      [](uint8_t b) { return b == 0x00; });
    const bool all_ff = std::all_of(data->begin(), data->end(),
                                    [](uint8_t b) { return b == 0xFF; });
    const std::string location =
        absl::StrFormat("Sheet 0x%02X at 0x%06X", sheet, extents[sheet]->pc);
    if (all_zero) {
      if (verbose || empty_sheets < 10) {
        AddFinding(report, "empty_sheet", DiagnosticSeverity::kInfo,
                   absl::StrFormat("Sheet 0x%02X is all zeros (empty)", sheet),
                   location);
      }
      ++empty_sheets;
    } else if (all_ff) {
      if (verbose || suspicious_sheets < 10) {
        AddFinding(
            report, "erased_sheet", DiagnosticSeverity::kWarning,
            absl::StrFormat("Sheet 0x%02X is all 0xFF (erased/uninitialized)",
                            sheet),
            location, "Sheet may need to be restored");
      }
      ++suspicious_sheets;
    }
  }
}

}  // namespace

absl::Status GraphicsDoctorCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  bool verbose = parser.HasFlag("verbose");
  // Every sheet is always scanned; --all is kept for compatibility.
  (void)parser.HasFlag("all");

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

  // 1-2. Resolve pointers, decode every sheet, check decoded sizes
  int successful_decomp = 0;
  int failed_decomp = 0;
  int oversized_sheets = 0;
  const auto extents =
      ScanSheets(*rom, report, target_sheet, verbose, successful_decomp,
                 failed_decomp, oversized_sheets);
  if (single_sheet && extents[target_sheet].has_value()) {
    formatter.AddField("decoded_size",
                       static_cast<int>(extents[target_sheet]->decoded_size));
    formatter.AddField("stored_size",
                       static_cast<int>(extents[target_sheet]->stored_size));
    formatter.AddField("pc_offset",
                       absl::StrFormat("0x%06X", extents[target_sheet]->pc));
  }

  // 3. Overlapping or aliased sheet data
  int aliased_sheets = 0;
  int overlapping_sheets = 0;
  CheckSheetOverlaps(extents, report, target_sheet, aliased_sheets,
                     overlapping_sheets);

  // 4. Blockset and spriteset references
  int invalid_group_refs = 0;
  if (!single_sheet) {
    ValidateGroupTables(*rom, report, invalid_group_refs);
  }

  // 5. Sheet contents
  int empty_sheets = 0;
  int suspicious_sheets = 0;
  CheckSheetContents(*rom, extents, report, target_sheet, verbose, empty_sheets,
                     suspicious_sheets);

  // Output results
  formatter.AddField("total_sheets", static_cast<int>(kNumGfxSheets));
  formatter.AddField("successful_decompressions", successful_decomp);
  formatter.AddField("failed_decompressions", failed_decomp);
  formatter.AddField("oversized_sheets", oversized_sheets);
  formatter.AddField("overlapping_sheets", overlapping_sheets);
  formatter.AddField("aliased_sheets", aliased_sheets);
  formatter.AddField("invalid_group_refs", invalid_group_refs);
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
