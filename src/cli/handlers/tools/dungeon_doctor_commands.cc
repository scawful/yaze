#include "cli/handlers/tools/dungeon_doctor_commands.h"

#include <iostream>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "rom/rom.h"
#include "cli/handlers/tools/diagnostic_types.h"
#include "zelda3/dungeon/dungeon_validator.h"
#include "zelda3/dungeon/room.h"

namespace yaze::cli {

namespace {

// Number of rooms in vanilla ALTTP
constexpr int kNumRooms = 296;

// Room header pointer table location
constexpr uint32_t kRoomHeaderPointer = 0x882D;

struct RoomDiagnostic {
  int room_id = 0;
  bool header_valid = false;
  bool objects_valid = false;
  bool sprites_valid = false;
  int object_count = 0;
  int sprite_count = 0;
  int chest_count = 0;
  std::vector<DiagnosticFinding> findings;

  bool IsValid() const { return header_valid && objects_valid && sprites_valid; }

  std::string FormatJson() const {
    std::string findings_json = "[";
    for (size_t i = 0; i < findings.size(); ++i) {
      if (i > 0) findings_json += ",";
      findings_json += findings[i].FormatJson();
    }
    findings_json += "]";

    return absl::StrFormat(
        R"({"room_id":%d,"header_valid":%s,"objects_valid":%s,"sprites_valid":%s,)"
        R"("object_count":%d,"sprite_count":%d,"chest_count":%d,"findings":%s})",
        room_id, header_valid ? "true" : "false",
        objects_valid ? "true" : "false", sprites_valid ? "true" : "false",
        object_count, sprite_count, chest_count, findings_json);
  }
};

RoomDiagnostic DiagnoseRoom(Rom* rom, int room_id) {
  RoomDiagnostic diag;
  diag.room_id = room_id;

  // Try to load room header
  zelda3::Room room = zelda3::LoadRoomHeaderFromRom(rom, room_id);

  // Check if header loaded correctly
  diag.header_valid = true;  // LoadRoomHeaderFromRom doesn't fail, it just returns empty

  // Load objects
  room.LoadObjects();
  diag.object_count = static_cast<int>(room.GetTileObjects().size());

  // Load sprites
  room.LoadSprites();
  diag.sprite_count = static_cast<int>(room.GetSprites().size());

  // Use DungeonValidator for detailed checks
  zelda3::DungeonValidator validator;
  auto result = validator.ValidateRoom(room);

  diag.objects_valid = result.is_valid;
  diag.sprites_valid = result.is_valid;

  // Convert validation warnings to findings
  for (const auto& warning : result.warnings) {
    DiagnosticFinding finding;
    finding.id = "room_warning";
    finding.severity = DiagnosticSeverity::kWarning;
    finding.message = warning;
    finding.location = absl::StrFormat("Room 0x%02X", room_id);
    finding.fixable = false;
    diag.findings.push_back(finding);
  }

  // Convert validation errors to findings
  for (const auto& error : result.errors) {
    DiagnosticFinding finding;
    finding.id = "room_error";
    finding.severity = DiagnosticSeverity::kError;
    finding.message = error;
    finding.location = absl::StrFormat("Room 0x%02X", room_id);
    finding.fixable = false;
    diag.findings.push_back(finding);
    diag.objects_valid = false;
  }

  // Count chests
  for (const auto& obj : room.GetTileObjects()) {
    if (obj.id_ >= 0xF9 && obj.id_ <= 0xFD) {
      diag.chest_count++;
    }
  }

  return diag;
}

void OutputTextBanner(bool is_json) {
  if (is_json) return;
  std::cout << "\n";
  std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
  std::cout << "║                    DUNGEON DOCTOR                             ║\n";
  std::cout << "║         Room Data Integrity Tool                              ║\n";
  std::cout << "╚═══════════════════════════════════════════════════════════════╝\n";
}

void OutputTextSummary(int total_rooms, int valid_rooms, int warning_rooms,
                       int error_rooms, int total_objects, int total_sprites) {
  std::cout << "\n";
  std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
  std::cout << "║                    DIAGNOSTIC SUMMARY                         ║\n";
  std::cout << "╠═══════════════════════════════════════════════════════════════╣\n";
  std::cout << absl::StrFormat("║  Rooms Analyzed: %-44d ║\n", total_rooms);
  std::cout << absl::StrFormat("║  Valid Rooms: %-47d ║\n", valid_rooms);
  std::cout << absl::StrFormat("║  Rooms with Warnings: %-39d ║\n", warning_rooms);
  std::cout << absl::StrFormat("║  Rooms with Errors: %-41d ║\n", error_rooms);
  std::cout << "╠═══════════════════════════════════════════════════════════════╣\n";
  std::cout << absl::StrFormat("║  Total Objects: %-45d ║\n", total_objects);
  std::cout << absl::StrFormat("║  Total Sprites: %-45d ║\n", total_sprites);
  std::cout << "╚═══════════════════════════════════════════════════════════════╝\n";
}

void CheckUnusedRooms(const std::vector<RoomDiagnostic>& diagnostics,
                      std::vector<DiagnosticFinding>& findings) {
  for (const auto& diag : diagnostics) {
    if (diag.object_count == 0 && diag.sprite_count == 0) {
      DiagnosticFinding finding;
      finding.id = "unused_room";
      finding.severity = DiagnosticSeverity::kInfo;
      finding.message = "Room appears to be empty (0 objects, 0 sprites)";
      finding.location = absl::StrFormat("Room 0x%02X", diag.room_id);
      finding.suggested_action = "Verify if this room is intended to be empty.";
      finding.fixable = false;
      findings.push_back(finding);
    }
  }
}

}  // namespace

absl::Status DungeonDoctorCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  bool verbose = parser.HasFlag("verbose");
  bool all_rooms = parser.HasFlag("all");
  bool deep_scan = parser.HasFlag("deep");
  auto room_id_arg = parser.GetInt("room");
  bool is_json = formatter.IsJson();

