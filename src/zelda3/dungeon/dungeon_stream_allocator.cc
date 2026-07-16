#include "zelda3/dungeon/dungeon_stream_allocator.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "rom/write_fence.h"
#include "util/macro.h"
#include "util/rom_hash.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"

namespace yaze::zelda3 {
namespace {

constexpr uint32_t kLoRomBankSize = 0x8000;
constexpr uint32_t kWramMappedPcBegin = 0x3F0000;
constexpr uint32_t kWramMappedPcEnd = 0x400000;

bool IntersectsWramMappedPc(uint32_t begin, uint32_t end) {
  return begin < kWramMappedPcEnd && kWramMappedPcBegin < end;
}

uint32_t PointerWidth(DungeonPointerEncoding encoding) {
  return encoding == DungeonPointerEncoding::kLong24 ? 3u : 2u;
}

bool Contains(const DungeonStreamPcRange& outer,
              const DungeonStreamPcRange& inner) {
  return outer.begin <= inner.begin && inner.end <= outer.end;
}

bool Intersects(const DungeonStreamPcRange& a, const DungeonStreamPcRange& b) {
  return a.begin < b.end && b.begin < a.end;
}

bool ContainsAddress(const DungeonStreamPcRange& range, uint32_t address) {
  return range.begin <= address && address < range.end;
}

absl::StatusOr<std::vector<DungeonStreamPcRange>> NormalizeRanges(
    const std::vector<DungeonStreamPcRange>& input, uint32_t rom_size,
    const char* label, bool allow_empty) {
  if (input.empty() && !allow_empty) {
    return absl::InvalidArgumentError(
        absl::StrCat(label, " must not be empty"));
  }

  std::vector<DungeonStreamPcRange> ranges = input;
  std::sort(ranges.begin(), ranges.end(), [](const auto& a, const auto& b) {
    return std::tie(a.begin, a.end) < std::tie(b.begin, b.end);
  });
  std::vector<DungeonStreamPcRange> normalized;
  for (const auto& range : ranges) {
    if (range.begin >= range.end) {
      return absl::InvalidArgumentError(
          absl::StrFormat("%s contains empty/reversed range [0x%06X,0x%06X)",
                          label, range.begin, range.end));
    }
    if (range.end > rom_size) {
      return absl::OutOfRangeError(
          absl::StrFormat("%s range [0x%06X,0x%06X) exceeds ROM size 0x%06X",
                          label, range.begin, range.end, rom_size));
    }
    if (IntersectsWramMappedPc(range.begin, range.end)) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "%s range [0x%06X,0x%06X) maps through SNES WRAM banks $7E/$7F",
          label, range.begin, range.end));
    }
    if (!normalized.empty() && range.begin < normalized.back().end) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "%s ranges overlap at PC 0x%06X", label, range.begin));
    }
    if (!normalized.empty() && range.begin == normalized.back().end) {
      normalized.back().end = range.end;
    } else {
      normalized.push_back(range);
    }
  }
  return normalized;
}

absl::Status ValidateAllocationSubset(
    const std::vector<DungeonStreamPcRange>& data_ranges,
    const std::vector<DungeonStreamPcRange>& allocation_ranges) {
  for (const auto& allocation : allocation_ranges) {
    const bool contained = std::any_of(
        data_ranges.begin(), data_ranges.end(),
        [&](const auto& data) { return Contains(data, allocation); });
    if (!contained) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Allocation range [0x%06X,0x%06X) is not contained in one data "
          "range",
          allocation.begin, allocation.end));
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<uint32_t> ReadWordAt(const Rom& rom, uint32_t address) {
  auto result = rom.ReadWord(static_cast<int>(address));
  if (!result.ok()) {
    return result.status();
  }
  return static_cast<uint32_t>(*result);
}

absl::StatusOr<uint32_t> ReadLongAt(const Rom& rom, uint32_t address) {
  auto result = rom.ReadLong(static_cast<int>(address));
  if (!result.ok()) {
    return result.status();
  }
  return *result;
}

