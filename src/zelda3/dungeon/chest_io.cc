#include "zelda3/dungeon/room.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "rom/snes.h"
#include "rom/transaction.h"
#include "rom/write_fence.h"
#include "util/macro.h"

namespace yaze::zelda3 {
absl::StatusOr<std::vector<std::pair<uint32_t, uint32_t>>>
GetChestTableWriteRanges(const Rom* rom) {
  if (rom == nullptr || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }
  const auto& rom_data = rom->vector();
  if (kChestsLengthPointer + 1 >= static_cast<int>(rom_data.size()) ||
      kChestsDataPointer1 + 2 >= static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Chest pointers out of range");
  }

  const uint32_t data_pointer =
      (static_cast<uint32_t>(rom_data[kChestsDataPointer1 + 2]) << 16) |
      (static_cast<uint32_t>(rom_data[kChestsDataPointer1 + 1]) << 8) |
      rom_data[kChestsDataPointer1];
  const uint32_t data_pc = SnesToPc(data_pointer);
  if (data_pc > rom_data.size() ||
      static_cast<size_t>(kChestTableCapacityBytes) >
          rom_data.size() - static_cast<size_t>(data_pc)) {
    return absl::OutOfRangeError("Chest data region out of range");
  }
  const uint32_t data_end =
      data_pc + static_cast<uint32_t>(kChestTableCapacityBytes);
  const auto overlaps = [](uint32_t begin, uint32_t end, uint32_t other_begin,
                           uint32_t other_end) {
    return begin < other_end && other_begin < end;
  };
  if (overlaps(data_pc, data_end, kChestsLengthPointer,
               kChestsLengthPointer + 2) ||
      overlaps(data_pc, data_end, kChestsDataPointer1,
               kChestsDataPointer1 + 3)) {
    return absl::FailedPreconditionError(
        "Chest data region overlaps chest metadata operands");
  }

  return std::vector<std::pair<uint32_t, uint32_t>>{
      {static_cast<uint32_t>(kChestsLengthPointer),
       static_cast<uint32_t>(kChestsLengthPointer + 2)},
      {data_pc, data_end},
  };
}

namespace {
struct PhysicalChestRecord {
  uint16_t word = 0;
  uint8_t item = 0;
  uint16_t room_id() const { return word & 0x7FFF; }
  bool operator==(const PhysicalChestRecord&) const = default;
};

// Parse current ROM chest data without grouping or normalizing records.
// `byte_length` is the runtime byte count at kChestsLengthPointer.
std::vector<PhysicalChestRecord> ParsePhysicalRomChests(
    const std::vector<uint8_t>& rom_data, int cpos, int byte_length) {
  std::vector<PhysicalChestRecord> records;
  const int record_count = byte_length / kChestTableRecordSize;
  records.reserve(record_count);
  for (int i = 0; i < record_count; ++i) {
    const int off = cpos + i * kChestTableRecordSize;
    if (off < 0 ||
        off + kChestTableRecordSize > static_cast<int>(rom_data.size())) {
      break;
    }
    const uint16_t word =
        (static_cast<uint16_t>(rom_data[off + 1]) << 8) | rom_data[off];
    records.push_back(PhysicalChestRecord{word, rom_data[off + 2]});
  }
  return records;
}

void AppendChestRecord(std::vector<uint8_t>* bytes, uint16_t word,
                       uint8_t item) {
  bytes->push_back(word & 0xFF);
  bytes->push_back((word >> 8) & 0xFF);
  bytes->push_back(item);
}

void AppendEditedChestRecord(std::vector<uint8_t>* bytes, int room_id,
                             const chest_data& chest) {
  const uint16_t word = static_cast<uint16_t>(room_id) |
                        (chest.size ? static_cast<uint16_t>(0x8000) : 0);
  AppendChestRecord(bytes, word, chest.id);
}

void AppendChangedChestRuns(uint32_t pc, absl::Span<const uint8_t> expected,
                            absl::Span<const uint8_t> replacement,
                            std::vector<ChestWriteRun>* writes) {
  size_t cursor = 0;
  while (cursor < replacement.size()) {
    if (replacement[cursor] == expected[cursor]) {
      ++cursor;
      continue;
    }
    const size_t begin = cursor;
    do {
      ++cursor;
    } while (cursor < replacement.size() &&
             replacement[cursor] != expected[cursor]);

    ChestWriteRun run;
    run.pc = pc + static_cast<uint32_t>(begin);
    run.expected_bytes.assign(expected.begin() + begin,
                              expected.begin() + cursor);
    run.replacement_bytes.assign(replacement.begin() + begin,
                                 replacement.begin() + cursor);
    if (!writes->empty() && writes->back().end() == run.pc) {
      writes->back().expected_bytes.insert(writes->back().expected_bytes.end(),
                                           run.expected_bytes.begin(),
                                           run.expected_bytes.end());
      writes->back().replacement_bytes.insert(
          writes->back().replacement_bytes.end(), run.replacement_bytes.begin(),
          run.replacement_bytes.end());
    } else {
      writes->push_back(std::move(run));
    }
  }
}

void SortAndCoalesceChestRuns(std::vector<ChestWriteRun>* writes) {
  std::sort(writes->begin(), writes->end(),
            [](const ChestWriteRun& lhs, const ChestWriteRun& rhs) {
              return lhs.pc < rhs.pc;
            });
  std::vector<ChestWriteRun> merged;
  merged.reserve(writes->size());
  for (ChestWriteRun& write : *writes) {
    if (!merged.empty() && merged.back().end() == write.pc) {
      merged.back().expected_bytes.insert(merged.back().expected_bytes.end(),
                                          write.expected_bytes.begin(),
                                          write.expected_bytes.end());
      merged.back().replacement_bytes.insert(
          merged.back().replacement_bytes.end(),
          write.replacement_bytes.begin(), write.replacement_bytes.end());
      continue;
    }
    merged.push_back(std::move(write));
  }
  *writes = std::move(merged);
}

std::vector<uint8_t> EncodeChestRoomState(int room_id, const Room& room) {
  std::vector<uint8_t> bytes;
  bytes.reserve(room.GetChests().size() * kChestTableRecordSize);
  for (const chest_data& chest : room.GetChests()) {
    AppendEditedChestRecord(&bytes, room_id, chest);
  }
  return bytes;
}

absl::StatusOr<ChestSavePlan> BuildChestSavePlanImpl(
    const Rom* rom, int room_count,
    const std::function<const Room*(int)>& room_lookup) {
  if (rom == nullptr || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }
  const auto& rom_data = rom->vector();
  if (kChestsLengthPointer + 1 >= static_cast<int>(rom_data.size()) ||
      kChestsDataPointer1 + 2 >= static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Chest pointers out of range");
  }

  ChestSavePlan plan;
  plan.room_limit = std::min(room_count, kNumberOfRooms);
  std::vector<const Room*> dirty_rooms(kNumberOfRooms, nullptr);
  for (int room_id = 0; room_id < plan.room_limit; ++room_id) {
    const Room* room = room_lookup(room_id);
    if (room != nullptr && room->chests_dirty()) {
      dirty_rooms[room_id] = room;
      plan.any_dirty = true;
      plan.dirty_rooms.push_back(
          ChestDirtyRoomState{room_id, EncodeChestRoomState(room_id, *room)});
    }
  }
  if (!plan.any_dirty) {
    return plan;
  }

  ASSIGN_OR_RETURN(auto potential_ranges, GetChestTableWriteRanges(rom));
  plan.data_pc = potential_ranges[1].first;
  std::copy_n(rom_data.begin() + kChestsDataPointer1,
              plan.pointer_operand.size(), plan.pointer_operand.begin());
  plan.original_byte_length =
      static_cast<uint16_t>((rom_data[kChestsLengthPointer + 1] << 8) |
                            rom_data[kChestsLengthPointer]);
  plan.original_capacity_bytes.assign(
      rom_data.begin() + plan.data_pc,
      rom_data.begin() + plan.data_pc + kChestTableCapacityBytes);
  if (plan.original_byte_length > kChestTableCapacityBytes ||
      (plan.original_byte_length % kChestTableRecordSize) != 0) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Chest table byte length %d is invalid (capacity %d)",
        static_cast<int>(plan.original_byte_length), kChestTableCapacityBytes));
  }

  const auto physical_records = ParsePhysicalRomChests(
      rom_data, static_cast<int>(plan.data_pc), plan.original_byte_length);
  if (physical_records.size() !=
      static_cast<size_t>(plan.original_byte_length / kChestTableRecordSize)) {
    return absl::OutOfRangeError("Chest data region is truncated");
  }

  std::vector<size_t> old_counts(kNumberOfRooms, 0);
  for (const PhysicalChestRecord& record : physical_records) {
    if (record.room_id() < kNumberOfRooms) {
      ++old_counts[record.room_id()];
    }
  }

  const size_t old_record_count = physical_records.size();
  size_t final_record_count = old_record_count;
  for (int room_id = 0; room_id < plan.room_limit; ++room_id) {
    if (dirty_rooms[room_id]) {
      final_record_count -= old_counts[room_id];
      final_record_count += dirty_rooms[room_id]->GetChests().size();
    }
  }
  if (final_record_count > static_cast<size_t>(kChestTableCapacityRecords))
    return absl::ResourceExhaustedError(absl::StrFormat(
        "Chest table has %d records; capacity is %d",
        static_cast<int>(final_record_count), kChestTableCapacityRecords));

  using Sequence = std::vector<PhysicalChestRecord>;
  std::vector<Sequence> desired(kNumberOfRooms);
  for (const auto& record : physical_records)
    if (record.room_id() < kNumberOfRooms)
      desired[record.room_id()].push_back(record);
  std::vector<Sequence> edited(kNumberOfRooms);
  for (const auto& state : plan.dirty_rooms) {
    auto& records = edited[state.room_id];
    for (size_t at = 0; at < state.encoded_chests.size(); at += 3)
      records.push_back(
          {static_cast<uint16_t>(state.encoded_chests[at] |
                                 (state.encoded_chests[at + 1] << 8)),
           state.encoded_chests[at + 2]});
    desired[state.room_id] = records;
  }
  // Keep all original slots in place until their room explicitly removes an
  // occurrence. New occurrences retain the existing append-by-room policy.
  std::vector<std::optional<PhysicalChestRecord>> slots;
  std::vector<size_t> seen(kNumberOfRooms, 0);
  for (const auto& record : physical_records) {
    const auto room_id = record.room_id();
    if (room_id >= kNumberOfRooms || !dirty_rooms[room_id]) {
      slots.push_back(record);
      continue;
    }
    const size_t occurrence = seen[room_id]++;
    if (occurrence < edited[room_id].size())
      slots.push_back(edited[room_id][occurrence]);
    else
      slots.push_back(std::nullopt);
  }
  for (const auto& state : plan.dirty_rooms) {
    for (size_t i = seen[state.room_id]; i < edited[state.room_id].size();
         ++i) {
      // Growth may use an explicitly deleted slot when that slot follows every
      // existing occurrence in this room. This preserves ordinal semantics and
      // avoids moving an appended reward ahead of its room's earlier rewards.
      size_t after_last = 0;
      for (size_t at = 0; at < slots.size(); ++at)
        if (slots[at] && slots[at]->room_id() == state.room_id)
          after_last = at + 1;
      auto hole =
          std::find(slots.begin() + after_last, slots.end(), std::nullopt);
      if (hole != slots.end())
        *hole = edited[state.room_id][i];
      else
        slots.push_back(edited[state.room_id][i]);
    }
  }

  // Fill only holes in the final declared prefix. Surviving records beyond
  // that prefix are the donors; deleted tail slots require no physical write.
  // Rightmost holes first also permits safe mixed deletion/growth in one Save.
  size_t donor = slots.size();
  for (size_t cursor = final_record_count; cursor > 0; --cursor) {
    const size_t hole = cursor - 1;
    if (slots[hole])
      continue;
    while (donor > final_record_count && !slots[donor - 1])
      --donor;
    if (donor <= final_record_count)
      return absl::InternalError("Chest tail-fill has no surviving donor");
    --donor;
    if (slots[donor]->room_id() >= kNumberOfRooms)
      return absl::FailedPreconditionError(
          "Cannot relocate an opaque chest record");
    slots[hole] = slots[donor];
    slots[donor].reset();
  }
  for (size_t i = final_record_count; i < slots.size(); ++i)
    if (slots[i])
      return absl::InternalError(
          "Chest tail-fill left an active record beyond the prefix");

  std::vector<Sequence> actual(kNumberOfRooms);
  std::vector<uint8_t> replacement_bytes;
  replacement_bytes.reserve(final_record_count * kChestTableRecordSize);
  for (size_t i = 0; i < final_record_count; ++i) {
    const auto& record = *slots[i];
    if (record.room_id() < kNumberOfRooms)
      actual[record.room_id()].push_back(record);
    else if (i >= physical_records.size() || record != physical_records[i])
      return absl::FailedPreconditionError(
          "Opaque chest records must retain their original slots");
    AppendChestRecord(&replacement_bytes, record.word, record.item);
  }
  if (actual != desired)
    return absl::FailedPreconditionError(
        "Chest tail-fill would change per-room reward order");
  // Identity protection is broader than the actual byte delta. It covers
  // word/type changes, newly activated slots and logical removal past the new
  // length, even when a protected item byte or obsolete tail stays unchanged.
  for (size_t i = 0; i < std::max(old_record_count, final_record_count); ++i) {
    if (i >= final_record_count || i >= old_record_count ||
        slots[i]->word != physical_records[i].word) {
      const uint32_t pc = plan.data_pc + i * kChestTableRecordSize;
      plan.identity_guard_ranges.emplace_back(pc, pc + kChestTableRecordSize);
    }
  }

  const std::array<uint8_t, 2> expected_length = {
      static_cast<uint8_t>(plan.original_byte_length & 0xFF),
      static_cast<uint8_t>((plan.original_byte_length >> 8) & 0xFF)};
  const uint16_t replacement_length =
      static_cast<uint16_t>(replacement_bytes.size());
  const std::array<uint8_t, 2> encoded_replacement_length = {
      static_cast<uint8_t>(replacement_length & 0xFF),
      static_cast<uint8_t>((replacement_length >> 8) & 0xFF)};
  AppendChangedChestRuns(kChestsLengthPointer, expected_length,
                         encoded_replacement_length, &plan.writes);
  AppendChangedChestRuns(plan.data_pc,
                         absl::MakeConstSpan(plan.original_capacity_bytes)
                             .subspan(0, replacement_bytes.size()),
                         replacement_bytes, &plan.writes);
  SortAndCoalesceChestRuns(&plan.writes);
  return plan;
}

}  // namespace

