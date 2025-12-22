#include "cli/handlers/tools/sprite_doctor_commands.h"

#include <iostream>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "cli/handlers/tools/diagnostic_types.h"
#include "rom/rom.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace cli {

namespace {

// Validate sprite pointer table entries
void ValidateSpritePointerTable(Rom* rom, DiagnosticReport& report,
                                bool verbose) {
  const auto& data = rom->vector();

  // Check sprite pointers for all 296 rooms
  int invalid_count = 0;
  for (int room = 0; room < zelda3::kNumberOfRooms; ++room) {
    uint32_t ptr_addr =
        zelda3::kRoomsSpritePointer + (room * 2);

    if (ptr_addr + 1 >= rom->size()) {
      DiagnosticFinding finding;
      finding.id = "sprite_ptr_out_of_bounds";
      finding.severity = DiagnosticSeverity::kCritical;
      finding.message = absl::StrFormat(
          "Sprite pointer table address 0x%06X is beyond ROM size", ptr_addr);
      finding.location = absl::StrFormat("Room %d", room);
      finding.fixable = false;
      report.AddFinding(finding);
      return;
    }

    // Read 2-byte pointer (little endian)
    uint16_t ptr = data[ptr_addr] | (data[ptr_addr + 1] << 8);

    // Pointers point into Bank 09 (0x090000)
    uint32_t sprite_addr = 0x090000 + ptr;

    // Validate pointer points to valid sprite data region
    if (sprite_addr < zelda3::kSpritesData ||
        sprite_addr >= zelda3::kSpritesEndData) {
      if (verbose || invalid_count < 10) {
        DiagnosticFinding finding;
        finding.id = "invalid_sprite_ptr";
        finding.severity = DiagnosticSeverity::kWarning;
        finding.message = absl::StrFormat(
            "Room %d sprite pointer 0x%04X -> 0x%06X outside valid range "
            "(0x%06X-0x%06X)",
            room, ptr, sprite_addr, zelda3::kSpritesData,
            zelda3::kSpritesEndData);
        finding.location = absl::StrFormat("0x%06X", ptr_addr);
        finding.fixable = false;
        report.AddFinding(finding);
      }
      invalid_count++;
    }
  }

  if (invalid_count > 0) {
    DiagnosticFinding finding;
    finding.id = "sprite_ptr_summary";
    finding.severity = DiagnosticSeverity::kInfo;
    finding.message = absl::StrFormat(
        "Found %d rooms with potentially invalid sprite pointers", invalid_count);
    finding.location = "Sprite Pointer Table";
    finding.fixable = false;
    report.AddFinding(finding);
  }
}

// Validate spriteset graphics references
void ValidateSpritesets(Rom* rom, DiagnosticReport& report) {
  const auto& data = rom->vector();

  // Spriteset table at kSpriteBlocksetPointer
  // 144 spritesets, 4 bytes each (4 graphics sheet references)
  uint32_t spriteset_addr = zelda3::kSpriteBlocksetPointer;
  constexpr int kNumSpritesets = 144;
  constexpr int kNumGfxSheets = 223;

  int invalid_refs = 0;

  for (int set = 0; set < kNumSpritesets; ++set) {
    for (int slot = 0; slot < 4; ++slot) {
      uint32_t addr = spriteset_addr + (set * 4) + slot;
      if (addr >= rom->size()) {
        DiagnosticFinding finding;
        finding.id = "spriteset_addr_out_of_bounds";
        finding.severity = DiagnosticSeverity::kError;
        finding.message = absl::StrFormat(
            "Spriteset %d address 0x%06X beyond ROM size", set, addr);
        finding.location = absl::StrFormat("Spriteset %d", set);
        finding.fixable = false;
        report.AddFinding(finding);
        return;
      }

      uint8_t sheet_id = data[addr];
      // 0xFF is valid (empty slot), other values should be < 223
      if (sheet_id != 0xFF && sheet_id >= kNumGfxSheets) {
        if (invalid_refs < 20) {
          DiagnosticFinding finding;
          finding.id = "invalid_spriteset_sheet";
          finding.severity = DiagnosticSeverity::kWarning;
          finding.message = absl::StrFormat(
              "Spriteset %d slot %d references invalid sheet %d (max: %d)",
              set, slot, sheet_id, kNumGfxSheets - 1);
          finding.location =
              absl::StrFormat("Spriteset %d slot %d", set, slot);
          finding.fixable = false;
          report.AddFinding(finding);
        }
        invalid_refs++;
      }
    }
  }

  if (invalid_refs > 0) {
    DiagnosticFinding finding;
    finding.id = "spriteset_summary";
    finding.severity = DiagnosticSeverity::kInfo;
    finding.message = absl::StrFormat(
        "Found %d invalid spriteset sheet references", invalid_refs);
    finding.location = "Spriteset Table";
    finding.fixable = false;
    report.AddFinding(finding);
  }
}

// Validate sprite data for a specific room
void ValidateRoomSprites(Rom* rom, int room_id, DiagnosticReport& report,
                         int& total_sprites, int& empty_rooms) {
  const auto& data = rom->vector();

  // Get sprite pointer for this room
  uint32_t ptr_addr = zelda3::kRoomsSpritePointer + (room_id * 2);
  if (ptr_addr + 1 >= rom->size()) return;

  uint16_t ptr = data[ptr_addr] | (data[ptr_addr + 1] << 8);
  uint32_t sprite_addr = 0x090000 + ptr;

  // Empty room check
  if (sprite_addr == zelda3::kSpritesDataEmptyRoom) {
    empty_rooms++;
    return;
  }

  if (sprite_addr >= rom->size()) return;

  // Read sort byte
  if (sprite_addr >= rom->size()) return;
  // uint8_t sort_byte = data[sprite_addr];
  sprite_addr++;

  // Parse sprites (3 bytes each, terminated by 0xFF)
  int room_sprite_count = 0;
  const int kMaxSpritesPerRoom = 32;  // Reasonable limit

  while (sprite_addr < rom->size() && room_sprite_count < kMaxSpritesPerRoom) {
    uint8_t y_pos = data[sprite_addr];
    if (y_pos == 0xFF) break;  // Terminator

    if (sprite_addr + 2 >= rom->size()) {
      DiagnosticFinding finding;
      finding.id = "truncated_sprite_data";
      finding.severity = DiagnosticSeverity::kError;
      finding.message = absl::StrFormat(
          "Room %d sprite data truncated at 0x%06X", room_id, sprite_addr);
      finding.location = absl::StrFormat("Room %d", room_id);
      finding.fixable = false;
      report.AddFinding(finding);
      break;
    }

    // uint8_t x_subtype = data[sprite_addr + 1];
    uint8_t sprite_id = data[sprite_addr + 2];

    // Check for potentially invalid sprite IDs
    // Valid sprite IDs are typically 0x00-0xF2
    // IDs above 0xF2 are special or invalid
    if (sprite_id > 0xF2) {
      // Some overlord sprites use high IDs, but many are invalid
      // This is informational only
    }

    sprite_addr += 3;
    room_sprite_count++;
    total_sprites++;
  }

  // Check if we hit the max without finding a terminator
  if (room_sprite_count >= kMaxSpritesPerRoom) {
    DiagnosticFinding finding;
    finding.id = "sprite_count_limit";
    finding.severity = DiagnosticSeverity::kWarning;
    finding.message = absl::StrFormat(
        "Room %d has %d+ sprites (hit scan limit)", room_id, kMaxSpritesPerRoom);
    finding.location = absl::StrFormat("Room %d", room_id);
    finding.fixable = false;
    report.AddFinding(finding);
  }
}

// Check for common sprite issues
void CheckCommonSpriteIssues(Rom* rom, DiagnosticReport& report) {
  const auto& data = rom->vector();

  // Check for zeroed sprite pointer table (corruption sign)
  int zero_pointers = 0;
  for (int room = 0; room < zelda3::kNumberOfRooms; ++room) {
    uint32_t ptr_addr = zelda3::kRoomsSpritePointer + (room * 2);
    if (ptr_addr + 1 >= rom->size()) break;

    uint16_t ptr = data[ptr_addr] | (data[ptr_addr + 1] << 8);
    if (ptr == 0x0000) {
      zero_pointers++;
    }
  }

  if (zero_pointers > 50) {  // More than ~17% zero is suspicious
    DiagnosticFinding finding;
    finding.id = "many_zero_sprite_ptrs";
    finding.severity = DiagnosticSeverity::kWarning;
    finding.message = absl::StrFormat(
        "Found %d rooms with zero sprite pointers (possible corruption)",
        zero_pointers);
    finding.location = "Sprite Pointer Table";
    finding.suggested_action = "Verify ROM integrity";
    finding.fixable = false;
    report.AddFinding(finding);
  }
}

}  // namespace