absl::Status ValidateKnownPointerTableSource(
    const Rom& rom, const DungeonStreamLayout& layout) {
  switch (layout.kind) {
    case DungeonStreamKind::kObject: {
      if (layout.pointer_encoding != DungeonPointerEncoding::kLong24) {
        return absl::InvalidArgumentError(
            "Object streams require 24-bit long pointers");
      }
      auto raw = ReadLongAt(rom, kRoomObjectPointer);
      if (!raw.ok()) {
        return raw.status();
      }
      if ((*raw & 0xFFFFu) < 0x8000u ||
          SnesToPc(*raw) != layout.pointer_table_pc) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "Object pointer-table source at PC 0x%06X does not resolve to "
            "configured table PC 0x%06X",
            kRoomObjectPointer, layout.pointer_table_pc));
      }
      return absl::OkStatus();
    }
    case DungeonStreamKind::kSprite: {
      if (layout.pointer_encoding != DungeonPointerEncoding::kFixedBank16 ||
          layout.pointer_bank != 0x09) {
        return absl::InvalidArgumentError(
            "Sprite streams require fixed-bank-16 pointers in bank 0x09");
      }
      auto raw = ReadWordAt(rom, kRoomsSpritePointer);
      if (!raw.ok()) {
        return raw.status();
      }
      if (*raw < 0x8000u ||
          SnesToPc((static_cast<uint32_t>(layout.pointer_bank) << 16) | *raw) !=
              layout.pointer_table_pc) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "Sprite pointer-table source at PC 0x%06X does not resolve to "
            "configured table PC 0x%06X",
            kRoomsSpritePointer, layout.pointer_table_pc));
      }
      return absl::OkStatus();
    }
    case DungeonStreamKind::kPotItem:
      if (layout.pointer_encoding != DungeonPointerEncoding::kFixedBank16 ||
          layout.pointer_bank != 0x01) {
        return absl::InvalidArgumentError(
            "Pot-item streams require fixed-bank-16 pointers in bank 0x01");
      }
      if (layout.pointer_table_pc != kRoomItemsPointers) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "Pot-item pointer table must be the fixed PC table at 0x%06X",
            kRoomItemsPointers));
      }
      return absl::OkStatus();
  }
  return absl::InvalidArgumentError("Unknown dungeon stream kind");
}

DungeonStreamPcRange KnownPointerSourceRange(DungeonStreamKind kind) {
  switch (kind) {
    case DungeonStreamKind::kObject:
      return {kRoomObjectPointer, kRoomObjectPointer + 3};
    case DungeonStreamKind::kSprite:
      return {kRoomsSpritePointer, kRoomsSpritePointer + 2};
    case DungeonStreamKind::kPotItem:
      // Pot-item pointers use a fixed table rather than an indirection source.
      return {kRoomItemsPointers, kRoomItemsPointers};
  }
  return {};
}

absl::StatusOr<DungeonStreamLayout> ValidateAndNormalizeLayout(
    const Rom& rom, const DungeonStreamLayout& input) {
  if (!rom.is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }
  if (rom.size() > std::numeric_limits<uint32_t>::max()) {
    return absl::OutOfRangeError("ROM is too large for PC-native layout");
  }
  if (input.pointer_count == 0 || input.pointer_count > kNumberOfRooms) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Pointer count %u is outside supported range 1..%d",
                        input.pointer_count, kNumberOfRooms));
  }

  DungeonStreamLayout layout = input;
  const uint32_t rom_size = static_cast<uint32_t>(rom.size());
  auto data_ranges =
      NormalizeRanges(input.data_ranges, rom_size, "data_ranges", false);
  if (!data_ranges.ok()) {
    return data_ranges.status();
  }
  auto allocation_ranges = NormalizeRanges(input.allocation_ranges, rom_size,
                                           "allocation_ranges", true);
  if (!allocation_ranges.ok()) {
    return allocation_ranges.status();
  }
  layout.data_ranges = std::move(*data_ranges);
  layout.allocation_ranges = std::move(*allocation_ranges);

  auto allocation_status =
      ValidateAllocationSubset(layout.data_ranges, layout.allocation_ranges);
  if (!allocation_status.ok()) {
    return allocation_status;
  }

  const uint32_t pointer_width = PointerWidth(layout.pointer_encoding);
  const uint64_t table_end =
      static_cast<uint64_t>(layout.pointer_table_pc) +
      static_cast<uint64_t>(layout.pointer_count) * pointer_width;
  if (table_end > rom_size) {
    return absl::OutOfRangeError(
        "Dungeon stream pointer table is out of range");
  }
  const DungeonStreamPcRange pointer_table_range{
      layout.pointer_table_pc, static_cast<uint32_t>(table_end)};
  if (IntersectsWramMappedPc(pointer_table_range.begin,
                             pointer_table_range.end)) {
    return absl::InvalidArgumentError(
        "Dungeon stream pointer table maps through SNES WRAM banks $7E/$7F");
  }
  if (layout.pointer_encoding == DungeonPointerEncoding::kFixedBank16) {
    const uint32_t pointer_table_word =
        PcToSnes(layout.pointer_table_pc) & 0xFFFFu;
    const uint64_t runtime_table_end =
        static_cast<uint64_t>(pointer_table_word) +
        static_cast<uint64_t>(layout.pointer_count) * pointer_width;
    if (runtime_table_end > 0x10000u) {
      return absl::InvalidArgumentError(
          "Fixed-bank-16 pointer table crosses its runtime CPU bank");
    }
  }

  const DungeonStreamPcRange pointer_source_range =
      KnownPointerSourceRange(layout.kind);
  if (pointer_source_range.begin < pointer_source_range.end) {
    if (pointer_source_range.end > rom_size) {
      return absl::OutOfRangeError(
          "Live pointer-table source is outside the source ROM");
    }
    if (Intersects(pointer_table_range, pointer_source_range)) {
      return absl::InvalidArgumentError(
          "Dungeon stream pointer table must not overlap the live "
          "pointer-table source");
    }
  }

  DungeonStreamPcRange door_pointer_range;
  if (layout.kind == DungeonStreamKind::kObject) {
    const uint64_t door_pointer_end =
        static_cast<uint64_t>(kDoorPointers) +
        static_cast<uint64_t>(kNumberOfRooms) * 3u;
    if (door_pointer_end > rom_size) {
      return absl::OutOfRangeError(
          "Complete object door-pointer table is outside the source ROM");
    }
    door_pointer_range = {kDoorPointers,
                          static_cast<uint32_t>(door_pointer_end)};
    if (Intersects(pointer_table_range, door_pointer_range)) {
      return absl::InvalidArgumentError(
          "Dungeon stream pointer table must not overlap the object "
          "door-pointer table");
    }
  }

  for (const auto& data : layout.data_ranges) {
    if (Intersects(pointer_table_range, data)) {
      return absl::InvalidArgumentError(
          "Dungeon stream data ranges must not overlap the pointer table");
    }
    if (pointer_source_range.begin < pointer_source_range.end &&
        Intersects(pointer_source_range, data)) {
      return absl::InvalidArgumentError(
          "Dungeon stream data ranges must not overlap the live "
          "pointer-table source");
    }
    if (door_pointer_range.begin < door_pointer_range.end &&
        Intersects(door_pointer_range, data)) {
      return absl::InvalidArgumentError(
          "Dungeon stream data ranges must not overlap the object "
          "door-pointer table");
    }
  }

  auto source_status = ValidateKnownPointerTableSource(rom, layout);
  if (!source_status.ok()) {
    return source_status;
  }
  return layout;
}

