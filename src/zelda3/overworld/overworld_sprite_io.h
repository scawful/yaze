#ifndef YAZE_ZELDA3_OVERWORLD_SPRITE_IO_H
#define YAZE_ZELDA3_OVERWORLD_SPRITE_IO_H

#include <array>
#include <cstdint>
#include <optional>
#include <utility>
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
// Operand of the room sprite pointer-table load in bank $09 (vanilla
// LDA $09D62E,X at $09:C298). ZScream can move that table below $09:D62E, so
// the overworld region ends at whichever comes first.
constexpr int kRoomSpritePointerTableOperand = 0x4C298;
// Exclusive PC end of the overworld sprite region for this ROM.
int GetOverworldSpriteRegionEnd(const Rom& rom);
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
// Minimal-diff edit of one (game state, map) list: rewrite in place when the
// list is unshared and fits (or the bytes after it are unreferenced), else
// copy the list into unreferenced region bytes and repoint only this slot.
// Fails with ResourceExhausted when no unreferenced run can hold the list.
enum class OverworldSpriteEditStrategy {
  kNoChange,
  kInPlace,
  kGrowInPlace,
  kRelocate,
};
const char* OverworldSpriteEditStrategyName(OverworldSpriteEditStrategy s);
struct OverworldSpriteListEditPlan {
  OverworldSpriteEditStrategy strategy = OverworldSpriteEditStrategy::kNoChange;
  int pointer_pc = 0;
  int old_list_pc = 0;
  OverworldSpriteBytes old_bytes;
  int new_list_pc = 0;
  OverworldSpriteBytes new_bytes;
  // Other (state, map) slots whose lists alias or overlap the old list.
  std::vector<std::pair<int, int>> sharers;
  int region_start = 0;
  int region_end = 0;
  int unreferenced_bytes = 0;  // before the edit
  OverworldSpriteSavePlan writes;
};
absl::StatusOr<OverworldSpriteListEditPlan> PlanOverworldSpriteListEdit(
    const Rom& rom, int state, int map,
    const OverworldSpriteBytes& replacement);
// Atomic, fenced publication; outer write fences remain effective.
absl::Status ApplyOverworldSpriteSave(Rom& rom,
                                      const OverworldSpriteSavePlan& plan);
}  // namespace yaze::zelda3
#endif
