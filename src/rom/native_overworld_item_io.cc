#include "rom/native_overworld_item_io.h"

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "rom/lyra_layout_descriptor.h"
#include "rom/native_overworld_storage.h"
#include "util/macro.h"

namespace yaze::rom {
namespace {

constexpr size_t kDescriptorPc = 0x147000;
constexpr size_t kDescriptorSize = 1024;
constexpr size_t kItemBankIndex = 0x1B;
constexpr size_t kItemBankBasePc = 0x0D8000;
constexpr size_t kItemBankEndPc = 0x0E0000;
constexpr size_t kItemBankBytePc = 0x0DC8BF;
constexpr size_t kItemStreamBeginPc = 0x0DC2F9;
constexpr size_t kItemStreamEndPc = 0x0DC89C;
constexpr size_t kItemStreamCapacity = kItemStreamEndPc - kItemStreamBeginPc;
constexpr size_t kBombDoorMirrorPc = 0x012644;
constexpr size_t kItemPointerMirrorPc = 0x012784;
constexpr size_t kLegacyMirrorRowCount = 160;

const LyraLayoutEntry* FindEntry(const LyraLayoutInspection& layout,
                                 uint16_t id) {
  const auto it = std::find_if(layout.entries.begin(), layout.entries.end(),
                               [id](const auto& e) { return e.id == id; });
  return it == layout.entries.end() ? nullptr : &*it;
}

uint16_t ReadWord(std::span<const uint8_t> bytes, size_t pc) {
  return static_cast<uint16_t>(bytes[pc] | (bytes[pc + 1] << 8));
}

bool Intersects(size_t start, size_t length, size_t other,
                size_t other_length) {
  return start < other + other_length && other < start + length;
}

absl::Status CheckStorage(const LyraLayoutInspection& layout, size_t start,
                          size_t length, uint16_t allowed_id,
                          bool native_mirror) {
  if (Intersects(start, length, kDescriptorPc, kDescriptorSize)) {
    return absl::PermissionDeniedError(
        "Native item storage overlaps descriptor");
  }
  for (const auto& native_ranges :
       {std::span<const internal::NativeOverworldTableStorage>(
            internal::kProperties),
        std::span<const internal::NativeOverworldTableStorage>(
            internal::kOtherNativeStorage)}) {
    for (const auto& native : native_ranges) {
      if (native.native_pc == 0) {
        continue;
      }
      size_t stride = 1;
      if (native.id < 500) {
        const auto* e = FindEntry(layout, native.id);
        if (!e) {
          continue;
        }
        stride = e->stride;
      }
      const size_t size = size_t{native.native_count} * stride;
      if (!Intersects(start, length, native.native_pc, size)) {
        continue;
      }
      if (!native_mirror || allowed_id != native.id ||
          (allowed_id != 22 && allowed_id != 23) || start != native.native_pc ||
          length != size) {
        return absl::PermissionDeniedError(
            "Native item storage aliases another native overworld table");
      }
    }
  }
  for (const auto& other : layout.entries) {
    const size_t other_len = size_t{other.count} * other.stride;
    if (!Intersects(start, length, other.pc_address, other_len)) {
      continue;
    }
    if (other.id != allowed_id ||
        (native_mirror && (start != other.pc_address || length != other_len))) {
      return absl::PermissionDeniedError(
          "Native item storage overlaps another directory allocation");
    }
  }
  return absl::OkStatus();
}

std::vector<uint8_t> EncodeItemStream(
    const std::vector<NativeOverworldItemRecord>& items) {
  std::vector<uint8_t> bytes;
  bytes.reserve(items.size() * 3 + 2);
  for (const auto& item : items) {
    bytes.push_back(static_cast<uint8_t>(item.map_pos & 0xFF));
    bytes.push_back(static_cast<uint8_t>((item.map_pos >> 8) & 0x1F));
    bytes.push_back(item.item_id);
  }
  bytes.push_back(0xFF);
  bytes.push_back(0xFF);
  return bytes;
}

bool InStreamStorage(size_t start, size_t len) {
  return start >= kItemStreamBeginPc && len <= kItemStreamEndPc - start;
}

bool FitsOnGrid(const std::vector<std::optional<uint8_t>>& grid, size_t start,
                std::span<const uint8_t> bytes) {
  if (!InStreamStorage(start, bytes.size())) {
    return false;
  }
  const size_t base = start - kItemStreamBeginPc;
  for (size_t i = 0; i < bytes.size(); ++i) {
    if (grid[base + i].has_value() && *grid[base + i] != bytes[i]) {
      return false;
    }
  }
  return true;
}

void StampGrid(std::vector<std::optional<uint8_t>>& grid, size_t start,
               std::span<const uint8_t> bytes) {
  const size_t base = start - kItemStreamBeginPc;
  for (size_t i = 0; i < bytes.size(); ++i) {
    grid[base + i] = bytes[i];
  }
}

uint16_t EncodeBank1BWord(size_t pc) {
  return static_cast<uint16_t>(0x8000u | ((pc - kItemBankBasePc) & 0x7FFFu));
}

void AppendChangedSpanWrites(size_t base_pc, std::span<const uint8_t> before,
                             std::span<const uint8_t> after,
                             std::vector<NativeOverworldItemWrite>* out) {
  for (size_t i = 0; i < after.size();) {
    if (before[i] == after[i]) {
      ++i;
      continue;
    }
    const size_t begin = i++;
    while (i < after.size() && before[i] != after[i]) {
      ++i;
    }
    out->push_back(
        {static_cast<uint32_t>(base_pc + begin),
         std::vector<uint8_t>(after.begin() + begin, after.begin() + i)});
  }
}

}  // namespace

absl::StatusOr<std::vector<NativeOverworldItemWrite>>
PlanNativeOverworldItemSave(const std::vector<uint8_t>& baseline,
                            const NativeOverworldItemEdits& edits,
                            std::span<const uint8_t> final_image) {
  std::vector<NativeOverworldItemWrite> writes;
  if (edits.maps.empty()) {
    return writes;
  }
  const auto layout = InspectLyraLayout(baseline.data(), baseline.size());
  if (layout.presence != LyraLayoutPresence::kValid ||
      !IsNativeSpecialOverworldLayout(layout)) {
    return absl::FailedPreconditionError(
        "Native item planning requires a valid native special-overworld "
        "directory");
  }
  if (final_image.empty()) {
    final_image = baseline;
  }
  if (final_image.size() != baseline.size()) {
    return absl::InvalidArgumentError(
        "Native item geometry image extent differs from baseline");
  }
  if (baseline.size() <= kItemBankBytePc ||
      (baseline[kItemBankBytePc] & 0x7F) != kItemBankIndex) {
    return absl::FailedPreconditionError(
        "Native item bank byte is missing or relocated outside bank $1B");
  }

  const auto* entry22 = FindEntry(layout, 22);
  const auto* entry23 = FindEntry(layout, 23);
  const auto* parents = FindEntry(layout, 7);
  const auto* sizes = FindEntry(layout, 30);
  if (!entry22 || !entry23 || !parents || !sizes) {
    return absl::FailedPreconditionError(
        "Native directory lacks required item or geometry tables");
  }

  RETURN_IF_ERROR(
      CheckStorage(layout, kItemStreamBeginPc, kItemStreamCapacity, 0, false));
  RETURN_IF_ERROR(CheckStorage(layout, entry22->pc_address,
                               size_t{entry22->count} * 2, 22, false));
  RETURN_IF_ERROR(CheckStorage(layout, entry23->pc_address,
                               size_t{entry23->count} * 2, 23, false));
  RETURN_IF_ERROR(CheckStorage(layout, kBombDoorMirrorPc,
                               kLegacyMirrorRowCount * 2, 22, true));
  RETURN_IF_ERROR(CheckStorage(layout, kItemPointerMirrorPc,
                               kLegacyMirrorRowCount * 2, 23, true));

  const size_t capacity = layout.capacity;
  std::map<uint16_t, const NativeOverworldItemMapEdit*> edits_by_map;
  for (const auto& edit : edits.maps) {
    if (edit.map_id >= layout.active_count ||
        !edits_by_map.emplace(edit.map_id, &edit).second) {
      return absl::InvalidArgumentError(
          "Invalid, out-of-range, or duplicate native item map edit");
    }
    const uint8_t parent = final_image[parents->pc_address + edit.map_id];
    const uint8_t area_size = final_image[sizes->pc_address + edit.map_id];
    if (parent >= capacity || area_size > 3) {
      return absl::FailedPreconditionError(
          "Native item target area has invalid parent or size");
    }
    if (parent != edit.map_id) {
      if (!edit.items.empty()) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Native item edit on child map $%02X must belong "
                            "to parent area $%02X",
                            edit.map_id, parent));
      }
      continue;
    }
    const uint16_t max_tile_x = (area_size == 1 || area_size == 2) ? 64 : 32;
    const uint16_t max_tile_y = (area_size == 1 || area_size == 3) ? 64 : 32;
    for (const auto& item : edit.items) {
      if ((item.map_pos & 1u) != 0 || item.map_pos >= 0x2000u) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Native item map $%02X has invalid packed position "
                            "$%04X",
                            edit.map_id, item.map_pos));
      }
      const uint16_t game_x = (item.map_pos >> 1) & 0x3Fu;
      const uint16_t game_y = (item.map_pos >> 7) & 0x3Fu;
      if (game_x >= max_tile_x || game_y >= max_tile_y) {
        return absl::InvalidArgumentError(absl::StrFormat(
            "Native item at (%u,%u) is outside area $%02X dimensions (%ux%u)",
            game_x, game_y, edit.map_id, max_tile_x, max_tile_y));
      }
    }
  }

  const size_t bank_limit = std::min(baseline.size(), kItemBankEndPc);
  std::vector<bool> base_valid_stream(capacity, false);
  std::vector<size_t> base_start(capacity, 0);
  std::vector<size_t> base_end(capacity, 0);
  std::vector<std::vector<uint8_t>> base_streams(capacity);
  for (size_t map = 0; map < capacity; ++map) {
    const bool base_is_child = (baseline[sizes->pc_address + map] != 0 &&
                                baseline[parents->pc_address + map] != map);
    const uint16_t word = ReadWord(baseline, entry23->pc_address + map * 2);
    auto invalid_slot = [&](std::string msg) -> absl::Status {
      if (base_is_child) {
        base_valid_stream[map] = false;
        base_start[map] = 0;
        base_end[map] = 0;
        base_streams[map] = {0xFF, 0xFF};
        return absl::OkStatus();
      }
      return absl::FailedPreconditionError(std::move(msg));
    };
    if (word < 0x8000u) {
      RETURN_IF_ERROR(invalid_slot(absl::StrFormat(
          "Native item pointer for map $%02X ($%04X) is outside ROM bank", map,
          word)));
      continue;
    }
    const size_t pc = kItemBankBasePc + (word & 0x7FFFu);
    if (pc >= bank_limit) {
      RETURN_IF_ERROR(invalid_slot(absl::StrFormat(
          "Native item stream for map $%02X starts past bank $1B end", map)));
      continue;
    }
    size_t cursor = pc;
    bool terminated = false;
    while (cursor + 1 < bank_limit) {
      if (baseline[cursor] == 0xFF && baseline[cursor + 1] == 0xFF) {
        cursor += 2;
        terminated = true;
        break;
      }
      if (bank_limit - cursor < 3) {
        if (!base_is_child) {
          return absl::FailedPreconditionError(absl::StrFormat(
              "Native item stream for map $%02X is truncated", map));
        }
        break;
      }
      cursor += 3;
    }
    if (!terminated) {
      RETURN_IF_ERROR(invalid_slot(absl::StrFormat(
          "Native item stream for map $%02X lacks FF FF terminator", map)));
      continue;
    }
    base_valid_stream[map] = true;
    base_start[map] = pc;
    base_end[map] = cursor;
    base_streams[map].assign(baseline.begin() + pc, baseline.begin() + cursor);
  }

  std::vector<bool> final_is_child_map(capacity, false);
  std::vector<std::vector<uint8_t>> target_streams(capacity);
  std::vector<bool> stream_changed(capacity, false);
  bool any_stream_changed = false;
  for (size_t map = 0; map < capacity; ++map) {
    final_is_child_map[map] = (final_image[sizes->pc_address + map] != 0 &&
                               final_image[parents->pc_address + map] != map);
    const auto it = edits_by_map.find(static_cast<uint16_t>(map));
    if (it != edits_by_map.end()) {
      target_streams[map] = EncodeItemStream(it->second->items);
    } else {
      target_streams[map] = base_streams[map];
    }
    if (final_is_child_map[map] &&
        !edits_by_map.contains(static_cast<uint16_t>(map))) {
      stream_changed[map] = false;
    } else {
      stream_changed[map] =
          (target_streams[map] != base_streams[map]) ||
          (!final_is_child_map[map] && !base_valid_stream[map]);
    }
    any_stream_changed |= stream_changed[map];
  }

  std::vector<uint8_t> candidate_streams(baseline.begin() + kItemStreamBeginPc,
                                         baseline.begin() + kItemStreamEndPc);
  std::vector<size_t> final_pc = base_start;

  if (any_stream_changed) {
    // Tier 1: minimal-churn in-place / free-gap placement inside
    // [0x0DC2F9, 0x0DC89C). Keeps untouched maps and their Entry 23/0x012784
    // pointers unchanged wherever possible.
    std::vector<std::optional<uint8_t>> grid(kItemStreamCapacity);
    std::map<std::vector<uint8_t>, size_t> placed_by_bytes;

    for (size_t map = 0; map < capacity; ++map) {
      if (stream_changed[map] || !base_valid_stream[map]) {
        continue;
      }
      if (InStreamStorage(base_start[map], base_streams[map].size())) {
        if (!FitsOnGrid(grid, base_start[map], base_streams[map])) {
          return absl::InternalError(
              "Conflicting unchanged baseline item streams");
        }
        StampGrid(grid, base_start[map], base_streams[map]);
        placed_by_bytes.try_emplace(base_streams[map], base_start[map]);
      }
    }

    bool tier1_fit = true;
    std::vector<size_t> empty_changed_maps;
    std::vector<size_t> relocated_nonempty_maps;

    // 1a) Non-empty changed maps that fit in their own baseline slot (same
    // length or shortened while preserving trailing FF FF if shared).
    for (size_t map = 0; map < capacity; ++map) {
      if (!stream_changed[map] || target_streams[map].size() <= 2) {
        continue;
      }
      if (base_valid_stream[map] &&
          target_streams[map].size() <= base_streams[map].size() &&
          FitsOnGrid(grid, base_start[map], target_streams[map])) {
        StampGrid(grid, base_start[map], target_streams[map]);
        final_pc[map] = base_start[map];
        placed_by_bytes.try_emplace(target_streams[map], base_start[map]);
      } else {
        relocated_nonempty_maps.push_back(map);
      }
    }

    // 1b) Remaining non-empty changed maps: deduplicate or find first gap.
    for (size_t map : relocated_nonempty_maps) {
      const auto& bytes = target_streams[map];
      if (const auto dup = placed_by_bytes.find(bytes);
          dup != placed_by_bytes.end()) {
        final_pc[map] = dup->second;
        continue;
      }
      bool placed = false;
      for (size_t pc = kItemStreamBeginPc;
           pc + bytes.size() <= kItemStreamEndPc; ++pc) {
        if (FitsOnGrid(grid, pc, bytes)) {
          StampGrid(grid, pc, bytes);
          final_pc[map] = pc;
          placed_by_bytes.emplace(bytes, pc);
          placed = true;
          break;
        }
      }
      if (!placed) {
        tier1_fit = false;
        break;
      }
    }

    // 1c) Changed maps that became empty ({0xFF, 0xFF}): keep base_start[map]
    // if already in range and grid-compatible (zero pointer churn), else
    // share any placed FF FF terminator or allocate a 2-byte slot.
    if (tier1_fit) {
      const std::array<uint8_t, 2> kEmptyBytes = {0xFF, 0xFF};
      for (size_t map = 0; map < capacity; ++map) {
        if (!stream_changed[map] || target_streams[map].size() != 2) {
          continue;
        }
        empty_changed_maps.push_back(map);
      }
      for (size_t map : empty_changed_maps) {
        if (base_valid_stream[map] && InStreamStorage(base_start[map], 2) &&
            FitsOnGrid(grid, base_start[map], kEmptyBytes)) {
          StampGrid(grid, base_start[map], kEmptyBytes);
          final_pc[map] = base_start[map];
          continue;
        }
        bool placed = false;
        for (size_t pc = kItemStreamBeginPc; pc + 2 <= kItemStreamEndPc; ++pc) {
          const size_t off = pc - kItemStreamBeginPc;
          if (grid[off] == 0xFF && grid[off + 1] == 0xFF) {
            final_pc[map] = pc;
            placed = true;
            break;
          }
        }
        if (!placed) {
          for (size_t pc = kItemStreamBeginPc; pc + 2 <= kItemStreamEndPc;
               ++pc) {
            if (FitsOnGrid(grid, pc, kEmptyBytes)) {
              StampGrid(grid, pc, kEmptyBytes);
              final_pc[map] = pc;
              placed = true;
              break;
            }
          }
        }
        if (!placed) {
          tier1_fit = false;
          break;
        }
      }
    }

    if (tier1_fit) {
      // Materialize stamped grid bytes, filling newly vacated parts of
      // shortened/relocated changed maps with 0xFF for cleanliness.
      for (size_t map = 0; map < capacity; ++map) {
        if (!stream_changed[map] || !base_valid_stream[map] ||
            !InStreamStorage(base_start[map], base_streams[map].size())) {
          continue;
        }
        const size_t off_begin = base_start[map] - kItemStreamBeginPc;
        const size_t off_end = base_end[map] - kItemStreamBeginPc;
        for (size_t i = off_begin; i < off_end; ++i) {
          if (!grid[i].has_value()) {
            candidate_streams[i] = 0xFF;
          }
        }
      }
      for (size_t i = 0; i < kItemStreamCapacity; ++i) {
        if (grid[i].has_value()) {
          candidate_streams[i] = *grid[i];
        }
      }
    } else {
      // Tier 2: compact deduplicated repack of all 0..capacity-1 streams into
      // [0x0DC2F9, 0x0DC89C). All empty lists share one 0xFF, 0xFF word.
      std::fill(candidate_streams.begin(), candidate_streams.end(), 0xFF);
      std::map<std::vector<uint8_t>, size_t> packed_by_bytes;
      size_t cursor = kItemStreamBeginPc;
      std::optional<size_t> shared_empty_pc;

      for (size_t map = 0; map < capacity; ++map) {
        if (final_is_child_map[map] && !base_valid_stream[map] &&
            !edits_by_map.contains(static_cast<uint16_t>(map))) {
          continue;
        }
        const auto& bytes = target_streams[map];
        if (bytes.size() == 2) {
          continue;
        }
        if (const auto dup = packed_by_bytes.find(bytes);
            dup != packed_by_bytes.end()) {
          final_pc[map] = dup->second;
          continue;
        }
        if (cursor + bytes.size() > kItemStreamEndPc) {
          return absl::ResourceExhaustedError(absl::StrFormat(
              "Native overworld item data exceeds bank $1B storage (%zu "
              "bytes); no bytes written",
              kItemStreamCapacity));
        }
        std::copy(bytes.begin(), bytes.end(),
                  candidate_streams.begin() + (cursor - kItemStreamBeginPc));
        final_pc[map] = cursor;
        packed_by_bytes.emplace(bytes, cursor);
        // Reuse the trailing 0xFF, 0xFF terminator of the packed non-empty
        // stream for empty maps so empty lists cost zero extra bytes.
        shared_empty_pc = cursor + bytes.size() - 2;
        cursor += bytes.size();
      }
      if (!shared_empty_pc.has_value()) {
        if (cursor + 2 > kItemStreamEndPc) {
          return absl::ResourceExhaustedError(absl::StrFormat(
              "Native overworld item data exceeds bank $1B storage (%zu "
              "bytes); no bytes written",
              kItemStreamCapacity));
        }
        candidate_streams[cursor - kItemStreamBeginPc] = 0xFF;
        candidate_streams[cursor - kItemStreamBeginPc + 1] = 0xFF;
        shared_empty_pc = cursor;
      }
      for (size_t map = 0; map < capacity; ++map) {
        if (final_is_child_map[map] && !base_valid_stream[map] &&
            !edits_by_map.contains(static_cast<uint16_t>(map))) {
          final_pc[map] = base_start[map];
          continue;
        }
        if (target_streams[map].size() == 2) {
          final_pc[map] = *shared_empty_pc;
        }
      }
    }
  }

  AppendChangedSpanWrites(
      kItemStreamBeginPc,
      std::span<const uint8_t>(baseline.data() + kItemStreamBeginPc,
                               kItemStreamCapacity),
      candidate_streams, &writes);

  // Entry 23 + legacy mirror (< $A0). Only update maps whose pointer changed
  // or was repacked.
  std::vector<uint8_t> e23_after(
      baseline.begin() + entry23->pc_address,
      baseline.begin() + entry23->pc_address + capacity * 2);
  std::vector<uint8_t> mirror23_after(
      baseline.begin() + kItemPointerMirrorPc,
      baseline.begin() + kItemPointerMirrorPc + kLegacyMirrorRowCount * 2);
  for (size_t map = 0; map < capacity; ++map) {
    if (final_pc[map] == base_start[map]) {
      continue;
    }
    const uint16_t word = EncodeBank1BWord(final_pc[map]);
    e23_after[map * 2] = static_cast<uint8_t>(word & 0xFF);
    e23_after[map * 2 + 1] = static_cast<uint8_t>(word >> 8);
    if (map < kLegacyMirrorRowCount) {
      mirror23_after[map * 2] = e23_after[map * 2];
      mirror23_after[map * 2 + 1] = e23_after[map * 2 + 1];
    }
  }
  AppendChangedSpanWrites(
      entry23->pc_address,
      std::span<const uint8_t>(baseline.data() + entry23->pc_address,
                               capacity * 2),
      e23_after, &writes);
  AppendChangedSpanWrites(
      kItemPointerMirrorPc,
      std::span<const uint8_t>(baseline.data() + kItemPointerMirrorPc,
                               kLegacyMirrorRowCount * 2),
      mirror23_after, &writes);

  // Entry 22 bomb-door locations (0x86 secret item) + legacy mirror (< $A0).
  std::vector<uint8_t> e22_after(
      baseline.begin() + entry22->pc_address,
      baseline.begin() + entry22->pc_address + capacity * 2);
  std::vector<uint8_t> mirror22_after(
      baseline.begin() + kBombDoorMirrorPc,
      baseline.begin() + kBombDoorMirrorPc + kLegacyMirrorRowCount * 2);
  for (const auto& [map_id, edit] : edits_by_map) {
    uint16_t bomb_word = 0;
    for (const auto& item : edit->items) {
      if (item.item_id == 0x86) {
        bomb_word = item.map_pos;
      }
    }
    const uint8_t lo = static_cast<uint8_t>(bomb_word & 0xFF);
    const uint8_t hi = static_cast<uint8_t>(bomb_word >> 8);
    if (e22_after[map_id * 2] != lo || e22_after[map_id * 2 + 1] != hi) {
      e22_after[map_id * 2] = lo;
      e22_after[map_id * 2 + 1] = hi;
      if (map_id < kLegacyMirrorRowCount) {
        mirror22_after[map_id * 2] = lo;
        mirror22_after[map_id * 2 + 1] = hi;
      }
    }
  }
  AppendChangedSpanWrites(
      entry22->pc_address,
      std::span<const uint8_t>(baseline.data() + entry22->pc_address,
                               capacity * 2),
      e22_after, &writes);
  AppendChangedSpanWrites(
      kBombDoorMirrorPc,
      std::span<const uint8_t>(baseline.data() + kBombDoorMirrorPc,
                               kLegacyMirrorRowCount * 2),
      mirror22_after, &writes);

  return writes;
}

}  // namespace yaze::rom