absl::StatusOr<uint32_t> DecodePointer(const Rom& rom,
                                       const DungeonStreamLayout& layout,
                                       uint32_t room_id) {
  const uint32_t width = PointerWidth(layout.pointer_encoding);
  const uint32_t slot = layout.pointer_table_pc + room_id * width;
  uint32_t snes = 0;
  if (layout.pointer_encoding == DungeonPointerEncoding::kLong24) {
    auto raw = ReadLongAt(rom, slot);
    if (!raw.ok()) {
      return raw.status();
    }
    snes = *raw;
  } else {
    auto raw = ReadWordAt(rom, slot);
    if (!raw.ok()) {
      return raw.status();
    }
    snes = (static_cast<uint32_t>(layout.pointer_bank) << 16) | *raw;
  }
  if ((snes & 0xFFFFu) < 0x8000u) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Pointer word 0x%04X is not a LoROM address", snes & 0xFFFFu));
  }
  const uint8_t bank = static_cast<uint8_t>((snes >> 16) & 0xFFu);
  if (bank == 0x7E || bank == 0x7F || bank == 0xFE || bank == 0xFF) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Pointer uses SNES WRAM bank or mirror 0x%02X", bank));
  }
  const uint32_t pc = SnesToPc(snes);
  if (pc >= rom.size()) {
    return absl::OutOfRangeError(
        absl::StrFormat("Pointer resolves outside ROM at PC 0x%06X", pc));
  }
  return pc;
}

const DungeonStreamPcRange* FindContainingRange(
    const std::vector<DungeonStreamPcRange>& ranges, uint32_t address) {
  auto it =
      std::upper_bound(ranges.begin(), ranges.end(), address,
                       [](uint32_t value, const DungeonStreamPcRange& range) {
                         return value < range.begin;
                       });
  if (it == ranges.begin()) {
    return nullptr;
  }
  --it;
  return ContainsAddress(*it, address) ? &*it : nullptr;
}

absl::StatusOr<uint32_t> ParseObjectStream(std::span<const uint8_t> bytes,
                                           uint32_t start, uint32_t limit,
                                           uint32_t* door_list_pc = nullptr) {
  if (start + 2 > limit) {
    return absl::DataLossError("Object stream is missing its two-byte header");
  }
  uint32_t cursor = start + 2;
  for (int list = 0; list < 2; ++list) {
    while (true) {
      if (cursor + 2 > limit) {
        return absl::DataLossError(
            absl::StrFormat("Object list %d has no 0xFFFF terminator", list));
      }
      if (bytes[cursor] == 0xFF && bytes[cursor + 1] == 0xFF) {
        cursor += 2;
        break;
      }
      if (cursor + 3 > limit) {
        return absl::DataLossError(
            absl::StrFormat("Object list %d has a truncated record", list));
      }
      cursor += 3;
    }
  }

  bool in_doors = false;
  while (true) {
    if (cursor + 2 > limit) {
      return absl::DataLossError(in_doors ? "Object door list has no terminator"
                                          : "Object list 2 has no door marker");
    }
    if (!in_doors) {
      if (bytes[cursor] == 0xF0 && bytes[cursor + 1] == 0xFF) {
        cursor += 2;
        if (door_list_pc != nullptr) {
          *door_list_pc = cursor;
        }
        in_doors = true;
        continue;
      }
      if (bytes[cursor] == 0xFF && bytes[cursor + 1] == 0xFF) {
        // Canonical vanilla/OOS empty streams may terminate the third list
        // directly without a 0xFFF0 marker. Their door pointer targets this
        // final terminator.
        if (door_list_pc != nullptr) {
          *door_list_pc = cursor;
        }
        return cursor + 2;
      }
      if (cursor + 3 > limit) {
        return absl::DataLossError("Object list 2 has a truncated record");
      }
      cursor += 3;
      continue;
    }

    if (bytes[cursor] == 0xFF && bytes[cursor + 1] == 0xFF) {
      return cursor + 2;
    }
    cursor += 2;
  }
}

