#include "zelda3/overworld/overworld_sprite_io.h"

#include <algorithm>
#include <map>
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

absl::StatusOr<OverworldSpriteBytes> ReadOverworldSpriteList(
    const Rom& rom, int pointer_address) {
  const auto layout = GetOverworldSpriteLayout(rom);
  if (pointer_address < 0 || pointer_address > rom.size() - 2 ||
      rom.size() < kOverworldSpriteDataEnd) {
    return absl::FailedPreconditionError(
        "Overworld sprite region is truncated");
  }
  const auto& data = rom.vector();
  const int pointer = data[pointer_address] | (data[pointer_address + 1] << 8);
  const int start = 0x40000 + pointer;  // bank $09, ROM half $8000..$FFFF
  if (pointer < 0x8000 || start < layout.data_start ||
      start >= kOverworldSpriteDataEnd) {
    return absl::FailedPreconditionError(
        "Overworld sprite pointer outside reserved region");
  }
  OverworldSpriteBytes bytes;
  for (int pos = start; pos < kOverworldSpriteDataEnd;) {
    if (data[pos] == 0xFF) {
      bytes.push_back(0xFF);
      return bytes;
    }
    if (pos + 3 > kOverworldSpriteDataEnd)
      break;
    bytes.insert(bytes.end(), data.begin() + pos, data.begin() + pos + 3);
    pos += 3;
  }
  return absl::FailedPreconditionError(
      "Overworld sprite list has no terminator within reserved region");
}

absl::StatusOr<OverworldSpriteSavePlan> PlanOverworldSpriteSave(
    const Rom& rom, const OverworldSpriteEdits& edits) {
  const auto layout = GetOverworldSpriteLayout(rom);
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
            static_cast<size_t>(kOverworldSpriteDataEnd - layout.data_start)) {
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
  RETURN_IF_ERROR(fence.Allow(layout.data_start, kOverworldSpriteDataEnd,
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
