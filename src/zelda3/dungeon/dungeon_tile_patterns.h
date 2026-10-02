#ifndef YAZE_ZELDA3_DUNGEON_DUNGEON_TILE_PATTERNS_H_
#define YAZE_ZELDA3_DUNGEON_DUNGEON_TILE_PATTERNS_H_

#include <cstdint>
#include <map>
#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace yaze::zelda3 {

// Read-only tile words for ObjectParser's standard-object payloads. This is
// deliberately not an assembler, ROM patch or authorization to write bytes.
struct DungeonTilePatterns {
  std::string source_path;
  std::map<uint32_t, uint16_t> words;  // Headerless PC offset -> tilemap word.
};

// Supports comments, labels, optional `lorom`, `org $00xxxx` and literal `dw`
// lists in the bank-00 object-data range. Unsupported directives, expressions,
// overlapping words and invalid addresses reject the entire source.
absl::StatusOr<DungeonTilePatterns> ParseDungeonTilePatterns(
    absl::string_view source);
absl::StatusOr<DungeonTilePatterns> LoadDungeonTilePatterns(
    const std::string& path);

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_DUNGEON_DUNGEON_TILE_PATTERNS_H_
