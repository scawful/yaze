#ifndef YAZE_ZELDA3_OVERWORLD_OVERWORLD_ALLOCATION_H
#define YAZE_ZELDA3_OVERWORLD_OVERWORLD_ALLOCATION_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "rom/rom.h"

namespace yaze::zelda3 {

/**
 * @file overworld_allocation.h
 * @brief Read-only map of overworld area IDs $00-$BF: which IDs are used,
 * which per-area tables cover each ID, and whether yaze's
 * TailMapExpansion.asm patch ($A0-$BF) is safe to apply to a ROM.
 *
 * Area IDs: light world $00-$3F, dark world $40-$7F, special world $80-$9F,
 * special-world tail $A0-$BF. A table "covers" an ID when the ID indexes an
 * entry of that table. Reading an ID past a table's last entry reads the
 * data that follows the table (listed as `past_end`), which is how a missed
 * table fails silently in game.
 */

inline constexpr int kAllocationAreaCount = 0xC0;
inline constexpr int kSpecialTailFirstArea = 0xA0;

enum class AllocationWorld { kLight, kDark, kSpecial };

enum class AllocationAreaSize { kSmall, kLarge, kWide, kTall };

const char* AllocationWorldName(AllocationWorld world);
const char* AllocationAreaSizeName(AllocationAreaSize size);
AllocationWorld AllocationWorldForArea(int area);

/// One table the game or yaze indexes with an overworld area ID.
struct AreaTableInfo {
  std::string id;      // Stable key, e.g. "pool_parent".
  std::string label;   // Human-readable name.
  std::string group;   // map, gfx, transition, sprites, text, music, items,
                       // sram
  bool in_rom = true;  // false for WRAM/SRAM tables.
  uint32_t pc = 0;     // PC address of the entry for `first_area`.
  uint32_t snes = 0;   // SNES address of the same entry (LoROM or WRAM).
  int entry_bytes = 1;
  int first_area = 0;
  int last_area = 0;     // Inclusive.
  std::string past_end;  // What an ID past `last_area` reads.
  std::string note;
  // IDs the game reads this table for. Special-world tables are read only
  // for $80+, the light-world music table only for $00-$3F.
  int applies_first = 0x00;
  int applies_last = 0xBF;

  bool Covers(int area) const {
    return area >= first_area && area <= last_area;
  }
  bool Applies(int area) const {
    return area >= applies_first && area <= applies_last;
  }
  /// The game reads this table for `area` but has no entry for it.
  bool Misses(int area) const { return Applies(area) && !Covers(area); }
};

/// Tables for this ROM's layout. ZSCustomOverworld v3 ROMs get the Pool
/// tables at $28:8000; every other ROM gets the vanilla tables.
std::vector<AreaTableInfo> OverworldAreaTableCatalogue(uint8_t asm_version);

/// Result of walking one compressed (LC_LZ2, big-endian copy offsets) map32
/// stream, bounded by the ROM size.
struct MapStreamScan {
  uint32_t snes = 0;
  uint32_t pc = 0;
  bool in_rom = false;
  bool terminated = false;   // Reached the $FF terminator.
  int compressed_bytes = 0;  // Bytes read, including the terminator.
  int decoded_bytes = 0;
  std::string error;  // Empty, or why decoding stopped early.
  std::vector<uint8_t> decoded;
};

/// Walks a map32 stream the way the game's decompressor does. Decoding stops
/// at $FF, at the end of `size`, or after `max_output` bytes.
MapStreamScan ScanMapStream(const uint8_t* data, size_t size, uint32_t pc,
                            size_t max_output = 0x4000);

struct AreaAllocation {
  int area = 0;
  AllocationWorld world = AllocationWorld::kLight;
  std::string name;  // From the project registry, when given.

  bool used = false;
  std::vector<std::string> evidence;  // Why the ID counts as used.

