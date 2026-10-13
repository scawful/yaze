#ifndef YAZE_ZELDA3_OVERWORLD_SPECIAL_ENTRANCE_INSPECTOR_H_
#define YAZE_ZELDA3_OVERWORLD_SPECIAL_ENTRANCE_INSPECTOR_H_

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "app/gfx/types/snes_tile.h"
#include "rom/rom.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::zelda3 {

// The inspected routine is the four-entry $0EDE49 implementation. These
// results describe the loaded ROM and the current Overworld model, not an ASM
// source tree or a claim that a controller can reach the trigger.
struct SpecialEntranceRow {
  uint16_t trigger = 0;
  uint16_t source_area = 0;
  uint16_t direction = 0;
  uint16_t destination_marker = 0;
  int destination_area = -1;  // Only known $0180-$019F marker encodings.
};

struct SpecialEntranceMatch {
  int screen_id = 0;
  int parent_area = 0;
  int tile_x = 0;  // Screen-local Tile16 coordinate.
  int tile_y = 0;
  int world_x = 0;  // Canvas Tile16 coordinate.
  int world_y = 0;
  uint16_t tile16_id = 0;
  uint16_t first_word = 0;
  uint16_t masked_trigger = 0;
  int row_index = 0;
};

struct SpecialEntranceInspection {
  bool available = false;
  std::string provenance;
  std::string limitation;
  bool overlay_may_replace_tiles = false;
  int inspected_map = -1;
  int parent_area = -1;
  std::array<SpecialEntranceRow, 4> rows{};
  std::vector<SpecialEntranceMatch> matches;
};

inline SpecialEntranceInspection InspectSpecialEntrances(const Rom& rom,
                                                         Overworld& overworld,
                                                         int map_id) {
  SpecialEntranceInspection result;
  result.inspected_map = map_id;
  result.limitation =
      "A tile match does not prove a reachable Link approach or a completed "
      "transition. Link samples at (x+8,y+12); direction is arrival behavior.";
  const auto* map = overworld.overworld_map(map_id);
  if (!overworld.is_loaded() || map == nullptr) {
    result.provenance = "Unavailable: no loaded overworld map";
    return result;
  }
  result.parent_area = map->parent();
  const auto* parent = overworld.overworld_map(result.parent_area);
  result.overlay_may_replace_tiles =
      parent != nullptr &&
      (parent->has_overlay() || parent->render_subscreen_overlay() != 0x00FF);

  // LoROM $0EDE49. Validate the actual compiled reader, including its count,
  // Map16Definitions operand, masked comparison, area comparison and target
  // table operands. A ROM with a different hook/table is left unsupported.
  constexpr size_t kReader = 0x75E49;
  constexpr size_t kTable = 0x75E29;
  constexpr std::array<uint8_t, 17> kReaderPrefix = {
      0xC2, 0x31, 0x20, 0x9A, 0xDE, 0xBF, 0x00, 0x80, 0xBD,
      0x29, 0xFF, 0x01, 0x85, 0x00, 0xA2, 0x08, 0x00};
  constexpr std::array<uint8_t, 18> kReaderComparisons = {
      0xDF, 0x29, 0xDE, 0x0E, 0xD0, 0xF6, 0xA5, 0x8A, 0xDF,
      0x31, 0xDE, 0x0E, 0xD0, 0xEC, 0xBF, 0x41, 0xDE, 0x0E};
  if (rom.size() < kReader + 0x52 ||
      !std::equal(kReaderPrefix.begin(), kReaderPrefix.end(),
                  rom.data() + kReader) ||
      !std::equal(kReaderComparisons.begin(), kReaderComparisons.end(),
                  rom.data() + kReader + 0x17)) {
    result.provenance =
        "Unavailable: loaded ROM does not have the verified four-row $0EDE49 "
        "reader";
    return result;
  }
  const auto read_word = [&rom](size_t offset) -> uint16_t {
    return static_cast<uint16_t>(rom.data()[offset] |
                                 (rom.data()[offset + 1] << 8));
  };
  for (int i = 0; i < 4; ++i) {
    const uint16_t marker = read_word(kTable + 24 + i * 2);
    result.rows[i] = {read_word(kTable + i * 2), read_word(kTable + 8 + i * 2),
                      read_word(kTable + 16 + i * 2), marker,
                      marker >= 0x0180 && marker <= 0x019F
                          ? static_cast<int>(marker & 0xFF)
                          : -1};
  }
  result.available = true;
  result.provenance = "Loaded ROM " + rom.filename() +
                      ": compiled $0EDE49 reader and $0EDE29 table";
  const int world = map_id / 0x40;
  const auto& grid = overworld.GetMapTiles(world);
  const auto& definitions = overworld.tiles16();
  for (int screen = world * 0x40;
       screen < std::min((world + 1) * 0x40, kNumOverworldMaps); ++screen) {
    const auto* candidate = overworld.overworld_map(screen);
    if (candidate == nullptr || candidate->parent() != result.parent_area) {
      continue;
    }
    const int left = (screen % 8) * 32;
    const int top = ((screen % 0x40) / 8) * 32;
    for (int y = 0; y < 32; ++y) {
      for (int x = 0; x < 32; ++x) {
        if (left + x >= static_cast<int>(grid.size()) ||
            top + y >= static_cast<int>(grid[left + x].size())) {
          continue;
        }
        const uint16_t tile16_id = grid[left + x][top + y];
        if (tile16_id >= definitions.size()) {
          continue;
        }
        const uint16_t first_word =
            gfx::TileInfoToWord(definitions[tile16_id].tile0_);
        const uint16_t masked = first_word & 0x01FF;
        // The compiled loop decrements X from 8 before each comparison.
        // A duplicate pair therefore resolves to the last table row.
        for (int row = 3; row >= 0; --row) {
          if (result.rows[row].source_area == result.parent_area &&
              result.rows[row].trigger == masked) {
            result.matches.push_back({screen, result.parent_area, x, y,
                                      left + x, top + y, tile16_id, first_word,
                                      masked, row});
            break;
          }
        }
      }
    }
  }
  return result;
}

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_OVERWORLD_SPECIAL_ENTRANCE_INSPECTOR_H_
