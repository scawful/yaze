#include "zelda3/dungeon/oracle_rom_safety_preflight.h"

#include <algorithm>
#include <string>

#include "absl/strings/str_format.h"
#include "rom/rom.h"
#include "zelda3/dungeon/custom_collision.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/water_fill_zone.h"

namespace yaze::zelda3 {

namespace {

void AddError(OracleRomSafetyPreflightResult* result, std::string code,
              std::string message, absl::StatusCode status_code,
              int room_id = -1) {
  if (!result) {
    return;
  }
  OracleRomSafetyIssue issue;
  issue.code = std::move(code);
  issue.message = std::move(message);
  issue.status_code = status_code;
  issue.room_id = room_id;
  result->errors.push_back(std::move(issue));
}

}  // namespace

absl::Status OracleRomSafetyPreflightResult::ToStatus() const {
  if (errors.empty()) {
    return absl::OkStatus();
  }
  const auto& first = errors.front();
  if (errors.size() == 1) {
    return absl::Status(first.status_code, first.message);
  }
  return absl::Status(first.status_code,
                      absl::StrFormat("%s (plus %d additional safety issue(s))",
                                      first.message, errors.size() - 1));
}

OracleRomSafetyPreflightResult RunOracleRomSafetyPreflight(
    Rom* rom, const OracleRomSafetyPreflightOptions& options) {
  OracleRomSafetyPreflightResult result;

  if (!rom || !rom->is_loaded()) {
    AddError(&result, "ORACLE_ROM_NOT_LOADED", "ROM not loaded",
             absl::StatusCode::kInvalidArgument);
    return result;
  }

  const std::size_t rom_size = rom->vector().size();

  if (options.require_water_fill_reserved_region &&
      !HasWaterFillReservedRegion(rom_size)) {
    AddError(&result, "ORACLE_WATER_FILL_REGION_MISSING",
             "WaterFill reserved region not present in this ROM",
             absl::StatusCode::kFailedPrecondition);
  }

  if (options.require_custom_collision_write_support &&
      !HasCustomCollisionWriteSupport(rom_size)) {
    AddError(&result, "ORACLE_COLLISION_WRITE_REGION_MISSING",
             "Custom collision write support not present in this ROM",
             absl::StatusCode::kFailedPrecondition);
  }

  if (options.validate_water_fill_table && HasWaterFillReservedRegion(rom_size)) {
    const auto& data = rom->vector();
    const uint8_t zone_count =
        data[static_cast<std::size_t>(kWaterFillTableStart)];
    if (zone_count > 8) {
      AddError(&result, "ORACLE_WATER_FILL_HEADER_CORRUPT",
               absl::StrFormat("WaterFill table header corrupted (zone_count=%u)",
                               zone_count),
               absl::StatusCode::kFailedPrecondition);
    }

    auto zones_or = LoadWaterFillTable(rom);
    if (!zones_or.ok()) {
      AddError(&result, "ORACLE_WATER_FILL_TABLE_INVALID",
               std::string(zones_or.status().message()), zones_or.status().code());
    }
  }

  if (options.validate_custom_collision_maps &&
      HasCustomCollisionWriteSupport(rom_size)) {
    const int max_errors = std::max(1, options.max_collision_errors);
    int collision_errors = 0;
    for (int room_id = 0; room_id < kNumberOfRooms; ++room_id) {
      auto map_or = LoadCustomCollisionMap(rom, room_id);
      if (map_or.ok()) {
        continue;
      }
      AddError(&result, "ORACLE_COLLISION_POINTER_INVALID",
               absl::StrFormat("Room 0x%02X: %s", room_id,
                               std::string(map_or.status().message()).c_str()),
               map_or.status().code(), room_id);
      ++collision_errors;
      if (collision_errors >= max_errors) {
        AddError(&result, "ORACLE_COLLISION_POINTER_INVALID_TRUNCATED",
                 absl::StrFormat("Collision pointer validation stopped after %d "
                                 "errors (increase max_collision_errors to scan more)",
                                 max_errors),
                 absl::StatusCode::kFailedPrecondition);
        break;
      }
    }
  }

  return result;
}

}  // namespace yaze::zelda3