absl::StatusOr<uint32_t> ParseSpriteStream(std::span<const uint8_t> bytes,
                                           uint32_t start, uint32_t limit) {
  if (start >= limit) {
    return absl::DataLossError("Sprite stream is missing its sort byte");
  }
  uint32_t cursor = start + 1;
  while (cursor < limit) {
    if (bytes[cursor] == 0xFF) {
      return cursor + 1;
    }
    if (cursor + 3 > limit) {
      return absl::DataLossError("Sprite stream has a truncated record");
    }
    cursor += 3;
  }
  return absl::DataLossError("Sprite stream has no 0xFF terminator");
}

absl::StatusOr<uint32_t> ParsePotItemStream(std::span<const uint8_t> bytes,
                                            uint32_t start, uint32_t limit) {
  uint32_t cursor = start;
  while (true) {
    if (cursor + 2 > limit) {
      return absl::DataLossError("Pot-item stream has no 0xFFFF terminator");
    }
    if (bytes[cursor] == 0xFF && bytes[cursor + 1] == 0xFF) {
      return cursor + 2;
    }
    if (cursor + 3 > limit) {
      return absl::DataLossError("Pot-item stream has a truncated record");
    }
    cursor += 3;
  }
}

absl::StatusOr<uint32_t> ParseStream(std::span<const uint8_t> bytes,
                                     DungeonStreamKind kind, uint32_t start,
                                     uint32_t limit) {
  if (limit > bytes.size() || start >= limit) {
    return absl::OutOfRangeError("Dungeon stream parse bounds are invalid");
  }
  switch (kind) {
    case DungeonStreamKind::kObject:
      return ParseObjectStream(bytes, start, limit);
    case DungeonStreamKind::kSprite:
      return ParseSpriteStream(bytes, start, limit);
    case DungeonStreamKind::kPotItem:
      return ParsePotItemStream(bytes, start, limit);
  }
  return absl::InvalidArgumentError("Unknown dungeon stream kind");
}

absl::Status ValidateCompleteEncodedStream(
    DungeonStreamKind kind, const std::vector<uint8_t>& encoded) {
  if (encoded.empty()) {
    return absl::InvalidArgumentError("Replacement stream must not be empty");
  }
  auto logical_end =
      ParseStream(encoded, kind, 0, static_cast<uint32_t>(encoded.size()));
  if (!logical_end.ok()) {
    return logical_end.status();
  }
  if (*logical_end != encoded.size()) {
    return absl::InvalidArgumentError(
        "Replacement stream contains bytes after its logical terminator");
  }
  return absl::OkStatus();
}

absl::StatusOr<uint32_t> ObjectDoorListOffset(
    const std::vector<uint8_t>& encoded) {
  uint32_t door_list_pc = std::numeric_limits<uint32_t>::max();
  auto logical_end = ParseObjectStream(
      encoded, 0, static_cast<uint32_t>(encoded.size()), &door_list_pc);
  if (!logical_end.ok()) {
    return logical_end.status();
  }
  if (*logical_end != encoded.size() || door_list_pc >= encoded.size()) {
    return absl::DataLossError(
        "Object stream does not have a valid door-list target");
  }
  return door_list_pc;
}

std::vector<DungeonStreamPcRange> UnionIntervals(
    std::vector<DungeonStreamPcRange> intervals) {
  std::sort(intervals.begin(), intervals.end(),
            [](const auto& a, const auto& b) {
              return std::tie(a.begin, a.end) < std::tie(b.begin, b.end);
            });
  std::vector<DungeonStreamPcRange> result;
  for (const auto& interval : intervals) {
    if (result.empty() || interval.begin > result.back().end) {
      result.push_back(interval);
    } else {
      result.back().end = std::max(result.back().end, interval.end);
    }
  }
  return result;
}

std::vector<DungeonStreamPcRange> ComplementIntervals(
    const std::vector<DungeonStreamPcRange>& containers,
    const std::vector<DungeonStreamPcRange>& occupied) {
  std::vector<DungeonStreamPcRange> free;
  for (const auto& container : containers) {
    uint32_t cursor = container.begin;
    for (const auto& used : occupied) {
      if (used.end <= cursor) {
        continue;
      }
      if (used.begin >= container.end) {
        break;
      }
      if (used.begin > cursor) {
        free.push_back({cursor, std::min(used.begin, container.end)});
      }
      cursor = std::max(cursor, std::min(used.end, container.end));
      if (cursor >= container.end) {
        break;
      }
    }
    if (cursor < container.end) {
      free.push_back({cursor, container.end});
    }
  }
  return free;
}