  // Parent and size as the game reads them. `parent` is empty when no parent
  // table covers the ID; `parent_past_end` then holds the byte a read would
  // return from the data after the table.
  std::optional<int> parent;
  std::optional<int> parent_past_end;
  AllocationAreaSize size = AllocationAreaSize::kSmall;

  // Map data. `map_pointer_source` is "vanilla_table", "tail_table" or
  // "none". A pointer is valid when both streams are inside the ROM, end
  // with $FF and decode to at least 256 bytes.
  std::string map_pointer_source = "none";
  bool map_pointer_valid = false;
  MapStreamScan high;  // decoded bytes are cleared after classification
  MapStreamScan low;
  int unique_tile32 = -1;
  std::optional<int> duplicate_of;  // Lower ID with the same two pointers.

  int entrances = 0;
  int holes = 0;
  int exits = 0;
  int sprites = 0;
  std::vector<int> overlay_users;  // Areas whose subscreen overlay is this ID.
  bool overlay_named_in_code = false;

  std::vector<std::string> uncovered_tables;  // AreaTableInfo::id values.
};

struct WorldAllocationSummary {
  std::string key;  // light, dark, special, special_tail
  int first_area = 0;
  int last_area = 0;
  int used = 0;
  int free = 0;
  int free_with_map_data = 0;     // Free, and a valid map pointer exists.
  int free_without_map_data = 0;  // Free, and no valid map pointer.
};

struct OverworldAllocationOptions {
  bool scan_entities = true;  // Entrances, holes and exits.
  bool scan_sprites = true;   // Overworld sprite lists (ZS v3 only).
  std::map<int, std::string> area_names;
};

struct OverworldAllocationReport {
  std::string rom_name;
  size_t rom_size = 0;
  uint8_t asm_version = 0xFF;
  std::string catalogue;  // "zscustom_v3" or "vanilla"
  bool tail_marker_present = false;
  std::vector<AreaTableInfo> tables;
  std::vector<AreaAllocation> areas;  // kAllocationAreaCount entries.
  std::vector<WorldAllocationSummary> summary;

