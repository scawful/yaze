#include "zelda3/overworld/overworld_sprite_io.h"

#include <algorithm>
#include <map>
#include "absl/strings/str_format.h"
#include "rom/transaction.h"
#include "rom/write_fence.h"
#include "util/macro.h"
#include "zelda3/overworld/overworld_version_helper.h"

namespace yaze::zelda3 {
OverworldSpriteLayout GetOverworldSpriteLayout(const Rom& rom) {
  if (OverworldVersionHelper::SupportsAreaEnum(
          OverworldVersionHelper::GetVersion(rom))) {
    return {{{0x141438, 0x141578, 0x1416B8}}, {{160, 160, 160}}, 0x4C881};
  }
  return {{{0x4C881, 0x4C901, 0x4CA21}},
          {{64, 144, 144}},
          kOverworldSpriteDataStart};
}

int GetOverworldSpriteRegionEnd(const Rom& rom) {
  const auto layout = GetOverworldSpriteLayout(rom);
  if (rom.size() < kOverworldSpriteDataEnd)
    return kOverworldSpriteDataEnd;
  const auto& data = rom.vector();
  const int pointer = data[kRoomSpritePointerTableOperand] |
                      (data[kRoomSpritePointerTableOperand + 1] << 8);
  const int table_pc = 0x40000 + pointer;
  // A table at data_start leaves no overworld sprite region: return that
  // empty end so every read and write fails closed, instead of falling back
  // to kOverworldSpriteDataEnd and letting sprite writes overwrite the table.
  if (pointer >= 0x8000 && table_pc >= layout.data_start &&
      table_pc < kOverworldSpriteDataEnd) {
    return table_pc;
  }
  return kOverworldSpriteDataEnd;
}

namespace {
struct ListSpan {
  int start = 0;
  int size = 0;  // includes the 0xFF terminator
};

absl::StatusOr<ListSpan> ReadListSpan(const std::vector<uint8_t>& data,
                                      int pointer_address, int data_start,
                                      int region_end) {
  if (pointer_address < 0 ||
      pointer_address > static_cast<int>(data.size()) - 2 ||
      static_cast<int>(data.size()) < kOverworldSpriteDataEnd) {
    return absl::FailedPreconditionError(
        "Overworld sprite region is truncated");
  }
  const int pointer = data[pointer_address] | (data[pointer_address + 1] << 8);
  const int start = 0x40000 + pointer;  // bank $09, ROM half $8000..$FFFF
  if (pointer < 0x8000 || start < data_start || start >= region_end) {
    return absl::FailedPreconditionError(
        "Overworld sprite pointer outside reserved region");
  }
  for (int pos = start; pos < region_end;) {
    if (data[pos] == 0xFF)
      return ListSpan{start, pos - start + 1};
    if (pos + 3 > region_end)
      break;
    pos += 3;
  }
  return absl::FailedPreconditionError(
      "Overworld sprite list has no terminator within reserved region");
}
}  // namespace

absl::StatusOr<OverworldSpriteBytes> ReadOverworldSpriteList(
    const Rom& rom, int pointer_address) {
  const auto layout = GetOverworldSpriteLayout(rom);
  ASSIGN_OR_RETURN(
      auto span, ReadListSpan(rom.vector(), pointer_address, layout.data_start,
                              GetOverworldSpriteRegionEnd(rom)));
  const auto& data = rom.vector();
  return OverworldSpriteBytes(data.begin() + span.start,
                              data.begin() + span.start + span.size);
}

const char* OverworldSpriteEditStrategyName(OverworldSpriteEditStrategy s) {
  switch (s) {
    case OverworldSpriteEditStrategy::kNoChange:
      return "no-change";
    case OverworldSpriteEditStrategy::kInPlace:
      return "in-place";
    case OverworldSpriteEditStrategy::kGrowInPlace:
      return "grow-in-place";
    case OverworldSpriteEditStrategy::kRelocate:
      return "relocate";
  }
  return "unknown";
}

absl::StatusOr<OverworldSpriteListEditPlan> PlanOverworldSpriteListEdit(
    const Rom& rom, int state, int map,
    const OverworldSpriteBytes& replacement) {
  const auto layout = GetOverworldSpriteLayout(rom);
  if (state < 0 || state >= 3 || map < 0 || map >= layout.counts[state]) {
    return absl::InvalidArgumentError(
        "Sprite map is not supported in this game state");
  }
  if (replacement.empty() || replacement.back() != 0xFF ||
      replacement.size() % 3 != 1) {
    return absl::InvalidArgumentError("Invalid overworld sprite stream");
  }
  for (size_t i = 0; i + 1 < replacement.size(); i += 3) {
    if (replacement[i] == 0xFF)
      return absl::InvalidArgumentError(
          "Sprite Y byte collides with terminator");
  }
  const int region_start = layout.data_start;
  const int region_end = GetOverworldSpriteRegionEnd(rom);
  const auto& data = rom.vector();

  // Inventory every pointer slot; any malformed list fails closed.
  std::vector<uint8_t> referenced(region_end - region_start, 0);
  std::vector<std::pair<std::pair<int, int>, ListSpan>> spans;
  for (int s = 0; s < 3; ++s) {
    for (int m = 0; m < layout.counts[s]; ++m) {
      ASSIGN_OR_RETURN(auto span, ReadListSpan(data, layout.tables[s] + m * 2,
                                               region_start, region_end));
      for (int i = span.start; i < span.start + span.size; ++i)
        referenced[i - region_start] = 1;
      spans.push_back({{s, m}, span});
    }
  }

  OverworldSpriteListEditPlan plan;
  plan.pointer_pc = layout.tables[state] + map * 2;
  plan.region_start = region_start;
  plan.region_end = region_end;
  plan.unreferenced_bytes = static_cast<int>(
      std::count(referenced.begin(), referenced.end(), uint8_t{0}));
  ListSpan target;
  for (const auto& [slot, span] : spans) {
    if (slot == std::make_pair(state, map))
      target = span;
  }
  plan.old_list_pc = target.start;
  plan.old_bytes.assign(data.begin() + target.start,
                        data.begin() + target.start + target.size);
  plan.new_bytes = replacement;
  plan.new_list_pc = target.start;
  for (const auto& [slot, span] : spans) {
    if (slot == std::make_pair(state, map))
      continue;
    if (span.start < target.start + target.size &&
        target.start < span.start + span.size) {
      plan.sharers.push_back(slot);
    }
  }
  if (replacement == plan.old_bytes)
    return plan;

  const int new_size = static_cast<int>(replacement.size());
  auto is_free = [&](int pc) {
    return pc >= region_start && pc < region_end &&
           !referenced[pc - region_start];
  };
  if (plan.sharers.empty()) {
    if (new_size <= target.size) {
      plan.strategy = OverworldSpriteEditStrategy::kInPlace;
    } else {
      bool fits = true;
      for (int pc = target.start + target.size; pc < target.start + new_size;
           ++pc) {
        fits &= is_free(pc);
      }
      if (fits)
        plan.strategy = OverworldSpriteEditStrategy::kGrowInPlace;
    }
  }
  if (plan.strategy == OverworldSpriteEditStrategy::kNoChange) {
    // Copy-on-write into the smallest unreferenced run that fits. The old
    // list's own bytes count as free only when no other slot uses them.
    auto usable = [&](int pc) {
      if (is_free(pc))
        return true;
      return plan.sharers.empty() && pc >= target.start &&
             pc < target.start + target.size;
    };
    int best = -1;
    int best_len = 0;
    for (int pc = region_start; pc < region_end;) {
      if (!usable(pc)) {
        ++pc;
        continue;
      }
      int end = pc;
      while (end < region_end && usable(end))
        ++end;
      const int len = end - pc;
      if (len >= new_size && (best < 0 || len < best_len)) {
        best = pc;
        best_len = len;
      }
      pc = end;
    }
    if (best < 0) {
      return absl::ResourceExhaustedError(absl::StrFormat(
          "Overworld sprite list for state %d map $%02X cannot grow in place "
          "and no unreferenced run of %d bytes exists in PC $%06X-$%06X "
          "(%d unreferenced bytes total); no bytes written",
          state, map, new_size, region_start, region_end,
          plan.unreferenced_bytes));
    }
    plan.strategy = OverworldSpriteEditStrategy::kRelocate;
    plan.new_list_pc = best;
  }

  if (plan.strategy == OverworldSpriteEditStrategy::kRelocate) {
    plan.writes.push_back({plan.new_list_pc, replacement});
    const uint16_t pointer = static_cast<uint16_t>(plan.new_list_pc - 0x40000);
    plan.writes.push_back({plan.pointer_pc,
                           {static_cast<uint8_t>(pointer & 0xFF),
                            static_cast<uint8_t>(pointer >> 8)}});
  } else {
    plan.writes.push_back({target.start, replacement});
  }

  // Simulate on a copy: the target must read back as the replacement and
  // every other slot must read back unchanged.
  std::vector<uint8_t> sim = data;
  for (const auto& write : plan.writes)
    std::copy(write.bytes.begin(), write.bytes.end(),
              sim.begin() + write.address);
  for (const auto& [slot, span] : spans) {
    ASSIGN_OR_RETURN(
        auto after,
        ReadListSpan(sim, layout.tables[slot.first] + slot.second * 2,
                     region_start, region_end));
    const OverworldSpriteBytes got(sim.begin() + after.start,
                                   sim.begin() + after.start + after.size);
    const OverworldSpriteBytes want =
        slot == std::make_pair(state, map)
            ? replacement
            : OverworldSpriteBytes(data.begin() + span.start,
                                   data.begin() + span.start + span.size);
    if (got != want) {
      return absl::InternalError(absl::StrFormat(
          "Sprite edit plan would change state %d map $%02X; no bytes written",
          slot.first, slot.second));
    }
  }
  return plan;
}

absl::StatusOr<OverworldSpriteSavePlan> PlanOverworldSpriteSave(
    const Rom& rom, const OverworldSpriteEdits& edits) {
  const auto layout = GetOverworldSpriteLayout(rom);
  const int region_end = GetOverworldSpriteRegionEnd(rom);
  std::vector<OverworldSpriteBytes> lists;
  bool changed = false;
  for (int state = 0; state < 3; ++state) {
    for (int map = 0; map < 160; ++map) {
      if (map >= layout.counts[state]) {
        if (edits[state][map])
          return absl::InvalidArgumentError(
              "Sprite map is not supported in this game state");
        continue;
      }
      ASSIGN_OR_RETURN(auto bytes, ReadOverworldSpriteList(
                                       rom, layout.tables[state] + map * 2));
      if (edits[state][map]) {
        const auto& replacement = *edits[state][map];
        if (replacement.empty() || replacement.back() != 0xFF ||
            replacement.size() % 3 != 1) {
          return absl::InvalidArgumentError("Invalid overworld sprite stream");
        }
        for (size_t i = 0; i + 1 < replacement.size(); i += 3) {
          if (replacement[i] == 0xFF)
            return absl::InvalidArgumentError(
                "Sprite Y byte collides with terminator");
        }
        changed |= replacement != bytes;
        bytes = replacement;
      }
      lists.push_back(std::move(bytes));
    }
  }
  if (!changed)
    return OverworldSpriteSavePlan{};
  std::map<OverworldSpriteBytes, uint16_t> shared;
  std::vector<uint8_t> payload;
  OverworldSpriteSavePlan plan;
  size_t list_index = 0;
  for (int state = 0; state < 3; ++state) {
    std::vector<uint8_t> pointers;
    for (int map = 0; map < layout.counts[state]; ++map) {
      const auto& bytes = lists[list_index++];
      auto it = shared.find(bytes);
      if (it == shared.end()) {
        if (payload.size() + bytes.size() >
            static_cast<size_t>(region_end - layout.data_start)) {
          return absl::ResourceExhaustedError(
              "Overworld sprite data exceeds reserved region; no bytes "
              "written");
        }
        const auto pointer =
            static_cast<uint16_t>(layout.data_start + payload.size() - 0x40000);
        it = shared.emplace(bytes, pointer).first;
        payload.insert(payload.end(), bytes.begin(), bytes.end());
      }
      pointers.push_back(it->second & 0xFF);
      pointers.push_back(it->second >> 8);
    }
    plan.push_back({layout.tables[state], std::move(pointers)});
  }
  plan.push_back({layout.data_start, std::move(payload)});
  return plan;
}

absl::Status ApplyOverworldSpriteSave(Rom& rom,
                                      const OverworldSpriteSavePlan& plan) {
  if (plan.empty())
    return absl::OkStatus();
  const auto layout = GetOverworldSpriteLayout(rom);
  rom::WriteFence fence;
  RETURN_IF_ERROR(fence.Allow(layout.data_start,
                              GetOverworldSpriteRegionEnd(rom),
                              "overworld sprite data"));
  for (int state = 0; state < 3; ++state) {
    RETURN_IF_ERROR(fence.Allow(layout.tables[state],
                                layout.tables[state] + layout.counts[state] * 2,
                                "overworld sprite pointers"));
  }
  ScopedRomTransaction transaction(rom);
  rom::ScopedWriteFence scope(&rom, &fence);
  for (const auto& write : plan) {
    RETURN_IF_ERROR(rom.WriteVector(write.address, write.bytes));
  }
  transaction.Commit();
  return absl::OkStatus();
}
}  // namespace yaze::zelda3