absl::StatusOr<std::vector<uint8_t>> EncodePointer(
    const DungeonStreamLayout& layout, uint32_t pc) {
  if (pc >= kWramMappedPcBegin && pc < kWramMappedPcEnd) {
    return absl::OutOfRangeError(absl::StrFormat(
        "PC address 0x%06X maps through SNES WRAM banks $7E/$7F", pc));
  }
  const uint32_t snes = PcToSnes(pc);
  if (SnesToPc(snes) != pc || (snes & 0xFFFFu) < 0x8000u) {
    return absl::OutOfRangeError(absl::StrFormat(
        "PC address 0x%06X is not representable as a LoROM pointer", pc));
  }
  if (layout.pointer_encoding == DungeonPointerEncoding::kLong24) {
    return std::vector<uint8_t>{static_cast<uint8_t>(snes & 0xFF),
                                static_cast<uint8_t>((snes >> 8) & 0xFF),
                                static_cast<uint8_t>((snes >> 16) & 0xFF)};
  }
  const uint32_t fixed_snes =
      (static_cast<uint32_t>(layout.pointer_bank) << 16) | (snes & 0xFFFFu);
  if (SnesToPc(fixed_snes) != pc) {
    return absl::OutOfRangeError(absl::StrFormat(
        "PC address 0x%06X is outside fixed pointer bank 0x%02X", pc,
        layout.pointer_bank));
  }
  return std::vector<uint8_t>{static_cast<uint8_t>(snes & 0xFF),
                              static_cast<uint8_t>((snes >> 8) & 0xFF)};
}

absl::StatusOr<uint32_t> FindFirstFit(
    const std::vector<DungeonStreamPcRange>& free_intervals,
    const DungeonStreamLayout& layout, uint32_t size) {
  if (size == 0 || size > kLoRomBankSize) {
    return absl::ResourceExhaustedError(
        "Replacement cannot fit in one LoROM bank");
  }

  if (layout.pointer_encoding == DungeonPointerEncoding::kFixedBank16) {
    const uint32_t bank_begin =
        SnesToPc((static_cast<uint32_t>(layout.pointer_bank) << 16) | 0x8000u);
    const uint32_t bank_end = bank_begin + kLoRomBankSize;
    for (const auto& interval : free_intervals) {
      const uint32_t begin = std::max(interval.begin, bank_begin);
      const uint32_t end = std::min(interval.end, bank_end);
      if (begin <= end && static_cast<uint64_t>(begin) + size <= end) {
        return begin;
      }
    }
    return absl::ResourceExhaustedError(
        absl::StrFormat("No %u-byte allocation fits fixed pointer bank 0x%02X",
                        size, layout.pointer_bank));
  }

  for (const auto& interval : free_intervals) {
    uint32_t candidate = interval.begin;
    while (candidate < interval.end) {
      const uint32_t bank_end =
          ((candidate / kLoRomBankSize) + 1) * kLoRomBankSize;
      const uint32_t usable_end = std::min(interval.end, bank_end);
      if (static_cast<uint64_t>(candidate) + size <= usable_end &&
          EncodePointer(layout, candidate).ok()) {
        return candidate;
      }
      if (bank_end <= candidate) {
        break;
      }
      candidate = bank_end;
    }
  }
  return absl::ResourceExhaustedError(
      absl::StrFormat("No declared free interval fits %u bytes", size));
}

void ConsumeInterval(std::vector<DungeonStreamPcRange>* free_intervals,
                     DungeonStreamPcRange consumed) {
  std::vector<DungeonStreamPcRange> next;
  next.reserve(free_intervals->size() + 1);
  for (const auto& interval : *free_intervals) {
    if (!Intersects(interval, consumed)) {
      next.push_back(interval);
      continue;
    }
    if (interval.begin < consumed.begin) {
      next.push_back({interval.begin, consumed.begin});
    }
    if (consumed.end < interval.end) {
      next.push_back({consumed.end, interval.end});
    }
  }
  *free_intervals = std::move(next);
}

bool IsSortedUniqueByRoom(const std::vector<DungeonStreamWrite>& writes) {
  for (size_t i = 1; i < writes.size(); ++i) {
    if (writes[i - 1].room_id >= writes[i].room_id) {
      return false;
    }
  }
  return true;
}