  // Ids of tables the game reads for $A0 that have no entry for it.
  std::vector<std::string> TablesStoppingBeforeTail() const;
};

absl::StatusOr<OverworldAllocationReport> BuildOverworldAllocationReport(
    Rom* rom, const OverworldAllocationOptions& options = {});

// ---------------------------------------------------------------------------
// TailMapExpansion.asm compatibility check
// ---------------------------------------------------------------------------

/// What assets/patches/Overworld/TailMapExpansion.asm writes. Keep in sync
/// with the .asm (a unit test compares them).
struct TailMapExpansionPlan {
  uint32_t marker_snes = 0x28A3FF;
  uint8_t marker_value = 0xEA;
  uint32_t high_table_snes = 0x28A400;
  uint32_t low_table_snes = 0x28A640;
  int table_entries = 192;
  uint32_t blank_high_snes = 0x308000;
  std::vector<uint8_t> blank_high = std::vector<uint8_t>(188, 0x00);
  uint32_t blank_low_snes = 0x309000;
  std::vector<uint8_t> blank_low = std::vector<uint8_t>(4, 0x00);
  // SNES addresses of the 3-byte LDA.l operands the patch rewrites, the
  // vanilla table address each holds, and the address the patch writes.
  std::array<uint32_t, 8> operand_snes = {0x02F59E, 0x02F5A4, 0x02F5C9,
                                          0x02F5CF, 0x02F7E4, 0x02F7EA,
                                          0x02F80F, 0x02F815};
  std::array<uint32_t, 8> operand_vanilla = {0x02F94D, 0x02F94E, 0x02FB2D,
                                             0x02FB2E, 0x02F94D, 0x02F94E,
                                             0x02FB2D, 0x02FB2E};
  std::array<uint32_t, 8> operand_patched = {0x28A400, 0x28A401, 0x28A640,
                                             0x28A641, 0x28A400, 0x28A401,
                                             0x28A640, 0x28A641};
  // yaze facts the verdict depends on. Overworld::SaveOverworldMaps writes
  // map pointers to 0x1794D/0x17B2D only, and yaze loads 160 maps.
  bool yaze_saves_relocated_tables = false;
  int yaze_map_count = 160;
};

enum class TailCheckVerdict { kSafe, kUnsafe, kAlreadyApplied };
enum class TailCheckScope { kRom, kPatch, kYaze };

const char* TailCheckVerdictName(TailCheckVerdict verdict);
const char* TailCheckScopeName(TailCheckScope scope);

struct TailCheckFinding {
  TailCheckScope scope = TailCheckScope::kRom;
  std::string id;
  std::string message;
  bool blocking = true;
};

struct TailCheckRegion {
  std::string id;  // marker, high_table, low_table, blank_high, blank_low
  std::string label;
  uint32_t snes = 0;
  uint32_t pc = 0;
  int length = 0;
  bool in_rom = false;  // The whole region is inside the current ROM.
  int nonzero_bytes = 0;
  std::string first_bytes;  // Up to 16 bytes, hex.
  std::string owner;        // From the annotator (hack manifest).
  std::string symbol;       // From the annotator (symbol file).
};

struct TailCheckOperand {
  uint32_t snes = 0;
  uint32_t pc = 0;
  bool in_rom = false;
  uint8_t opcode = 0;
  uint32_t current = 0;
  uint32_t vanilla = 0;
  uint32_t patched = 0;
  std::string state;  // vanilla, patched, other, missing
};

struct TailStreamCheck {
  std::string id;  // blank_high, blank_low
  uint32_t snes = 0;
  int stream_bytes = 0;  // Bytes the patch writes (or 0 when applied).
  bool terminated = false;
  bool terminated_within_stream = false;
  int decoded_bytes = 0;
  int consumed_bytes = 0;
  std::string stopped_at;  // SNES address where decoding stopped.
  std::string error;
  bool valid = false;  // $FF inside the stream and >= 256 decoded bytes.
};

struct TailCheckAnnotator {
  /// Returns an ownership note for a SNES address and whether that owner
  /// makes the address unavailable to the patch (e.g. an ASM-owned bank).
  std::function<std::optional<std::pair<std::string, bool>>(uint32_t)>
      ownership;
  /// Returns a symbol for a SNES address (empty when none).
  std::function<std::string(uint32_t)> symbol;
};

struct TailMapExpansionCheck {
  size_t rom_size = 0;
  uint8_t asm_version = 0xFF;
  bool marker_present = false;
  uint8_t marker_value = 0;
  std::vector<TailCheckRegion> regions;
  std::vector<TailStreamCheck> streams;
  std::vector<TailCheckOperand> operands;
  bool zs_code_found = false;
  uint32_t zs_code_end_snes = 0;  // Last non-zero byte before the marker.
  int free_bytes_before_marker = 0;
  std::vector<std::string> tables_stopping_before_tail;
  std::vector<TailCheckFinding> findings;
  TailCheckVerdict verdict = TailCheckVerdict::kUnsafe;
  bool rom_compatible = false;  // No blocking finding with scope kRom.

  bool IsSafe() const { return verdict == TailCheckVerdict::kSafe; }
  /// One line per blocking finding, for error messages.
  std::string BlockingSummary() const;
};

TailMapExpansionCheck CheckTailMapExpansion(
    const Rom& rom, const TailMapExpansionPlan& plan = {},
    const TailCheckAnnotator& annotator = {});

/// Error to return from any path that would apply TailMapExpansion.asm.
/// OK when the check says safe (or the patch is already applied).
absl::Status TailMapExpansionApplyGuard(const Rom& rom);

// ---------------------------------------------------------------------------
// Output
// ---------------------------------------------------------------------------

std::string OverworldAllocationToJson(const OverworldAllocationReport& report,
                                      const TailMapExpansionCheck* tail_check);
std::string FormatOverworldAllocationText(
    const OverworldAllocationReport& report,
    const TailMapExpansionCheck* tail_check);

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_OVERWORLD_OVERWORLD_ALLOCATION_H
