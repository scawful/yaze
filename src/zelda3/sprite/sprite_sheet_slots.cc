#include "zelda3/sprite/sprite_sheet_slots.h"

#include <algorithm>
#include <map>

#include "zelda3/gfx_sheet_storage.h"

namespace yaze::zelda3 {

std::array<uint8_t, 8> SpriteSheetSlots(
    const std::array<uint8_t, 4>& spriteset_values, bool underworld) {
  constexpr uint8_t kBase = 0x73;
  return {kBase,
          static_cast<uint8_t>(kBase + (underworld ? 10 : 1)),
          static_cast<uint8_t>(kBase + 6),
          static_cast<uint8_t>(kBase + 7),
          static_cast<uint8_t>(kBase + spriteset_values[0]),
          static_cast<uint8_t>(kBase + spriteset_values[1]),
          static_cast<uint8_t>(kBase + spriteset_values[2]),
          static_cast<uint8_t>(kBase + spriteset_values[3])};
}

std::vector<SpriteTileIssue> CheckSpriteTiles(
    const Rom& rom, const std::vector<int>& tiles,
    const std::array<uint8_t, 8>& slots,
    const std::set<uint16_t>& reserved_sheets) {
  constexpr size_t kTileBytes = 24;  // one 3bpp 8x8 tile
  std::map<uint16_t, std::vector<uint8_t>> sheet_data;
  std::vector<SpriteTileIssue> issues;
  for (int tile : tiles) {
    SpriteTileIssue issue;
    issue.tile = tile;
    issue.slot = SpriteSlotForTile(tile);
    issue.sheet = slots[issue.slot];
    if (reserved_sheets.count(issue.sheet) != 0) {
      issue.kind = SpriteTileIssue::Kind::kReservedSheet;
      issues.push_back(issue);
      continue;
    }
    auto cached = sheet_data.find(issue.sheet);
    if (cached == sheet_data.end()) {
      auto data = issue.sheet < kGfxSheetCount
                      ? ReadGfxSheetData(rom, issue.sheet)
                      : absl::StatusOr<std::vector<uint8_t>>(
                            absl::OutOfRangeError("sheet past the tables"));
      cached = sheet_data
                   .emplace(issue.sheet, data.ok() ? std::move(*data)
                                                   : std::vector<uint8_t>())
                   .first;
    }
    const auto& data = cached->second;
    const size_t begin = static_cast<size_t>(tile % 0x40) * kTileBytes;
    if (data.size() < begin + kTileBytes) {
      issue.kind = SpriteTileIssue::Kind::kUnreadableSheet;
      issues.push_back(issue);
      continue;
    }
    if (std::all_of(data.begin() + begin, data.begin() + begin + kTileBytes,
                    [](uint8_t b) { return b == 0; })) {
      issue.kind = SpriteTileIssue::Kind::kBlank;
      issues.push_back(issue);
    }
  }
  return issues;
}

}  // namespace yaze::zelda3