absl::Status SpriteDoctorCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  bool verbose = parser.HasFlag("verbose");
  bool scan_all = parser.HasFlag("all");

  DiagnosticReport report;

  if (!rom || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }

  // Check for specific room
  auto room_arg = parser.GetInt("room");
  bool single_room = room_arg.ok();
  int target_room = single_room ? *room_arg : -1;

  if (single_room) {
    if (target_room < 0 || target_room >= zelda3::kNumberOfRooms) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Room ID %d out of range (0-%d)", target_room,
          zelda3::kNumberOfRooms - 1));
    }
  }

  // 1. Validate sprite pointer table
  ValidateSpritePointerTable(rom, report, verbose);

  // 2. Validate spriteset references
  ValidateSpritesets(rom, report);

  // 3. Validate room sprites
  int total_sprites = 0;
  int empty_rooms = 0;
  int rooms_scanned = 0;

  if (single_room) {
    ValidateRoomSprites(rom, target_room, report, total_sprites, empty_rooms);
    rooms_scanned = 1;
  } else if (scan_all) {
    for (int room = 0; room < zelda3::kNumberOfRooms; ++room) {
      ValidateRoomSprites(rom, room, report, total_sprites, empty_rooms);
      rooms_scanned++;
    }
  } else {
    // Sample rooms: first 20, some middle, some end
    std::vector<int> sample_rooms;
    for (int i = 0; i < 20; ++i) sample_rooms.push_back(i);
    for (int i = 100; i < 110; ++i) sample_rooms.push_back(i);
    for (int i = 200; i < 210; ++i) sample_rooms.push_back(i);

    for (int room : sample_rooms) {
      if (room < zelda3::kNumberOfRooms) {
        ValidateRoomSprites(rom, room, report, total_sprites, empty_rooms);
        rooms_scanned++;
      }
    }
  }

  // 4. Check common issues
  CheckCommonSpriteIssues(rom, report);

  // Output results
  formatter.AddField("rooms_scanned", rooms_scanned);
  formatter.AddField("total_sprites", total_sprites);
  formatter.AddField("empty_rooms", empty_rooms);
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
    std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                      SPRITE DOCTOR                            ║\n";
    std::cout << "╠═══════════════════════════════════════════════════════════════╣\n";
    std::cout << absl::StrFormat("║  Rooms Scanned: %-45d ║\n", rooms_scanned);
    std::cout << absl::StrFormat("║  Total Sprites Found: %-39d ║\n",
                                 total_sprites);
    std::cout << absl::StrFormat("║  Empty Rooms: %-47d ║\n", empty_rooms);
    std::cout << "╠═══════════════════════════════════════════════════════════════╣\n";
    std::cout << absl::StrFormat(
        "║  Findings: %d total (%d errors, %d warnings, %d info)%-8s ║\n",
        report.TotalFindings(), report.error_count, report.warning_count,
        report.info_count, "");
    std::cout << "╚═══════════════════════════════════════════════════════════════╝\n";

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
