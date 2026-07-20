#ifndef YAZE_ZELDA3_DUNGEON_DUNGEON_STREAM_ALLOCATOR_H_
#define YAZE_ZELDA3_DUNGEON_DUNGEON_STREAM_ALLOCATOR_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze {

class Rom;

namespace zelda3 {

// The three variable-length, per-room dungeon stream formats supported by the
// copy-on-write planner.
enum class DungeonStreamKind {
  kObject,
  kSprite,
  kPotItem,
};

enum class DungeonPointerEncoding {
  kLong24,
  kFixedBank16,
};

struct DungeonStreamPcRange {
  uint32_t begin = 0;
  uint32_t end = 0;

  bool operator==(const DungeonStreamPcRange&) const = default;
};

// All addresses in a layout are headerless PC offsets. data_ranges describe
// where existing streams are allowed to live. allocation_ranges are the
// explicitly allocator-owned subset; the planner never allocates merely
// because ROM bytes happen to look unused.
struct DungeonStreamLayout {
  DungeonStreamKind kind = DungeonStreamKind::kObject;
  uint32_t pointer_table_pc = 0;
  uint32_t pointer_count = 0;
  DungeonPointerEncoding pointer_encoding = DungeonPointerEncoding::kLong24;
  uint8_t pointer_bank = 0;  // Used only by kFixedBank16.
  std::vector<DungeonStreamPcRange> data_ranges;
  std::vector<DungeonStreamPcRange> allocation_ranges;
};

enum class DungeonStreamIssueCode {
  kInvalidPointer,
  kPointerOutsideDataRanges,
  kMalformedStream,
};

struct DungeonStreamIssue {
  DungeonStreamIssueCode code = DungeonStreamIssueCode::kInvalidPointer;
  uint32_t room_id = 0;
  uint32_t address = 0;
  std::string message;
};

struct DungeonStreamRecord {
  uint32_t room_id = 0;
  uint32_t pointer_slot_pc = 0;
  uint32_t data_pc = 0;
  uint32_t logical_end_pc = 0;
  bool valid = false;
  // Immutable logical payload captured with the inventory snapshot. Keeping
  // the bytes here lets a full repack preserve rooms that have no replacement
  // without consulting a ROM that may have changed since inventory.
  std::vector<uint8_t> encoded_stream;

  uint32_t size() const {
    return valid && logical_end_pc >= data_pc ? logical_end_pc - data_pc : 0;
  }
};

struct DungeonStreamAliasGroup {
  uint32_t data_pc = 0;
  std::vector<uint32_t> room_ids;
};

enum class DungeonStreamOverlapKind {
  // The second stream starts inside the first and shares its logical end.
  kSuffix,
  // The logical intervals intersect but neither is an exact alias or suffix.
  kInterior,
};

struct DungeonStreamOverlap {
  DungeonStreamOverlapKind kind = DungeonStreamOverlapKind::kInterior;
  std::vector<uint32_t> first_room_ids;
  std::vector<uint32_t> second_room_ids;
  DungeonStreamPcRange intersection;
};

// An inventory is an immutable snapshot of the source ROM. occupied_intervals
// is the union of all successfully parsed logical streams. free_intervals is
// its complement inside data_ranges; allocatable_free_intervals is the much
// narrower complement inside allocation_ranges and is the only space consumed
// by PlanDungeonStreamWrites.
struct DungeonStreamInventory {
  DungeonStreamLayout layout;
  uint32_t source_size = 0;
  uint32_t source_crc32 = 0;
  std::vector<DungeonStreamRecord> streams;
  std::vector<DungeonStreamAliasGroup> aliases;
  std::vector<DungeonStreamOverlap> overlaps;
  std::vector<DungeonStreamIssue> issues;
  std::vector<DungeonStreamPcRange> occupied_intervals;
  std::vector<DungeonStreamPcRange> free_intervals;
  std::vector<DungeonStreamPcRange> allocatable_free_intervals;

  bool ok() const { return issues.empty(); }
};

struct DungeonStreamReplacement {
  uint32_t room_id = 0;
  // The complete logical stream, including format header(s) and terminator.
  std::vector<uint8_t> encoded_stream;
};

struct DungeonStreamWrite {
  uint32_t room_id = 0;
  uint32_t address = 0;
  std::vector<uint8_t> bytes;

  bool operator==(const DungeonStreamWrite&) const = default;
};

enum class DungeonStreamWriteMode {
  kCopyOnWrite,
  kRepackAll,
};

struct DungeonStreamWritePlan {
  DungeonStreamLayout layout;
  uint32_t source_size = 0;
  uint32_t source_crc32 = 0;
  DungeonStreamWriteMode mode = DungeonStreamWriteMode::kCopyOnWrite;
  // Kept separate so apply can guarantee all payloads land before any pointer
  // becomes live.
  std::vector<DungeonStreamWrite> payload_writes;
  std::vector<DungeonStreamWrite> pointer_writes;
  // Object relocation also needs the runtime door pointer updated to the
  // first door byte (or the final terminator for a doorless stream).
  std::vector<DungeonStreamWrite> auxiliary_pointer_writes;
};

// Reads and strictly parses every pointer-table entry without modifying rom.
// Fatal layout/source-table errors are returned as a non-OK status; per-room
// pointer and stream errors are retained in DungeonStreamInventory::issues.
absl::StatusOr<DungeonStreamInventory> InventoryDungeonStreams(
    const Rom& rom, const DungeonStreamLayout& layout);

// Deterministic first-fit copy-on-write planning. Replacements are validated as
// complete streams, sorted by room ID, and allocated only from the inventory's
// declared allocation-range complement. Existing streams are never reclaimed.
absl::StatusOr<DungeonStreamWritePlan> PlanDungeonStreamWrites(
    const DungeonStreamInventory& inventory,
    const std::vector<DungeonStreamReplacement>& replacements);

// Deterministically repacks every pot-item stream into one normalized declared
// allocation range. Exact byte-identical payloads share one placement owned by
// their lowest room ID, while every pointer-table entry receives an update.
// Untouched rooms come from the immutable inventory snapshot. No ROM bytes are
// changed during planning, and the operation fails before producing a plan if
// all unique payloads cannot fit without crossing the fixed pointer bank.
absl::StatusOr<DungeonStreamWritePlan> PlanDungeonStreamRepack(
    const DungeonStreamInventory& inventory,
    const std::vector<DungeonStreamReplacement>& replacements);

// Applies a source-CRC-guarded plan transactionally. The complete ROM and dirty
// flag are restored if any write fails. Payload writes always precede pointer
// writes.
absl::Status ApplyDungeonStreamWritePlan(Rom* rom,
                                         const DungeonStreamWritePlan& plan);

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_DUNGEON_STREAM_ALLOCATOR_H_
