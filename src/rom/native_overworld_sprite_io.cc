#include "rom/lyra_layout_descriptor.h"

#include <algorithm>
#include <array>
#include <map>
#include <utility>
#include <vector>

#include "rom/native_overworld_authoring_metadata.h"
#include "rom/native_overworld_storage.h"

namespace yaze::rom {
namespace {
const LyraLayoutEntry& Entry(const LyraLayoutInspection& layout, uint16_t id) {
  return *std::find_if(layout.entries.begin(), layout.entries.end(),
                       [id](const auto& e) { return e.id == id; });
}
uint16_t Word(const std::vector<uint8_t>& bytes, size_t pc) {
  return bytes[pc] | (bytes[pc + 1] << 8);
}
bool Intersects(size_t start, size_t length, size_t other,
                size_t other_length) {
  return start < other + other_length && other < start + length;
}
// The descriptor cannot grant permission to overwrite the original engine's
// shorter tables. Exact sprite pointer mirrors are the only permitted aliases.
absl::Status CheckStorage(const LyraLayoutInspection& layout, size_t start,
                          size_t length, uint16_t id, bool native_mirror) {
  if (id != 0 && Intersects(start, length, 0x4C881, 0x4D62E - 0x4C881))
    return absl::PermissionDeniedError(
        "Native sprite allocation aliases original sprite storage");
  for (const auto& native_ranges :
       {std::span<const internal::NativeOverworldTableStorage>(
            internal::kProperties),
        std::span<const internal::NativeOverworldTableStorage>(
            internal::kOtherNativeStorage)}) {
    for (const auto& native : native_ranges) {
      if (native.native_pc == 0)
        continue;
      const size_t stride =
          native.id >= 500 ? 1 : Entry(layout, native.id).stride;
      const size_t size = native.native_count * stride;
      if (!Intersects(start, length, native.native_pc, size))
        continue;
      if (id != native.id || id < 16 || id > 18 || start != native.native_pc ||
          length != size)
        return absl::PermissionDeniedError(
            "Native sprite allocation aliases another original table");
    }
  }
  for (const auto& other : layout.entries) {
    if (!Intersects(start, length, other.pc_address,
                    other.count * other.stride))
      continue;
    // A mirror may coincide with its own 160-row directory table, but must
    // never overlap a different allocation or a relocated part of that table.
    if (other.id != id ||
        (native_mirror &&
         (start != other.pc_address || length != other.count * other.stride)))
      return absl::PermissionDeniedError(
          "Native sprite storage overlaps another directory allocation");
  }
  return absl::OkStatus();
}

absl::Status ValidateSpritePlacementWitnessForPartition(
    const NativeOverworldAuthoringMetadata& witness,
    const std::array<std::vector<std::vector<uint8_t>>, 3>& lists, size_t first,
    size_t count, uint16_t capacity) {
  for (const auto& placement : witness.placements) {
    if (placement.state != NativePlacementState::kBound)
      continue;
    const auto* sprite =
        std::get_if<NativeAuthoringSpritePlacement>(&placement.value);
    if (!sprite)
      continue;
    if (sprite->phase > 2 || sprite->owner_map >= capacity)
      return absl::FailedPreconditionError(
          "Native placement witness references a map outside the current "
          "sprite partition");
    if (sprite->owner_map < first || sprite->owner_map >= first + count)
      continue;
    const auto& list = lists[sprite->phase][sprite->owner_map - first];
    if (list.empty() || list.back() != 0xFF)
      return absl::FailedPreconditionError(
          "Native sprite stream missing terminator for placement witness");
    const size_t triples = (list.size() - 1) / 3;
    if (sprite->ordinal >= triples)
      return absl::FailedPreconditionError(
          "Native placement ordinal exceeds proposed sprite stream");
    for (int i = 0; i < 3; ++i) {
      if (list[sprite->ordinal * 3 + i] != sprite->packed[i])
        return absl::FailedPreconditionError(
            "Native placement witness disagrees with proposed sprite stream");
    }
  }
  return absl::OkStatus();
}
}  // namespace

absl::StatusOr<std::vector<NativeOverworldSpriteWrite>>
PlanNativeOverworldSpriteSave(
    const std::vector<uint8_t>& baseline,
    std::span<const NativeOverworldSpriteEdit> edits,
    const NativeOverworldAuthoringMetadata* placement_witness) {
  std::vector<NativeOverworldSpriteWrite> plan;
  if (edits.empty() && !placement_witness)
    return plan;
  const auto layout = InspectLyraLayout(baseline.data(), baseline.size());
  if (layout.presence != LyraLayoutPresence::kValid ||
      !IsNativeSpecialOverworldLayout(layout))
    return absl::FailedPreconditionError(
        "Native sprite planning requires a valid native special-overworld "
        "directory");
  if (placement_witness && !layout.has_authoring_metadata)
    return absl::InvalidArgumentError(
        "Native placement witness requires authoring metadata in layout");
  if (placement_witness) {
    const auto encoded = EncodeNativeOverworldAuthoringMetadata(
        *placement_witness, layout.capacity);
    if (!encoded.ok())
      return encoded.status();
    const auto& metadata = Entry(layout, kNativeAuthoringMetadataTableId);
    const auto storage =
        CheckStorage(layout, metadata.pc_address, kNativeAuthoringMetadataSize,
                     metadata.id, false);
    if (!storage.ok())
      return storage;
  }
  std::map<std::pair<uint16_t, uint16_t>, const std::vector<uint8_t>*> changed;
  for (const auto& edit : edits) {
    if (edit.phase >= 3 || edit.map_id >= layout.capacity ||
        edit.value.empty() || edit.value.size() > 128 * 3 + 1 ||
        edit.value.size() % 3 != 1 || edit.value.back() != 0xFF ||
        !changed.emplace(std::pair{edit.phase, edit.map_id}, &edit.value)
             .second)
      return absl::InvalidArgumentError(
          "Invalid or duplicate native sprite edit");
    for (size_t i = 0; i + 1 < edit.value.size(); i += 3)
      if (edit.value[i] == 0xFF)
        return absl::InvalidArgumentError(
            "Native sprite Y byte collides with terminator");
  }
  constexpr std::array<size_t, 3> mirrors = {0x141438, 0x141578, 0x1416B8};
  const size_t original_end = 0x4D62E;
  const auto& actors = Entry(layout, 100);
  for (int partition = 0; partition < (layout.capacity == 192 ? 2 : 1);
       ++partition) {
    const size_t first = partition == 0 ? 0 : 160;
    const size_t count = partition == 0 ? 160 : layout.capacity - 160;
    const size_t begin = partition == 0 ? 0x4C881 : actors.pc_address;
    size_t end = partition == 0 ? original_end : begin + actors.count;
    const int bank_base =
        partition == 0 ? 0x40000
                       : (layout.tail_actor_bank & 0x7F) * 0x8000 - 0x8000;
    if (partition == 0) {
      const auto room_table = Word(baseline, 0x4C298);
      if (room_table >= 0x8000 && 0x40000 + room_table >= begin &&
          0x40000 + room_table < end)
        end = 0x40000 + room_table;
    }
    if (end <= begin || end > baseline.size())
      return absl::FailedPreconditionError(
          "Native sprite storage is empty or truncated");
    std::array<std::vector<std::vector<uint8_t>>, 3> lists;
    bool any_change = false;
    for (int phase = 0; phase < 3; ++phase) {
      const auto& pointers = Entry(layout, 16 + phase);
      if (partition == 0 &&
          !std::equal(baseline.begin() + pointers.pc_address,
                      baseline.begin() + pointers.pc_address + 160 * 2,
                      baseline.begin() + mirrors[phase]))
        return absl::FailedPreconditionError(
            "Native sprite pointer mirrors disagree before Save");
      for (size_t map = first; map < first + count; ++map) {
        const auto pointer = Word(baseline, pointers.pc_address + map * 2);
        size_t at = bank_base + pointer;
        if (pointer < 0x8000 || at < begin || at >= end)
          return absl::FailedPreconditionError(
              "Native sprite pointer outside its declared storage");
        const size_t start = at;
        while (at < end && baseline[at] != 0xFF) {
          if (end - at < 3)
            return absl::FailedPreconditionError(
                "Native sprite stream is truncated");
          at += 3;
        }
        if (at >= end)
          return absl::FailedPreconditionError(
              "Native sprite stream is unterminated");
        std::vector<uint8_t> bytes(baseline.begin() + start,
                                   baseline.begin() + at + 1);
        const auto edit = changed.find({phase, static_cast<uint16_t>(map)});
        if (edit != changed.end()) {
          any_change |= bytes != *edit->second;
          bytes = *edit->second;
        }
        lists[phase].push_back(std::move(bytes));
      }
    }
    if (placement_witness) {
      auto status = ValidateSpritePlacementWitnessForPartition(
          *placement_witness, lists, first, count, layout.capacity);
      if (!status.ok())
        return status;
    }
    if (layout.has_authoring_metadata && any_change && !placement_witness)
      return absl::FailedPreconditionError(
          "Authoring metadata sprite Save requires stable placement-ID "
          "binding");
    if (!any_change)
      continue;
    const auto data_status = CheckStorage(layout, begin, end - begin,
                                          partition == 0 ? 0 : 100, false);
    if (!data_status.ok())
      return data_status;
    std::map<std::vector<uint8_t>, uint16_t> shared;
    std::vector<uint8_t> payload;
    for (int phase = 0; phase < 3; ++phase) {
      const auto& table = Entry(layout, 16 + phase);
      const size_t at = table.pc_address + first * 2;
      const auto status = CheckStorage(layout, at, count * 2, table.id, false);
      if (!status.ok())
        return status;
      std::vector<uint8_t> pointers;
      for (const auto& bytes : lists[phase]) {
        auto found = shared.find(bytes);
        if (found == shared.end()) {
          if (payload.size() + bytes.size() > end - begin)
            return absl::ResourceExhaustedError(
                "Native sprite data exceeds declared storage; no bytes "
                "written");
          const size_t address = begin + payload.size() - bank_base;
          if (address < 0x8000 || address > 0xFFFF)
            return absl::FailedPreconditionError(
                "Native sprite pointer crosses its bank");
          found = shared.emplace(bytes, static_cast<uint16_t>(address)).first;
          payload.insert(payload.end(), bytes.begin(), bytes.end());
        }
        pointers.push_back(static_cast<uint8_t>(found->second));
        pointers.push_back(static_cast<uint8_t>(found->second >> 8));
      }
      if (partition == 0) {
        const auto mirror_status =
            CheckStorage(layout, mirrors[phase], 320, table.id, true);
        if (!mirror_status.ok())
          return mirror_status;
        plan.push_back({static_cast<int>(mirrors[phase]), pointers});
      }
      plan.push_back({static_cast<int>(at), std::move(pointers)});
    }
    plan.push_back({static_cast<int>(begin), std::move(payload)});
  }
  return plan;
}
}  // namespace yaze::rom