  if (deep_scan) all_rooms = true;

  OutputTextBanner(is_json);

  std::vector<RoomDiagnostic> diagnostics;
  int total_objects = 0;
  int total_sprites = 0;
  int valid_rooms = 0;
  int warning_rooms = 0;
  int error_rooms = 0;

  if (room_id_arg.ok()) {
    // Single room mode
    int room_id = room_id_arg.value();
    if (room_id < 0 || room_id >= kNumRooms) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Room ID must be between 0 and %d", kNumRooms - 1));
    }

    auto diag = DiagnoseRoom(rom, room_id);
    diagnostics.push_back(diag);
    total_objects = diag.object_count;
    total_sprites = diag.sprite_count;

    if (diag.IsValid() && diag.findings.empty()) {
      valid_rooms = 1;
    } else {
      bool has_errors = false;
      bool has_warnings = false;
      for (const auto& finding : diag.findings) {
        if (finding.severity == DiagnosticSeverity::kError ||
            finding.severity == DiagnosticSeverity::kCritical) {
          has_errors = true;
        } else if (finding.severity == DiagnosticSeverity::kWarning) {
          has_warnings = true;
        }
      }
      if (has_errors) error_rooms = 1;
      else if (has_warnings) warning_rooms = 1;
      else valid_rooms = 1;
    }

  } else if (all_rooms) {
    // All rooms mode
    if (!is_json) {
      std::cout << "\nAnalyzing all " << kNumRooms << " rooms...\n";
    }

    for (int room_id = 0; room_id < kNumRooms; ++room_id) {
      auto diag = DiagnoseRoom(rom, room_id);
      diagnostics.push_back(diag);
      total_objects += diag.object_count;
      total_sprites += diag.sprite_count;

      bool has_errors = false;
      bool has_warnings = false;
      for (const auto& finding : diag.findings) {
        if (finding.severity == DiagnosticSeverity::kError ||
            finding.severity == DiagnosticSeverity::kCritical) {
          has_errors = true;
        } else if (finding.severity == DiagnosticSeverity::kWarning) {
          has_warnings = true;
        }
      }

      if (has_errors) {
        error_rooms++;
      } else if (has_warnings) {
        warning_rooms++;
      } else {
        valid_rooms++;
      }
    }
  } else {
    // Default: sample key rooms
    std::vector<int> sample_rooms = {0, 1, 2, 3, 4, 5, 6, 7,  // Eastern Palace
                                      32, 33, 34, 35,          // Desert Palace
                                      64, 65, 66, 67,          // Tower of Hera
                                      128, 129, 130};          // Dark rooms

    if (!is_json) {
      std::cout << "\nAnalyzing " << sample_rooms.size() << " sample rooms...\n";
      std::cout << "(Use --all to analyze all " << kNumRooms << " rooms)\n";
    }

    for (int room_id : sample_rooms) {
      if (room_id >= kNumRooms) continue;
      auto diag = DiagnoseRoom(rom, room_id);
      diagnostics.push_back(diag);
      total_objects += diag.object_count;
      total_sprites += diag.sprite_count;

      bool has_errors = false;
      bool has_warnings = false;
      for (const auto& finding : diag.findings) {
        if (finding.severity == DiagnosticSeverity::kError ||
            finding.severity == DiagnosticSeverity::kCritical) {
          has_errors = true;
        } else if (finding.severity == DiagnosticSeverity::kWarning) {
          has_warnings = true;
        }
      }

      if (has_errors) {
        error_rooms++;
      } else if (has_warnings) {
        warning_rooms++;
      } else {
        valid_rooms++;
      }
    }
  }

  // Deep scan analysis
  std::vector<DiagnosticFinding> deep_findings;
  if (deep_scan) {
    CheckUnusedRooms(diagnostics, deep_findings);
  }

  // Output results
  formatter.AddField("total_rooms", static_cast<int>(diagnostics.size()));
  formatter.AddField("valid_rooms", valid_rooms);
  formatter.AddField("warning_rooms", warning_rooms);
  formatter.AddField("error_rooms", error_rooms);
  formatter.AddField("total_objects", total_objects);
  formatter.AddField("total_sprites", total_sprites);

  formatter.BeginArray("rooms");
  for (const auto& diag : diagnostics) {
    if (is_json) {
      formatter.AddArrayItem(diag.FormatJson());
    } else if (verbose || !diag.findings.empty()) {
      // In text mode, show rooms with issues or in verbose mode
      std::string status = diag.IsValid() && diag.findings.empty() ? "OK" : "ISSUES";
      formatter.AddArrayItem(absl::StrFormat(
          "Room 0x%02X: %s (objects=%d, sprites=%d, chests=%d)",
          diag.room_id, status, diag.object_count, diag.sprite_count,
          diag.chest_count));
    }
  }
  formatter.EndArray();

  // Collect all findings
  std::vector<DiagnosticFinding> all_findings;
  for (const auto& diag : diagnostics) {
    for (const auto& finding : diag.findings) {
      all_findings.push_back(finding);
    }
  }
  // Add deep scan findings
  for (const auto& finding : deep_findings) {
    all_findings.push_back(finding);
  }

  formatter.BeginArray("findings");
  for (const auto& finding : all_findings) {
    formatter.AddArrayItem(finding.FormatJson());
  }
  formatter.EndArray();

  // Text summary
  if (!is_json) {
    OutputTextSummary(static_cast<int>(diagnostics.size()), valid_rooms,
                      warning_rooms, error_rooms, total_objects, total_sprites);

    if (!all_findings.empty()) {
      std::cout << "\n=== Issues Found ===\n";
      for (const auto& finding : all_findings) {
        std::cout << "  " << finding.FormatText() << "\n";
      }
    }
  }

  return absl::OkStatus();
}

}  // namespace yaze::cli

