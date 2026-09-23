#ifndef YAZE_ZELDA3_OVERWORLD_SPRITE_IO_H
#define YAZE_ZELDA3_OVERWORLD_SPRITE_IO_H

#include <array>
#include <cstdint>
#include <optional>
#include <vector>
#include "absl/status/statusor.h"
#include "rom/rom.h"

namespace yaze::zelda3 {
// PC offsets: USDASM bank_09, Overworld_Sprites_EMPTY through the start of
// RoomData_SpritePointers. Expanded tables reclaim the original OW pointers.
constexpr int kOverworldSpriteDataStart = 0x4CB41;
constexpr int kOverworldSpriteDataEnd = 0x4D62E;
struct OverworldSpriteLayout {
  std::array<int, 3> tables;
  std::array<int, 3> counts;
  int data_start;
};
OverworldSpriteLayout GetOverworldSpriteLayout(const Rom& rom);
using OverworldSpriteBytes = std::vector<uint8_t>;  // y, x, id; final FF
using OverworldSpriteEdits =
    std::array<std::array<std::optional<OverworldSpriteBytes>, 160>, 3>;
struct OverworldSpriteWrite {
  int address;
  std::vector<uint8_t> bytes;
};
using OverworldSpriteSavePlan = std::vector<OverworldSpriteWrite>;

absl::StatusOr<OverworldSpriteBytes> ReadOverworldSpriteList(
    const Rom& rom, int pointer_address);
// Preserve every untouched list, including entries outside the editor's loaded
// maps. Deduplicate exact ordered streams; reject overflow before any mutation.
absl::StatusOr<OverworldSpriteSavePlan> PlanOverworldSpriteSave(
    const Rom& rom, const OverworldSpriteEdits& edits);
// Atomic, fenced publication; outer write fences remain effective.
absl::Status ApplyOverworldSpriteSave(Rom& rom,
                                      const OverworldSpriteSavePlan& plan);
}  // namespace yaze::zelda3
#endif
