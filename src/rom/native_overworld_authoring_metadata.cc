#include "rom/native_overworld_authoring_metadata.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <tuple>
#include <utility>

#include "absl/status/status.h"

namespace yaze::rom {

absl::Status ValidateNativeAuthoringSpriteTransition(
    const NativeOverworldAuthoringMetadata& expected,
    const NativeOverworldAuthoringMetadata& proposed,
    uint16_t materialized_count) {
  const auto before =
      EncodeNativeOverworldAuthoringMetadata(expected, materialized_count);
  const auto after =
      EncodeNativeOverworldAuthoringMetadata(proposed, materialized_count);
  if (!before.ok())
    return before.status();
  if (!after.ok())
    return after.status();
  if (proposed.placements.size() < expected.placements.size() ||
      proposed.next_placement_id < expected.next_placement_id)
    return absl::FailedPreconditionError(
        "Native placement identities cannot be removed or reused");
  uint32_t next = expected.next_placement_id;
  for (size_t i = 0; i < proposed.placements.size(); ++i) {
    const auto& value = proposed.placements[i];
    const auto* sprite =
        std::get_if<NativeAuthoringSpritePlacement>(&value.value);
    if (i >= expected.placements.size()) {
      if (!sprite || value.id < next)
        return absl::FailedPreconditionError(
            "New native sprite identities must append monotonically");
      next = value.id + 1;  // Codec proves id < nonzero nextID, so no overflow.
      continue;
    }
    const auto& old = expected.placements[i];
    if (value.id != old.id || value.value.index() != old.value.index())
      return absl::FailedPreconditionError(
          "Native placement order, identity and kind are immutable");
    if (!sprite) {
      if (value != old)
        return absl::FailedPreconditionError(
            "Sprite Save cannot change pending endpoints");
    } else {
      const auto& previous =
          std::get<NativeAuthoringSpritePlacement>(old.value);
      if (sprite->phase != previous.phase ||
          (value.state == NativePlacementState::kDeleted &&
           value.value != old.value))
        return absl::FailedPreconditionError(
            "Native sprite phase and saved tombstone witness must be "
            "preserved");
    }
  }
  return absl::OkStatus();
}

namespace {

constexpr char kMagic[8] = {'O', 'W', 'P', 'L', 'A', 'C', '1', '\0'};

uint16_t Word(const uint8_t* p) {
  return p[0] | (p[1] << 8);
}
uint32_t Long(const uint8_t* p) {
  return Word(p) | (static_cast<uint32_t>(Word(p + 2)) << 16);
}
void PutWord(uint8_t* p, uint16_t value) {
  p[0] = static_cast<uint8_t>(value);
  p[1] = static_cast<uint8_t>(value >> 8);
}
void PutLong(uint8_t* p, uint32_t value) {
  PutWord(p, static_cast<uint16_t>(value));
  PutWord(p + 2, static_cast<uint16_t>(value >> 16));
}
bool Zero(std::span<const uint8_t> bytes) {
  return std::all_of(bytes.begin(), bytes.end(),
                     [](uint8_t byte) { return byte == 0; });
}
absl::Status Invalid() {
  return absl::InvalidArgumentError(
      "Invalid native overworld authoring metadata");
}

}  // namespace

absl::StatusOr<NativeOverworldAuthoringMetadata>
DecodeNativeOverworldAuthoringMetadata(std::span<const uint8_t> bytes,
                                       uint16_t materialized_count) {
  if ((materialized_count != 160 && materialized_count != 192) ||
      bytes.size() != kNativeAuthoringMetadataSize ||
      std::memcmp(bytes.data(), kMagic, 8) != 0 ||
      Word(bytes.data() + 8) != 1 || Word(bytes.data() + 10) != 32 ||
      Word(bytes.data() + 12) != kNativeAuthoringMetadataSlots ||
      Word(bytes.data() + 14) > kNativeAuthoringMetadataSlots ||
      !Zero(bytes.subspan(20, 12))) {
    return Invalid();
  }
  NativeOverworldAuthoringMetadata result;
  result.next_placement_id = Long(bytes.data() + 16);
  if (result.next_placement_id == 0)
    return Invalid();
  const size_t count = Word(bytes.data() + 14);
  if (!Zero(bytes.subspan((count + 1) * kNativeAuthoringMetadataStride)))
    return Invalid();
  std::set<uint32_t> ids;
  std::set<std::tuple<uint8_t, uint8_t, uint8_t>> live_bindings;
  for (size_t i = 0; i < count; ++i) {
    const uint8_t* p = bytes.data() + (i + 1) * kNativeAuthoringMetadataStride;
    NativeAuthoringPlacement placement;
    placement.id = Long(p);
    placement.state = static_cast<NativePlacementState>(p[5]);
    if (placement.id == 0 || placement.id >= result.next_placement_id ||
        !ids.insert(placement.id).second ||
        (placement.state != NativePlacementState::kBound &&
         placement.state != NativePlacementState::kPending &&
         placement.state != NativePlacementState::kDeleted)) {
      return Invalid();
    }
    if (p[4] == 1) {
      if (placement.state == NativePlacementState::kPending || p[6] > 2 ||
          p[7] != 0 || p[8] >= materialized_count || p[9] >= 128 ||
          p[10] == 0xFF || !Zero({p + 13, 19})) {
        return Invalid();
      }
      if (placement.state == NativePlacementState::kBound &&
          !live_bindings.emplace(p[6], p[8], p[9]).second) {
        return Invalid();
      }
      placement.value = NativeAuthoringSpritePlacement{
          p[6], p[8], p[9], {p[10], p[11], p[12]}};
    } else if (p[4] == 2 || p[4] == 3) {
      if (placement.state == NativePlacementState::kBound || p[6] != 0xFF ||
          p[7] > 3 || p[8] >= materialized_count || p[9] >= 32 || p[10] >= 32 ||
          p[11] != 0 || !Zero({p + 16, 16}) ||
          ((p[7] & 1) == 0 && Word(p + 12) != 0) ||
          ((p[7] & 2) == 0 && Word(p + 14) != 0)) {
        return Invalid();
      }
      const auto target =
          (p[7] & 1) ? std::optional<uint16_t>(Word(p + 12)) : std::nullopt;
      const auto row =
          (p[7] & 2) ? std::optional<uint16_t>(Word(p + 14)) : std::nullopt;
      if (p[4] == 2) {
        if ((target && *target > 0xFF) || (row && *row > 0xFF))
          return Invalid();
        const auto entrance =
            target ? std::optional<uint8_t>(static_cast<uint8_t>(*target))
                   : std::nullopt;
        placement.value =
            NativeAuthoringDoorPlacement{p[8], p[9], p[10], entrance, row};
      } else {
        if (row && *row >= 0x4F)
          return Invalid();
        placement.value =
            NativeAuthoringReturnPlacement{p[8], p[9], p[10], target, row};
      }
    } else {
      return Invalid();
    }
    result.placements.push_back(std::move(placement));
  }
  return result;
}

absl::StatusOr<std::array<uint8_t, kNativeAuthoringMetadataSize>>
EncodeNativeOverworldAuthoringMetadata(
    const NativeOverworldAuthoringMetadata& metadata,
    uint16_t materialized_count) {
  if (metadata.placements.size() > kNativeAuthoringMetadataSlots)
    return Invalid();
  std::array<uint8_t, kNativeAuthoringMetadataSize> bytes{};
  std::copy_n(kMagic, 8, bytes.begin());
  PutWord(bytes.data() + 8, 1);
  PutWord(bytes.data() + 10, kNativeAuthoringMetadataStride);
  PutWord(bytes.data() + 12, kNativeAuthoringMetadataSlots);
  PutWord(bytes.data() + 14, static_cast<uint16_t>(metadata.placements.size()));
  PutLong(bytes.data() + 16, metadata.next_placement_id);
  for (size_t i = 0; i < metadata.placements.size(); ++i) {
    const auto& placement = metadata.placements[i];
    uint8_t* p = bytes.data() + (i + 1) * kNativeAuthoringMetadataStride;
    PutLong(p, placement.id);
    p[5] = static_cast<uint8_t>(placement.state);
    if (const auto* sprite =
            std::get_if<NativeAuthoringSpritePlacement>(&placement.value)) {
      p[4] = 1;
      p[6] = sprite->phase;
      p[8] = sprite->owner_map;
      p[9] = sprite->ordinal;
      std::copy(sprite->packed.begin(), sprite->packed.end(), p + 10);
    } else {
      p[6] = 0xFF;
      auto endpoint = [&](const auto& value, auto target, auto row) {
        p[8] = value.map;
        p[9] = value.x;
        p[10] = value.y;
        if (target) {
          p[7] |= 1;
          PutWord(p + 12, *target);
        }
        if (row) {
          p[7] |= 2;
          PutWord(p + 14, *row);
        }
      };
      if (const auto* door =
              std::get_if<NativeAuthoringDoorPlacement>(&placement.value)) {
        p[4] = 2;
        endpoint(*door, door->entrance_id, door->trigger_row);
      } else if (const auto* back = std::get_if<NativeAuthoringReturnPlacement>(
                     &placement.value)) {
        p[4] = 3;
        endpoint(*back, back->room_id, back->return_row);
      } else {
        return Invalid();
      }
    }
  }
  // One validation path for callers and serialized input. No ROM is mutated.
  const auto validated =
      DecodeNativeOverworldAuthoringMetadata(bytes, materialized_count);
  if (!validated.ok())
    return validated.status();
  return bytes;
}

}  // namespace yaze::rom
