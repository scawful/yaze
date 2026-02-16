#include "zelda3/dungeon/oracle_rom_safety_preflight.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#endif

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

namespace {

// Convert a raw digest to a lowercase hex string.
std::string DigestToHex(const unsigned char* digest, std::size_t len) {
  std::string hex;
  hex.reserve(len * 2);
  for (std::size_t i = 0; i < len; ++i) {
    hex += absl::StrFormat("%02x", digest[i]);
  }
  return hex;
}

std::string ExtractHexDigestLine(const std::string& output) {
  std::istringstream lines(output);
  std::string line;
  while (std::getline(lines, line)) {
    std::string compact;
    compact.reserve(line.size());
    for (unsigned char c : line) {
      if (std::isxdigit(c)) {
        compact.push_back(static_cast<char>(std::tolower(c)));
      }
    }
    if (compact.size() == 64) {
      return compact;
    }
  }
  return {};
}

}  // namespace

absl::StatusOr<std::string> ComputeSha256(const std::string& file_path) {
  std::ifstream file(file_path, std::ios::binary);
  if (!file.is_open()) {
    return absl::NotFoundError(
        absl::StrFormat("Cannot open file for hashing: %s", file_path));
  }

#ifdef __APPLE__
  // Use CommonCrypto on macOS (part of libSystem, no extra linking needed).
  CC_SHA256_CTX ctx;
  CC_SHA256_Init(&ctx);

  std::array<char, 8192> buffer;
  while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
    CC_SHA256_Update(&ctx, buffer.data(),
                     static_cast<CC_LONG>(file.gcount()));
  }

  unsigned char digest[CC_SHA256_DIGEST_LENGTH];
  CC_SHA256_Final(digest, &ctx);
  return DigestToHex(digest, CC_SHA256_DIGEST_LENGTH);
#else
  // Fallback: shell out to platform hash utilities.
  file.close();

  std::string command;
#ifdef _WIN32
  command = absl::StrFormat("certutil -hashfile \"%s\" SHA256", file_path);
#else
  // Try sha256sum first (Linux), then shasum -a 256 (BSD/macOS shell envs).
  if (std::system("command -v sha256sum >/dev/null 2>&1") == 0) {
    command = absl::StrFormat("sha256sum '%s'", file_path);
  } else if (std::system("command -v shasum >/dev/null 2>&1") == 0) {
    command = absl::StrFormat("shasum -a 256 '%s'", file_path);
  } else {
    return absl::UnavailableError(
        "Neither sha256sum nor shasum found on this system");
  }
#endif

#ifdef _WIN32
  FILE* pipe = _popen(command.c_str(), "r");
#else
  FILE* pipe = popen(command.c_str(), "r");
#endif
  if (!pipe) {
    return absl::InternalError("Failed to execute hash command");
  }

  std::array<char, 128> result_buf;
  std::string output;
  while (fgets(result_buf.data(), result_buf.size(), pipe) != nullptr) {
    output += result_buf.data();
  }

#ifdef _WIN32
  int status = _pclose(pipe);
#else
  int status = pclose(pipe);
#endif
  if (status != 0) {
    return absl::InternalError(
        absl::StrFormat("Hash command failed with status %d", status));
  }

  const std::string hash = ExtractHexDigestLine(output);
  if (hash.empty()) {
    return absl::InternalError("Unexpected hash command output format");
  }
  return hash;
#endif
}

absl::Status VerifySha256(const std::string& file_path,
                          const std::string& expected_hash) {
  auto actual_hash_or = ComputeSha256(file_path);
  if (!actual_hash_or.ok()) {
    return actual_hash_or.status();
  }

  const std::string& actual_hash = *actual_hash_or;
  if (actual_hash != expected_hash) {
    return absl::DataLossError(absl::StrFormat(
        "SHA-256 mismatch for %s: expected %s, got %s", file_path,
        expected_hash, actual_hash));
  }

  return absl::OkStatus();
}

}  // namespace yaze::zelda3