absl::Status ValidatePlanAgainstInventory(
    const DungeonStreamWritePlan& plan,
    const DungeonStreamInventory& inventory) {
  if (!inventory.ok()) {
    return absl::FailedPreconditionError(
        "Source inventory contains invalid pointers or malformed streams");
  }
  if (plan.source_size != inventory.source_size ||
      plan.source_crc32 != inventory.source_crc32) {
    return absl::AbortedError(
        "Write plan does not match the current inventory snapshot");
  }
  if (plan.payload_writes.empty() ||
      plan.payload_writes.size() != plan.pointer_writes.size()) {
    return absl::InvalidArgumentError(
        "Write plan must contain matching payload and pointer writes");
  }
  const size_t expected_auxiliary_count =
      plan.layout.kind == DungeonStreamKind::kObject
          ? plan.payload_writes.size()
          : 0;
  if (plan.auxiliary_pointer_writes.size() != expected_auxiliary_count) {
    return absl::InvalidArgumentError(
        "Write plan has the wrong number of auxiliary pointer writes");
  }
  if (!IsSortedUniqueByRoom(plan.payload_writes) ||
      !IsSortedUniqueByRoom(plan.pointer_writes) ||
      (!plan.auxiliary_pointer_writes.empty() &&
       !IsSortedUniqueByRoom(plan.auxiliary_pointer_writes))) {
    return absl::InvalidArgumentError(
        "Write plan must be strictly sorted by room ID");
  }

  std::vector<DungeonStreamPcRange> planned_payloads;
  planned_payloads.reserve(plan.payload_writes.size());
  const uint32_t pointer_width = PointerWidth(plan.layout.pointer_encoding);
  for (size_t i = 0; i < plan.payload_writes.size(); ++i) {
    const auto& payload = plan.payload_writes[i];
    const auto& pointer = plan.pointer_writes[i];
    if (payload.room_id != pointer.room_id ||
        payload.room_id >= plan.layout.pointer_count || payload.bytes.empty()) {
      return absl::InvalidArgumentError("Write plan room mapping is invalid");
    }
    auto encoded_status =
        ValidateCompleteEncodedStream(plan.layout.kind, payload.bytes);
    if (!encoded_status.ok()) {
      return encoded_status;
    }
    const uint64_t payload_end64 =
        static_cast<uint64_t>(payload.address) + payload.bytes.size();
    if (payload_end64 > std::numeric_limits<uint32_t>::max()) {
      return absl::OutOfRangeError("Payload write address overflows PC space");
    }
    const DungeonStreamPcRange payload_range{
        payload.address, static_cast<uint32_t>(payload_end64)};
    const uint32_t payload_bank_end =
        ((payload.address / kLoRomBankSize) + 1) * kLoRomBankSize;
    if (payload_range.end > payload_bank_end) {
      return absl::FailedPreconditionError(
          "Planned payload crosses a LoROM bank boundary");
    }
    const bool in_declared_free = std::any_of(
        inventory.allocatable_free_intervals.begin(),
        inventory.allocatable_free_intervals.end(),
        [&](const auto& range) { return Contains(range, payload_range); });
    if (!in_declared_free) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Payload for room %u is outside declared allocator-owned free space",
          payload.room_id));
    }
    for (const auto& prior : planned_payloads) {
      if (Intersects(prior, payload_range)) {
        return absl::InvalidArgumentError("Planned payload writes overlap");
      }
    }
    planned_payloads.push_back(payload_range);

    auto expected_pointer = EncodePointer(plan.layout, payload.address);
    if (!expected_pointer.ok()) {
      return expected_pointer.status();
    }
    const uint32_t expected_slot =
        plan.layout.pointer_table_pc + payload.room_id * pointer_width;
    if (pointer.address != expected_slot ||
        pointer.bytes != *expected_pointer) {
      return absl::InvalidArgumentError(
          "Pointer write does not match its planned payload");
    }
    if (plan.layout.kind == DungeonStreamKind::kObject) {
      const auto& auxiliary = plan.auxiliary_pointer_writes[i];
      auto door_offset = ObjectDoorListOffset(payload.bytes);
      if (!door_offset.ok()) {
        return door_offset.status();
      }
      auto expected_door_pointer =
          EncodePointer(plan.layout, payload.address + *door_offset);
      if (!expected_door_pointer.ok()) {
        return expected_door_pointer.status();
      }
      const uint32_t expected_door_slot = kDoorPointers + payload.room_id * 3;
      if (auxiliary.room_id != payload.room_id ||
          auxiliary.address != expected_door_slot ||
          auxiliary.bytes != *expected_door_pointer) {
        return absl::InvalidArgumentError(
            "Object door pointer does not match its planned payload");
      }
      if (static_cast<uint64_t>(auxiliary.address) + auxiliary.bytes.size() >
          inventory.source_size) {
        return absl::OutOfRangeError(
            "Object door-pointer write is outside the source ROM");
      }
    }
  }
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<DungeonStreamInventory> InventoryDungeonStreams(
    const Rom& rom, const DungeonStreamLayout& requested_layout) {
  auto normalized_layout = ValidateAndNormalizeLayout(rom, requested_layout);
  if (!normalized_layout.ok()) {
    return normalized_layout.status();
  }

  DungeonStreamInventory inventory;
  inventory.layout = std::move(*normalized_layout);
  inventory.source_size = static_cast<uint32_t>(rom.size());
  inventory.source_crc32 =
      util::CalculateCrc32(rom.data(), static_cast<size_t>(rom.size()));
  inventory.streams.reserve(inventory.layout.pointer_count);

  const uint32_t pointer_width =
      PointerWidth(inventory.layout.pointer_encoding);
  std::map<uint32_t, std::vector<size_t>> valid_by_start;
  std::vector<DungeonStreamPcRange> occupied;
  const std::span<const uint8_t> bytes(rom.data(), rom.size());
  for (uint32_t room_id = 0; room_id < inventory.layout.pointer_count;
       ++room_id) {
    DungeonStreamRecord record;
    record.room_id = room_id;
    record.pointer_slot_pc =
        inventory.layout.pointer_table_pc + room_id * pointer_width;

    auto pointer = DecodePointer(rom, inventory.layout, room_id);
    if (!pointer.ok()) {
      inventory.issues.push_back({DungeonStreamIssueCode::kInvalidPointer,
                                  room_id, 0,
                                  absl::StrCat("Room pointer is invalid: ",
                                               pointer.status().message())});
      inventory.streams.push_back(record);
      continue;
    }
    record.data_pc = *pointer;
    const auto* data_range =
        FindContainingRange(inventory.layout.data_ranges, record.data_pc);
    if (data_range == nullptr) {
      inventory.issues.push_back(
          {DungeonStreamIssueCode::kPointerOutsideDataRanges, room_id,
           record.data_pc,
           absl::StrFormat(
               "Room %u pointer PC 0x%06X is outside declared data ranges",
               room_id, record.data_pc)});
      inventory.streams.push_back(record);
      continue;
    }

    const uint32_t bank_end =
        ((record.data_pc / kLoRomBankSize) + 1) * kLoRomBankSize;
    const uint32_t parse_limit = std::min(data_range->end, bank_end);
    auto logical_end =
        ParseStream(bytes, inventory.layout.kind, record.data_pc, parse_limit);
    if (!logical_end.ok()) {
      inventory.issues.push_back(
          {DungeonStreamIssueCode::kMalformedStream, room_id, record.data_pc,
           absl::StrCat("Room stream is malformed: ",
                        logical_end.status().message())});
      inventory.streams.push_back(record);
      continue;
    }

    record.logical_end_pc = *logical_end;
    record.valid = true;
    inventory.streams.push_back(record);
    const size_t index = inventory.streams.size() - 1;
    valid_by_start[record.data_pc].push_back(index);
    occupied.push_back({record.data_pc, record.logical_end_pc});
  }

  struct UniqueStream {
    uint32_t begin = 0;
    uint32_t end = 0;
    std::vector<uint32_t> room_ids;
  };
  std::vector<UniqueStream> unique;
  unique.reserve(valid_by_start.size());
  for (const auto& [start, indices] : valid_by_start) {
    UniqueStream stream;
    stream.begin = start;
    stream.end = inventory.streams[indices.front()].logical_end_pc;
    for (size_t index : indices) {
      stream.room_ids.push_back(inventory.streams[index].room_id);
    }
    if (stream.room_ids.size() > 1) {
      inventory.aliases.push_back({start, stream.room_ids});
    }
    unique.push_back(std::move(stream));
  }

  for (size_t i = 0; i < unique.size(); ++i) {
    for (size_t j = i + 1; j < unique.size(); ++j) {
      if (unique[j].begin >= unique[i].end) {
        break;
      }
      const uint32_t intersection_end = std::min(unique[i].end, unique[j].end);
      if (unique[j].begin >= intersection_end) {
        continue;
      }
      const auto kind = unique[j].end == unique[i].end
                            ? DungeonStreamOverlapKind::kSuffix
                            : DungeonStreamOverlapKind::kInterior;
      inventory.overlaps.push_back({kind,
                                    unique[i].room_ids,
                                    unique[j].room_ids,
                                    {unique[j].begin, intersection_end}});
    }
  }

  inventory.occupied_intervals = UnionIntervals(std::move(occupied));
  inventory.free_intervals = ComplementIntervals(inventory.layout.data_ranges,
                                                 inventory.occupied_intervals);
  inventory.allocatable_free_intervals = ComplementIntervals(
      inventory.layout.allocation_ranges, inventory.occupied_intervals);
  return inventory;
}

