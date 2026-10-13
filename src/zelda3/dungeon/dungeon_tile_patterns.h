#ifndef YAZE_ZELDA3_DUNGEON_DUNGEON_TILE_PATTERNS_H_
#define YAZE_ZELDA3_DUNGEON_DUNGEON_TILE_PATTERNS_H_

#include <cstdint>
#include <map>
#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace yaze::zelda3 {

// Literal tile words for ObjectParser and their exact source tokens. This is
// deliberately not an assembler or a ROM patch.
struct DungeonTilePatterns {
  std::string source_path;
  std::string source_text;
  struct Token {
    size_t offset;
    size_t length;
  };
  std::map<uint32_t, Token> tokens;
  std::map<uint32_t, uint16_t> words;  // Headerless PC offset -> tilemap word.
};

// Supports comments, labels, optional `lorom`, `org $00xxxx` and literal `dw`
// lists in the bank-00 object-data range. Literal `assert pc() == $00xxxx`
// checks the current address, with an optional quoted message. Other directives,
// expressions, overlapping words and invalid addresses reject the entire source.
absl::StatusOr<DungeonTilePatterns> ParseDungeonTilePatterns(
    absl::string_view source);
absl::StatusOr<DungeonTilePatterns> LoadDungeonTilePatterns(
    const std::string& path);

// Replace only requested literal words. Requires the exact captured file bytes;
// publication uses the shared atomic source-artifact transaction.
absl::Status PublishDungeonTilePatterns(
    const DungeonTilePatterns& expected,
    const std::map<uint32_t, uint16_t>& replacements);

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_DUNGEON_DUNGEON_TILE_PATTERNS_H_
