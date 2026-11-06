#ifndef YAZE_ROM_NATIVE_OVERWORLD_AUTHORING_METADATA_H_
#define YAZE_ROM_NATIVE_OVERWORLD_AUTHORING_METADATA_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>
#include <vector>

#include "absl/status/statusor.h"

namespace yaze::rom {

inline constexpr uint16_t kNativeAuthoringMetadataTableId = 412;
inline constexpr uint8_t kNativeAuthoringMetadataFlag = 0x20;
inline constexpr size_t kNativeAuthoringMetadataStride = 32;
inline constexpr size_t kNativeAuthoringMetadataSlots = 255;
inline constexpr size_t kNativeAuthoringMetadataSize = 256 * 32;

enum class NativePlacementState : uint8_t {
  kBound = 1,
  kPending = 2,
  kDeleted = 3,
};

struct NativeAuthoringSpritePlacement {
  uint8_t phase = 0;
  uint8_t owner_map = 0;
  uint8_t ordinal = 0;
  // Exact runtime list bytes: Y, X, sprite code. This codec validates their
  // shape, not their agreement with a live ROM sprite stream.
  std::array<uint8_t, 3> packed = {};
  bool operator==(const NativeAuthoringSpritePlacement&) const = default;
};

struct NativeAuthoringDoorPlacement {
  uint8_t map = 0;
  uint8_t x = 0;
  uint8_t y = 0;
  std::optional<uint8_t> entrance_id;
  // An assigned entrance-trigger row is distinct from its destination ID.
  std::optional<uint16_t> trigger_row;
  bool operator==(const NativeAuthoringDoorPlacement&) const = default;
};

struct NativeAuthoringReturnPlacement {
  uint8_t map = 0;
  uint8_t x = 0;
  uint8_t y = 0;
  std::optional<uint16_t> room_id;
  std::optional<uint16_t> return_row;
  bool operator==(const NativeAuthoringReturnPlacement&) const = default;
};

struct NativeAuthoringPlacement {
  uint32_t id = 0;
  NativePlacementState state = NativePlacementState::kBound;
  // Variant alternatives encode kind 1=sprite, 2=door, 3=return.
  std::variant<NativeAuthoringSpritePlacement, NativeAuthoringDoorPlacement,
               NativeAuthoringReturnPlacement>
      value;
  bool operator==(const NativeAuthoringPlacement&) const = default;
};

struct NativeOverworldAuthoringMetadata {
  uint32_t next_placement_id = 1;
  // Dense source order, including tombstones. IDs are never reused; deletion
  // retains the typed payload and consumes its slot. No sorting or compaction.
  std::vector<NativeAuthoringPlacement> placements;
  bool operator==(const NativeOverworldAuthoringMetadata&) const = default;
};

// ID412 v1 is exactly 8192 bytes, little endian, with its outer directory CRC.
// Header (32 bytes): "OWPLAC1\0", u16 schema=1, stride=32, slots=255,
// occupied (including tombstones), u32 nextID, then 12 reserved zero bytes.
// Each occupied slot: u32 ID, u8 kind/state/phase/presence, 24-byte payload.
// Sprite: phase 0..2, presence=0; owner, ordinal, packed Y/X/code, 19 zeros.
// Door/return: phase=FF, presence bit0=target, bit1=row; map/x/y/zero,
// u16 target, u16 row, 16 zeros. Absent optional values must encode as zero.
// Door targets fit a byte; trigger rows fit the expanded 256-row table.
// Return rows fit the existing 0x4F-row table. These pending records do not
// enable a runtime entrance/return. Remaining unoccupied slots are all zero.
// Bound sprites and pending endpoints may be tombstoned (state=deleted);
// their identity/payload survives. Live sprite bindings must be unique.
// materialized_count is the validated native map domain (160 or 192).
absl::StatusOr<NativeOverworldAuthoringMetadata>
DecodeNativeOverworldAuthoringMetadata(std::span<const uint8_t> bytes,
                                       uint16_t materialized_count);

absl::StatusOr<std::array<uint8_t, kNativeAuthoringMetadataSize>>
EncodeNativeOverworldAuthoringMetadata(
    const NativeOverworldAuthoringMetadata& metadata,
    uint16_t materialized_count);

}  // namespace yaze::rom

#endif  // YAZE_ROM_NATIVE_OVERWORLD_AUTHORING_METADATA_H_