absl::StatusOr<DungeonStreamWritePlan> PlanDungeonStreamWrites(
    const DungeonStreamInventory& inventory,
    const std::vector<DungeonStreamReplacement>& requested_replacements) {
  if (!inventory.ok()) {
    return absl::FailedPreconditionError(
        "Cannot plan writes from an inventory with stream issues");
  }
  if (requested_replacements.empty()) {
    return absl::InvalidArgumentError("No dungeon stream replacements given");
  }

  std::vector<DungeonStreamReplacement> replacements = requested_replacements;
  std::sort(replacements.begin(), replacements.end(),
            [](const auto& a, const auto& b) { return a.room_id < b.room_id; });
  for (size_t i = 0; i < replacements.size(); ++i) {
    if (replacements[i].room_id >= inventory.layout.pointer_count) {
      return absl::OutOfRangeError("Replacement room ID is out of range");
    }
    if (i > 0 && replacements[i - 1].room_id == replacements[i].room_id) {
      return absl::InvalidArgumentError("Replacement room IDs must be unique");
    }
    auto status = ValidateCompleteEncodedStream(inventory.layout.kind,
                                                replacements[i].encoded_stream);
    if (!status.ok()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Room %u replacement is invalid: %s",
                          replacements[i].room_id, status.message()));
    }
  }

  DungeonStreamWritePlan plan;
  plan.layout = inventory.layout;
  plan.source_size = inventory.source_size;
  plan.source_crc32 = inventory.source_crc32;
  if (plan.layout.kind == DungeonStreamKind::kObject &&
      static_cast<uint64_t>(kDoorPointers) +
              static_cast<uint64_t>(plan.layout.pointer_count) * 3 >
          plan.source_size) {
    return absl::OutOfRangeError(
        "Object door-pointer table is outside the source ROM");
  }
  std::vector<DungeonStreamPcRange> free = inventory.allocatable_free_intervals;
  const uint32_t pointer_width = PointerWidth(plan.layout.pointer_encoding);
  for (const auto& replacement : replacements) {
    const uint32_t size =
        static_cast<uint32_t>(replacement.encoded_stream.size());
    auto address = FindFirstFit(free, plan.layout, size);
    if (!address.ok()) {
      return address.status();
    }
    auto pointer_bytes = EncodePointer(plan.layout, *address);
    if (!pointer_bytes.ok()) {
      return pointer_bytes.status();
    }

    plan.payload_writes.push_back(
        {replacement.room_id, *address, replacement.encoded_stream});
    plan.pointer_writes.push_back(
        {replacement.room_id,
         plan.layout.pointer_table_pc + replacement.room_id * pointer_width,
         std::move(*pointer_bytes)});
    if (plan.layout.kind == DungeonStreamKind::kObject) {
      auto door_offset = ObjectDoorListOffset(replacement.encoded_stream);
      if (!door_offset.ok()) {
        return door_offset.status();
      }
      auto door_pointer = EncodePointer(plan.layout, *address + *door_offset);
      if (!door_pointer.ok()) {
        return door_pointer.status();
      }
      plan.auxiliary_pointer_writes.push_back(
          {replacement.room_id, kDoorPointers + replacement.room_id * 3,
           std::move(*door_pointer)});
    }
    ConsumeInterval(&free, {*address, *address + size});
  }
  return plan;
}