absl::StatusOr<ChestSavePlan> BuildChestSavePlan(
    const Rom* rom, int room_count,
    const std::function<const Room*(int)>& room_lookup) {
  return BuildChestSavePlanImpl(rom, room_count, room_lookup);
}

absl::StatusOr<std::vector<std::pair<uint32_t, uint32_t>>>
GetDirtyChestWriteRanges(const Rom* rom, int room_count,
                         const std::function<const Room*(int)>& room_lookup) {
  ASSIGN_OR_RETURN(ChestSavePlan plan,
                   BuildChestSavePlan(rom, room_count, room_lookup));
  return plan.write_ranges();
}

absl::Status ApplyChestSavePlan(
    Rom* rom, const ChestSavePlan& plan,
    const std::function<const Room*(int)>& room_lookup) {
  if (rom == nullptr || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }
  if (!plan.any_dirty) {
    ASSIGN_OR_RETURN(ChestSavePlan canonical_plan,
                     BuildChestSavePlan(rom, plan.room_limit, room_lookup));
    if (canonical_plan != plan) {
      return absl::FailedPreconditionError(
          "Chest save plan does not match canonical serialization");
    }
    return absl::OkStatus();
  }

  const auto& rom_data = rom->vector();
  if (kChestsLengthPointer + 1 >= static_cast<int>(rom_data.size()) ||
      kChestsDataPointer1 + 2 >= static_cast<int>(rom_data.size()) ||
      plan.data_pc > rom_data.size() ||
      plan.original_capacity_bytes.size() > rom_data.size() - plan.data_pc) {
    return absl::FailedPreconditionError(
        "Chest save plan source is no longer addressable");
  }
  if (!std::equal(plan.pointer_operand.begin(), plan.pointer_operand.end(),
                  rom_data.begin() + kChestsDataPointer1)) {
    return absl::FailedPreconditionError(
        "Chest save plan is stale: data pointer changed");
  }
  const uint16_t current_length =
      static_cast<uint16_t>((rom_data[kChestsLengthPointer + 1] << 8) |
                            rom_data[kChestsLengthPointer]);
  if (current_length != plan.original_byte_length) {
    return absl::FailedPreconditionError(
        "Chest save plan is stale: runtime length changed");
  }
  if (!std::equal(plan.original_capacity_bytes.begin(),
                  plan.original_capacity_bytes.end(),
                  rom_data.begin() + plan.data_pc)) {
    return absl::FailedPreconditionError(
        "Chest save plan is stale: table bytes changed");
  }
  for (const ChestDirtyRoomState& state : plan.dirty_rooms) {
    const Room* room = room_lookup(state.room_id);
    if (room == nullptr || !room->chests_dirty() ||
        EncodeChestRoomState(state.room_id, *room) != state.encoded_chests) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Chest save plan is stale for room 0x%03X", state.room_id));
    }
  }
  ASSIGN_OR_RETURN(ChestSavePlan canonical_plan,
                   BuildChestSavePlan(rom, plan.room_limit, room_lookup));
  if (canonical_plan != plan) {
    return absl::FailedPreconditionError(
        "Chest save plan does not match canonical serialization");
  }

  yaze::ScopedRomTransaction transaction(*rom);
  yaze::rom::WriteFence fence;
  for (const ChestWriteRun& write : plan.writes) {
    RETURN_IF_ERROR(fence.Allow(write.pc, write.end(), "ChestTableExactDelta"));
  }
  yaze::rom::ScopedWriteFence scope(rom, &fence);
  for (const ChestWriteRun& write : plan.writes) {
    RETURN_IF_ERROR(rom->WriteVector(write.pc, write.replacement_bytes));
  }
  for (const ChestDirtyRoomState& state : plan.dirty_rooms) {
    const_cast<Room*>(room_lookup(state.room_id))->ClearChestsDirty();
  }
  transaction.Commit();
  return absl::OkStatus();
}

template <typename RoomLookup>
absl::Status SaveAllChestsImpl(Rom* rom, int room_count,
                               RoomLookup&& room_lookup) {
  const std::function<const Room*(int)> lookup =
      std::forward<RoomLookup>(room_lookup);
  ASSIGN_OR_RETURN(ChestSavePlan plan,
                   BuildChestSavePlan(rom, room_count, lookup));
  return ApplyChestSavePlan(rom, plan, lookup);
}

absl::Status SaveAllChests(Rom* rom, absl::Span<const Room> rooms) {
  return SaveAllChestsImpl(rom, static_cast<int>(rooms.size()),
                           [&rooms](int room_id) { return &rooms[room_id]; });
}

absl::Status SaveAllChests(Rom* rom, int room_count,
                           const std::function<const Room*(int)>& room_lookup) {
  return SaveAllChestsImpl(rom, room_count, room_lookup);
}

}  // namespace yaze::zelda3
