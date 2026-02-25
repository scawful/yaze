#ifndef YAZE_APP_ZELDA3_DUNGEON_ORACLE_ROM_SAFETY_PREFLIGHT_H
#define YAZE_APP_ZELDA3_DUNGEON_ORACLE_ROM_SAFETY_PREFLIGHT_H

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze {
class Rom;
}

namespace yaze::zelda3 {

struct OracleRomSafetyIssue {
  std::string code;
  std::string message;
  absl::StatusCode status_code = absl::StatusCode::kUnknown;
  int room_id = -1;  // -1 when the issue is not room-scoped.
};

struct OracleRomSafetyPreflightResult {
  std::vector<OracleRomSafetyIssue> errors;

  bool ok() const { return errors.empty(); }
  absl::Status ToStatus() const;
};

struct OracleRomSafetyPreflightOptions {
  // Require Oracle's reserved WaterFill region to exist.
  bool require_water_fill_reserved_region = true;
  // Require expanded custom collision pointer/data support.
  bool require_custom_collision_write_support = false;
  // Validate water-fill table header/content consistency.
  bool validate_water_fill_table = true;
  // Validate custom-collision pointers/maps for all rooms.
  bool validate_custom_collision_maps = true;
  // Limit per-room collision map errors to keep reports actionable.
  int max_collision_errors = 8;

  // Room IDs that must have non-empty custom collision data authored.
  //
  // Use this to verify that game-mechanic-critical rooms (e.g. the D3 Kalyxo
  // Castle prison escape room, which requires MinishSwitch collision geometry)
  // have been fully authored before any write path is allowed to proceed.
  //
  // Rooms are checked independently of validate_custom_collision_maps.
  // Errors use code "ORACLE_REQUIRED_ROOM_MISSING_COLLISION".
  std::vector<int> room_ids_requiring_custom_collision;
};

// Run Oracle ROM safety checks used by save/import write paths.
//
// Checks are fail-closed and return structured issues so callers can
// surface detailed diagnostics and machine-readable reports.
OracleRomSafetyPreflightResult RunOracleRomSafetyPreflight(
    Rom* rom, const OracleRomSafetyPreflightOptions& options = {});

// Compute SHA-256 hash of a file, returning the lowercase hex string.
//
// Uses CommonCrypto (CC_SHA256) on macOS or falls back to shelling out
// to platform hash utilities (certutil, sha256sum, or shasum).
absl::StatusOr<std::string> ComputeSha256(const std::string& file_path);

// Verify that the SHA-256 hash of a file matches an expected value.
//
// Returns OK if the hashes match, or a DataLoss status if they differ.
absl::Status VerifySha256(const std::string& file_path,
                          const std::string& expected_hash);

}  // namespace yaze::zelda3

#endif  // YAZE_APP_ZELDA3_DUNGEON_ORACLE_ROM_SAFETY_PREFLIGHT_H