absl::Status ApplyDungeonStreamWritePlan(Rom* rom,
                                         const DungeonStreamWritePlan& plan) {
  if (rom == nullptr || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }
  if (rom->size() != plan.source_size) {
    return absl::AbortedError("Dungeon stream plan source size is stale");
  }
  const uint32_t current_crc =
      util::CalculateCrc32(rom->data(), static_cast<size_t>(rom->size()));
  if (current_crc != plan.source_crc32) {
    return absl::AbortedError(absl::StrFormat(
        "Dungeon stream plan is stale (source CRC %08X, current CRC %08X)",
        plan.source_crc32, current_crc));
  }

  auto inventory = InventoryDungeonStreams(*rom, plan.layout);
  if (!inventory.ok()) {
    return inventory.status();
  }
  auto validation = ValidatePlanAgainstInventory(plan, *inventory);
  if (!validation.ok()) {
    return validation;
  }

  yaze::rom::WriteFence write_fence;
  auto allow_writes = [&](const std::vector<DungeonStreamWrite>& writes,
                          const char* label) -> absl::Status {
    for (const auto& write : writes) {
      const uint64_t end =
          static_cast<uint64_t>(write.address) + write.bytes.size();
      if (end > std::numeric_limits<uint32_t>::max()) {
        return absl::OutOfRangeError("Dungeon stream write range overflows");
      }
      RETURN_IF_ERROR(
          write_fence.Allow(write.address, static_cast<uint32_t>(end), label));
    }
    return absl::OkStatus();
  };
  RETURN_IF_ERROR(allow_writes(plan.payload_writes, "DungeonStreamPayload"));
  RETURN_IF_ERROR(allow_writes(plan.pointer_writes, "DungeonStreamPointer"));
  RETURN_IF_ERROR(allow_writes(plan.auxiliary_pointer_writes,
                               "DungeonStreamAuxiliaryPointer"));
  yaze::rom::ScopedWriteFence write_scope(rom, &write_fence);

  const std::vector<uint8_t> snapshot = rom->vector();
  const bool was_dirty = rom->dirty();
  auto rollback = [&]() {
    rom->mutable_vector() = snapshot;
    rom->set_dirty(was_dirty);
  };

  for (const auto& write : plan.payload_writes) {
    auto status =
        rom->WriteVector(static_cast<int>(write.address), write.bytes);
    if (!status.ok()) {
      rollback();
      return status;
    }
  }
  for (const auto& write : plan.pointer_writes) {
    auto status =
        rom->WriteVector(static_cast<int>(write.address), write.bytes);
    if (!status.ok()) {
      rollback();
      return status;
    }
  }
  for (const auto& write : plan.auxiliary_pointer_writes) {
    auto status =
        rom->WriteVector(static_cast<int>(write.address), write.bytes);
    if (!status.ok()) {
      rollback();
      return status;
    }
  }
  return absl::OkStatus();
}

}  // namespace yaze::zelda3
