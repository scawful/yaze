#ifndef YAZE_ZELDA3_DUNGEON_ENTRANCE_LAYOUT_H_
#define YAZE_ZELDA3_DUNGEON_ENTRANCE_LAYOUT_H_

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "zelda3/dungeon/dungeon_spawn_point.h"

namespace yaze::zelda3 {

enum class EntranceLayoutKind { kLegacy, kZscreamExpanded };

enum class EntranceField : size_t {
  kRoom,
  kScrollEdge,
  kYScroll,
  kXScroll,
  kYPosition,
  kXPosition,
  kCameraYTrigger,
  kCameraXTrigger,
  kBlockset,
  kFloor,
  kDungeon,
  kDoor,
  kLadderBG,
  kScrolling,
  kScrollQuadrant,
  kExit,
  kMusic,
  kCount
};

// Table PC address and one record's byte width. Keep the existing 133 regular
// entrance IDs; recognizing relocated storage does not activate additional IDs.
using EntranceTable = std::pair<uint32_t, uint32_t>;
struct EntranceLayout {
  EntranceLayoutKind kind = EntranceLayoutKind::kLegacy;
  std::array<EntranceTable, static_cast<size_t>(EntranceField::kCount)> tables;

  uint32_t table(EntranceField field) const {
    return tables[static_cast<size_t>(field)].first;
  }
  uint32_t address(EntranceField field, int id) const {
    const auto& [base, width] = tables[static_cast<size_t>(field)];
    return base + static_cast<uint32_t>(id) * width;
  }
};

inline constexpr EntranceLayout kLegacyEntranceLayout{
    EntranceLayoutKind::kLegacy,
    {{{0x14813, 2},
      {0x1491D, 8},
      {0x14E4F, 2},
      {0x14D45, 2},
      {0x14F59, 2},
      {0x15063, 2},
      {0x1516D, 2},
      {0x15277, 2},
      {0x15381, 1},
      {0x15406, 1},
      {0x1548B, 1},
      {0x15510, 1},
      {0x15595, 1},
      {0x1561A, 1},
      {0x1569F, 1},
      {0x15724, 2},
      {0x1582E, 1}}}};

inline constexpr EntranceLayout kExpandedEntranceLayout{
    EntranceLayoutKind::kZscreamExpanded,
    {{{0x78000, 2},
      {0x78200, 8},
      {0x78C00, 2},
      {0x78A00, 2},
      {0x78E00, 2},
      {0x79000, 2},
      {0x79200, 2},
      {0x79400, 2},
      {0x79600, 1},
      {0x79700, 1},
      {0x79800, 1},
      {0x79900, 1},
      {0x79A00, 1},
      {0x79B00, 1},
      {0x79C00, 1},
      {0x79D00, 2},
      {0x79F00, 1}}}};

// The actual expanded loader, unlike ZScream's CameraX/CameraY constant names,
// reads vertical scroll from $0F8C00 and horizontal scroll from $0F8A00.
// Each site names the instruction's PC offset, opcode and table being loaded.
struct EntranceLoaderSite {
  uint32_t pc;
  uint8_t opcode;
  EntranceField field;
};
inline constexpr std::array<EntranceLoaderSite, 17> kEntranceLoaderSites = {{
    {0x159A3, 0xBD, EntranceField::kRoom},
    {0x15A1A, 0xB9, EntranceField::kScrollEdge},
    {0x159AB, 0xBD, EntranceField::kYScroll},
    {0x159B8, 0xBD, EntranceField::kXScroll},
    {0x159CB, 0xBD, EntranceField::kYPosition},
    {0x159D0, 0xBD, EntranceField::kXPosition},
    {0x159D5, 0xBD, EntranceField::kCameraYTrigger},
    {0x159E0, 0xBD, EntranceField::kCameraXTrigger},
    {0x15A74, 0xBD, EntranceField::kBlockset},
    {0x15A91, 0xBD, EntranceField::kFloor},
    {0x15A96, 0xBD, EntranceField::kDungeon},
    {0x15A9C, 0xBD, EntranceField::kDoor},
    {0x15AA1, 0xBD, EntranceField::kLadderBG},
    {0x15AB2, 0xBD, EntranceField::kScrolling},
    {0x15AC2, 0xBD, EntranceField::kScrollQuadrant},
    {0x159F0, 0xBD, EntranceField::kExit},
    {0x15A7A, 0xBD, EntranceField::kMusic},
}};

// Resolve against this ROM's current bytes, never process-global settings or
// a previous session's detection. A recognized but inconsistent expansion must
// not fall back to the populated, stale vanilla tables.
inline absl::StatusOr<EntranceLayout> ResolveEntranceLayout(
    std::span<const uint8_t> data) {
  const auto matches = [&](size_t pc, std::initializer_list<uint8_t> bytes) {
    return pc <= data.size() && bytes.size() <= data.size() - pc &&
           std::equal(bytes.begin(), bytes.end(), data.begin() + pc);
  };
  const bool marker = matches(0x7F000, {0x00, 0x01});
  const bool hook = matches(0x1599F, {0x22});
  const bool relocated_load = matches(0x15A74, {0xBD, 0x00, 0x96});
  EntranceLayout layout = kLegacyEntranceLayout;
  if (marker || hook || relocated_load) {
    const auto invalid = [](const char* reason) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Unsupported or malformed expanded dungeon entrance layout: %s",
          reason));
    };
    if (!marker || !matches(0x1599F, {0x22, 0x08, 0xF0, 0x0F}) ||
        !matches(0x15ACE, {0x22, 0x15, 0xF0, 0x0F})) {
      return invalid("marker or databank hooks do not match");
    }
    if (!matches(0x7F008, {0xAA, 0x0A, 0x0A, 0xA8, 0xE2, 0x20, 0xA9, 0x0F, 0x48,
                           0xAB, 0xC2, 0x20, 0x6B}) ||
        !matches(0x7F015,
                 {0x29, 0x0F, 0x85, 0xAA, 0xA9, 0x02, 0x48, 0xAB, 0x6B})) {
      return invalid("databank helpers do not match");
    }
    layout = kExpandedEntranceLayout;
    const auto operand_matches = [&](uint32_t pc, uint8_t opcode,
                                     uint32_t table) {
      const auto snes = static_cast<uint16_t>((table & 0x7FFF) | 0x8000);
      return matches(pc, {opcode, static_cast<uint8_t>(snes),
                          static_cast<uint8_t>(snes >> 8)});
    };
    for (const auto& site : kEntranceLoaderSites) {
      if (!operand_matches(site.pc, site.opcode, layout.table(site.field))) {
        return invalid("loader table operand does not match");
      }
    }
    for (int i = 1; i < 8; ++i) {
      if (!operand_matches(0x15A1A + i * 6, 0xB9, 0x78200 + i)) {
        return invalid("camera boundary operand does not match");
      }
    }
    for (const auto& [pc, table] : std::array<std::pair<uint32_t, uint32_t>, 3>{
             {{0x15AAA, 0x79A00}, {0x15ABB, 0x79B00}, {0x15ACB, 0x79C00}}}) {
      if (!operand_matches(pc, 0xBD, table)) {
        return invalid("secondary loader table operand does not match");
      }
    }
  }
  for (const auto& [base, width] : layout.tables) {
    if (base + width * kNumRegularDungeonEntrances > data.size()) {
      return absl::FailedPreconditionError(
          "Dungeon entrance tables exceed ROM size");
    }
  }
  return layout;
}

}  // namespace yaze::zelda3
#endif  // YAZE_ZELDA3_DUNGEON_ENTRANCE_LAYOUT_H_
