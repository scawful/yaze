#ifndef YAZE_ZELDA3_OVERWORLD_ZSCREAM_CUSTOM_LAYOUT_H_
#define YAZE_ZELDA3_OVERWORLD_ZSCREAM_CUSTOM_LAYOUT_H_

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "rom/rom.h"

namespace yaze::zelda3::zscream {

inline constexpr size_t kVersionMarkerPc = 0x140145;
inline constexpr uint8_t kPerSheetCollisionMarker = 0x05;
inline constexpr size_t kSheetCollisionTablePc = 0x141938;
inline constexpr size_t kSheetCollisionTableSize = 0x2000;
inline constexpr size_t kCollisionSheetCount = 128;
inline constexpr size_t kCollisionsPerSheet = 64;
inline constexpr size_t kResolvedSheetCount = 8;
inline constexpr size_t kTile8CollisionCount = 512;

using SheetCollisions = std::array<uint8_t, kCollisionsPerSheet>;
using ResolvedSheetIds = std::array<uint8_t, kResolvedSheetCount>;
using Tile8Collisions = std::array<uint8_t, kTile8CollisionCount>;

// A raw layout marker is not a game identity, a complete compatibility profile,
// or permission to save. Keep this independent of legacy GetVersion() semantics.
enum class LayoutDialect {
  kUnmarked,
  kVanilla,
  kZSCustomV1,
  kZSCustomV2,
  kZSCustomV3,
  kZScreamV5,
  kUnknown,
};

inline LayoutDialect DetectLayout(const Rom& rom) {
  const auto& bytes = rom.vector();
  if (bytes.size() <= kVersionMarkerPc) {
    return LayoutDialect::kUnmarked;
  }
  switch (bytes[kVersionMarkerPc]) {
    case 0x00:
    case 0xFF:
      return LayoutDialect::kVanilla;
    case 0x01:
      return LayoutDialect::kZSCustomV1;
    case 0x02:
      return LayoutDialect::kZSCustomV2;
    case 0x03:
      return LayoutDialect::kZSCustomV3;
    case kPerSheetCollisionMarker:
      return LayoutDialect::kZScreamV5;
    default:
      return LayoutDialect::kUnknown;
  }
}

inline const char* LayoutName(LayoutDialect dialect) {
  switch (dialect) {
    case LayoutDialect::kUnmarked:
      return "Unmarked layout";
    case LayoutDialect::kVanilla:
      return "Vanilla layout marker";
    case LayoutDialect::kZSCustomV1:
      return "ZSCustomOverworld V1";
    case LayoutDialect::kZSCustomV2:
      return "ZSCustomOverworld V2";
    case LayoutDialect::kZSCustomV3:
      return "ZSCustomOverworld V3";
    case LayoutDialect::kZScreamV5:
      return "ZScream V5 per-sheet collisions";
    case LayoutDialect::kUnknown:
      return "Unknown ZScream layout marker";
  }
  return "Unknown ZScream layout marker";
}

// Necessary marker check only, not proof of free space. V5 places collision
// data in the old tail/function-space region; unknown newer layouts must not
// inherit V3's allocator. Missing markers fail closed in this narrow helper.
inline bool CanUseTailExpansion(const Rom& rom) {
  switch (DetectLayout(rom)) {
    case LayoutDialect::kVanilla:
    case LayoutDialect::kZSCustomV1:
    case LayoutDialect::kZSCustomV2:
    case LayoutDialect::kZSCustomV3:
      return true;
    case LayoutDialect::kUnmarked:
    case LayoutDialect::kZScreamV5:
    case LayoutDialect::kUnknown:
      return false;
  }
  return false;
}

inline absl::Status ValidateSheetCollisionTable(const Rom& rom) {
  if (DetectLayout(rom) != LayoutDialect::kZScreamV5) {
    return absl::FailedPreconditionError(
        "Per-sheet collisions require raw ZScream marker 0x05");
  }
  if (rom.vector().size() < kSheetCollisionTablePc + kSheetCollisionTableSize) {
    return absl::OutOfRangeError(
        "Incomplete ZScream V5 per-sheet collision table");
  }
  return absl::OkStatus();
}

// PC offsets address Rom's header-stripped bytes. Require the complete declared
// table even when the requested early sheet would fit in a truncated image.
inline absl::StatusOr<SheetCollisions> LoadSheetCollisions(const Rom& rom,
                                                           int sheet_id) {
  if (sheet_id < 0 || sheet_id >= static_cast<int>(kCollisionSheetCount)) {
    return absl::InvalidArgumentError("Collision sheet ID must be in [0, 128)");
  }
  const auto status = ValidateSheetCollisionTable(rom);
  if (!status.ok()) {
    return status;
  }
  const size_t offset = kSheetCollisionTablePc +
                        static_cast<size_t>(sheet_id) * kCollisionsPerSheet;
  SheetCollisions collisions{};
  std::copy_n(rom.vector().begin() + offset, kCollisionsPerSheet,
              collisions.begin());
  return collisions;
}

// NewCollisionLoad/.nextSheet reads eight already-resolved PriorSheets IDs.
// (id XBA >> 2) is id*64; 32 words per sheet fill the next 64 output bytes.
// Resolve main/aux/animated sheet selection at the caller; this function neither
// guesses those IDs from a map nor substitutes a vanilla collision table.
inline absl::StatusOr<Tile8Collisions> ComposeTile8Collisions(
    const Rom& rom, const ResolvedSheetIds& sheet_ids) {
  const auto status = ValidateSheetCollisionTable(rom);
  if (!status.ok()) {
    return status;
  }
  Tile8Collisions collisions{};
  for (size_t slot = 0; slot < sheet_ids.size(); ++slot) {
    const auto sheet = LoadSheetCollisions(rom, sheet_ids[slot]);
    if (!sheet.ok()) {
      return sheet.status();
    }
    std::copy(sheet->begin(), sheet->end(),
              collisions.begin() + slot * kCollisionsPerSheet);
  }
  return collisions;
}

static_assert(kCollisionSheetCount * kCollisionsPerSheet ==
              kSheetCollisionTableSize);
static_assert(kResolvedSheetCount * kCollisionsPerSheet ==
              kTile8CollisionCount);

}  // namespace yaze::zelda3::zscream

#endif  // YAZE_ZELDA3_OVERWORLD_ZSCREAM_CUSTOM_LAYOUT_H_
